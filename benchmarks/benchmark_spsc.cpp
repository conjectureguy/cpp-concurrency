//
// Created by rahul on 7/4/26.
//

#include <benchmark/benchmark.h>

#include "lockfree/spsc_queue.h"

static void BM_PushPop(benchmark::State& state) {
    spsc_queue<int, 1024> q;

    for (auto _ : state) {
        benchmark::DoNotOptimize(q.push(42));

        int x;
        benchmark::DoNotOptimize(q.pop(x));
    }
}

BENCHMARK(BM_PushPop);

BENCHMARK_MAIN();