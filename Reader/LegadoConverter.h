#ifndef __LEGADO_CONVERTER_H__
#define __LEGADO_CONVERTER_H__

#include "types.h"

// Legado 书源转换结果
typedef struct legado_convert_result_t
{
    int success_count;      // 成功转换的书源数量
    int failed_count;       // 转换失败的书源数量
    int skipped_count;      // 跳过的书源数量（不兼容）
    char error_msg[1024];   // 错误信息
} legado_convert_result_t;

// 检测是否为 Legado 格式的书源
BOOL is_legado_format(const char* json);

// 将 Legado 格式的书源转换为 Reader 格式
// 参数:
//   json: Legado 格式的书源 JSON 字符串
//   bs: 输出的 Reader 书源数组
//   count: 输出的书源数量
//   result: 转换结果信息
// 返回:
//   TRUE: 转换成功（至少有一个书源转换成功）
//   FALSE: 转换失败
BOOL convert_legado_to_reader(const char* json, book_source_t* bs, int* count, legado_convert_result_t* result);

// 将 Legado 规则转换为 XPath 格式
// 参数:
//   legado_rule: Legado 格式的规则字符串
//   xpath_out: 输出的 XPath 字符串
//   max_len: xpath_out 的最大长度
// 返回:
//   TRUE: 转换成功
//   FALSE: 规则不兼容（如包含 JS）
BOOL convert_legado_rule_to_xpath(const char* legado_rule, char* xpath_out, int max_len);

// 从 Legado searchUrl 中提取搜索参数
// 参数:
//   search_url: Legado 的 searchUrl
//   query_url: 输出的查询 URL
//   query_method: 输出的请求方法 (0: GET, 1: POST)
//   query_params: 输出的 POST 参数
//   query_charset: 输出的字符编码 (0: auto, 1: utf8, 2: gbk)
BOOL parse_legado_search_url(const char* search_url, char* query_url, int* query_method, 
                              char* query_params, int* query_charset);

#endif
