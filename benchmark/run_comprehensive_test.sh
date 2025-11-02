#!/bin/bash
# Comprehensive performance test with varying pattern counts

echo "==================================================================="
echo "COMPREHENSIVE PERFORMANCE BENCHMARK"
echo "Comparing mod_replace (Aho-Corasick) vs mod_substitute (Sequential)"
echo "==================================================================="
echo ""

# Generate test data with different pattern counts
for num_patterns in 10 25 50 75 100; do
    echo "Generating test data with $num_patterns patterns..."
    python3 generate_test_data.py $num_patterns 100 0.15
    mv patterns.txt patterns_${num_patterns}.txt
done

echo ""
echo "==================================================================="
echo "RUNNING BENCHMARKS"
echo "==================================================================="

# Test with different pattern counts on 100KB file
for num_patterns in 10 25 50 75 100; do
    echo ""
    echo "-------------------------------------------------------------------"
    echo "TEST: $num_patterns patterns on 100KB content"
    echo "-------------------------------------------------------------------"
    ./performance_benchmark patterns_${num_patterns}.txt 50 test_content_100kb.html
done

# Test 100 patterns on different file sizes
echo ""
echo ""
echo "==================================================================="
echo "TESTING SCALABILITY WITH 100 PATTERNS"
echo "==================================================================="
python3 generate_test_data.py 100 100 0.15
mv patterns.txt patterns_100.txt

for size in 10kb 50kb 100kb; do
    echo ""
    echo "-------------------------------------------------------------------"
    echo "TEST: 100 patterns on ${size} content"
    echo "-------------------------------------------------------------------"
    ./performance_benchmark patterns_100.txt 100 test_content_${size}.html
done

echo ""
echo "==================================================================="
echo "BENCHMARK COMPLETE"
echo "==================================================================="
