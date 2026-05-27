#ifndef MEMORY_DECOMPRESSOR_H
#define MEMORY_DECOMPRESSOR_H

#include <cstdint>
#include <vector>
#include <string>

namespace md_compressor {

// 支持的压缩格式枚举
// ZSTD = 高压缩比，适合存储和传输
// LZ4  = 极速压缩/解压，适合实时场景
enum class CompressionFormat {
    ZSTD,
    LZ4,
    SNAPPY,
    UNKNOWN
};

// 压缩选项：调用 compress 时传入的参数
struct CompressionOptions {
    CompressionFormat format = CompressionFormat::ZSTD; // 使用哪种算法
    int level = 3;                                      // 压缩级别（越大压缩比越高）
    bool use_dictionary = false;                        // 是否使用预训练字典
    std::vector<uint8_t> dictionary;                    // 字典数据
};

// 压缩/解压结果：统一返回结构
struct CompressionResult {
    bool success = false;              // 操作是否成功
    std::vector<uint8_t> data;         // 压缩或解压后的数据
    std::string error_message;         // 失败时的错误描述
    size_t original_size = 0;          // 原始数据大小
    size_t compressed_size = 0;        // 压缩后数据大小
    double ratio = 0.0;                // 压缩比（compressed / original）
};

// MemoryDecompressor 类：统一封装 ZSTD 和 LZ4 的压缩/解压操作
// 对外提供统一的 compress/decompress 接口，内部自动路由到对应算法
class MemoryDecompressor {
public:
    MemoryDecompressor() = default;
    ~MemoryDecompressor() = default;

    // 压缩数据：根据 options.format 自动选择 ZSTD 或 LZ4
    CompressionResult compress(const uint8_t* data, size_t size, const CompressionOptions& options);

    // 解压数据：根据 format 自动选择，format=UNKNOWN 时自动检测魔数
    CompressionResult decompress(const uint8_t* data, size_t size, CompressionFormat format = CompressionFormat::UNKNOWN);

    // 获取支持的压缩格式列表
    static std::vector<CompressionFormat> get_supported_formats();

    // 获取格式名称字符串（如 "ZSTD"、"LZ4"）
    static std::string get_format_name(CompressionFormat format);

    // 通过魔数检测数据是哪种压缩格式
    static CompressionFormat detect_format(const uint8_t* data, size_t size);

    // 获取库版本信息
    static std::string get_version();

private:
    // ZSTD 内部实现
    CompressionResult compress_zstd(const uint8_t* data, size_t size, const CompressionOptions& options);
    CompressionResult decompress_zstd(const uint8_t* data, size_t size);

    // LZ4 内部实现
    CompressionResult compress_lz4(const uint8_t* data, size_t size, const CompressionOptions& options);
    CompressionResult decompress_lz4(const uint8_t* data, size_t size);
    
    // Snappy内部实现
    CompressionResult compress_snappy(const uint8_t* data, size_t size, const CompressionOptions& options);
    CompressionResult decompress_snappy(const uint8_t* data, size_t size);
};

} // namespace md_compressor

#endif // MEMORY_DECOMPRESSOR_H