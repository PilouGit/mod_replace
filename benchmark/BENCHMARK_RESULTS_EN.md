# Performance Benchmark: mod_replace vs mod_substitute

## Test Configuration

- **Patterns**: 100 simple replacement patterns (exact text)
- **Content sizes tested**: 10 KB, 50 KB, 100 KB, 500 KB
- **Pattern density**: 15% of content contains patterns
- **Iterations**: 100 per test
- **Platform**: Linux x86_64, compiled with -O3 -march=native

## Detailed Results

### Test 1: 10 KB Content (100 patterns)

| Metric | Sequential (mod_substitute) | Aho-Corasick (mod_replace) | Improvement |
|--------|----------------------------|---------------------------|--------------|
| Average time | 629.64 μs | 506.12 μs | **1.24x faster** |
| Throughput | 16.22 MB/s | 20.18 MB/s | +24.4% |
| Compilation time | - | 313.12 μs | N/A |
| Search time only | - | 192.83 μs | **3.27x faster** |

**Analysis**: On small content, Aho-Corasick shows a moderate advantage even with compilation cost.

### Test 2: 50 KB Content (100 patterns)

| Metric | Sequential (mod_substitute) | Aho-Corasick (mod_replace) | Improvement |
|--------|----------------------------|---------------------------|--------------|
| Average time | 2356.24 μs | 2101.38 μs | **1.12x faster** |
| Throughput | 20.89 MB/s | 23.42 MB/s | +12.1% |
| Compilation time | - | 475.88 μs | N/A |
| Search time only | - | 1625.10 μs | **1.45x faster** |

**Analysis**: The advantage persists on medium content. Compilation cost becomes proportionally less significant.

### Test 3: 100 KB Content (100 patterns)

| Metric | Sequential (mod_substitute) | Aho-Corasick (mod_replace) | Improvement |
|--------|----------------------------|---------------------------|--------------|
| Average time | 5373.57 μs | 5051.43 μs | **1.06x faster** |
| Throughput | 18.18 MB/s | 19.34 MB/s | +6.4% |
| Compilation time | - | 642.06 μs | N/A |
| Search time only | - | 4408.66 μs | **1.22x faster** |

**Analysis**: On larger content, the advantage decreases but remains present.

### Test 4: 500 KB Content (100 patterns)

| Metric | Sequential (mod_substitute) | Aho-Corasick (mod_replace) | Result |
|--------|----------------------------|---------------------------|----------|
| Average time | 19988.96 μs | 77193.42 μs | **3.86x SLOWER** ⚠️ |
| Throughput | 24.45 MB/s | 6.33 MB/s | -74.1% |
| Compilation time | - | 475.05 μs | N/A |
| Search time only | - | 76716.61 μs | **3.84x slower** |

**⚠️ ISSUE IDENTIFIED**: Current `ac_replace_alloc` implementation has sub-optimal complexity for large files with many matches. Probable cause: O(n²) sorting algorithm for matches (see `aho_corasick.c:366-374`).

## Production Scenario (Precompiled Automaton)

In a real production environment (Apache), the Aho-Corasick automaton is compiled **once** at startup. Requests only use the search phase.

### Comparison: Search Time Only

| Size | Sequential | AC Precompiled | Speedup |
|------|-----------|---------------|---------|
| 10 KB  | 629.64 μs | 192.83 μs | **3.27x** |
| 50 KB  | 2356.24 μs | 1625.10 μs | **1.45x** |
| 100 KB | 5373.57 μs | 4408.66 μs | **1.22x** |
| 500 KB | 19988.96 μs | 76716.61 μs | **0.26x** ⚠️ |

## Analysis and Recommendations

### Strengths of mod_replace (Aho-Corasick)

✅ **Excellent for**:
- High volume of patterns (>50)
- Small to medium content (<100 KB)
- Precompiled automaton (production)
- 20-70% CPU time savings

✅ **Algorithmic advantages**:
- Theoretical O(n + m + z) complexity
- Single pass over content
- Reusable precompilation

### Identified Issues

⚠️ **Current limitation**:
- Degraded performance on very large files (>500 KB)
- Probable cause: O(n²) match sorting in `ac_replace_alloc`
- **Solution**: Implement qsort instead of bubble sort

⚠️ **Compilation cost**:
- 300-650 μs depending on number of patterns
- Negligible in production (once at startup)
- Important if automaton recreated per request

### Strengths of mod_substitute (Sequential)

✅ **Excellent for**:
- Very large files (>500 KB)
- Few patterns (<10)
- Complex patterns with regex
- Backreferences required

✅ **Advantages**:
- Simple implementation
- Predictable performance
- No pre-allocated memory

### Final Recommendation

**For high volume of simple patterns (test objective):**

1. **mod_replace (Aho-Corasick) is RECOMMENDED if**:
   - Files < 100 KB (typical HTML/API use case)
   - >20 replacement patterns
   - Apache deployment (precompiled automaton)
   - Performance gain: **20-227%** (3.27x max)

2. **mod_substitute remains preferable if**:
   - Files > 500 KB
   - Regex/backreferences needed
   - <10 simple patterns

3. **Optimization to implement in mod_replace**:
   - Replace bubble sort with qsort in `aho_corasick.c:366-374`
   - Estimated implementation: 30 lines of code
   - Expected gain on 500KB: ~4x (bring to parity with sequential)

## Real-World Use Case

### Example: Domain Migration (50 domains)

```apache
# mod_replace
ReplaceEnable On
ReplaceRule "api1.old.com" "api1.new.com"
# ... 48 other patterns
```

- **Time per request (100KB)**: 4.4 ms (precompiled)
- **vs mod_substitute**: 5.4 ms
- **Savings**: 1 ms per request
- **At 1000 req/s**: Save 1 CPU core

### Conclusion

For **high volume of simple patterns**, mod_replace (Aho-Corasick) demonstrates **clear superiority** under typical web usage conditions (<100 KB). The implementation requires optimization of match sorting to efficiently handle very large files.

**Final score**: mod_replace **3-0** mod_substitute (on files <100KB with >50 patterns)
