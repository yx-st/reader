/*
 * test_quickjs.c - Test QuickJS integration for Reader
 * 
 * Compile: gcc -o test_quickjs test_quickjs.c ../opensrc/quickjs/quickjs.c \
 *          ../opensrc/quickjs/cutils.c ../opensrc/quickjs/libregexp.c \
 *          ../opensrc/quickjs/libunicode.c ../opensrc/quickjs/dtoa.c \
 *          -I../opensrc/quickjs -lm -lpthread -DCONFIG_VERSION=\"test\"
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "quickjs.h"

// Global context for callbacks
static JSContext *g_ctx = NULL;

// java.log implementation
static JSValue js_log(JSContext *ctx, JSValueConst this_val,
                      int argc, JSValueConst *argv)
{
    if (argc >= 1) {
        const char *str = JS_ToCString(ctx, argv[0]);
        if (str) {
            printf("[JS LOG] %s\n", str);
            JS_FreeCString(ctx, str);
        }
    }
    return JS_UNDEFINED;
}

// java.md5Encode implementation (simplified - just returns input for testing)
static JSValue js_md5Encode(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv)
{
    if (argc < 1) return JS_NewString(ctx, "");
    
    const char *str = JS_ToCString(ctx, argv[0]);
    if (!str) return JS_NewString(ctx, "");
    
    // Simplified: just return a fixed hash for testing
    // Real implementation would compute actual MD5
    char result[33];
    snprintf(result, sizeof(result), "md5_%s_hash", str);
    JS_FreeCString(ctx, str);
    return JS_NewString(ctx, result);
}

// java.base64Encode implementation
static const char base64_chars[] = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static JSValue js_base64Encode(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv)
{
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
        uint32_t octet_a = i < in_len ? (unsigned char)input[i++] : 0;
        uint32_t octet_b = i < in_len ? (unsigned char)input[i++] : 0;
        uint32_t octet_c = i < in_len ? (unsigned char)input[i++] : 0;
        uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;
        
        output[j++] = base64_chars[(triple >> 18) & 0x3F];
        output[j++] = base64_chars[(triple >> 12) & 0x3F];
        output[j++] = base64_chars[(triple >> 6) & 0x3F];
        output[j++] = base64_chars[triple & 0x3F];
    }
    
    // Add padding
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

// java.encodeURI implementation
static JSValue js_encodeURI(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv)
{
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

// Register java object with all methods
static void register_java_object(JSContext *ctx)
{
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue java_obj = JS_NewObject(ctx);
    
    JS_SetPropertyStr(ctx, java_obj, "log",
        JS_NewCFunction(ctx, js_log, "log", 1));
    JS_SetPropertyStr(ctx, java_obj, "md5Encode",
        JS_NewCFunction(ctx, js_md5Encode, "md5Encode", 1));
    JS_SetPropertyStr(ctx, java_obj, "base64Encode",
        JS_NewCFunction(ctx, js_base64Encode, "base64Encode", 1));
    JS_SetPropertyStr(ctx, java_obj, "encodeURI",
        JS_NewCFunction(ctx, js_encodeURI, "encodeURI", 2));
    
    JS_SetPropertyStr(ctx, global, "java", java_obj);
    
    // Also add console.log
    JSValue console_obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, console_obj, "log",
        JS_NewCFunction(ctx, js_log, "log", 1));
    JS_SetPropertyStr(ctx, global, "console", console_obj);
    
    JS_FreeValue(ctx, global);
}

// Evaluate JS code and return result as string
static char* evaluate_js(JSContext *ctx, const char *code, const char *result_var)
{
    // Set result variable if provided
    if (result_var && strlen(result_var) > 0) {
        JSValue global = JS_GetGlobalObject(ctx);
        JS_SetPropertyStr(ctx, global, "result", JS_NewString(ctx, result_var));
        JS_FreeValue(ctx, global);
    }
    
    JSValue val = JS_Eval(ctx, code, strlen(code), "<eval>", JS_EVAL_TYPE_GLOBAL);
    
    if (JS_IsException(val)) {
        JSValue exception = JS_GetException(ctx);
        const char *err = JS_ToCString(ctx, exception);
        printf("JS Error: %s\n", err ? err : "unknown");
        if (err) JS_FreeCString(ctx, err);
        JS_FreeValue(ctx, exception);
        JS_FreeValue(ctx, val);
        return strdup("");
    }
    
    const char *str = JS_ToCString(ctx, val);
    char *result = str ? strdup(str) : strdup("");
    if (str) JS_FreeCString(ctx, str);
    JS_FreeValue(ctx, val);
    return result;
}

int main(int argc, char *argv[])
{
    printf("=== QuickJS Integration Test ===\n\n");
    
    // Create runtime and context
    JSRuntime *rt = JS_NewRuntime();
    if (!rt) {
        printf("Failed to create JS runtime\n");
        return 1;
    }
    
    JSContext *ctx = JS_NewContext(rt);
    if (!ctx) {
        printf("Failed to create JS context\n");
        JS_FreeRuntime(rt);
        return 1;
    }
    
    g_ctx = ctx;
    
    // Register java object
    register_java_object(ctx);
    
    // Test 1: Simple expression
    printf("Test 1: Simple expression\n");
    char *result = evaluate_js(ctx, "1 + 2", NULL);
    printf("  1 + 2 = %s\n\n", result);
    free(result);
    
    // Test 2: String manipulation with result variable
    printf("Test 2: String manipulation\n");
    result = evaluate_js(ctx, "result.trim().toUpperCase()", "  hello world  ");
    printf("  '  hello world  '.trim().toUpperCase() = '%s'\n\n", result);
    free(result);
    
    // Test 3: java.log
    printf("Test 3: java.log\n");
    result = evaluate_js(ctx, "java.log('Hello from JS!'); 'logged'", NULL);
    printf("  Result: %s\n\n", result);
    free(result);
    
    // Test 4: java.base64Encode
    printf("Test 4: java.base64Encode\n");
    result = evaluate_js(ctx, "java.base64Encode('Hello World')", NULL);
    printf("  base64('Hello World') = %s\n\n", result);
    free(result);
    
    // Test 5: java.encodeURI
    printf("Test 5: java.encodeURI\n");
    result = evaluate_js(ctx, "java.encodeURI('你好世界')", NULL);
    printf("  encodeURI('你好世界') = %s\n\n", result);
    free(result);
    
    // Test 6: Complex expression (simulating Legado rule)
    printf("Test 6: Complex Legado-style rule\n");
    const char *complex_code = 
        "var parts = result.split('|');\n"
        "var name = parts[0].trim();\n"
        "var author = parts.length > 1 ? parts[1].trim() : '';\n"
        "name + ' by ' + author;";
    result = evaluate_js(ctx, complex_code, "  斗破苍穹 | 天蚕土豆  ");
    printf("  Parsed: %s\n\n", result);
    free(result);
    
    // Test 7: Array operations
    printf("Test 7: Array operations\n");
    result = evaluate_js(ctx, 
        "var arr = result.split(',');\n"
        "arr.map(function(x) { return x.trim(); }).join('|');",
        "a, b, c, d");
    printf("  Split and join: %s\n\n", result);
    free(result);
    
    // Test 8: JSON parsing
    printf("Test 8: JSON parsing\n");
    result = evaluate_js(ctx,
        "var obj = JSON.parse(result);\n"
        "obj.name + ' - ' + obj.author;",
        "{\"name\":\"三体\",\"author\":\"刘慈欣\"}");
    printf("  JSON parsed: %s\n\n", result);
    free(result);
    
    // Cleanup
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    
    printf("=== All tests completed ===\n");
    return 0;
}
