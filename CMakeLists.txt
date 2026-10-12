cmake_minimum_required(VERSION 3.16)

project(cache_system
    VERSION 1.0.0
    DESCRIPTION "LRU / LFU / ARC 缓存淘汰算法练习项目"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 未指定构建类型时默认 Release（基准测试需要优化，否则数据没有参考意义）。
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# 核心实现是「模板 + 头文件」：include/*.hpp 末尾包含 src/*.tpp 片段，
# 因此这里只需要把 include/ 暴露给使用方，无需编译任何源文件。
add_library(cache_core INTERFACE)
target_include_directories(cache_core INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(cache_core INTERFACE cxx_std_17)

# 可选 Sanitizer：-DCACHE_SANITIZE=address,undefined（或 thread）。留空则不启用。
set(CACHE_SANITIZE "" CACHE STRING "逗号分隔的 Sanitizer 列表：address,undefined,thread；留空关闭")
if(CACHE_SANITIZE)
    if(MSVC)
        message(WARNING "MSVC 不支持 -fsanitize，CACHE_SANITIZE 已忽略")
    else()
        string(REPLACE "," ";" _cache_sans "${CACHE_SANITIZE}")
        foreach(_s IN LISTS _cache_sans)
            target_compile_options(cache_core INTERFACE -fsanitize=${_s} -fno-omit-frame-pointer -g)
            target_link_options(cache_core INTERFACE -fsanitize=${_s})
        endforeach()
    endif()
endif()

# 统一告警开关（GCC / Clang / MSVC 通用）。
function(cache_add_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra)
    endif()
endfunction()

# 演示程序
add_executable(demo main.cpp)
target_link_libraries(demo PRIVATE cache_core)
cache_add_warnings(demo)

# 单元测试
set(CACHE_TESTS
    test_cache
    test_lfu_cache
    test_arc_cache
    test_model
    test_complexity
    test_edge
    test_arc_invariants
    test_ttl_cache
    test_sharded_cache)
foreach(t IN LISTS CACHE_TESTS)
    add_executable(${t} tests/${t}.cpp)
    target_link_libraries(${t} PRIVATE cache_core)
    cache_add_warnings(${t})
endforeach()

# 分片缓存测试用到多线程。
find_package(Threads REQUIRED)
target_link_libraries(test_sharded_cache PRIVATE Threads::Threads)

# 基准 / 对比程序
set(CACHE_BENCHES bench compare_lru_lfu compare_arc compare_arc_phases compare_arc_adaptive bench_sharded)
foreach(b IN LISTS CACHE_BENCHES)
    add_executable(${b} bench/${b}.cpp)
    target_link_libraries(${b} PRIVATE cache_core)
    cache_add_warnings(${b})
endforeach()

# 分片并发吞吐基准用到多线程。
target_link_libraries(bench_sharded PRIVATE Threads::Threads)

# 接入 CTest：ctest 会依次运行全部单元测试。
enable_testing()
foreach(t IN LISTS CACHE_TESTS)
    add_test(NAME ${t} COMMAND ${t})
endforeach()

# 由基准程序的实际输出自动生成 HTML 报告（需要 Python 3；找不到则跳过该目标）。
unset(CACHE_PYTHON CACHE)   # 重新探测，避免缓存到 WindowsApps 的 store 占位程序
find_program(CACHE_PYTHON NAMES py python3 python)
if(CACHE_PYTHON)
    add_custom_target(report
        COMMAND ${CACHE_PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_report.py
                --build-dir $<TARGET_FILE_DIR:bench>
        DEPENDS bench compare_lru_lfu compare_arc compare_arc_phases bench_sharded
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "生成报告：cache-benchmark-report/cache-benchmark-report.html"
        VERBATIM)
else()
    message(STATUS "未找到 Python，跳过 `report` 目标（可手动运行 tools/gen_report.py）")
endif()
