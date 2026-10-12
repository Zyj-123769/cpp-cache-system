// src/cache.tpp —— LRUCache 模板实现片段。
//
// 重要：本文件是「实现片段」，而非独立编译单元。
// 它在 include/cache.hpp 末尾通过 #include "../src/cache.tpp" 被包含，
// 这样模板定义对每个实例化点（main.cpp、tests/*.cpp）都可见。
// 因此：不要单独编译本文件，也不要在这里重复 #include "cache.hpp"。

namespace lru {

// ---------------------------------------------------------------------------
// 构造 / 容量 / 大小 / 清空（Phase 1 直接实现，属于接口的一部分）
// ---------------------------------------------------------------------------

template <typename K, typename V>
LRUCache<K, V>::LRUCache(size_type capacity)
    : capacity_(capacity) {
    // capacity == 0 是合法态，见 get/put 的 Phase 2 处理。
}

template <typename K, typename V>
bool LRUCache<K, V>::contains(const key_type& key) const {
    return index_.find(key) != index_.end();
}

template <typename K, typename V>
typename LRUCache<K, V>::size_type LRUCache<K, V>::size() const noexcept {
    return index_.size();
}

template <typename K, typename V>
typename LRUCache<K, V>::size_type LRUCache<K, V>::capacity() const noexcept {
    return capacity_;
}

template <typename K, typename V>
void LRUCache<K, V>::clear() noexcept {
    order_.clear();
    index_.clear();
}

// ---------------------------------------------------------------------------
// 核心接口（Phase 2：O(1) 实现）
// ---------------------------------------------------------------------------

template <typename K, typename V>
std::optional<typename LRUCache<K, V>::value_type>
LRUCache<K, V>::get(const key_type& key) {
    // O(1) 哈希查找
    auto found = index_.find(key);
    if (found == index_.end()) {
        return std::nullopt;
    }

    iterator it = found->second;
    touch(it);          // splice 移到头部，不失效迭代器
    return it->value;
}

template <typename K, typename V>
std::optional<typename LRUCache<K, V>::value_type>
LRUCache<K, V>::peek(const key_type& key) const {
    auto found = index_.find(key);
    if (found == index_.end()) {
        return std::nullopt;
    }
    return found->second->value;   // 只读，不改变访问顺序
}

template <typename K, typename V>
void LRUCache<K, V>::put(const key_type& key, const value_type& value) {
    if (capacity_ == 0) {
        // 禁用缓存：写入即被淘汰，等价于不保留任何条目
        return;
    }

    auto found = index_.find(key);
    if (found != index_.end()) {
        // 已存在：更新值并移到头部
        iterator it = found->second;
        it->value = value;
        touch(it);
        return;
    }

    // 不存在：头部插入新节点并登记到哈希表
    order_.emplace_front(Node{key, value});
    try {
        index_.emplace(key, order_.begin());
    } catch (...) {
        order_.pop_front();   // 回滚：保持链表与哈希表条目一致
        throw;
    }

    // 超容量：淘汰链表尾部（最久未使用）
    if (index_.size() > capacity_) {
        evict_lru();
    }
}

template <typename K, typename V>
bool LRUCache<K, V>::erase(const key_type& key) {
    auto found = index_.find(key);
    if (found == index_.end()) {
        return false;
    }
    order_.erase(found->second);   // 先删链表节点
    index_.erase(found);           // 再删哈希表条目（用迭代器，避免二次哈希）
    return true;
}

template <typename K, typename V>
void LRUCache<K, V>::touch(iterator it) {
    // 用 splice 把节点整体搬移到头部，O(1) 且不产生二次分配、不失效其他迭代器；
    // 避免 erase + insert 的实现方式。
    order_.splice(order_.begin(), order_, it);
}

template <typename K, typename V>
void LRUCache<K, V>::evict_lru() {
    // 链表尾即最久未使用：先按 key 删除哈希表条目，再删链表节点，保持二者同步
    index_.erase(order_.back().key);
    order_.pop_back();
}

} // namespace lru