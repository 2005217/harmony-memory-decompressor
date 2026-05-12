#ifndef MEMORY_DECOMPRESSOR_H
#define MEMORY_DECOMPRESSOR_H

#include <cstdint>
#include <vector>
#include <string>

namespace md_compressor {

// 压缩格式枚举
enum class CompressionFormat {
    ZSTD,
    UNKNOWN
};

// 压缩选项结构体
struct CompressionOptions {
    CompressionFormat format = CompressionFormat::ZSTD;
    int level = 3;  // 默认压缩级别
    bool use_dictionary = false;
    std::vector<uint8_t> dictionary;
};

// 压缩结果结构体
struct CompressionResult {
    bool success = false;
    std::vector<uint8_t> data;
    std::string error_message;
    size_t original_size = 0;
    size_t compressed_size = 0;
    double ratio = 0.0;
};

// 内存解压缩器类
class MemoryDecompressor {
public:
    MemoryDecompressor() = default;
    ~MemoryDecompressor() = default;

    // 压缩数据
    CompressionResult compress(const uint8_t* data, size_t size, const CompressionOptions& options);

    // 解压缩数据
    CompressionResult decompress(const uint8_t* data, size_t size, CompressionFormat format = CompressionFormat::ZSTD);

    // 获取支持的压缩格式
    static std::vector<CompressionFormat> get_supported_formats();

    // 获取格式名称
    static std::string get_format_name(CompressionFormat format);

    // 检测数据格式
    static CompressionFormat detect_format(const uint8_t* data, size_t size);

    // 获取版本信息
    static std::string get_version();

private:
    // ZSTD压缩
    CompressionResult compress_zstd(const uint8_t* data, size_t size, const CompressionOptions& options);

    // ZSTD解压缩
    CompressionResult decompress_zstd(const uint8_t* data, size_t size);
};

} // namespace md_compressor

#endif // MEMORY_DECOMPRESSOR_H