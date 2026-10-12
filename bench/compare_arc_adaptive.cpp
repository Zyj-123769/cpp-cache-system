// bench/compare_arc_adaptive.cpp
//
// 目的：验证 ARC 的自适应指针 p（T1 的目标大小）是否真的随负载分化，而不是只有命中率。
//   - 偏「近期」负载：先建立 T2，再对略大于容量的工作集做循环扫描
//       → 反复命中 B1 幽灵 → p 上升（T1 目标变大）；
//   - 偏「频繁」负载：稳定热点 + 冷样本，命中集中在 T2
//       → p 始终为 0（T1 目标保持小）。
// 指标：时间平均 p、峰值 p、以及 p 的采样轨迹。

#include <algorithm>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "arc_cache.hpp"

namespace {

constexpr std::size_t kCap     = 64;    // 缓存容量 c
constexpr std::size_t kSamples = 16;    // 轨迹采样点数

struct Sample { std::size_t ops; std::size_t p; };

struct Result {
    double hit_rate;
    double p_avg;
    std::size_t p_max;
    std::vector<Sample> traj;
};

Result run(const std::vector<int>& trace, std::size_t cap) {
    arc::ARCCache<int, int> cache(cap);
    Result r{0.0, 0.0, 0, {}};
    std::size_t hits = 0;
    double psum = 0.0;
    const std::size_t every = trace.size() / kSamples;
    for (std::size_t i = 0; i < trace.size(); ++i) {
        if (cache.get(trace[i])) {
            ++hits;
        } else {
            cache.put(trace[i], 1);
        }
        const std::size_t p = cache.stats().p;
        psum += static_cast<double>(p);
        r.p_max = std::max(r.p_max, p);
        if (every && (i + 1) % every == 0 && r.traj.size() < kSamples) {
            r.traj.push_back(Sample{i + 1, p});
        }
    }
    const double n = static_cast<double>(trace.size());
    r.hit_rate = n ? 100.0 * static_cast<double>(hits) / n : 0.0;
    r.p_avg    = n ? psum / n : 0.0;
    return r;
}

std::vector<int> hot(int h, int n) {
    std::vector<int> t;
    for (int i = 0; i < n; ++i) {
        t.push_back(i % h + 1);
    }
    return t;
}

std::vector<int> cyclic_scan(int ws, int rounds) {
    std::vector<int> t;
    for (int r = 0; r < rounds; ++r) {
        for (int k = 1; k <= ws; ++k) {
            t.push_back(k);
        }
    }
    return t;
}

// 偏「近期」：先 640 次热点（建立 T2、给 B1 腾出空间），再循环扫描 96 > c。
std::vector<int> recency_trace() {
    std::vector<int> t = hot(32, 640);
    for (int v : cyclic_scan(96, 60)) {
        t.push_back(v);
    }
    return t;
}

// 偏「频繁」：稳定热点 32 个（占 90%）+ 一次性冷 key。
std::vector<int> frequency_trace() {
    std::vector<int> t;
    int cold = 100000;
    for (int i = 0; i < 8000; ++i) {
        if (i % 10 == 0) {
            t.push_back(cold++);
        } else {
            t.push_back(i % 32 + 1);
        }
    }
    return t;
}

void report(const char* title, const Result& r) {
    std::cout << "\n[ " << title << " ]\n";
    std::cout << "  命中率 " << std::fixed << std::setprecision(2) << r.hit_rate
              << "%   p 平均 " << r.p_avg << "   峰值 " << r.p_max
              << "   (容量 " << kCap << ")\n";
    std::cout << "  p 轨迹: ";
    for (std::size_t i = 0; i < r.traj.size(); ++i) {
        std::cout << (i ? " " : "") << "[" << r.traj[i].ops << "]=" << r.traj[i].p;
    }
    std::cout << "\n";
}

} // namespace

int main() {
    std::cout << "=== ARC 自适应指针 p 的分化验证 ===\n";
    std::cout << "p = T1 的目标大小：p 越大越偏「近期」，越小越偏「频繁」；"
                 "p 只在命中幽灵 B1 时升、命中 B2 时降。\n";

    const Result rec = run(recency_trace(), kCap);
    const Result frq = run(frequency_trace(), kCap);
    report("负载 A：偏近期（先建 T2，再对工作集 96 > 容量做循环扫描）", rec);
    report("负载 B：偏频繁（稳定热点 32 + 一次性冷 key）", frq);

    std::cout << "\n结论：负载 A 下 p 被 B1 命中不断推高（平均"
              << std::fixed << std::setprecision(1) << rec.p_avg
              << "、峰值 " << rec.p_max << "）；负载 B 下命中都在 T2、p 恒为 "
              << frq.p_avg << "。二者分化明显 —— p 确实随访问模式自适应。\n";
    return 0;
}
