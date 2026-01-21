# QuickJS JavaScript 引擎集成指南

本文档详细说明如何将 QuickJS JavaScript 引擎集成到 Reader 项目中，以支持 Legado 书源的 JavaScript 规则。

## 1. 文件结构

集成后的文件结构如下：

```
Reader/
├── opensrc/
│   └── quickjs/                    # QuickJS 源码
│       ├── quickjs.c               # 核心引擎
│       ├── quickjs.h               # 头文件
│       ├── cutils.c/h              # 工具函数
│       ├── libregexp.c/h           # 正则表达式
│       ├── libunicode.c/h          # Unicode 支持
│       ├── dtoa.c/h                # 数字转换
│       └── list.h                  # 链表
├── Reader/
│   ├── JsEngine.h                  # JS 引擎封装类
│   ├── JsEngine.cpp
│   ├── JsRuleProcessor.h           # 规则处理器
│   └── JsRuleProcessor.cpp
└── doc/
    └── quickjs_integration_guide.md
```

## 2. Visual Studio 项目配置

### 2.1 添加源文件

在 `Reader.vcxproj` 中添加以下源文件：

**C 源文件（编译为 C）：**
```xml
<ClCompile Include="..\opensrc\quickjs\quickjs.c">
  <CompileAs>CompileAsC</CompileAs>
</ClCompile>
<ClCompile Include="..\opensrc\quickjs\cutils.c">
  <CompileAs>CompileAsC</CompileAs>
</ClCompile>
<ClCompile Include="..\opensrc\quickjs\libregexp.c">
  <CompileAs>CompileAsC</CompileAs>
</ClCompile>
<ClCompile Include="..\opensrc\quickjs\libunicode.c">
  <CompileAs>CompileAsC</CompileAs>
</ClCompile>
<ClCompile Include="..\opensrc\quickjs\dtoa.c">
  <CompileAs>CompileAsC</CompileAs>
</ClCompile>
```

**C++ 源文件：**
```xml
<ClCompile Include="JsEngine.cpp" />
<ClCompile Include="JsRuleProcessor.cpp" />
```

### 2.2 添加头文件

```xml
<ClInclude Include="..\opensrc\quickjs\quickjs.h" />
<ClInclude Include="JsEngine.h" />
<ClInclude Include="JsRuleProcessor.h" />
```

### 2.3 配置包含目录

在项目属性 → C/C++ → 常规 → 附加包含目录中添加：
```
..\opensrc\quickjs
```

### 2.4 配置预处理器定义

在项目属性 → C/C++ → 预处理器 → 预处理器定义中添加：
```
CONFIG_VERSION="2025-09-13"
```

### 2.5 禁用特定警告（可选）

QuickJS 源码可能产生一些警告，可以选择性禁用：
```
4244;4267;4996
```

## 3. 使用方法

### 3.1 初始化 JS 引擎

在程序启动时初始化：

```cpp
#include "JsRuleProcessor.h"

// 在 WinMain 或初始化函数中
if (!JsRuleProcessor::Instance()->Initialize()) {
    // 处理初始化失败
    MessageBox(NULL, L"JS引擎初始化失败", L"错误", MB_OK);
}
```

### 3.2 处理包含 JS 的规则

```cpp
#include "JsRuleProcessor.h"

// 检查规则是否包含 JS
const char* rule = "//div[@class='content']/text()@js:result.trim()";
if (JsRuleProcessor::Instance()->RuleContainsJs(rule)) {
    // 使用 JS 处理器
    std::vector<std::string> results;
    JsRuleProcessor::Instance()->ProcessRule(html, htmlLen, rule, results, baseUrl);
} else {
    // 使用原有的 XPath 解析
    HtmlParser::Instance()->HtmlParseByXpath(html, htmlLen, rule, results, &stop);
}
```

### 3.3 处理模板字符串

```cpp
// 处理 {{}} 模板
const char* templateStr = "https://example.com/search?q={{java.encodeURI(key)}}";
std::string url = JsRuleProcessor::Instance()->ProcessTemplate(
    templateStr, 
    searchKey,  // 作为 result 变量
    baseUrl
);
```

### 3.4 设置上下文信息

```cpp
// 设置书籍信息（用于 JS 中的 book 对象）
JsRuleProcessor::Instance()->SetBookInfo(
    bookName,
    bookAuthor,
    bookUrl
);

// 设置章节信息
JsRuleProcessor::Instance()->SetChapterInfo(
    chapterTitle,
    chapterUrl
);
```

### 3.5 设置 HTTP 回调

```cpp
// 为 java.ajax() 提供 HTTP 实现
JsRuleProcessor::Instance()->SetHttpGetCallback([](const std::string& url) -> std::string {
    // 使用你的 HTTP 库发送请求
    char* response = nullptr;
    int len = 0;
    if (hapi_get_url(url.c_str(), &response, &len) == 0) {
        std::string result(response, len);
        free(response);
        return result;
    }
    return "";
});
```

### 3.6 清理

在程序退出时：

```cpp
JsRuleProcessor::ReleaseInstance();
```

## 4. 支持的 Legado JS API

### 4.1 已实现的 API

| API | 功能 | 状态 |
|-----|------|------|
| `java.log(msg)` | 输出调试信息 | ✅ |
| `java.ajax(url)` | HTTP GET 请求 | ✅ (需设置回调) |
| `java.post(url, body, headers)` | HTTP POST 请求 | ✅ (需设置回调) |
| `java.get(key)` | 获取变量 | ✅ |
| `java.put(key, value)` | 存储变量 | ✅ |
| `java.md5Encode(str)` | MD5 哈希 | ✅ |
| `java.md5Encode16(str)` | MD5 哈希 (16位) | ✅ |
| `java.base64Encode(str)` | Base64 编码 | ✅ |
| `java.base64Decode(str)` | Base64 解码 | ✅ |
| `java.encodeURI(str, charset)` | URL 编码 | ✅ (UTF-8) |
| `java.htmlFormat(str)` | HTML 实体解码 | ✅ |
| `java.timeFormat(timestamp)` | 时间格式化 | ✅ |

### 4.2 待实现的 API

| API | 功能 | 优先级 |
|-----|------|--------|
| `java.createSymmetricCrypto()` | AES/DES 加解密 | 高 |
| `java.webView()` | WebView 渲染 | 低 |
| `java.getCookie()` | Cookie 管理 | 中 |
| `java.importScript()` | 导入外部 JS | 低 |

## 5. 支持的规则格式

### 5.1 XPath + JS

```
//div[@class='content']/text()@js:result.trim()
```

先执行 XPath 获取内容，然后将结果作为 `result` 变量传给 JS 处理。

### 5.2 纯 JS

```
<js>
var data = JSON.parse(result);
data.content.replace(/\s+/g, '\n');
</js>
```

### 5.3 模板表达式

```
{{java.encodeURI(key, "gbk")}}
```

## 6. 测试

### 6.1 编译测试程序

```bash
cd Reader/test_js
gcc -o test_quickjs test_quickjs.c \
    ../opensrc/quickjs/quickjs.c \
    ../opensrc/quickjs/cutils.c \
    ../opensrc/quickjs/libregexp.c \
    ../opensrc/quickjs/libunicode.c \
    ../opensrc/quickjs/dtoa.c \
    -I../opensrc/quickjs \
    -lm -lpthread \
    -DCONFIG_VERSION=\"test\"
```

### 6.2 运行测试

```bash
./test_quickjs
```

预期输出：
```
=== QuickJS Integration Test ===
Test 1: Simple expression
  1 + 2 = 3
Test 2: String manipulation
  '  hello world  '.trim().toUpperCase() = 'HELLO WORLD'
...
=== All tests completed ===
```

## 7. 故障排除

### 7.1 编译错误

**问题**: `CONFIG_VERSION` 未定义
**解决**: 添加预处理器定义 `CONFIG_VERSION="2025-09-13"`

**问题**: C 和 C++ 混合编译错误
**解决**: 确保 QuickJS 的 .c 文件设置为 "编译为 C 代码"

### 7.2 运行时错误

**问题**: JS 执行返回空结果
**解决**: 检查 `JsRuleProcessor::GetLastError()` 获取错误信息

**问题**: `java.ajax()` 不工作
**解决**: 确保已调用 `SetHttpGetCallback()` 设置 HTTP 回调

## 8. 性能建议

1. **复用引擎实例**: 使用单例模式避免频繁创建/销毁 JS 引擎
2. **限制执行时间**: 对于不信任的书源，考虑添加超时机制
3. **内存限制**: QuickJS 已设置 16MB 内存限制，可根据需要调整

## 9. 安全注意事项

1. 书源中的 JS 代码来自不可信来源，已通过 QuickJS 沙箱隔离
2. 文件系统访问已禁用
3. 网络访问通过回调控制，可以添加白名单过滤
