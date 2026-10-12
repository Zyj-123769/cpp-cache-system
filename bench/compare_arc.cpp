#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

#include "arc_cache.hpp"
#include "cache.hpp"
#include "lfu_cache.hpp"

namespace {

// 统一接口的命中率统计：get 命中则 hit，未命中则 put 回源填充。
template <typename Cache>
double hit_rate(const std::vector<int>& trace, std::size_t capacity) {
    Cache cache(capacity);
    std::size_t hits = 0;
    for (int key : trace) {
        if (cache.get(key)) {
            ++hits;
        } else {
            cache.put(key, 1);
        }
    }
    return trace.empty() ? 0.0 : static_cast<double>(hits) / static_cast<double>(trace.size());
}

} // namespace

int main() {
    const std::size_t capacity   = 32;
    const int         hot_keys   = 20;    // 局部热点数量
    const int         scan_keys  = 200;   // 循环扫描的 key 数量（> 容量，制造扫描抖动）
    const int         hot_id_base = 10000;
    const double      hot_ratio   = 0.6;  // 60% 访问热点，40% 顺序扫描
    const std::size_t steps       = 200000;

    std::mt19937 rng(42);
    std::uniform_real_distribution<double> op(0.0, 1.0);
    std::uniform_int_distribution<int> hot(1, hot_keys);

    int next_scan = 1;
    std::vector<int> trace;
    trace.reserve(steps);
    for (std::size_t i = 0; i < steps; ++i) {
        int key;
        if (op(rng) < hot_ratio) {
            key = hot_id_base + hot(rng);   // 局部热点
        } else {
            key = next_scan;                // 循环顺序扫描
            next_scan = next_scan % scan_keys + 1;
        }
        trace.push_back(key);
    }

    const double lru_rate = hit_rate<lru::LRUCache<int, int>>(trace, capacity);
    const double lfu_rate = hit_rate<lfu::LFUCache<int, int>>(trace, capacity);
    const double arc_rate = hit_rate<arc::ARCCache<int, int>>(trace, capacity);

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "=== ARC vs LRU vs LFU 命中率对比 ===\n";
    std::cout << "场景：循环扫描（" << scan_keys << " 个 key）+ 局部热点（" << hot_keys
              << " 个 key，占比 " << (hot_ratio * 100) << "%），容量 " << capacity
              << "，共 " << steps << " 次访问\n\n";
    std::cout << "LRU 命中率 = " << (lru_rate * 100.0) << "%\n";
    std::cout << "LFU 命中率 = " << (lfu_rate * 100.0) << "%\n";
    std::cout << "ARC 命中率 = " << (arc_rate * 100.0) << "%\n";

    if (arc_rate >= lru_rate) {
        std::cout << "\n结论：ARC 命中率不低于 LRU（符合题面验收要求）。\n";
        return 0;
    }

    std::cout << "\n结论：异常，ARC 低于 LRU，请检查实现或序列构造。\n";
    return 1;
}