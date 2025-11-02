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
 * aho_corasick.h - High-Performance Aho-Corasick String Matching Algorithm
 * 
 * Zero-copy implementation of the Aho-Corasick algorithm for efficient
 * multi-pattern string search and replacement operations.
 */

#ifndef AHO_CORASICK_H
#define AHO_CORASICK_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Aho-Corasick Automaton for zero-copy string replacement
 * 
 * This implementation provides efficient multi-string search and replace
 * operations using the Aho-Corasick algorithm with zero-copy semantics.
 * The original buffer is modified in-place when possible.
 */

#define AC_MAX_ALPHABET_SIZE 256
#define AC_DEFAULT_NODE_CAPACITY 1024

typedef struct ac_node ac_node_t;
typedef struct ac_automaton ac_automaton_t;
typedef struct ac_match ac_match_t;

/**
 * Match structure representing a found pattern
 */
struct ac_match {
    size_t start_pos;       // Start position in the text
    size_t end_pos;         // End position in the text
    const char *pattern;    // Original pattern that matched
    const char *replacement; // Replacement string
    size_t pattern_len;     // Length of the pattern
    size_t replacement_len; // Length of the replacement
};

/**
 * Node in the Aho-Corasick trie
 */
struct ac_node {
    ac_node_t *children[AC_MAX_ALPHABET_SIZE];  // Children nodes
    ac_node_t *failure;                         // Failure link
    ac_node_t *output;                          // Output link for matches

    const char *pattern;                        // Pattern ending at this node (NULL if none)
    const char *replacement;                    // Replacement for the pattern
    size_t pattern_len;                         // Length of pattern
    size_t replacement_len;                     // Length of replacement

    void *user_data;                           // User data associated with pattern (for callbacks)

    bool is_end;                               // True if this node represents end of a pattern
    uint32_t node_id;                          // Unique node identifier
};

/**
 * Aho-Corasick automaton structure
 */
struct ac_automaton {
    ac_node_t *root;                           // Root node of the trie
    ac_node_t *nodes;                          // Pool of nodes
    size_t node_count;                         // Number of nodes used
    size_t node_capacity;                      // Total capacity of node pool
    
    bool is_compiled;                          // True if automaton is compiled (failure links built)
};

/**
 * Callback function type for handling matches
 * 
 * @param match The match information
 * @param user_data User-provided data
 * @return true to continue searching, false to stop
 */
typedef bool (*ac_match_callback_t)(const ac_match_t *match, void *user_data);

/**
 * Initialize a new Aho-Corasick automaton
 * 
 * @param capacity Initial capacity for nodes (0 for default)
 * @return Pointer to initialized automaton, or NULL on failure
 */
ac_automaton_t *ac_create(size_t capacity);

/**
 * Destroy an Aho-Corasick automaton and free all memory
 * 
 * @param ac Pointer to automaton to destroy
 */
void ac_destroy(ac_automaton_t *ac);

/**
 * Add a pattern and its replacement to the automaton
 * 
 * @param ac Pointer to automaton
 * @param pattern Pattern string to search for
 * @param pattern_len Length of pattern (0 for strlen)
 * @param replacement Replacement string
 * @param replacement_len Length of replacement (0 for strlen)
 * @return true on success, false on failure
 */
bool ac_add_pattern(ac_automaton_t *ac, 
                    const char *pattern, size_t pattern_len,
                    const char *replacement, size_t replacement_len);

/**
 * Compile the automaton by building failure links
 * Must be called after adding all patterns and before searching
 * 
 * @param ac Pointer to automaton
 * @return true on success, false on failure
 */
bool ac_compile(ac_automaton_t *ac);

/**
 * Search for patterns in text and call callback for each match
 * 
 * @param ac Compiled automaton
 * @param text Text to search in
 * @param text_len Length of text
 * @param callback Callback function for matches
 * @param user_data User data passed to callback
 * @return Number of matches found, or -1 on error
 */
int ac_search(const ac_automaton_t *ac, 
              const char *text, size_t text_len,
              ac_match_callback_t callback, void *user_data);

/**
 * Perform zero-copy string replacement in-place
 * 
 * This function modifies the input buffer directly when possible.
 * If replacements would make the text longer than the buffer capacity,
 * the function will truncate or return an error.
 * 
 * @param ac Compiled automaton
 * @param buffer Buffer containing text to process (modified in-place)
 * @param buffer_len Current length of text in buffer
 * @param buffer_capacity Total capacity of buffer
 * @param new_len Pointer to store new length after replacements
 * @return Number of replacements made, or -1 on error
 */
int ac_replace_inplace(const ac_automaton_t *ac,
                       char *buffer, size_t buffer_len, size_t buffer_capacity,
                       size_t *new_len);

/**
 * Perform string replacement with memory allocation
 * 
 * This function allocates a new buffer for the result.
 * The caller is responsible for freeing the returned buffer.
 * 
 * @param ac Compiled automaton
 * @param text Text to process
 * @param text_len Length of input text
 * @param result_len Pointer to store length of result
 * @return Newly allocated buffer with replacements, or NULL on error
 */
char *ac_replace_alloc(const ac_automaton_t *ac,
                       const char *text, size_t text_len,
                       size_t *result_len);

/**
 * Callback function type for dynamic replacement generation
 *
 * This callback is called for each match found to generate the replacement string.
 * This allows variable expansion without recompiling the automaton.
 *
 * @param pattern The matched pattern string
 * @param pattern_len Length of the pattern
 * @param user_data User data from the pattern (set via ac_add_pattern_ex)
 * @param context_data User context data passed to ac_replace_with_callback
 * @param replacement_len Output: length of the returned replacement string
 * @return Replacement string (must remain valid until next callback call)
 */
typedef const char* (*ac_replacement_callback_t)(
    const char *pattern,
    size_t pattern_len,
    void *user_data,
    void *context_data,
    size_t *replacement_len
);

/**
 * Add a pattern with user data to the automaton
 *
 * Extended version of ac_add_pattern that allows associating user data with the pattern.
 * This user data will be passed to the replacement callback.
 *
 * @param ac Pointer to automaton
 * @param pattern Pattern string to search for
 * @param pattern_len Length of pattern (0 for strlen)
 * @param replacement Replacement string (can be NULL if using callback)
 * @param replacement_len Length of replacement (0 for strlen)
 * @param user_data User data to associate with this pattern
 * @return true on success, false on failure
 */
bool ac_add_pattern_ex(ac_automaton_t *ac,
                       const char *pattern, size_t pattern_len,
                       const char *replacement, size_t replacement_len,
                       void *user_data);

/**
 * Perform string replacement with dynamic callback
 *
 * This function uses a precompiled automaton but allows dynamic replacement
 * generation via a callback. This is ideal for variable expansion without
 * recompiling the automaton.
 *
 * @param ac Compiled automaton
 * @param text Text to process
 * @param text_len Length of input text
 * @param callback Callback to generate replacement strings
 * @param context_data User context passed to callback
 * @param result_len Pointer to store length of result
 * @return Newly allocated buffer with replacements, or NULL on error
 */
char *ac_replace_with_callback(const ac_automaton_t *ac,
                               const char *text, size_t text_len,
                               ac_replacement_callback_t callback,
                               void *context_data,
                               size_t *result_len);

/**
 * Get statistics about the automaton
 * 
 * @param ac Automaton
 * @param node_count Pointer to store number of nodes (can be NULL)
 * @param pattern_count Pointer to store number of patterns (can be NULL)
 * @param memory_usage Pointer to store estimated memory usage (can be NULL)
 */
void ac_get_stats(const ac_automaton_t *ac,
                  size_t *node_count, size_t *pattern_count, size_t *memory_usage);

/**
 * Reset automaton to empty state (removes all patterns)
 * 
 * @param ac Automaton to reset
 */
void ac_reset(ac_automaton_t *ac);

#ifdef __cplusplus
}
#endif

#endif // AHO_CORASICK_H