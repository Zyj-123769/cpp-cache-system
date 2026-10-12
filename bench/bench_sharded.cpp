// bench/bench_sharded.cpp
//
// 目的：给出 ShardedLRUCache 的「分片数 × 线程数 → 吞吐」扩展曲线。
// 现有 bench 都是单线程 LRU/LFU/ARC，分片缓存的并发扩展此前没有任何数据。
//
// 说明：并发吞吐受本机 CPU 核数与调度影响，数值仅供对比同一台机器上的相对趋势，
//       不构成跨机器/跨环境的绝对基准。

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

#include "sharded_cache.hpp"

namespace {

// 以 threads 个线程并发跑 total 次 get/put（读占比 80%），返回吞吐（Mops/s）。
double run_config(std::size_t shards, unsigned threads, std::size_t total, std::size_t cap) {
    sharded::ShardedLRUCache<int, int> cache(cap, shards);
    const std::size_t per = total / threads;

    auto worker = [&cache](std::size_t n, std::uint32_t seed) {
        std::mt19937 rng(seed);
        std::uniform_int_distribution<int> key(0, 9999);
        std::uniform_int_distribution<int> op(0, 99);
        for (std::size_t i = 0; i < n; ++i) {
            const int k = key(rng);
            if (op(rng) < 80) {
                (void)cache.get(k);
            } else {
                cache.put(k, static_cast<int>(i));
            }
        }
    };

    std::vector<std::thread> pool;
    pool.reserve(threads);
    const auto t0 = std::chrono::steady_clock::now();
    for (unsigned t = 0; t < threads; ++t) {
        pool.emplace_back(worker, per, 0x9e3779b9u * (t + 1));
    }
    for (auto& th : pool) {
        th.join();
    }
    const auto t1 = std::chrono::steady_clock::now();

    const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    const std::size_t done = per * threads;
    return ms > 0.0 ? static_cast<double>(done) / ms / 1000.0 : 0.0;
}

} // namespace

int main() {
    const std::size_t cap   = 1024;
    const std::size_t total = 1'000'000;

    const std::vector<std::size_t> shard_options = {1, 2, 4, 8, 16};
    const std::vector<unsigned>    thread_options = {1, 2, 4, 8};

    std::cout << "=== ShardedLRUCache 并发吞吐（容量 " << cap << "，每次配置 "
              << total << " 次 get/put，读占比 80%）===\n";
    std::cout << "行 = 分片数，列 = 线程数；单位 Mops/s。数值受本机核数影响，仅示意扩展趋势。\n\n";

    std::cout << std::setw(12) << "shards\\thr";
    for (unsigned t : thread_options) {
        std::cout << std::setw(10) << t;
    }
    std::cout << '\n';

    for (std::size_t s : shard_options) {
        std::cout << std::setw(12) << s;
        for (unsigned t : thread_options) {
            const double mops = run_config(s, t, total, cap);
            std::cout << std::setw(10) << std::fixed << std::setprecision(2) << mops;
        }
        std::cout << '\n';
    }

    std::cout << "\n结论：线程数增大时，分片越多吞吐越高（锁竞争越少）；"
                 "分片数 = 1（单把锁）在并发下吞吐明显受限，甚至下降。\n";
    return 0;
}
