#include <cstddef>
#include <list>
#include <optional>
#include <random>
#include <unordered_map>

#include "cache.hpp"
#include "lfu_cache.hpp"
#include "test_common.hpp"

namespace {

// ---- 暴力参考 LRU：语义对齐 LRUCache（get 命中即提升为最近使用）----
class RefLRU {
public:
    explicit RefLRU(std::size_t cap) : cap_(cap) {}

    std::optional<int> get(int k) {
        auto it = index_.find(k);
        if (it == index_.end()) {
            return std::nullopt;
        }
        order_.erase(it->second);
        order_.push_front(k);
        index_[k] = order_.begin();
        return value_[k];
    }

    void put(int k, int v) {
        if (cap_ == 0) {
            return;
        }
        auto it = index_.find(k);
        if (it != index_.end()) {
            value_[k] = v;
            order_.erase(it->second);
            order_.push_front(k);
            index_[k] = order_.begin();
            return;
        }
        order_.push_front(k);
        index_[k] = order_.begin();
        value_[k] = v;
        if (index_.size() > cap_) {
            const int victim = order_.back();
            order_.pop_back();
            index_.erase(victim);
            value_.erase(victim);
        }
    }

    bool contains(int k) const { return index_.count(k) != 0; }
    std::size_t size() const { return index_.size(); }

private:
    std::size_t cap_;
    std::list<int> order_;
    std::unordered_map<int, std::list<int>::iterator> index_;
    std::unordered_map<int, int> value_;
};

// ---- 暴力参考 LFU：淘汰「频次最低、同频最久未提升」的 key ----
class RefLFU {
public:
    explicit RefLFU(std::size_t cap) : cap_(cap) {}

    std::optional<int> get(int k) {
        auto it = freq_.find(k);
        if (it == freq_.end()) {
            return std::nullopt;
        }
        it->second += 1;
        seq_[k] = ++clock_;
        return value_[k];
    }

    void put(int k, int v) {
        if (cap_ == 0) {
            return;
        }
        auto it = freq_.find(k);
        if (it != freq_.end()) {
            value_[k] = v;
            it->second += 1;
            seq_[k] = ++clock_;
            return;
        }
        if (freq_.size() >= cap_) {
            int victim = -1;
            long best_freq = 0;
            long best_seq = 0;
            bool first = true;
            for (const auto& entry : freq_) {
                const int key = entry.first;
                const long f = entry.second;
                const long s = seq_[key];
                if (first || f < best_freq || (f == best_freq && s < best_seq)) {
                    first = false;
                    best_freq = f;
                    best_seq = s;
                    victim = key;
                }
            }
            freq_.erase(victim);
            seq_.erase(victim);
            value_.erase(victim);
        }
        freq_[k] = 1;
        seq_[k] = ++clock_;
        value_[k] = v;
    }

    bool contains(int k) const { return freq_.count(k) != 0; }
    std::size_t size() const { return freq_.size(); }

private:
    std::size_t cap_;
    long clock_ = 0;   // 单调递增的「最近提升」序号
    std::unordered_map<int, long> freq_;
    std::unordered_map<int, long> seq_;
    std::unordered_map<int, int> value_;
};

const std::size_t KEY_SPACE = 50;
const int         OPS       = 20000;

void check_membership_lru(lru::LRUCache<int, int>& c, const RefLRU& ref) {
    CHECK(c.size() == ref.size());
    for (int k = 0; k < static_cast<int>(KEY_SPACE); ++k) {
        CHECK(c.contains(k) == ref.contains(k));
    }
}

void check_membership_lfu(lfu::LFUCache<int, int>& c, const RefLFU& ref) {
    CHECK(c.size() == ref.size());
    for (int k = 0; k < static_cast<int>(KEY_SPACE); ++k) {
        CHECK(c.contains(k) == ref.contains(k));
    }
}

void test_lru_matches_reference() {
    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> key(0, static_cast<int>(KEY_SPACE) - 1);
    std::uniform_int_distribution<int> val(0, 1 << 20);
    std::uniform_int_distribution<int> op(0, 99);

    for (std::size_t cap : {std::size_t(1), std::size_t(2), std::size_t(3),
                            std::size_t(7), std::size_t(16)}) {
        lru::LRUCache<int, int> c(cap);
        RefLRU ref(cap);
        for (int i = 0; i < OPS; ++i) {
            const int k = key(rng);
            if (op(rng) < 60) {
                CHECK(c.get(k) == ref.get(k));   // 逐步比对命中结果
            } else {
                const int v = val(rng);
                c.put(k, v);
                ref.put(k, v);
            }
        }
        check_membership_lru(c, ref);
    }
}

void test_lfu_matches_reference() {
    std::mt19937 rng(67890);
    std::uniform_int_distribution<int> key(0, static_cast<int>(KEY_SPACE) - 1);
    std::uniform_int_distribution<int> val(0, 1 << 20);
    std::uniform_int_distribution<int> op(0, 99);

    for (std::size_t cap : {std::size_t(1), std::size_t(2), std::size_t(3),
                            std::size_t(7), std::size_t(16)}) {
        lfu::LFUCache<int, int> c(cap);
        RefLFU ref(cap);
        for (int i = 0; i < OPS; ++i) {
            const int k = key(rng);
            if (op(rng) < 60) {
                CHECK(c.get(k) == ref.get(k));
            } else {
                const int v = val(rng);
                c.put(k, v);
                ref.put(k, v);
            }
        }
        check_membership_lfu(c, ref);
    }
}

} // namespace

int main() {
    test_lru_matches_reference();
    test_lfu_matches_reference();
    return cachetest::summary();
}
