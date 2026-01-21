/*
 * LegadoConverter.cpp
 * 
 * Legado 书源格式转换器
 * 将 Legado (阅读) App 的书源格式转换为 Reader 兼容格式
 * 
 * 支持的 Legado 规则类型:
 * - XPath 规则 (以 // 或 @XPath: 开头)
 * - JSOUP Default 规则 (基本的 @ 分隔符规则)
 * - CSS 规则 (以 @css: 开头) - 部分支持
 * - JSONPath 规则 (以 $. 或 @json: 开头) - 部分支持
 * 
 * 不支持的规则类型:
 * - JavaScript 规则 (包含 <js> 或 @js:)
 * - 复杂的组合规则
 */

#include "cJSON.h"
#include "LegadoConverter.h"
#include "Utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// 检查规则是否包含不支持的 JS
static BOOL contains_js(const char* rule)
{
    if (!rule || strlen(rule) == 0)
        return FALSE;
    
    if (strstr(rule, "@js:") != NULL)
        return TRUE;
    if (strstr(rule, "<js>") != NULL)
        return TRUE;
    if (strstr(rule, "{{") != NULL && strstr(rule, "}}") != NULL)
    {
        // 检查 {{}} 中是否包含复杂 JS
        const char* start = strstr(rule, "{{");
        const char* end = strstr(rule, "}}");
        if (start && end && end > start)
        {
            // 简单的变量替换如 {{key}} 或 {{page}} 是允许的
            int len = end - start - 2;
            if (len > 10) // 超过简单变量长度，可能是 JS
                return TRUE;
        }
    }
    return FALSE;
}

// 检查是否为 XPath 规则
static BOOL is_xpath_rule(const char* rule)
{
    if (!rule || strlen(rule) == 0)
        return FALSE;
    
    // 以 // 开头是 XPath
    if (strncmp(rule, "//", 2) == 0)
        return TRUE;
    
    // 以 @XPath: 开头
    if (strncmp(rule, "@XPath:", 7) == 0)
        return TRUE;
    
    // 以 @xpath: 开头 (小写)
    if (strncmp(rule, "@xpath:", 7) == 0)
        return TRUE;
    
    return FALSE;
}

// 检查是否为 JSONPath 规则
static BOOL is_jsonpath_rule(const char* rule)
{
    if (!rule || strlen(rule) == 0)
        return FALSE;
    
    // 以 $. 开头是 JSONPath
    if (strncmp(rule, "$.", 2) == 0)
        return TRUE;
    
    // 以 @json: 开头
    if (strncmp(rule, "@json:", 6) == 0)
        return TRUE;
    
    return FALSE;
}

// 检查是否为 CSS 规则
static BOOL is_css_rule(const char* rule)
{
    if (!rule || strlen(rule) == 0)
        return FALSE;
    
    // 以 @css: 开头
    if (strncmp(rule, "@css:", 5) == 0)
        return TRUE;
    
    return FALSE;
}

// 将 JSOUP Default 规则转换为 XPath
// JSOUP 格式: class.name.0@tag.a@text
// XPath 格式: //*[@class='name'][1]//a/text()
static BOOL convert_jsoup_to_xpath(const char* jsoup_rule, char* xpath_out, int max_len)
{
    if (!jsoup_rule || !xpath_out || max_len <= 0)
        return FALSE;
    
    // 简单的 JSOUP 规则转换
    // 这是一个简化的实现，只处理常见情况
    
    char temp[2048] = {0};
    char* rule_copy = _strdup(jsoup_rule);
    char* token;
    char* context = NULL;
    BOOL first = TRUE;
    
    strcpy(temp, "//");
    
    token = strtok_s(rule_copy, "@", &context);
    while (token != NULL)
    {
        char type[64] = {0};
        char name[256] = {0};
        char index[32] = {0};
        
        // 解析 type.name.index 格式
        char* dot1 = strchr(token, '.');
        if (dot1)
        {
            strncpy(type, token, dot1 - token);
            char* dot2 = strchr(dot1 + 1, '.');
            if (dot2)
            {
                strncpy(name, dot1 + 1, dot2 - dot1 - 1);
                strcpy(index, dot2 + 1);
            }
            else
            {
                strcpy(name, dot1 + 1);
            }
        }
        else
        {
            strcpy(type, token);
        }
        
        // 转换为 XPath
        if (strcmp(type, "class") == 0)
        {
            strcat(temp, "*[@class='");
            strcat(temp, name);
            strcat(temp, "']");
            if (strlen(index) > 0)
            {
                strcat(temp, "[");
                strcat(temp, index);
                strcat(temp, "]");
            }
        }
        else if (strcmp(type, "id") == 0)
        {
            strcat(temp, "*[@id='");
            strcat(temp, name);
            strcat(temp, "']");
        }
        else if (strcmp(type, "tag") == 0)
        {
            if (!first)
                strcat(temp, "//");
            strcat(temp, name);
            if (strlen(index) > 0)
            {
                strcat(temp, "[");
                strcat(temp, index);
                strcat(temp, "]");
            }
        }
        else if (strcmp(type, "text") == 0)
        {
            strcat(temp, "/text()");
        }
        else if (strcmp(type, "href") == 0)
        {
            strcat(temp, "/@href");
        }
        else if (strcmp(type, "src") == 0)
        {
            strcat(temp, "/@src");
        }
        else if (strcmp(type, "html") == 0 || strcmp(type, "all") == 0)
        {
            // html/all 保持原样，Reader 会处理
        }
        
        first = FALSE;
        token = strtok_s(NULL, "@", &context);
    }
    
    free(rule_copy);
    
    if (strlen(temp) > 2)
    {
        strncpy(xpath_out, temp, max_len - 1);
        xpath_out[max_len - 1] = '\0';
        return TRUE;
    }
    
    return FALSE;
}

// 将 JSONPath 规则转换为 XPath (仅支持简单情况)
static BOOL convert_jsonpath_to_xpath(const char* json_rule, char* xpath_out, int max_len)
{
    if (!json_rule || !xpath_out || max_len <= 0)
        return FALSE;
    
    // JSONPath 转 XPath 的简单映射
    // $. 开头的 JSONPath 可以部分转换
    // 例如: $.data.list -> /data/list
    
    const char* start = json_rule;
    if (strncmp(start, "@json:", 6) == 0)
        start += 6;
    if (strncmp(start, "$.", 2) == 0)
        start += 1; // 跳过 $，保留 .
    
    char temp[2048] = {0};
    strcpy(temp, "/");
    
    // 将 . 替换为 /
    for (int i = 0; start[i] != '\0' && i < 2000; i++)
    {
        if (start[i] == '.')
            strcat(temp, "/");
        else
        {
            char c[2] = {start[i], '\0'};
            strcat(temp, c);
        }
    }
    
    strncpy(xpath_out, temp, max_len - 1);
    xpath_out[max_len - 1] = '\0';
    return TRUE;
}

// 转换 Legado 规则为 XPath
BOOL convert_legado_rule_to_xpath(const char* legado_rule, char* xpath_out, int max_len)
{
    if (!legado_rule || !xpath_out || max_len <= 0)
    {
        if (xpath_out && max_len > 0)
            xpath_out[0] = '\0';
        return TRUE; // 空规则视为成功
    }
    
    if (strlen(legado_rule) == 0)
    {
        xpath_out[0] = '\0';
        return TRUE;
    }
    
    // 检查是否包含 JS
    if (contains_js(legado_rule))
        return FALSE;
    
    // 处理正则替换部分 ##regex##replacement
    char* rule_copy = _strdup(legado_rule);
    char* regex_part = strstr(rule_copy, "##");
    if (regex_part)
    {
        *regex_part = '\0'; // 截断正则部分，只处理规则部分
    }
    
    BOOL result = FALSE;
    
    // 如果已经是 XPath 格式
    if (is_xpath_rule(rule_copy))
    {
        const char* xpath_start = rule_copy;
        if (strncmp(xpath_start, "@XPath:", 7) == 0 || strncmp(xpath_start, "@xpath:", 7) == 0)
            xpath_start += 7;
        
        strncpy(xpath_out, xpath_start, max_len - 1);
        xpath_out[max_len - 1] = '\0';
        result = TRUE;
    }
    // JSONPath 格式
    else if (is_jsonpath_rule(rule_copy))
    {
        result = convert_jsonpath_to_xpath(rule_copy, xpath_out, max_len);
    }
    // CSS 格式 - 暂不支持完整转换
    else if (is_css_rule(rule_copy))
    {
        // CSS 规则较复杂，这里只做简单处理
        // @css:selector@attr 格式
        const char* css_start = rule_copy + 5; // 跳过 @css:
        
        // 尝试提取选择器和属性
        char* attr_sep = strstr((char*)css_start, "@");
        if (attr_sep)
        {
            *attr_sep = '\0';
            char* attr = attr_sep + 1;
            
            // 简单的 CSS 选择器转 XPath
            // 例如: #id -> //*[@id='id']
            // 例如: .class -> //*[@class='class']
            char temp[2048] = {0};
            
            if (css_start[0] == '#')
            {
                sprintf(temp, "//*[@id='%s']", css_start + 1);
            }
            else if (css_start[0] == '.')
            {
                sprintf(temp, "//*[contains(@class,'%s')]", css_start + 1);
            }
            else
            {
                sprintf(temp, "//%s", css_start);
            }
            
            // 添加属性
            if (strcmp(attr, "text") == 0)
                strcat(temp, "/text()");
            else if (strcmp(attr, "href") == 0)
                strcat(temp, "/@href");
            else if (strcmp(attr, "src") == 0)
                strcat(temp, "/@src");
            else if (strcmp(attr, "content") == 0)
                strcat(temp, "/@content");
            
            strncpy(xpath_out, temp, max_len - 1);
            xpath_out[max_len - 1] = '\0';
            result = TRUE;
        }
        else
        {
            result = FALSE;
        }
    }
    // JSOUP Default 格式
    else if (strchr(rule_copy, '@') != NULL || strchr(rule_copy, '.') != NULL)
    {
        result = convert_jsoup_to_xpath(rule_copy, xpath_out, max_len);
    }
    else
    {
        // 简单文本，可能是属性名
        strncpy(xpath_out, rule_copy, max_len - 1);
        xpath_out[max_len - 1] = '\0';
        result = TRUE;
    }
    
    free(rule_copy);
    return result;
}

// 解析 Legado searchUrl
BOOL parse_legado_search_url(const char* search_url, char* query_url, int* query_method, 
                              char* query_params, int* query_charset)
{
    if (!search_url || !query_url)
        return FALSE;
    
    // 默认值
    *query_method = 0; // GET
    *query_charset = 1; // UTF-8
    if (query_params)
        query_params[0] = '\0';
    
    // 检查是否包含 JSON 配置
    const char* json_start = strchr(search_url, ',');
    if (json_start && json_start[1] == '{')
    {
        // 复制 URL 部分
        int url_len = json_start - search_url;
        strncpy(query_url, search_url, url_len);
        query_url[url_len] = '\0';
        
        // 解析 JSON 配置
        cJSON* config = cJSON_Parse(json_start + 1);
        if (config)
        {
            cJSON* method = cJSON_GetObjectItem(config, "method");
            if (method && method->valuestring)
            {
                if (strcmp(method->valuestring, "POST") == 0)
                    *query_method = 1;
            }
            
            cJSON* charset = cJSON_GetObjectItem(config, "charset");
            if (charset && charset->valuestring)
            {
                if (strcmp(charset->valuestring, "gbk") == 0 || 
                    strcmp(charset->valuestring, "GBK") == 0)
                    *query_charset = 2;
            }
            
            cJSON* body = cJSON_GetObjectItem(config, "body");
            if (body && body->valuestring && query_params)
            {
                strcpy(query_params, body->valuestring);
            }
            
            cJSON_Delete(config);
        }
    }
    else
    {
        strcpy(query_url, search_url);
    }
    
    // 将 {{key}} 替换为 %s
    char* key_pos = strstr(query_url, "{{key}}");
    if (key_pos)
    {
        char temp[2048] = {0};
        int prefix_len = key_pos - query_url;
        strncpy(temp, query_url, prefix_len);
        strcat(temp, "%s");
        strcat(temp, key_pos + 7);
        strcpy(query_url, temp);
    }
    
    // 处理 searchKey 变量
    key_pos = strstr(query_url, "searchKey");
    if (key_pos)
    {
        // 将 searchKey 替换为 %s
        char temp[2048] = {0};
        int prefix_len = key_pos - query_url;
        strncpy(temp, query_url, prefix_len);
        strcat(temp, "%s");
        strcat(temp, key_pos + 9);
        strcpy(query_url, temp);
    }
    
    // 同样处理 query_params
    if (query_params && strlen(query_params) > 0)
    {
        key_pos = strstr(query_params, "{{key}}");
        if (key_pos)
        {
            char temp[2048] = {0};
            int prefix_len = key_pos - query_params;
            strncpy(temp, query_params, prefix_len);
            strcat(temp, "%s");
            strcat(temp, key_pos + 7);
            strcpy(query_params, temp);
        }
    }
    
    return TRUE;
}

// 检测是否为 Legado 格式
BOOL is_legado_format(const char* json)
{
    if (!json)
        return FALSE;
    
    cJSON* root = cJSON_Parse(json);
    if (!root)
        return FALSE;
    
    BOOL is_legado = FALSE;
    
    // Legado 格式是数组，每个元素有 bookSourceUrl 字段
    if (cJSON_IsArray(root))
    {
        cJSON* first = cJSON_GetArrayItem(root, 0);
        if (first)
        {
            cJSON* url = cJSON_GetObjectItem(first, "bookSourceUrl");
            cJSON* name = cJSON_GetObjectItem(first, "bookSourceName");
            if (url && name)
                is_legado = TRUE;
        }
    }
    // 也可能是单个书源对象
    else if (cJSON_IsObject(root))
    {
        cJSON* url = cJSON_GetObjectItem(root, "bookSourceUrl");
        cJSON* name = cJSON_GetObjectItem(root, "bookSourceName");
        if (url && name)
            is_legado = TRUE;
    }
    
    cJSON_Delete(root);
    return is_legado;
}

// 转换单个 Legado 书源
static BOOL convert_single_legado_source(cJSON* source, book_source_t* bs)
{
    if (!source || !bs)
        return FALSE;
    
    memset(bs, 0, sizeof(book_source_t));
    
    // 基本信息
    cJSON* name = cJSON_GetObjectItem(source, "bookSourceName");
    cJSON* url = cJSON_GetObjectItem(source, "bookSourceUrl");
    cJSON* search_url = cJSON_GetObjectItem(source, "searchUrl");
    
    if (!name || !url || !search_url)
        return FALSE;
    
    // 检查书源类型，只支持文本类型 (type=0)
    cJSON* type = cJSON_GetObjectItem(source, "bookSourceType");
    if (type && type->valueint != 0)
        return FALSE; // 跳过音频、图片等类型
    
    // 检查是否启用
    cJSON* enabled = cJSON_GetObjectItem(source, "enabled");
    if (enabled && !cJSON_IsTrue(enabled))
        return FALSE; // 跳过禁用的书源
    
    // 设置书源名称
    if (name->valuestring)
        wcscpy(bs->title, Utf8ToUtf16(name->valuestring));
    
    // 设置 host
    if (url->valuestring)
        strcpy(bs->host, url->valuestring);
    
    // 解析搜索 URL
    if (search_url->valuestring)
    {
        if (!parse_legado_search_url(search_url->valuestring, bs->query_url, 
                                      &bs->query_method, bs->query_params, &bs->query_charset))
            return FALSE;
    }
    
    // 获取搜索规则
    cJSON* rule_search = cJSON_GetObjectItem(source, "ruleSearch");
    if (rule_search)
    {
        cJSON* book_list = cJSON_GetObjectItem(rule_search, "bookList");
        cJSON* book_name = cJSON_GetObjectItem(rule_search, "name");
        cJSON* book_url = cJSON_GetObjectItem(rule_search, "bookUrl");
        cJSON* author = cJSON_GetObjectItem(rule_search, "author");
        
        // 转换书名规则
        if (book_name && book_name->valuestring)
        {
            char combined_rule[2048] = {0};
            if (book_list && book_list->valuestring)
            {
                // 组合 bookList 和 name 规则
                sprintf(combined_rule, "%s", book_name->valuestring);
            }
            else
            {
                strcpy(combined_rule, book_name->valuestring);
            }
            
            if (!convert_legado_rule_to_xpath(combined_rule, bs->book_name_xpath, sizeof(bs->book_name_xpath)))
                return FALSE;
        }
        
        // 转换书籍URL规则
        if (book_url && book_url->valuestring)
        {
            if (!convert_legado_rule_to_xpath(book_url->valuestring, bs->book_mainpage_xpath, sizeof(bs->book_mainpage_xpath)))
                return FALSE;
        }
        
        // 转换作者规则
        if (author && author->valuestring)
        {
            if (!convert_legado_rule_to_xpath(author->valuestring, bs->book_author_xpath, sizeof(bs->book_author_xpath)))
                return FALSE;
        }
    }
    
    // 获取目录规则
    cJSON* rule_toc = cJSON_GetObjectItem(source, "ruleToc");
    if (rule_toc)
    {
        cJSON* chapter_list = cJSON_GetObjectItem(rule_toc, "chapterList");
        cJSON* chapter_name = cJSON_GetObjectItem(rule_toc, "chapterName");
        cJSON* chapter_url = cJSON_GetObjectItem(rule_toc, "chapterUrl");
        cJSON* next_toc_url = cJSON_GetObjectItem(rule_toc, "nextTocUrl");
        
        // 转换章节标题规则
        if (chapter_name && chapter_name->valuestring)
        {
            if (!convert_legado_rule_to_xpath(chapter_name->valuestring, bs->chapter_title_xpath, sizeof(bs->chapter_title_xpath)))
                return FALSE;
        }
        else if (chapter_list && chapter_list->valuestring)
        {
            // 如果没有单独的 chapterName，使用 chapterList
            if (!convert_legado_rule_to_xpath(chapter_list->valuestring, bs->chapter_title_xpath, sizeof(bs->chapter_title_xpath)))
                return FALSE;
        }
        
        // 转换章节URL规则
        if (chapter_url && chapter_url->valuestring)
        {
            if (!convert_legado_rule_to_xpath(chapter_url->valuestring, bs->chapter_url_xpath, sizeof(bs->chapter_url_xpath)))
                return FALSE;
        }
        
        // 目录下一页
        if (next_toc_url && next_toc_url->valuestring && strlen(next_toc_url->valuestring) > 0)
        {
            bs->enable_chapter_next = 1;
            if (!convert_legado_rule_to_xpath(next_toc_url->valuestring, bs->chapter_next_url_xpath, sizeof(bs->chapter_next_url_xpath)))
            {
                bs->enable_chapter_next = 0;
            }
        }
    }
    
    // 获取正文规则
    cJSON* rule_content = cJSON_GetObjectItem(source, "ruleContent");
    if (rule_content)
    {
        cJSON* content = cJSON_GetObjectItem(rule_content, "content");
        cJSON* next_content_url = cJSON_GetObjectItem(rule_content, "nextContentUrl");
        cJSON* replace_regex = cJSON_GetObjectItem(rule_content, "replaceRegex");
        
        // 转换正文规则
        if (content && content->valuestring)
        {
            if (!convert_legado_rule_to_xpath(content->valuestring, bs->content_xpath, sizeof(bs->content_xpath)))
                return FALSE;
        }
        
        // 正文下一页
        if (next_content_url && next_content_url->valuestring && strlen(next_content_url->valuestring) > 0)
        {
            bs->enable_content_next = 1;
            if (!convert_legado_rule_to_xpath(next_content_url->valuestring, bs->content_next_url_xpath, sizeof(bs->content_next_url_xpath)))
            {
                bs->enable_content_next = 0;
            }
        }
        
        // 内容过滤
        if (replace_regex && replace_regex->valuestring && strlen(replace_regex->valuestring) > 0)
        {
            bs->content_filter_type = 2; // 正则表达式
            wcscpy(bs->content_filter_keyword, Utf8ToUtf16(replace_regex->valuestring));
        }
    }
    
    // 获取详情页规则 (用于判断是否需要章节列表页)
    cJSON* rule_book_info = cJSON_GetObjectItem(source, "ruleBookInfo");
    if (rule_book_info)
    {
        cJSON* toc_url = cJSON_GetObjectItem(rule_book_info, "tocUrl");
        if (toc_url && toc_url->valuestring && strlen(toc_url->valuestring) > 0)
        {
            bs->enable_chapter_page = 1;
            if (!convert_legado_rule_to_xpath(toc_url->valuestring, bs->chapter_page_xpath, sizeof(bs->chapter_page_xpath)))
            {
                bs->enable_chapter_page = 0;
            }
        }
    }
    
    return TRUE;
}

// 转换 Legado 书源为 Reader 格式
BOOL convert_legado_to_reader(const char* json, book_source_t* bs, int* count, legado_convert_result_t* result)
{
    if (!json || !bs || !count)
        return FALSE;
    
    if (result)
    {
        memset(result, 0, sizeof(legado_convert_result_t));
    }
    
    *count = 0;
    
    cJSON* root = cJSON_Parse(json);
    if (!root)
    {
        if (result)
            strcpy(result->error_msg, "JSON parse error");
        return FALSE;
    }
    
    // 处理数组格式
    if (cJSON_IsArray(root))
    {
        int array_size = cJSON_GetArraySize(root);
        for (int i = 0; i < array_size && *count < MAX_BOOKSRC_COUNT; i++)
        {
            cJSON* item = cJSON_GetArrayItem(root, i);
            if (item)
            {
                if (convert_single_legado_source(item, &bs[*count]))
                {
                    (*count)++;
                    if (result)
                        result->success_count++;
                }
                else
                {
                    if (result)
                        result->skipped_count++;
                }
            }
        }
    }
    // 处理单个对象格式
    else if (cJSON_IsObject(root))
    {
        if (convert_single_legado_source(root, &bs[0]))
        {
            *count = 1;
            if (result)
                result->success_count = 1;
        }
        else
        {
            if (result)
                result->skipped_count = 1;
        }
    }
    
    cJSON_Delete(root);
    
    if (*count > 0)
    {
        return TRUE;
    }
    else
    {
        if (result)
            strcpy(result->error_msg, "No compatible book sources found");
        return FALSE;
    }
}
