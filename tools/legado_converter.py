#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Legado 书源转换器 v2.1

将 Legado (阅读) App 的书源格式转换为 Reader 兼容格式。

使用方法:
    python legado_converter.py input.json output.json
    python legado_converter.py input.json  # 输出到 stdout

支持的 Legado 规则类型:
    - XPath 规则 (以 // 或 @XPath: 开头)
    - JSOUP Default 规则 (基本的 @ 分隔符规则)
    - CSS 规则 (以 @css: 开头 或 CSS 选择器格式)
    - JSONPath 规则 (以 $. 或 @json: 开头)

不支持的规则类型:
    - JavaScript 规则 (包含 <js> 或 @js:)
    - 复杂的组合规则 (&&, ||, %%)
"""

import json
import re
import sys
import argparse
from typing import Dict, List, Optional, Tuple, Any


class LegadoConverter:
    """Legado 书源转换器"""
    
    def __init__(self, strict_mode: bool = False):
        """
        初始化转换器
        
        Args:
            strict_mode: 严格模式，遇到不支持的规则时跳过整个书源
                        非严格模式下会尽量转换可用的部分
        """
        self.strict_mode = strict_mode
        self.success_count = 0
        self.failed_count = 0
        self.skipped_count = 0
        self.partial_count = 0  # 部分转换成功
        self.errors = []
        self.warnings = []
    
    def contains_js(self, rule: str) -> bool:
        """检查规则是否包含不支持的 JavaScript"""
        if not rule:
            return False
        
        # 明确的 JS 标记
        if '@js:' in rule or '<js>' in rule or '</js>' in rule:
            return True
        
        # 检查 {{}} 中是否包含复杂 JS (但允许简单变量)
        for match in re.finditer(r'\{\{(.+?)\}\}', rule):
            content = match.group(1).strip()
            # 允许的简单变量
            simple_vars = ['key', 'page', 'searchKey', 'searchPage', 
                          'book.name', 'book.author', 'chapter.title']
            if content not in simple_vars:
                # 检查是否是简单的 JSONPath 引用
                if not re.match(r'^\$\.[a-zA-Z_][a-zA-Z0-9_.]*$', content):
                    # 检查是否是 @css: 规则
                    if not content.startswith('@css:') and not content.startswith('@xpath:'):
                        return True
        
        return False
    
    def contains_complex_operators(self, rule: str) -> bool:
        """检查是否包含复杂的组合操作符"""
        if not rule:
            return False
        # && 和 || 是 Legado 的组合规则，%% 是交替取值
        # 但在正则替换部分 ## 后面的不算
        rule_part = rule.split('##')[0]
        return '&&' in rule_part or '%%' in rule_part
    
    def is_xpath_rule(self, rule: str) -> bool:
        """检查是否为 XPath 规则"""
        if not rule:
            return False
        rule = rule.strip()
        return rule.startswith('//') or rule.startswith('@XPath:') or rule.startswith('@xpath:')
    
    def is_jsonpath_rule(self, rule: str) -> bool:
        """检查是否为 JSONPath 规则"""
        if not rule:
            return False
        rule = rule.strip()
        return rule.startswith('$.') or rule.startswith('@json:')
    
    def is_css_rule(self, rule: str) -> bool:
        """检查是否为 CSS 规则"""
        if not rule:
            return False
        rule = rule.strip()
        # 明确的 CSS 标记
        if rule.startswith('@css:'):
            return True
        # CSS 选择器特征: #id, .class, tag, [attr]
        # 但要排除 XPath 和 JSONPath
        if self.is_xpath_rule(rule) or self.is_jsonpath_rule(rule):
            return False
        # 检查是否像 CSS 选择器
        if re.match(r'^[#.\[\w]', rule):
            return True
        return False
    
    def is_jsoup_default_rule(self, rule: str) -> bool:
        """检查是否为 JSOUP Default 规则"""
        if not rule:
            return False
        rule = rule.strip()
        # JSOUP 规则特征: type.name.index@type.name@attr
        # 例如: class.content@tag.p@text
        if self.is_xpath_rule(rule) or self.is_jsonpath_rule(rule) or self.is_css_rule(rule):
            return False
        # 包含 @ 分隔符，且有 type.name 格式
        if '@' in rule and re.search(r'(class|id|tag|text|href|src|html|all)\.', rule):
            return True
        return False
    
    def convert_jsoup_to_xpath(self, jsoup_rule: str) -> Optional[str]:
        """将 JSOUP Default 规则转换为 XPath"""
        if not jsoup_rule:
            return None
        
        # 处理正则替换部分
        rule = jsoup_rule.split('##')[0].strip()
        
        parts = rule.split('@')
        xpath_parts = []
        
        for i, part in enumerate(parts):
            if not part:
                continue
            
            part = part.strip()
            
            # 解析 type.name.index 格式
            segments = part.split('.')
            rule_type = segments[0].lower() if segments else ''
            name = segments[1] if len(segments) > 1 else ''
            index = segments[2] if len(segments) > 2 else ''
            
            if rule_type == 'class':
                if xpath_parts:
                    xpath_parts.append('//')
                xpath_parts.append(f"*[contains(@class,'{name}')]")
                if index:
                    try:
                        idx = int(index)
                        xpath_parts.append(f"[{idx + 1}]")  # XPath 索引从 1 开始
                    except ValueError:
                        pass
            elif rule_type == 'id':
                if xpath_parts:
                    xpath_parts.append('//')
                xpath_parts.append(f"*[@id='{name}']")
            elif rule_type == 'tag':
                if xpath_parts:
                    xpath_parts.append('//')
                xpath_parts.append(name if name else '*')
                if index:
                    try:
                        idx = int(index)
                        xpath_parts.append(f"[{idx + 1}]")
                    except ValueError:
                        pass
            elif rule_type == 'text':
                xpath_parts.append('/text()')
            elif rule_type == 'textnodes':
                xpath_parts.append('/text()')
            elif rule_type == 'owntext':
                xpath_parts.append('/text()')
            elif rule_type == 'href':
                xpath_parts.append('/@href')
            elif rule_type == 'src':
                xpath_parts.append('/@src')
            elif rule_type == 'content':
                xpath_parts.append('/@content')
            elif rule_type in ('html', 'all'):
                pass  # 保持原样，Reader 会处理
            elif rule_type == 'children':
                xpath_parts.append('/*')
            else:
                # 可能是简单的属性名
                if rule_type and not xpath_parts:
                    xpath_parts.append(f"//{rule_type}")
        
        if xpath_parts:
            result = '//' + ''.join(xpath_parts).lstrip('/')
            return result
        return None
    
    def convert_jsonpath_to_xpath(self, json_rule: str) -> str:
        """将 JSONPath 规则转换为 XPath (简化版)"""
        if not json_rule:
            return ''
        
        # 处理正则替换部分
        rule = json_rule.split('##')[0].strip()
        
        start = rule
        if start.startswith('@json:'):
            start = start[6:]
        if start.startswith('$.'):
            start = start[2:]
        elif start.startswith('$'):
            start = start[1:]
        
        # 移除数组索引和过滤器
        start = re.sub(r'\[\*\]', '', start)
        start = re.sub(r'\[\d+\]', '', start)
        start = re.sub(r'\[:\d+\]', '', start)
        start = re.sub(r'\[\?\(.+?\)\]', '', start)
        
        # 将 . 替换为 /
        xpath = '//' + start.replace('.', '/')
        return xpath
    
    def tokenize_css_selector(self, css: str) -> List[str]:
        """将 CSS 选择器分词，正确处理引号内的空格"""
        tokens = []
        current = ''
        in_quotes = False
        quote_char = None
        in_brackets = 0
        in_parens = 0
        
        i = 0
        while i < len(css):
            char = css[i]
            
            # 处理引号
            if char in '"\'':
                if not in_quotes:
                    in_quotes = True
                    quote_char = char
                elif char == quote_char:
                    in_quotes = False
                    quote_char = None
                current += char
            # 处理方括号
            elif char == '[' and not in_quotes:
                in_brackets += 1
                current += char
            elif char == ']' and not in_quotes:
                in_brackets -= 1
                current += char
            # 处理圆括号
            elif char == '(' and not in_quotes:
                in_parens += 1
                current += char
            elif char == ')' and not in_quotes:
                in_parens -= 1
                current += char
            # 处理空格
            elif char == ' ' and not in_quotes and in_brackets == 0 and in_parens == 0:
                if current:
                    tokens.append(current)
                    current = ''
            else:
                current += char
            
            i += 1
        
        if current:
            tokens.append(current)
        
        return tokens
    
    def convert_css_to_xpath(self, css_rule: str) -> Optional[str]:
        """将 CSS 规则转换为 XPath"""
        if not css_rule:
            return None
        
        # 处理正则替换部分
        rule = css_rule.split('##')[0].strip()
        
        # 移除 @css: 前缀
        if rule.startswith('@css:'):
            rule = rule[5:]
        
        # 分离选择器和属性获取器
        attr = ''
        # 查找最后一个不在括号内的 @
        in_brackets = 0
        in_parens = 0
        at_pos = -1
        for i, char in enumerate(rule):
            if char == '[':
                in_brackets += 1
            elif char == ']':
                in_brackets -= 1
            elif char == '(':
                in_parens += 1
            elif char == ')':
                in_parens -= 1
            elif char == '@' and in_brackets == 0 and in_parens == 0:
                at_pos = i
        
        if at_pos > 0:
            attr = rule[at_pos + 1:].strip()
            rule = rule[:at_pos].strip()
        
        # 使用正确的分词方法
        selectors = self.tokenize_css_selector(rule)
        
        xpath_parts = []
        
        for selector in selectors:
            if not selector:
                continue
            
            # 跳过 > 组合符
            if selector == '>':
                if xpath_parts:
                    # 将 // 改为 /
                    if xpath_parts[-1] == '//':
                        xpath_parts[-1] = '/'
                continue
            
            if xpath_parts:
                xpath_parts.append('//')
            
            # 解析单个选择器
            remaining = selector
            tag = '*'
            conditions = []
            
            # 首先检查是否以标签名开头
            tag_match = re.match(r'^([\w-]+)', remaining)
            if tag_match:
                tag = tag_match.group(1)
                remaining = remaining[len(tag):]
            
            # 解析剩余部分
            max_iterations = 100
            iterations = 0
            while remaining and iterations < max_iterations:
                iterations += 1
                
                if remaining.startswith('#'):
                    id_match = re.match(r'#([\w-]+)', remaining)
                    if id_match:
                        conditions.append(f"@id='{id_match.group(1)}'")
                        remaining = remaining[len(id_match.group(0)):]
                    else:
                        break
                elif remaining.startswith('.'):
                    class_match = re.match(r'\.([\w-]+)', remaining)
                    if class_match:
                        conditions.append(f"contains(@class,'{class_match.group(1)}')")
                        remaining = remaining[len(class_match.group(0)):]
                    else:
                        break
                elif remaining.startswith('['):
                    # 找到匹配的 ]
                    bracket_count = 0
                    end_pos = 0
                    for j, c in enumerate(remaining):
                        if c == '[':
                            bracket_count += 1
                        elif c == ']':
                            bracket_count -= 1
                            if bracket_count == 0:
                                end_pos = j + 1
                                break
                    
                    if end_pos > 0:
                        attr_str = remaining[1:end_pos - 1]
                        # 解析属性
                        attr_match = re.match(r'([^\]=]+)(?:=["\']?([^"\'\]]+)["\']?)?', attr_str)
                        if attr_match:
                            attr_name = attr_match.group(1).strip()
                            attr_value = attr_match.group(2)
                            if attr_value:
                                conditions.append(f"@{attr_name}='{attr_value}'")
                            else:
                                conditions.append(f"@{attr_name}")
                        remaining = remaining[end_pos:]
                    else:
                        break
                elif remaining.startswith(':'):
                    # 伪类选择器
                    pseudo_match = re.match(r':[\w-]+(?:\([^)]*\))?', remaining)
                    if pseudo_match:
                        remaining = remaining[len(pseudo_match.group(0)):]
                    else:
                        break
                else:
                    break
            
            # 构建 XPath 部分
            if conditions:
                xpath_parts.append(f"{tag}[{' and '.join(conditions)}]")
            else:
                xpath_parts.append(tag)
        
        if not xpath_parts:
            return None
        
        xpath = '//' + ''.join(xpath_parts).lstrip('/')
        
        # 添加属性获取
        if attr:
            attr = attr.lower()
            if attr == 'text':
                xpath += '/text()'
            elif attr == 'textnodes':
                xpath += '/text()'
            elif attr == 'owntext':
                xpath += '/text()'
            elif attr == 'href':
                xpath += '/@href'
            elif attr == 'src':
                xpath += '/@src'
            elif attr == 'content':
                xpath += '/@content'
            elif attr == 'html':
                pass  # 保持原样
            elif attr.startswith('data-'):
                xpath += f'/@{attr}'
            elif re.match(r'^[\w-]+$', attr):
                xpath += f'/@{attr}'
        
        return xpath
    
    def convert_rule_to_xpath(self, legado_rule: str) -> Tuple[bool, str, str]:
        """
        转换 Legado 规则为 XPath
        
        返回: (成功标志, XPath 字符串, 警告信息)
        """
        if not legado_rule:
            return True, '', ''
        
        legado_rule = legado_rule.strip()
        warning = ''
        
        # 检查是否包含 JS
        if self.contains_js(legado_rule):
            return False, '', 'contains JavaScript'
        
        # 检查是否包含复杂操作符
        if self.contains_complex_operators(legado_rule):
            # 尝试只取第一个规则
            rule_part = legado_rule.split('##')[0]
            if '&&' in rule_part:
                legado_rule = rule_part.split('&&')[0].strip()
                warning = 'using first rule from && combination'
            elif '%%' in rule_part:
                legado_rule = rule_part.split('%%')[0].strip()
                warning = 'using first rule from %% combination'
        
        # 处理 || 操作符 (取第一个有效的)
        rule_part = legado_rule.split('##')[0]
        if '||' in rule_part:
            legado_rule = rule_part.split('||')[0].strip()
            warning = warning or 'using first rule from || combination'
        
        # 处理换行分隔的多规则
        if '\n' in legado_rule.split('##')[0]:
            legado_rule = legado_rule.split('\n')[0].strip()
            warning = warning or 'using first rule from multi-line'
        
        # 如果已经是 XPath 格式
        if self.is_xpath_rule(legado_rule):
            xpath_start = legado_rule
            if xpath_start.startswith('@XPath:') or xpath_start.startswith('@xpath:'):
                xpath_start = xpath_start[7:]
            # 移除正则替换部分
            xpath_start = xpath_start.split('##')[0].strip()
            return True, xpath_start, warning
        
        # JSONPath 格式
        if self.is_jsonpath_rule(legado_rule):
            xpath = self.convert_jsonpath_to_xpath(legado_rule)
            return True, xpath, warning
        
        # JSOUP Default 格式
        if self.is_jsoup_default_rule(legado_rule):
            result = self.convert_jsoup_to_xpath(legado_rule)
            if result:
                return True, result, warning
            return False, '', 'failed to convert JSOUP rule'
        
        # CSS 格式
        if self.is_css_rule(legado_rule):
            result = self.convert_css_to_xpath(legado_rule)
            if result:
                return True, result, warning
            return False, '', 'failed to convert CSS rule'
        
        # 简单文本，可能是属性名或简单选择器
        simple_rule = legado_rule.split('##')[0].strip()
        if simple_rule:
            # 检查是否是简单的属性名 (如 text, href)
            if simple_rule in ('text', 'textNodes', 'ownText'):
                return True, '/text()', warning
            elif simple_rule == 'href':
                return True, '/@href', warning
            elif simple_rule == 'src':
                return True, '/@src', warning
            elif simple_rule == 'html' or simple_rule == 'all':
                return True, '', warning
            # 尝试作为 CSS 选择器处理
            result = self.convert_css_to_xpath(simple_rule)
            if result:
                return True, result, warning
        
        return False, '', 'unknown rule format'
    
    def parse_search_url(self, search_url: str, book_source_url: str = '') -> Dict[str, Any]:
        """解析 Legado searchUrl"""
        result = {
            'query_url': '',
            'query_method': 0,  # 0: GET, 1: POST
            'query_params': '',
            'query_charset': 1,  # 0: auto, 1: utf8, 2: gbk
        }
        
        if not search_url:
            return result
        
        # 处理 java.encodeURI 等函数调用，提取编码信息
        gbk_match = re.search(r'java\.encodeURI\s*\(\s*key\s*,\s*["\']?(gbk|gb2312|gb18030)["\']?\s*\)', search_url, re.IGNORECASE)
        if gbk_match:
            result['query_charset'] = 2  # GBK
            # 将 java.encodeURI(key, "gbk") 替换为 {{key}}
            search_url = re.sub(r'\{\{java\.encodeURI\s*\([^)]+\)\}\}', '{{key}}', search_url)
        
        # 处理相对 URL
        if search_url.startswith('/'):
            search_url = book_source_url.rstrip('/') + search_url
        
        # 检查是否包含 JSON 配置
        json_match = re.search(r',\s*(\{.+\})\s*$', search_url)
        if json_match:
            url_part = search_url[:json_match.start()]
            json_part = json_match.group(1)
            result['query_url'] = url_part.strip()
            
            try:
                config = json.loads(json_part)
                if config.get('method', '').upper() == 'POST':
                    result['query_method'] = 1
                charset = config.get('charset', '').lower()
                if charset in ('gbk', 'gb2312', 'gb18030'):
                    result['query_charset'] = 2
                if 'body' in config:
                    result['query_params'] = str(config['body'])
            except json.JSONDecodeError:
                pass
        else:
            result['query_url'] = search_url
        
        # 同样处理 query_params
        if result['query_params']:
            result['query_params'] = re.sub(r'\{\{key\}\}', '%s', result['query_params'], flags=re.IGNORECASE)
            result['query_params'] = re.sub(r'\{\{searchKey\}\}', '%s', result['query_params'], flags=re.IGNORECASE)
            result['query_params'] = re.sub(r'\{\{java\.[^}]+\}\}', '%s', result['query_params'])
        
        # 将 {{key}} 替换为 %s
        result['query_url'] = re.sub(r'\{\{key\}\}', '%s', result['query_url'], flags=re.IGNORECASE)
        result['query_url'] = re.sub(r'\{\{searchKey\}\}', '%s', result['query_url'], flags=re.IGNORECASE)
        # 处理残留的 java 函数调用
        result['query_url'] = re.sub(r'\{\{java\.[^}]+\}\}', '%s', result['query_url'])
        
        # 移除 {{page}} 相关的部分（Reader 不支持分页搜索）
        result['query_url'] = re.sub(r'[&?]page=\{\{page\}\}', '', result['query_url'])
        result['query_url'] = re.sub(r'\{\{page\}\}', '1', result['query_url'])
        
        # 对于 POST 请求，%s 应该在 query_params 中
        # 对于 GET 请求，%s 应该在 query_url 中
        # 检查是否有效
        has_placeholder_in_url = '%s' in result['query_url']
        has_placeholder_in_params = '%s' in result['query_params']
        
        # 如果是 POST 且 params 中有 %s，则有效
        # 如果是 GET 且 url 中有 %s，则有效
        if result['query_method'] == 1:  # POST
            if not has_placeholder_in_params and not has_placeholder_in_url:
                result['query_url'] = ''  # 标记为无效
        else:  # GET
            if not has_placeholder_in_url:
                result['query_url'] = ''  # 标记为无效
        
        return result
    
    def convert_single_source(self, source: Dict) -> Tuple[Optional[Dict], List[str]]:
        """
        转换单个 Legado 书源
        
        返回: (转换后的书源或None, 警告列表)
        """
        warnings = []
        
        # 检查必要字段
        if not source.get('bookSourceName') or not source.get('bookSourceUrl'):
            return None, ['missing required fields']
        
        # 检查书源类型，只支持文本类型 (type=0)
        source_type = source.get('bookSourceType', 0)
        if isinstance(source_type, str):
            source_type = int(source_type) if source_type.isdigit() else 0
        if source_type != 0:
            return None, ['unsupported source type (not text)']
        
        # 检查是否启用
        if source.get('enabled') is False:
            return None, ['source is disabled']
        
        book_source_url = source.get('bookSourceUrl', '')
        
        reader_source = {
            'title': source.get('bookSourceName', ''),
            'host': book_source_url,
            'query_url': '',
            'query_method': 0,
            'query_params': '',
            'query_charset': 1,
            'book_name_xpath': '',
            'book_mainpage_xpath': '',
            'book_author_xpath': '',
            'enable_chapter_page': 0,
            'chapter_page_xpath': '',
            'chapter_title_xpath': '',
            'chapter_url_xpath': '',
            'enable_chapter_next': 0,
            'chapter_next_url_xpath': '',
            'chapter_next_keyword_xpath': '',
            'chapter_next_keyword': '',
            'content_xpath': '',
            'enable_content_next': 0,
            'content_next_url_xpath': '',
            'content_next_keyword_xpath': '',
            'content_next_keyword': '',
            'content_filter_type': 0,
            'content_filter_keyword': '',
        }
        
        # 解析搜索 URL
        search_url = source.get('searchUrl', '')
        if search_url:
            url_info = self.parse_search_url(search_url, book_source_url)
            reader_source.update(url_info)
        else:
            warnings.append('no searchUrl')
        
        # 标记是否有关键规则转换失败
        critical_failure = False
        
        # 获取搜索规则
        rule_search = source.get('ruleSearch', {})
        if rule_search:
            # 书名规则 (关键)
            name_rule = rule_search.get('name', '')
            if name_rule:
                success, xpath, warn = self.convert_rule_to_xpath(name_rule)
                if success and xpath:
                    reader_source['book_name_xpath'] = xpath
                    if warn:
                        warnings.append(f'name rule: {warn}')
                elif self.strict_mode:
                    critical_failure = True
                    warnings.append(f'name rule conversion failed')
            
            # 书籍URL规则 (关键)
            book_url_rule = rule_search.get('bookUrl', '')
            if book_url_rule:
                success, xpath, warn = self.convert_rule_to_xpath(book_url_rule)
                if success and xpath:
                    reader_source['book_mainpage_xpath'] = xpath
                    if warn:
                        warnings.append(f'bookUrl rule: {warn}')
                elif self.strict_mode:
                    critical_failure = True
                    warnings.append(f'bookUrl rule conversion failed')
            
            # 作者规则 (非关键)
            author_rule = rule_search.get('author', '')
            if author_rule:
                success, xpath, warn = self.convert_rule_to_xpath(author_rule)
                if success and xpath:
                    reader_source['book_author_xpath'] = xpath
                    if warn:
                        warnings.append(f'author rule: {warn}')
        
        # 获取目录规则
        rule_toc = source.get('ruleToc', {})
        if rule_toc:
            # 章节列表规则
            chapter_list = rule_toc.get('chapterList', '')
            
            # 章节标题规则
            chapter_name = rule_toc.get('chapterName', '')
            if chapter_name:
                success, xpath, warn = self.convert_rule_to_xpath(chapter_name)
                if success and xpath:
                    reader_source['chapter_title_xpath'] = xpath
                    if warn:
                        warnings.append(f'chapterName rule: {warn}')
                elif chapter_list:
                    # 尝试使用 chapterList
                    success, xpath, warn = self.convert_rule_to_xpath(chapter_list)
                    if success and xpath:
                        reader_source['chapter_title_xpath'] = xpath + '/text()'
            elif chapter_list:
                success, xpath, warn = self.convert_rule_to_xpath(chapter_list)
                if success and xpath:
                    reader_source['chapter_title_xpath'] = xpath + '/text()'
            
            # 章节URL规则
            chapter_url = rule_toc.get('chapterUrl', '')
            if chapter_url:
                success, xpath, warn = self.convert_rule_to_xpath(chapter_url)
                if success and xpath:
                    reader_source['chapter_url_xpath'] = xpath
                    if warn:
                        warnings.append(f'chapterUrl rule: {warn}')
                elif chapter_list:
                    success, xpath, warn = self.convert_rule_to_xpath(chapter_list)
                    if success and xpath:
                        reader_source['chapter_url_xpath'] = xpath + '/@href'
            elif chapter_list:
                success, xpath, warn = self.convert_rule_to_xpath(chapter_list)
                if success and xpath:
                    reader_source['chapter_url_xpath'] = xpath + '/@href'
            
            # 目录下一页
            next_toc_url = rule_toc.get('nextTocUrl', '')
            if next_toc_url:
                success, xpath, warn = self.convert_rule_to_xpath(next_toc_url)
                if success and xpath:
                    reader_source['enable_chapter_next'] = 1
                    reader_source['chapter_next_url_xpath'] = xpath
        
        # 获取正文规则
        rule_content = source.get('ruleContent', {})
        if rule_content:
            # 正文规则 (关键)
            content_rule = rule_content.get('content', '')
            if content_rule:
                success, xpath, warn = self.convert_rule_to_xpath(content_rule)
                if success and xpath:
                    reader_source['content_xpath'] = xpath
                    if warn:
                        warnings.append(f'content rule: {warn}')
                elif self.strict_mode:
                    critical_failure = True
                    warnings.append(f'content rule conversion failed')
            
            # 正文下一页
            next_content_url = rule_content.get('nextContentUrl', '')
            if next_content_url:
                success, xpath, warn = self.convert_rule_to_xpath(next_content_url)
                if success and xpath:
                    reader_source['enable_content_next'] = 1
                    reader_source['content_next_url_xpath'] = xpath
            
            # 内容过滤
            replace_regex = rule_content.get('replaceRegex', '')
            if replace_regex:
                reader_source['content_filter_type'] = 2  # 正则表达式
                reader_source['content_filter_keyword'] = replace_regex
        
        # 获取详情页规则
        rule_book_info = source.get('ruleBookInfo', {})
        if rule_book_info:
            toc_url = rule_book_info.get('tocUrl', '')
            if toc_url:
                success, xpath, warn = self.convert_rule_to_xpath(toc_url)
                if success and xpath:
                    reader_source['enable_chapter_page'] = 1
                    reader_source['chapter_page_xpath'] = xpath
        
        # 检查是否有足够的规则
        if critical_failure:
            return None, warnings
        
        # 检查是否有基本的可用规则
        has_search = bool(reader_source['query_url'])
        has_placeholder = '%s' in reader_source['query_url'] or '%s' in reader_source['query_params']
        has_name = bool(reader_source['book_name_xpath'])
        has_content = bool(reader_source['content_xpath'])
        
        if not has_search or not has_placeholder:
            warnings.append('no valid search URL')
        if not has_name:
            warnings.append('no valid name rule')
        if not has_content:
            warnings.append('no valid content rule')
        
        # 至少需要搜索URL和书名规则
        if not has_search or not has_placeholder or not has_name:
            return None, warnings
        
        return reader_source, warnings
    
    def convert(self, legado_json: str) -> str:
        """
        转换 Legado 书源 JSON 为 Reader 格式
        
        参数:
            legado_json: Legado 格式的 JSON 字符串
        
        返回:
            Reader 格式的 JSON 字符串
        """
        self.success_count = 0
        self.failed_count = 0
        self.skipped_count = 0
        self.partial_count = 0
        self.errors = []
        self.warnings = []
        
        try:
            data = json.loads(legado_json)
        except json.JSONDecodeError as e:
            self.errors.append(f"JSON 解析错误: {e}")
            return ''
        
        # 处理数组格式
        if isinstance(data, list):
            sources = data
        # 处理单个对象格式
        elif isinstance(data, dict):
            sources = [data]
        else:
            self.errors.append("无效的 JSON 格式")
            return ''
        
        reader_sources = []
        for source in sources:
            source_name = source.get('bookSourceName', 'Unknown')
            try:
                result, warnings = self.convert_single_source(source)
                if result:
                    reader_sources.append(result)
                    if warnings:
                        self.partial_count += 1
                        self.warnings.append(f"'{source_name}': {', '.join(warnings)}")
                    else:
                        self.success_count += 1
                else:
                    self.skipped_count += 1
                    if warnings:
                        self.warnings.append(f"'{source_name}' skipped: {', '.join(warnings)}")
            except Exception as e:
                self.failed_count += 1
                self.errors.append(f"转换 '{source_name}' 失败: {e}")
        
        if not reader_sources:
            return ''
        
        return json.dumps({'book_sources': reader_sources}, ensure_ascii=False, indent='\t')
    
    def get_summary(self) -> str:
        """获取转换摘要"""
        lines = [
            f"转换完成:",
            f"  完全成功: {self.success_count}",
            f"  部分成功: {self.partial_count}",
            f"  跳过: {self.skipped_count}",
            f"  失败: {self.failed_count}",
        ]
        if self.warnings:
            lines.append("警告信息:")
            for warning in self.warnings[:20]:
                lines.append(f"  - {warning}")
            if len(self.warnings) > 20:
                lines.append(f"  ... 还有 {len(self.warnings) - 20} 条警告")
        if self.errors:
            lines.append("错误信息:")
            for error in self.errors[:10]:
                lines.append(f"  - {error}")
            if len(self.errors) > 10:
                lines.append(f"  ... 还有 {len(self.errors) - 10} 个错误")
        return '\n'.join(lines)


def is_legado_format(json_str: str) -> bool:
    """检测是否为 Legado 格式"""
    try:
        data = json.loads(json_str)
    except json.JSONDecodeError:
        return False
    
    # Legado 格式是数组，每个元素有 bookSourceUrl 字段
    if isinstance(data, list) and len(data) > 0:
        first = data[0]
        if isinstance(first, dict) and 'bookSourceUrl' in first and 'bookSourceName' in first:
            return True
    # 也可能是单个书源对象
    elif isinstance(data, dict):
        if 'bookSourceUrl' in data and 'bookSourceName' in data:
            return True
    
    return False


def main():
    parser = argparse.ArgumentParser(
        description='将 Legado 书源转换为 Reader 格式 (v2.1)',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
示例:
    %(prog)s input.json output.json
    %(prog)s input.json > output.json
    cat input.json | %(prog)s - output.json
    %(prog)s -v --strict input.json output.json
        '''
    )
    parser.add_argument('input', help='输入文件路径 (Legado 格式), 使用 - 表示 stdin')
    parser.add_argument('output', nargs='?', help='输出文件路径 (Reader 格式), 不指定则输出到 stdout')
    parser.add_argument('-v', '--verbose', action='store_true', help='显示详细信息')
    parser.add_argument('--strict', action='store_true', help='严格模式，跳过任何有问题的书源')
    
    args = parser.parse_args()
    
    # 读取输入
    if args.input == '-':
        input_json = sys.stdin.read()
    else:
        try:
            with open(args.input, 'r', encoding='utf-8') as f:
                input_json = f.read()
        except FileNotFoundError:
            print(f"错误: 找不到文件 '{args.input}'", file=sys.stderr)
            sys.exit(1)
        except Exception as e:
            print(f"错误: 读取文件失败 - {e}", file=sys.stderr)
            sys.exit(1)
    
    # 检测格式
    if not is_legado_format(input_json):
        print("警告: 输入可能不是 Legado 格式", file=sys.stderr)
    
    # 转换
    converter = LegadoConverter(strict_mode=args.strict)
    output_json = converter.convert(input_json)
    
    if args.verbose:
        print(converter.get_summary(), file=sys.stderr)
    
    if not output_json:
        print("错误: 转换失败，没有可用的书源", file=sys.stderr)
        sys.exit(1)
    
    # 输出
    if args.output:
        try:
            with open(args.output, 'w', encoding='utf-8') as f:
                f.write(output_json)
            if args.verbose:
                print(f"已保存到: {args.output}", file=sys.stderr)
        except Exception as e:
            print(f"错误: 写入文件失败 - {e}", file=sys.stderr)
            sys.exit(1)
    else:
        print(output_json)


if __name__ == '__main__':
    main()
