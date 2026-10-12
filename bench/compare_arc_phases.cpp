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
    const std::size_t capacity    = 32;
    const int         hot_size    = 20;   // 每个热点集合的 key 数（< 容量）
    const int         num_hot_sets = 4;   // 轮流切换 4 个不同热点集合
    const int         scan_keys   = 200;  // 顺序扫描 key 范围（> 容量，制造污染）
    const int         hot_id_base = 10000;
    const double      hot_ratio   = 0.7;  // 相位内 70% 访问当前热点，30% 顺序扫描
    const int         epoch_len   = 2000; // 每个相位持续访问次数
    const int         num_epochs  = 40;   // 相位段数（4 个热点集合循环 10 轮）

    std::mt19937 rng(7);
    std::uniform_real_distribution<double> op(0.0, 1.0);
    std::uniform_int_distribution<int> hot_pick(0, hot_size - 1);

    // 相位切换负载：每个 epoch 固定「当前热点集合」，epoch 之间整体切换热点；
    // 叠加顺序扫描污染，使 LRU 被扫描冲垮、LFU 因频次固化在旧热点上而滞后，
    // 从而考验 ARC 的自适应调权。
    int next_scan = 1;
    std::vector<int> trace;
    trace.reserve(static_cast<std::size_t>(epoch_len) * num_epochs);
    for (int e = 0; e < num_epochs; ++e) {
        const int set_base = hot_id_base + (e % num_hot_sets) * hot_size;  // 当前热点集合
        for (int t = 0; t < epoch_len; ++t) {
            int key;
            if (op(rng) < hot_ratio) {
                key = set_base + hot_pick(rng);   // 当前相位热点
            } else {
                key = next_scan;                  // 循环顺序扫描
                next_scan = next_scan % scan_keys + 1;
            }
            trace.push_back(key);
        }
    }

    const double lru_rate = hit_rate<lru::LRUCache<int, int>>(trace, capacity);
    const double lfu_rate = hit_rate<lfu::LFUCache<int, int>>(trace, capacity);
    const double arc_rate = hit_rate<arc::ARCCache<int, int>>(trace, capacity);

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "=== 相位切换负载：ARC vs LRU vs LFU ===\n";
    std::cout << "参数：容量 " << capacity << "，热点集合 " << num_hot_sets << " 组（每组 "
              << hot_size << " key，相位间整体切换），扫描 " << scan_keys
              << " key，相位内热点占比 " << (hot_ratio * 100) << "%，每相位 " << epoch_len
              << " 次、共 " << num_epochs << " 相位\n";
    std::cout << "trace 长度 = " << trace.size() << "\n\n";
    std::cout << "LRU 命中率 = " << (lru_rate * 100.0) << "%\n";
    std::cout << "LFU 命中率 = " << (lfu_rate * 100.0) << "%\n";
    std::cout << "ARC 命中率 = " << (arc_rate * 100.0) << "%\n";

    if (arc_rate >= lru_rate) {
        std::cout << "\n结论：ARC 命中率不低于 LRU（自适应调权在相位切换下生效）。\n";
        if (arc_rate >= lfu_rate) {
            std::cout << "且 ARC 同时不低于 LFU，体现了对「频次固化」的缓解。\n";
        }
        return 0;
    }

    std::cout << "\n结论：异常，ARC 低于 LRU，请检查实现或序列构造。\n";
    return 1;
}