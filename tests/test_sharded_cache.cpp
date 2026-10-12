#include <thread>
#include <vector>

#include "sharded_cache.hpp"
#include "test_common.hpp"

namespace {

constexpr int kThreads = 4;
constexpr int kIters   = 50000;

void test_single_thread() {
    sharded::ShardedLRUCache<int, int> c(256, 8);
    CHECK(c.shard_count() == 8);
    CHECK(c.capacity() == 256);

    // 8 分片共 256 槽（每片 32），只写 64 个 key，确保 key=5 不会被淘汰。
    for (int i = 0; i < 64; ++i) {
        c.put(i, i * 2);
    }
    CHECK(c.size() <= c.capacity());

    CHECK(c.get(5) == 10);
    CHECK(c.peek(5) == 10);   // 只读查询
    CHECK(c.contains(5));
    CHECK(!c.contains(999));
    CHECK(c.erase(5));
    CHECK(!c.get(5).has_value());
    CHECK(!c.contains(5));
    CHECK(!c.erase(5));   // 二次删除返回 false
}

// 容量余数分摊：不是整除时，capacity() 也应精确等于请求值。
void test_capacity_exact() {
    sharded::ShardedLRUCache<int, int> c(100, 16);   // 100 不能被 16 整除
    CHECK(c.shard_count() == 16);
    CHECK(c.capacity() == 100);

    for (int i = 0; i < 500; ++i) {
        c.put(i, i);
    }
    CHECK(c.size() <= 100);
}

// 多线程冒烟：并发读写不崩溃、容量约束始终成立（数据竞争由 CI 的 TSan 作业把关）。
void test_multithread_smoke() {
    sharded::ShardedLRUCache<int, int> c(256, 16);

    std::vector<std::thread> pool;
    for (int t = 0; t < kThreads; ++t) {
        pool.emplace_back([&c, t] {
            for (int i = 0; i < kIters; ++i) {
                const int k = (i * 7 + t) % 512;
                if (i % 3 == 0) {
                    c.put(k, i);
                } else {
                    (void)c.get(k);   // 只验证线程安全，不校验值
                }
            }
        });
    }
    for (auto& th : pool) {
        th.join();
    }

    CHECK(c.size() <= c.capacity());
    CHECK(c.size() > 0);
}

} // namespace

int main() {
    test_single_thread();
    test_capacity_exact();
    test_multithread_smoke();
    return cachetest::summary();
}
