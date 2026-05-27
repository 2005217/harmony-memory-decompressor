#include "napi/native_api.h"
#include "memory_decompressor.h"
#include "zstd.h"
#include "lz4frame.h"
#include "snappy-c.h"
#include <cstring>
#include <string>
// ============================================================
// ZSTD 异步流式压缩
// ============================================================

// 异步压缩数据结构体：保存从主线程传到后台线程的所有数据
struct ZstdAsyncCompressData {
    void* input_data;          // 输入数据指针，从 ArrayBuffer 直接获取（零拷贝）
    size_t input_size;         // 输入数据字节数
    napi_ref input_ref;        // NAPI 引用，防止 GC 在异步执行期间回收输入 ArrayBuffer

    void* output_data;         // 输出缓冲区指针，预先分配保证够用
    napi_ref output_ref;       // NAPI 引用，防止 GC 回收输出 ArrayBuffer
    size_t output_capacity;    // 输出缓冲区总容量（ZSTD_compressBound 计算）
    size_t compressed_size;    // 压缩后的实际大小，Complete 中用于裁剪精确输出

    int32_t compression_level; // 压缩级别（默认 3，范围 1-22）

    bool success;              // 压缩是否成功，Execute 中设置
    std::string error_message; // 错误描述，ZSTD_isError 时用 ZSTD_getErrorName 获取

    napi_deferred deferred;    // Promise 控制器：成功时 resolve，失败时 reject
    napi_async_work work;      // 异步工作句柄：创建、入队、清理都需要
};

// 后台线程执行：调用 ZSTD_compress 执行实际压缩
// 这个函数在 NAPI 的线程池中运行，不阻塞主线程
static void ZstdAsyncCompressExecute(napi_env env, void* data) {
    ZstdAsyncCompressData* async_data = static_cast<ZstdAsyncCompressData*>(data);

    md_compressor::MemoryDecompressor decompressor;
    md_compressor::CompressionOptions options;
    options.format = md_compressor::CompressionFormat::ZSTD;
    options.level = async_data->compression_level;

    auto result = decompressor.compress(
        static_cast<const uint8_t*>(async_data->input_data),
        async_data->input_size,
        options);

    if (result.success) {
        std::memcpy(async_data->output_data, result.data.data(), result.compressed_size);
        async_data->success = true;
        async_data->compressed_size = result.compressed_size;
    } else {
        async_data->success = false;
        async_data->error_message = result.error_message;
    }
}

// 主线程完成回调：将后台线程的结果通过 Promise 返回给 ArkTS
static void ZstdAsyncCompressComplete(napi_env env, napi_status status, void* data) {
    ZstdAsyncCompressData* async_data = static_cast<ZstdAsyncCompressData*>(data);

    if (status != napi_ok) {
        // 异步框架本身失败（如线程池满）
        napi_value err_msg;
        napi_create_string_utf8(env, "Async work failed", NAPI_AUTO_LENGTH, &err_msg);
        napi_value error;
        napi_create_error(env, nullptr, err_msg, &error);
        napi_reject_deferred(env, async_data->deferred, error);
    } else if (!async_data->success) {
        // 压缩执行失败，传递 ZSTD 错误信息
        napi_value err_msg;
        napi_create_string_utf8(env, async_data->error_message.c_str(), NAPI_AUTO_LENGTH, &err_msg);
        napi_value error;
        napi_create_error(env, nullptr, err_msg, &error);
        napi_reject_deferred(env, async_data->deferred, error);
    } else {
        // 压缩成功，返回输出 ArrayBuffer
        napi_value output_val;
        napi_get_reference_value(env, async_data->output_ref, &output_val);

        // 如果实际压缩大小小于缓冲区，创建精确大小的新 ArrayBuffer 返回
        if (async_data->compressed_size < async_data->output_capacity) {
            void* exact_data = nullptr;
            napi_value exact_buffer;
            napi_status s = napi_create_arraybuffer(env, async_data->compressed_size, &exact_data, &exact_buffer);
            if (s == napi_ok) {
                std::memcpy(exact_data, async_data->output_data, async_data->compressed_size);
                napi_resolve_deferred(env, async_data->deferred, exact_buffer);
            } else {
                napi_resolve_deferred(env, async_data->deferred, output_val);
            }
        } else {
            napi_resolve_deferred(env, async_data->deferred, output_val);
        }
    }

    // 清理所有资源：删除引用、删除异步工作、释放结构体
    napi_delete_reference(env, async_data->input_ref);
    napi_delete_reference(env, async_data->output_ref);
    napi_delete_async_work(env, async_data->work);
    delete async_data;
}

// NAPI 入口函数：在主线程执行，解析参数并启动异步工作
static napi_value ZstdCompressAsync(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2] = {nullptr};
    napi_value this_arg = nullptr;

    // 解析 ArkTS 传入的参数
    napi_status status = napi_get_cb_info(env, info, &argc, args, &this_arg, nullptr);
    if (status != napi_ok) {
        napi_throw_error(env, nullptr, "Failed to get callback info");
        return nullptr;
    }
    if (argc < 1) {
        napi_throw_error(env, nullptr, "Missing required argument: data");
        return nullptr;
    }

    // 解析第一个参数：输入 ArrayBuffer
    void* input_data = nullptr;
    size_t input_size = 0;
    status = napi_get_arraybuffer_info(env, args[0], &input_data, &input_size);
    if (status != napi_ok) {
        napi_throw_error(env, nullptr, "First argument must be ArrayBuffer");
        return nullptr;
    }

    // 解析第二个参数（可选）：压缩级别，默认 3
    int32_t compression_level = 3;
    if (argc >= 2) {
        napi_valuetype argType;
        napi_typeof(env, args[1], &argType);
        if (argType != napi_undefined && argType != napi_null) {
            status = napi_get_value_int32(env, args[1], &compression_level);
            if (status != napi_ok) {
                napi_throw_error(env, nullptr, "Invalid compression level");
                return nullptr;
            }
        }
    }

    // ZSTD_compressBound 返回压缩后最大可能的大小
    size_t max_size = ZSTD_compressBound(input_size);

    // 创建输出 ArrayBuffer
    void* output_data = nullptr;
    napi_value output_val;
    status = napi_create_arraybuffer(env, max_size, &output_data, &output_val);
    if (status != napi_ok) {
        napi_throw_error(env, nullptr, "Failed to create output buffer");
        return nullptr;
    }

    // 创建 NAPI 引用，防止 GC 在异步执行期间回收 ArrayBuffer
    napi_ref input_ref;
    napi_create_reference(env, args[0], 1, &input_ref);
    napi_ref output_ref;
    napi_create_reference(env, output_val, 1, &output_ref);

    // 创建 Promise，deferred 用于后续 resolve/reject
    napi_value promise;
    napi_deferred deferred;
    napi_create_promise(env, &deferred, &promise);

    // 填充异步数据结构体
    ZstdAsyncCompressData* async_data = new ZstdAsyncCompressData();
    async_data->input_data = input_data;
    async_data->input_size = input_size;
    async_data->compression_level = compression_level;
    async_data->input_ref = input_ref;
    async_data->output_data = output_data;
    async_data->output_ref = output_ref;
    async_data->output_capacity = max_size;
    async_data->compressed_size = 0;
    async_data->success = false;
    async_data->deferred = deferred;

    // 创建异步工作：指定执行函数和完成回调函数
    napi_value resource_name;
    napi_create_string_utf8(env, "ZstdCompressAsync", NAPI_AUTO_LENGTH, &resource_name);

    status = napi_create_async_work(env, nullptr, resource_name,
        ZstdAsyncCompressExecute,
        ZstdAsyncCompressComplete,
        async_data, &async_data->work);
    if (status != napi_ok) {
        napi_delete_reference(env, input_ref);
        napi_delete_reference(env, output_ref);
        delete async_data;
        napi_throw_error(env, nullptr, "Failed to create async work");
        return nullptr;
    }

    // 将异步任务加入线程池，立即返回 Promise
    status = napi_queue_async_work(env, async_data->work);
    if (status != napi_ok) {
        napi_delete_reference(env, input_ref);
        napi_delete_reference(env, output_ref);
        napi_delete_async_work(env, async_data->work);
        delete async_data;
        napi_throw_error(env, nullptr, "Failed to queue async work");
        return nullptr;
    }
    return promise;
}

// ============================================================
// ZSTD 异步流式解压
// ============================================================

// 异步解压数据结构体：保存从主线程传到后台线程的所有数据
struct ZstdAsyncDecompressData {
    void* input_data;          // 输入数据指针，从 ArrayBuffer 直接获取（零拷贝）
    size_t input_size;         // 输入数据字节数
    napi_ref input_ref;        // NAPI 引用，防止 GC 在异步执行期间回收输入 ArrayBuffer

    void* output_data;         // 输出缓冲区指针，预先分配保证够用
    napi_ref output_ref;       // NAPI 引用，防止 GC 回收输出 ArrayBuffer
    size_t output_size;        // 输出缓冲区大小（从帧头读取的原始数据大小）

    bool success;              // 解压是否成功，Execute 中设置
    std::string error_message; // 错误描述，ZSTD_isError 时用 ZSTD_getErrorName 获取

    napi_deferred deferred;    // Promise 控制器：成功时 resolve，失败时 reject
    napi_async_work work;      // 异步工作句柄：创建、入队、清理都需要
};

// 后台线程执行：调用 ZSTD_decompress 执行实际解压
// 这个函数在 NAPI 的线程池中运行，不阻塞主线程
static void ZstdAsyncDecompressExecute(napi_env env, void* data) {
    ZstdAsyncDecompressData* async_data = static_cast<ZstdAsyncDecompressData*>(data);

    md_compressor::MemoryDecompressor decompressor;

    auto result = decompressor.decompress(
        static_cast<const uint8_t*>(async_data->input_data),
        async_data->input_size,
        md_compressor::CompressionFormat::ZSTD);

    if (result.success) {
        std::memcpy(async_data->output_data, result.data.data(), result.original_size);
        async_data->success = true;
    } else {
        async_data->success = false;
        async_data->error_message = result.error_message;
    }
}

// 主线程完成回调：将后台线程的结果通过 Promise 返回给 ArkTS
static void ZstdAsyncDecompressComplete(napi_env env, napi_status status, void* data) {
    ZstdAsyncDecompressData* async_data = static_cast<ZstdAsyncDecompressData*>(data);

    if (status != napi_ok) {
        // 异步框架本身失败（如线程池满）
        napi_value err_msg;
        napi_create_string_utf8(env, "Async work failed", NAPI_AUTO_LENGTH, &err_msg);
        napi_value error;
        napi_create_error(env, nullptr, err_msg, &error);
        napi_reject_deferred(env, async_data->deferred, error);
    } else if (!async_data->success) {
        // 解压执行失败，传递 ZSTD 错误信息
        napi_value err_msg;
        napi_create_string_utf8(env, async_data->error_message.c_str(), NAPI_AUTO_LENGTH, &err_msg);
        napi_value error;
        napi_create_error(env, nullptr, err_msg, &error);
        napi_reject_deferred(env, async_data->deferred, error);
    } else {
        // 解压成功，返回输出 ArrayBuffer
        napi_value output_val;
        napi_get_reference_value(env, async_data->output_ref, &output_val);
        napi_resolve_deferred(env, async_data->deferred, output_val);
    }

    // 清理所有资源：删除引用、删除异步工作、释放结构体
    napi_delete_reference(env, async_data->input_ref);
    napi_delete_reference(env, async_data->output_ref);
    napi_delete_async_work(env, async_data->work);
    delete async_data;
}

// NAPI 入口函数：在主线程执行，解析参数并启动异步工作
static napi_value ZstdDecompressAsync(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1] = {nullptr};
    napi_value this_arg = nullptr;

    // 解析 ArkTS 传入的参数
    napi_status status = napi_get_cb_info(env, info, &argc, args, &this_arg, nullptr);
    if (status != napi_ok) {
        napi_throw_error(env, nullptr, "Failed to get callback info");
        return nullptr;
    }
    if (argc < 1) {
        napi_throw_error(env, nullptr, "Missing required argument: data");
        return nullptr;
    }

    // 解析第一个参数：输入 ArrayBuffer（压缩后的数据）
    void* input_data = nullptr;
    size_t input_size = 0;
    status = napi_get_arraybuffer_info(env, args[0], &input_data, &input_size);
    if (status != napi_ok) {
        napi_throw_error(env, nullptr, "First argument must be ArrayBuffer");
        return nullptr;
    }

    // 从 ZSTD 帧头读取原始数据大小，用于分配输出缓冲区
    unsigned long long decompressed_size = ZSTD_getFrameContentSize(input_data, input_size);
    if (decompressed_size == ZSTD_CONTENTSIZE_UNKNOWN || decompressed_size == ZSTD_CONTENTSIZE_ERROR) {
        napi_throw_error(env, nullptr, "Cannot determine decompressed size");
        return nullptr;
    }

    // 创建输出 ArrayBuffer
    void* output_data = nullptr;
    napi_value output_val;
    status = napi_create_arraybuffer(env, decompressed_size, &output_data, &output_val);
    if (status != napi_ok) {
        napi_throw_error(env, nullptr, "Failed to create output buffer");
        return nullptr;
    }

    // 创建 NAPI 引用，防止 GC 在异步执行期间回收 ArrayBuffer
    napi_ref input_ref;
    napi_create_reference(env, args[0], 1, &input_ref);
    napi_ref output_ref;
    napi_create_reference(env, output_val, 1, &output_ref);

    // 创建 Promise，deferred 用于后续 resolve/reject
    napi_value promise;
    napi_deferred deferred;
    napi_create_promise(env, &deferred, &promise);

    // 填充异步数据结构体
    ZstdAsyncDecompressData* async_data = new ZstdAsyncDecompressData();
    async_data->input_data = input_data;
    async_data->input_size = input_size;
    async_data->input_ref = input_ref;
    async_data->output_data = output_data;
    async_data->output_ref = output_ref;
    async_data->output_size = decompressed_size;
    async_data->success = false;
    async_data->deferred = deferred;

    // 创建异步工作：指定执行函数和完成回调函数
    napi_value resource_name;
    napi_create_string_utf8(env, "ZstdDecompressAsync", NAPI_AUTO_LENGTH, &resource_name);

    status = napi_create_async_work(env, nullptr, resource_name,
        ZstdAsyncDecompressExecute,
        ZstdAsyncDecompressComplete,
        async_data, &async_data->work);
    if (status != napi_ok) {
        napi_delete_reference(env, input_ref);
        napi_delete_reference(env, output_ref);
        delete async_data;
        napi_throw_error(env, nullptr, "Failed to create async work");
        return nullptr;
    }

    // 将异步任务加入线程池，立即返回 Promise
    status = napi_queue_async_work(env, async_data->work);
    if (status != napi_ok) {
        napi_delete_reference(env, input_ref);
        napi_delete_reference(env, output_ref);
        napi_delete_async_work(env, async_data->work);
        delete async_data;
        napi_throw_error(env, nullptr, "Failed to queue async work");
        return nullptr;
    }
    return promise;
}

// ============================================================
// LZ4 异步流式压缩
// ============================================================

// 异步压缩数据结构体：保存从主线程传到后台线程的所有数据
struct Lz4AsyncCompressData {
    void* input_data;          // 输入数据指针，从 ArrayBuffer 直接获取（零拷贝）
    size_t input_size;         // 输入数据字节数
    napi_ref input_ref;        // NAPI 引用，防止 GC 在异步执行期间回收输入 ArrayBuffer

    void* output_data;         // 输出缓冲区指针，预先分配保证够用
    napi_ref output_ref;       // NAPI 引用，防止 GC 回收输出 ArrayBuffer
    size_t output_capacity;    // 输出缓冲区总容量（LZ4F_compressBound + 64）
    size_t compressed_size;    // 压缩后的实际大小，Complete 中用于裁剪精确输出

    int32_t compression_level; // 压缩级别（0=默认，1-12，越大压缩比越高但越慢）

    bool success;              // 压缩是否成功，Execute 中设置
    std::string error_message; // 错误描述，LZ4F_isError 时用 LZ4F_getErrorName 获取

    napi_deferred deferred;    // Promise 控制器：成功时 resolve，失败时 reject
    napi_async_work work;      // 异步工作句柄：创建、入队、清理都需要
};

// 后台线程执行：使用 LZ4 Frame API 流式压缩
// 流程：创建上下文 → 写帧头 → 压缩数据块 → 写帧尾 → 释放上下文
static void Lz4AsyncCompressExecute(napi_env env, void* data) {
    Lz4AsyncCompressData* async_data = static_cast<Lz4AsyncCompressData*>(data);

    md_compressor::MemoryDecompressor decompressor;
    md_compressor::CompressionOptions options;
    options.format = md_compressor::CompressionFormat::LZ4;
    options.level = async_data->compression_level;

    auto result = decompressor.compress(
        static_cast<const uint8_t*>(async_data->input_data),
        async_data->input_size,
        options);

    if (result.success) {
        std::memcpy(async_data->output_data, result.data.data(), result.compressed_size);
        async_data->success = true;
        async_data->compressed_size = result.compressed_size;
    } else {
        async_data->success = false;
        async_data->error_message = result.error_message;
    }
}

// 主线程完成回调：将后台线程的结果通过 Promise 返回给 ArkTS
static void Lz4AsyncCompressComplete(napi_env env, napi_status status, void* data) {
    Lz4AsyncCompressData* async_data = static_cast<Lz4AsyncCompressData*>(data);

    if (status != napi_ok) {
        // 异步框架本身失败（如线程池满）
        napi_value err_msg;
        napi_create_string_utf8(env, "Async work failed", NAPI_AUTO_LENGTH, &err_msg);
        napi_value error;
        napi_create_error(env, nullptr, err_msg, &error);
        napi_reject_deferred(env, async_data->deferred, error);
    } else if (!async_data->success) {
        // 压缩执行失败，传递 LZ4 错误信息
        napi_value err_msg;
        napi_create_string_utf8(env, async_data->error_message.c_str(),
                                NAPI_AUTO_LENGTH, &err_msg);
        napi_value error;
        napi_create_error(env, nullptr, err_msg, &error);
        napi_reject_deferred(env, async_data->deferred, error);
    } else {
        // 压缩成功，返回输出 ArrayBuffer
        napi_value output_val;
        napi_get_reference_value(env, async_data->output_ref, &output_val);

        // 如果实际压缩大小小于缓冲区，创建精确大小的新 ArrayBuffer 返回
        if (async_data->compressed_size < async_data->output_capacity) {
            void* exact_data = nullptr;
            napi_value exact_buffer;
            napi_status s = napi_create_arraybuffer(
                env, async_data->compressed_size, &exact_data, &exact_buffer);
            if (s == napi_ok) {
                std::memcpy(exact_data, async_data->output_data,
                           async_data->compressed_size);
                napi_resolve_deferred(env, async_data->deferred, exact_buffer);
            } else {
                napi_resolve_deferred(env, async_data->deferred, output_val);
            }
        } else {
            napi_resolve_deferred(env, async_data->deferred, output_val);
        }
    }

    // 清理所有资源：删除引用、删除异步工作、释放结构体
    napi_delete_reference(env, async_data->input_ref);
    napi_delete_reference(env, async_data->output_ref);
    napi_delete_async_work(env, async_data->work);
    delete async_data;
}

// NAPI 入口函数：在主线程执行，解析参数并启动异步工作
static napi_value Lz4CompressAsync(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2] = {nullptr};
    napi_value this_arg = nullptr;

    napi_status status = napi_get_cb_info(env, info, &argc, args, &this_arg, nullptr);
    if (status != napi_ok) {
        napi_throw_error(env, nullptr, "Failed to get callback info");
        return nullptr;
    }
    if (argc < 1) {
        napi_throw_error(env, nullptr, "Missing required argument: data");
        return nullptr;
    }

    // 解析第一个参数：输入 ArrayBuffer
    void* input_data = nullptr;
    size_t input_size = 0;
    status = napi_get_arraybuffer_info(env, args[0], &input_data, &input_size);
    if (status != napi_ok) {
        napi_throw_error(env, nullptr, "First argument must be ArrayBuffer");
        return nullptr;
    }

    // 解析第二个参数（可选）：压缩级别
    int32_t compression_level = 0;
    if (argc >= 2) {
        napi_valuetype argType;
        napi_typeof(env, args[1], &argType);
        if (argType != napi_undefined && argType != napi_null) {
            status = napi_get_value_int32(env, args[1], &compression_level);
            if (status != napi_ok) {
                napi_throw_error(env, nullptr, "Invalid compression level");
                return nullptr;
            }
        }
    }

    // 计算输出缓冲区大小：LZ4F_compressBound 保证够用，+64 给帧头和帧尾
    size_t max_size = LZ4F_compressBound(input_size, nullptr) + 64;

    // 创建输出 ArrayBuffer
    void* output_data = nullptr;
    napi_value output_val;
    status = napi_create_arraybuffer(env, max_size, &output_data, &output_val);
    if (status != napi_ok) {
        napi_throw_error(env, nullptr, "Failed to create output buffer");
        return nullptr;
    }

    // 创建 NAPI 引用，防止 GC 在异步执行期间回收 ArrayBuffer
    napi_ref input_ref;
    napi_create_reference(env, args[0], 1, &input_ref);
    napi_ref output_ref;
    napi_create_reference(env, output_val, 1, &output_ref);

    // 创建 Promise，deferred 用于后续 resolve/reject
    napi_value promise;
    napi_deferred deferred;
    napi_create_promise(env, &deferred, &promise);

    // 填充异步数据结构体
    Lz4AsyncCompressData* async_data = new Lz4AsyncCompressData();
    async_data->input_data = input_data;
    async_data->input_size = input_size;
    async_data->compression_level = compression_level;
    async_data->input_ref = input_ref;
    async_data->output_data = output_data;
    async_data->output_ref = output_ref;
    async_data->output_capacity = max_size;
    async_data->compressed_size = 0;
    async_data->success = false;
    async_data->deferred = deferred;

    // 创建异步工作：指定执行函数和完成回调函数
    napi_value resource_name;
    napi_create_string_utf8(env, "Lz4CompressAsync", NAPI_AUTO_LENGTH, &resource_name);

    status = napi_create_async_work(env, nullptr, resource_name,
        Lz4AsyncCompressExecute,
        Lz4AsyncCompressComplete,
        async_data, &async_data->work);
    if (status != napi_ok) {
        napi_delete_reference(env, input_ref);
        napi_delete_reference(env, output_ref);
        delete async_data;
        napi_throw_error(env, nullptr, "Failed to create async work");
        return nullptr;
    }

    // 将异步任务加入线程池，立即返回 Promise
    status = napi_queue_async_work(env, async_data->work);
    if (status != napi_ok) {
        napi_delete_reference(env, input_ref);
        napi_delete_reference(env, output_ref);
        napi_delete_async_work(env, async_data->work);
        delete async_data;
        napi_throw_error(env, nullptr, "Failed to queue async work");
        return nullptr;
    }
    return promise;
}

// ============================================================
// LZ4 异步流式解压
// ============================================================

// 异步解压数据结构体
struct Lz4AsyncDecompressData {
    void* input_data;          // 输入数据指针（LZ4 Frame 格式的压缩数据）
    size_t input_size;         // 输入数据字节数
    napi_ref input_ref;        // NAPI 引用，防止 GC 回收输入 ArrayBuffer

    void* output_data;         // 输出缓冲区指针（在 Execute 中用 new[] 分配）
    size_t output_size;        // 解压后的实际大小

    bool success;              // 解压是否成功
    std::string error_message; // 错误描述

    napi_deferred deferred;    // Promise 控制器
    napi_async_work work;      // 异步工作句柄
};

// 后台线程执行：使用 LZ4 Frame API 流式解压
// LZ4 Frame 内部可能包含多个数据块，需要用 while 循环一块一块解压
static void Lz4AsyncDecompressExecute(napi_env env, void* data) {
    Lz4AsyncDecompressData* async_data = static_cast<Lz4AsyncDecompressData*>(data);

    md_compressor::MemoryDecompressor decompressor;

    auto result = decompressor.decompress(
        static_cast<const uint8_t*>(async_data->input_data),
        async_data->input_size,
        md_compressor::CompressionFormat::LZ4);

    if (result.success) {
        async_data->output_data = new char[result.original_size];
        std::memcpy(async_data->output_data, result.data.data(), result.original_size);
        async_data->output_size = result.original_size;
        async_data->success = true;
    } else {
        async_data->success = false;
        async_data->error_message = result.error_message;
    }
}

// 主线程完成回调：将后台线程的结果通过 Promise 返回给 ArkTS
static void Lz4AsyncDecompressComplete(napi_env env, napi_status status, void* data) {
    Lz4AsyncDecompressData* async_data = static_cast<Lz4AsyncDecompressData*>(data);

    if (status != napi_ok) {
        napi_value err_msg;
        napi_create_string_utf8(env, "Async work failed", NAPI_AUTO_LENGTH, &err_msg);
        napi_value error;
        napi_create_error(env, nullptr, err_msg, &error);
        napi_reject_deferred(env, async_data->deferred, error);
    } else if (!async_data->success) {
        napi_value err_msg;
        napi_create_string_utf8(env, async_data->error_message.c_str(),
                                NAPI_AUTO_LENGTH, &err_msg);
        napi_value error;
        napi_create_error(env, nullptr, err_msg, &error);
        napi_reject_deferred(env, async_data->deferred, error);
    } else {
        // 创建精确大小的 NAPI ArrayBuffer，从临时缓冲区拷贝数据
        void* exact_data = nullptr;
        napi_value exact_buffer;
        napi_status s = napi_create_arraybuffer(env, async_data->output_size, &exact_data, &exact_buffer);
        if (s == napi_ok) {
            std::memcpy(exact_data, async_data->output_data, async_data->output_size);
            napi_resolve_deferred(env, async_data->deferred, exact_buffer);
        } else {
            napi_value err_msg;
            napi_create_string_utf8(env, "Failed to create output ArrayBuffer", NAPI_AUTO_LENGTH, &err_msg);
            napi_value error;
            napi_create_error(env, nullptr, err_msg, &error);
            napi_reject_deferred(env, async_data->deferred, error);
        }
    }

    // 清理资源
    if (async_data->output_data != nullptr) {
        delete[] static_cast<char*>(async_data->output_data);
        async_data->output_data = nullptr;
    }
    napi_delete_reference(env, async_data->input_ref);
    napi_delete_async_work(env, async_data->work);
    delete async_data;
}

// NAPI 入口函数
static napi_value Lz4DecompressAsync(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1] = {nullptr};
    napi_value this_arg = nullptr;

    napi_status status = napi_get_cb_info(env, info, &argc, args, &this_arg, nullptr);
    if (status != napi_ok) {
        napi_throw_error(env, nullptr, "Failed to get callback info");
        return nullptr;
    }
    if (argc < 1) {
        napi_throw_error(env, nullptr, "Missing required argument: data");
        return nullptr;
    }

    // 解析输入 ArrayBuffer
    void* input_data = nullptr;
    size_t input_size = 0;
    status = napi_get_arraybuffer_info(env, args[0], &input_data, &input_size);
    if (status != napi_ok) {
        napi_throw_error(env, nullptr, "First argument must be ArrayBuffer");
        return nullptr;
    }

    // 创建 NAPI 引用（仅需输入引用，输出缓冲区在后台线程动态分配）
    napi_ref input_ref;
    napi_create_reference(env, args[0], 1, &input_ref);

    // 创建 Promise
    napi_value promise;
    napi_deferred deferred;
    napi_create_promise(env, &deferred, &promise);

    // 填充异步数据结构体
    Lz4AsyncDecompressData* async_data = new Lz4AsyncDecompressData();
    async_data->input_data = input_data;
    async_data->input_size = input_size;
    async_data->input_ref = input_ref;
    async_data->output_data = nullptr;
    async_data->output_size = 0;
    async_data->success = false;
    async_data->deferred = deferred;

    // 创建并启动异步工作
    napi_value resource_name;
    napi_create_string_utf8(env, "Lz4DecompressAsync", NAPI_AUTO_LENGTH, &resource_name);

    status = napi_create_async_work(env, nullptr, resource_name,
        Lz4AsyncDecompressExecute,
        Lz4AsyncDecompressComplete,
        async_data, &async_data->work);
    if (status != napi_ok) {
        napi_delete_reference(env, input_ref);
        delete async_data;
        napi_throw_error(env, nullptr, "Failed to create async work");
        return nullptr;
    }

    status = napi_queue_async_work(env, async_data->work);
    if (status != napi_ok) {
        napi_delete_reference(env, input_ref);
        napi_delete_async_work(env, async_data->work);
        delete async_data;
        napi_throw_error(env, nullptr, "Failed to queue async work");
        return nullptr;
    }
    return promise;
}

// ============================================================
// Snappy 异步流式压缩
// ============================================================

struct SnappyAsyncCompressData {
    void* input_data;
    size_t input_size;
    napi_ref input_ref;

    void* output_data;
    napi_ref output_ref;
    size_t output_capacity;
    size_t compressed_size;

    bool success;
    std::string error_message;

    napi_deferred deferred;
    napi_async_work work;
};

static void SnappyAsyncCompressExecute(napi_env env, void* data) {
    SnappyAsyncCompressData* async_data = static_cast<SnappyAsyncCompressData*>(data);

    md_compressor::MemoryDecompressor decompressor;
    md_compressor::CompressionOptions options;
    options.format = md_compressor::CompressionFormat::SNAPPY;

    auto result = decompressor.compress(
        static_cast<const uint8_t*>(async_data->input_data),
        async_data->input_size,
        options);

    if (result.success) {
        std::memcpy(async_data->output_data, result.data.data(), result.compressed_size);
        async_data->success = true;
        async_data->compressed_size = result.compressed_size;
    } else {
        async_data->success = false;
        async_data->error_message = result.error_message;
    }
}

static void SnappyAsyncCompressComplete(napi_env env, napi_status status, void* data) {
    SnappyAsyncCompressData* async_data = static_cast<SnappyAsyncCompressData*>(data);

    if (status != napi_ok) {
        napi_value err_msg;
        napi_create_string_utf8(env, "Async work failed", NAPI_AUTO_LENGTH, &err_msg);
        napi_value error;
        napi_create_error(env, nullptr, err_msg, &error);
        napi_reject_deferred(env, async_data->deferred, error);
    } else if (!async_data->success) {
        napi_value err_msg;
        napi_create_string_utf8(env, async_data->error_message.c_str(), NAPI_AUTO_LENGTH, &err_msg);
        napi_value error;
        napi_create_error(env, nullptr, err_msg, &error);
        napi_reject_deferred(env, async_data->deferred, error);
    } else {
        napi_value output_val;
        napi_get_reference_value(env, async_data->output_ref, &output_val);

        if (async_data->compressed_size < async_data->output_capacity) {
            void* exact_data = nullptr;
            napi_value exact_buffer;
            napi_status s = napi_create_arraybuffer(env, async_data->compressed_size, &exact_data, &exact_buffer);
            if (s == napi_ok) {
                std::memcpy(exact_data, async_data->output_data, async_data->compressed_size);
                napi_resolve_deferred(env, async_data->deferred, exact_buffer);
            } else {
                napi_resolve_deferred(env, async_data->deferred, output_val);
            }
        } else {
            napi_resolve_deferred(env, async_data->deferred, output_val);
        }
    }

    napi_delete_reference(env, async_data->input_ref);
    napi_delete_reference(env, async_data->output_ref);
    napi_delete_async_work(env, async_data->work);
    delete async_data;
}

static napi_value SnappyCompressAsync(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1] = {nullptr};
    napi_value this_arg = nullptr;

    napi_status status = napi_get_cb_info(env, info, &argc, args, &this_arg, nullptr);
    if (status != napi_ok) {
        napi_throw_error(env, nullptr, "Failed to get callback info");
        return nullptr;
    }
    if (argc < 1) {
        napi_throw_error(env, nullptr, "Missing required argument: data");
        return nullptr;
    }

    void* input_data = nullptr;
    size_t input_size = 0;
    status = napi_get_arraybuffer_info(env, args[0], &input_data, &input_size);
    if (status != napi_ok) {
        napi_throw_error(env, nullptr, "First argument must be ArrayBuffer");
        return nullptr;
    }

    size_t max_size = snappy_max_compressed_length(input_size);

    void* output_data = nullptr;
    napi_value output_val;
    status = napi_create_arraybuffer(env, max_size, &output_data, &output_val);
    if (status != napi_ok) {
        napi_throw_error(env, nullptr, "Failed to create output buffer");
        return nullptr;
    }

    napi_ref input_ref;
    napi_create_reference(env, args[0], 1, &input_ref);
    napi_ref output_ref;
    napi_create_reference(env, output_val, 1, &output_ref);

    napi_value promise;
    napi_deferred deferred;
    napi_create_promise(env, &deferred, &promise);

    SnappyAsyncCompressData* async_data = new SnappyAsyncCompressData();
    async_data->input_data = input_data;
    async_data->input_size = input_size;
    async_data->input_ref = input_ref;
    async_data->output_data = output_data;
    async_data->output_ref = output_ref;
    async_data->output_capacity = max_size;
    async_data->compressed_size = 0;
    async_data->success = false;
    async_data->deferred = deferred;

    napi_value resource_name;
    napi_create_string_utf8(env, "SnappyCompressAsync", NAPI_AUTO_LENGTH, &resource_name);

    status = napi_create_async_work(env, nullptr, resource_name,
        SnappyAsyncCompressExecute,
        SnappyAsyncCompressComplete,
        async_data, &async_data->work);
    if (status != napi_ok) {
        napi_delete_reference(env, input_ref);
        napi_delete_reference(env, output_ref);
        delete async_data;
        napi_throw_error(env, nullptr, "Failed to create async work");
        return nullptr;
    }

    status = napi_queue_async_work(env, async_data->work);
    if (status != napi_ok) {
        napi_delete_reference(env, input_ref);
        napi_delete_reference(env, output_ref);
        napi_delete_async_work(env, async_data->work);
        delete async_data;
        napi_throw_error(env, nullptr, "Failed to queue async work");
        return nullptr;
    }

    return promise;
}

// ============================================================
// Snappy 异步流式解压
// ============================================================

struct SnappyAsyncDecompressData {
    void* input_data;
    size_t input_size;
    napi_ref input_ref;

    void* output_data;
    napi_ref output_ref;
    size_t output_size;

    bool success;
    std::string error_message;

    napi_deferred deferred;
    napi_async_work work;
};

static void SnappyAsyncDecompressExecute(napi_env env, void* data) {
    SnappyAsyncDecompressData* async_data = static_cast<SnappyAsyncDecompressData*>(data);

    md_compressor::MemoryDecompressor decompressor;

    auto result = decompressor.decompress(
        static_cast<const uint8_t*>(async_data->input_data),
        async_data->input_size,
        md_compressor::CompressionFormat::SNAPPY);

    if (result.success) {
        std::memcpy(async_data->output_data, result.data.data(), result.original_size);
        async_data->success = true;
    } else {
        async_data->success = false;
        async_data->error_message = result.error_message;
    }
}

static void SnappyAsyncDecompressComplete(napi_env env, napi_status status, void* data) {
    SnappyAsyncDecompressData* async_data = static_cast<SnappyAsyncDecompressData*>(data);

    if (status != napi_ok) {
        napi_value err_msg;
        napi_create_string_utf8(env, "Async work failed", NAPI_AUTO_LENGTH, &err_msg);
        napi_value error;
        napi_create_error(env, nullptr, err_msg, &error);
        napi_reject_deferred(env, async_data->deferred, error);
    } else if (!async_data->success) {
        napi_value err_msg;
        napi_create_string_utf8(env, async_data->error_message.c_str(), NAPI_AUTO_LENGTH, &err_msg);
        napi_value error;
        napi_create_error(env, nullptr, err_msg, &error);
        napi_reject_deferred(env, async_data->deferred, error);
    } else {
        napi_value output_val;
        napi_get_reference_value(env, async_data->output_ref, &output_val);
        napi_resolve_deferred(env, async_data->deferred, output_val);
    }

    napi_delete_reference(env, async_data->input_ref);
    napi_delete_reference(env, async_data->output_ref);
    napi_delete_async_work(env, async_data->work);
    delete async_data;
}

static napi_value SnappyDecompressAsync(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1] = {nullptr};
    napi_value this_arg = nullptr;

    napi_status status = napi_get_cb_info(env, info, &argc, args, &this_arg, nullptr);
    if (status != napi_ok) {
        napi_throw_error(env, nullptr, "Failed to get callback info");
        return nullptr;
    }
    if (argc < 1) {
        napi_throw_error(env, nullptr, "Missing required argument: data");
        return nullptr;
    }

    void* input_data = nullptr;
    size_t input_size = 0;
    status = napi_get_arraybuffer_info(env, args[0], &input_data, &input_size);
    if (status != napi_ok) {
        napi_throw_error(env, nullptr, "First argument must be ArrayBuffer");
        return nullptr;
    }

    size_t decompressed_size = 0;
    snappy_uncompressed_length(
        static_cast<const char*>(input_data), input_size, &decompressed_size);

    void* output_data = nullptr;
    napi_value output_val;
    status = napi_create_arraybuffer(env, decompressed_size, &output_data, &output_val);
    if (status != napi_ok) {
        napi_throw_error(env, nullptr, "Failed to create output buffer");
        return nullptr;
    }

    napi_ref input_ref;
    napi_create_reference(env, args[0], 1, &input_ref);
    napi_ref output_ref;
    napi_create_reference(env, output_val, 1, &output_ref);

    napi_value promise;
    napi_deferred deferred;
    napi_create_promise(env, &deferred, &promise);

    SnappyAsyncDecompressData* async_data = new SnappyAsyncDecompressData();
    async_data->input_data = input_data;
    async_data->input_size = input_size;
    async_data->input_ref = input_ref;
    async_data->output_data = output_data;
    async_data->output_ref = output_ref;
    async_data->output_size = decompressed_size;
    async_data->success = false;
    async_data->deferred = deferred;

    napi_value resource_name;
    napi_create_string_utf8(env, "SnappyDecompressAsync", NAPI_AUTO_LENGTH, &resource_name);

    status = napi_create_async_work(env, nullptr, resource_name,
        SnappyAsyncDecompressExecute,
        SnappyAsyncDecompressComplete,
        async_data, &async_data->work);
    if (status != napi_ok) {
        napi_delete_reference(env, input_ref);
        napi_delete_reference(env, output_ref);
        delete async_data;
        napi_throw_error(env, nullptr, "Failed to create async work");
        return nullptr;
    }

    status = napi_queue_async_work(env, async_data->work);
    if (status != napi_ok) {
        napi_delete_reference(env, input_ref);
        napi_delete_reference(env, output_ref);
        napi_delete_async_work(env, async_data->work);
        delete async_data;
        napi_throw_error(env, nullptr, "Failed to queue async work");
        return nullptr;
    }

    return promise;
}

// ============================================================
// 工具函数
// ============================================================

// 获取版本信息：返回 "MemoryDecompressor v1.0.1 (ZSTD x.y.z, LZ4 x.y.z)"
static napi_value GetVersion(napi_env env, napi_callback_info info) {
    std::string version = md_compressor::MemoryDecompressor::get_version();

    napi_value result;
    napi_status status = napi_create_string_utf8(env, version.c_str(), version.length(), &result);
    if (status != napi_ok) {
        napi_throw_error(env, nullptr, "Failed to create version string");
        return nullptr;
    }
    return result;
}

// 获取支持的压缩格式列表：返回 ["ZSTD", "LZ4"]
static napi_value GetSupportedFormats(napi_env env, napi_callback_info info) {
    std::vector<md_compressor::CompressionFormat> formats =
        md_compressor::MemoryDecompressor::get_supported_formats();

    napi_value result;
    napi_status status = napi_create_array_with_length(env, formats.size(), &result);
    if (status != napi_ok) {
        napi_throw_error(env, nullptr, "Failed to create array");
        return nullptr;
    }

    for (size_t i = 0; i < formats.size(); i++) {
        std::string format_name = md_compressor::MemoryDecompressor::get_format_name(formats[i]);
        napi_value format_str;
        status = napi_create_string_utf8(env, format_name.c_str(), format_name.length(), &format_str);
        if (status != napi_ok) {
            napi_throw_error(env, nullptr, "Failed to create format string");
            return nullptr;
        }
        status = napi_set_element(env, result, i, format_str);
        if (status != napi_ok) {
            napi_throw_error(env, nullptr, "Failed to set array element");
            return nullptr;
        }
    }

    return result;
}

// 检测压缩格式：传入 ArrayBuffer，返回格式名称字符串
// 通过前 4 字节魔数判断是 ZSTD、LZ4 还是 UNKNOWN
static napi_value DetectFormat(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1] = {nullptr};
    napi_value this_arg = nullptr;

    napi_status status = napi_get_cb_info(env, info, &argc, args, &this_arg, nullptr);
    if (status != napi_ok) {
        napi_throw_error(env, nullptr, "Failed to get callback info");
        return nullptr;
    }
    if (argc < 1) {
        napi_throw_error(env, nullptr, "Missing required argument: data");
        return nullptr;
    }

    void* input_data = nullptr;
    size_t input_size = 0;
    status = napi_get_arraybuffer_info(env, args[0], &input_data, &input_size);
    if (status != napi_ok) {
        napi_throw_error(env, nullptr, "First argument must be ArrayBuffer");
        return nullptr;
    }

    md_compressor::CompressionFormat format =
        md_compressor::MemoryDecompressor::detect_format(
            static_cast<const uint8_t*>(input_data), input_size);

    std::string format_name = md_compressor::MemoryDecompressor::get_format_name(format);

    napi_value result;
    status = napi_create_string_utf8(env, format_name.c_str(), format_name.length(), &result);
    if (status != napi_ok) {
        napi_throw_error(env, nullptr, "Failed to create format name string");
        return nullptr;
    }

    return result;
}

// ============================================================
// 模块注册
// 将 C++ 函数注册为 NAPI 接口，供 ArkTS 通过 import 调用
// ============================================================

EXTERN_C_START
static napi_value Init(napi_env env, napi_value exports)
{
    // 定义所有要暴露给 ArkTS 的 NAPI 函数
    napi_property_descriptor desc[] = {
        // ZSTD 压缩/解压（异步，返回 Promise<ArrayBuffer>）
        { "zstdCompressAsync", nullptr, ZstdCompressAsync, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "zstdDecompressAsync", nullptr, ZstdDecompressAsync, nullptr, nullptr, nullptr, napi_default, nullptr },

        // LZ4 压缩/解压（异步，返回 Promise<ArrayBuffer>）
        { "lz4CompressAsync", nullptr, Lz4CompressAsync, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "lz4DecompressAsync", nullptr, Lz4DecompressAsync, nullptr, nullptr, nullptr, napi_default, nullptr },

        // Snappy 压缩/解压（异步，返回 Promise<ArrayBuffer>）
        { "snappyCompressAsync", nullptr, SnappyCompressAsync, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "snappyDecompressAsync", nullptr, SnappyDecompressAsync, nullptr, nullptr, nullptr, napi_default, nullptr },

        // 工具函数（同步，直接返回值）
        { "version", nullptr, GetVersion, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "getSupportedFormats", nullptr, GetSupportedFormats, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "detectFormat", nullptr, DetectFormat, nullptr, nullptr, nullptr, napi_default, nullptr },
    };
    napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
    return exports;
}
EXTERN_C_END

// 模块定义：指定模块名和注册函数
// 模块名 "memory_decompressor" 必须与 ArkTS 中 import 的名称一致
static napi_module decompressorModule = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = Init,       // 模块初始化时调用 Init 注册所有函数
    .nm_modname = "memory_decompressor",  // 模块名，对应 import 的包名
    .nm_priv = ((void*)0),
    .reserved = { 0 },
};

// 构造函数：在动态库加载时自动注册 NAPI 模块
// __attribute__((constructor)) 保证在 dlopen 时自动执行
extern "C" __attribute__((constructor)) void RegisterDecompressorModule(void)
{
    napi_module_register(&decompressorModule);
}