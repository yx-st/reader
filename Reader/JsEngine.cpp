/*
 * JsEngine.cpp - QuickJS JavaScript Engine Wrapper Implementation
 */

#include "JsEngine.h"

// Include QuickJS headers
extern "C" {
#include "../opensrc/quickjs/quickjs.h"
}

#include <sstream>
#include <algorithm>
#include <cstring>
#include <regex>

// For Windows-specific functions
#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "advapi32.lib")
#else
// Linux/Unix implementations
#include <openssl/md5.h>
#include <openssl/evp.h>
#endif

namespace reader {

// Global engine pointer for static callbacks (per-context)
static JsEngine* g_currentEngine = nullptr;

JsEngine::JsEngine()
    : m_runtime(nullptr)
    , m_context(nullptr)
    , m_initialized(false)
{
}

JsEngine::~JsEngine()
{
    shutdown();
}

bool JsEngine::initialize()
{
    if (m_initialized) {
        return true;
    }

    // Create QuickJS runtime
    m_runtime = JS_NewRuntime();
    if (!m_runtime) {
        m_lastError = "Failed to create JS runtime";
        return false;
    }

    // Set memory limit (16MB should be enough for book source parsing)
    JS_SetMemoryLimit(m_runtime, 16 * 1024 * 1024);

    // Create context
    m_context = JS_NewContext(m_runtime);
    if (!m_context) {
        m_lastError = "Failed to create JS context";
        JS_FreeRuntime(m_runtime);
        m_runtime = nullptr;
        return false;
    }

    // Store this pointer in context for static callbacks
    JS_SetContextOpaque(m_context, this);
    g_currentEngine = this;

    // Register native functions
    registerNativeFunctions();

    m_initialized = true;
    return true;
}

void JsEngine::shutdown()
{
    if (m_context) {
        JS_FreeContext(m_context);
        m_context = nullptr;
    }
    if (m_runtime) {
        JS_FreeRuntime(m_runtime);
        m_runtime = nullptr;
    }
    m_initialized = false;
    if (g_currentEngine == this) {
        g_currentEngine = nullptr;
    }
}

void JsEngine::registerNativeFunctions()
{
    // Create the "java" object that Legado JS rules expect
    JSValue global = JS_GetGlobalObject(m_context);
    JSValue javaObj = JS_NewObject(m_context);

    // Register methods on java object
    JS_SetPropertyStr(m_context, javaObj, "log",
        JS_NewCFunction(m_context, js_log, "log", 1));
    JS_SetPropertyStr(m_context, javaObj, "ajax",
        JS_NewCFunction(m_context, js_ajax, "ajax", 1));
    JS_SetPropertyStr(m_context, javaObj, "post",
        JS_NewCFunction(m_context, js_post, "post", 3));
    JS_SetPropertyStr(m_context, javaObj, "get",
        JS_NewCFunction(m_context, js_get, "get", 1));
    JS_SetPropertyStr(m_context, javaObj, "put",
        JS_NewCFunction(m_context, js_put, "put", 2));
    JS_SetPropertyStr(m_context, javaObj, "md5Encode",
        JS_NewCFunction(m_context, js_md5Encode, "md5Encode", 1));
    JS_SetPropertyStr(m_context, javaObj, "md5Encode16",
        JS_NewCFunction(m_context, js_md5Encode16, "md5Encode16", 1));
    JS_SetPropertyStr(m_context, javaObj, "base64Encode",
        JS_NewCFunction(m_context, js_base64Encode, "base64Encode", 1));
    JS_SetPropertyStr(m_context, javaObj, "base64Decode",
        JS_NewCFunction(m_context, js_base64Decode, "base64Decode", 1));
    JS_SetPropertyStr(m_context, javaObj, "encodeURI",
        JS_NewCFunction(m_context, js_encodeURI, "encodeURI", 2));
    JS_SetPropertyStr(m_context, javaObj, "htmlFormat",
        JS_NewCFunction(m_context, js_htmlFormat, "htmlFormat", 1));
    JS_SetPropertyStr(m_context, javaObj, "timeFormat",
        JS_NewCFunction(m_context, js_timeFormat, "timeFormat", 1));

    // Set java object as global
    JS_SetPropertyStr(m_context, global, "java", javaObj);

    // Also create a console.log for debugging
    JSValue consoleObj = JS_NewObject(m_context);
    JS_SetPropertyStr(m_context, consoleObj, "log",
        JS_NewCFunction(m_context, js_log, "log", 1));
    JS_SetPropertyStr(m_context, global, "console", consoleObj);

    JS_FreeValue(m_context, global);
}

void JsEngine::setVariable(const std::string& name, const std::string& value)
{
    if (!m_initialized) return;

    JSValue global = JS_GetGlobalObject(m_context);
    JS_SetPropertyStr(m_context, global, name.c_str(),
        JS_NewString(m_context, value.c_str()));
    JS_FreeValue(m_context, global);
}

void JsEngine::setBookInfo(const std::string& bookName,
                           const std::string& bookAuthor,
                           const std::string& bookUrl)
{
    if (!m_initialized) return;

    JSValue global = JS_GetGlobalObject(m_context);
    JSValue bookObj = JS_NewObject(m_context);

    JS_SetPropertyStr(m_context, bookObj, "name",
        JS_NewString(m_context, bookName.c_str()));
    JS_SetPropertyStr(m_context, bookObj, "author",
        JS_NewString(m_context, bookAuthor.c_str()));
    JS_SetPropertyStr(m_context, bookObj, "bookUrl",
        JS_NewString(m_context, bookUrl.c_str()));

    JS_SetPropertyStr(m_context, global, "book", bookObj);
    JS_FreeValue(m_context, global);
}

std::string JsEngine::evaluate(const std::string& code)
{
    if (!m_initialized) {
        m_lastError = "Engine not initialized";
        return "";
    }

    // Wrap code to return result if it's just an expression
    std::string wrappedCode = code;
    
    // If code doesn't contain semicolons and doesn't start with var/let/const/function,
    // treat it as an expression and wrap it
    if (code.find(';') == std::string::npos &&
        code.find("var ") != 0 &&
        code.find("let ") != 0 &&
        code.find("const ") != 0 &&
        code.find("function") != 0) {
        wrappedCode = "(" + code + ")";
    }

    JSValue result = JS_Eval(m_context, wrappedCode.c_str(), wrappedCode.length(),
                             "<eval>", JS_EVAL_TYPE_GLOBAL);

    if (JS_IsException(result)) {
        JSValue exception = JS_GetException(m_context);
        m_lastError = jsValueToString(exception);
        JS_FreeValue(m_context, exception);
        JS_FreeValue(m_context, result);
        return "";
    }

    std::string output = jsValueToString(result);
    JS_FreeValue(m_context, result);
    return output;
}

std::string JsEngine::evaluateWithResult(const std::string& code, const std::string& inputResult)
{
    setVariable("result", inputResult);
    return evaluate(code);
}

std::string JsEngine::getLastError() const
{
    return m_lastError;
}

void JsEngine::setHttpCallback(std::function<std::string(const std::string&)> callback)
{
    m_httpGetCallback = callback;
}

void JsEngine::setHttpPostCallback(std::function<std::string(const std::string&, const std::string&, const std::map<std::string, std::string>&)> callback)
{
    m_httpPostCallback = callback;
}

std::string JsEngine::jsValueToString(JSValue val)
{
    if (JS_IsUndefined(val) || JS_IsNull(val)) {
        return "";
    }

    if (JS_IsBool(val)) {
        return JS_ToBool(m_context, val) ? "true" : "false";
    }

    const char* str = JS_ToCString(m_context, val);
    if (!str) {
        return "";
    }
    std::string result(str);
    JS_FreeCString(m_context, str);
    return result;
}

// ============ Native Function Implementations ============

JSValue JsEngine::js_log(JSContext* ctx, JSValue this_val, int argc, JSValue* argv)
{
    if (argc < 1) return JS_UNDEFINED;

    const char* str = JS_ToCString(ctx, argv[0]);
    if (str) {
#ifdef _WIN32
        OutputDebugStringA("[JsEngine] ");
        OutputDebugStringA(str);
        OutputDebugStringA("\n");
#else
        fprintf(stderr, "[JsEngine] %s\n", str);
#endif
        JS_FreeCString(ctx, str);
    }
    return JS_UNDEFINED;
}

JSValue JsEngine::js_ajax(JSContext* ctx, JSValue this_val, int argc, JSValue* argv)
{
    JsEngine* engine = static_cast<JsEngine*>(JS_GetContextOpaque(ctx));
    if (!engine || argc < 1) return JS_NewString(ctx, "");

    const char* url = JS_ToCString(ctx, argv[0]);
    if (!url) return JS_NewString(ctx, "");

    std::string result;
    if (engine->m_httpGetCallback) {
        result = engine->m_httpGetCallback(url);
    }

    JS_FreeCString(ctx, url);
    return JS_NewString(ctx, result.c_str());
}

JSValue JsEngine::js_post(JSContext* ctx, JSValue this_val, int argc, JSValue* argv)
{
    JsEngine* engine = static_cast<JsEngine*>(JS_GetContextOpaque(ctx));
    if (!engine || argc < 2) return JS_NewString(ctx, "");

    const char* url = JS_ToCString(ctx, argv[0]);
    const char* body = JS_ToCString(ctx, argv[1]);
    if (!url || !body) {
        if (url) JS_FreeCString(ctx, url);
        if (body) JS_FreeCString(ctx, body);
        return JS_NewString(ctx, "");
    }

    std::map<std::string, std::string> headers;
    // TODO: Parse headers from argv[2] if provided

    std::string result;
    if (engine->m_httpPostCallback) {
        result = engine->m_httpPostCallback(url, body, headers);
    }

    JS_FreeCString(ctx, url);
    JS_FreeCString(ctx, body);
    return JS_NewString(ctx, result.c_str());
}

JSValue JsEngine::js_get(JSContext* ctx, JSValue this_val, int argc, JSValue* argv)
{
    JsEngine* engine = static_cast<JsEngine*>(JS_GetContextOpaque(ctx));
    if (!engine || argc < 1) return JS_NewString(ctx, "");

    const char* key = JS_ToCString(ctx, argv[0]);
    if (!key) return JS_NewString(ctx, "");

    std::string value;
    auto it = engine->m_variables.find(key);
    if (it != engine->m_variables.end()) {
        value = it->second;
    }

    JS_FreeCString(ctx, key);
    return JS_NewString(ctx, value.c_str());
}

JSValue JsEngine::js_put(JSContext* ctx, JSValue this_val, int argc, JSValue* argv)
{
    JsEngine* engine = static_cast<JsEngine*>(JS_GetContextOpaque(ctx));
    if (!engine || argc < 2) return JS_UNDEFINED;

    const char* key = JS_ToCString(ctx, argv[0]);
    const char* value = JS_ToCString(ctx, argv[1]);
    if (key && value) {
        engine->m_variables[key] = value;
    }

    if (key) JS_FreeCString(ctx, key);
    if (value) JS_FreeCString(ctx, value);
    return JS_UNDEFINED;
}

// MD5 implementation
static std::string md5_hash(const std::string& input)
{
#ifdef _WIN32
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    BYTE hash[16];
    DWORD hashLen = 16;
    std::string result;

    if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        if (CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash)) {
            if (CryptHashData(hHash, (BYTE*)input.c_str(), (DWORD)input.length(), 0)) {
                if (CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0)) {
                    char hex[33];
                    for (int i = 0; i < 16; i++) {
                        sprintf(hex + i * 2, "%02x", hash[i]);
                    }
                    result = hex;
                }
            }
            CryptDestroyHash(hHash);
        }
        CryptReleaseContext(hProv, 0);
    }
    return result;
#else
    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5((unsigned char*)input.c_str(), input.length(), digest);
    char hex[33];
    for (int i = 0; i < 16; i++) {
        sprintf(hex + i * 2, "%02x", digest[i]);
    }
    return std::string(hex);
#endif
}

JSValue JsEngine::js_md5Encode(JSContext* ctx, JSValue this_val, int argc, JSValue* argv)
{
    if (argc < 1) return JS_NewString(ctx, "");

    const char* str = JS_ToCString(ctx, argv[0]);
    if (!str) return JS_NewString(ctx, "");

    std::string hash = md5_hash(str);
    JS_FreeCString(ctx, str);
    return JS_NewString(ctx, hash.c_str());
}

JSValue JsEngine::js_md5Encode16(JSContext* ctx, JSValue this_val, int argc, JSValue* argv)
{
    if (argc < 1) return JS_NewString(ctx, "");

    const char* str = JS_ToCString(ctx, argv[0]);
    if (!str) return JS_NewString(ctx, "");

    std::string hash = md5_hash(str);
    // Return middle 16 characters (8-24)
    if (hash.length() >= 24) {
        hash = hash.substr(8, 16);
    }
    JS_FreeCString(ctx, str);
    return JS_NewString(ctx, hash.c_str());
}

// Base64 implementation
static const char base64_chars[] = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string base64_encode(const std::string& input)
{
    std::string ret;
    int i = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    size_t in_len = input.length();
    const unsigned char* bytes_to_encode = (const unsigned char*)input.c_str();

    while (in_len--) {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;
            for (i = 0; i < 4; i++)
                ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i) {
        for (int j = i; j < 3; j++)
            char_array_3[j] = '\0';
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        for (int j = 0; j < i + 1; j++)
            ret += base64_chars[char_array_4[j]];
        while (i++ < 3)
            ret += '=';
    }
    return ret;
}

static std::string base64_decode(const std::string& encoded)
{
    std::string ret;
    int in_len = (int)encoded.length();
    int i = 0;
    int in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];

    while (in_len-- && encoded[in_] != '=') {
        const char* pos = strchr(base64_chars, encoded[in_]);
        if (!pos) {
            in_++;
            continue;
        }
        char_array_4[i++] = (unsigned char)(pos - base64_chars);
        in_++;
        if (i == 4) {
            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
            for (i = 0; i < 3; i++)
                ret += char_array_3[i];
            i = 0;
        }
    }

    if (i) {
        for (int j = i; j < 4; j++)
            char_array_4[j] = 0;
        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        for (int j = 0; j < i - 1; j++)
            ret += char_array_3[j];
    }
    return ret;
}

JSValue JsEngine::js_base64Encode(JSContext* ctx, JSValue this_val, int argc, JSValue* argv)
{
    if (argc < 1) return JS_NewString(ctx, "");

    const char* str = JS_ToCString(ctx, argv[0]);
    if (!str) return JS_NewString(ctx, "");

    std::string encoded = base64_encode(str);
    JS_FreeCString(ctx, str);
    return JS_NewString(ctx, encoded.c_str());
}

JSValue JsEngine::js_base64Decode(JSContext* ctx, JSValue this_val, int argc, JSValue* argv)
{
    if (argc < 1) return JS_NewString(ctx, "");

    const char* str = JS_ToCString(ctx, argv[0]);
    if (!str) return JS_NewString(ctx, "");

    std::string decoded = base64_decode(str);
    JS_FreeCString(ctx, str);
    return JS_NewString(ctx, decoded.c_str());
}

// URL encoding
static std::string url_encode(const std::string& str, const std::string& charset = "UTF-8")
{
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : str) {
        if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << '%' << std::setw(2) << int((unsigned char)c);
        }
    }
    return escaped.str();
}

JSValue JsEngine::js_encodeURI(JSContext* ctx, JSValue this_val, int argc, JSValue* argv)
{
    if (argc < 1) return JS_NewString(ctx, "");

    const char* str = JS_ToCString(ctx, argv[0]);
    if (!str) return JS_NewString(ctx, "");

    std::string charset = "UTF-8";
    if (argc >= 2) {
        const char* cs = JS_ToCString(ctx, argv[1]);
        if (cs) {
            charset = cs;
            JS_FreeCString(ctx, cs);
        }
    }

    // TODO: Handle GBK encoding conversion
    std::string encoded = url_encode(str, charset);
    JS_FreeCString(ctx, str);
    return JS_NewString(ctx, encoded.c_str());
}

JSValue JsEngine::js_htmlFormat(JSContext* ctx, JSValue this_val, int argc, JSValue* argv)
{
    if (argc < 1) return JS_NewString(ctx, "");

    const char* str = JS_ToCString(ctx, argv[0]);
    if (!str) return JS_NewString(ctx, "");

    // Simple HTML entity decoding
    std::string result(str);
    JS_FreeCString(ctx, str);

    // Replace common HTML entities
    size_t pos;
    while ((pos = result.find("&nbsp;")) != std::string::npos)
        result.replace(pos, 6, " ");
    while ((pos = result.find("&lt;")) != std::string::npos)
        result.replace(pos, 4, "<");
    while ((pos = result.find("&gt;")) != std::string::npos)
        result.replace(pos, 4, ">");
    while ((pos = result.find("&amp;")) != std::string::npos)
        result.replace(pos, 5, "&");
    while ((pos = result.find("&quot;")) != std::string::npos)
        result.replace(pos, 6, "\"");
    while ((pos = result.find("<br>")) != std::string::npos)
        result.replace(pos, 4, "\n");
    while ((pos = result.find("<br/>")) != std::string::npos)
        result.replace(pos, 5, "\n");
    while ((pos = result.find("<br />")) != std::string::npos)
        result.replace(pos, 6, "\n");

    return JS_NewString(ctx, result.c_str());
}

JSValue JsEngine::js_timeFormat(JSContext* ctx, JSValue this_val, int argc, JSValue* argv)
{
    if (argc < 1) return JS_NewString(ctx, "");

    int64_t timestamp;
    if (JS_ToInt64(ctx, &timestamp, argv[0]) < 0) {
        return JS_NewString(ctx, "");
    }

    // Convert timestamp to formatted string
    time_t t = (time_t)(timestamp / 1000); // Assume milliseconds
    struct tm* tm_info = localtime(&t);
    char buffer[64];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);

    return JS_NewString(ctx, buffer);
}

// ============ Helper Functions ============

bool extractJsFromRule(const std::string& rule, std::string& jsCode, std::string& preRule)
{
    jsCode.clear();
    preRule.clear();

    // Check for @js: pattern
    size_t jsPos = rule.find("@js:");
    if (jsPos != std::string::npos) {
        preRule = rule.substr(0, jsPos);
        jsCode = rule.substr(jsPos + 4);
        return true;
    }

    // Check for <js>...</js> pattern
    size_t startTag = rule.find("<js>");
    size_t endTag = rule.find("</js>");
    if (startTag != std::string::npos && endTag != std::string::npos && endTag > startTag) {
        preRule = rule.substr(0, startTag);
        jsCode = rule.substr(startTag + 4, endTag - startTag - 4);
        return true;
    }

    // Check for {{...}} pattern (template expression)
    if (rule.find("{{") != std::string::npos && rule.find("}}") != std::string::npos) {
        // This is a template, the whole thing needs JS processing
        jsCode = rule;
        return true;
    }

    return false;
}

bool ruleContainsJs(const std::string& rule)
{
    return rule.find("@js:") != std::string::npos ||
           rule.find("<js>") != std::string::npos ||
           (rule.find("{{") != std::string::npos && rule.find("}}") != std::string::npos);
}

} // namespace reader
