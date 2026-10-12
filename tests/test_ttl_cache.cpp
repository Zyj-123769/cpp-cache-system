#include <chrono>
#include <memory>
#include <optional>

#include "test_common.hpp"
#include "ttl_cache.hpp"

namespace {

using clock = std::chrono::steady_clock;

// 可手动推进的假时钟，保证过期测试确定性。
struct FakeClock {
    clock::time_point t{};
};

void test_basic_and_expiry() {
    auto fake = std::make_shared<FakeClock>();
    ttl::TTLCache<int, int> c(2, std::chrono::seconds(10), [fake] { return fake->t; });

    c.put(1, 100);
    CHECK(c.get(1) == 100);
    CHECK(c.size() == 1);

    fake->t += std::chrono::seconds(5);
    CHECK(c.get(1) == 100);              // 未过期

    fake->t += std::chrono::seconds(6);  // 累计 11s > 10s
    CHECK(!c.get(1).has_value());        // 过期 -> 未命中
    CHECK(c.size() == 0);                // 惰性删除已移除该条目
}

void test_per_entry_ttl() {
    auto fake = std::make_shared<FakeClock>();
    ttl::TTLCache<int, int> c(4, std::chrono::seconds(100), [fake] { return fake->t; });

    c.put(1, 1, std::chrono::seconds(1));    // 短 TTL
    c.put(2, 2, std::chrono::seconds(50));   // 长 TTL

    fake->t += std::chrono::seconds(2);
    CHECK(!c.get(1).has_value());   // 1 已过期
    CHECK(c.get(2) == 2);           // 2 仍有效
}

void test_capacity_eviction_still_lru() {
    auto fake = std::make_shared<FakeClock>();
    ttl::TTLCache<int, int> c(2, std::chrono::seconds(1000), [fake] { return fake->t; });

    c.put(1, 1);
    c.put(2, 2);
    CHECK(c.get(1) == 1);     // 1 变为最近使用
    c.put(3, 3);              // 容量满，LRU 淘汰 2
    CHECK(!c.get(2).has_value());
    CHECK(c.get(1) == 1);
    CHECK(c.get(3) == 3);
    CHECK(c.size() == 2);
}

void test_clear() {
    auto fake = std::make_shared<FakeClock>();
    ttl::TTLCache<int, int> c(2, std::chrono::seconds(10), [fake] { return fake->t; });
    c.put(1, 1);
    c.clear();
    CHECK(c.size() == 0);
    CHECK(!c.get(1).has_value());
}

void test_peek_and_contains_readonly() {
    auto fake = std::make_shared<FakeClock>();
    ttl::TTLCache<int, int> c(2, std::chrono::seconds(10), [fake] { return fake->t; });
    c.put(1, 1);
    c.put(2, 2);

    CHECK(c.peek(1) == 1);   // 命中
    CHECK(c.contains(1));    // 只读
    c.put(3, 3);             // peek/contains 不提升 1 → 淘汰最久未用的 1
    CHECK(!c.contains(1));
    CHECK(c.contains(2));
    CHECK(c.contains(3));

    // 过期：peek/contains 视为未命中，但都不做惰性删除（size 不变）
    auto fake2 = std::make_shared<FakeClock>();
    ttl::TTLCache<int, int> c2(4, std::chrono::seconds(10), [fake2] { return fake2->t; });
    c2.put(1, 1);
    fake2->t += std::chrono::seconds(11);
    CHECK(!c2.peek(1).has_value());
    CHECK(!c2.contains(1));
    CHECK(c2.size() == 1);        // peek/contains 不删除
    CHECK(!c2.get(1).has_value()); // get 才惰性删除
    CHECK(c2.size() == 0);
}

void test_erase() {
    auto fake = std::make_shared<FakeClock>();
    ttl::TTLCache<int, int> c(4, std::chrono::seconds(10), [fake] { return fake->t; });
    c.put(1, 1);
    c.put(2, 2);
    CHECK(!c.erase(9));      // 不存在 -> false
    CHECK(c.erase(1));
    CHECK(!c.contains(1));
    CHECK(c.size() == 1);

    fake->t += std::chrono::seconds(11);   // 过期也不会让已删除的 key 复活
    CHECK(!c.contains(1));
    CHECK(c.size() == 1);    // 只剩 2（未 get，未惰性删除）
}

} // namespace

int main() {
    test_basic_and_expiry();
    test_per_entry_ttl();
    test_capacity_eviction_still_lru();
    test_clear();
    test_peek_and_contains_readonly();
    test_erase();
    return cachetest::summary();
}
