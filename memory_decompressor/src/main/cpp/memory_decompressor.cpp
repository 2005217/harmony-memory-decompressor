// memory_decompressor.cpp
// C++ 包装器实现：统一封装 ZSTD 和 LZ4 的 C API
// 调用方（napi_init.cpp）不需要关心底层用的是哪个算法

#include "memory_decompressor.h"
#include "zstd.h"
#include "lz4frame.h"
#include "lz4.h"
#include <cstring>
#include <algorithm>
#include <sstream>
#include "snappy-c.h"

namespace md_compressor {

// ============================================================
// 对外统一接口：compress
// 根据 options.format 自动路由到 ZSTD 或 LZ4 的实现
// ============================================================
CompressionResult MemoryDecompressor::compress(const uint8_t* data, size_t size, const CompressionOptions& options) {
    CompressionResult result;
    result.original_size = size;

    switch (options.format) {
        case CompressionFormat::ZSTD:
            return compress_zstd(data, size, options);
        case CompressionFormat::LZ4:
            return compress_lz4(data, size, options);
        case CompressionFormat::SNAPPY:
            return compress_snappy(data, size, options);
        default:
            result.success = false;
            result.error_message = "Unsupported compression format";
            return result;
    }
}

// ============================================================
// 对外统一接口：decompress
// 如果 format 为 UNKNOWN，先通过魔数自动检测格式
// ============================================================
CompressionResult MemoryDecompressor::decompress(const uint8_t* data, size_t size, CompressionFormat format) {
    CompressionResult result;

    if (format == CompressionFormat::UNKNOWN) {
        format = detect_format(data, size);
    }

    switch (format) {
        case CompressionFormat::ZSTD:
            return decompress_zstd(data, size);
        case CompressionFormat::LZ4:
            return decompress_lz4(data, size);
        case CompressionFormat::SNAPPY:
            return decompress_snappy(data, size);
        default:
            result.success = false;
            result.error_message = "Unsupported or unknown compression format";
            return result;
    }
}

// ============================================================
// 工具函数
// ============================================================

// 返回当前支持的压缩格式列表
std::vector<CompressionFormat> MemoryDecompressor::get_supported_formats() {
    return {CompressionFormat::ZSTD, CompressionFormat::LZ4, CompressionFormat::SNAPPY};
}

// 将格式枚举转为可读的字符串
std::string MemoryDecompressor::get_format_name(CompressionFormat format) {
    switch (format) {
        case CompressionFormat::ZSTD:
            return "ZSTD";
        case CompressionFormat::LZ4:
            return "LZ4";
        case CompressionFormat::SNAPPY:
            return "SNAPPY";
        case CompressionFormat::UNKNOWN:
            return "UNKNOWN";
        default:
            return "UNKNOWN";
    }
}

// 通过前 4 字节的魔数检测压缩格式
// ZSTD 魔数：0xFD2FB528
// LZ4 魔数：0x184D2204
CompressionFormat MemoryDecompressor::detect_format(const uint8_t* data, size_t size) {
    if (size < 4) {
        return CompressionFormat::UNKNOWN;
    }

    uint32_t magic = 0;
    std::memcpy(&magic, data, sizeof(uint32_t));

    if (magic == 0xFD2FB528) {
        return CompressionFormat::ZSTD;
    }

    if (magic == 0x184D2204) {
        return CompressionFormat::LZ4;
    }

    return CompressionFormat::UNKNOWN;
}

// 获取版本信息字符串
std::string MemoryDecompressor::get_version() {
    std::string zstd_version = ZSTD_versionString();
    std::string lz4_version = LZ4_versionString();
    std::stringstream ss;
    ss << "MemoryDecompressor v1.0.0 (ZSTD " << zstd_version
       << ", LZ4 " << lz4_version
       << ", Snappy 1.2.2)";
    return ss.str();
}

// ============================================================
// ZSTD 压缩实现
// 使用 ZSTD 的一键式 API：ZSTD_compress
// ============================================================
CompressionResult MemoryDecompressor::compress_zstd(const uint8_t* data, size_t size, const CompressionOptions& options) {
    CompressionResult result;
    result.original_size = size;

    // ZSTD_compressBound 返回压缩后最大可能的大小
    size_t max_compressed_size = ZSTD_compressBound(size);
    if (max_compressed_size == 0) {
        result.success = false;
        result.error_message = "Invalid source size for ZSTD compression";
        return result;
    }

    result.data.resize(max_compressed_size);

    // 执行压缩：一次调用完成所有工作
    size_t compressed_size = ZSTD_compress(
        result.data.data(), max_compressed_size,
        data, size,
        options.level
    );

    if (ZSTD_isError(compressed_size)) {
        result.success = false;
        result.error_message = ZSTD_getErrorName(compressed_size);
        result.data.clear();
        return result;
    }

    // 裁剪到实际大小
    result.data.resize(compressed_size);
    result.compressed_size = compressed_size;

    if (size > 0) {
        result.ratio = static_cast<double>(compressed_size) / static_cast<double>(size);
    }

    result.success = true;
    return result;
}

// ============================================================
// ZSTD 解压实现
// 先通过 ZSTD_getFrameContentSize 获取原始大小，再解压
// ============================================================
CompressionResult MemoryDecompressor::decompress_zstd(const uint8_t* data, size_t size) {
    CompressionResult result;

    // 从帧头读取原始数据大小
    unsigned long long decompressed_size = ZSTD_getFrameContentSize(data, size);

    if (decompressed_size == ZSTD_CONTENTSIZE_UNKNOWN) {
        result.success = false;
        result.error_message = "Cannot determine decompressed size: unknown";
        return result;
    }

    if (decompressed_size == ZSTD_CONTENTSIZE_ERROR) {
        result.success = false;
        result.error_message = "Cannot determine decompressed size: error";
        return result;
    }

    result.data.resize(decompressed_size);

    // 执行解压
    size_t actual_decompressed_size = ZSTD_decompress(
        result.data.data(), decompressed_size,
        data, size
    );

    if (ZSTD_isError(actual_decompressed_size)) {
        result.success = false;
        result.error_message = ZSTD_getErrorName(actual_decompressed_size);
        result.data.clear();
        return result;
    }

    if (actual_decompressed_size != decompressed_size) {
        result.success = false;
        result.error_message = "Decompressed size mismatch";
        result.data.clear();
        return result;
    }

    result.compressed_size = size;
    result.original_size = actual_decompressed_size;

    if (actual_decompressed_size > 0) {
        result.ratio = static_cast<double>(size) / static_cast<double>(actual_decompressed_size);
    }

    result.success = true;
    return result;
}

// ============================================================
// LZ4 压缩实现
// 使用 LZ4 Frame API 的 LZ4F_compressFrame（一键式）
// Frame 格式自包含元数据（原始大小、校验和），解压时无需额外参数
// ============================================================
CompressionResult MemoryDecompressor::compress_lz4(const uint8_t* data, size_t size, const CompressionOptions& options) {
    CompressionResult result;
    result.original_size = size;

    // 配置压缩参数
    LZ4F_preferences_t prefs = LZ4F_INIT_PREFERENCES;
    if (options.level > 0) {
        prefs.compressionLevel = options.level;
    }

    // LZ4F_compressFrameBound 返回压缩后最大可能的大小
    size_t max_size = LZ4F_compressFrameBound(size, &prefs);
    result.data.resize(max_size);

    // 执行压缩：一次调用完成帧头+数据+帧尾
    size_t compressed_size = LZ4F_compressFrame(
        result.data.data(), max_size,
        data, size,
        &prefs
    );

    if (LZ4F_isError(compressed_size)) {
        result.success = false;
        result.error_message = LZ4F_getErrorName(compressed_size);
        result.data.clear();
        return result;
    }

    result.data.resize(compressed_size);
    result.compressed_size = compressed_size;

    if (size > 0) {
        result.ratio = static_cast<double>(compressed_size) / static_cast<double>(size);
    }

    result.success = true;
    return result;
}

// ============================================================
// LZ4 解压实现
// 使用 LZ4 Frame API 流式解压（while 循环处理多个数据块）
// ============================================================
CompressionResult MemoryDecompressor::decompress_lz4(const uint8_t* data, size_t size) {
    CompressionResult result;

    // 1. 创建解压上下文
    LZ4F_dctx* dctx = nullptr;
    LZ4F_errorCode_t err = LZ4F_createDecompressionContext(&dctx, LZ4F_VERSION);
    if (LZ4F_isError(err)) {
        result.success = false;
        result.error_message = LZ4F_getErrorName(err);
        return result;
    }

    // 2. 读取帧头，获取帧信息（包括原始大小）
    LZ4F_frameInfo_t frame_info = LZ4F_INIT_FRAMEINFO;
    size_t src_pos = size;
    size_t hint = LZ4F_getFrameInfo(dctx, &frame_info, data, &src_pos);

    if (LZ4F_isError(hint)) {
        LZ4F_freeDecompressionContext(dctx);
        result.success = false;
        result.error_message = LZ4F_getErrorName(hint);
        return result;
    }

    // 3. 确定输出缓冲区大小
    size_t decompressed_size = 0;
    if (frame_info.contentSize > 0) {
        decompressed_size = (size_t)frame_info.contentSize;
    } else {
        decompressed_size = size * 3;
    }

    result.data.resize(decompressed_size);

    // 4. 循环解压每个数据块
    // hint != 0 表示还有数据块需要解压
    size_t src_offset = src_pos;
    size_t dst_offset = 0;

    while (hint != 0) {
        size_t src_size = size - src_offset;
        size_t dst_size = result.data.size() - dst_offset;

        // 如果输出缓冲区不够，自动扩容
        if (dst_size == 0) {
            result.data.resize(result.data.size() * 2);
            dst_size = result.data.size() - dst_offset;
        }

        hint = LZ4F_decompress(
            dctx,
            result.data.data() + dst_offset, &dst_size,
            data + src_offset, &src_size,
            nullptr
        );

        if (LZ4F_isError(hint)) {
            LZ4F_freeDecompressionContext(dctx);
            result.success = false;
            result.error_message = LZ4F_getErrorName(hint);
            result.data.clear();
            return result;
        }

        src_offset += src_size;
        dst_offset += dst_size;
    }

    // 5. 释放解压上下文
    LZ4F_freeDecompressionContext(dctx);

    // 6. 裁剪到实际大小
    result.data.resize(dst_offset);
    result.compressed_size = size;
    result.original_size = dst_offset;

    if (dst_offset > 0) {
        result.ratio = static_cast<double>(size) / static_cast<double>(dst_offset);
    }

    result.success = true;
    return result;
}

// ============================================================
// Snappy 压缩实现
// 使用 snappy_compress（一键式 API）
// Snappy 不支持压缩级别参数
// ============================================================
CompressionResult MemoryDecompressor::compress_snappy(const uint8_t* data, size_t size, const CompressionOptions& options) {
    CompressionResult result;
    result.original_size = size;

    size_t max_size = snappy_max_compressed_length(size);
    result.data.resize(max_size);

    size_t compressed_size = max_size;
    snappy_status status = snappy_compress(
        reinterpret_cast<const char*>(data), size,
        reinterpret_cast<char*>(result.data.data()), &compressed_size);

    if (status != SNAPPY_OK) {
        result.success = false;
        result.error_message = "Snappy compression failed";
        result.data.clear();
        return result;
    }

    result.data.resize(compressed_size);
    result.compressed_size = compressed_size;

    if (size > 0) {
        result.ratio = static_cast<double>(compressed_size) / static_cast<double>(size);
    }

    result.success = true;
    return result;
}

// ============================================================
// Snappy 解压实现
// 先通过 snappy_uncompressed_length 获取原始大小，再解压
// ============================================================
CompressionResult MemoryDecompressor::decompress_snappy(const uint8_t* data, size_t size) {
    CompressionResult result;

    size_t uncompressed_size = 0;
    snappy_uncompressed_length(reinterpret_cast<const char*>(data), size, &uncompressed_size);
    result.data.resize(uncompressed_size);

    snappy_status status = snappy_uncompress(
        reinterpret_cast<const char*>(data), size,
        reinterpret_cast<char*>(result.data.data()), &uncompressed_size);

    if (status != SNAPPY_OK) {
        result.success = false;
        result.error_message = "Snappy decompression failed";
        result.data.clear();
        return result;
    }

    result.compressed_size = size;
    result.original_size = uncompressed_size;

    if (uncompressed_size > 0) {
        result.ratio = static_cast<double>(size) / static_cast<double>(uncompressed_size);
    }

    result.success = true;
    return result;
}

} // namespace md_compressor