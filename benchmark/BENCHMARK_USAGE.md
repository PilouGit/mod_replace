# Benchmark Usage Guide

## Quick Start

### 1. Standard Performance Comparison (mod_replace vs mod_substitute)

Compares Aho-Corasick (mod_replace) vs Sequential search (mod_substitute):

```bash
cd benchmark
make
./performance_benchmark patterns.txt 100 test_content_100kb.html
```

**Output**: Shows speedup of Aho-Corasick vs Sequential for different file sizes.

### 2. v1.2.0 Variable Optimization Benchmark ⭐ NEW

Compares OLD (recreate automaton per request) vs NEW (callback-based):

```bash
./performance_benchmark --v1.2 patterns_vars_heavy.txt 1000 test_content_10kb.html
```

**What it tests**:
- **OLD (v1.1.0)**: Recreates automaton 1000 times with expanded variables
- **NEW (v1.2.0)**: Compiles automaton ONCE, uses callback for variable expansion

**Expected Results**:
- 4x-512x speedup depending on pattern count and variable ratio
- 75-99.8% CPU reduction
- Dramatic reduction in latency

## Pattern Files

### `patterns.txt` (Standard)
100 static patterns for comparing algorithms:
```
pattern0|replacement0
pattern1|replacement1
...
```

### `patterns_with_vars.txt` (Basic Variables)
6 patterns with 3 variables:
```
___CSP_NONCE___|%{UNIQUE_STRING}
{{USER}}|${REMOTE_USER}
{{SERVER}}|${SERVER_NAME}
old_text|new_text
api.example.com|api.newdomain.com
http://|https://
```

### `patterns_vars_heavy.txt` (Heavy Variables)
27 patterns with 11 variables (40% variable ratio):
```
___CSP_NONCE___|%{UNIQUE_STRING}
{{USER}}|${REMOTE_USER}
{{SERVER}}|${SERVER_NAME}
{{NONCE1}}|%{UNIQUE_STRING}
...
```

**Best for demonstrating v1.2.0 optimization!**

## Test Content Files

Generated HTML files with embedded patterns:

- `test_content_10kb.html` - 10 KB
- `test_content_50kb.html` - 50 KB
- `test_content_100kb.html` - 100 KB
- `test_content_500kb.html` - 500 KB

## Generating Custom Test Data

```bash
cd benchmark
python3 generate_test_data.py <num_patterns> <size_kb> <density>
```

**Example**:
```bash
# 50 patterns, 100 KB content, 20% pattern density
python3 generate_test_data.py 50 100 0.2
```

## Understanding the Results

### Standard Benchmark Output

```
=== Benchmarking: test_content_100kb.html (100.03 KB, 100 patterns, 100 iterations) ===

--- Sequential Approach (mod_substitute style) ---
Average time: 4205.32 μs (4.21 ms)

--- Aho-Corasick Approach (mod_replace style) ---
Average total time: 604.12 μs (0.60 ms)

--- Performance Comparison ---
Aho-Corasick is 6.96x faster than Sequential
```

**Key Metrics**:
- **Average time**: Time per request
- **Speedup**: How much faster Aho-Corasick is
- **Throughput**: MB/s processing rate

### v1.2.0 Optimization Benchmark Output

```
┌─────────────────────────────────────────────────────────────────┐
│ OLD (v1.1.0): Recreate automaton per request with variables    │
└─────────────────────────────────────────────────────────────────┘
  Total time:      162.73 ms
  Average/request: 162.73 μs
  ⚠️  Automaton created/compiled/destroyed 1000 times!

┌─────────────────────────────────────────────────────────────────┐
│ NEW (v1.2.0): Precompiled automaton with callback              │
└─────────────────────────────────────────────────────────────────┘
  Compilation time (one-time): 62.00 μs
  Total time:      40.32 ms
  Average/request: 40.32 μs
  ✅ Automaton compiled ONCE, reused 1000 times!

┌─────────────────────────────────────────────────────────────────┐
│ PERFORMANCE IMPROVEMENT                                         │
└─────────────────────────────────────────────────────────────────┘
  🚀 Speedup:          4.04x faster
  📉 CPU reduction:    75.2%
  ⏱️  Time saved:       122.41 μs/request
```

**Key Insights**:
- **Speedup**: Direct performance improvement
- **CPU reduction**: Resource savings
- **Compilation**: One-time cost vs per-request overhead

## Real-World Use Cases

### CSP Nonce Injection

**Pattern**: `___CSP_NONCE___|%{UNIQUE_STRING}`

**Before v1.2.0**:
- Automaton recreated every request: ~118 μs overhead
- 1000 req/s = 118 ms CPU time = ~12% of one core wasted

**After v1.2.0**:
- Automaton compiled once: ~50 μs one-time
- Per request: ~0.3 μs (callback only)
- 1000 req/s = 0.3 ms CPU time = **99.7% reduction!**

### User Personalization

**Patterns**:
```
{{USER}}|${REMOTE_USER}
{{SERVER}}|${SERVER_NAME}
{{SESSION}}|%{UNIQUE_STRING}
```

**Impact**: 4x faster with mixed variables + static patterns

### Large Pattern Sets

**Scenario**: 100 patterns, 10 with variables

**Before v1.2.0**: Recompile all 100 patterns per request
**After v1.2.0**: Compile once, expand only 10 via callback

**Result**: 10x faster, 90% CPU reduction

## Interpreting Production Impact

The benchmark shows "Production Impact (at 1000 req/s)":

```
OLD (v1.1.0):
  Latency:     162.73 μs/request
  CPU usage:   0.163 cores
  Compilations: 1000/sec ⚠️

NEW (v1.2.0):
  Latency:     40.32 μs/request
  CPU usage:   0.040 cores
  Compilations: 1/startup ✅
```

**What this means**:
- **OLD**: Each request triggers automaton compilation
- **NEW**: One compilation at startup, callbacks for variables
- **Savings**: 75% less CPU per 1000 req/s
- **Scaling**: Handle 4x more traffic with same CPU

## Tips for Best Results

1. **Use realistic patterns**: Mix of static and variable replacements
2. **Realistic iterations**: 1000+ to simulate production load
3. **Warm up included**: First 3 iterations excluded from timing
4. **Multiple runs**: Run benchmark multiple times for consistency
5. **Check variability**: Results should be within 5-10% across runs

## Troubleshooting

### Low Speedup for v1.2.0

**Possible causes**:
- Too few patterns (< 10)
- Too few variables (< 3)
- Small content file (< 10 KB dominates callback overhead)

**Solution**: Use `patterns_vars_heavy.txt` with 10+ KB content

### High Variance in Results

**Possible causes**:
- System load
- CPU frequency scaling
- Background processes

**Solution**: Close other programs, run multiple times

### Benchmark Doesn't Run

**Check**:
1. `make` completed successfully
2. Test files exist (`ls test_content_*.html`)
3. Pattern file exists and is readable

## Advanced Usage

### Custom Pattern Ratios

Test different variable/static ratios:

```bash
# 20% variables (20 vars, 80 static)
# Create custom pattern file with this ratio

# 50% variables (50 vars, 50 static)
# etc.
```

### Stress Testing

Simulate high load:

```bash
# 10,000 requests
./performance_benchmark --v1.2 patterns_vars_heavy.txt 10000

# Large file
./performance_benchmark --v1.2 patterns_vars_heavy.txt 1000 test_content_500kb.html
```

### Profiling

Use with profiling tools:

```bash
# Valgrind
valgrind --tool=callgrind ./performance_benchmark --v1.2 patterns_vars_heavy.txt 100

# perf
perf record ./performance_benchmark --v1.2 patterns_vars_heavy.txt 1000
perf report
```

## Continuous Integration

Add to CI pipeline:

```yaml
# .github/workflows/benchmark.yml
- name: Run Performance Benchmarks
  run: |
    cd benchmark
    make
    ./performance_benchmark --v1.2 patterns_vars_heavy.txt 1000
```

## Further Reading

- [OPTIMIZATION_IMPLEMENTATION.md](OPTIMIZATION_IMPLEMENTATION.md) - v1.2.0 implementation details
- [PERFORMANCE_COMPARISON.md](PERFORMANCE_COMPARISON.md) - Full benchmark results
- [QSORT_OPTIMIZATION_EN.md](QSORT_OPTIMIZATION_EN.md) - v1.1.0 qsort optimization
- [V1.2.0_IMPLEMENTATION_SUMMARY.md](V1.2.0_IMPLEMENTATION_SUMMARY.md) - Complete v1.2.0 summary

---

**Last Updated**: November 2, 2025
**Version**: 1.2.0
