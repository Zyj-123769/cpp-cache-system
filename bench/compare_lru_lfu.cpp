#include <iomanip>
#include <iostream>
#include <vector>

#include "cache.hpp"
#include "lfu_cache.hpp"

namespace {

struct Metrics {
    double overall;  // 测量阶段整体命中率
    double hot;      // 测量阶段热点 key 命中率
};

// 用给定缓存实现统计命中率：
//   先跑 warmup（不计入统计）建立热点频次，再跑 trace 统计命中/未命中（未命中则 put 回源填充）。
template <typename Cache>
Metrics measure(const std::vector<int>& warmup, const std::vector<int>& trace,
                std::size_t capacity, int hot_id_base) {
    Cache cache(capacity);

    for (int k : warmup) {
        if (!cache.get(k)) {
            cache.put(k, 1);
        }
    }

    std::size_t total = 0, hits = 0, hot_total = 0, hot_hits = 0;
    for (int key : trace) {
        const bool is_hot = (key >= hot_id_base);
        ++total;
        if (is_hot) {
            ++hot_total;
        }
        if (cache.get(key)) {
            ++hits;
            if (is_hot) {
                ++hot_hits;
            }
        } else {
            cache.put(key, 1);
        }
    }

    Metrics m;
    m.overall = total ? static_cast<double>(hits) / static_cast<double>(total) : 0.0;
    m.hot     = hot_total ? static_cast<double>(hot_hits) / static_cast<double>(hot_total) : 0.0;
    return m;
}

} // namespace

int main() {
    const std::size_t capacity    = 10;
    const int         hot_keys    = 10;    // 稳定热点数量
    const int         cold_keys   = 100;   // 偶发冷数据数量
    const int         hot_id_base = 10000; // 热点 key id 基址（与冷 key 区分）
    const int         cycles      = 50;

    // 预热：让热点 key 高频访问，累积较高频次
    std::vector<int> warmup;
    for (int w = 0; w < 20; ++w) {
        for (int h = 1; h <= hot_keys; ++h) {
            warmup.push_back(hot_id_base + h);
        }
    }

    // 测量序列：每轮「偶发冷扫描（每个冷 key 仅 1 次） + 稳定热点（每个 1 次）」
    std::vector<int> trace;
    trace.reserve(static_cast<std::size_t>(cycles) * (cold_keys + hot_keys));
    for (int r = 0; r < cycles; ++r) {
        for (int c = 1; c <= cold_keys; ++c) {
            trace.push_back(c);
        }
        for (int h = 1; h <= hot_keys; ++h) {
            trace.push_back(hot_id_base + h);
        }
    }

    const auto lru = measure<lru::LRUCache<int, int>>(warmup, trace, capacity, hot_id_base);
    const auto lfu = measure<lfu::LFUCache<int, int>>(warmup, trace, capacity, hot_id_base);

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "=== LRU vs LFU 命中率对比 ===\n";
    std::cout << "场景：偶发冷扫描（" << cold_keys << " 个 key，每轮各 1 次） + 稳定热点（"
              << hot_keys << " 个 key，每轮各 1 次），容量 " << capacity << "，共 " << cycles
              << " 轮\n";
    std::cout << "trace 长度 = " << trace.size() << "（预热 " << warmup.size()
              << " 次不计入）\n\n";
    std::cout << "LRU: 整体命中率 = " << (lru.overall * 100.0)
              << "%  热点命中率 = " << (lru.hot * 100.0) << "%\n";
    std::cout << "LFU: 整体命中率 = " << (lfu.overall * 100.0)
              << "%  热点命中率 = " << (lfu.hot * 100.0) << "%\n";

    if (lfu.overall > lru.overall && lfu.hot > lru.hot) {
        std::cout << "\n结论：LFU 命中率高于 LRU，符合预期（LFU 抗缓存污染，保护稳定热点）。\n";
        return 0;
    }

    std::cout << "\n结论：异常，LFU 未胜出 LRU，请检查实现或序列构造。\n";
    return 1;
}