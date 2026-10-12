#pragma once

#include <iostream>

// 极简测试辅助：共享计数器 + CHECK 宏，避免每份测试重复样板。
namespace cachetest {

inline int passed = 0;
inline int failed = 0;

inline int summary() {
    std::cout << "passed = " << passed << ", failed = " << failed << '\n';
    return failed == 0 ? 0 : 1;
}

} // namespace cachetest

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (cond) {                                                          \
            ++cachetest::passed;                                             \
        } else {                                                             \
            ++cachetest::failed;                                             \
            std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__ << ": "    \
                      << #cond << '\n';                                      \
        }                                                                    \
    } while (0)
