#!/usr/bin/env bash
set -euo pipefail

result_file="${1:-hashmap.csv}"

cmake --preset debug
cmake --build --preset debug --target benchmark_hashmap

./build/benchmark_hashmap "${result_file}"

if python3 -c "import matplotlib, pandas" >/dev/null 2>&1; then
    python3 plot_benchmarks.py "benchmark_results/${result_file}"
else
    echo "Skipping plots: install pandas and matplotlib to enable plot generation."
fi
