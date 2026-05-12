#include "napi/native_api.h"
#include "memory_decompressor.h"
#include "zstd.h"
#include <cstring>

static napi_value ZstdCompress(napi_env env, napi_callback_info info) {
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

    void* input_data = nullptr;
    size_t input_size = 0;
    status = napi_get_arraybuffer_info(env, args[0], &input_data, &input_size);
    if (status != napi_ok) {
        napi_throw_error(env, nullptr, "First argument must be ArrayBuffer");
        return nullptr;
    }

    int32_t level = 3;
    if (argc >= 2) {
        status = napi_get_value_int32(env, args[1], &level);
        if (status != napi_ok) {
            napi_throw_error(env, nullptr, "Invalid compression level");
            return nullptr;
        }
    }

    size_t max_size = ZSTD_compressBound(input_size);

    void* output_data = nullptr;
    napi_value arraybuffer;
    status = napi_create_arraybuffer(env, max_size, &output_data, &arraybuffer);
    if (status != napi_ok) {
        napi_throw_error(env, nullptr, "Failed to create output buffer");
        return nullptr;
    }

    size_t compressed_size = ZSTD_compress(output_data, max_size, input_data, input_size, level);

    if (ZSTD_isError(compressed_size)) {
        napi_throw_error(env, nullptr, ZSTD_getErrorName(compressed_size));
        return nullptr;
    }

    if (compressed_size < max_size) {
        void* exact_output = nullptr;
        napi_value exact_buffer;
        status = napi_create_arraybuffer(env, compressed_size, &exact_output, &exact_buffer);
        if (status == napi_ok) {
            std::memcpy(exact_output, output_data, compressed_size);
            return exact_buffer;
        }
    }

    return arraybuffer;
}

static napi_value ZstdDecompress(napi_env env, napi_callback_info info) {
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

    unsigned long long decompressed_size = ZSTD_getFrameContentSize(input_data, input_size);

    if (decompressed_size == ZSTD_CONTENTSIZE_UNKNOWN) {
        napi_throw_error(env, nullptr, "Cannot determine decompressed size: unknown");
        return nullptr;
    }

    if (decompressed_size == ZSTD_CONTENTSIZE_ERROR) {
        napi_throw_error(env, nullptr, "Cannot determine decompressed size: error");
        return nullptr;
    }

    void* output_data = nullptr;
    napi_value arraybuffer;
    status = napi_create_arraybuffer(env, decompressed_size, &output_data, &arraybuffer);
    if (status != napi_ok) {
        napi_throw_error(env, nullptr, "Failed to create output buffer");
        return nullptr;
    }

    size_t actual_decompressed_size = ZSTD_decompress(
        output_data, decompressed_size,
        input_data, input_size
    );

    if (ZSTD_isError(actual_decompressed_size)) {
        napi_throw_error(env, nullptr, ZSTD_getErrorName(actual_decompressed_size));
        return nullptr;
    }

    return arraybuffer;
}

static napi_value ZstdVersion(napi_env env, napi_callback_info info) {
    std::string version = md_compressor::MemoryDecompressor::get_version();

    napi_value result;
    napi_status status = napi_create_string_utf8(env, version.c_str(), version.length(), &result);
    if (status != napi_ok) {
        napi_throw_error(env, nullptr, "Failed to create version string");
        return nullptr;
    }
    return result;
}

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

// ===== 异步压缩数据结构 =====
struct AsyncCompressData {
    void* input_data;
    size_t input_size;
    int32_t level;
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

// ===== 异步解压数据结构 =====
struct AsyncDecompressData {
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

// ===== 异步压缩 - 工作线程执行 =====
static void AsyncCompressExecute(napi_env env, void* data) {
    AsyncCompressData* async_data = static_cast<AsyncCompressData*>(data);

    size_t result = ZSTD_compress(
        async_data->output_data, async_data->output_capacity,
        async_data->input_data, async_data->input_size,
        async_data->level);

    if (ZSTD_isError(result)) {
        async_data->success = false;
        async_data->error_message = ZSTD_getErrorName(result);
    } else {
        async_data->success = true;
        async_data->compressed_size = result;
    }
}

// ===== 异步压缩 - 主线程完成 =====
static void AsyncCompressComplete(napi_env env, napi_status status, void* data) {
    AsyncCompressData* async_data = static_cast<AsyncCompressData*>(data);

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

// ===== 异步压缩 NAPI 接口 =====
static napi_value ZstdCompressAsync(napi_env env, napi_callback_info info) {
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

    void* input_data = nullptr;
    size_t input_size = 0;
    status = napi_get_arraybuffer_info(env, args[0], &input_data, &input_size);
    if (status != napi_ok) {
        napi_throw_error(env, nullptr, "First argument must be ArrayBuffer");
        return nullptr;
    }

    int32_t level = 3;
    if (argc >= 2) {
        status = napi_get_value_int32(env, args[1], &level);
        if (status != napi_ok) {
            napi_throw_error(env, nullptr, "Invalid compression level");
            return nullptr;
        }
    }

    size_t max_size = ZSTD_compressBound(input_size);

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

    AsyncCompressData* async_data = new AsyncCompressData();
    async_data->input_data = input_data;
    async_data->input_size = input_size;
    async_data->level = level;
    async_data->input_ref = input_ref;
    async_data->output_data = output_data;
    async_data->output_ref = output_ref;
    async_data->output_capacity = max_size;
    async_data->compressed_size = 0;
    async_data->success = false;
    async_data->deferred = deferred;

    napi_value resource_name;
    napi_create_string_utf8(env, "ZstdCompressAsync", NAPI_AUTO_LENGTH, &resource_name);

    status = napi_create_async_work(env, nullptr, resource_name,
        AsyncCompressExecute, AsyncCompressComplete,
        async_data, &async_data->work);
    if (status != napi_ok) {
        napi_delete_reference(env, input_ref);
        napi_delete_reference(env, output_ref);
        delete async_data;
        napi_throw_error(env, nullptr, "Failed to create async work");
        return nullptr;
    }

    napi_queue_async_work(env, async_data->work);

    return promise;
}

// ===== 异步解压 - 工作线程执行 =====
static void AsyncDecompressExecute(napi_env env, void* data) {
    AsyncDecompressData* async_data = static_cast<AsyncDecompressData*>(data);

    size_t result = ZSTD_decompress(
        async_data->output_data, async_data->output_size,
        async_data->input_data, async_data->input_size);

    if (ZSTD_isError(result)) {
        async_data->success = false;
        async_data->error_message = ZSTD_getErrorName(result);
    } else {
        async_data->success = true;
    }
}

// ===== 异步解压 - 主线程完成 =====
static void AsyncDecompressComplete(napi_env env, napi_status status, void* data) {
    AsyncDecompressData* async_data = static_cast<AsyncDecompressData*>(data);

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

// ===== 异步解压 NAPI 接口 =====
static napi_value ZstdDecompressAsync(napi_env env, napi_callback_info info) {
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

    unsigned long long decompressed_size = ZSTD_getFrameContentSize(input_data, input_size);
    if (decompressed_size == ZSTD_CONTENTSIZE_UNKNOWN) {
        napi_throw_error(env, nullptr, "Cannot determine decompressed size: unknown");
        return nullptr;
    }
    if (decompressed_size == ZSTD_CONTENTSIZE_ERROR) {
        napi_throw_error(env, nullptr, "Cannot determine decompressed size: error");
        return nullptr;
    }

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

    AsyncDecompressData* async_data = new AsyncDecompressData();
    async_data->input_data = input_data;
    async_data->input_size = input_size;
    async_data->input_ref = input_ref;
    async_data->output_data = output_data;
    async_data->output_ref = output_ref;
    async_data->output_size = decompressed_size;
    async_data->success = false;
    async_data->deferred = deferred;

    napi_value resource_name;
    napi_create_string_utf8(env, "ZstdDecompressAsync", NAPI_AUTO_LENGTH, &resource_name);

    status = napi_create_async_work(env, nullptr, resource_name,
        AsyncDecompressExecute, AsyncDecompressComplete,
        async_data, &async_data->work);
    if (status != napi_ok) {
        napi_delete_reference(env, input_ref);
        napi_delete_reference(env, output_ref);
        delete async_data;
        napi_throw_error(env, nullptr, "Failed to create async work");
        return nullptr;
    }

    napi_queue_async_work(env, async_data->work);

    return promise;
}

EXTERN_C_START
static napi_value Init(napi_env env, napi_value exports) {
    napi_property_descriptor desc[] = {
        { "compress", nullptr, ZstdCompress, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "decompress", nullptr, ZstdDecompress, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "compressAsync", nullptr, ZstdCompressAsync, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "decompressAsync", nullptr, ZstdDecompressAsync, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "version", nullptr, ZstdVersion, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "getSupportedFormats", nullptr, GetSupportedFormats, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "detectFormat", nullptr, DetectFormat, nullptr, nullptr, nullptr, napi_default, nullptr }
    };

    napi_status status = napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
    if (status != napi_ok) {
        napi_throw_error(env, nullptr, "Failed to define properties");
        return nullptr;
    }

    return exports;
}
EXTERN_C_END

static napi_module memory_decompressor_module = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = Init,
    .nm_modname = "memory_decompressor",
    .nm_priv = ((void*)0),
    .reserved = { 0 },
};

extern "C" __attribute__((constructor)) void RegisterMemory_decompressorModule(void) {
    napi_module_register(&memory_decompressor_module);
}