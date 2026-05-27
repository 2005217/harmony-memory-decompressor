# CMakeLists.txt
# 构建配置：告诉编译器如何编译 memory_decompressor 动态库
# 这个文件被 DevEco Studio 的 build-profile.json5 中的 externalNativeOptions 引用

cmake_minimum_required(VERSION 3.5.0)
project(harmony_memory_decompressor)

# NATIVERENDER_ROOT_PATH 指向当前目录（cpp/），所有源文件路径都基于此
set(NATIVERENDER_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})

if(DEFINED PACKAGE_FIND_FILE)
    include(${PACKAGE_FIND_FILE})
endif()

# ===== ZSTD 配置 =====
# ZSTD 头文件路径
set(ZSTD_INCLUDE_DIR ${NATIVERENDER_ROOT_PATH}/zstd/lib)

# ZSTD 编译宏定义
# ZSTD_DISABLE_ASM / HUF_DISABLE_ASM：禁用汇编优化（HarmonyOS 兼容性）
# ZSTD_LEGACY_SUPPORT=0：不编译旧版本兼容代码，减小体积
add_definitions(
    -DZSTD_DISABLE_ASM
    -DHUF_DISABLE_ASM
    -DZDICT_QSORT=ZDICT_QSORT_C90
    -DZSTD_LEGACY_SUPPORT=0
)

# 用 GLOB 自动收集 ZSTD 各模块的 .c 源文件
file(GLOB ZSTD_COMMON_SRC ${NATIVERENDER_ROOT_PATH}/zstd/lib/common/*.c)
file(GLOB ZSTD_COMPRESS_SRC ${NATIVERENDER_ROOT_PATH}/zstd/lib/compress/*.c)
file(GLOB ZSTD_DECOMPRESS_SRC ${NATIVERENDER_ROOT_PATH}/zstd/lib/decompress/*.c)
file(GLOB ZSTD_DICTBUILDER_SRC ${NATIVERENDER_ROOT_PATH}/zstd/lib/dictBuilder/*.c)

# ===== LZ4 配置 =====
# LZ4 头文件路径（lz4.h、lz4frame.h、lz4hc.h、xxhash.h）
set(LZ4_INCLUDE_DIR ${NATIVERENDER_ROOT_PATH}/lz4)

# ===== SNAPPY 配置 =====
# Snappy 头文件路径
set(SNAPPY_INCLUDE_DIR ${NATIVERENDER_ROOT_PATH}/snappy)

# 包含头文件路径，让编译器能找到 #include 的文件
include_directories(${NATIVERENDER_ROOT_PATH}
                    ${NATIVERENDER_ROOT_PATH}/include
                    ${ZSTD_INCLUDE_DIR}
                    ${LZ4_INCLUDE_DIR}
                    ${SNAPPY_INCLUDE_DIR}
                    ${CMAKE_CURRENT_BINARY_DIR}/generated)

# Snappy 需要 snappy-stubs-public.h 但官方用 configure_file() 生成
# 这里用 file(WRITE) 在构建目录自动生成，避免手动创建文件
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/generated/snappy-stubs-public.h
"#ifndef THIRD_PARTY_SNAPPY_OPENSOURCE_SNAPPY_STUBS_PUBLIC_H_
#define THIRD_PARTY_SNAPPY_OPENSOURCE_SNAPPY_STUBS_PUBLIC_H_
#include <cstddef>
#define SNAPPY_MAJOR 1
#define SNAPPY_MINOR 2
#define SNAPPY_PATCHLEVEL 2
#define SNAPPY_VERSION ((1 << 16) | (2 << 8) | 2)
namespace snappy {
struct iovec { void* iov_base; size_t iov_len; };
}
#endif")

# 编译为动态库（.so 文件），供 ArkTS 通过 NAPI 调用
add_library(memory_decompressor SHARED 
    napi_init.cpp              # NAPI 绑定：桥接 ArkTS 和 C++
    memory_decompressor.cpp    # C++ 包装器：统一封装 ZSTD/LZ4
    ${ZSTD_COMMON_SRC}         # ZSTD 公共模块
    ${ZSTD_COMPRESS_SRC}       # ZSTD 压缩模块
    ${ZSTD_DECOMPRESS_SRC}     # ZSTD 解压模块
    ${ZSTD_DICTBUILDER_SRC}    # ZSTD 字典构建模块
    ${NATIVERENDER_ROOT_PATH}/lz4/lz4.c       # LZ4 核心
    ${NATIVERENDER_ROOT_PATH}/lz4/lz4hc.c     # LZ4 高压缩比
    ${NATIVERENDER_ROOT_PATH}/lz4/lz4frame.c  # LZ4 Frame 格式
    ${NATIVERENDER_ROOT_PATH}/lz4/xxhash.c    # 校验和算法
    ${NATIVERENDER_ROOT_PATH}/snappy/snappy.cc      # Snappy 核心算法
    ${NATIVERENDER_ROOT_PATH}/snappy/snappy-c.cc    # Snappy C API 包装
    ${NATIVERENDER_ROOT_PATH}/snappy/snappy-sinksource.cc  # Snappy 源/汇实现（ByteArraySource 等）
)

# 链接 NAPI 系统库，否则无法使用 napi_xxx 函数
target_link_libraries(memory_decompressor PUBLIC libace_napi.z.so)