# C++ Concurrency

Small C++ benchmark project currently comparing SPSC queue implementations:

- `lock_free_spsc`
- `optimize_memory_ordering`
- `remove_false_sharing`
- `lock_based_spsc`
- `recursive_mutex_spsc`

## Layout

- `include/cpp_concurrency/queues/spsc/`: SPSC queue implementations.
- `include/cpp_concurrency/hashmaps/`: hashmap implementations.
- `include/cpp_concurrency/spsc_variants.h`: registry used by SPSC tests and benchmarks.
- `tests/`: GoogleTest coverage for data structures.
- `benchmarks/`: standalone benchmark executables.

Benchmarks cover same-thread push/pop and concurrent producer-consumer transfer.

## Results

From `benchmark_results/removed_false_sharing.csv`, `remove_false_sharing` is the best overall implementation.

- Same-thread: ~24.8M items/sec average, about 24% faster than the basic lock-free queue and about 7% faster than `optimize_memory_ordering`.
- Concurrent transfer: ~18.3M items/sec average, about 77% faster than the basic lock-free queue and about 67% faster than `optimize_memory_ordering`.
- Mutex-based variants are much slower under concurrent transfer.
- All benchmark rows passed validation.

Average throughput by implementation:

| Implementation | Same-thread push/pop | Concurrent transfer | Overall |
| --- | ---: | ---: | ---: |
| `remove_false_sharing` | 24.8M items/sec | 18.3M items/sec | 21.6M items/sec |
| `optimize_memory_ordering` | 23.3M items/sec | 11.0M items/sec | 17.1M items/sec |
| `lock_free_spsc` | 20.0M items/sec | 10.3M items/sec | 15.2M items/sec |
| `lock_based_spsc` | 15.1M items/sec | 4.4M items/sec | 9.7M items/sec |
| `recursive_mutex_spsc` | 13.3M items/sec | 3.7M items/sec | 8.5M items/sec |
