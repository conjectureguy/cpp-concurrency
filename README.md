# Lock-Free DS

Small C++ benchmark project currently comparing SPSC queue implementations:

- `lock_free_spsc`
- `optimize_memory_ordering`
- `lock_based_spsc`
- `recursive_mutex_spsc`

Benchmarks cover same-thread push/pop and concurrent producer-consumer transfer.

## Results

From `benchmark_results/with_memory_ordering.csv`, `optimize_memory_ordering` is the best overall implementation.

- Same-thread: ~23.3M items/sec average, about 47% faster than the basic lock-free queue.
- Concurrent transfer: ~16.4M items/sec average, about 82% faster than the basic lock-free queue.
- Mutex-based variants are much slower under concurrent transfer.
