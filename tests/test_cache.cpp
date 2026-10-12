#include <iostream>
#include <string>

#include "cache.hpp"
#include "test_common.hpp"

namespace {

void test_hit_miss() {
    lru::LRUCache<int, int> c(3);
    CHECK(!c.get(1).has_value());   // 未命中
    CHECK(c.size() == 0);

    c.put(1, 10);
    CHECK(c.get(1) == 10);          // 命中
    CHECK(c.get(1) == 10);          // 命中后移到头部，仍可再取
    CHECK(c.size() == 1);
    CHECK(!c.get(2).has_value());   // 仍未插入的 key
}

void test_eviction_lru() {
    lru::LRUCache<int, int> c(2);
    c.put(1, 1);
    c.put(2, 2);
    c.get(1);            // 1 变为最近使用
    c.put(3, 3);         // 容量满，淘汰最久未使用的 2

    CHECK(!c.get(2).has_value());
    CHECK(c.get(1) == 1);
    CHECK(c.get(3) == 3);
    CHECK(c.size() == 2);
}

void test_update_existing() {
    lru::LRUCache<int, int> c(2);
    c.put(1, 1);
    c.put(2, 2);
    c.put(1, 100);       // 更新已存在 key，1 移到头部
    c.put(3, 3);         // 淘汰最久未使用的 2

    CHECK(c.get(1) == 100);
    CHECK(!c.get(2).has_value());
    CHECK(c.get(3) == 3);
    CHECK(c.size() == 2);
}

void test_capacity_one() {
    lru::LRUCache<int, int> c(1);
    c.put(1, 1);
    c.put(2, 2);         // 立即淘汰 1
    CHECK(!c.get(1).has_value());
    CHECK(c.get(2) == 2);
    CHECK(c.size() == 1);

    c.put(2, 20);        // 更新，size 不增长
    CHECK(c.get(2) == 20);
    CHECK(c.size() == 1);
}

void test_capacity_zero() {
    lru::LRUCache<int, int> c(0);
    c.put(1, 1);         // 写后即淘汰
    CHECK(c.size() == 0);
    CHECK(!c.get(1).has_value());
}

void test_clear_contains() {
    lru::LRUCache<int, int> c(3);
    c.put(1, 1);
    c.put(2, 2);
    CHECK(c.contains(1));
    CHECK(!c.contains(3));

    c.clear();
    CHECK(c.size() == 0);
    CHECK(!c.contains(1));
}

void test_string_keys() {
    lru::LRUCache<std::string, std::string> c(2);
    c.put("k1", "v1");
    c.put("k2", "v2");
    c.get("k1");
    c.put("k3", "v3");   // 淘汰 k2

    CHECK(c.get("k1") == std::string("v1"));
    CHECK(!c.get("k2").has_value());
    CHECK(c.get("k3") == std::string("v3"));
}

void test_peek() {
    lru::LRUCache<int, int> c(2);
    c.put(1, 10);
    c.put(2, 20);

    CHECK(!c.peek(3).has_value());   // 未命中
    CHECK(c.peek(2) == 20);          // 命中返回 value
    CHECK(c.size() == 2);

    CHECK(c.peek(1) == 10);          // peek 不提升 1
    c.put(3, 30);                    // 仍淘汰最久未用的 1
    CHECK(!c.contains(1));
    CHECK(c.contains(2));
    CHECK(c.contains(3));

    const auto& cc = c;              // const 对象可调用
    CHECK(cc.peek(3) == 30);
}

} // namespace

int main() {
    test_hit_miss();
    test_eviction_lru();
    test_update_existing();
    test_capacity_one();
    test_capacity_zero();
    test_clear_contains();
    test_string_keys();
    test_peek();

    return cachetest::summary();
}