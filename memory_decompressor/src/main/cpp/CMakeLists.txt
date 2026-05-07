cmake_minimum_required(VERSION 3.5.0)
project(harmony_memory_decompressor)

set(NATIVERENDER_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})

if(DEFINED PACKAGE_FIND_FILE)
    include(${PACKAGE_FIND_FILE})
endif()

# zstd 库路径配置
set(ZSTD_INCLUDE_DIR ${NATIVERENDER_ROOT_PATH}/zstd/lib)

# 添加 zstd 源文件
file(GLOB ZSTD_COMMON_SRC ${NATIVERENDER_ROOT_PATH}/zstd/lib/common/*.c)
file(GLOB ZSTD_COMPRESS_SRC ${NATIVERENDER_ROOT_PATH}/zstd/lib/compress/*.c)
file(GLOB ZSTD_DECOMPRESS_SRC ${NATIVERENDER_ROOT_PATH}/zstd/lib/decompress/*.c)
file(GLOB ZSTD_DECOMPRESS_ASM ${NATIVERENDER_ROOT_PATH}/zstd/lib/decompress/*.S)
file(GLOB ZSTD_DICTBUILDER_SRC ${NATIVERENDER_ROOT_PATH}/zstd/lib/dictBuilder/*.c)

include_directories(${NATIVERENDER_ROOT_PATH}
                    ${NATIVERENDER_ROOT_PATH}/include
                    ${ZSTD_INCLUDE_DIR})

add_library(memory_decompressor SHARED 
    napi_init.cpp
    ${ZSTD_COMMON_SRC}
    ${ZSTD_COMPRESS_SRC}
    ${ZSTD_DECOMPRESS_SRC}
    ${ZSTD_DECOMPRESS_ASM}
    ${ZSTD_DICTBUILDER_SRC}
)

target_link_libraries(memory_decompressor PUBLIC libace_napi.z.so)