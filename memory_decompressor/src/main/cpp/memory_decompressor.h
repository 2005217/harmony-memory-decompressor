#ifndef MEMORY_DECOMPRESSOR_H
#define MEMORY_DECOMPRESSOR_H

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>

namespace memory_decompressor {

// 压缩格式枚举
enum class CompressionFormat {
    ZSTD,
    GZIP,
    DEFLATE,
    LZ4
};

// 压缩/解压缩结果结构
struct CompressionResult {
    bool success;
    std::vector<uint8_t> data;
    std::string error_message;
    size_t original_size;
    size_t compressed_size;
};

// 压缩选项
struct CompressionOptions {
    CompressionFormat format;
    int level;  // 压缩级别，对于不同格式有不同含义
    bool use_dictionary;  // 是否使用字典
    std::vector<uint8_t> dictionary;  // 字典数据
};

// 内存解压缩器类
class MemoryDecompressor {
public:
    MemoryDecompressor();
    ~MemoryDecompressor();

    // 压缩数据
    CompressionResult compress(const uint8_t* data, size_t size, const CompressionOptions& options);
    
    // 解压缩数据
    CompressionResult decompress(const uint8_t* data, size_t size, CompressionFormat format);
    
    // 获取格式名称
    static std::string get_format_name(CompressionFormat format);
    
    // 获取支持的格式列表
    static std::vector<CompressionFormat> get_supported_formats();
    
    // 检测数据格式
    static CompressionFormat detect_format(const uint8_t* data, size_t size);
    
    // 获取版本信息
    static std::string get_version();

private:
    // ZSTD压缩
    CompressionResult compress_zstd(const uint8_t* data, size_t size, const CompressionOptions& options);
    
    // ZSTD解压缩
    CompressionResult decompress_zstd(const uint8_t* data, size_t size);
    
    // GZIP压缩
    CompressionResult compress_gzip(const uint8_t* data, size_t size, const CompressionOptions& options);
    
    // GZIP解压缩
    CompressionResult decompress_gzip(const uint8_t* data, size_t size);
    
    // DEFLATE压缩
    CompressionResult compress_deflate(const uint8_t* data, size_t size, const CompressionOptions& options);
    
    // DEFLATE解压缩
    CompressionResult decompress_deflate(const uint8_t* data, size_t size);
    
    // LZ4压缩
    CompressionResult compress_lz4(const uint8_t* data, size_t size, const CompressionOptions& options);
    
    // LZ4解压缩
    CompressionResult decompress_lz4(const uint8_t* data, size_t size);
    
    // 工具函数
    static bool is_zstd_format(const uint8_t* data, size_t size);
    static bool is_gzip_format(const uint8_t* data, size_t size);
    static bool is_deflate_format(const uint8_t* data, size_t size);
    static bool is_lz4_format(const uint8_t* data, size_t size);
};

} // namespace memory_decompressor

#endif // MEMORY_DECOMPRESSOR_H