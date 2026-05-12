#include "memory_decompressor.h"
#include "zstd.h"
#include <cstring>
#include <algorithm>
#include <sstream>

namespace md_compressor {

// 压缩数据
CompressionResult MemoryDecompressor::compress(const uint8_t* data, size_t size, const CompressionOptions& options) {
    CompressionResult result;
    result.original_size = size;
    
    switch (options.format) {
        case CompressionFormat::ZSTD:
            return compress_zstd(data, size, options);
        default:
            result.success = false;
            result.error_message = "Unsupported compression format";
            return result;
    }
}

// 解压缩数据
CompressionResult MemoryDecompressor::decompress(const uint8_t* data, size_t size, CompressionFormat format) {
    CompressionResult result;
    
    // 如果格式是UNKNOWN，尝试自动检测
    if (format == CompressionFormat::UNKNOWN) {
        format = detect_format(data, size);
    }
    
    switch (format) {
        case CompressionFormat::ZSTD:
            return decompress_zstd(data, size);
        default:
            result.success = false;
            result.error_message = "Unsupported or unknown compression format";
            return result;
    }
}

// 获取支持的压缩格式
std::vector<CompressionFormat> MemoryDecompressor::get_supported_formats() {
    return {CompressionFormat::ZSTD};
}

// 获取格式名称
std::string MemoryDecompressor::get_format_name(CompressionFormat format) {
    switch (format) {
        case CompressionFormat::ZSTD:
            return "ZSTD";
        case CompressionFormat::UNKNOWN:
            return "UNKNOWN";
        default:
            return "UNKNOWN";
    }
}

// 检测数据格式
CompressionFormat MemoryDecompressor::detect_format(const uint8_t* data, size_t size) {
    if (size < 4) {
        return CompressionFormat::UNKNOWN;
    }
    
    // 检查ZSTD魔数
    // ZSTD格式以4字节的魔数开头：0xFD2FB528 (小端序)
    uint32_t magic = 0;
    std::memcpy(&magic, data, sizeof(uint32_t));
    
    if (magic == 0xFD2FB528) {
        return CompressionFormat::ZSTD;
    }
    
    return CompressionFormat::UNKNOWN;
}

// 获取版本信息
std::string MemoryDecompressor::get_version() {
    std::string zstd_version = ZSTD_versionString();
    std::stringstream ss;
    ss << "MemoryDecompressor v1.0.0 (ZSTD " << zstd_version << ")";
    return ss.str();
}

// ZSTD压缩
CompressionResult MemoryDecompressor::compress_zstd(const uint8_t* data, size_t size, const CompressionOptions& options) {
    CompressionResult result;
    result.original_size = size;
    
    // 计算最大压缩大小
    size_t max_compressed_size = ZSTD_compressBound(size);
    if (max_compressed_size == 0) {
        result.success = false;
        result.error_message = "Invalid source size for ZSTD compression";
        return result;
    }
    
    // 分配输出缓冲区
    result.data.resize(max_compressed_size);
    
    // 执行压缩
    size_t compressed_size = ZSTD_compress(
        result.data.data(), max_compressed_size,
        data, size,
        options.level
    );
    
    // 检查压缩结果
    if (ZSTD_isError(compressed_size)) {
        result.success = false;
        result.error_message = ZSTD_getErrorName(compressed_size);
        result.data.clear();
        return result;
    }
    
    // 调整输出缓冲区大小
    result.data.resize(compressed_size);
    result.compressed_size = compressed_size;
    
    // 计算压缩比
    if (size > 0) {
        result.ratio = static_cast<double>(compressed_size) / static_cast<double>(size);
    }
    
    result.success = true;
    return result;
}

// ZSTD解压缩
CompressionResult MemoryDecompressor::decompress_zstd(const uint8_t* data, size_t size) {
    CompressionResult result;
    
    // 获取解压缩后的大小
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
    
    // 分配输出缓冲区
    result.data.resize(decompressed_size);
    
    // 执行解压缩
    size_t actual_decompressed_size = ZSTD_decompress(
        result.data.data(), decompressed_size,
        data, size
    );
    
    // 检查解压缩结果
    if (ZSTD_isError(actual_decompressed_size)) {
        result.success = false;
        result.error_message = ZSTD_getErrorName(actual_decompressed_size);
        result.data.clear();
        return result;
    }
    
    // 验证解压缩大小
    if (actual_decompressed_size != decompressed_size) {
        result.success = false;
        result.error_message = "Decompressed size mismatch";
        result.data.clear();
        return result;
    }
    
    result.compressed_size = size;
    result.original_size = actual_decompressed_size;
    
    // 计算压缩比
    if (actual_decompressed_size > 0) {
        result.ratio = static_cast<double>(size) / static_cast<double>(actual_decompressed_size);
    }
    
    result.success = true;
    return result;
}

} // namespace md_compressor