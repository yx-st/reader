# Reader 集成 Legado 书源使用说明

## 概述

本项目为 [Reader](https://github.com/binbyu/Reader) 阅读器添加了对 [Legado](https://github.com/gedoor/legado)（阅读 App）书源格式的支持。通过书源转换器，您可以将 Legado 格式的书源文件转换为 Reader 兼容的格式。

## 快速开始

### 方法一：使用 Python 转换工具（推荐）

```bash
# 基本用法
python tools/legado_converter.py legado_sources.json reader_sources.json

# 显示详细信息
python tools/legado_converter.py -v legado_sources.json reader_sources.json

# 严格模式（跳过任何有问题的书源）
python tools/legado_converter.py --strict legado_sources.json reader_sources.json
```

### 方法二：直接在 Reader 中导入（需要编译修改版）

修改后的 Reader 可以直接识别并导入 Legado 格式的书源文件，无需手动转换。

## 支持的规则类型

| Legado 规则类型 | 示例 | 支持程度 |
|---------------|------|---------|
| XPath | `//div[@id='content']` | ✅ 完全支持 |
| @XPath: | `@XPath://div/text()` | ✅ 完全支持 |
| CSS 选择器 | `#content`, `.book-name` | ✅ 大部分支持 |
| @css: | `@css:div#content @html` | ✅ 大部分支持 |
| JSOUP Default | `class.content@tag.p@text` | ✅ 大部分支持 |
| JSONPath | `$.data.name` | ⚠️ 部分支持 |
| @json: | `@json:$.results[*].name` | ⚠️ 部分支持 |
| JavaScript | `@js:result.trim()` | ❌ 不支持 |
| `<js>` 标签 | `<js>...</js>` | ❌ 不支持 |

## 支持的书源类型

| 类型 | bookSourceType | 支持 |
|-----|---------------|------|
| 文本小说 | 0 | ✅ |
| 音频书 | 1 | ❌ |
| 图片/漫画 | 2 | ❌ |
| 文件 | 3 | ❌ |

## 转换规则对照表

### 搜索 URL

| Legado | Reader |
|--------|--------|
| `{{key}}` | `%s` |
| `{{searchKey}}` | `%s` |
| `{{java.encodeURI(key, "gbk")}}` | `%s` (charset=gbk) |
| POST 方法 | `query_method=1` |
| GET 方法 | `query_method=0` |

### 字符编码

| Legado charset | Reader query_charset |
|---------------|---------------------|
| utf-8 (默认) | 1 |
| gbk/gb2312/gb18030 | 2 |

### CSS 选择器转 XPath

| CSS | XPath |
|-----|-------|
| `#id` | `//*[@id='id']` |
| `.class` | `//*[contains(@class,'class')]` |
| `tag` | `//tag` |
| `tag.class` | `//tag[contains(@class,'class')]` |
| `tag#id` | `//tag[@id='id']` |
| `[attr=value]` | `//*[@attr='value']` |
| `parent > child` | `//parent/child` |
| `ancestor descendant` | `//ancestor//descendant` |

### 属性获取器

| Legado | XPath |
|--------|-------|
| `@text` | `/text()` |
| `@href` | `/@href` |
| `@src` | `/@src` |
| `@content` | `/@content` |
| `@html` | (保持原样) |

## 测试结果

使用多个真实 Legado 书源进行测试：

### 测试集 1：Orokapei/BookSource (11个书源)
```
转换完成:
  完全成功: 11
  部分成功: 0
  跳过: 0
  失败: 0
```

### 测试集 2：KotaHv/legadoBookSource (2个书源)
```
转换完成:
  完全成功: 1
  部分成功: 1
  跳过: 0
  失败: 0
```

## 已知限制

1. **JavaScript 规则不支持**
   - 包含 `@js:` 或 `<js>` 的规则会被跳过
   - 建议使用不含 JS 的书源

2. **复杂组合规则**
   - `&&`（与）、`||`（或）、`%%`（交替）规则只取第一个
   - 可能导致部分功能缺失

3. **发现功能不支持**
   - Legado 的 `exploreUrl` 和 `ruleExplore` 不会被转换
   - Reader 目前不支持发现功能

4. **登录功能不支持**
   - 需要登录的书源无法使用

5. **正则替换规则**
   - `##` 后的正则替换规则会被保留
   - 但 Reader 的正则引擎可能与 Legado 不完全兼容

## 获取 Legado 书源

以下是一些公开的 Legado 书源仓库：

- [Orokapei/BookSource](https://github.com/Orokapei/BookSource)
- [KotaHv/legadoBookSource](https://github.com/KotaHv/legadoBookSource)
- [shidahuilang/shuyuan](https://github.com/shidahuilang/shuyuan)

## 故障排除

### 转换后书源不工作

1. 检查原书源是否使用了 JavaScript 规则
2. 检查网站是否需要登录或有反爬措施
3. 检查 XPath 规则是否正确转换

### 搜索无结果

1. 检查 `query_url` 是否包含 `%s`
2. 检查字符编码是否正确（中文网站常用 GBK）
3. 检查 `book_name_xpath` 是否正确

### 章节列表为空

1. 检查 `chapter_title_xpath` 和 `chapter_url_xpath`
2. 某些网站需要启用 `enable_chapter_page`

### 正文内容为空

1. 检查 `content_xpath` 是否正确
2. 某些网站使用 JavaScript 动态加载内容，无法支持

## 文件结构

```
Reader/
├── Reader/
│   ├── LegadoConverter.h      # C++ 转换器头文件
│   ├── LegadoConverter.cpp    # C++ 转换器实现
│   └── Jsondata.cpp           # 修改后的导入逻辑
├── tools/
│   ├── legado_converter.py    # Python 转换工具
│   └── test/                  # 测试文件
└── doc/
    └── legado_integration.md  # 本文档
```

## 版本历史

- **v2.1** (2026-01-19)
  - 修复 CSS 选择器中带空格属性值的解析问题
  - 改进 POST 请求的处理
  - 支持 `java.encodeURI` 函数的自动转换

- **v2.0** (2026-01-19)
  - 完全重写转换逻辑
  - 支持更多 CSS 选择器语法
  - 改进错误处理和警告信息

- **v1.0** (2026-01-19)
  - 初始版本
  - 基本的 XPath 和 CSS 规则转换
