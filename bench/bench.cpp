#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <vector>

#include "cache.hpp"

namespace {

struct Result {
    std::size_t ops;
    std::size_t reads;
    std::size_t hits;
    double      hit_rate;   // = hits / reads
    double      elapsed_ms;
};

// 随机 get/put 基准：
//   - 每次用 zipf 分布采样一个 key（热点共 num_keys 个）；
//   - 以 read_ratio 概率做 get（命中记 hit，未命中记 miss）；
//   - 否则做 put（写入/更新，不参与命中率统计）。
Result run_bench(std::size_t capacity, int num_keys, std::size_t num_ops,
                 double read_ratio, std::uint32_t seed) {
    lru::LRUCache<int, int> cache(capacity);

    // zipf 权重：rank r(1..N) 权重 1/r^skew
    const double skew = 1.0;
    std::vector<double> weights(static_cast<std::size_t>(num_keys));
    for (int r = 1; r <= num_keys; ++r) {
        weights[static_cast<std::size_t>(r - 1)] =
            1.0 / std::pow(static_cast<double>(r), skew);
    }

    std::mt19937 rng(seed);
    std::discrete_distribution<int> dist(weights.begin(), weights.end());
    std::uniform_real_distribution<double> op_dist(0.0, 1.0);

    std::size_t reads = 0, hits = 0;

    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < num_ops; ++i) {
        const int key = dist(rng);
        if (op_dist(rng) < read_ratio) {
            ++reads;
            if (cache.get(key)) {
                ++hits;
            }
        } else {
            cache.put(key, static_cast<int>(i));
        }
    }
    const auto t1 = std::chrono::steady_clock::now();

    Result r;
    r.ops        = num_ops;
    r.reads      = reads;
    r.hits       = hits;
    r.hit_rate   = reads ? static_cast<double>(hits) / static_cast<double>(reads) : 0.0;
    r.elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return r;
}

} // namespace

int main() {
    const std::size_t NUM_OPS    = 1'000'000;
    const int         HOT_KEYS   = 20;
    const double      READ_RATIO = 0.8;

    std::cout << "== 基准：1,000,000 次随机 get/put，zipf 热点 " << HOT_KEYS
              << " 个 key，读占比 " << READ_RATIO << " ==\n\n";

    std::cout << std::left
              << std::setw(9)  << "capacity"
              << std::setw(12) << "hit_rate"
              << std::setw(10) << "time_ms"
              << std::setw(9)  << "Mops/s"
              << '\n';

    const std::vector<std::size_t> capacities = {2, 3, 5, 10, 20, 50, 100};
    for (auto cap : capacities) {
        auto r = run_bench(cap, HOT_KEYS, NUM_OPS, READ_RATIO, 42);

        const double throughput = r.elapsed_ms > 0.0
                                      ? static_cast<double>(r.ops) / r.elapsed_ms / 1000.0
                                      : 0.0;

        std::ostringstream rate;
        rate << std::fixed << std::setprecision(2) << (r.hit_rate * 100.0) << '%';

        std::cout << std::left << std::fixed
                  << std::setw(9)  << cap
                  << std::setw(12) << rate.str()
                  << std::setw(10) << std::setprecision(1) << r.elapsed_ms
                  << std::setw(9)  << std::setprecision(1) << throughput
                  << '\n';
    }

    return 0;
}