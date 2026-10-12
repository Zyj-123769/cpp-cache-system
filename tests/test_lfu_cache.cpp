#include <iostream>
#include <string>

#include "lfu_cache.hpp"
#include "test_common.hpp"

namespace {

void test_basic_get_put() {
    lfu::LFUCache<int, int> c(3);
    CHECK(!c.get(1).has_value());   // 未命中
    CHECK(c.size() == 0);

    c.put(1, 10);
    CHECK(c.get(1) == 10);          // 命中
    CHECK(c.get(1) == 10);          // 再命中，频次 +1
    CHECK(c.size() == 1);
}

void test_eviction_by_frequency() {
    lfu::LFUCache<int, int> c(2);
    c.put(1, 1);
    c.put(2, 2);
    c.get(1);            // 1 频次 2
    c.put(3, 3);         // 容量满，淘汰频次最低的 2
    CHECK(!c.get(2).has_value());
    CHECK(c.get(1) == 1);
    CHECK(c.get(3) == 3);
}

void test_tie_break_recency() {
    lfu::LFUCache<int, int> c(2);
    c.put(1, 1);
    c.put(2, 2);         // 同频 1，1 更旧
    c.put(3, 3);         // 淘汰同频中最旧的 1
    CHECK(!c.get(1).has_value());
    CHECK(c.get(2) == 2);
    CHECK(c.get(3) == 3);
}

void test_update_existing() {
    lfu::LFUCache<int, int> c(2);
    c.put(1, 1);
    c.put(2, 2);
    c.put(1, 100);       // 更新且频次 2
    c.put(3, 3);         // 淘汰频次 1 的 2
    CHECK(c.get(1) == 100);
    CHECK(!c.get(2).has_value());
    CHECK(c.get(3) == 3);
    CHECK(c.size() == 2);
}

void test_capacity_one() {
    lfu::LFUCache<int, int> c(1);
    c.put(1, 1);
    c.put(2, 2);         // 立即淘汰 1
    CHECK(!c.get(1).has_value());
    CHECK(c.get(2) == 2);
    CHECK(c.size() == 1);
}

void test_capacity_zero() {
    lfu::LFUCache<int, int> c(0);
    c.put(1, 1);
    CHECK(c.size() == 0);
    CHECK(!c.get(1).has_value());
}

void test_clear_contains() {
    lfu::LFUCache<int, int> c(3);
    c.put(1, 1);
    CHECK(c.contains(1));
    CHECK(!c.contains(2));
    c.clear();
    CHECK(c.size() == 0);
    CHECK(!c.contains(1));
}

// 关键边界：频次桶最小频次指针 min_freq_ 的更新逻辑。
void test_min_freq_pointer_update() {
    lfu::LFUCache<int, int> c(3);
    c.put(1, 1);
    c.put(2, 2);
    c.put(3, 3);         // 全部 freq=1, minFreq=1

    c.get(1);            // 1 -> freq2, bucket1 仍含 2,3，minFreq 仍为 1
    c.get(2);            // 2 -> freq2, bucket1 仍含 3
    c.get(3);            // 3 -> freq2, bucket1 变空 -> minFreq 更新为 2

    c.get(1);            // 1 -> freq3, bucket2 仍含 2,3
    c.get(2);            // 2 -> freq3, bucket2 仍含 3
    c.get(3);            // 3 -> freq3, bucket2 变空 -> minFreq 更新为 3

    c.put(4, 4);         // 容量满，淘汰 minFreq=3 同频最旧的 1
    CHECK(!c.get(1).has_value());
    CHECK(c.get(2) == 2);
    CHECK(c.get(3) == 3);
    CHECK(c.get(4) == 4);
}

void test_peek_no_freq_change() {
    lfu::LFUCache<int, int> c(2);
    c.put(1, 1);
    c.put(2, 2);
    for (int i = 0; i < 5; ++i) {
        CHECK(c.peek(1) == 1);   // peek 不增加频次
    }
    c.put(3, 3);                 // 频次相同则淘汰更旧的 1（若 peek 偷偷 +频次，就会淘汰 2）
    CHECK(!c.contains(1));
    CHECK(c.contains(2));
    CHECK(c.contains(3));
}

void test_erase() {
    lfu::LFUCache<int, int> c(3);
    c.put(1, 1);
    c.put(2, 2);
    c.put(3, 3);
    c.get(1);                // 1 频次 2
    CHECK(!c.erase(99));     // 不存在 -> false
    CHECK(c.erase(1));
    CHECK(!c.contains(1));
    CHECK(c.size() == 2);
    CHECK(c.peek(2) == 2);   // 其余 key 不受影响
    CHECK(c.contains(3));
    CHECK(c.erase(2));
    CHECK(c.erase(3));       // 删空（含最小频次桶变空的重算路径）
    CHECK(c.size() == 0);
    CHECK(!c.erase(3));      // 空缓存再删 -> false
}

} // namespace

int main() {
    test_basic_get_put();
    test_eviction_by_frequency();
    test_tie_break_recency();
    test_update_existing();
    test_capacity_one();
    test_capacity_zero();
    test_clear_contains();
    test_min_freq_pointer_update();
    test_peek_no_freq_change();
    test_erase();

    return cachetest::summary();
}