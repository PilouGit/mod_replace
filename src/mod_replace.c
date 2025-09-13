/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * mod_replace.c - High-Performance Apache Module for Text Replacement
 * 
 * This module provides real-time text replacement in web content using
 * the Aho-Corasick string matching algorithm with zero-copy optimizations
 * and variable expansion support.
 */

#ifndef TEST_BUILD
#include "httpd.h"
#include "http_config.h"
#include "http_request.h"
#include "http_protocol.h"
#include "http_log.h"
#include "ap_config.h"
#include "util_filter.h"
#endif

#include "apr_strings.h"
#include "apr_buckets.h"
#include "apr_hash.h"
#include "apr_time.h"
#include "../inc/aho_corasick.h"

#ifndef TEST_BUILD
module AP_MODULE_DECLARE_DATA replace_module;
#endif

typedef struct {
    apr_hash_t *replacements;
    ac_automaton_t *automaton;
    int enabled;
    int automaton_compiled;
    apr_pool_t *pool;  // Pool for automaton cleanup
} replace_config;

typedef struct {
    apr_bucket_brigade *bb;
    apr_pool_t *pool;
} replace_ctx;

static apr_status_t cleanup_automaton(void *data)
{
    ac_automaton_t *automaton = (ac_automaton_t *)data;
    if (automaton) {
        ac_destroy(automaton);
    }
    return APR_SUCCESS;
}

static void *create_replace_config(apr_pool_t *pool, char *path)
{
    replace_config *cfg = apr_pcalloc(pool, sizeof(replace_config));
    cfg->replacements = apr_hash_make(pool);
    cfg->automaton = ac_create(0);
    cfg->enabled = 0;
    cfg->automaton_compiled = 0;
    cfg->pool = pool;
    
    // Register cleanup for automaton
    if (cfg->automaton) {
        apr_pool_cleanup_register(pool, cfg->automaton, cleanup_automaton, apr_pool_cleanup_null);
    }
    
    return cfg;
}

static void *merge_replace_config(apr_pool_t *pool, void *parent_conf, void *new_conf)
{
    replace_config *parent = (replace_config *)parent_conf;
    replace_config *new = (replace_config *)new_conf;
    replace_config *merged = apr_pcalloc(pool, sizeof(replace_config));
    
    merged->replacements = apr_hash_overlay(pool, new->replacements, parent->replacements);
    merged->enabled = new->enabled ? new->enabled : parent->enabled;
    merged->automaton = ac_create(0);
    merged->automaton_compiled = 0;
    merged->pool = pool;
    
    // Register cleanup for merged automaton
    if (merged->automaton) {
        apr_pool_cleanup_register(pool, merged->automaton, cleanup_automaton, apr_pool_cleanup_null);
        
        // Add all patterns from merged replacements to automaton
        apr_hash_index_t *hi;
        for (hi = apr_hash_first(pool, merged->replacements); hi; hi = apr_hash_next(hi)) {
            const char *search = NULL;
            char *replace_val = NULL;
            apr_hash_this(hi, (const void **)&search, NULL, (void **)&replace_val);
            
            if (search && replace_val) {
                ac_add_pattern(merged->automaton, search, strlen(search), replace_val, strlen(replace_val));
            }
        }
    }
    
    return merged;
}

static const char *set_replace_rule(cmd_parms *cmd, void *cfg, const char *search, const char *replace)
{
    replace_config *config = (replace_config *)cfg;
    
    if (!search || !replace) {
        return "ReplaceRule requires both search and replace parameters";
    }
    
    // Add to hash table
    apr_hash_set(config->replacements, apr_pstrdup(cmd->pool, search), 
                 APR_HASH_KEY_STRING, apr_pstrdup(cmd->pool, replace));
    
    // Add to automaton if it exists
    if (config->automaton && !config->automaton_compiled) {
        if (!ac_add_pattern(config->automaton, search, strlen(search), replace, strlen(replace))) {
            return "Failed to add pattern to Aho-Corasick automaton";
        }
    }
    
    return NULL;
}

static const char *set_replace_enable(cmd_parms *cmd, void *cfg, int flag)
{
    replace_config *config = (replace_config *)cfg;
    config->enabled = flag;
    return NULL;
}

static void ensure_automaton_compiled(replace_config *config)
{
    if (config->automaton && !config->automaton_compiled && apr_hash_count(config->replacements) > 0) {
#ifndef TEST_BUILD
        apr_time_t compile_start = apr_time_now();
#endif
        if (ac_compile(config->automaton)) {
            config->automaton_compiled = 1;
#ifndef TEST_BUILD
            apr_time_t compile_end = apr_time_now();
            size_t node_count = 0, pattern_count = 0, memory_usage = 0;
            ac_get_stats(config->automaton, &node_count, &pattern_count, &memory_usage);
            
            // We can't use ap_log_rerror here as we don't have request_rec
            // This will go to the main Apache error log
            ap_log_perror(APLOG_MARK, APLOG_DEBUG, 0, config->pool,
                         "mod_replace: Compiled precompiled automaton - compile_time=%d μs, "
                         "patterns=%zu, nodes=%zu, memory=%zu bytes",
                         (int)(compile_end - compile_start), pattern_count, node_count, memory_usage);
#endif
        }
    }
}

static char *expand_replacement_value(apr_pool_t *pool, const char *replace_val, request_rec *r)
{
    if (!replace_val) return apr_pstrdup(pool, "");
    
    // Support both ${VAR} and %{VAR} formats
    if (strlen(replace_val) > 2 && 
        ((replace_val[0] == '$' && replace_val[1] == '{') ||
         (replace_val[0] == '%' && replace_val[1] == '{'))) {
        char *var_end = strchr(replace_val + 2, '}');
        if (var_end && var_end > replace_val + 2) {
            char *var_name = apr_pstrndup(pool, replace_val + 2, var_end - replace_val - 2);
            const char *env_val = NULL;
            
            if (r && r->subprocess_env) {
                env_val = apr_table_get(r->subprocess_env, var_name);
            }
            if (!env_val) {
                env_val = getenv(var_name);
            }
            if (env_val) {
                return apr_pstrdup(pool, env_val);
            }
        }
    }
    
    return apr_pstrdup(pool, replace_val);
}

static char *perform_replacements(apr_pool_t *pool, const char *input, apr_hash_t *replacements, request_rec *r)
{
    if (!input || !replacements || apr_hash_count(replacements) == 0) {
        return apr_pstrdup(pool, input);
    }
    
    apr_time_t start_time = apr_time_now();
    size_t input_len = strlen(input);
    size_t pattern_count = apr_hash_count(replacements);
    
    // Get config from request to access precompiled automaton
    replace_config *cfg = NULL;
    if (r) {
        cfg = ap_get_module_config(r->per_dir_config, &replace_module);
    }
    
#ifndef TEST_BUILD
    if (r) {
        ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, 
                      "mod_replace: Starting replacements - input_len=%zu, pattern_count=%zu",
                      input_len, pattern_count);
    }
#endif
    
    // Use precompiled automaton if available and variables don't need expansion
    if (cfg && cfg->automaton && cfg->automaton_compiled) {
        // Check if any replacement values contain variables
        int has_variables = 0;
        apr_hash_index_t *hi;
        for (hi = apr_hash_first(pool, replacements); hi; hi = apr_hash_next(hi)) {
            const char *search = NULL;
            char *replace_val = NULL;
            apr_hash_this(hi, (const void **)&search, NULL, (void **)&replace_val);
            
            if (replace_val && strlen(replace_val) > 2 && 
                ((replace_val[0] == '$' && replace_val[1] == '{') ||
                 (replace_val[0] == '%' && replace_val[1] == '{'))) {
                has_variables = 1;
                break;
            }
        }
        
        // Use precompiled automaton if no variables need expansion
        if (!has_variables) {
#ifndef TEST_BUILD
            if (r) {
                ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, 
                              "mod_replace: Using precompiled automaton (fast path)");
            }
#endif
            apr_time_t ac_start = apr_time_now();
            size_t result_len;
            char *result = ac_replace_alloc(cfg->automaton, input, strlen(input), &result_len);
            apr_time_t ac_end = apr_time_now();
            
            if (!result) {
#ifndef TEST_BUILD
                if (r) {
                    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, 
                                  "mod_replace: ac_replace_alloc failed");
                }
#endif
                return apr_pstrdup(pool, input);
            }
            
            // Copy result to APR pool memory
            char *pool_result = apr_pstrndup(pool, result, result_len);
            free(result);
            
            apr_time_t end_time = apr_time_now();
#ifndef TEST_BUILD
            if (r) {
                ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, 
                              "mod_replace: Fast path completed - ac_time=%d μs, total_time=%d μs, "
                              "input_len=%zu, output_len=%zu, patterns=%zu",
                              (int)(ac_end - ac_start), (int)(end_time - start_time),
                              input_len, result_len, pattern_count);
            }
#endif
            
            return pool_result ? pool_result : apr_pstrdup(pool, input);
        }
#ifndef TEST_BUILD
        else if (r) {
            ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, 
                          "mod_replace: Variables detected, using fallback path");
        }
#endif
    }
    
    // Fallback: create temporary automaton with expanded variables
#ifndef TEST_BUILD
    if (r) {
        ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, 
                      "mod_replace: Creating temporary automaton (slow path)");
    }
#endif
    
    apr_time_t create_start = apr_time_now();
    ac_automaton_t *ac = ac_create(0);
    if (!ac) {
#ifndef TEST_BUILD
        if (r) {
            ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, 
                          "mod_replace: Failed to create temporary automaton");
        }
#endif
        return apr_pstrdup(pool, input);
    }
    
    // Add all patterns to the automaton with expanded replacements
    apr_hash_index_t *hi;
    int patterns_added = 0;
    for (hi = apr_hash_first(pool, replacements); hi; hi = apr_hash_next(hi)) {
        const char *search = NULL;
        char *replace_val = NULL;
        apr_hash_this(hi, (const void **)&search, NULL, (void **)&replace_val);
        
        if (!search || !replace_val) {
            continue;
        }
        
        // Expand variable replacements
        char *expanded_replace = expand_replacement_value(pool, replace_val, r);
        
        // Add pattern to automaton
        if (!ac_add_pattern(ac, search, strlen(search), expanded_replace, strlen(expanded_replace))) {
            ac_destroy(ac);
#ifndef TEST_BUILD
            if (r) {
                ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, 
                              "mod_replace: Failed to add pattern to temporary automaton");
            }
#endif
            return apr_pstrdup(pool, input);
        }
        patterns_added++;
    }
    
    // Compile the automaton
    apr_time_t compile_start = apr_time_now();
    if (!ac_compile(ac)) {
        ac_destroy(ac);
#ifndef TEST_BUILD
        if (r) {
            ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, 
                          "mod_replace: Failed to compile temporary automaton");
        }
#endif
        return apr_pstrdup(pool, input);
    }
    apr_time_t compile_end = apr_time_now();
    
    // Perform replacements using Aho-Corasick
    apr_time_t replace_start = apr_time_now();
    size_t result_len;
    char *result = ac_replace_alloc(ac, input, strlen(input), &result_len);
    apr_time_t replace_end = apr_time_now();
    
    ac_destroy(ac);
    
    if (!result) {
#ifndef TEST_BUILD
        if (r) {
            ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, 
                          "mod_replace: Temporary automaton replacement failed");
        }
#endif
        return apr_pstrdup(pool, input);
    }
    
    // Copy result to APR pool memory
    char *pool_result = apr_pstrndup(pool, result, result_len);
    free(result); // Free the malloc'd result from ac_replace_alloc
    
    apr_time_t end_time = apr_time_now();
#ifndef TEST_BUILD
    if (r) {
        ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, 
                      "mod_replace: Slow path completed - create_time=%d μs, compile_time=%d μs, "
                      "replace_time=%d μs, total_time=%d μs, input_len=%zu, output_len=%zu, "
                      "patterns=%d",
                      (int)(compile_start - create_start), (int)(compile_end - compile_start),
                      (int)(replace_end - replace_start), (int)(end_time - start_time),
                      input_len, result_len, patterns_added);
    }
#endif
    
    return pool_result ? pool_result : apr_pstrdup(pool, input);
}

static apr_status_t replace_output_filter(ap_filter_t *f, apr_bucket_brigade *bb)
{
    replace_config *cfg;
    replace_ctx *ctx;
    apr_bucket *b, *next_b;
    apr_status_t rv;
    int eos_found = 0;
    
    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, f ? f->r : NULL, "mod_replace: filter entry point");
    
    if (!f || !f->r || !bb) {
        ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, f ? f->r : NULL, "mod_replace: filter called with null params");
        return ap_pass_brigade(f->next, bb);
    }
    
    cfg = ap_get_module_config(f->r->per_dir_config, &replace_module);
    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, f->r, 
                  "mod_replace: filter called - enabled=%d, replacements_count=%d", 
                  cfg ? cfg->enabled : -1, 
                  cfg ? apr_hash_count(cfg->replacements) : -1);
    
    if (!cfg || !cfg->enabled || apr_hash_count(cfg->replacements) == 0) {
        ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, f->r, "mod_replace: passing brigade through");
        return ap_pass_brigade(f->next, bb);
    }
    
    // Only process text content to avoid encoding issues
    const char *content_type = f->r->content_type;
    
    if (content_type && strncmp(content_type, "text/", 5) != 0 && 
        strstr(content_type, "html") == NULL && strstr(content_type, "xml") == NULL) {
        ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, f->r, 
                      "mod_replace: skipping non-text content type: %s", content_type);
        return ap_pass_brigade(f->next, bb);
    }
    
    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, f->r, 
                  "mod_replace: processing content type: %s", content_type ? content_type : "unknown");
    
    // Ensure automaton is compiled
    ensure_automaton_compiled(cfg);
    
    ctx = f->ctx;
    if (!ctx) {
        ctx = apr_pcalloc(f->r->pool, sizeof(replace_ctx));
        if (!ctx) {
            return ap_pass_brigade(f->next, bb);
        }
        ctx->bb = apr_brigade_create(f->r->pool, f->c->bucket_alloc);
        if (!ctx->bb) {
            return ap_pass_brigade(f->next, bb);
        }
        ctx->pool = f->r->pool;
        f->ctx = ctx;
    }
    
    for (b = APR_BRIGADE_FIRST(bb); b != APR_BRIGADE_SENTINEL(bb); b = next_b) {
        next_b = APR_BUCKET_NEXT(b);
        
        if (APR_BUCKET_IS_EOS(b)) {
            eos_found = 1;
            break;
        }
        
        if (APR_BUCKET_IS_METADATA(b)) {
            continue;
        }
        
        APR_BUCKET_REMOVE(b);
        APR_BRIGADE_INSERT_TAIL(ctx->bb, b);
    }
    
    if (eos_found) {
        if (!APR_BRIGADE_EMPTY(ctx->bb)) {
            char *data;
            apr_size_t len;
            
            rv = apr_brigade_pflatten(ctx->bb, &data, &len, ctx->pool);
            if (rv == APR_SUCCESS && data && len > 0) {
                char *processed = perform_replacements(ctx->pool, data, cfg->replacements, f->r);
                if (processed) {
                    apr_bucket *data_bucket = apr_bucket_pool_create(processed, strlen(processed), 
                                                                   ctx->pool, f->c->bucket_alloc);
                    if (data_bucket) {
                        apr_brigade_cleanup(bb);
                        APR_BRIGADE_INSERT_TAIL(bb, data_bucket);
                        APR_BRIGADE_INSERT_TAIL(bb, apr_bucket_eos_create(f->c->bucket_alloc));
                    }
                }
            }
        }
        return ap_pass_brigade(f->next, bb);
    }
    
    return APR_SUCCESS;
}

static void insert_replace_filter(request_rec *r)
{
    replace_config *cfg = ap_get_module_config(r->per_dir_config, &replace_module);
    
    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, 
                  "mod_replace: insert_replace_filter called - enabled=%d, replacements_count=%d", 
                  cfg ? cfg->enabled : -1, 
                  cfg ? apr_hash_count(cfg->replacements) : -1);
    
    if (cfg->enabled && apr_hash_count(cfg->replacements) > 0) {
        ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, "mod_replace: Adding REPLACE filter");
        ap_add_output_filter("REPLACE", NULL, r, r->connection);
    }
}

static const command_rec replace_cmds[] = {
    AP_INIT_TAKE2("ReplaceRule", set_replace_rule, NULL, ACCESS_CONF | RSRC_CONF,
                  "Define a replacement rule: ReplaceRule <search> <replace>"),
    AP_INIT_FLAG("ReplaceEnable", set_replace_enable, NULL, ACCESS_CONF | RSRC_CONF,
                 "Enable or disable text replacement"),
    { NULL }
};

#ifndef TEST_BUILD
static void register_hooks(apr_pool_t *pool)
{
    ap_log_perror(APLOG_MARK, APLOG_DEBUG, 0, pool, "mod_replace: Registering hooks");
    ap_register_output_filter("REPLACE", replace_output_filter, NULL, AP_FTYPE_RESOURCE);
    ap_hook_insert_filter(insert_replace_filter, NULL, NULL, APR_HOOK_MIDDLE);
}

module AP_MODULE_DECLARE_DATA replace_module = {
    STANDARD20_MODULE_STUFF,
    create_replace_config,
    merge_replace_config,
    NULL,
    NULL,
    replace_cmds,
    register_hooks
};
#endif