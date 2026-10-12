#include <iostream>

#include "arc_cache.hpp"
#include "test_common.hpp"

namespace {

void test_basic_get_put() {
    arc::ARCCache<int, int> c(3);
    CHECK(!c.get(1).has_value());
    c.put(1, 10);
    CHECK(c.get(1) == 10);
    CHECK(c.size() == 1);
    c.get(1);               // 二次访问（升为频繁），仍命中
    CHECK(c.get(1) == 10);
    CHECK(c.size() == 1);
}

void test_capacity_bound() {
    arc::ARCCache<int, int> c(3);
    for (int i = 1; i <= 20; ++i) {
        c.put(i, i * 10);
    }
    CHECK(c.size() == 3);   // 永不超容量

    int cnt = 0;
    for (int i = 1; i <= 20; ++i) {
        if (c.get(i).has_value()) {
            ++cnt;
        }
    }
    CHECK(cnt == 3);        // 恰好存满 3 个 key
}

void test_update_existing() {
    arc::ARCCache<int, int> c(2);
    c.put(1, 1);
    c.put(2, 2);
    c.put(1, 100);          // 更新已存在 key
    CHECK(c.get(1) == 100);
    CHECK(c.size() == 2);
}

void test_capacity_one() {
    arc::ARCCache<int, int> c(1);
    c.put(1, 1);
    c.put(2, 2);
    CHECK(c.size() == 1);
    CHECK(c.get(2) == 2);
}

void test_capacity_zero() {
    arc::ARCCache<int, int> c(0);
    c.put(1, 1);
    CHECK(c.size() == 0);
    CHECK(!c.get(1).has_value());
}

void test_clear_contains() {
    arc::ARCCache<int, int> c(3);
    c.put(1, 1);
    c.put(2, 2);
    CHECK(c.contains(1));
    CHECK(!c.contains(3));
    c.clear();
    CHECK(c.size() == 0);
    CHECK(!c.contains(1));
}

// 幽灵列表容错：反复淘汰 + 回头访问，确保不崩溃且容量约束始终成立。
void test_eviction_and_ghost_paths() {
    arc::ARCCache<int, int> c(4);
    for (int i = 1; i <= 30; ++i) {
        c.put(i, i);
    }
    CHECK(c.size() == 4);

    for (int i = 1; i <= 30; ++i) {
        c.get(i);           // 含大量幽灵命中 / 未命中路径
    }
    CHECK(c.size() == 4);

    for (int i = 31; i <= 60; ++i) {
        c.put(i, i);
    }
    CHECK(c.size() == 4);

    for (int i = 1; i <= 60; ++i) {
        c.get(i);
    }
    CHECK(c.size() == 4);
}

void test_peek_no_promote() {
    arc::ARCCache<int, int> c(3);
    c.put(1, 10);
    const auto s0 = c.stats();
    CHECK(s0.t1 == 1 && s0.t2 == 0);

    CHECK(c.peek(1) == 10);            // 只读，不应把 T1 提升到 T2
    const auto s1 = c.stats();
    CHECK(s1.t1 == 1 && s1.t2 == 0);

    CHECK(c.get(1) == 10);             // get 才会提升
    const auto s2 = c.stats();
    CHECK(s2.t1 == 0 && s2.t2 == 1);

    CHECK(!c.peek(99).has_value());
}

} // namespace

int main() {
    test_basic_get_put();
    test_capacity_bound();
    test_update_existing();
    test_capacity_one();
    test_capacity_zero();
    test_clear_contains();
    test_eviction_and_ghost_paths();
    test_peek_no_promote();

    return cachetest::summary();
}