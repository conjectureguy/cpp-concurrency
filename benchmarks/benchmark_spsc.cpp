//
// Created by rahul on 7/4/26.
//

#include <algorithm>
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

#include "cpp_concurrency/spsc_variants.h"

namespace {

struct BenchmarkResult {
    std::string name;
    std::size_t capacity;
    std::uint64_t item_count;
    double elapsed_seconds;
    double million_items_per_second;
    double nanoseconds_per_item;
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
        file_ << "benchmark,capacity,items,seconds,mitems_per_second,ns_per_item,"
              << "p50_ns,p95_ns,p99_ns,validation_passed\n";
    }

    void write_result(const BenchmarkResult& result) {
        write_line(format_human_readable(result));
        file_ << std::setprecision(12);
        file_ << result.name << ','
              << result.capacity << ','
              << result.item_count << ','
              << result.elapsed_seconds << ','
              << result.million_items_per_second << ','
              << result.nanoseconds_per_item << ','
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
                            std::size_t capacity,
                            std::uint64_t item_count,
                            double elapsed_seconds,
                            std::vector<std::uint64_t> latency_samples,
                            bool validation_passed) {
    const auto seconds = elapsed_seconds > 0.0 ? elapsed_seconds : 1e-9;
    const auto items_per_second = static_cast<double>(item_count) / seconds;

    std::sort(latency_samples.begin(), latency_samples.end());

    return BenchmarkResult{
        name,
        capacity,
        item_count,
        elapsed_seconds,
        items_per_second / 1'000'000.0,
        (seconds * 1'000'000'000.0) / static_cast<double>(item_count),
        percentile_ns(latency_samples, 50),
        percentile_ns(latency_samples, 95),
        percentile_ns(latency_samples, 99),
        validation_passed,
    };
}

std::string format_human_readable(const BenchmarkResult& result) {
    return result.name + " | capacity=" + std::to_string(result.capacity)
           + " | items=" + std::to_string(result.item_count)
           + " | seconds=" + std::to_string(result.elapsed_seconds)
           + " | Mitems/s=" + std::to_string(result.million_items_per_second)
           + " | ns/item=" + std::to_string(result.nanoseconds_per_item)
           + " | p50_ns=" + std::to_string(result.p50_ns)
           + " | p95_ns=" + std::to_string(result.p95_ns)
           + " | p99_ns=" + std::to_string(result.p99_ns)
           + " | validation_passed=" + (result.validation_passed ? "1" : "0");
}

template <typename Variant, std::size_t Capacity>
BenchmarkResult run_same_thread_push_pop(std::uint64_t item_count) {
    typename Variant::template queue<int, Capacity> queue;
    bool valid = true;
    std::vector<std::uint64_t> latency_samples;
    latency_samples.reserve(static_cast<std::size_t>(item_count));

    const double elapsed_seconds = measure_seconds([&]() {
        for (std::uint64_t i = 0; i < item_count; ++i) {
            const int expected = static_cast<int>(i);
            const auto start_ns = now_ns();

            if (!queue.push(expected)) {
                valid = false;
                break;
            }

            int actual = 0;
            if (!queue.pop(actual) || actual != expected) {
                valid = false;
                break;
            }

            latency_samples.push_back(now_ns() - start_ns);
        }
    });

    if (!valid) {
        throw std::runtime_error("same-thread benchmark validation failed");
    }

    return make_result(std::string(Variant::name) + "/same_thread_push_pop",
                       Capacity,
                       item_count,
                       elapsed_seconds,
                       std::move(latency_samples),
                       valid);
}

struct TimedPayload {
    std::uint64_t sequence = 0;
    std::uint64_t enqueue_time_ns = 0;
};

template <typename Variant, std::size_t Capacity>
BenchmarkResult run_concurrent_transfer(std::uint64_t item_count) {
    typename Variant::template queue<TimedPayload, Capacity> queue;
    bool valid = true;
    std::vector<std::uint64_t> latency_samples;
    latency_samples.reserve(static_cast<std::size_t>(item_count));

    const double elapsed_seconds = measure_seconds([&]() {
        std::thread producer([&]() {
            for (std::uint64_t i = 0; i < item_count; ++i) {
                TimedPayload payload{i, now_ns()};
                while (!queue.push(payload)) {
                    std::this_thread::yield();
                    payload.enqueue_time_ns = now_ns();
                }
            }
        });

        std::thread consumer([&]() {
            for (std::uint64_t i = 0; i < item_count; ++i) {
                TimedPayload payload;

                while (!queue.pop(payload)) {
                    std::this_thread::yield();
                }

                if (payload.sequence != i) {
                    valid = false;
                }

                latency_samples.push_back(now_ns() - payload.enqueue_time_ns);
            }
        });

        producer.join();
        consumer.join();
    });

    if (!valid) {
        throw std::runtime_error("concurrent benchmark validation failed");
    }

    return make_result(std::string(Variant::name) + "/concurrent_transfer",
                       Capacity,
                       item_count,
                       elapsed_seconds,
                       std::move(latency_samples),
                       valid);
}

template <typename Variant, std::size_t Capacity>
void run_capacity_benchmarks(ResultWriter& writer, const std::vector<std::uint64_t>& item_counts) {
    for (const auto item_count : item_counts) {
        writer.write_result(run_same_thread_push_pop<Variant, Capacity>(item_count));
        writer.write_result(run_concurrent_transfer<Variant, Capacity>(item_count));
    }
}

template <typename Variant>
void run_variant_benchmarks(ResultWriter& writer, const std::vector<std::uint64_t>& item_counts) {
    run_capacity_benchmarks<Variant, 1024>(writer, item_counts);
    run_capacity_benchmarks<Variant, 65536>(writer, item_counts);
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
        const std::vector<std::uint64_t> item_counts{1'000'000, 10'000'000};

        writer.write_line("SPSC queue benchmark results");
        writer.write_line("result_file=" + result_file.string());
        writer.write_csv_header();
        for_each_registered_spsc_variant([&]<typename Variant>() {
            run_variant_benchmarks<Variant>(writer, item_counts);
        });
    } catch (const std::exception& error) {
        std::cerr << "benchmark failed: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
