/*
 * JsEngine.h - QuickJS JavaScript Engine Wrapper for Reader
 * 
 * This module provides JavaScript execution capability for parsing
 * Legado book source rules that contain @js: or <js> scripts.
 */

#ifndef JS_ENGINE_H
#define JS_ENGINE_H

#include <string>
#include <map>
#include <functional>

// Forward declarations for QuickJS types
struct JSRuntime;
struct JSContext;
typedef uint64_t JSValue;

namespace reader {

/**
 * JsEngine - A C++ wrapper around QuickJS for executing Legado JS rules
 * 
 * Usage:
 *   JsEngine engine;
 *   engine.setVariable("result", "some text from previous step");
 *   engine.setVariable("baseUrl", "https://example.com");
 *   std::string output = engine.evaluate("result.trim()");
 */
class JsEngine {
public:
    JsEngine();
    ~JsEngine();

    // Disable copy
    JsEngine(const JsEngine&) = delete;
    JsEngine& operator=(const JsEngine&) = delete;

    /**
     * Initialize the JavaScript engine
     * @return true if initialization succeeded
     */
    bool initialize();

    /**
     * Shutdown the JavaScript engine and release resources
     */
    void shutdown();

    /**
     * Set a global variable in the JS context
     * @param name Variable name (e.g., "result", "baseUrl")
     * @param value Variable value as string
     */
    void setVariable(const std::string& name, const std::string& value);

    /**
     * Set the book object with its properties
     * @param bookName Book name
     * @param bookAuthor Book author
     * @param bookUrl Book URL
     */
    void setBookInfo(const std::string& bookName, 
                     const std::string& bookAuthor,
                     const std::string& bookUrl);

    /**
     * Evaluate a JavaScript expression or code block
     * @param code JavaScript code to execute
     * @return Result as string, or empty string on error
     */
    std::string evaluate(const std::string& code);

    /**
     * Evaluate JS code with a given input result
     * @param code JavaScript code
     * @param inputResult The "result" variable value
     * @return Evaluation result as string
     */
    std::string evaluateWithResult(const std::string& code, const std::string& inputResult);

    /**
     * Get the last error message
     * @return Error message string
     */
    std::string getLastError() const;

    /**
     * Check if engine is initialized
     */
    bool isInitialized() const { return m_initialized; }

    /**
     * Set HTTP request callback for java.ajax() implementation
     * @param callback Function that takes URL and returns response body
     */
    void setHttpCallback(std::function<std::string(const std::string&)> callback);

    /**
     * Set HTTP POST callback for java.post() implementation
     * @param callback Function that takes URL, body, headers and returns response
     */
    void setHttpPostCallback(std::function<std::string(const std::string&, const std::string&, const std::map<std::string, std::string>&)> callback);

private:
    // QuickJS runtime and context
    JSRuntime* m_runtime;
    JSContext* m_context;
    
    bool m_initialized;
    std::string m_lastError;

    // Variable storage for java.get/put
    std::map<std::string, std::string> m_variables;

    // HTTP callbacks
    std::function<std::string(const std::string&)> m_httpGetCallback;
    std::function<std::string(const std::string&, const std::string&, const std::map<std::string, std::string>&)> m_httpPostCallback;

    // Register native functions to JS context
    void registerNativeFunctions();

    // Helper to convert JS value to string
    std::string jsValueToString(JSValue val);

    // Static callbacks for QuickJS (these call instance methods via opaque pointer)
    static JSValue js_log(JSContext* ctx, JSValue this_val, int argc, JSValue* argv);
    static JSValue js_ajax(JSContext* ctx, JSValue this_val, int argc, JSValue* argv);
    static JSValue js_post(JSContext* ctx, JSValue this_val, int argc, JSValue* argv);
    static JSValue js_get(JSContext* ctx, JSValue this_val, int argc, JSValue* argv);
    static JSValue js_put(JSContext* ctx, JSValue this_val, int argc, JSValue* argv);
    static JSValue js_md5Encode(JSContext* ctx, JSValue this_val, int argc, JSValue* argv);
    static JSValue js_md5Encode16(JSContext* ctx, JSValue this_val, int argc, JSValue* argv);
    static JSValue js_base64Encode(JSContext* ctx, JSValue this_val, int argc, JSValue* argv);
    static JSValue js_base64Decode(JSContext* ctx, JSValue this_val, int argc, JSValue* argv);
    static JSValue js_encodeURI(JSContext* ctx, JSValue this_val, int argc, JSValue* argv);
    static JSValue js_htmlFormat(JSContext* ctx, JSValue this_val, int argc, JSValue* argv);
    static JSValue js_timeFormat(JSContext* ctx, JSValue this_val, int argc, JSValue* argv);
};

/**
 * Helper function to extract JS code from Legado rule
 * Handles @js:, <js>...</js>, and {{...}} patterns
 * 
 * @param rule The full rule string
 * @param jsCode Output: extracted JS code
 * @param preRule Output: rule part before JS (for chained rules)
 * @return true if JS code was found
 */
bool extractJsFromRule(const std::string& rule, 
                       std::string& jsCode, 
                       std::string& preRule);

/**
 * Check if a rule contains JavaScript
 */
bool ruleContainsJs(const std::string& rule);

} // namespace reader

#endif // JS_ENGINE_H
