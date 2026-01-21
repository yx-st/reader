# Reader + Legado 书源集成版 - 编译指南

本文档详细说明如何编译集成了 QuickJS JavaScript 引擎和 Legado 书源支持的 Reader 阅读器。

## 项目概述

本项目在原版 Reader 阅读器基础上，集成了以下功能：

1. **QuickJS JavaScript 引擎**：支持 ES2023 标准的轻量级 JS 引擎
2. **Legado 书源格式支持**：可以直接导入 Legado 格式的书源文件
3. **JS 规则解析**：支持 `@js:`、`<js></js>`、`{{}}` 等 Legado JS 规则语法

## 系统要求

### Windows 编译环境

- **Visual Studio 2019** 或更高版本（推荐 VS 2022）
- **Windows SDK 10.0.17763.0** 或更高版本
- 至少 4GB 可用磁盘空间

### 依赖库

项目已包含所有必要的依赖库（位于 `opensrc/` 目录）：
- cJSON
- zlib
- libxml2
- libhttps
- miniz
- libmobi
- **QuickJS**（新增）

## 编译步骤

### 方法一：使用 Visual Studio

1. **打开解决方案**
   ```
   双击 Reader.sln 文件
   ```

2. **选择配置**
   - 配置：`Release` 或 `Debug`
   - 平台：`x64`（推荐）或 `Win32`

3. **编译项目**
   - 菜单：`生成` → `生成解决方案`
   - 或按 `Ctrl+Shift+B`

4. **获取可执行文件**
   - 编译成功后，可执行文件位于：
     - `x64/Release/Reader.exe`（64位发布版）
     - `x64/Debug/Reader.exe`（64位调试版）

### 方法二：使用命令行

1. **打开开发者命令提示符**
   ```
   开始菜单 → Visual Studio 2019/2022 → x64 Native Tools Command Prompt
   ```

2. **进入项目目录**
   ```batch
   cd path\to\Reader_Final
   ```

3. **编译**
   ```batch
   msbuild Reader.sln /p:Configuration=Release /p:Platform=x64
   ```

## 新增文件说明

### 核心文件

| 文件 | 说明 |
|------|------|
| `Reader/QuickJsEngine.hpp` | QuickJS 引擎 C++ 封装类头文件 |
| `Reader/QuickJsEngine.cpp` | QuickJS 引擎 C++ 封装类实现 |
| `Reader/LegadoRuleParser.h` | Legado 规则解析器头文件 |
| `Reader/LegadoRuleParser.cpp` | Legado 规则解析器实现 |
| `Reader/LegadoBookSource.hpp` | Legado 书源数据结构定义 |
| `Reader/LegadoConverter.h` | Legado 书源格式转换器头文件 |
| `Reader/LegadoConverter.cpp` | Legado 书源格式转换器实现 |

### QuickJS 引擎文件

| 文件 | 说明 |
|------|------|
| `opensrc/quickjs/quickjs.c` | QuickJS 核心引擎 |
| `opensrc/quickjs/quickjs.h` | QuickJS 头文件 |
| `opensrc/quickjs/cutils.c/h` | 工具函数 |
| `opensrc/quickjs/libregexp.c/h` | 正则表达式支持 |
| `opensrc/quickjs/libunicode.c/h` | Unicode 支持 |
| `opensrc/quickjs/dtoa.c/h` | 浮点数转换 |

## 使用说明

### 导入 Legado 书源

1. 打开 Reader
2. 进入书源管理界面
3. 点击"导入"按钮
4. 选择 Legado 格式的 `.json` 书源文件
5. 程序会自动检测并转换格式

### 支持的 Legado 规则类型

| 规则类型 | 示例 | 支持程度 |
|---------|------|---------|
| XPath | `//div[@class='content']/text()` | ✅ 完全支持 |
| CSS 选择器 | `@css:.content@text` | ✅ 大部分支持 |
| JSONPath | `$.data.list[*].name` | ✅ 基本支持 |
| JavaScript | `@js:result.trim()` | ✅ 完全支持 |
| 混合规则 | `//div/text()@js:result.trim()` | ✅ 完全支持 |

### 支持的 Legado java.* API

| API | 功能 |
|-----|------|
| `java.ajax(url)` | GET 请求 |
| `java.post(url, body)` | POST 请求 |
| `java.base64Encode/Decode()` | Base64 编解码 |
| `java.md5Encode()` | MD5 哈希 |
| `java.encodeURI/decodeURI()` | URL 编解码 |
| `java.put/get()` | 变量存取 |
| `java.htmlFormat()` | HTML 实体解码 |
| `java.timeFormat()` | 时间格式化 |

## 常见问题

### Q: 编译时提示找不到 Windows SDK

**A:** 在 Visual Studio Installer 中安装对应版本的 Windows SDK，或修改项目属性中的 Windows SDK 版本。

### Q: 编译时出现 C4996 警告

**A:** 这是安全函数警告，项目已通过 `_CRT_SECURE_NO_WARNINGS` 宏禁用，不影响编译。

### Q: 导入 Legado 书源失败

**A:** 
1. 确保书源文件是有效的 JSON 格式
2. 检查书源是否使用了不支持的 JS 特性
3. 查看日志了解具体错误信息

### Q: JS 规则执行出错

**A:**
1. 检查 JS 语法是否正确
2. 确保使用的 API 在支持列表中
3. 复杂的 JS 规则可能需要简化

## 技术支持

如有问题，请在 GitHub Issues 中反馈。

## 许可证

- Reader 原项目：遵循原项目许可证
- QuickJS：MIT License
- 本集成代码：MIT License
