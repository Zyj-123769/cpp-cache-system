#pragma once

#include <algorithm>
#include <cstddef>
#include <list>
#include <optional>
#include <unordered_map>

namespace arc {

// 自适应替换缓存 ARC（模板类声明）。
//
// 四个结构：
//   - T1：近期项（访问过一次），front=MRU；实际缓存，占用约 p 个槽；
//   - T2：频繁项（访问≥2次），front=MRU；实际缓存，占用约 c-p 个槽；
//   - B1：T1 的「幽灵列表」——只记 key，记录最近被从 T1 淘汰的 key；
//   - B2：T2 的「幽灵列表」——只记 key，记录最近被从 T2 淘汰的 key。
// 自适应：p 是 T1 的目标大小，命中 B1 时增大 p（偏重近期），命中 B2 时减小 p（偏重频繁）。
// 不变量（规范化 ARC）：|T1|+|T2| <= c、|T1|+|B1| <= c、|T2|+|B2| <= c。
template <typename K, typename V>
class ARCCache {
public:
    using key_type   = K;
    using value_type = V;
    using size_type  = std::size_t;

    struct Entry {
        key_type   key;
        value_type value;
    };

    using entry_list = std::list<Entry>;
    using entry_iter = typename entry_list::iterator;
    using key_list   = std::list<key_type>;
    using key_iter   = typename key_list::iterator;

    // 构造缓存。capacity == 0 表示禁用缓存。
    explicit ARCCache(size_type capacity);

    // 查询：命中（T1/T2）则把节点提升为「频繁」并返回 value；未命中返回 std::nullopt。
    std::optional<value_type> get(const key_type& key);

    // 查询但不改变局部性（const）：命中返回 value，不把 T1 提升到 T2。
    std::optional<value_type> peek(const key_type& key) const;

    // 写入：命中则更新并提升；未命中则按 ARC 自适应策略淘汰、并按幽灵命中插入 T1/T2。
    void put(const key_type& key, const value_type& value);

    // 删除缓存中的 key（若在 T1/T2）；返回是否删除成功。幽灵列表中的 key 不算命中。
    bool erase(const key_type& key);

    bool contains(const key_type& key) const;
    size_type size() const noexcept;
    size_type capacity() const noexcept;

    // 内省（只读，不改变缓存状态）：供基准/诊断观察 ARC 内部结构。
    struct Stats {
        size_type p;                // T1 的目标大小（自适应调权结果）
        size_type t1, t2, b1, b2;   // 四个列表的当前长度
    };
    Stats stats() const noexcept;

    void clear() noexcept;

private:
    // 缓存满时决定淘汰 T1 还是 T2，并把被淘汰项送入对应幽灵列表。
    void replace(bool key_in_b2);

    void move_t1_to_t2(entry_iter it);
    void move_t2_front(entry_iter it);
    void insert_t1(const key_type& key, const value_type& value);
    void insert_t2(const key_type& key, const value_type& value);

    void push_b1(const key_type& key);
    void push_b2(const key_type& key);
    void erase_b1(const key_type& key);
    void erase_b2(const key_type& key);

    // 维护规范不变量 |T1|+|B1| <= c、|T2|+|B2| <= c：T1/T2 增大后回裁对应幽灵列表。
    void trim_b1();
    void trim_b2();

    size_type capacity_;
    size_type p_ = 0;   // T1 目标大小（自适应调权结果）

    entry_list t1_;   // 近期项
    entry_list t2_;   // 频繁项
    key_list   b1_;   // T1 幽灵（仅 key）
    key_list   b2_;   // T2 幽灵（仅 key）

    std::unordered_map<key_type, entry_iter> t1_map_;
    std::unordered_map<key_type, entry_iter> t2_map_;
    std::unordered_map<key_type, key_iter>   b1_map_;
    std::unordered_map<key_type, key_iter>   b2_map_;
};

} // namespace arc

// 模板实现必须对实例化点可见，故在声明末尾包含实现文件。
#include "../src/arc_cache.tpp"