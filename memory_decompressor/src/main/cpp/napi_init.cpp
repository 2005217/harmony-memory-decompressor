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

EXTERN_C_START
static napi_value Init(napi_env env, napi_value exports) {
    napi_property_descriptor desc[] = {
        { "compress", nullptr, ZstdCompress, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "decompress", nullptr, ZstdDecompress, nullptr, nullptr, nullptr, napi_default, nullptr },
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