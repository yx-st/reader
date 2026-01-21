/*
 * JsRuleProcessor.h - Process Legado rules that contain JavaScript
 * 
 * This module integrates JsEngine with the existing HtmlParser to handle
 * Legado book source rules that contain @js:, <js>, or {{}} patterns.
 */

#ifndef JS_RULE_PROCESSOR_H
#define JS_RULE_PROCESSOR_H

#include <string>
#include <vector>
#include <functional>

// Forward declarations
namespace reader {
    class JsEngine;
}

/**
 * JsRuleProcessor - Processes Legado rules with JavaScript support
 * 
 * This class wraps the JsEngine and provides high-level functions
 * for processing book source rules that may contain JavaScript.
 */
class JsRuleProcessor {
public:
    /**
     * Get singleton instance
     */
    static JsRuleProcessor* Instance();

    /**
     * Release singleton instance
     */
    static void ReleaseInstance();

    /**
     * Initialize the JS engine
     * @return true if successful
     */
    bool Initialize();

    /**
     * Check if a rule contains JavaScript
     * @param rule The rule string to check
     * @return true if rule contains @js:, <js>, or {{}}
     */
    bool RuleContainsJs(const char* rule);

    /**
     * Process a rule that may contain JavaScript
     * 
     * This function handles the following patterns:
     * 1. Pure XPath: returns as-is
     * 2. XPath@js:code: executes XPath first, then JS on result
     * 3. <js>code</js>: executes JS directly
     * 4. {{expression}}: evaluates template expression
     * 
     * @param html The HTML content to parse
     * @param htmlLen Length of HTML content
     * @param rule The rule string (may contain JS)
     * @param results Output vector for results
     * @param baseUrl Base URL for relative path resolution
     * @return 0 on success, non-zero on error
     */
    int ProcessRule(const char* html, int htmlLen,
                    const char* rule,
                    std::vector<std::string>& results,
                    const char* baseUrl = nullptr);

    /**
     * Process a template string with {{}} expressions
     * 
     * @param templateStr Template string with {{}} placeholders
     * @param inputResult The "result" variable value
     * @param baseUrl Base URL
     * @return Processed string with expressions evaluated
     */
    std::string ProcessTemplate(const char* templateStr,
                                const char* inputResult,
                                const char* baseUrl = nullptr);

    /**
     * Set book information for JS context
     */
    void SetBookInfo(const char* name, const char* author, const char* url);

    /**
     * Set chapter information for JS context
     */
    void SetChapterInfo(const char* title, const char* url);

    /**
     * Set HTTP GET callback for java.ajax()
     */
    void SetHttpGetCallback(std::function<std::string(const std::string&)> callback);

    /**
     * Get last error message
     */
    std::string GetLastError() const;

private:
    JsRuleProcessor();
    ~JsRuleProcessor();

    // Disable copy
    JsRuleProcessor(const JsRuleProcessor&) = delete;
    JsRuleProcessor& operator=(const JsRuleProcessor&) = delete;

    reader::JsEngine* m_engine;
    std::string m_lastError;
    bool m_initialized;

    // Parse rule to extract XPath and JS parts
    bool ParseRule(const char* rule, 
                   std::string& xpathPart, 
                   std::string& jsPart,
                   bool& isTemplate);

    // Execute XPath and return results
    int ExecuteXPath(const char* html, int htmlLen,
                     const std::string& xpath,
                     std::vector<std::string>& results);

    // Execute JS code with input
    std::string ExecuteJs(const std::string& code, 
                          const std::string& input,
                          const char* baseUrl);

    // Process template expressions
    std::string EvaluateTemplate(const std::string& templateStr,
                                 const std::string& result,
                                 const char* baseUrl);
};

#endif // JS_RULE_PROCESSOR_H
