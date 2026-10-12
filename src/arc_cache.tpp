// src/arc_cache.tpp —— ARCCache 模板实现片段（T1/T2 双链表 + B1/B2 幽灵列表自适应调权）。
//
// 同 cache.tpp / lfu_cache.tpp：这是「实现片段」，在 include/arc_cache.hpp 末尾被包含，
// 不要单独编译，也不要重复 #include "arc_cache.hpp"。

namespace arc {

template <typename K, typename V>
ARCCache<K, V>::ARCCache(size_type capacity)
    : capacity_(capacity) {}

template <typename K, typename V>
bool ARCCache<K, V>::contains(const key_type& key) const {
    return t1_map_.find(key) != t1_map_.end() || t2_map_.find(key) != t2_map_.end();
}

template <typename K, typename V>
bool ARCCache<K, V>::erase(const key_type& key) {
    auto it1 = t1_map_.find(key);
    if (it1 != t1_map_.end()) {
        t1_.erase(it1->second);
        t1_map_.erase(it1);
        return true;
    }
    auto it2 = t2_map_.find(key);
    if (it2 != t2_map_.end()) {
        t2_.erase(it2->second);
        t2_map_.erase(it2);
        return true;
    }
    return false;   // 只有真正缓存着的 key（T1/T2）才算删除成功
}

template <typename K, typename V>
typename ARCCache<K, V>::size_type ARCCache<K, V>::size() const noexcept {
    return t1_.size() + t2_.size();
}

template <typename K, typename V>
typename ARCCache<K, V>::size_type ARCCache<K, V>::capacity() const noexcept {
    return capacity_;
}

template <typename K, typename V>
typename ARCCache<K, V>::Stats ARCCache<K, V>::stats() const noexcept {
    return Stats{p_, t1_.size(), t2_.size(), b1_.size(), b2_.size()};
}

template <typename K, typename V>
void ARCCache<K, V>::clear() noexcept {
    t1_.clear();
    t2_.clear();
    b1_.clear();
    b2_.clear();
    t1_map_.clear();
    t2_map_.clear();
    b1_map_.clear();
    b2_map_.clear();
    p_ = 0;
}

template <typename K, typename V>
std::optional<typename ARCCache<K, V>::value_type>
ARCCache<K, V>::get(const key_type& key) {
    auto it1 = t1_map_.find(key);
    if (it1 != t1_map_.end()) {
        entry_iter eit = it1->second;
        move_t1_to_t2(eit);   // 二次访问 → 升为「频繁」
        return eit->value;
    }

    auto it2 = t2_map_.find(key);
    if (it2 != t2_map_.end()) {
        entry_iter eit = it2->second;
        move_t2_front(eit);
        return eit->value;
    }

    return std::nullopt;
}

template <typename K, typename V>
std::optional<typename ARCCache<K, V>::value_type>
ARCCache<K, V>::peek(const key_type& key) const {
    auto it1 = t1_map_.find(key);
    if (it1 != t1_map_.end()) {
        return it1->second->value;   // 只读：T1 保持在 T1
    }
    auto it2 = t2_map_.find(key);
    if (it2 != t2_map_.end()) {
        return it2->second->value;
    }
    return std::nullopt;
}

template <typename K, typename V>
void ARCCache<K, V>::put(const key_type& key, const value_type& value) {
    if (capacity_ == 0) {
        return;
    }

    auto it1 = t1_map_.find(key);
    if (it1 != t1_map_.end()) {
        entry_iter eit = it1->second;
        eit->value = value;
        move_t1_to_t2(eit);
        return;
    }

    auto it2 = t2_map_.find(key);
    if (it2 != t2_map_.end()) {
        entry_iter eit = it2->second;
        eit->value = value;
        move_t2_front(eit);
        return;
    }

    // —— 未命中（需回源填充）——
    const bool in_b2 = b2_map_.find(key) != b2_map_.end();
    if (size() >= capacity_) {
        replace(in_b2);   // 先按自适应策略腾出位置
    }

    if (b1_map_.find(key) != b1_map_.end()) {
        // B1 幽灵命中：近期被淘汰的 key 又回来 → 增大 p（偏重近期 T1）
        const size_type delta = std::max<size_type>(1, b2_.size() / b1_.size());
        p_ = std::min(p_ + delta, capacity_);
        erase_b1(key);
        insert_t2(key, value);
        return;
    }

    if (in_b2) {
        // B2 幽灵命中：频繁项被淘汰后又回来 → 减小 p（偏重频繁 T2）
        const size_type delta = std::max<size_type>(1, b1_.size() / b2_.size());
        p_ = (p_ > delta) ? (p_ - delta) : 0;
        erase_b2(key);
        insert_t2(key, value);
        return;
    }

    // 全新 key → 进 T1
    insert_t1(key, value);
}

template <typename K, typename V>
void ARCCache<K, V>::replace(bool key_in_b2) {
    // 决定淘汰 T1 还是 T2：
    //   T1 非空，且（T2 空，或 B2 命中且 |T1|==p，或 |T1|>p）→ 淘汰 T1 尾部；
    //   否则淘汰 T2 尾部（此时 T2 必非空，见下方不变量）。
    const bool evict_t1 =
        !t1_.empty() &&
        (t2_.empty() || (key_in_b2 && t1_.size() == p_) || t1_.size() > p_);

    if (evict_t1) {
        key_type evicted = t1_.back().key;
        t1_map_.erase(evicted);
        t1_.pop_back();
        push_b1(evicted);
    } else {
        key_type evicted = t2_.back().key;
        t2_map_.erase(evicted);
        t2_.pop_back();
        push_b2(evicted);
    }
}

template <typename K, typename V>
void ARCCache<K, V>::move_t1_to_t2(entry_iter it) {
    const key_type key = it->key;
    // 先把迭代器登记进 T2 索引（可能抛 bad_alloc）；失败时 T1 尚未改动，状态一致。
    t2_map_.emplace(key, it);
    t1_map_.erase(key);                 // noexcept
    t2_.splice(t2_.begin(), t1_, it);   // O(1)、不分配、不失效迭代器
    trim_b2();                          // |T2| 增大，维护 |T2|+|B2| <= c
}

template <typename K, typename V>
void ARCCache<K, V>::move_t2_front(entry_iter it) {
    t2_.splice(t2_.begin(), t2_, it);
}

template <typename K, typename V>
void ARCCache<K, V>::insert_t1(const key_type& key, const value_type& value) {
    t1_.emplace_front(Entry{key, value});
    try {
        t1_map_.emplace(key, t1_.begin());
    } catch (...) {
        t1_.pop_front();   // 回滚：保持 T1 链表与其索引一致
        throw;
    }
    trim_b1();             // |T1| 增大，维护 |T1|+|B1| <= c
}

template <typename K, typename V>
void ARCCache<K, V>::insert_t2(const key_type& key, const value_type& value) {
    t2_.emplace_front(Entry{key, value});
    try {
        t2_map_.emplace(key, t2_.begin());
    } catch (...) {
        t2_.pop_front();   // 回滚：保持 T2 链表与其索引一致
        throw;
    }
    trim_b2();             // |T2| 增大，维护 |T2|+|B2| <= c
}

template <typename K, typename V>
void ARCCache<K, V>::push_b1(const key_type& key) {
    b1_.push_front(key);
    try {
        b1_map_[key] = b1_.begin();
    } catch (...) {
        b1_.pop_front();   // 回滚：保持幽灵链表与幽灵索引一致
        throw;
    }
    trim_b1();
}

template <typename K, typename V>
void ARCCache<K, V>::push_b2(const key_type& key) {
    b2_.push_front(key);
    try {
        b2_map_[key] = b2_.begin();
    } catch (...) {
        b2_.pop_front();   // 回滚：保持幽灵链表与幽灵索引一致
        throw;
    }
    trim_b2();
}

template <typename K, typename V>
void ARCCache<K, V>::erase_b1(const key_type& key) {
    auto it = b1_map_.find(key);
    if (it != b1_map_.end()) {
        b1_.erase(it->second);
        b1_map_.erase(it);
    }
}

template <typename K, typename V>
void ARCCache<K, V>::trim_b1() {
    // 规范化 ARC 边界：|T1| + |B1| <= c，超出则淘汰 B1 最久未用的幽灵。
    while (t1_.size() + b1_.size() > capacity_) {
        const key_type lru = b1_.back();
        b1_map_.erase(lru);
        b1_.pop_back();
    }
}

template <typename K, typename V>
void ARCCache<K, V>::trim_b2() {
    // 规范化 ARC 边界：|T2| + |B2| <= c。
    while (t2_.size() + b2_.size() > capacity_) {
        const key_type lru = b2_.back();
        b2_map_.erase(lru);
        b2_.pop_back();
    }
}

template <typename K, typename V>
void ARCCache<K, V>::erase_b2(const key_type& key) {
    auto it = b2_map_.find(key);
    if (it != b2_map_.end()) {
        b2_.erase(it->second);
        b2_map_.erase(it);
    }
}

} // namespace arc