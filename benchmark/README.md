# mod_replace Performance Benchmarks

This directory contains comprehensive performance benchmarks comparing mod_replace (Aho-Corasick algorithm) with mod_substitute (sequential search).

## Quick Summary

**mod_replace with qsort optimization is 4-7x faster than mod_substitute for high pattern volumes.**

| File Size | mod_substitute | mod_replace | Speedup |
|-----------|----------------|-------------|---------|
| 10 KB     | 405 μs         | 106 μs      | **3.82x** |
| 50 KB     | 1795 μs        | 362 μs      | **4.96x** |
| 100 KB    | 4205 μs        | 604 μs      | **6.96x** |
| 500 KB    | 18712 μs       | 3534 μs     | **5.29x** |

## Benchmark Reports

### 📊 Main Reports (English)

- **[PERFORMANCE_COMPARISON.md](PERFORMANCE_COMPARISON.md)** - Complete performance comparison with recommendations
- **[QSORT_OPTIMIZATION_EN.md](QSORT_OPTIMIZATION_EN.md)** - qsort optimization impact and analysis
- **[BENCHMARK_RESULTS_EN.md](BENCHMARK_RESULTS_EN.md)** - Initial benchmark results (before qsort)

### 📊 Rapports (Français)

- **[FINAL_BENCHMARK_RESULTS.md](FINAL_BENCHMARK_RESULTS.md)** - Résultats finaux complets
- **[QSORT_OPTIMIZATION_RESULTS.md](QSORT_OPTIMIZATION_RESULTS.md)** - Impact de l'optimisation qsort
- **[BENCHMARK_RESULTS.md](BENCHMARK_RESULTS.md)** - Résultats initiaux (avant qsort)
- **[ALGORITHM_COMPARISON.md](ALGORITHM_COMPARISON.md)** - Comparaison algorithmique détaillée

## Running Benchmarks

### Prerequisites

```bash
cd benchmark
make
```

### Run Tests

```bash
# Quick test (10 iterations on 10KB file)
make run-quick

# Full benchmark (100 iterations on all sizes)
make run

# Custom test
./performance_benchmark patterns.txt <iterations> <content_files...>
```

### Generate Test Data

```bash
# Generate patterns and test content
python3 generate_test_data.py <num_patterns> <content_size_kb> <pattern_density>

# Example: 100 patterns, 100KB content, 15% density
python3 generate_test_data.py 100 100 0.15
```

## Key Findings

### qsort Optimization (Version 1.1.0)

The critical optimization replacing O(n²) bubble sort with O(n log n) qsort resulted in:

- **21.7x speedup** on 500KB files
- **95% reduction** in match sorting time
- Production-ready performance for all file sizes

**Before qsort**: 500KB file took 76717 μs (3.84x SLOWER than sequential)
**After qsort**: 500KB file takes 3534 μs (5.29x FASTER than sequential)

### Algorithm Comparison

**mod_replace (Aho-Corasick)**:
- Complexity: O(n + m + z log z)
- Single pass over content
- Precompiled automaton
- Excellent for high pattern volumes (>10 patterns)

**mod_substitute (Sequential)**:
- Complexity: O(p × n)
- Multiple passes (one per pattern)
- Simple implementation
- Better for complex regex with backreferences

## Production Use Cases

### Domain Migration Example

Migrating 100 domain patterns on 100KB pages at 1000 req/s:

- **CPU saved**: ~3.6 cores
- **Latency reduction**: -85% (P95)
- **Throughput gain**: +464%

### Recommendations

✅ **Use mod_replace** when:
- High volume of patterns (>10)
- Simple text replacements
- Maximum performance required
- Any file size (10KB - 500KB+)

⚠️ **Use mod_substitute** when:
- Complex regex with backreferences
- Very few patterns (<5)
- Dynamic pattern expressions

## Test Configuration

- **Platform**: Linux x86_64
- **Compiler**: GCC with -O3 -march=native
- **Patterns**: 100 simple text patterns
- **Content sizes**: 10 KB, 50 KB, 100 KB, 500 KB
- **Pattern density**: 15% of content
- **Iterations**: 100 per test

## Files in This Directory

### Benchmark Programs
- `performance_benchmark.c` - Main benchmark program
- `generate_test_data.py` - Test data generator
- `Makefile` - Build configuration
- `run_comprehensive_test.sh` - Comprehensive test script

### Test Data (Generated)
- `patterns.txt` - Test patterns
- `test_content_*.html` - Test content files

### Documentation
- `README.md` - This file
- Various benchmark result files (MD format)

## Contributing

To add new benchmarks:

1. Add test cases to `performance_benchmark.c`
2. Generate appropriate test data
3. Run benchmarks and document results
4. Update this README with findings

## License

Same as mod_replace - Apache License 2.0

---

**Last updated**: November 2, 2025
**Benchmark version**: 1.1.0 (qsort optimized)
