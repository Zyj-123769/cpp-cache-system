#pragma once
#include <cstddef>
#include <list>
#include <optional>
#include <unordered_map>

namespace lru {

// 容量受限的 LRU 缓存（模板类声明）。
//
// 核心结构：
//   - std::list<Node> order_                  : 双向链表，头部=最近使用，尾部=最久未使用；
//   - std::unordered_map<K, list::iterator>   : key -> 链表节点迭代器，负责 O(1) 定位。
template <typename K, typename V>
class LRUCache {
public:
    using key_type   = K;
    using value_type = V;
    using size_type  = std::size_t;

    // 链表节点：同时保存 key 与 value。
    // 淘汰尾部节点时，靠 key 反查 index_ 并删除对应哈希表条目。
    struct Node {
        key_type   key;
        value_type value;
    };

    using list_type = std::list<Node>;
    using iterator  = typename list_type::iterator;
    using map_type  = std::unordered_map<key_type, iterator>;

    // 构造缓存。capacity == 0 表示禁用缓存（get 恒未命中、put 立即淘汰）。
    explicit LRUCache(size_type capacity);

    // 查询：命中则把节点移动到头部并返回 value；未命中返回 std::nullopt。
    std::optional<value_type> get(const key_type& key);

    // 查询但不改变访问顺序（const）：命中返回 value，未命中返回 std::nullopt。
    std::optional<value_type> peek(const key_type& key) const;

    // 写入：已存在则更新并移到头部；否则插入头部，超容量时淘汰尾部。
    void put(const key_type& key, const value_type& value);

    // 删除指定 key；返回是否删除成功（key 不存在返回 false）。
    bool erase(const key_type& key);

    // 是否存在（不改变访问顺序）。
    bool contains(const key_type& key) const;

    // 当前条目数 / 容量。
    size_type size() const noexcept;
    size_type capacity() const noexcept;

    // 清空缓存。
    void clear() noexcept;

private:
    // 把节点移到链表头部（视为最近使用）。
    void touch(iterator it);

    // 淘汰链表尾部（最久未使用），并同步删除哈希表条目。
    void evict_lru();

    size_type capacity_;
    list_type  order_;   // 头部 = 最近使用，尾部 = 最久未使用
    map_type   index_;   // key -> 链表迭代器
};

} // namespace lru

// 模板实现必须对实例化点可见，故在声明末尾包含实现文件，
// 从而让每个包含本头文件的翻译单元都能看到完整定义。
#include "../src/cache.tpp"