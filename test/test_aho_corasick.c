#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "aho_corasick.h"

void test_basic_replacement() {
    printf("Test 1: Basic replacement...\n");
    
    ac_automaton_t *ac = ac_create(0);
    assert(ac != NULL);
    
    assert(ac_add_pattern(ac, "hello", 0, "hi", 0));
    assert(ac_add_pattern(ac, "world", 0, "universe", 0));
    assert(ac_compile(ac));
    
    char buffer[256] = "hello world";
    size_t new_len = 0;
    int replacements = ac_replace_inplace(ac, buffer, strlen(buffer), sizeof(buffer), &new_len);
    
    printf("  Original: \"hello world\"\n");
    printf("  Result:   \"%.*s\"\n", (int)new_len, buffer);
    printf("  Replacements: %d\n", replacements);
    
    assert(replacements == 2);
    assert(new_len == strlen("hi universe"));
    assert(strncmp(buffer, "hi universe", new_len) == 0);
    
    ac_destroy(ac);
    printf("  ✓ Passed\n\n");
}

void test_overlapping_patterns() {
    printf("Test 2: Overlapping patterns...\n");
    
    ac_automaton_t *ac = ac_create(0);
    assert(ac != NULL);
    
    assert(ac_add_pattern(ac, "abc", 0, "123", 0));
    assert(ac_add_pattern(ac, "bcd", 0, "456", 0));
    assert(ac_compile(ac));
    
    char buffer[256] = "abcd";
    size_t new_len = 0;
    int replacements = ac_replace_inplace(ac, buffer, strlen(buffer), sizeof(buffer), &new_len);
    
    printf("  Original: \"abcd\"\n");
    printf("  Result:   \"%.*s\"\n", (int)new_len, buffer);
    printf("  Replacements: %d\n", replacements);
    
    // Should replace "abc" first, leaving "123d"
    assert(replacements >= 1);
    
    ac_destroy(ac);
    printf("  ✓ Passed\n\n");
}

void test_multiple_occurrences() {
    printf("Test 3: Multiple occurrences...\n");
    
    ac_automaton_t *ac = ac_create(0);
    assert(ac != NULL);
    
    assert(ac_add_pattern(ac, "test", 0, "exam", 0));
    assert(ac_compile(ac));
    
    char buffer[256] = "test test test";
    size_t new_len = 0;
    int replacements = ac_replace_inplace(ac, buffer, strlen(buffer), sizeof(buffer), &new_len);
    
    printf("  Original: \"test test test\"\n");
    printf("  Result:   \"%.*s\"\n", (int)new_len, buffer);
    printf("  Replacements: %d\n", replacements);
    
    assert(replacements == 3);
    assert(strncmp(buffer, "exam exam exam", new_len) == 0);
    
    ac_destroy(ac);
    printf("  ✓ Passed\n\n");
}

void test_length_changes() {
    printf("Test 4: Length changes...\n");
    
    ac_automaton_t *ac = ac_create(0);
    assert(ac != NULL);
    
    // Shorter replacement
    assert(ac_add_pattern(ac, "hello", 0, "hi", 0));
    // Longer replacement
    assert(ac_add_pattern(ac, "ok", 0, "okay", 0));
    assert(ac_compile(ac));
    
    char buffer[256] = "hello ok";
    size_t new_len = 0;
    int replacements = ac_replace_inplace(ac, buffer, strlen(buffer), sizeof(buffer), &new_len);
    
    printf("  Original: \"hello ok\"\n");
    printf("  Result:   \"%.*s\"\n", (int)new_len, buffer);
    printf("  Replacements: %d\n", replacements);
    
    assert(replacements == 2);
    assert(strncmp(buffer, "hi okay", new_len) == 0);
    
    ac_destroy(ac);
    printf("  ✓ Passed\n\n");
}

void test_no_matches() {
    printf("Test 5: No matches...\n");
    
    ac_automaton_t *ac = ac_create(0);
    assert(ac != NULL);
    
    assert(ac_add_pattern(ac, "xyz", 0, "abc", 0));
    assert(ac_compile(ac));
    
    char buffer[256] = "hello world";
    size_t original_len = strlen(buffer);
    size_t new_len = 0;
    int replacements = ac_replace_inplace(ac, buffer, original_len, sizeof(buffer), &new_len);
    
    printf("  Original: \"hello world\"\n");
    printf("  Result:   \"%.*s\"\n", (int)new_len, buffer);
    printf("  Replacements: %d\n", replacements);
    
    assert(replacements == 0);
    assert(new_len == original_len);
    assert(strncmp(buffer, "hello world", new_len) == 0);
    
    ac_destroy(ac);
    printf("  ✓ Passed\n\n");
}

void test_allocation_version() {
    printf("Test 6: Allocation version...\n");
    
    ac_automaton_t *ac = ac_create(0);
    assert(ac != NULL);
    
    assert(ac_add_pattern(ac, "cat", 0, "dog", 0));
    assert(ac_add_pattern(ac, "mouse", 0, "elephant", 0));
    assert(ac_compile(ac));
    
    const char *text = "The cat chased the mouse";
    size_t result_len = 0;
    char *result = ac_replace_alloc(ac, text, strlen(text), &result_len);
    
    printf("  Original: \"%s\"\n", text);
    printf("  Result:   \"%.*s\"\n", (int)result_len, result);
    
    assert(result != NULL);
    assert(strstr(result, "dog") != NULL);
    assert(strstr(result, "elephant") != NULL);
    
    free(result);
    ac_destroy(ac);
    printf("  ✓ Passed\n\n");
}

void test_stats() {
    printf("Test 7: Statistics...\n");
    
    ac_automaton_t *ac = ac_create(0);
    assert(ac != NULL);
    
    assert(ac_add_pattern(ac, "a", 0, "1", 0));
    assert(ac_add_pattern(ac, "ab", 0, "12", 0));
    assert(ac_add_pattern(ac, "abc", 0, "123", 0));
    assert(ac_compile(ac));
    
    size_t node_count = 0, pattern_count = 0, memory_usage = 0;
    ac_get_stats(ac, &node_count, &pattern_count, &memory_usage);
    
    printf("  Node count: %zu\n", node_count);
    printf("  Pattern count: %zu\n", pattern_count);
    printf("  Memory usage: %zu bytes\n", memory_usage);
    
    assert(pattern_count == 3);
    assert(node_count > 0);
    assert(memory_usage > 0);
    
    ac_destroy(ac);
    printf("  ✓ Passed\n\n");
}

int main() {
    printf("=== Aho-Corasick Algorithm Tests ===\n\n");
    
    test_basic_replacement();
    test_overlapping_patterns();
    test_multiple_occurrences();
    test_length_changes();
    test_no_matches();
    test_allocation_version();
    test_stats();
    
    printf("=== All tests passed! ===\n");
    return 0;
}