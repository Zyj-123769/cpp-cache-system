#pragma once

#include <cstddef>
#include <cstdint>
#include <list>
#include <optional>
#include <unordered_map>

namespace lfu {

// 容量受限的 LFU 缓存（模板类声明）。
//
// 核心结构：
//   - std::unordered_map<int, std::list<Node>> buckets_ : 频次桶，同频节点同链表
//       （链表头部=同频内最久未用，尾部=最新）；
//   - std::unordered_map<K, list::iterator> index_       : key -> 节点迭代器，O(1) 定位；
//   - int min_freq_                                      : 最小非空频次指针，淘汰直接取该桶。
template <typename K, typename V>
class LFUCache {
public:
    using key_type   = K;
    using value_type = V;
    using size_type  = std::size_t;
    // 频次用 64 位无符号：热点 key 被访问数亿次也不会回绕成 0 污染桶序（回绕需约 1.8e19 次）。
    using freq_type  = std::uint64_t;

    struct Node {
        key_type   key;
        value_type value;
        freq_type  freq;
    };

    using list_type = std::list<Node>;
    using iterator  = typename list_type::iterator;
    using key_map   = std::unordered_map<key_type, iterator>;
    using freq_map  = std::unordered_map<freq_type, list_type>;

    // 构造缓存。capacity == 0 表示禁用缓存。
    explicit LFUCache(size_type capacity);

    // 查询：命中则频次 +1 并返回 value；未命中返回 std::nullopt。
    std::optional<value_type> get(const key_type& key);

    // 查询但不改变频次（const）：命中返回 value，未命中返回 std::nullopt。
    std::optional<value_type> peek(const key_type& key) const;

    // 写入：已存在则更新并频次 +1；否则插入（频次 1），超容量时淘汰。
    void put(const key_type& key, const value_type& value);

    // 删除指定 key；返回是否删除成功（key 不存在返回 false）。
    bool erase(const key_type& key);

    bool contains(const key_type& key) const;
    size_type size() const noexcept;
    size_type capacity() const noexcept;
    void clear() noexcept;

private:
    // 频次 +1：把节点从 freq 桶搬到 freq+1 桶，并维护 min_freq_。
    void promote(iterator it, freq_type old_freq);

    // 淘汰频次最低、同频最久未用的节点；若最小频次桶因此变空，就地重算 min_freq_。
    void evict_lfu();

    // 重算最小非空频次（桶全空则置 0）；频次集合可能存在空洞。
    void recompute_min_freq();

    size_type capacity_;
    key_map    index_;          // key -> 链表迭代器
    freq_map   buckets_;        // freq -> 链表（同频节点）
    freq_type  min_freq_ = 0;   // 最小非空频次指针（0 表示空缓存）
};

} // namespace lfu

// 模板实现必须对实例化点可见，故在声明末尾包含实现文件。
#include "../src/lfu_cache.tpp"