#include <cstddef>
#include <functional>
#include <string>

#include "arc_cache.hpp"
#include "cache.hpp"
#include "lfu_cache.hpp"
#include "test_common.hpp"

// 对抗性 key：哈希恒为 0，强制所有 key 落进同一个桶，验证「正确性与哈希质量无关」。
struct CollidingKey {
    int v;
};

inline bool operator==(const CollidingKey& a, const CollidingKey& b) noexcept {
    return a.v == b.v;
}

namespace std {
template <>
struct hash<CollidingKey> {
    std::size_t operator()(const CollidingKey&) const noexcept { return 0; }
};
} // namespace std

namespace {

const std::string kBigValue(1u << 20, 'x');   // 1 MiB 的超大 value

void test_large_value() {
    lru::LRUCache<int, std::string> lru(2);
    lru.put(1, kBigValue);
    auto a = lru.get(1);
    CHECK(a.has_value() && *a == kBigValue);

    lfu::LFUCache<int, std::string> lfu(2);
    lfu.put(1, kBigValue);
    auto b = lfu.get(1);
    CHECK(b.has_value() && *b == kBigValue);

    arc::ARCCache<int, std::string> arc(2);
    arc.put(1, kBigValue);
    auto c = arc.get(1);
    CHECK(c.has_value() && *c == kBigValue);
}

void test_colliding_keys_lru() {
    lru::LRUCache<CollidingKey, int> c(3);
    for (int i = 0; i < 3; ++i) {
        c.put(CollidingKey{i}, i);
    }
    CHECK(c.get(CollidingKey{0}) == 0);       // 0 提升为最近使用
    c.put(CollidingKey{3}, 3);                // 淘汰最久未用的 1
    CHECK(!c.get(CollidingKey{1}).has_value());
    CHECK(c.get(CollidingKey{2}) == 2);
    CHECK(c.get(CollidingKey{0}) == 0);
    CHECK(c.get(CollidingKey{3}) == 3);
    CHECK(c.size() == 3);

    // 大量冲突 key 下容量约束与成员一致性仍成立
    for (int i = 0; i < 50; ++i) {
        c.put(CollidingKey{i}, i);
    }
    CHECK(c.size() == 3);
}

void test_colliding_keys_lfu() {
    lfu::LFUCache<CollidingKey, int> c(2);
    c.put(CollidingKey{0}, 0);
    c.put(CollidingKey{1}, 1);
    CHECK(c.get(CollidingKey{0}) == 0);       // 0 频次 +1
    c.put(CollidingKey{2}, 2);                // 淘汰频次最低的 1
    CHECK(!c.get(CollidingKey{1}).has_value());
    CHECK(c.get(CollidingKey{0}) == 0);
    CHECK(c.get(CollidingKey{2}) == 2);
    CHECK(c.size() == 2);
}

void test_colliding_keys_arc() {
    arc::ARCCache<CollidingKey, int> c(3);
    for (int i = 0; i < 20; ++i) {
        c.put(CollidingKey{i}, i);
    }
    CHECK(c.size() == 3);                     // 冲突不会破坏容量约束
    CHECK(c.get(CollidingKey{19}) == 19);     // 最新写入的 key 一定命中
    CHECK(c.size() <= 3);
}

} // namespace

int main() {
    test_large_value();
    test_colliding_keys_lru();
    test_colliding_keys_lfu();
    test_colliding_keys_arc();
    return cachetest::summary();
}
