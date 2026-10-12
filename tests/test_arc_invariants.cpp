#include <cstddef>
#include <random>

#include "arc_cache.hpp"
#include "test_common.hpp"

namespace {

constexpr int kKeyspace = 64;

// ARC 结构不变量（论文口径）。若实现只在 push 时约束、后续插入又破坏，
// 这里会直接暴露出来。
void check_invariants(const arc::ARCCache<int, int>& c, std::size_t cap) {
    const auto s = c.stats();
    CHECK(s.t1 + s.t2 <= cap);        // |T1| + |T2| <= c
    CHECK(s.t1 + s.b1 <= cap);        // |T1| + |B1| <= c
    CHECK(s.t2 + s.b2 <= cap);        // |T2| + |B2| <= c
    CHECK(s.p <= cap);                // 自适应指针不超过容量
    CHECK(c.size() == s.t1 + s.t2);   // size() == |T1| + |T2|
}

void test_capacity_zero() {
    arc::ARCCache<int, int> c(0);
    c.put(1, 1);
    CHECK(c.size() == 0);
    CHECK(!c.get(1).has_value());
    CHECK(!c.peek(1).has_value());
    CHECK(!c.erase(1));
    check_invariants(c, 0);
}

void test_fuzz_invariants() {
    std::mt19937 rng(2024);
    std::uniform_int_distribution<int> key(0, kKeyspace - 1);
    std::uniform_int_distribution<int> op(0, 99);
    std::uniform_int_distribution<int> val(0, 1 << 20);

    for (std::size_t cap : {std::size_t(1), std::size_t(2), std::size_t(3),
                            std::size_t(5), std::size_t(8), std::size_t(16)}) {
        arc::ARCCache<int, int> c(cap);
        for (int i = 0; i < 20000; ++i) {
            const int k = key(rng);
            const int r = op(rng);
            if (r < 50) {
                (void)c.get(k);
            } else if (r < 85) {
                c.put(k, val(rng));
            } else if (r < 95) {
                c.erase(k);
            } else {
                // peek 必须纯只读：不改变任何内部计数
                const auto before = c.stats();
                (void)c.peek(k);
                const auto after = c.stats();
                CHECK(before.p == after.p && before.t1 == after.t1 &&
                      before.t2 == after.t2 && before.b1 == after.b1 &&
                      before.b2 == after.b2);
            }

            check_invariants(c, cap);

            // contains 与 peek 对同一 key 应一致（两者都不改状态）
            const int probe = key(rng);
            CHECK(c.contains(probe) == c.peek(probe).has_value());
        }
    }
}

void test_erase_semantics() {
    arc::ARCCache<int, int> c(4);
    for (int i = 0; i < 4; ++i) {
        c.put(i, i);
    }
    const std::size_t before = c.size();
    CHECK(c.contains(2));
    CHECK(c.erase(2));
    CHECK(!c.contains(2));
    CHECK(c.size() == before - 1);
    CHECK(!c.erase(2));   // 再删返回 false
    check_invariants(c, 4);
}

} // namespace

int main() {
    test_capacity_zero();
    test_fuzz_invariants();
    test_erase_semantics();
    return cachetest::summary();
}
