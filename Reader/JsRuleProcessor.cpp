/*
 * JsRuleProcessor.cpp - Implementation of Legado rule processor with JS support
 */

#include "JsRuleProcessor.h"
#include "JsEngine.h"
#include "HtmlParser.h"
#include <regex>
#include <sstream>

// Singleton instance
static JsRuleProcessor* s_instance = nullptr;

JsRuleProcessor* JsRuleProcessor::Instance()
{
    if (!s_instance) {
        s_instance = new JsRuleProcessor();
    }
    return s_instance;
}

void JsRuleProcessor::ReleaseInstance()
{
    if (s_instance) {
        delete s_instance;
        s_instance = nullptr;
    }
}

JsRuleProcessor::JsRuleProcessor()
    : m_engine(nullptr)
    , m_initialized(false)
{
}

JsRuleProcessor::~JsRuleProcessor()
{
    if (m_engine) {
        m_engine->shutdown();
        delete m_engine;
        m_engine = nullptr;
    }
}

bool JsRuleProcessor::Initialize()
{
    if (m_initialized) {
        return true;
    }

    m_engine = new reader::JsEngine();
    if (!m_engine->initialize()) {
        m_lastError = m_engine->getLastError();
        delete m_engine;
        m_engine = nullptr;
        return false;
    }

    m_initialized = true;
    return true;
}

bool JsRuleProcessor::RuleContainsJs(const char* rule)
{
    if (!rule || strlen(rule) == 0) {
        return false;
    }

    return reader::ruleContainsJs(rule);
}

bool JsRuleProcessor::ParseRule(const char* rule,
                                std::string& xpathPart,
                                std::string& jsPart,
                                bool& isTemplate)
{
    xpathPart.clear();
    jsPart.clear();
    isTemplate = false;

    if (!rule || strlen(rule) == 0) {
        return true;
    }

    std::string ruleStr(rule);

    // Check for @js: pattern
    size_t jsPos = ruleStr.find("@js:");
    if (jsPos != std::string::npos) {
        xpathPart = ruleStr.substr(0, jsPos);
        jsPart = ruleStr.substr(jsPos + 4);
        return true;
    }

    // Check for <js>...</js> pattern
    size_t startTag = ruleStr.find("<js>");
    size_t endTag = ruleStr.find("</js>");
    if (startTag != std::string::npos && endTag != std::string::npos && endTag > startTag) {
        xpathPart = ruleStr.substr(0, startTag);
        jsPart = ruleStr.substr(startTag + 4, endTag - startTag - 4);
        return true;
    }

    // Check for {{...}} template pattern
    if (ruleStr.find("{{") != std::string::npos && ruleStr.find("}}") != std::string::npos) {
        isTemplate = true;
        jsPart = ruleStr;
        return true;
    }

    // No JS, entire rule is XPath
    xpathPart = ruleStr;
    return true;
}

int JsRuleProcessor::ExecuteXPath(const char* html, int htmlLen,
                                   const std::string& xpath,
                                   std::vector<std::string>& results)
{
    if (xpath.empty()) {
        return 0;
    }

    BOOL stop = FALSE;
    return HtmlParser::Instance()->HtmlParseByXpath(html, htmlLen, xpath, results, &stop);
}

std::string JsRuleProcessor::ExecuteJs(const std::string& code,
                                        const std::string& input,
                                        const char* baseUrl)
{
    if (!m_engine || !m_initialized) {
        m_lastError = "JS engine not initialized";
        return "";
    }

    // Set context variables
    m_engine->setVariable("result", input);
    if (baseUrl) {
        m_engine->setVariable("baseUrl", baseUrl);
    }

    // Execute JS
    std::string result = m_engine->evaluate(code);
    if (result.empty() && !m_engine->getLastError().empty()) {
        m_lastError = m_engine->getLastError();
    }

    return result;
}

std::string JsRuleProcessor::EvaluateTemplate(const std::string& templateStr,
                                               const std::string& result,
                                               const char* baseUrl)
{
    if (!m_engine || !m_initialized) {
        m_lastError = "JS engine not initialized";
        return templateStr;
    }

    // Set context
    m_engine->setVariable("result", result);
    if (baseUrl) {
        m_engine->setVariable("baseUrl", baseUrl);
    }

    std::string output;
    size_t pos = 0;
    size_t len = templateStr.length();

    while (pos < len) {
        size_t start = templateStr.find("{{", pos);
        if (start == std::string::npos) {
            // No more templates, append rest
            output += templateStr.substr(pos);
            break;
        }

        // Append text before {{
        output += templateStr.substr(pos, start - pos);

        // Find closing }}
        size_t end = templateStr.find("}}", start + 2);
        if (end == std::string::npos) {
            // Malformed template, append rest as-is
            output += templateStr.substr(start);
            break;
        }

        // Extract and evaluate expression
        std::string expr = templateStr.substr(start + 2, end - start - 2);
        
        // Check if expression contains @css: or @XPath: prefix
        // These need special handling
        if (expr.find("@css:") == 0 || expr.find("@XPath:") == 0) {
            // This is a selector, not pure JS
            // For now, just pass it through
            output += "{{" + expr + "}}";
        } else {
            // Evaluate as JS expression
            std::string value = m_engine->evaluate(expr);
            output += value;
        }

        pos = end + 2;
    }

    return output;
}

int JsRuleProcessor::ProcessRule(const char* html, int htmlLen,
                                  const char* rule,
                                  std::vector<std::string>& results,
                                  const char* baseUrl)
{
    results.clear();

    if (!rule || strlen(rule) == 0) {
        return 0;
    }

    // Parse the rule
    std::string xpathPart, jsPart;
    bool isTemplate = false;
    
    if (!ParseRule(rule, xpathPart, jsPart, isTemplate)) {
        m_lastError = "Failed to parse rule";
        return 1;
    }

    // Case 1: Template expression ({{...}})
    if (isTemplate) {
        // For templates, we need to evaluate each {{}} block
        // The "result" variable should be set by caller
        std::string processed = EvaluateTemplate(jsPart, "", baseUrl);
        if (!processed.empty()) {
            results.push_back(processed);
        }
        return 0;
    }

    // Case 2: Pure XPath (no JS)
    if (jsPart.empty()) {
        return ExecuteXPath(html, htmlLen, xpathPart, results);
    }

    // Case 3: XPath + JS
    // First execute XPath
    std::vector<std::string> xpathResults;
    if (!xpathPart.empty()) {
        int ret = ExecuteXPath(html, htmlLen, xpathPart, xpathResults);
        if (ret != 0) {
            return ret;
        }
    }

    // Then apply JS to each result
    if (xpathResults.empty()) {
        // No XPath results, execute JS with empty input
        std::string jsResult = ExecuteJs(jsPart, "", baseUrl);
        if (!jsResult.empty()) {
            results.push_back(jsResult);
        }
    } else {
        // Apply JS to each XPath result
        for (const auto& xpathResult : xpathResults) {
            std::string jsResult = ExecuteJs(jsPart, xpathResult, baseUrl);
            if (!jsResult.empty()) {
                results.push_back(jsResult);
            }
        }
    }

    return 0;
}

std::string JsRuleProcessor::ProcessTemplate(const char* templateStr,
                                              const char* inputResult,
                                              const char* baseUrl)
{
    if (!templateStr) {
        return "";
    }

    return EvaluateTemplate(templateStr, inputResult ? inputResult : "", baseUrl);
}

void JsRuleProcessor::SetBookInfo(const char* name, const char* author, const char* url)
{
    if (m_engine && m_initialized) {
        m_engine->setBookInfo(
            name ? name : "",
            author ? author : "",
            url ? url : ""
        );
    }
}

void JsRuleProcessor::SetChapterInfo(const char* title, const char* url)
{
    if (m_engine && m_initialized) {
        m_engine->setVariable("title", title ? title : "");
        m_engine->setVariable("chapterUrl", url ? url : "");
    }
}

void JsRuleProcessor::SetHttpGetCallback(std::function<std::string(const std::string&)> callback)
{
    if (m_engine) {
        m_engine->setHttpCallback(callback);
    }
}

std::string JsRuleProcessor::GetLastError() const
{
    return m_lastError;
}
