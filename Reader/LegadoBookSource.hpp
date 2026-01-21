/**
 * LegadoBookSource.hpp
 * 
 * Legado 书源解析器
 * 展示如何在 Reader 中使用 QuickJsEngine 解析 Legado 书源
 */

#ifndef LEGADO_BOOK_SOURCE_HPP
#define LEGADO_BOOK_SOURCE_HPP

#include "QuickJsEngine.hpp"
#include <string>
#include <vector>
#include <memory>

namespace Reader {

/**
 * 搜索结果
 */
struct SearchResult {
    std::string name;       // 书名
    std::string author;     // 作者
    std::string bookUrl;    // 书籍详情页 URL
    std::string coverUrl;   // 封面 URL
    std::string intro;      // 简介
    std::string kind;       // 分类/标签
    std::string latestChapter; // 最新章节
};

/**
 * 章节信息
 */
struct Chapter {
    std::string title;      // 章节标题
    std::string url;        // 章节 URL
    int index;              // 章节序号
};

/**
 * 书籍详情
 */
struct BookInfo {
    std::string name;       // 书名
    std::string author;     // 作者
    std::string intro;      // 简介
    std::string coverUrl;   // 封面
    std::string kind;       // 分类
    std::string lastChapter;// 最新章节
    std::string tocUrl;     // 目录页 URL
};

/**
 * Legado 书源规则
 */
struct BookSourceRule {
    // 基本信息
    std::string bookSourceUrl;      // 书源 URL
    std::string bookSourceName;     // 书源名称
    std::string bookSourceGroup;    // 书源分组
    int bookSourceType;             // 书源类型 (0=文本, 1=音频, 2=图片)
    
    // 搜索规则
    std::string searchUrl;          // 搜索 URL
    std::string ruleSearchList;     // 搜索结果列表规则
    std::string ruleSearchName;     // 书名规则
    std::string ruleSearchAuthor;   // 作者规则
    std::string ruleSearchBookUrl;  // 书籍 URL 规则
    std::string ruleSearchCover;    // 封面规则
    std::string ruleSearchIntro;    // 简介规则
    std::string ruleSearchKind;     // 分类规则
    
    // 详情页规则
    std::string ruleBookInfoName;   // 书名规则
    std::string ruleBookInfoAuthor; // 作者规则
    std::string ruleBookInfoIntro;  // 简介规则
    std::string ruleBookInfoCover;  // 封面规则
    std::string ruleBookInfoTocUrl; // 目录 URL 规则
    
    // 目录规则
    std::string ruleTocList;        // 章节列表规则
    std::string ruleTocName;        // 章节名规则
    std::string ruleTocUrl;         // 章节 URL 规则
    std::string ruleTocNext;        // 下一页规则
    
    // 正文规则
    std::string ruleContentUrl;     // 正文 URL 规则
    std::string ruleContent;        // 正文规则
    std::string ruleContentNext;    // 下一页规则
    std::string ruleContentReplace; // 正文替换规则
};

/**
 * Legado 书源解析器
 * 
 * 使用示例:
 * ```cpp
 * LegadoBookSource source;
 * source.loadFromJson(jsonString);
 * 
 * // 设置 HTTP 回调
 * source.setHttpCallback([](const std::string& url, ...) {
 *     return httpClient.get(url);
 * });
 * 
 * // 搜索书籍
 * auto results = source.search("斗破苍穹");
 * 
 * // 获取章节列表
 * auto chapters = source.getChapterList(results[0].bookUrl);
 * 
 * // 获取正文
 * auto content = source.getContent(chapters[0].url);
 * ```
 */
class LegadoBookSource {
public:
    LegadoBookSource();
    ~LegadoBookSource();
    
    /**
     * 从 JSON 字符串加载书源
     * @param json Legado 格式的书源 JSON
     * @return 是否加载成功
     */
    bool loadFromJson(const std::string& json);
    
    /**
     * 从文件加载书源
     * @param filePath 书源文件路径
     * @return 是否加载成功
     */
    bool loadFromFile(const std::string& filePath);
    
    /**
     * 搜索书籍
     * @param keyword 搜索关键词
     * @return 搜索结果列表
     */
    std::vector<SearchResult> search(const std::string& keyword);
    
    /**
     * 获取书籍详情
     * @param bookUrl 书籍详情页 URL
     * @return 书籍详情
     */
    BookInfo getBookInfo(const std::string& bookUrl);
    
    /**
     * 获取章节列表
     * @param tocUrl 目录页 URL
     * @return 章节列表
     */
    std::vector<Chapter> getChapterList(const std::string& tocUrl);
    
    /**
     * 获取正文内容
     * @param chapterUrl 章节 URL
     * @return 正文内容
     */
    std::string getContent(const std::string& chapterUrl);
    
    /**
     * 设置 HTTP 请求回调
     */
    void setHttpCallback(HttpCallback callback);
    
    /**
     * 设置日志回调
     */
    void setLogCallback(LogCallback callback);
    
    /**
     * 获取书源名称
     */
    std::string getSourceName() const { return m_rule.bookSourceName; }
    
    /**
     * 获取书源 URL
     */
    std::string getSourceUrl() const { return m_rule.bookSourceUrl; }
    
    /**
     * 是否已加载书源
     */
    bool isLoaded() const { return m_loaded; }
    
    /**
     * 获取最后错误信息
     */
    std::string getLastError() const { return m_lastError; }

private:
    std::unique_ptr<QuickJsEngine> m_jsEngine;
    BookSourceRule m_rule;
    HttpCallback m_httpCallback;
    LogCallback m_logCallback;
    bool m_loaded;
    std::string m_lastError;
    
    // 构建搜索 URL
    std::string buildSearchUrl(const std::string& keyword);
    
    // 解析规则
    std::string parseRule(const std::string& html, const std::string& rule);
    
    // 解析列表规则
    std::vector<std::string> parseListRule(const std::string& html, const std::string& rule);
    
    // 处理相对 URL
    std::string resolveUrl(const std::string& baseUrl, const std::string& relativeUrl);
    
    // 发送 HTTP 请求
    std::string httpGet(const std::string& url);
    std::string httpPost(const std::string& url, const std::string& body);
    
    // 设置错误
    void setError(const std::string& error);
};

} // namespace Reader

#endif // LEGADO_BOOK_SOURCE_HPP
