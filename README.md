# C++ 缓存系统（LRU / LFU / ARC）

单机缓存组件练习项目：用哈希表 + 双向链表实现三种主流缓存淘汰算法，附单元测试与命中率基准。

## 算法概览

| 缓存 | 淘汰依据 | 核心结构 | 复杂度 |
|---|---|---|---|
| LRU | 最久未使用 | `unordered_map` + `list`（靠 `splice` 搬移） | get/put O(1) |
| LFU | 使用频次最低 | 频次桶 + 最小频次指针 | get/put 均摊 O(1) |
| ARC | 近期 / 频繁局部性自适应 | T1/T2 双链 + B1/B2 幽灵列表 + 指针 p | get/put 均摊 O(1) |

## 扩展组件

| 组件 | 说明 |
|---|---|
| `TTLCache` | 在 LRU 之上做惰性 TTL 过期（可注入时钟，便于测试） |
| `ShardedLRUCache` | 按 key 哈希分片 + 每片独立锁的线程安全 LRU |

## 目录结构

```
include/       头文件（cache.hpp / lfu_cache.hpp / arc_cache.hpp / ttl_cache.hpp / sharded_cache.hpp）
src/           模板实现片段（*.tpp，由对应头文件末尾 #include 引入）
tests/         单元测试
bench/         基准与算法对比
main.cpp       三种缓存的演示
CMakeLists.txt
```

## 构建与运行

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure   # 运行单元测试
./build/demo                                 # 三种缓存演示（Windows：.\build\demo.exe）
./build/bench                                # 命中率 / 耗时基准
cmake --build build --target report          # 生成 HTML 报告（需 Python 3）
```

无 CMake 时可用单文件编译验证（`main.cpp` 直接 include 头文件）：

```sh
g++ -std=c++17 -O2 -I include main.cpp -o demo
```

带 Sanitizer 调试（Linux / macOS；Windows MinGW 缺 ASan/UBSan 运行时）：

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCACHE_SANITIZE=address,undefined
cmake --build build && ctest --test-dir build --output-on-failure
```

## 说明

- 模板实现放在 `src/*.tpp`，由 `include/*.hpp` 末尾 `#include` 引入，因此**不要单独编译** `src/*.tpp`。
- 详细学习路线、AI 提示词与验收清单见 [01_C++_缓存系统.md](01_C++_缓存系统.md)。
- 实测结果图表（命中率 / 算法对比 / 并发吞吐）见 [cache-benchmark-report/cache-benchmark-report.html](cache-benchmark-report/cache-benchmark-report.html)（由 `tools/gen_report.py` 从基准程序输出**自动生成**，不再是手写数值）。
- CI：`.github/workflows/ci.yml`（gcc/clang 构建+测试、ASan+UBSan、TSan、Windows/MSVC）。
- ARC 自适应验证：`bench/compare_arc_adaptive.cpp` 直接观测指针 `p` 在「偏近期 / 偏频繁」两类负载下的分化，而不只是看命中率。
- 只读查询：各缓存都提供 `peek(key)`（不改变访问序 / 频次 / 局部性）与 `contains(key)`（同样只读）；`TTLCache` 的 `peek`/`contains` 不刷新访问序、不删除过期项（惰性删除只在 `get`）。
- 手动失效：各缓存都提供 `erase(key)`（LRU / LFU / ARC / TTL / Sharded）。
- 并发扩展基准：`bench/bench_sharded.cpp` 给出「分片数 × 线程数 → 吞吐」曲线（`demo` 也演示了全部 5 种缓存）。
- ARC 结构不变量由 `tests/test_arc_invariants.cpp` 的 fuzz 测试守护（`|T1|+|T2|≤c`、`|T1|+|B1|≤c`、`|T2|+|B2|≤c`、`p≤c` 等）。

## 后续可做

- TTL 主动清理（定期扫描，而非仅惰性删除）
- 分片缓存的并发性能基准；读写锁 / 无锁化探索
