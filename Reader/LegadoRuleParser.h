#ifndef __LEGADO_RULE_PARSER_H__
#define __LEGADO_RULE_PARSER_H__

#include <string>
#include <vector>
#include <map>
#include <functional>

// 前向声明
namespace Reader {
    class QuickJsEngine;
}

/**
 * Legado 书源规则解析器
 * 
 * 支持的规则格式:
 * 1. XPath: //div[@class='content']/text()
 * 2. CSS: @css:.content@text
 * 3. JSONPath: $.data.list[*].name
 * 4. JavaScript: @js:result.trim()
 * 5. 混合规则: //div/text()@js:result.trim()
 */
class LegadoRuleParser
{
public:
    // HTTP 请求回调类型
    typedef std::function<std::string(const std::string& url, 
                                       const std::string& method,
                                       const std::string& body,
                                       const std::map<std::string, std::string>& headers)> HttpCallback;
    
    // 日志回调类型
    typedef std::function<void(const std::string& message)> LogCallback;

private:
    LegadoRuleParser();
    ~LegadoRuleParser();

public:
    static LegadoRuleParser* Instance();
    static void ReleaseInstance();

    // 设置回调
    void SetHttpCallback(HttpCallback callback);
    void SetLogCallback(LogCallback callback);

    // 解析规则
    // html: 待解析的 HTML/JSON 内容
    // rule: Legado 规则字符串
    // value: 输出结果
    // stop: 停止标志
    int ParseRule(const char* html, int len, const std::string& rule, 
                  std::vector<std::string>& value, BOOL* stop);

    // 处理搜索 URL 模板
    // template_url: 包含 {{}} 模板的 URL
    // keyword: 搜索关键词
    std::string ProcessSearchUrl(const std::string& template_url, const std::string& keyword);

    // 执行 JS 代码
    std::string EvalJs(const std::string& code);

    // 设置/获取变量
    void SetVariable(const std::string& key, const std::string& value);
    std::string GetVariable(const std::string& key);

    // 设置 result 变量 (用于 JS 规则)
    void SetResult(const std::string& result);

    // 检查规则是否包含 JS
    static bool ContainsJs(const std::string& rule);

    // 检查是否有错误
    bool HasError() const;
    std::string GetLastError() const;

private:
    // 解析不同类型的规则
    int ParseXPathRule(const char* html, int len, const std::string& xpath, 
                       std::vector<std::string>& value, BOOL* stop);
    int ParseCssRule(const char* html, int len, const std::string& css, 
                     std::vector<std::string>& value, BOOL* stop);
    int ParseJsonPathRule(const char* json, int len, const std::string& jsonpath, 
                          std::vector<std::string>& value, BOOL* stop);
    int ParseJsRule(const char* content, int len, const std::string& js, 
                    std::vector<std::string>& value, BOOL* stop);

    // 分离混合规则
    void SplitRule(const std::string& rule, std::string& baseRule, std::string& jsRule);

    // CSS 选择器转 XPath
    std::string CssToXPath(const std::string& css);

    // 初始化 JS 引擎
    void InitJsEngine();

private:
    Reader::QuickJsEngine* m_jsEngine;
    HttpCallback m_httpCallback;
    LogCallback m_logCallback;
    std::string m_lastError;
    bool m_hasError;
};

#endif // !__LEGADO_RULE_PARSER_H__
