//
// Created by rahul on 7/17/26.
//

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "cpp_concurrency/hashmap_variants.h"

namespace {

struct BenchmarkResult {
    std::string name;
    std::size_t capacity;
    std::uint64_t item_count;
    std::uint32_t thread_count;
    double elapsed_seconds;
    double million_ops_per_second;
    double nanoseconds_per_op;
    std::uint64_t p50_ns;
    std::uint64_t p95_ns;
    std::uint64_t p99_ns;
    bool validation_passed;
};

std::string format_human_readable(const BenchmarkResult& result);

class ResultWriter {
public:
    explicit ResultWriter(const std::filesystem::path& filename)
        : file_(filename) {
        if (!file_) {
            throw std::runtime_error("failed to open result file: " + filename.string());
        }
    }

    void write_line(const std::string& line) {
        std::cout << line << '\n';
    }

    void write_csv_header() {
        file_ << "benchmark,capacity,items,threads,seconds,mitems_per_second,ns_per_item,"
              << "p50_ns,p95_ns,p99_ns,validation_passed\n";
    }

    void write_result(const BenchmarkResult& result) {
        write_line(format_human_readable(result));
        file_ << std::setprecision(12);
        file_ << result.name << ','
              << result.capacity << ','
              << result.item_count << ','
              << result.thread_count << ','
              << result.elapsed_seconds << ','
              << result.million_ops_per_second << ','
              << result.nanoseconds_per_op << ','
              << result.p50_ns << ','
              << result.p95_ns << ','
              << result.p99_ns << ','
              << (result.validation_passed ? 1 : 0) << '\n';
    }

private:
    std::ofstream file_;
};

std::uint64_t now_ns() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

template <typename Function>
double measure_seconds(Function&& function) {
    const auto start = std::chrono::steady_clock::now();
    function();
    const auto end = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(end - start).count();
}

std::uint64_t percentile_ns(const std::vector<std::uint64_t>& sorted_samples, std::uint64_t percentile) {
    if (sorted_samples.empty()) {
        return 0;
    }

    const auto rank = ((sorted_samples.size() * percentile) + 99) / 100;
    const auto index = std::min<std::size_t>(rank - 1, sorted_samples.size() - 1);
    return sorted_samples[index];
}

BenchmarkResult make_result(const std::string& name,
                            std::size_t bucket_count,
                            std::uint64_t operation_count,
                            std::uint32_t thread_count,
                            double elapsed_seconds,
                            std::vector<std::uint64_t> latency_samples,
                            bool validation_passed) {
    const auto seconds = elapsed_seconds > 0.0 ? elapsed_seconds : 1e-9;
    const auto ops_per_second = static_cast<double>(operation_count) / seconds;

    std::sort(latency_samples.begin(), latency_samples.end());

    return BenchmarkResult{
        name,
        bucket_count,
        operation_count,
        thread_count,
        elapsed_seconds,
        ops_per_second / 1'000'000.0,
        (seconds * 1'000'000'000.0) / static_cast<double>(operation_count),
        percentile_ns(latency_samples, 50),
        percentile_ns(latency_samples, 95),
        percentile_ns(latency_samples, 99),
        validation_passed,
    };
}

std::string format_human_readable(const BenchmarkResult& result) {
    return result.name + " | buckets=" + std::to_string(result.capacity)
           + " | ops=" + std::to_string(result.item_count)
           + " | threads=" + std::to_string(result.thread_count)
           + " | seconds=" + std::to_string(result.elapsed_seconds)
           + " | Mops/s=" + std::to_string(result.million_ops_per_second)
           + " | ns/op=" + std::to_string(result.nanoseconds_per_op)
           + " | p50_ns=" + std::to_string(result.p50_ns)
           + " | p95_ns=" + std::to_string(result.p95_ns)
           + " | p99_ns=" + std::to_string(result.p99_ns)
           + " | validation_passed=" + (result.validation_passed ? "1" : "0");
}

template <typename Variant>
BenchmarkResult run_same_thread_insert_get_remove(std::size_t bucket_count, std::uint64_t item_count) {
    typename Variant::template hashmap<int, int, std::hash<int>> map(bucket_count);
    bool valid = true;
    std::vector<std::uint64_t> latency_samples;
    latency_samples.reserve(static_cast<std::size_t>(item_count) * 3);

    const double elapsed_seconds = measure_seconds([&]() {
        for (std::uint64_t i = 0; i < item_count; ++i) {
            const int key = static_cast<int>(i);
            const int value = key * 2;

            auto start_ns = now_ns();
            map.add_or_update_value(key, value);
            latency_samples.push_back(now_ns() - start_ns);

            start_ns = now_ns();
            if (map.get_value(key, -1) != value) {
                valid = false;
                break;
            }
            latency_samples.push_back(now_ns() - start_ns);

            start_ns = now_ns();
            map.remove_key(key);
            latency_samples.push_back(now_ns() - start_ns);

            if (map.get_value(key, -1) != -1) {
                valid = false;
                break;
            }
        }
    });

    if (!valid) {
        throw std::runtime_error("same-thread hashmap benchmark validation failed");
    }

    return make_result(std::string(Variant::name) + "/same_thread_insert_get_remove",
                       bucket_count,
                       item_count * 3,
                       1,
                       elapsed_seconds,
                       std::move(latency_samples),
                       valid);
}

template <typename Variant>
BenchmarkResult run_concurrent_distinct_writes(std::size_t bucket_count,
                                               std::uint64_t item_count,
                                               std::uint32_t thread_count) {
    typename Variant::template hashmap<int, int, std::hash<int>> map(bucket_count);
    std::atomic<bool> valid{true};
    std::vector<std::uint64_t> latency_samples;
    latency_samples.reserve(static_cast<std::size_t>(item_count));

    const auto per_thread = item_count / thread_count;

    const double elapsed_seconds = measure_seconds([&]() {
        std::vector<std::thread> threads;
        threads.reserve(thread_count);
        std::vector<std::vector<std::uint64_t>> thread_samples(thread_count);

        for (std::uint32_t thread_id = 0; thread_id < thread_count; ++thread_id) {
            thread_samples[thread_id].reserve(static_cast<std::size_t>(per_thread));
            threads.emplace_back([&, thread_id]() {
                const auto begin = static_cast<std::uint64_t>(thread_id) * per_thread;
                const auto end = thread_id == thread_count - 1 ? item_count : begin + per_thread;

                for (std::uint64_t i = begin; i < end; ++i) {
                    const int key = static_cast<int>(i);
                    const auto start_ns = now_ns();
                    map.add_or_update_value(key, key * 2);
                    thread_samples[thread_id].push_back(now_ns() - start_ns);
                }
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }

        for (auto& samples : thread_samples) {
            latency_samples.insert(latency_samples.end(), samples.begin(), samples.end());
        }
    });

    for (std::uint64_t i = 0; i < item_count; ++i) {
        const int key = static_cast<int>(i);
        if (map.get_value(key, -1) != key * 2) {
            valid.store(false, std::memory_order_relaxed);
            break;
        }
    }

    if (!valid.load(std::memory_order_relaxed)) {
        throw std::runtime_error("concurrent distinct-write hashmap benchmark validation failed");
    }

    return make_result(std::string(Variant::name) + "/concurrent_distinct_writes",
                       bucket_count,
                       item_count,
                       thread_count,
                       elapsed_seconds,
                       std::move(latency_samples),
                       valid.load(std::memory_order_relaxed));
}

template <typename Variant>
BenchmarkResult run_concurrent_shared_updates(std::size_t bucket_count,
                                              std::uint64_t item_count,
                                              std::uint32_t thread_count) {
    typename Variant::template hashmap<int, int, std::hash<int>> map(bucket_count);
    std::atomic<bool> valid{true};
    std::vector<std::uint64_t> latency_samples;
    latency_samples.reserve(static_cast<std::size_t>(item_count));

    constexpr int hot_key_count = 64;
    const auto per_thread = item_count / thread_count;

    const double elapsed_seconds = measure_seconds([&]() {
        std::vector<std::thread> threads;
        threads.reserve(thread_count);
        std::vector<std::vector<std::uint64_t>> thread_samples(thread_count);

        for (std::uint32_t thread_id = 0; thread_id < thread_count; ++thread_id) {
            thread_samples[thread_id].reserve(static_cast<std::size_t>(per_thread));
            threads.emplace_back([&, thread_id]() {
                const auto begin = static_cast<std::uint64_t>(thread_id) * per_thread;
                const auto end = thread_id == thread_count - 1 ? item_count : begin + per_thread;

                for (std::uint64_t i = begin; i < end; ++i) {
                    const int key = static_cast<int>(i % hot_key_count);
                    const auto start_ns = now_ns();
                    map.add_or_update_value(key, static_cast<int>(i));
                    thread_samples[thread_id].push_back(now_ns() - start_ns);
                }
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }

        for (auto& samples : thread_samples) {
            latency_samples.insert(latency_samples.end(), samples.begin(), samples.end());
        }
    });

    for (int key = 0; key < hot_key_count; ++key) {
        if (map.get_value(key, -1) == -1) {
            valid.store(false, std::memory_order_relaxed);
            break;
        }
    }

    if (!valid.load(std::memory_order_relaxed)) {
        throw std::runtime_error("concurrent shared-update hashmap benchmark validation failed");
    }

    return make_result(std::string(Variant::name) + "/concurrent_shared_updates",
                       bucket_count,
                       item_count,
                       thread_count,
                       elapsed_seconds,
                       std::move(latency_samples),
                       valid.load(std::memory_order_relaxed));
}

template <typename Variant>
void run_variant_benchmarks(ResultWriter& writer,
                            const std::vector<std::size_t>& bucket_counts,
                            const std::vector<std::uint64_t>& item_counts,
                            std::uint32_t thread_count) {
    for (const auto bucket_count : bucket_counts) {
        for (const auto item_count : item_counts) {
            writer.write_result(run_same_thread_insert_get_remove<Variant>(bucket_count, item_count));
            writer.write_result(run_concurrent_distinct_writes<Variant>(bucket_count, item_count, thread_count));
            writer.write_result(run_concurrent_shared_updates<Variant>(bucket_count, item_count, thread_count));
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <result-filename>\n";
        return 1;
    }

    try {
        const auto result_directory = std::filesystem::path("benchmark_results");
        std::filesystem::create_directories(result_directory);

        const auto result_file = result_directory / std::filesystem::path(argv[1]).filename();
        ResultWriter writer(result_file);
        const std::vector<std::size_t> bucket_counts{64, 4096};
        const std::vector<std::uint64_t> item_counts{10'000, 50'000};
        const std::uint32_t thread_count = std::min(4u, std::max(2u, std::thread::hardware_concurrency()));

        writer.write_line("Hashmap benchmark results");
        writer.write_line("result_file=" + result_file.string());
        writer.write_csv_header();
        for_each_registered_hashmap_variant([&]<typename Variant>() {
            run_variant_benchmarks<Variant>(writer, bucket_counts, item_counts, thread_count);
        });
    } catch (const std::exception& error) {
        std::cerr << "benchmark failed: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
