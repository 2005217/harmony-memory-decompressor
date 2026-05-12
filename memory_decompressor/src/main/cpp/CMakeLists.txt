cmake_minimum_required(VERSION 3.5.0)
project(harmony_memory_decompressor)

set(NATIVERENDER_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})

if(DEFINED PACKAGE_FIND_FILE)
    include(${PACKAGE_FIND_FILE})
endif()

# zstd 库路径配置
set(ZSTD_INCLUDE_DIR ${NATIVERENDER_ROOT_PATH}/zstd/lib)

# 添加编译宏，解决平台兼容性问题
# 1. ZSTD_DISABLE_ASM - 禁用所有 zstd 汇编优化（避免 x86_64 汇编符号未定义）
# 2. HUF_DISABLE_ASM - 禁用 HUF 汇编优化（避免 x86_64 汇编在 ARM 上无法链接）
# 3. ZDICT_QSORT=ZDICT_QSORT_C90 - 使用标准 qsort 回退（避免 qsort_r 不可用）
# 4. ZSTD_LEGACY_SUPPORT=0 - 禁用旧版本支持，减少编译大小
add_definitions(
    -DZSTD_DISABLE_ASM
    -DHUF_DISABLE_ASM
    -DZDICT_QSORT=ZDICT_QSORT_C90
    -DZSTD_LEGACY_SUPPORT=0
)

# zstd 源文件 (仅使用 C 实现，避免 ARM 架构不兼容的汇编文件)
file(GLOB ZSTD_COMMON_SRC ${NATIVERENDER_ROOT_PATH}/zstd/lib/common/*.c)
file(GLOB ZSTD_COMPRESS_SRC ${NATIVERENDER_ROOT_PATH}/zstd/lib/compress/*.c)
file(GLOB ZSTD_DECOMPRESS_SRC ${NATIVERENDER_ROOT_PATH}/zstd/lib/decompress/*.c)
file(GLOB ZSTD_DICTBUILDER_SRC ${NATIVERENDER_ROOT_PATH}/zstd/lib/dictBuilder/*.c)

include_directories(${NATIVERENDER_ROOT_PATH}
                    ${NATIVERENDER_ROOT_PATH}/include
                    ${ZSTD_INCLUDE_DIR})

add_library(memory_decompressor SHARED 
    napi_init.cpp
    memory_decompressor.cpp
    ${ZSTD_COMMON_SRC}
    ${ZSTD_COMPRESS_SRC}
    ${ZSTD_DECOMPRESS_SRC}
    ${ZSTD_DICTBUILDER_SRC}
)

target_link_libraries(memory_decompressor PUBLIC libace_napi.z.so)