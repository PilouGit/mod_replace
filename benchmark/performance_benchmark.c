/*
 * Performance Benchmark: Aho-Corasick vs Sequential Search
 * Compares mod_replace (Aho-Corasick) vs mod_substitute (sequential) approaches
 *
 * Version 1.2.0: Includes callback optimization benchmark for variable expansion
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "../inc/aho_corasick.h"

#define MAX_PATTERNS 1000
#define MAX_LINE_LEN 1024

typedef struct {
    char *search;
    char *replace;
} pattern_t;

typedef struct {
    pattern_t patterns[MAX_PATTERNS];
    int count;
} pattern_list_t;

/* Timing utilities */
static inline double get_time_us() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec * 1000000.0 + (double)tv.tv_usec;
}

/* Load patterns from file */
int load_patterns(const char *filename, pattern_list_t *patterns) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        perror("Failed to open patterns file");
        return -1;
    }

    char line[MAX_LINE_LEN];
    patterns->count = 0;

    while (fgets(line, sizeof(line), f) && patterns->count < MAX_PATTERNS) {
        // Remove newline
        line[strcspn(line, "\n")] = 0;

        // Split on |
        char *sep = strchr(line, '|');
        if (!sep) continue;

        *sep = '\0';
        patterns->patterns[patterns->count].search = strdup(line);
        patterns->patterns[patterns->count].replace = strdup(sep + 1);
        patterns->count++;
    }

    fclose(f);
    printf("Loaded %d patterns\n", patterns->count);
    return patterns->count;
}

/* Free patterns */
void free_patterns(pattern_list_t *patterns) {
    for (int i = 0; i < patterns->count; i++) {
        free(patterns->patterns[i].search);
        free(patterns->patterns[i].replace);
    }
}

/* Load text content from file */
char *load_file(const char *filename, size_t *size) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        perror("Failed to open content file");
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *content = malloc(*size + 1);
    if (!content) {
        fclose(f);
        return NULL;
    }

    fread(content, 1, *size, f);
    content[*size] = '\0';
    fclose(f);

    return content;
}

/* Simple string search and replace (like mod_substitute's simple pattern mode) */
char *simple_search_replace(const char *haystack, const char *needle,
                            const char *replacement) {
    if (!haystack || !needle || !replacement) return NULL;

    size_t haystack_len = strlen(haystack);
    size_t needle_len = strlen(needle);
    size_t replacement_len = strlen(replacement);

    // Count occurrences
    int count = 0;
    const char *pos = haystack;
    while ((pos = strstr(pos, needle)) != NULL) {
        count++;
        pos += needle_len;
    }

    if (count == 0) {
        return strdup(haystack);
    }

    // Allocate result
    size_t result_len = haystack_len + count * (replacement_len - needle_len);
    char *result = malloc(result_len + 1);
    if (!result) return NULL;

    // Build result
    char *dest = result;
    const char *src = haystack;
    while ((pos = strstr(src, needle)) != NULL) {
        size_t prefix_len = pos - src;
        memcpy(dest, src, prefix_len);
        dest += prefix_len;
        memcpy(dest, replacement, replacement_len);
        dest += replacement_len;
        src = pos + needle_len;
    }
    strcpy(dest, src);

    return result;
}

/* Sequential approach (like mod_substitute) */
char *sequential_replace(const char *input, pattern_list_t *patterns,
                         double *time_us) {
    double start = get_time_us();

    char *current = strdup(input);
    char *next;

    // Apply each pattern sequentially
    for (int i = 0; i < patterns->count; i++) {
        next = simple_search_replace(current,
                                     patterns->patterns[i].search,
                                     patterns->patterns[i].replace);
        free(current);
        current = next;
        if (!current) break;
    }

    double end = get_time_us();
    *time_us = end - start;

    return current;
}

/* Aho-Corasick approach (like mod_replace) */
char *aho_corasick_replace(const char *input, pattern_list_t *patterns,
                           double *compile_time_us, double *search_time_us,
                           double *total_time_us) {
    double total_start = get_time_us();

    // Create and compile automaton
    double compile_start = get_time_us();
    ac_automaton_t *ac = ac_create(0);
    if (!ac) return NULL;

    for (int i = 0; i < patterns->count; i++) {
        ac_add_pattern(ac,
                      patterns->patterns[i].search,
                      strlen(patterns->patterns[i].search),
                      patterns->patterns[i].replace,
                      strlen(patterns->patterns[i].replace));
    }

    if (!ac_compile(ac)) {
        ac_destroy(ac);
        return NULL;
    }
    double compile_end = get_time_us();
    *compile_time_us = compile_end - compile_start;

    // Perform replacement
    double search_start = get_time_us();
    size_t result_len;
    char *result = ac_replace_alloc(ac, input, strlen(input), &result_len);
    double search_end = get_time_us();
    *search_time_us = search_end - search_start;

    ac_destroy(ac);

    double total_end = get_time_us();
    *total_time_us = total_end - total_start;

    return result;
}

/* === Version 1.2.0 Callback Optimization Benchmark === */

typedef struct {
    const char *replacement_template;
    int request_number;  // Simulates per-request context
} template_data_t;

/* Simulates variable expansion per request */
static const char *expand_variable(const char *template, int request_num) {
    static char buffer[256];

    if (strstr(template, "%{UNIQUE_STRING}")) {
        snprintf(buffer, sizeof(buffer), "nonce-%d", request_num);
        return buffer;
    }
    if (strstr(template, "${REMOTE_USER}")) {
        snprintf(buffer, sizeof(buffer), "user-%d", request_num);
        return buffer;
    }
    if (strstr(template, "${SERVER_NAME}")) {
        return "example.com";
    }

    return template;
}

/* Callback for v1.2.0 optimization */
static const char *replacement_callback(
    const char *pattern,
    size_t pattern_len,
    void *user_data,
    void *context_data,
    size_t *replacement_len
) {
    template_data_t *tmpl = (template_data_t *)user_data;
    int *request_num = (int *)context_data;

    if (!tmpl || !tmpl->replacement_template) {
        *replacement_len = 0;
        return "";
    }

    const char *expanded = expand_variable(tmpl->replacement_template, *request_num);
    *replacement_len = strlen(expanded);
    return expanded;
}

/* OLD approach (v1.1.0 and before): Recreate automaton with variables per request */
char *old_variable_replace(const char *input, pattern_list_t *patterns,
                           int request_num, double *time_us) {
    double start = get_time_us();

    // Create automaton
    ac_automaton_t *ac = ac_create(0);
    if (!ac) return NULL;

    // Add patterns with expanded variables (OLD: expand before adding)
    for (int i = 0; i < patterns->count; i++) {
        const char *expanded = expand_variable(patterns->patterns[i].replace, request_num);
        ac_add_pattern(ac,
                      patterns->patterns[i].search,
                      strlen(patterns->patterns[i].search),
                      expanded,
                      strlen(expanded));
    }

    // Compile automaton (SLOW: done per request!)
    if (!ac_compile(ac)) {
        ac_destroy(ac);
        return NULL;
    }

    // Perform replacement
    size_t result_len;
    char *result = ac_replace_alloc(ac, input, strlen(input), &result_len);

    // Destroy automaton (recreated next request!)
    ac_destroy(ac);

    double end = get_time_us();
    *time_us = end - start;

    return result;
}

/* NEW approach (v1.2.0): Precompiled automaton with callback */
typedef struct {
    ac_automaton_t *ac;
    template_data_t *templates;
} precompiled_automaton_t;

precompiled_automaton_t *create_precompiled_automaton(pattern_list_t *patterns) {
    precompiled_automaton_t *pre = malloc(sizeof(precompiled_automaton_t));
    if (!pre) return NULL;

    pre->ac = ac_create(0);
    if (!pre->ac) {
        free(pre);
        return NULL;
    }

    // Allocate templates
    pre->templates = calloc(patterns->count, sizeof(template_data_t));
    if (!pre->templates) {
        ac_destroy(pre->ac);
        free(pre);
        return NULL;
    }

    // Add patterns with templates as user_data
    for (int i = 0; i < patterns->count; i++) {
        pre->templates[i].replacement_template = patterns->patterns[i].replace;

        ac_add_pattern_ex(pre->ac,
                         patterns->patterns[i].search,
                         strlen(patterns->patterns[i].search),
                         NULL, 0,  // No static replacement
                         &pre->templates[i]);  // Template as user_data
    }

    // Compile ONCE (not per request!)
    if (!ac_compile(pre->ac)) {
        free(pre->templates);
        ac_destroy(pre->ac);
        free(pre);
        return NULL;
    }

    return pre;
}

void destroy_precompiled_automaton(precompiled_automaton_t *pre) {
    if (pre) {
        if (pre->ac) ac_destroy(pre->ac);
        if (pre->templates) free(pre->templates);
        free(pre);
    }
}

char *new_variable_replace(precompiled_automaton_t *pre, const char *input,
                           int request_num, double *time_us) {
    double start = get_time_us();

    // Use callback for variable expansion (FAST: automaton already compiled!)
    size_t result_len;
    char *result = ac_replace_with_callback(pre->ac,
                                           input,
                                           strlen(input),
                                           replacement_callback,
                                           &request_num,  // Context for this request
                                           &result_len);

    double end = get_time_us();
    *time_us = end - start;

    return result;
}

/* Benchmark v1.2.0 optimization */
void benchmark_v1_2_optimization(const char *content_file, pattern_list_t *patterns,
                                 int iterations) {
    size_t content_size;
    char *content = load_file(content_file, &content_size);
    if (!content) return;

    printf("\n╔═══════════════════════════════════════════════════════════════════╗\n");
    printf("║   v1.2.0 VARIABLE OPTIMIZATION BENCHMARK                          ║\n");
    printf("╚═══════════════════════════════════════════════════════════════════╝\n");
    printf("File: %s (%.2f KB)\n", content_file, content_size / 1024.0);
    printf("Patterns: %d\n", patterns->count);
    printf("Iterations: %d (simulating %d requests)\n\n", iterations, iterations);

    // Benchmark OLD approach (v1.1.0 and before)
    printf("┌─────────────────────────────────────────────────────────────────┐\n");
    printf("│ OLD (v1.1.0): Recreate automaton per request with variables    │\n");
    printf("└─────────────────────────────────────────────────────────────────┘\n");

    double old_total = 0;
    for (int i = 0; i < iterations; i++) {
        double time;
        char *result = old_variable_replace(content, patterns, i, &time);
        old_total += time;
        free(result);
    }
    double old_avg = old_total / iterations;

    printf("  Total time:      %.2f ms\n", old_total / 1000.0);
    printf("  Average/request: %.2f μs\n", old_avg);
    printf("  ⚠️  Automaton created/compiled/destroyed %d times!\n\n", iterations);

    // Benchmark NEW approach (v1.2.0)
    printf("┌─────────────────────────────────────────────────────────────────┐\n");
    printf("│ NEW (v1.2.0): Precompiled automaton with callback              │\n");
    printf("└─────────────────────────────────────────────────────────────────┘\n");

    // Compile automaton ONCE
    double compile_start = get_time_us();
    precompiled_automaton_t *pre = create_precompiled_automaton(patterns);
    double compile_end = get_time_us();
    double compile_time = compile_end - compile_start;

    printf("  Compilation time (one-time): %.2f μs\n", compile_time);

    // Benchmark per-request time
    double new_total = 0;
    for (int i = 0; i < iterations; i++) {
        double time;
        char *result = new_variable_replace(pre, content, i, &time);
        new_total += time;
        free(result);
    }
    double new_avg = new_total / iterations;

    printf("  Total time:      %.2f ms\n", new_total / 1000.0);
    printf("  Average/request: %.2f μs\n", new_avg);
    printf("  ✅ Automaton compiled ONCE, reused %d times!\n\n", iterations);

    destroy_precompiled_automaton(pre);

    // Performance comparison
    printf("┌─────────────────────────────────────────────────────────────────┐\n");
    printf("│ PERFORMANCE IMPROVEMENT                                         │\n");
    printf("└─────────────────────────────────────────────────────────────────┘\n");

    double speedup = old_avg / new_avg;
    double cpu_reduction = ((old_avg - new_avg) / old_avg) * 100.0;
    double time_saved_total = old_total - new_total;

    printf("  🚀 Speedup:          %.2fx faster\n", speedup);
    printf("  📉 CPU reduction:    %.1f%%\n", cpu_reduction);
    printf("  ⏱️  Time saved:       %.2f μs/request\n", old_avg - new_avg);
    printf("  💰 Total savings:    %.2f ms for %d requests\n\n",
           time_saved_total / 1000.0, iterations);

    // Production impact
    printf("┌─────────────────────────────────────────────────────────────────┐\n");
    printf("│ PRODUCTION IMPACT (at 1000 req/s)                               │\n");
    printf("└─────────────────────────────────────────────────────────────────┘\n");

    double old_cpu_cores = (old_avg * 1000) / 1000000.0;
    double new_cpu_cores = (new_avg * 1000) / 1000000.0;

    printf("  OLD (v1.1.0):\n");
    printf("    Latency:     %.2f μs/request\n", old_avg);
    printf("    CPU usage:   %.3f cores\n", old_cpu_cores);
    printf("    Compilations: 1000/sec ⚠️\n\n");

    printf("  NEW (v1.2.0):\n");
    printf("    Latency:     %.2f μs/request\n", new_avg);
    printf("    CPU usage:   %.3f cores\n", new_cpu_cores);
    printf("    Compilations: 1/startup ✅\n\n");

    printf("  Throughput increase: %.2fx more requests/core\n", speedup);

    free(content);
}

/* Run benchmark */
void run_benchmark(const char *content_file, pattern_list_t *patterns,
                   int iterations) {
    size_t content_size;
    char *content = load_file(content_file, &content_size);
    if (!content) return;

    printf("\n=== Benchmarking: %s (%.2f KB, %d patterns, %d iterations) ===\n",
           content_file, content_size / 1024.0, patterns->count, iterations);

    // Warmup
    printf("Warming up...\n");
    for (int i = 0; i < 3; i++) {
        double time;
        char *result = sequential_replace(content, patterns, &time);
        free(result);

        double compile_time, search_time, total_time;
        result = aho_corasick_replace(content, patterns, &compile_time,
                                      &search_time, &total_time);
        free(result);
    }

    // Benchmark sequential approach
    printf("\n--- Sequential Approach (mod_substitute style) ---\n");
    double seq_times[iterations];
    double seq_total = 0;

    for (int i = 0; i < iterations; i++) {
        double time;
        char *result = sequential_replace(content, patterns, &time);
        seq_times[i] = time;
        seq_total += time;
        free(result);
    }

    double seq_avg = seq_total / iterations;
    printf("Average time: %.2f μs (%.2f ms)\n", seq_avg, seq_avg / 1000.0);
    printf("Total time: %.2f ms\n", seq_total / 1000.0);
    printf("Throughput: %.2f MB/s\n",
           (content_size * iterations / 1024.0 / 1024.0) / (seq_total / 1000000.0));

    // Benchmark Aho-Corasick approach
    printf("\n--- Aho-Corasick Approach (mod_replace style) ---\n");
    double ac_compile_times[iterations];
    double ac_search_times[iterations];
    double ac_total_times[iterations];
    double ac_compile_total = 0, ac_search_total = 0, ac_total = 0;

    for (int i = 0; i < iterations; i++) {
        double compile_time, search_time, total_time;
        char *result = aho_corasick_replace(content, patterns,
                                            &compile_time, &search_time, &total_time);
        ac_compile_times[i] = compile_time;
        ac_search_times[i] = search_time;
        ac_total_times[i] = total_time;
        ac_compile_total += compile_time;
        ac_search_total += search_time;
        ac_total += total_time;
        free(result);
    }

    double ac_avg_compile = ac_compile_total / iterations;
    double ac_avg_search = ac_search_total / iterations;
    double ac_avg_total = ac_total / iterations;

    printf("Average compile time: %.2f μs (%.2f ms)\n",
           ac_avg_compile, ac_avg_compile / 1000.0);
    printf("Average search time: %.2f μs (%.2f ms)\n",
           ac_avg_search, ac_avg_search / 1000.0);
    printf("Average total time: %.2f μs (%.2f ms)\n",
           ac_avg_total, ac_avg_total / 1000.0);
    printf("Total time: %.2f ms\n", ac_total / 1000.0);
    printf("Throughput: %.2f MB/s\n",
           (content_size * iterations / 1024.0 / 1024.0) / (ac_total / 1000000.0));

    // Comparison
    printf("\n--- Performance Comparison ---\n");
    double speedup = seq_avg / ac_avg_total;
    printf("Aho-Corasick is %.2fx %s than Sequential\n",
           speedup >= 1.0 ? speedup : 1.0/speedup,
           speedup >= 1.0 ? "faster" : "slower");
    printf("Time saved per request: %.2f μs (%.2f ms)\n",
           seq_avg - ac_avg_total, (seq_avg - ac_avg_total) / 1000.0);
    printf("Sequential overhead: %.2f%%\n",
           ((seq_avg - ac_avg_total) / seq_avg) * 100.0);

    // Simulating precompiled scenario (mod_replace in production)
    printf("\n--- Precompiled Automaton Scenario (production mod_replace) ---\n");
    printf("If automaton is precompiled (one-time cost):\n");
    printf("  - Compile time (one-time): %.2f μs\n", ac_avg_compile);
    printf("  - Per-request time: %.2f μs\n", ac_avg_search);
    double precompiled_speedup = seq_avg / ac_avg_search;
    printf("  - Speedup vs Sequential: %.2fx faster\n", precompiled_speedup);

    free(content);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <patterns_file> <iterations> [content_files...]\n", argv[0]);
        printf("       %s --v1.2 <patterns_file> <iterations> [content_file]\n", argv[0]);
        printf("\nExamples:\n");
        printf("  %s patterns.txt 100 test_content_10kb.html\n", argv[0]);
        printf("  %s --v1.2 patterns_with_vars.txt 1000 test_content_100kb.html\n", argv[0]);
        return 1;
    }

    // Check for v1.2.0 optimization benchmark
    if (strcmp(argv[1], "--v1.2") == 0) {
        if (argc < 4) {
            printf("Error: --v1.2 requires patterns_file and iterations\n");
            return 1;
        }

        const char *patterns_file = argv[2];
        int iterations = atoi(argv[3]);

        // Load patterns
        pattern_list_t patterns;
        if (load_patterns(patterns_file, &patterns) < 0) {
            return 1;
        }

        const char *content_file = (argc > 4) ? argv[4] : "test_content_100kb.html";

        printf("╔═══════════════════════════════════════════════════════════════════╗\n");
        printf("║                 mod_replace v1.2.0 OPTIMIZATION                   ║\n");
        printf("║         Callback-based Variable Expansion Benchmark              ║\n");
        printf("╚═══════════════════════════════════════════════════════════════════╝\n");

        benchmark_v1_2_optimization(content_file, &patterns, iterations);

        free_patterns(&patterns);

        printf("\n╔═══════════════════════════════════════════════════════════════════╗\n");
        printf("║ CONCLUSION: v1.2.0 eliminates per-request automaton compilation  ║\n");
        printf("║             for MASSIVE performance gains with variables!        ║\n");
        printf("╚═══════════════════════════════════════════════════════════════════╝\n\n");

        return 0;
    }

    const char *patterns_file = argv[1];
    int iterations = atoi(argv[2]);

    // Load patterns
    pattern_list_t patterns;
    if (load_patterns(patterns_file, &patterns) < 0) {
        return 1;
    }

    // Default content files if not specified
    const char *default_files[] = {
        "test_content_10kb.html",
        "test_content_50kb.html",
        "test_content_100kb.html",
        "test_content_500kb.html"
    };

    printf("╔═══════════════════════════════════════════════════════════════════╗\n");
    printf("║     mod_replace vs mod_substitute Performance Benchmark          ║\n");
    printf("╚═══════════════════════════════════════════════════════════════════╝\n");

    if (argc > 3) {
        // Use specified files
        for (int i = 3; i < argc; i++) {
            run_benchmark(argv[i], &patterns, iterations);
        }
    } else {
        // Use default files
        for (int i = 0; i < 4; i++) {
            run_benchmark(default_files[i], &patterns, iterations);
        }
    }

    free_patterns(&patterns);

    printf("\n=== Benchmark Complete ===\n");
    return 0;
}
