# Reader + Legado 书源集成版

这是一个集成了 **QuickJS JavaScript 引擎** 和 **Legado 书源格式支持** 的 Reader 阅读器完整源码包。

## 新增功能

### 1. Legado 书源格式支持
- 可以直接导入 Legado 格式的书源 JSON 文件
- 自动检测并转换书源格式
- 支持大部分 Legado 书源规则

### 2. JavaScript 规则解析
- 集成 QuickJS 引擎（ES2023 标准）
- 支持 `@js:` 格式规则
- 支持 `<js></js>` 格式规则
- 支持 `{{}}` 模板语法

### 3. Legado java.* API 支持
- `java.ajax()` / `java.post()` - HTTP 请求
- `java.base64Encode/Decode()` - Base64 编解码
- `java.md5Encode()` - MD5 哈希
- `java.encodeURI/decodeURI()` - URL 编解码
- `java.put/get()` - 变量存取
- 更多 API 详见文档

## 项目结构

```
Reader_Final/
├── Reader/                     # 主程序源码
│   ├── QuickJsEngine.hpp/cpp  # QuickJS 引擎封装
│   ├── LegadoRuleParser.h/cpp # Legado 规则解析器
│   ├── LegadoConverter.h/cpp  # Legado 格式转换器
│   ├── LegadoBookSource.hpp   # 书源数据结构
│   └── ...                    # 其他原有文件
├── opensrc/
│   ├── quickjs/               # QuickJS 引擎源码
│   └── ...                    # 其他依赖库
├── doc/
│   ├── BUILD_GUIDE.md         # 编译指南
│   └── legado_integration.md  # 集成说明
├── tools/
│   └── legado_converter.py    # Python 书源转换工具
├── Reader.sln                 # Visual Studio 解决方案
└── README_FINAL.md            # 本文件
```

## 快速开始

### 编译步骤

1. **安装 Visual Studio 2019/2022**
2. **打开 `Reader.sln`**
3. **选择配置**：`Release | x64`
4. **编译**：`Ctrl+Shift+B`
5. **运行**：`x64/Release/Reader.exe`

### 导入 Legado 书源

1. 打开 Reader
2. 进入书源管理
3. 点击"导入"
4. 选择 Legado 格式的 `.json` 文件
5. 完成！

## 技术规格

| 项目 | 规格 |
|------|------|
| QuickJS 版本 | 2024-01-13 |
| ES 标准 | ES2023 |
| 编译器 | MSVC v142+ |
| 平台 | Windows x64/x86 |

## 测试结果

QuickJS 引擎测试：**40/40 通过 (100%)**

支持的规则类型：
- ✅ XPath
- ✅ CSS 选择器
- ✅ JSONPath
- ✅ JavaScript
- ✅ 混合规则

## 许可证

- Reader 原项目：遵循原项目许可证
- QuickJS：MIT License
- 本集成代码：MIT License

## 致谢

- [binbyu/Reader](https://github.com/binbyu/Reader) - 原版 Reader 阅读器
- [gedoor/legado](https://github.com/gedoor/legado) - Legado 阅读 App
- [bellard/quickjs](https://bellard.org/quickjs/) - QuickJS JavaScript 引擎
