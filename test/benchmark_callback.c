/*
 * Performance benchmark for callback-based variable optimization
 *
 * Compares:
 * - OLD: Creating/compiling/destroying automaton per request
 * - NEW: Using precompiled automaton with callback
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "../inc/aho_corasick.h"

// Get time in microseconds
static long long get_time_us() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000000 + tv.tv_usec;
}

typedef struct {
    const char *replacement_template;
    void *context;
} template_data_t;

static const char *expand_variable(const char *template, void *context) {
    if (strcmp(template, "${REMOTE_USER}") == 0) {
        return (const char *)context;
    }
    if (strcmp(template, "%{UNIQUE_STRING}") == 0) {
        return (const char *)context;
    }
    return template;
}

static const char *replacement_callback(
    const char *pattern,
    size_t pattern_len,
    void *user_data,
    void *context_data,
    size_t *replacement_len
) {
    template_data_t *tmpl = (template_data_t *)user_data;
    if (!tmpl || !tmpl->replacement_template) {
        *replacement_len = 0;
        return "";
    }
    const char *expanded = expand_variable(tmpl->replacement_template, context_data);
    *replacement_len = expanded ? strlen(expanded) : 0;
    return expanded ? expanded : "";
}

// OLD approach: Recreate automaton per request
static long long benchmark_old_approach(const char *input, int iterations) {
    long long total_time = 0;

    for (int i = 0; i < iterations; i++) {
        char nonce[32];
        snprintf(nonce, sizeof(nonce), "nonce-%d", i);

        long long start = get_time_us();

        // Create automaton
        ac_automaton_t *ac = ac_create(0);

        // Add pattern with expanded variable
        ac_add_pattern(ac, "___CSP_NONCE___", 15, nonce, strlen(nonce));

        // Compile automaton
        ac_compile(ac);

        // Perform replacement
        size_t result_len;
        char *result = ac_replace_alloc(ac, input, strlen(input), &result_len);

        // Destroy automaton
        ac_destroy(ac);

        long long end = get_time_us();
        total_time += (end - start);

        free(result);
    }

    return total_time;
}

// NEW approach: Precompiled automaton with callback
static long long benchmark_new_approach(const char *input, int iterations) {
    // Create and compile automaton ONCE
    ac_automaton_t *ac = ac_create(0);

    template_data_t tmpl;
    tmpl.replacement_template = "%{UNIQUE_STRING}";
    ac_add_pattern_ex(ac, "___CSP_NONCE___", 15, NULL, 0, &tmpl);

    ac_compile(ac);

    // Measure only the replacement time (automaton already compiled)
    long long total_time = 0;

    for (int i = 0; i < iterations; i++) {
        char nonce[32];
        snprintf(nonce, sizeof(nonce), "nonce-%d", i);

        long long start = get_time_us();

        // Perform replacement with callback
        size_t result_len;
        char *result = ac_replace_with_callback(ac, input, strlen(input),
                                               replacement_callback, nonce, &result_len);

        long long end = get_time_us();
        total_time += (end - start);

        free(result);
    }

    ac_destroy(ac);

    return total_time;
}

int main() {
    printf("Callback-based Variable Optimization Benchmark\n");
    printf("===============================================\n\n");

    // Test with different content sizes
    const char *test_cases[] = {
        "<script nonce='___CSP_NONCE___'></script>",
        "<html><head><script nonce='___CSP_NONCE___'></script></head>"
        "<body><script nonce='___CSP_NONCE___'></script></body></html>",
        NULL
    };

    const char *test_names[] = {
        "Small HTML (60 bytes)",
        "Medium HTML (150 bytes)"
    };

    int iterations = 1000;

    for (int test = 0; test_cases[test] != NULL; test++) {
        const char *input = test_cases[test];
        size_t input_len = strlen(input);

        printf("Test Case: %s\n", test_names[test]);
        printf("----------------------------------------\n");
        printf("Input size: %zu bytes\n", input_len);
        printf("Iterations: %d\n\n", iterations);

        // Benchmark OLD approach
        printf("OLD: Recreate automaton per request\n");
        long long old_time = benchmark_old_approach(input, iterations);
        double old_avg = (double)old_time / iterations;
        printf("  Total time:   %lld μs\n", old_time);
        printf("  Average/req:  %.2f μs\n\n", old_avg);

        // Benchmark NEW approach
        printf("NEW: Precompiled automaton with callback\n");
        long long new_time = benchmark_new_approach(input, iterations);
        double new_avg = (double)new_time / iterations;
        printf("  Total time:   %lld μs\n", new_time);
        printf("  Average/req:  %.2f μs\n\n", new_avg);

        // Calculate speedup
        double speedup = (double)old_time / new_time;
        double time_saved = ((double)(old_time - new_time) / old_time) * 100.0;

        printf("Performance Improvement:\n");
        printf("  Speedup:      %.2fx faster\n", speedup);
        printf("  Time saved:   %.1f%%\n", time_saved);
        printf("  Reduction:    %lld μs -> %lld μs (-%lld μs)\n\n",
               old_time, new_time, old_time - new_time);
        printf("========================================\n\n");
    }

    // Multi-pattern test
    printf("Multi-Pattern Test (3 variables, 100 patterns)\n");
    printf("================================================\n");

    const char *multi_input = "User: {{USER}}, Server: {{SERVER}}, Nonce: ___CSP_NONCE___";

    // OLD approach with multiple patterns
    long long multi_old_start = get_time_us();
    for (int i = 0; i < iterations; i++) {
        ac_automaton_t *ac = ac_create(0);

        // Add 3 variable patterns
        ac_add_pattern(ac, "{{USER}}", 8, "alice", 5);
        ac_add_pattern(ac, "{{SERVER}}", 10, "server.com", 10);
        char nonce[32];
        snprintf(nonce, sizeof(nonce), "nonce-%d", i);
        ac_add_pattern(ac, "___CSP_NONCE___", 15, nonce, strlen(nonce));

        // Add 97 static patterns to reach 100 total
        for (int j = 0; j < 97; j++) {
            char pattern[32], replacement[32];
            snprintf(pattern, sizeof(pattern), "{{STATIC%d}}", j);
            snprintf(replacement, sizeof(replacement), "value%d", j);
            ac_add_pattern(ac, pattern, strlen(pattern), replacement, strlen(replacement));
        }

        ac_compile(ac);

        size_t result_len;
        char *result = ac_replace_alloc(ac, multi_input, strlen(multi_input), &result_len);
        free(result);

        ac_destroy(ac);
    }
    long long multi_old_time = get_time_us() - multi_old_start;

    // NEW approach with multiple patterns
    ac_automaton_t *multi_ac = ac_create(0);

    template_data_t tmpl_user, tmpl_server, tmpl_nonce;
    tmpl_user.replacement_template = "${REMOTE_USER}";
    tmpl_server.replacement_template = "${SERVER_NAME}";
    tmpl_nonce.replacement_template = "%{UNIQUE_STRING}";

    ac_add_pattern_ex(multi_ac, "{{USER}}", 8, NULL, 0, &tmpl_user);
    ac_add_pattern_ex(multi_ac, "{{SERVER}}", 10, NULL, 0, &tmpl_server);
    ac_add_pattern_ex(multi_ac, "___CSP_NONCE___", 15, NULL, 0, &tmpl_nonce);

    // Add 97 static patterns
    for (int j = 0; j < 97; j++) {
        char pattern[32], replacement[32];
        snprintf(pattern, sizeof(pattern), "{{STATIC%d}}", j);
        snprintf(replacement, sizeof(replacement), "value%d", j);
        template_data_t *tmpl_static = malloc(sizeof(template_data_t));
        tmpl_static->replacement_template = strdup(replacement);
        ac_add_pattern_ex(multi_ac, pattern, strlen(pattern), NULL, 0, tmpl_static);
    }

    ac_compile(multi_ac);

    long long multi_new_start = get_time_us();
    for (int i = 0; i < iterations; i++) {
        char nonce[32];
        snprintf(nonce, sizeof(nonce), "nonce-%d", i);

        size_t result_len;
        char *result = ac_replace_with_callback(multi_ac, multi_input, strlen(multi_input),
                                               replacement_callback, nonce, &result_len);
        free(result);
    }
    long long multi_new_time = get_time_us() - multi_new_start;

    ac_destroy(multi_ac);

    printf("OLD: Recreate automaton with 100 patterns per request\n");
    printf("  Total time:   %lld μs\n", multi_old_time);
    printf("  Average/req:  %.2f μs\n\n", (double)multi_old_time / iterations);

    printf("NEW: Precompiled automaton with 100 patterns, callback for 3 variables\n");
    printf("  Total time:   %lld μs\n", multi_new_time);
    printf("  Average/req:  %.2f μs\n\n", (double)multi_new_time / iterations);

    double multi_speedup = (double)multi_old_time / multi_new_time;
    printf("Performance Improvement:\n");
    printf("  Speedup:      %.2fx faster\n", multi_speedup);
    printf("  Time saved:   %.1f%%\n\n", ((double)(multi_old_time - multi_new_time) / multi_old_time) * 100.0);

    // Production impact
    printf("========================================\n");
    printf("Production Impact (1000 req/s)\n");
    printf("========================================\n\n");

    double old_latency = (double)multi_old_time / iterations;
    double new_latency = (double)multi_new_time / iterations;

    printf("Before Optimization:\n");
    printf("  Latency:        %.0f μs/req\n", old_latency);
    printf("  CPU cores:      %.2f cores (at 1000 req/s)\n", (old_latency * 1000) / 1000000.0);
    printf("\n");

    printf("After Optimization:\n");
    printf("  Latency:        %.0f μs/req\n", new_latency);
    printf("  CPU cores:      %.2f cores (at 1000 req/s)\n", (new_latency * 1000) / 1000000.0);
    printf("\n");

    printf("Savings:\n");
    printf("  CPU reduction:  %.1f%%\n", ((old_latency - new_latency) / old_latency) * 100.0);
    printf("  Throughput:     %.0fx more requests per core\n", multi_speedup);

    return 0;
}
