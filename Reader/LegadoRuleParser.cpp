#include "stdafx.h"
#include "LegadoRuleParser.h"
#include "HtmlParser.h"
#include "QuickJsEngine.hpp"
#include <regex>
#include <sstream>

// 静态实例
static LegadoRuleParser* s_instance = nullptr;

LegadoRuleParser::LegadoRuleParser()
    : m_jsEngine(nullptr)
    , m_httpCallback(nullptr)
    , m_logCallback(nullptr)
    , m_hasError(false)
{
    InitJsEngine();
}

LegadoRuleParser::~LegadoRuleParser()
{
    if (m_jsEngine)
    {
        delete m_jsEngine;
        m_jsEngine = nullptr;
    }
}

LegadoRuleParser* LegadoRuleParser::Instance()
{
    if (!s_instance)
    {
        s_instance = new LegadoRuleParser();
    }
    return s_instance;
}

void LegadoRuleParser::ReleaseInstance()
{
    if (s_instance)
    {
        delete s_instance;
        s_instance = nullptr;
    }
}

void LegadoRuleParser::InitJsEngine()
{
    m_jsEngine = new Reader::QuickJsEngine();
    
    // 设置默认的日志回调
    m_jsEngine->setLogCallback([this](const std::string& msg) {
        if (m_logCallback)
        {
            m_logCallback(msg);
        }
    });
}

void LegadoRuleParser::SetHttpCallback(HttpCallback callback)
{
    m_httpCallback = callback;
    if (m_jsEngine && callback)
    {
        m_jsEngine->setHttpCallback([callback](const std::string& url, 
                                                const std::string& method,
                                                const std::string& body,
                                                const std::map<std::string, std::string>& headers) {
            return callback(url, method, body, headers);
        });
    }
}

void LegadoRuleParser::SetLogCallback(LogCallback callback)
{
    m_logCallback = callback;
    if (m_jsEngine && callback)
    {
        m_jsEngine->setLogCallback([callback](const std::string& msg) {
            callback(msg);
        });
    }
}

bool LegadoRuleParser::ContainsJs(const std::string& rule)
{
    if (rule.find("@js:") != std::string::npos)
        return true;
    if (rule.find("<js>") != std::string::npos)
        return true;
    if (rule.find("{{") != std::string::npos && rule.find("}}") != std::string::npos)
        return true;
    return false;
}

void LegadoRuleParser::SplitRule(const std::string& rule, std::string& baseRule, std::string& jsRule)
{
    baseRule.clear();
    jsRule.clear();

    // 查找 @js: 分隔符
    size_t jsPos = rule.find("@js:");
    if (jsPos != std::string::npos)
    {
        baseRule = rule.substr(0, jsPos);
        jsRule = rule.substr(jsPos + 4); // 跳过 "@js:"
        return;
    }

    // 查找 <js></js> 标签
    size_t jsStart = rule.find("<js>");
    size_t jsEnd = rule.find("</js>");
    if (jsStart != std::string::npos && jsEnd != std::string::npos)
    {
        baseRule = rule.substr(0, jsStart);
        jsRule = rule.substr(jsStart + 4, jsEnd - jsStart - 4);
        return;
    }

    // 没有 JS 规则
    baseRule = rule;
}

int LegadoRuleParser::ParseRule(const char* html, int len, const std::string& rule, 
                                 std::vector<std::string>& value, BOOL* stop)
{
    m_hasError = false;
    m_lastError.clear();

    if (rule.empty())
    {
        return 0;
    }

    // 分离基础规则和 JS 规则
    std::string baseRule, jsRule;
    SplitRule(rule, baseRule, jsRule);

    std::vector<std::string> baseResult;
    int ret = 0;

    // 如果有基础规则，先执行基础规则
    if (!baseRule.empty())
    {
        // 判断规则类型
        if (baseRule.find("@css:") == 0)
        {
            // CSS 选择器
            ret = ParseCssRule(html, len, baseRule.substr(5), baseResult, stop);
        }
        else if (baseRule.find("@json:") == 0 || baseRule.find("$.") == 0)
        {
            // JSONPath
            std::string jsonpath = baseRule;
            if (jsonpath.find("@json:") == 0)
                jsonpath = jsonpath.substr(6);
            ret = ParseJsonPathRule(html, len, jsonpath, baseResult, stop);
        }
        else if (baseRule.find("//") == 0 || baseRule.find("@XPath:") == 0)
        {
            // XPath
            std::string xpath = baseRule;
            if (xpath.find("@XPath:") == 0)
                xpath = xpath.substr(7);
            ret = ParseXPathRule(html, len, xpath, baseResult, stop);
        }
        else
        {
            // 默认当作 JSOUP 规则，尝试转换为 XPath
            std::string xpath = CssToXPath(baseRule);
            if (!xpath.empty())
            {
                ret = ParseXPathRule(html, len, xpath, baseResult, stop);
            }
            else
            {
                // 无法解析，直接返回原内容
                baseResult.push_back(std::string(html, len));
            }
        }

        if (ret != 0)
        {
            return ret;
        }
    }
    else
    {
        // 没有基础规则，直接使用原内容
        baseResult.push_back(std::string(html, len));
    }

    // 如果有 JS 规则，对每个结果执行 JS
    if (!jsRule.empty() && m_jsEngine)
    {
        for (const auto& item : baseResult)
        {
            if (stop && *stop)
                break;

            m_jsEngine->setResult(item);
            std::string jsResult = m_jsEngine->eval(jsRule);
            
            if (m_jsEngine->hasError())
            {
                m_hasError = true;
                m_lastError = m_jsEngine->getLastError();
                // 出错时使用原结果
                value.push_back(item);
            }
            else
            {
                value.push_back(jsResult);
            }
        }
    }
    else
    {
        // 没有 JS 规则，直接返回基础结果
        value = baseResult;
    }

    return 0;
}

int LegadoRuleParser::ParseXPathRule(const char* html, int len, const std::string& xpath, 
                                      std::vector<std::string>& value, BOOL* stop)
{
    return HtmlParser::Instance()->HtmlParseByXpath(html, len, xpath, value, stop);
}

int LegadoRuleParser::ParseCssRule(const char* html, int len, const std::string& css, 
                                    std::vector<std::string>& value, BOOL* stop)
{
    // 将 CSS 选择器转换为 XPath
    std::string xpath = CssToXPath(css);
    if (xpath.empty())
    {
        m_hasError = true;
        m_lastError = "Failed to convert CSS to XPath: " + css;
        return 1;
    }
    return ParseXPathRule(html, len, xpath, value, stop);
}

int LegadoRuleParser::ParseJsonPathRule(const char* json, int len, const std::string& jsonpath, 
                                         std::vector<std::string>& value, BOOL* stop)
{
    // 使用 JS 引擎解析 JSONPath
    if (!m_jsEngine)
    {
        m_hasError = true;
        m_lastError = "JS engine not initialized";
        return 1;
    }

    std::string content(json, len);
    m_jsEngine->setResult(content);

    // 构建 JS 代码来解析 JSONPath
    std::string jsCode;
    if (jsonpath.find("$.") == 0)
    {
        // 简单的 JSONPath，转换为 JS 属性访问
        std::string path = jsonpath.substr(2); // 去掉 "$."
        jsCode = "var data = JSON.parse(result); data." + path;
    }
    else
    {
        jsCode = "var data = JSON.parse(result); data." + jsonpath;
    }

    std::string result = m_jsEngine->eval(jsCode);
    if (m_jsEngine->hasError())
    {
        m_hasError = true;
        m_lastError = m_jsEngine->getLastError();
        return 1;
    }

    value.push_back(result);
    return 0;
}

int LegadoRuleParser::ParseJsRule(const char* content, int len, const std::string& js, 
                                   std::vector<std::string>& value, BOOL* stop)
{
    if (!m_jsEngine)
    {
        m_hasError = true;
        m_lastError = "JS engine not initialized";
        return 1;
    }

    m_jsEngine->setResult(std::string(content, len));
    std::string result = m_jsEngine->eval(js);
    
    if (m_jsEngine->hasError())
    {
        m_hasError = true;
        m_lastError = m_jsEngine->getLastError();
        return 1;
    }

    value.push_back(result);
    return 0;
}

std::string LegadoRuleParser::CssToXPath(const std::string& css)
{
    // 简单的 CSS 到 XPath 转换
    // 支持: tag, .class, #id, tag.class, tag#id, tag[attr], tag[attr=value]
    
    std::string xpath;
    std::string selector = css;
    
    // 处理 @text, @href 等属性提取
    std::string attrExtract;
    size_t atPos = selector.find('@');
    if (atPos != std::string::npos && atPos > 0)
    {
        attrExtract = selector.substr(atPos + 1);
        selector = selector.substr(0, atPos);
    }

    // 处理选择器链 (空格分隔)
    std::vector<std::string> parts;
    std::istringstream iss(selector);
    std::string part;
    while (iss >> part)
    {
        parts.push_back(part);
    }

    xpath = "/";
    for (const auto& p : parts)
    {
        xpath += "/";
        
        std::string tag = "*";
        std::string className;
        std::string id;
        std::string attr;
        std::string attrValue;

        size_t pos = 0;
        size_t len = p.length();

        // 解析 tag
        while (pos < len && p[pos] != '.' && p[pos] != '#' && p[pos] != '[')
        {
            tag += p[pos];
            pos++;
        }
        if (tag == "*")
            tag = "*";
        else
            tag = tag.substr(1); // 去掉开头的 *

        // 解析 class
        if (pos < len && p[pos] == '.')
        {
            pos++;
            while (pos < len && p[pos] != '.' && p[pos] != '#' && p[pos] != '[')
            {
                className += p[pos];
                pos++;
            }
        }

        // 解析 id
        if (pos < len && p[pos] == '#')
        {
            pos++;
            while (pos < len && p[pos] != '.' && p[pos] != '#' && p[pos] != '[')
            {
                id += p[pos];
                pos++;
            }
        }

        // 解析属性
        if (pos < len && p[pos] == '[')
        {
            pos++;
            while (pos < len && p[pos] != '=' && p[pos] != ']')
            {
                attr += p[pos];
                pos++;
            }
            if (pos < len && p[pos] == '=')
            {
                pos++;
                if (pos < len && (p[pos] == '"' || p[pos] == '\''))
                    pos++;
                while (pos < len && p[pos] != '"' && p[pos] != '\'' && p[pos] != ']')
                {
                    attrValue += p[pos];
                    pos++;
                }
            }
        }

        // 构建 XPath
        xpath += tag.empty() ? "*" : tag;
        
        std::string predicate;
        if (!className.empty())
        {
            predicate += "contains(@class,'" + className + "')";
        }
        if (!id.empty())
        {
            if (!predicate.empty())
                predicate += " and ";
            predicate += "@id='" + id + "'";
        }
        if (!attr.empty())
        {
            if (!predicate.empty())
                predicate += " and ";
            if (!attrValue.empty())
                predicate += "@" + attr + "='" + attrValue + "'";
            else
                predicate += "@" + attr;
        }

        if (!predicate.empty())
        {
            xpath += "[" + predicate + "]";
        }
    }

    // 添加属性提取
    if (!attrExtract.empty())
    {
        if (attrExtract == "text")
            xpath += "/text()";
        else if (attrExtract == "html" || attrExtract == "innerHtml")
            xpath += ""; // 返回整个节点
        else
            xpath += "/@" + attrExtract;
    }

    return xpath;
}

std::string LegadoRuleParser::ProcessSearchUrl(const std::string& template_url, const std::string& keyword)
{
    if (!m_jsEngine)
    {
        return template_url;
    }

    m_jsEngine->setKeyword(keyword);
    return m_jsEngine->processTemplate(template_url);
}

std::string LegadoRuleParser::EvalJs(const std::string& code)
{
    if (!m_jsEngine)
    {
        m_hasError = true;
        m_lastError = "JS engine not initialized";
        return "";
    }

    std::string result = m_jsEngine->eval(code);
    if (m_jsEngine->hasError())
    {
        m_hasError = true;
        m_lastError = m_jsEngine->getLastError();
    }
    return result;
}

void LegadoRuleParser::SetVariable(const std::string& key, const std::string& value)
{
    if (m_jsEngine)
    {
        m_jsEngine->setVariable(key, value);
    }
}

std::string LegadoRuleParser::GetVariable(const std::string& key)
{
    if (m_jsEngine)
    {
        return m_jsEngine->getVariable(key);
    }
    return "";
}

void LegadoRuleParser::SetResult(const std::string& result)
{
    if (m_jsEngine)
    {
        m_jsEngine->setResult(result);
    }
}

bool LegadoRuleParser::HasError() const
{
    return m_hasError;
}

std::string LegadoRuleParser::GetLastError() const
{
    return m_lastError;
}
