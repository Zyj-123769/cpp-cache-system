#include <chrono>
#include <iostream>
#include <string>

#include "arc_cache.hpp"
#include "cache.hpp"
#include "lfu_cache.hpp"
#include "sharded_cache.hpp"
#include "ttl_cache.hpp"

namespace {

// 打印一次 get 的结果：兼容各缓存统一的 std::optional<V> 接口。
template <typename Cache>
void show(Cache& cache, const typename Cache::key_type& key) {
    auto v = cache.get(key);
    std::cout << "  get(" << key << ") = "
              << (v ? std::to_string(*v) : std::string("<未命中>")) << '\n';
}

void demo_lru() {
    using lru::LRUCache;
    std::cout << "== LRU（容量 3）==\n";

    LRUCache<std::string, int> cache(3);
    cache.put("a", 1);
    cache.put("b", 2);
    cache.put("c", 3);
    std::cout << "  put a/b/c 后 size = " << cache.size() << '\n';

    show(cache, std::string("a"));   // 命中，a 变为最新
    cache.put("d", 4);               // 容量满，淘汰最久未使用的 b
    std::cout << "  put d 后 size = " << cache.size() << '\n';
    show(cache, std::string("b"));   // 未命中（已被淘汰）
    show(cache, std::string("d"));   // 命中

    LRUCache<std::string, int> zero(0);
    zero.put("x", 1);
    std::cout << "  容量 0：put 后 size = " << zero.size() << "（不崩溃）\n";
}

void demo_lfu() {
    using lfu::LFUCache;
    std::cout << "\n== LFU（容量 2）==\n";

    LFUCache<int, int> cache(2);
    cache.put(1, 1);
    cache.put(2, 2);
    show(cache, 1);                  // 命中，1 的频次 +1
    cache.put(3, 3);                 // 容量满，淘汰频次最低的 2
    show(cache, 2);                  // 未命中
    show(cache, 1);                  // 命中
    show(cache, 3);                  // 命中
}

void demo_arc() {
    using arc::ARCCache;
    std::cout << "\n== ARC（容量 3）==\n";

    ARCCache<int, int> cache(3);
    for (int i = 1; i <= 5; ++i) {
        cache.put(i, i * 10);
    }
    std::cout << "  put 1..5 后 size = " << cache.size() << "（始终不超容量）\n";
    show(cache, 5);
    show(cache, 1);
}

void demo_ttl() {
    using ttl::TTLCache;
    std::cout << "\n== TTL（容量 4，默认 TTL=10s；注入时钟演示过期，无需 sleep）==\n";

    auto now = std::chrono::steady_clock::time_point{};
    TTLCache<std::string, int> cache(4, std::chrono::seconds(10), [&now] { return now; });

    cache.put("a", 1);                              // 用默认 TTL
    cache.put("b", 2, std::chrono::seconds(3));     // 单独指定 3s
    show(cache, std::string("a"));
    show(cache, std::string("b"));

    now += std::chrono::seconds(5);
    std::cout << "  时间前进 5s 后：\n";
    show(cache, std::string("a"));   // 命中（10s 未到）
    show(cache, std::string("b"));   // 已过期 -> 未命中（惰性删除）
}

void demo_sharded() {
    using sharded::ShardedLRUCache;
    std::cout << "\n== ShardedLRUCache（总容量 100，16 分片，线程安全）==\n";

    ShardedLRUCache<int, int> cache(100, 16);
    for (int i = 0; i < 32; ++i) {
        cache.put(i, i * 10);
    }
    std::cout << "  put 0..31 后 size = " << cache.size()
              << "（容量 " << cache.capacity() << "）\n";
    show(cache, 7);
    std::cout << "  contains(7) = " << std::boolalpha << cache.contains(7) << '\n';
    cache.erase(7);
    std::cout << "  erase(7) 后 contains(7) = " << cache.contains(7) << '\n';
}

} // namespace

int main() {
    demo_lru();
    demo_lfu();
    demo_arc();
    demo_ttl();
    demo_sharded();
    return 0;
}
