#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "apr_general.h"
#include "apr_pools.h"
#include "apr_hash.h"
#include "apr_strings.h"

// Simplified version of perform_replacements for testing
static char *perform_replacements_test(apr_pool_t *pool, const char *input, apr_hash_t *replacements)
{
    if (!input || !replacements || apr_hash_count(replacements) == 0) {
        return apr_pstrdup(pool, input);
    }
    
    char *result = apr_pstrdup(pool, input);
    apr_hash_index_t *hi;
    
    for (hi = apr_hash_first(pool, replacements); hi; hi = apr_hash_next(hi)) {
        const char *search = NULL;
        char *replace_val = NULL;
        apr_hash_this(hi, (const void **)&search, NULL, (void **)&replace_val);
        
        if (!search || !replace_val) {
            continue;
        }
        
        char *expanded_replace = replace_val;
        
        // Support both ${VAR} and %{VAR} formats
        if (strlen(replace_val) > 2 && 
            ((replace_val[0] == '$' && replace_val[1] == '{') ||
             (replace_val[0] == '%' && replace_val[1] == '{'))) {
            char *var_end = strchr(replace_val + 2, '}');
            if (var_end && var_end > replace_val + 2) {
                char *var_name = apr_pstrndup(pool, replace_val + 2, var_end - replace_val - 2);
                const char *env_val = getenv(var_name);
                if (env_val) {
                    expanded_replace = apr_pstrdup(pool, env_val);
                }
            }
        }
        
        if (!expanded_replace) {
            expanded_replace = "";
        }
        
        char *pos = strstr(result, search);
        while (pos != NULL) {
            size_t prefix_len = pos - result;
            size_t search_len = strlen(search);
            size_t replace_len = strlen(expanded_replace);
            size_t suffix_len = strlen(pos + search_len);
            
            char *temp = apr_palloc(pool, prefix_len + replace_len + suffix_len + 1);
            if (!temp) {
                break;
            }
            
            if (prefix_len > 0) {
                memcpy(temp, result, prefix_len);
            }
            if (replace_len > 0) {
                memcpy(temp + prefix_len, expanded_replace, replace_len);
            }
            if (suffix_len > 0) {
                memcpy(temp + prefix_len + replace_len, pos + search_len, suffix_len);
            }
            temp[prefix_len + replace_len + suffix_len] = '\0';
            
            result = temp;
            pos = strstr(result + prefix_len + replace_len, search);
        }
    }
    
    return result;
}

int main() {
    apr_pool_t *pool;
    apr_hash_t *replacements;
    
    apr_app_initialize(NULL, NULL, NULL);
    apr_pool_create(&pool, NULL);
    
    replacements = apr_hash_make(pool);
    apr_hash_set(replacements, "test", APR_HASH_KEY_STRING, "replacement");
    apr_hash_set(replacements, "${VAR}", APR_HASH_KEY_STRING, "value");
    apr_hash_set(replacements, "%{USER}", APR_HASH_KEY_STRING, "%{USER}"); // Will try to resolve from env
    
    // Test simple replacement
    char *result1 = perform_replacements_test(pool, "This is a test string", replacements);
    printf("Test 1: %s\n", result1);
    
    // Test variable replacement
    char *result2 = perform_replacements_test(pool, "Variable: ${VAR}", replacements);
    printf("Test 2: %s\n", result2);
    
    // Test environment variable replacement
    char *result3 = perform_replacements_test(pool, "User: %{USER}", replacements);
    printf("Test 3: %s\n", result3);
    
    // Test multiple replacements
    char *result4 = perform_replacements_test(pool, "test ${VAR} test %{USER} test", replacements);
    printf("Test 4: %s\n", result4);
    
    apr_pool_destroy(pool);
    apr_terminate();
    
    printf("All tests completed successfully!\n");
    return 0;
}