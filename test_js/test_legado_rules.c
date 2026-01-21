/*
 * test_legado_rules.c - 完整的 Legado 书源规则解析示例
 * 
 * 本程序演示如何使用 QuickJS 引擎解析复杂的 Legado 书源规则，包括：
 * 1. 搜索 URL 模板处理
 * 2. 搜索结果解析（含 JS 后处理）
 * 3. 章节列表解析
 * 4. 正文内容解析（含反爬虫 JS 解密）
 * 5. 变量存取和 HTTP 请求模拟
 * 
 * 编译命令:
 * gcc -o test_legado_rules test_legado_rules.c \
 *     ../opensrc/quickjs/quickjs.c \
 *     ../opensrc/quickjs/cutils.c \
 *     ../opensrc/quickjs/libregexp.c \
 *     ../opensrc/quickjs/libunicode.c \
 *     ../opensrc/quickjs/dtoa.c \
 *     -I../opensrc/quickjs \
 *     -lm -lpthread \
 *     -DCONFIG_VERSION=\"test\" -O2
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "quickjs.h"

// ============ 全局变量 ============
static JSRuntime *g_runtime = NULL;
static JSContext *g_context = NULL;

// 模拟的变量存储
#define MAX_VARS 100
static struct {
    char key[64];
    char value[1024];
} g_variables[MAX_VARS];
static int g_var_count = 0;

// ============ 辅助函数 ============

// 存储变量
static void store_variable(const char *key, const char *value) {
    for (int i = 0; i < g_var_count; i++) {
        if (strcmp(g_variables[i].key, key) == 0) {
            strncpy(g_variables[i].value, value, sizeof(g_variables[i].value) - 1);
            return;
        }
    }
    if (g_var_count < MAX_VARS) {
        strncpy(g_variables[g_var_count].key, key, sizeof(g_variables[g_var_count].key) - 1);
        strncpy(g_variables[g_var_count].value, value, sizeof(g_variables[g_var_count].value) - 1);
        g_var_count++;
    }
}

// 获取变量
static const char* get_variable(const char *key) {
    for (int i = 0; i < g_var_count; i++) {
        if (strcmp(g_variables[i].key, key) == 0) {
            return g_variables[i].value;
        }
    }
    return "";
}

// ============ JS 原生函数实现 ============

// java.log(msg)
static JSValue js_log(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc >= 1) {
        const char *str = JS_ToCString(ctx, argv[0]);
        if (str) {
            printf("    [JS LOG] %s\n", str);
            JS_FreeCString(ctx, str);
        }
    }
    return JS_UNDEFINED;
}

// java.get(key) - 获取存储的变量
static JSValue js_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1) return JS_NewString(ctx, "");
    const char *key = JS_ToCString(ctx, argv[0]);
    if (!key) return JS_NewString(ctx, "");
    const char *value = get_variable(key);
    JS_FreeCString(ctx, key);
    return JS_NewString(ctx, value);
}

// java.put(key, value) - 存储变量
static JSValue js_put(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 2) return JS_UNDEFINED;
    const char *key = JS_ToCString(ctx, argv[0]);
    const char *value = JS_ToCString(ctx, argv[1]);
    if (key && value) {
        store_variable(key, value);
    }
    if (key) JS_FreeCString(ctx, key);
    if (value) JS_FreeCString(ctx, value);
    return JS_UNDEFINED;
}

// java.ajax(url) - 模拟 HTTP GET 请求
static JSValue js_ajax(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1) return JS_NewString(ctx, "");
    const char *url = JS_ToCString(ctx, argv[0]);
    if (!url) return JS_NewString(ctx, "");
    
    printf("    [HTTP GET] %s\n", url);
    
    // 模拟返回数据（实际应用中这里会发起真实 HTTP 请求）
    const char *mock_response = "{\"code\":0,\"data\":{\"list\":[{\"name\":\"测试书籍\",\"author\":\"测试作者\"}]}}";
    JS_FreeCString(ctx, url);
    return JS_NewString(ctx, mock_response);
}

// java.post(url, body, headers) - 模拟 HTTP POST 请求
static JSValue js_post(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 2) return JS_NewString(ctx, "");
    const char *url = JS_ToCString(ctx, argv[0]);
    const char *body = JS_ToCString(ctx, argv[1]);
    
    printf("    [HTTP POST] %s\n", url);
    printf("    [POST BODY] %s\n", body);
    
    // 模拟返回数据
    const char *mock_response = "{\"success\":true,\"content\":\"这是解密后的正文内容...\"}";
    
    if (url) JS_FreeCString(ctx, url);
    if (body) JS_FreeCString(ctx, body);
    return JS_NewString(ctx, mock_response);
}

// java.md5Encode(str) - MD5 哈希（简化实现）
static JSValue js_md5Encode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1) return JS_NewString(ctx, "");
    const char *str = JS_ToCString(ctx, argv[0]);
    if (!str) return JS_NewString(ctx, "");
    
    // 简化的 MD5 模拟（实际应使用真实 MD5 算法）
    char hash[33];
    unsigned int h = 0;
    for (const char *p = str; *p; p++) {
        h = h * 31 + (unsigned char)*p;
    }
    snprintf(hash, sizeof(hash), "%08x%08x%08x%08x", h, h ^ 0x12345678, h ^ 0x87654321, h ^ 0xabcdef01);
    
    JS_FreeCString(ctx, str);
    return JS_NewString(ctx, hash);
}

// java.base64Encode(str)
static const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static JSValue js_base64Encode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1) return JS_NewString(ctx, "");
    const char *input = JS_ToCString(ctx, argv[0]);
    if (!input) return JS_NewString(ctx, "");
    
    size_t in_len = strlen(input);
    size_t out_len = 4 * ((in_len + 2) / 3);
    char *output = malloc(out_len + 1);
    if (!output) {
        JS_FreeCString(ctx, input);
        return JS_NewString(ctx, "");
    }
    
    size_t i, j;
    for (i = 0, j = 0; i < in_len;) {
        uint32_t a = i < in_len ? (unsigned char)input[i++] : 0;
        uint32_t b = i < in_len ? (unsigned char)input[i++] : 0;
        uint32_t c = i < in_len ? (unsigned char)input[i++] : 0;
        uint32_t triple = (a << 16) + (b << 8) + c;
        output[j++] = base64_chars[(triple >> 18) & 0x3F];
        output[j++] = base64_chars[(triple >> 12) & 0x3F];
        output[j++] = base64_chars[(triple >> 6) & 0x3F];
        output[j++] = base64_chars[triple & 0x3F];
    }
    
    int mod = in_len % 3;
    if (mod > 0) {
        output[out_len - 1] = '=';
        if (mod == 1) output[out_len - 2] = '=';
    }
    output[out_len] = '\0';
    
    JS_FreeCString(ctx, input);
    JSValue result = JS_NewString(ctx, output);
    free(output);
    return result;
}

// java.base64Decode(str)
static JSValue js_base64Decode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1) return JS_NewString(ctx, "");
    const char *input = JS_ToCString(ctx, argv[0]);
    if (!input) return JS_NewString(ctx, "");
    
    size_t in_len = strlen(input);
    char *output = malloc(in_len);
    if (!output) {
        JS_FreeCString(ctx, input);
        return JS_NewString(ctx, "");
    }
    
    int val = 0, valb = -8;
    size_t j = 0;
    for (size_t i = 0; i < in_len; i++) {
        char c = input[i];
        const char *p = strchr(base64_chars, c);
        if (p == NULL) continue;
        val = (val << 6) + (int)(p - base64_chars);
        valb += 6;
        if (valb >= 0) {
            output[j++] = (char)((val >> valb) & 0xFF);
            valb -= 8;
        }
    }
    output[j] = '\0';
    
    JS_FreeCString(ctx, input);
    JSValue result = JS_NewString(ctx, output);
    free(output);
    return result;
}

// java.encodeURI(str, charset)
static JSValue js_encodeURI(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1) return JS_NewString(ctx, "");
    const char *str = JS_ToCString(ctx, argv[0]);
    if (!str) return JS_NewString(ctx, "");
    
    size_t len = strlen(str);
    char *output = malloc(len * 3 + 1);
    if (!output) {
        JS_FreeCString(ctx, str);
        return JS_NewString(ctx, "");
    }
    
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = str[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            output[j++] = c;
        } else {
            sprintf(output + j, "%%%02X", c);
            j += 3;
        }
    }
    output[j] = '\0';
    
    JS_FreeCString(ctx, str);
    JSValue result = JS_NewString(ctx, output);
    free(output);
    return result;
}

// java.timeFormat(timestamp)
static JSValue js_timeFormat(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    int64_t timestamp;
    if (argc < 1 || JS_ToInt64(ctx, &timestamp, argv[0]) < 0) {
        return JS_NewString(ctx, "");
    }
    
    time_t t = (time_t)(timestamp / 1000);
    struct tm *tm_info = localtime(&t);
    char buffer[64];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
    return JS_NewString(ctx, buffer);
}

// java.htmlFormat(str) - HTML 实体解码
static JSValue js_htmlFormat(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1) return JS_NewString(ctx, "");
    const char *str = JS_ToCString(ctx, argv[0]);
    if (!str) return JS_NewString(ctx, "");
    
    // 简单的 HTML 实体替换
    char *result = strdup(str);
    // 实际实现中需要更完整的替换逻辑
    
    JS_FreeCString(ctx, str);
    JSValue ret = JS_NewString(ctx, result);
    free(result);
    return ret;
}

// ============ 引擎初始化 ============

static void register_java_object(JSContext *ctx) {
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue java_obj = JS_NewObject(ctx);
    
    // 注册所有 java.* 方法
    JS_SetPropertyStr(ctx, java_obj, "log", JS_NewCFunction(ctx, js_log, "log", 1));
    JS_SetPropertyStr(ctx, java_obj, "get", JS_NewCFunction(ctx, js_get, "get", 1));
    JS_SetPropertyStr(ctx, java_obj, "put", JS_NewCFunction(ctx, js_put, "put", 2));
    JS_SetPropertyStr(ctx, java_obj, "ajax", JS_NewCFunction(ctx, js_ajax, "ajax", 1));
    JS_SetPropertyStr(ctx, java_obj, "post", JS_NewCFunction(ctx, js_post, "post", 3));
    JS_SetPropertyStr(ctx, java_obj, "md5Encode", JS_NewCFunction(ctx, js_md5Encode, "md5Encode", 1));
    JS_SetPropertyStr(ctx, java_obj, "base64Encode", JS_NewCFunction(ctx, js_base64Encode, "base64Encode", 1));
    JS_SetPropertyStr(ctx, java_obj, "base64Decode", JS_NewCFunction(ctx, js_base64Decode, "base64Decode", 1));
    JS_SetPropertyStr(ctx, java_obj, "encodeURI", JS_NewCFunction(ctx, js_encodeURI, "encodeURI", 2));
    JS_SetPropertyStr(ctx, java_obj, "timeFormat", JS_NewCFunction(ctx, js_timeFormat, "timeFormat", 1));
    JS_SetPropertyStr(ctx, java_obj, "htmlFormat", JS_NewCFunction(ctx, js_htmlFormat, "htmlFormat", 1));
    
    JS_SetPropertyStr(ctx, global, "java", java_obj);
    
    // 添加 console.log
    JSValue console_obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, console_obj, "log", JS_NewCFunction(ctx, js_log, "log", 1));
    JS_SetPropertyStr(ctx, global, "console", console_obj);
    
    JS_FreeValue(ctx, global);
}

static int init_js_engine() {
    g_runtime = JS_NewRuntime();
    if (!g_runtime) return -1;
    
    JS_SetMemoryLimit(g_runtime, 16 * 1024 * 1024);
    
    g_context = JS_NewContext(g_runtime);
    if (!g_context) {
        JS_FreeRuntime(g_runtime);
        return -1;
    }
    
    register_java_object(g_context);
    return 0;
}

static void cleanup_js_engine() {
    if (g_context) JS_FreeContext(g_context);
    if (g_runtime) JS_FreeRuntime(g_runtime);
}

// ============ 规则执行函数 ============

// 设置全局变量
static void set_js_variable(const char *name, const char *value) {
    JSValue global = JS_GetGlobalObject(g_context);
    JS_SetPropertyStr(g_context, global, name, JS_NewString(g_context, value));
    JS_FreeValue(g_context, global);
}

// 设置 book 对象
static void set_book_info(const char *name, const char *author, const char *url) {
    JSValue global = JS_GetGlobalObject(g_context);
    JSValue book_obj = JS_NewObject(g_context);
    JS_SetPropertyStr(g_context, book_obj, "name", JS_NewString(g_context, name));
    JS_SetPropertyStr(g_context, book_obj, "author", JS_NewString(g_context, author));
    JS_SetPropertyStr(g_context, book_obj, "bookUrl", JS_NewString(g_context, url));
    JS_SetPropertyStr(g_context, global, "book", book_obj);
    JS_FreeValue(g_context, global);
}

// 执行 JS 代码并返回结果
static char* evaluate_js(const char *code) {
    JSValue val = JS_Eval(g_context, code, strlen(code), "<eval>", JS_EVAL_TYPE_GLOBAL);
    
    if (JS_IsException(val)) {
        JSValue exception = JS_GetException(g_context);
        const char *err = JS_ToCString(g_context, exception);
        printf("    [JS ERROR] %s\n", err ? err : "unknown");
        if (err) JS_FreeCString(g_context, err);
        JS_FreeValue(g_context, exception);
        JS_FreeValue(g_context, val);
        return strdup("");
    }
    
    const char *str = JS_ToCString(g_context, val);
    char *result = str ? strdup(str) : strdup("");
    if (str) JS_FreeCString(g_context, str);
    JS_FreeValue(g_context, val);
    return result;
}

// ============ 测试用例 ============

void print_separator(const char *title) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║ %-62s ║\n", title);
    printf("╚══════════════════════════════════════════════════════════════╝\n");
}

void print_subsection(const char *title) {
    printf("\n  ┌─ %s ─┐\n", title);
}

// 测试 1: 搜索 URL 模板处理
void test_search_url_template() {
    print_separator("测试 1: 搜索 URL 模板处理");
    
    // 模拟 Legado 书源的 searchUrl
    const char *search_url_template = 
        "https://www.example.com/search?q={{java.encodeURI(key)}}&page={{page}}&sign={{java.md5Encode(key + '123456')}}";
    
    printf("\n  书源 searchUrl 模板:\n");
    printf("  %s\n", search_url_template);
    
    // 设置变量
    set_js_variable("key", "斗破苍穹");
    set_js_variable("page", "1");
    
    // 处理模板中的 {{}} 表达式
    print_subsection("处理 {{java.encodeURI(key)}}");
    char *encoded_key = evaluate_js("java.encodeURI(key)");
    printf("  结果: %s\n", encoded_key);
    
    print_subsection("处理 {{java.md5Encode(key + '123456')}}");
    char *sign = evaluate_js("java.md5Encode(key + '123456')");
    printf("  结果: %s\n", sign);
    
    print_subsection("最终生成的搜索 URL");
    char final_url[1024];
    snprintf(final_url, sizeof(final_url),
             "https://www.example.com/search?q=%s&page=1&sign=%s",
             encoded_key, sign);
    printf("  %s\n", final_url);
    
    free(encoded_key);
    free(sign);
}

// 测试 2: 搜索结果解析（含 JS 后处理）
void test_search_result_parsing() {
    print_separator("测试 2: 搜索结果解析（含 JS 后处理）");
    
    // 模拟从网页获取的搜索结果 JSON
    const char *mock_search_response = 
        "[{\"name\":\"  斗破苍穹  \",\"author\":\"天蚕土豆\",\"url\":\"/book/123\",\"cover\":\"//img.example.com/cover.jpg\",\"intro\":\"这是简介...\",\"lastChapter\":\"第1000章 大结局\",\"updateTime\":1705651200000},"
        "{\"name\":\"斗破苍穹前传\",\"author\":\"天蚕土豆\",\"url\":\"/book/456\",\"cover\":\"//img.example.com/cover2.jpg\",\"intro\":\"前传简介...\",\"lastChapter\":\"第100章\",\"updateTime\":1705564800000}]";
    
    printf("\n  模拟的搜索结果 JSON:\n");
    printf("  %s\n", mock_search_response);
    
    // 设置 result 变量
    set_js_variable("result", mock_search_response);
    set_js_variable("baseUrl", "https://www.example.com");
    
    // Legado 规则示例: 解析书名并去除空格
    print_subsection("规则: $.name@js:result.trim()");
    const char *name_rule_js = 
        "var data = JSON.parse(result);\n"
        "var names = [];\n"
        "for (var i = 0; i < data.length; i++) {\n"
        "    names.push(data[i].name.trim());\n"
        "}\n"
        "names.join('\\n');";
    
    char *names = evaluate_js(name_rule_js);
    printf("  解析出的书名:\n");
    char *line = strtok(names, "\n");
    int idx = 1;
    while (line) {
        printf("    %d. %s\n", idx++, line);
        line = strtok(NULL, "\n");
    }
    free(names);
    
    // 解析封面 URL（处理协议缺失）
    print_subsection("规则: $.cover@js:处理相对协议 URL");
    const char *cover_rule_js = 
        "var data = JSON.parse(result);\n"
        "var covers = [];\n"
        "for (var i = 0; i < data.length; i++) {\n"
        "    var cover = data[i].cover;\n"
        "    if (cover.startsWith('//')) {\n"
        "        cover = 'https:' + cover;\n"
        "    }\n"
        "    covers.push(cover);\n"
        "}\n"
        "covers.join('\\n');";
    
    char *covers = evaluate_js(cover_rule_js);
    printf("  解析出的封面 URL:\n");
    line = strtok(covers, "\n");
    idx = 1;
    while (line) {
        printf("    %d. %s\n", idx++, line);
        line = strtok(NULL, "\n");
    }
    free(covers);
    
    // 解析更新时间（时间戳转换）
    print_subsection("规则: $.updateTime@js:java.timeFormat(result)");
    const char *time_rule_js = 
        "var data = JSON.parse(result);\n"
        "var times = [];\n"
        "for (var i = 0; i < data.length; i++) {\n"
        "    times.push(java.timeFormat(data[i].updateTime));\n"
        "}\n"
        "times.join('\\n');";
    
    char *times = evaluate_js(time_rule_js);
    printf("  解析出的更新时间:\n");
    line = strtok(times, "\n");
    idx = 1;
    while (line) {
        printf("    %d. %s\n", idx++, line);
        line = strtok(NULL, "\n");
    }
    free(times);
}

// 测试 3: 章节列表解析（含翻页处理）
void test_chapter_list_parsing() {
    print_separator("测试 3: 章节列表解析（含翻页处理）");
    
    // 模拟章节列表 HTML 解析后的 JSON 数据
    const char *mock_chapters = 
        "[{\"title\":\"第一章 陨落的天才\",\"url\":\"/chapter/1\"},"
        "{\"title\":\"第二章 斗之气\",\"url\":\"/chapter/2\"},"
        "{\"title\":\"第三章 客人\",\"url\":\"/chapter/3\"}]";
    
    printf("\n  模拟的章节数据:\n");
    printf("  %s\n", mock_chapters);
    
    set_js_variable("result", mock_chapters);
    set_js_variable("baseUrl", "https://www.example.com");
    
    // 复杂规则: 章节标题格式化 + URL 拼接
    print_subsection("规则: 格式化章节标题并拼接完整 URL");
    const char *chapter_rule_js = 
        "var data = JSON.parse(result);\n"
        "var chapters = [];\n"
        "for (var i = 0; i < data.length; i++) {\n"
        "    var title = data[i].title;\n"
        "    var url = baseUrl + data[i].url;\n"
        "    // 格式化标题：移除多余空格\n"
        "    title = title.replace(/\\s+/g, ' ').trim();\n"
        "    chapters.push(title + ' | ' + url);\n"
        "}\n"
        "chapters.join('\\n');";
    
    char *chapters = evaluate_js(chapter_rule_js);
    printf("  解析出的章节:\n");
    char *line = strtok(chapters, "\n");
    int idx = 1;
    while (line) {
        printf("    %d. %s\n", idx++, line);
        line = strtok(NULL, "\n");
    }
    free(chapters);
    
    // 模拟翻页检测
    print_subsection("规则: 检测是否有下一页");
    set_js_variable("result", "{\"hasNext\":true,\"nextPage\":\"/chapters?page=2\"}");
    const char *next_page_js = 
        "var data = JSON.parse(result);\n"
        "if (data.hasNext) {\n"
        "    baseUrl + data.nextPage;\n"
        "} else {\n"
        "    '';\n"
        "}";
    
    char *next_page = evaluate_js(next_page_js);
    printf("  下一页 URL: %s\n", strlen(next_page) > 0 ? next_page : "(无)");
    free(next_page);
}

// 测试 4: 正文内容解析（含反爬虫 JS 解密）
void test_content_parsing() {
    print_separator("测试 4: 正文内容解析（含反爬虫 JS 解密）");
    
    // 模拟加密的正文内容
    const char *encrypted_content = "6L+Z5piv5Yqg5a+G55qE5q2j5paH5YaF5a65Li4u";  // Base64 编码的内容
    
    printf("\n  模拟的加密正文 (Base64):\n");
    printf("  %s\n", encrypted_content);
    
    set_js_variable("result", encrypted_content);
    
    // 规则: Base64 解密
    print_subsection("规则: @js:java.base64Decode(result)");
    char *decoded = evaluate_js("java.base64Decode(result)");
    printf("  解密后的内容: %s\n", decoded);
    free(decoded);
    
    // 更复杂的解密场景: 需要从服务器获取密钥
    print_subsection("规则: 复杂解密（需要获取密钥）");
    const char *complex_decrypt_js = 
        "// 1. 先获取密钥\n"
        "var keyResponse = java.ajax('https://api.example.com/getKey');\n"
        "java.log('获取密钥响应: ' + keyResponse);\n"
        "\n"
        "// 2. 解析密钥\n"
        "var keyData = JSON.parse(keyResponse);\n"
        "java.log('解析密钥成功');\n"
        "\n"
        "// 3. 使用密钥解密内容（这里简化为 Base64 解码）\n"
        "var content = java.base64Decode(result);\n"
        "java.log('解密完成');\n"
        "\n"
        "// 4. 格式化内容\n"
        "content.replace(/\\n/g, '\\n\\n');";
    
    char *content = evaluate_js(complex_decrypt_js);
    printf("  最终内容: %s\n", content);
    free(content);
}

// 测试 5: 变量存取和跨规则数据传递
void test_variable_storage() {
    print_separator("测试 5: 变量存取和跨规则数据传递");
    
    // 场景: 搜索时保存 cookie，后续请求使用
    print_subsection("场景: 保存和使用 Cookie/Token");
    
    // 第一步: 模拟登录获取 token
    const char *login_js = 
        "// 模拟登录请求\n"
        "var loginResponse = java.post('https://api.example.com/login', '{\"user\":\"test\",\"pass\":\"123\"}');\n"
        "var data = JSON.parse(loginResponse);\n"
        "\n"
        "// 保存 token 供后续使用\n"
        "java.put('token', 'mock_token_12345');\n"
        "java.put('userId', '10086');\n"
        "java.log('Token 已保存');\n"
        "'登录成功';";
    
    char *login_result = evaluate_js(login_js);
    printf("  登录结果: %s\n", login_result);
    free(login_result);
    
    // 第二步: 使用保存的 token 请求数据
    print_subsection("使用保存的 Token 请求书架数据");
    const char *bookshelf_js = 
        "// 获取之前保存的 token\n"
        "var token = java.get('token');\n"
        "var userId = java.get('userId');\n"
        "java.log('使用 Token: ' + token);\n"
        "java.log('用户 ID: ' + userId);\n"
        "\n"
        "// 构造请求 URL\n"
        "var url = 'https://api.example.com/bookshelf?userId=' + userId + '&token=' + token;\n"
        "url;";
    
    char *bookshelf_url = evaluate_js(bookshelf_js);
    printf("  构造的请求 URL: %s\n", bookshelf_url);
    free(bookshelf_url);
}

// 测试 6: 完整的书源规则链
void test_complete_rule_chain() {
    print_separator("测试 6: 完整的书源规则链模拟");
    
    printf("\n  模拟完整的书籍搜索和阅读流程:\n");
    
    // 设置书籍信息
    set_book_info("斗破苍穹", "天蚕土豆", "https://www.example.com/book/123");
    
    // Step 1: 搜索
    print_subsection("Step 1: 构造搜索请求");
    set_js_variable("key", "斗破苍穹");
    char *search_url = evaluate_js(
        "var url = 'https://www.example.com/search';\n"
        "var params = 'q=' + java.encodeURI(key);\n"
        "url + '?' + params;"
    );
    printf("  搜索 URL: %s\n", search_url);
    free(search_url);
    
    // Step 2: 解析搜索结果
    print_subsection("Step 2: 解析搜索结果");
    set_js_variable("result", "[{\"name\":\"斗破苍穹\",\"author\":\"天蚕土豆\",\"bookUrl\":\"/book/123\"}]");
    char *book_info = evaluate_js(
        "var data = JSON.parse(result)[0];\n"
        "'书名: ' + data.name + ', 作者: ' + data.author;"
    );
    printf("  %s\n", book_info);
    free(book_info);
    
    // Step 3: 获取章节列表
    print_subsection("Step 3: 获取章节列表");
    set_js_variable("result", "[{\"title\":\"第一章\",\"url\":\"/c/1\"},{\"title\":\"第二章\",\"url\":\"/c/2\"}]");
    char *chapters = evaluate_js(
        "var data = JSON.parse(result);\n"
        "var list = [];\n"
        "for (var i = 0; i < data.length; i++) {\n"
        "    list.push((i+1) + '. ' + data[i].title);\n"
        "}\n"
        "list.join('\\n');"
    );
    printf("  章节列表:\n");
    char *line = strtok(chapters, "\n");
    while (line) {
        printf("    %s\n", line);
        line = strtok(NULL, "\n");
    }
    free(chapters);
    
    // Step 4: 获取正文
    print_subsection("Step 4: 获取正文内容");
    set_js_variable("result", "ICAgIOesrOS4gOeroO+8muS4jeW5uOeahOWkqeaJjQ==");  // Base64
    char *content = evaluate_js(
        "var decoded = java.base64Decode(result);\n"
        "// 清理内容\n"
        "decoded.trim().replace(/\\s+/g, ' ');"
    );
    printf("  正文内容: %s\n", content);
    free(content);
    
    printf("\n  ✓ 完整流程测试完成!\n");
}

// ============ 主函数 ============

int main(int argc, char *argv[]) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════════════╗\n");
    printf("║     QuickJS + Legado 书源规则解析 完整示例                           ║\n");
    printf("║     Reader 阅读器 JavaScript 引擎集成演示                            ║\n");
    printf("╚══════════════════════════════════════════════════════════════════════╝\n");
    
    // 初始化 JS 引擎
    printf("\n初始化 QuickJS 引擎...\n");
    if (init_js_engine() != 0) {
        printf("错误: 无法初始化 JS 引擎\n");
        return 1;
    }
    printf("✓ JS 引擎初始化成功\n");
    
    // 运行所有测试
    test_search_url_template();
    test_search_result_parsing();
    test_chapter_list_parsing();
    test_content_parsing();
    test_variable_storage();
    test_complete_rule_chain();
    
    // 清理
    cleanup_js_engine();
    
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════════════╗\n");
    printf("║                      所有测试完成!                                   ║\n");
    printf("╚══════════════════════════════════════════════════════════════════════╝\n\n");
    
    return 0;
}
