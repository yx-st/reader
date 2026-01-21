/**
 * QuickJsEngine.hpp
 * 
 * QuickJS JavaScript 引擎的 C++ 封装类
 * 专为 Reader 阅读器集成 Legado 书源设计
 * 
 * 作者: Manus AI
 * 日期: 2025-01-20
 */

#ifndef QUICKJS_ENGINE_HPP
#define QUICKJS_ENGINE_HPP

#include <string>
#include <map>
#include <vector>
#include <functional>
#include <memory>

// QuickJS C 头文件
extern "C" {
#include "quickjs.h"
}

namespace Reader {

/**
 * HTTP 请求回调函数类型
 * 用于在 JS 中调用 java.ajax() 和 java.post() 时执行实际的网络请求
 */
using HttpCallback = std::function<std::string(const std::string& url, 
                                                const std::string& method,
                                                const std::string& body,
                                                const std::map<std::string, std::string>& headers)>;

/**
 * 日志回调函数类型
 */
using LogCallback = std::function<void(const std::string& message)>;

/**
 * QuickJS 引擎封装类
 * 
 * 提供以下功能：
 * 1. JavaScript 代码执行
 * 2. Legado 书源 API 模拟 (java.* 系列函数)
 * 3. 变量存取和跨调用持久化
 * 4. 模板字符串处理 ({{...}} 语法)
 * 5. 规则解析 (@js:, <js></js> 语法)
 */
class QuickJsEngine {
public:
    /**
     * 构造函数
     * @param memoryLimit 内存限制（字节），默认 16MB
     */
    explicit QuickJsEngine(size_t memoryLimit = 16 * 1024 * 1024);
    
    /**
     * 析构函数
     */
    ~QuickJsEngine();
    
    // 禁止拷贝
    QuickJsEngine(const QuickJsEngine&) = delete;
    QuickJsEngine& operator=(const QuickJsEngine&) = delete;
    
    // ============ 基础 JS 执行 ============
    
    /**
     * 执行 JavaScript 代码
     * @param code JavaScript 代码
     * @return 执行结果的字符串表示
     */
    std::string eval(const std::string& code);
    
    /**
     * 执行 JavaScript 代码并返回整数
     * @param code JavaScript 代码
     * @param defaultValue 执行失败时的默认值
     * @return 执行结果
     */
    int evalInt(const std::string& code, int defaultValue = 0);
    
    /**
     * 执行 JavaScript 代码并返回布尔值
     * @param code JavaScript 代码
     * @param defaultValue 执行失败时的默认值
     * @return 执行结果
     */
    bool evalBool(const std::string& code, bool defaultValue = false);
    
    // ============ Legado 规则处理 ============
    
    /**
     * 处理 Legado 规则字符串
     * 支持的格式:
     * - @js:code        -> 执行 code，result 变量包含前置结果
     * - <js>code</js>   -> 同上
     * - {{expression}}  -> 模板替换
     * 
     * @param rule 规则字符串
     * @param result 前置处理的结果（会设置为 JS 中的 result 变量）
     * @return 处理后的结果
     */
    std::string processRule(const std::string& rule, const std::string& result = "");
    
    /**
     * 处理模板字符串
     * 将 {{expression}} 替换为表达式的执行结果
     * 
     * @param templateStr 模板字符串
     * @return 处理后的字符串
     */
    std::string processTemplate(const std::string& templateStr);
    
    /**
     * 判断规则是否包含 JS 代码
     * @param rule 规则字符串
     * @return 是否包含 JS
     */
    static bool containsJs(const std::string& rule);
    
    // ============ 变量管理 ============
    
    /**
     * 设置 JS 变量
     * @param name 变量名
     * @param value 变量值
     */
    void setVariable(const std::string& name, const std::string& value);
    
    /**
     * 获取 JS 变量
     * @param name 变量名
     * @return 变量值，不存在则返回空字符串
     */
    std::string getVariable(const std::string& name);
    
    /**
     * 设置 result 变量（用于规则链处理）
     * @param value result 的值
     */
    void setResult(const std::string& value);
    
    /**
     * 设置 baseUrl 变量
     * @param url 基础 URL
     */
    void setBaseUrl(const std::string& url);
    
    /**
     * 设置搜索关键词
     * @param keyword 关键词
     */
    void setKeyword(const std::string& keyword);
    
    // ============ 回调设置 ============
    
    /**
     * 设置 HTTP 请求回调
     * 当 JS 调用 java.ajax() 或 java.post() 时会调用此回调
     * @param callback HTTP 回调函数
     */
    void setHttpCallback(HttpCallback callback);
    
    /**
     * 设置日志回调
     * 当 JS 调用 java.log() 时会调用此回调
     * @param callback 日志回调函数
     */
    void setLogCallback(LogCallback callback);
    
    // ============ 错误处理 ============
    
    /**
     * 获取最后一次错误信息
     * @return 错误信息
     */
    std::string getLastError() const;
    
    /**
     * 是否有错误
     * @return 是否有错误
     */
    bool hasError() const;
    
    /**
     * 清除错误状态
     */
    void clearError();

private:
    // QuickJS 运行时和上下文
    JSRuntime* m_runtime;
    JSContext* m_context;
    
    // 持久化变量存储
    std::map<std::string, std::string> m_variables;
    
    // 回调函数
    HttpCallback m_httpCallback;
    LogCallback m_logCallback;
    
    // 错误信息
    std::string m_lastError;
    bool m_hasError;
    
    // 初始化 Legado API
    void initLegadoApi();
    
    // 注册 java 对象的方法
    void registerJavaObject();
    
    // 辅助函数
    std::string jsValueToString(JSValue val);
    void setError(const std::string& error);
    
    // 静态回调函数（用于 QuickJS C API）
    static JSValue js_java_log(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_java_ajax(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_java_post(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_java_get(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_java_put(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_java_base64Encode(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_java_base64Decode(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_java_md5Encode(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_java_md5Encode16(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_java_encodeURI(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_java_decodeURI(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_java_timeFormat(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_java_htmlFormat(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
};

} // namespace Reader

#endif // QUICKJS_ENGINE_HPP
