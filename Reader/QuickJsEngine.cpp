/**
 * QuickJsEngine.cpp
 * 
 * QuickJS JavaScript 引擎的 C++ 封装类实现
 * 专为 Reader 阅读器集成 Legado 书源设计
 */

#include "QuickJsEngine.hpp"
#include <cstring>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <regex>
#include <algorithm>
#include <cctype>

namespace Reader {

// ============ 辅助函数 ============

// Base64 编码表
static const char base64_chars[] = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Base64 编码
static std::string base64_encode(const std::string& input) {
    std::string output;
    int val = 0, valb = -6;
    for (unsigned char c : input) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            output.push_back(base64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) output.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    while (output.size() % 4) output.push_back('=');
    return output;
}

// Base64 解码
static std::string base64_decode(const std::string& input) {
    std::string output;
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) T[base64_chars[i]] = i;
    
    int val = 0, valb = -8;
    for (unsigned char c : input) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            output.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return output;
}

// MD5 实现
static void md5_transform(uint32_t state[4], const uint8_t block[64]);

static std::string md5_encode(const std::string& input) {
    // MD5 初始化
    uint32_t state[4] = { 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476 };
    uint64_t count = 0;
    uint8_t buffer[64];
    
    // 处理输入
    const uint8_t* data = reinterpret_cast<const uint8_t*>(input.c_str());
    size_t len = input.length();
    
    size_t i = 0;
    size_t index = 0;
    
    // 处理完整的 64 字节块
    for (i = 0; i + 64 <= len; i += 64) {
        md5_transform(state, data + i);
        count += 512;
    }
    
    // 处理剩余数据
    index = len - i;
    memcpy(buffer, data + i, index);
    count += index * 8;
    
    // 填充
    buffer[index++] = 0x80;
    if (index > 56) {
        memset(buffer + index, 0, 64 - index);
        md5_transform(state, buffer);
        index = 0;
    }
    memset(buffer + index, 0, 56 - index);
    
    // 添加长度
    for (int j = 0; j < 8; j++) {
        buffer[56 + j] = (count >> (j * 8)) & 0xff;
    }
    md5_transform(state, buffer);
    
    // 输出
    std::ostringstream oss;
    for (int j = 0; j < 4; j++) {
        for (int k = 0; k < 4; k++) {
            oss << std::hex << std::setfill('0') << std::setw(2) 
                << ((state[j] >> (k * 8)) & 0xff);
        }
    }
    return oss.str();
}

// MD5 变换函数
static void md5_transform(uint32_t state[4], const uint8_t block[64]) {
    #define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
    #define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
    #define H(x, y, z) ((x) ^ (y) ^ (z))
    #define I(x, y, z) ((y) ^ ((x) | (~z)))
    #define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32-(n))))
    #define FF(a, b, c, d, x, s, ac) { \
        (a) += F((b), (c), (d)) + (x) + (uint32_t)(ac); \
        (a) = ROTATE_LEFT((a), (s)); \
        (a) += (b); \
    }
    #define GG(a, b, c, d, x, s, ac) { \
        (a) += G((b), (c), (d)) + (x) + (uint32_t)(ac); \
        (a) = ROTATE_LEFT((a), (s)); \
        (a) += (b); \
    }
    #define HH(a, b, c, d, x, s, ac) { \
        (a) += H((b), (c), (d)) + (x) + (uint32_t)(ac); \
        (a) = ROTATE_LEFT((a), (s)); \
        (a) += (b); \
    }
    #define II(a, b, c, d, x, s, ac) { \
        (a) += I((b), (c), (d)) + (x) + (uint32_t)(ac); \
        (a) = ROTATE_LEFT((a), (s)); \
        (a) += (b); \
    }
    
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t x[16];
    
    for (int i = 0; i < 16; i++) {
        x[i] = block[i*4] | (block[i*4+1] << 8) | (block[i*4+2] << 16) | (block[i*4+3] << 24);
    }
    
    // Round 1
    FF(a, b, c, d, x[ 0],  7, 0xd76aa478); FF(d, a, b, c, x[ 1], 12, 0xe8c7b756);
    FF(c, d, a, b, x[ 2], 17, 0x242070db); FF(b, c, d, a, x[ 3], 22, 0xc1bdceee);
    FF(a, b, c, d, x[ 4],  7, 0xf57c0faf); FF(d, a, b, c, x[ 5], 12, 0x4787c62a);
    FF(c, d, a, b, x[ 6], 17, 0xa8304613); FF(b, c, d, a, x[ 7], 22, 0xfd469501);
    FF(a, b, c, d, x[ 8],  7, 0x698098d8); FF(d, a, b, c, x[ 9], 12, 0x8b44f7af);
    FF(c, d, a, b, x[10], 17, 0xffff5bb1); FF(b, c, d, a, x[11], 22, 0x895cd7be);
    FF(a, b, c, d, x[12],  7, 0x6b901122); FF(d, a, b, c, x[13], 12, 0xfd987193);
    FF(c, d, a, b, x[14], 17, 0xa679438e); FF(b, c, d, a, x[15], 22, 0x49b40821);
    
    // Round 2
    GG(a, b, c, d, x[ 1],  5, 0xf61e2562); GG(d, a, b, c, x[ 6],  9, 0xc040b340);
    GG(c, d, a, b, x[11], 14, 0x265e5a51); GG(b, c, d, a, x[ 0], 20, 0xe9b6c7aa);
    GG(a, b, c, d, x[ 5],  5, 0xd62f105d); GG(d, a, b, c, x[10],  9, 0x02441453);
    GG(c, d, a, b, x[15], 14, 0xd8a1e681); GG(b, c, d, a, x[ 4], 20, 0xe7d3fbc8);
    GG(a, b, c, d, x[ 9],  5, 0x21e1cde6); GG(d, a, b, c, x[14],  9, 0xc33707d6);
    GG(c, d, a, b, x[ 3], 14, 0xf4d50d87); GG(b, c, d, a, x[ 8], 20, 0x455a14ed);
    GG(a, b, c, d, x[13],  5, 0xa9e3e905); GG(d, a, b, c, x[ 2],  9, 0xfcefa3f8);
    GG(c, d, a, b, x[ 7], 14, 0x676f02d9); GG(b, c, d, a, x[12], 20, 0x8d2a4c8a);
    
    // Round 3
    HH(a, b, c, d, x[ 5],  4, 0xfffa3942); HH(d, a, b, c, x[ 8], 11, 0x8771f681);
    HH(c, d, a, b, x[11], 16, 0x6d9d6122); HH(b, c, d, a, x[14], 23, 0xfde5380c);
    HH(a, b, c, d, x[ 1],  4, 0xa4beea44); HH(d, a, b, c, x[ 4], 11, 0x4bdecfa9);
    HH(c, d, a, b, x[ 7], 16, 0xf6bb4b60); HH(b, c, d, a, x[10], 23, 0xbebfbc70);
    HH(a, b, c, d, x[13],  4, 0x289b7ec6); HH(d, a, b, c, x[ 0], 11, 0xeaa127fa);
    HH(c, d, a, b, x[ 3], 16, 0xd4ef3085); HH(b, c, d, a, x[ 6], 23, 0x04881d05);
    HH(a, b, c, d, x[ 9],  4, 0xd9d4d039); HH(d, a, b, c, x[12], 11, 0xe6db99e5);
    HH(c, d, a, b, x[15], 16, 0x1fa27cf8); HH(b, c, d, a, x[ 2], 23, 0xc4ac5665);
    
    // Round 4
    II(a, b, c, d, x[ 0],  6, 0xf4292244); II(d, a, b, c, x[ 7], 10, 0x432aff97);
    II(c, d, a, b, x[14], 15, 0xab9423a7); II(b, c, d, a, x[ 5], 21, 0xfc93a039);
    II(a, b, c, d, x[12],  6, 0x655b59c3); II(d, a, b, c, x[ 3], 10, 0x8f0ccc92);
    II(c, d, a, b, x[10], 15, 0xffeff47d); II(b, c, d, a, x[ 1], 21, 0x85845dd1);
    II(a, b, c, d, x[ 8],  6, 0x6fa87e4f); II(d, a, b, c, x[15], 10, 0xfe2ce6e0);
    II(c, d, a, b, x[ 6], 15, 0xa3014314); II(b, c, d, a, x[13], 21, 0x4e0811a1);
    II(a, b, c, d, x[ 4],  6, 0xf7537e82); II(d, a, b, c, x[11], 10, 0xbd3af235);
    II(c, d, a, b, x[ 2], 15, 0x2ad7d2bb); II(b, c, d, a, x[ 9], 21, 0xeb86d391);
    
    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    
    #undef F
    #undef G
    #undef H
    #undef I
    #undef ROTATE_LEFT
    #undef FF
    #undef GG
    #undef HH
    #undef II
}

// URL 编码
static std::string url_encode(const std::string& str, const std::string& charset = "UTF-8") {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;
    
    for (unsigned char c : str) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << std::uppercase;
            escaped << '%' << std::setw(2) << int(c);
            escaped << std::nouppercase;
        }
    }
    return escaped.str();
}

// URL 解码
static std::string url_decode(const std::string& str) {
    std::string result;
    for (size_t i = 0; i < str.length(); i++) {
        if (str[i] == '%' && i + 2 < str.length()) {
            int value;
            std::istringstream iss(str.substr(i + 1, 2));
            if (iss >> std::hex >> value) {
                result += static_cast<char>(value);
                i += 2;
            } else {
                result += str[i];
            }
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    return result;
}

// HTML 实体解码
static std::string html_decode(const std::string& str) {
    std::string result = str;
    
    // 常见 HTML 实体
    const std::pair<std::string, std::string> entities[] = {
        {"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"},
        {"&quot;", "\""}, {"&apos;", "'"}, {"&nbsp;", " "},
        {"&#39;", "'"}, {"&#34;", "\""}
    };
    
    for (const auto& entity : entities) {
        size_t pos = 0;
        while ((pos = result.find(entity.first, pos)) != std::string::npos) {
            result.replace(pos, entity.first.length(), entity.second);
            pos += entity.second.length();
        }
    }
    
    // 处理数字实体 &#xxx;
    std::regex numEntity("&#(\\d+);");
    std::smatch match;
    std::string temp = result;
    result.clear();
    
    while (std::regex_search(temp, match, numEntity)) {
        result += match.prefix();
        int code = std::stoi(match[1].str());
        if (code < 128) {
            result += static_cast<char>(code);
        } else {
            // UTF-8 编码
            if (code < 0x800) {
                result += static_cast<char>(0xC0 | (code >> 6));
                result += static_cast<char>(0x80 | (code & 0x3F));
            } else {
                result += static_cast<char>(0xE0 | (code >> 12));
                result += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                result += static_cast<char>(0x80 | (code & 0x3F));
            }
        }
        temp = match.suffix();
    }
    result += temp;
    
    return result;
}

// 获取引擎实例指针（用于静态回调）
static QuickJsEngine* getEngineFromContext(JSContext* ctx) {
    return static_cast<QuickJsEngine*>(JS_GetContextOpaque(ctx));
}

// ============ QuickJsEngine 实现 ============

QuickJsEngine::QuickJsEngine(size_t memoryLimit)
    : m_runtime(nullptr)
    , m_context(nullptr)
    , m_hasError(false)
{
    // 创建运行时
    m_runtime = JS_NewRuntime();
    if (!m_runtime) {
        setError("Failed to create JS runtime");
        return;
    }
    
    // 设置内存限制
    JS_SetMemoryLimit(m_runtime, memoryLimit);
    
    // 创建上下文
    m_context = JS_NewContext(m_runtime);
    if (!m_context) {
        setError("Failed to create JS context");
        JS_FreeRuntime(m_runtime);
        m_runtime = nullptr;
        return;
    }
    
    // 存储 this 指针到上下文
    JS_SetContextOpaque(m_context, this);
    
    // 初始化 Legado API
    initLegadoApi();
}

QuickJsEngine::~QuickJsEngine() {
    if (m_context) {
        JS_FreeContext(m_context);
    }
    if (m_runtime) {
        JS_FreeRuntime(m_runtime);
    }
}

void QuickJsEngine::initLegadoApi() {
    registerJavaObject();
}

void QuickJsEngine::registerJavaObject() {
    JSValue global = JS_GetGlobalObject(m_context);
    
    // 创建 java 对象
    JSValue java = JS_NewObject(m_context);
    
    // 注册方法
    JS_SetPropertyStr(m_context, java, "log",
        JS_NewCFunction(m_context, js_java_log, "log", 1));
    JS_SetPropertyStr(m_context, java, "ajax",
        JS_NewCFunction(m_context, js_java_ajax, "ajax", 1));
    JS_SetPropertyStr(m_context, java, "post",
        JS_NewCFunction(m_context, js_java_post, "post", 3));
    JS_SetPropertyStr(m_context, java, "get",
        JS_NewCFunction(m_context, js_java_get, "get", 1));
    JS_SetPropertyStr(m_context, java, "put",
        JS_NewCFunction(m_context, js_java_put, "put", 2));
    JS_SetPropertyStr(m_context, java, "base64Encode",
        JS_NewCFunction(m_context, js_java_base64Encode, "base64Encode", 1));
    JS_SetPropertyStr(m_context, java, "base64Decode",
        JS_NewCFunction(m_context, js_java_base64Decode, "base64Decode", 1));
    JS_SetPropertyStr(m_context, java, "base64DecodeToString",
        JS_NewCFunction(m_context, js_java_base64Decode, "base64DecodeToString", 1));
    JS_SetPropertyStr(m_context, java, "md5Encode",
        JS_NewCFunction(m_context, js_java_md5Encode, "md5Encode", 1));
    JS_SetPropertyStr(m_context, java, "md5Encode16",
        JS_NewCFunction(m_context, js_java_md5Encode16, "md5Encode16", 1));
    JS_SetPropertyStr(m_context, java, "encodeURI",
        JS_NewCFunction(m_context, js_java_encodeURI, "encodeURI", 2));
    JS_SetPropertyStr(m_context, java, "decodeURI",
        JS_NewCFunction(m_context, js_java_decodeURI, "decodeURI", 1));
    JS_SetPropertyStr(m_context, java, "timeFormat",
        JS_NewCFunction(m_context, js_java_timeFormat, "timeFormat", 2));
    JS_SetPropertyStr(m_context, java, "htmlFormat",
        JS_NewCFunction(m_context, js_java_htmlFormat, "htmlFormat", 1));
    
    // 将 java 对象添加到全局
    JS_SetPropertyStr(m_context, global, "java", java);
    
    JS_FreeValue(m_context, global);
}

std::string QuickJsEngine::eval(const std::string& code) {
    clearError();
    
    if (!m_context) {
        setError("JS context not initialized");
        return "";
    }
    
    JSValue result = JS_Eval(m_context, code.c_str(), code.length(), "<eval>", JS_EVAL_TYPE_GLOBAL);
    
    if (JS_IsException(result)) {
        JSValue exception = JS_GetException(m_context);
        const char* str = JS_ToCString(m_context, exception);
        if (str) {
            setError(std::string("JS Error: ") + str);
            JS_FreeCString(m_context, str);
        } else {
            setError("Unknown JS error");
        }
        JS_FreeValue(m_context, exception);
        JS_FreeValue(m_context, result);
        return "";
    }
    
    std::string resultStr = jsValueToString(result);
    JS_FreeValue(m_context, result);
    return resultStr;
}

int QuickJsEngine::evalInt(const std::string& code, int defaultValue) {
    std::string result = eval(code);
    if (hasError() || result.empty()) {
        return defaultValue;
    }
    try {
        return std::stoi(result);
    } catch (...) {
        return defaultValue;
    }
}

bool QuickJsEngine::evalBool(const std::string& code, bool defaultValue) {
    std::string result = eval(code);
    if (hasError() || result.empty()) {
        return defaultValue;
    }
    return result == "true" || result == "1";
}

std::string QuickJsEngine::processRule(const std::string& rule, const std::string& result) {
    if (rule.empty()) {
        return result;
    }
    
    // 设置 result 变量
    if (!result.empty()) {
        setResult(result);
    }
    
    std::string code;
    
    // 检查 @js: 格式
    if (rule.substr(0, 4) == "@js:") {
        code = rule.substr(4);
    }
    // 检查 <js></js> 格式
    else if (rule.find("<js>") != std::string::npos) {
        size_t start = rule.find("<js>") + 4;
        size_t end = rule.find("</js>");
        if (end != std::string::npos) {
            code = rule.substr(start, end - start);
        }
    }
    // 检查 {{}} 模板格式
    else if (rule.find("{{") != std::string::npos) {
        return processTemplate(rule);
    }
    // 普通规则，直接返回
    else {
        return rule;
    }
    
    // 执行 JS 代码
    return eval(code);
}

std::string QuickJsEngine::processTemplate(const std::string& templateStr) {
    std::string result;
    size_t pos = 0;
    
    while (pos < templateStr.length()) {
        size_t start = templateStr.find("{{", pos);
        if (start == std::string::npos) {
            result += templateStr.substr(pos);
            break;
        }
        
        result += templateStr.substr(pos, start - pos);
        
        size_t end = templateStr.find("}}", start);
        if (end == std::string::npos) {
            result += templateStr.substr(start);
            break;
        }
        
        std::string expr = templateStr.substr(start + 2, end - start - 2);
        std::string value = eval(expr);
        result += value;
        
        pos = end + 2;
    }
    
    return result;
}

bool QuickJsEngine::containsJs(const std::string& rule) {
    return rule.find("@js:") != std::string::npos ||
           rule.find("<js>") != std::string::npos ||
           rule.find("{{") != std::string::npos;
}

void QuickJsEngine::setVariable(const std::string& name, const std::string& value) {
    m_variables[name] = value;
    
    // 同时设置到 JS 环境
    std::string code = "var " + name + " = " + 
        "\"" + value + "\";";  // 简化处理，实际应转义
    eval(code);
}

std::string QuickJsEngine::getVariable(const std::string& name) {
    auto it = m_variables.find(name);
    if (it != m_variables.end()) {
        return it->second;
    }
    return "";
}

void QuickJsEngine::setResult(const std::string& value) {
    // 转义字符串
    std::string escaped;
    for (char c : value) {
        switch (c) {
            case '\\': escaped += "\\\\"; break;
            case '"': escaped += "\\\""; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default: escaped += c;
        }
    }
    eval("var result = \"" + escaped + "\";");
}

void QuickJsEngine::setBaseUrl(const std::string& url) {
    setVariable("baseUrl", url);
}

void QuickJsEngine::setKeyword(const std::string& keyword) {
    setVariable("key", keyword);
}

void QuickJsEngine::setHttpCallback(HttpCallback callback) {
    m_httpCallback = callback;
}

void QuickJsEngine::setLogCallback(LogCallback callback) {
    m_logCallback = callback;
}

std::string QuickJsEngine::getLastError() const {
    return m_lastError;
}

bool QuickJsEngine::hasError() const {
    return m_hasError;
}

void QuickJsEngine::clearError() {
    m_lastError.clear();
    m_hasError = false;
}

std::string QuickJsEngine::jsValueToString(JSValue val) {
    if (JS_IsUndefined(val) || JS_IsNull(val)) {
        return "";
    }
    
    const char* str = JS_ToCString(m_context, val);
    if (!str) {
        return "";
    }
    
    std::string result(str);
    JS_FreeCString(m_context, str);
    return result;
}

void QuickJsEngine::setError(const std::string& error) {
    m_lastError = error;
    m_hasError = true;
}

// ============ JS API 静态回调实现 ============

JSValue QuickJsEngine::js_java_log(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    QuickJsEngine* engine = getEngineFromContext(ctx);
    if (argc > 0) {
        const char* str = JS_ToCString(ctx, argv[0]);
        if (str) {
            if (engine && engine->m_logCallback) {
                engine->m_logCallback(str);
            } else {
                printf("[JS LOG] %s\n", str);
            }
            JS_FreeCString(ctx, str);
        }
    }
    return JS_UNDEFINED;
}

JSValue QuickJsEngine::js_java_ajax(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    QuickJsEngine* engine = getEngineFromContext(ctx);
    if (argc < 1) {
        return JS_NewString(ctx, "");
    }
    
    const char* url = JS_ToCString(ctx, argv[0]);
    if (!url) {
        return JS_NewString(ctx, "");
    }
    
    std::string result;
    if (engine && engine->m_httpCallback) {
        result = engine->m_httpCallback(url, "GET", "", {});
    } else {
        // 模拟返回
        result = "{\"code\":0,\"msg\":\"mock response\"}";
    }
    
    JS_FreeCString(ctx, url);
    return JS_NewString(ctx, result.c_str());
}

JSValue QuickJsEngine::js_java_post(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    QuickJsEngine* engine = getEngineFromContext(ctx);
    if (argc < 1) {
        return JS_NewString(ctx, "");
    }
    
    const char* url = JS_ToCString(ctx, argv[0]);
    std::string body;
    if (argc > 1) {
        const char* bodyStr = JS_ToCString(ctx, argv[1]);
        if (bodyStr) {
            body = bodyStr;
            JS_FreeCString(ctx, bodyStr);
        }
    }
    
    std::string result;
    if (engine && engine->m_httpCallback) {
        result = engine->m_httpCallback(url ? url : "", "POST", body, {});
    } else {
        result = "{\"code\":0,\"msg\":\"mock post response\"}";
    }
    
    if (url) JS_FreeCString(ctx, url);
    return JS_NewString(ctx, result.c_str());
}

JSValue QuickJsEngine::js_java_get(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    QuickJsEngine* engine = getEngineFromContext(ctx);
    if (argc < 1 || !engine) {
        return JS_NewString(ctx, "");
    }
    
    const char* key = JS_ToCString(ctx, argv[0]);
    if (!key) {
        return JS_NewString(ctx, "");
    }
    
    std::string value = engine->getVariable(key);
    JS_FreeCString(ctx, key);
    return JS_NewString(ctx, value.c_str());
}

JSValue QuickJsEngine::js_java_put(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    QuickJsEngine* engine = getEngineFromContext(ctx);
    if (argc < 2 || !engine) {
        return JS_UNDEFINED;
    }
    
    const char* key = JS_ToCString(ctx, argv[0]);
    const char* value = JS_ToCString(ctx, argv[1]);
    
    if (key && value) {
        engine->m_variables[key] = value;
    }
    
    if (key) JS_FreeCString(ctx, key);
    if (value) JS_FreeCString(ctx, value);
    return JS_UNDEFINED;
}

JSValue QuickJsEngine::js_java_base64Encode(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc < 1) {
        return JS_NewString(ctx, "");
    }
    
    const char* str = JS_ToCString(ctx, argv[0]);
    if (!str) {
        return JS_NewString(ctx, "");
    }
    
    std::string encoded = base64_encode(str);
    JS_FreeCString(ctx, str);
    return JS_NewString(ctx, encoded.c_str());
}

JSValue QuickJsEngine::js_java_base64Decode(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc < 1) {
        return JS_NewString(ctx, "");
    }
    
    const char* str = JS_ToCString(ctx, argv[0]);
    if (!str) {
        return JS_NewString(ctx, "");
    }
    
    std::string decoded = base64_decode(str);
    JS_FreeCString(ctx, str);
    return JS_NewString(ctx, decoded.c_str());
}

JSValue QuickJsEngine::js_java_md5Encode(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc < 1) {
        return JS_NewString(ctx, "");
    }
    
    const char* str = JS_ToCString(ctx, argv[0]);
    if (!str) {
        return JS_NewString(ctx, "");
    }
    
    std::string md5 = md5_encode(str);
    JS_FreeCString(ctx, str);
    return JS_NewString(ctx, md5.c_str());
}

JSValue QuickJsEngine::js_java_md5Encode16(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc < 1) {
        return JS_NewString(ctx, "");
    }
    
    const char* str = JS_ToCString(ctx, argv[0]);
    if (!str) {
        return JS_NewString(ctx, "");
    }
    
    std::string md5 = md5_encode(str);
    // 取中间 16 位
    std::string md5_16 = md5.substr(8, 16);
    JS_FreeCString(ctx, str);
    return JS_NewString(ctx, md5_16.c_str());
}

JSValue QuickJsEngine::js_java_encodeURI(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc < 1) {
        return JS_NewString(ctx, "");
    }
    
    const char* str = JS_ToCString(ctx, argv[0]);
    if (!str) {
        return JS_NewString(ctx, "");
    }
    
    std::string charset = "UTF-8";
    if (argc > 1) {
        const char* cs = JS_ToCString(ctx, argv[1]);
        if (cs) {
            charset = cs;
            JS_FreeCString(ctx, cs);
        }
    }
    
    std::string encoded = url_encode(str, charset);
    JS_FreeCString(ctx, str);
    return JS_NewString(ctx, encoded.c_str());
}

JSValue QuickJsEngine::js_java_decodeURI(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc < 1) {
        return JS_NewString(ctx, "");
    }
    
    const char* str = JS_ToCString(ctx, argv[0]);
    if (!str) {
        return JS_NewString(ctx, "");
    }
    
    std::string decoded = url_decode(str);
    JS_FreeCString(ctx, str);
    return JS_NewString(ctx, decoded.c_str());
}

JSValue QuickJsEngine::js_java_timeFormat(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc < 1) {
        return JS_NewString(ctx, "");
    }
    
    int64_t timestamp;
    JS_ToInt64(ctx, &timestamp, argv[0]);
    
    // 如果是毫秒时间戳，转换为秒
    if (timestamp > 9999999999LL) {
        timestamp /= 1000;
    }
    
    time_t t = static_cast<time_t>(timestamp);
    struct tm* tm_info = localtime(&t);
    
    char buffer[64];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
    
    return JS_NewString(ctx, buffer);
}

JSValue QuickJsEngine::js_java_htmlFormat(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc < 1) {
        return JS_NewString(ctx, "");
    }
    
    const char* str = JS_ToCString(ctx, argv[0]);
    if (!str) {
        return JS_NewString(ctx, "");
    }
    
    std::string decoded = html_decode(str);
    JS_FreeCString(ctx, str);
    return JS_NewString(ctx, decoded.c_str());
}

} // namespace Reader
