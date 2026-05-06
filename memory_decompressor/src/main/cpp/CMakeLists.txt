# the minimum version of CMake.
cmake_minimum_required(VERSION 3.5.0)
project(harmony_memory_decompressor)

set(NATIVERENDER_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})

if(DEFINED PACKAGE_FIND_FILE)
    include(${PACKAGE_FIND_FILE})
endif()

include_directories(${NATIVERENDER_ROOT_PATH}
                    ${NATIVERENDER_ROOT_PATH}/include)

add_library(memory_decompressor SHARED napi_init.cpp)
target_link_libraries(memory_decompressor PUBLIC libace_napi.z.so)