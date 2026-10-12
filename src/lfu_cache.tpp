// src/lfu_cache.tpp —— LFUCache 模板实现片段（频次桶 + 最小频次指针，均摊 O(1)）。
//
// 同 src/cache.tpp：这是「实现片段」，在 include/lfu_cache.hpp 末尾被包含，
// 不要单独编译，也不要重复 #include "lfu_cache.hpp"。

namespace lfu {

template <typename K, typename V>
LFUCache<K, V>::LFUCache(size_type capacity)
    : capacity_(capacity) {}

template <typename K, typename V>
bool LFUCache<K, V>::contains(const key_type& key) const {
    return index_.find(key) != index_.end();
}

template <typename K, typename V>
typename LFUCache<K, V>::size_type LFUCache<K, V>::size() const noexcept {
    return index_.size();
}

template <typename K, typename V>
typename LFUCache<K, V>::size_type LFUCache<K, V>::capacity() const noexcept {
    return capacity_;
}

template <typename K, typename V>
void LFUCache<K, V>::clear() noexcept {
    index_.clear();
    buckets_.clear();
    min_freq_ = 0;
}

template <typename K, typename V>
std::optional<typename LFUCache<K, V>::value_type>
LFUCache<K, V>::get(const key_type& key) {
    auto found = index_.find(key);
    if (found == index_.end()) {
        return std::nullopt;
    }

    iterator it = found->second;
    promote(it, it->freq);   // 频次 +1（会把节点搬到新的频次桶）
    return it->value;
}

template <typename K, typename V>
std::optional<typename LFUCache<K, V>::value_type>
LFUCache<K, V>::peek(const key_type& key) const {
    auto found = index_.find(key);
    if (found == index_.end()) {
        return std::nullopt;
    }
    return found->second->value;   // 只读，不增加频次
}

template <typename K, typename V>
void LFUCache<K, V>::put(const key_type& key, const value_type& value) {
    if (capacity_ == 0) {
        return;   // 禁用缓存：写入即淘汰
    }

    auto found = index_.find(key);
    if (found != index_.end()) {
        iterator it = found->second;
        it->value = value;
        promote(it, it->freq);
        return;
    }

    // 新 key：容量已满先淘汰，再插入 freq=1
    if (index_.size() >= capacity_) {
        evict_lfu();
    }

    auto& lst = buckets_[1];
    lst.emplace_back(Node{key, value, 1});   // 同频内插尾部（视作最新）
    try {
        index_.emplace(key, std::prev(lst.end()));
    } catch (...) {
        lst.pop_back();   // 回滚：保持频次桶与索引一致
        throw;
    }
    min_freq_ = 1;   // 新插入的 key 频次恒为 1，一定是最小频次
}

template <typename K, typename V>
void LFUCache<K, V>::promote(iterator it, freq_type old_freq) {
    auto& src = buckets_[old_freq];
    auto& dst = buckets_[old_freq + 1];

    // 用 splice 把节点从一个频次桶整体搬到下一个桶，O(1) 且迭代器保持有效。
    dst.splice(dst.end(), src, it);
    it->freq = old_freq + 1;

    // 频次桶空指针更新：只有当「被腾空的桶」恰好是最小频次桶时，最小频次才 +1。
    if (src.empty()) {
        buckets_.erase(old_freq);
        if (min_freq_ == old_freq) {
            min_freq_ = old_freq + 1;
        }
    }
}

template <typename K, typename V>
void LFUCache<K, V>::evict_lfu() {
    auto& lst = buckets_[min_freq_];
    iterator victim = lst.begin();   // 同频内头部 = 最久未用

    index_.erase(victim->key);
    lst.pop_front();

    if (!lst.empty()) {
        return;   // 最小频次桶仍有节点，min_freq_ 不变
    }

    // 最小频次桶已空：删桶并重算 min_freq_（频次集合可能有空洞，不能简单 +1）。
    // 这样 evict_lfu 自身闭环，不依赖「调用方随后把 min_freq_ 重置为 1」的隐含约定。
    buckets_.erase(min_freq_);
    recompute_min_freq();
}

template <typename K, typename V>
void LFUCache<K, V>::recompute_min_freq() {
    freq_type next_min = 0;
    bool found = false;
    for (const auto& entry : buckets_) {
        if (!entry.second.empty() && (!found || entry.first < next_min)) {
            next_min = entry.first;
            found = true;
        }
    }
    min_freq_ = found ? next_min : 0;
}

template <typename K, typename V>
bool LFUCache<K, V>::erase(const key_type& key) {
    auto found = index_.find(key);
    if (found == index_.end()) {
        return false;
    }
    iterator it = found->second;
    const freq_type f = it->freq;

    auto bucket = buckets_.find(f);
    if (bucket != buckets_.end()) {
        bucket->second.erase(it);
        if (bucket->second.empty()) {
            buckets_.erase(bucket);
            if (min_freq_ == f) {
                recompute_min_freq();
            }
        }
    }
    index_.erase(found);
    return true;
}

} // namespace lfu