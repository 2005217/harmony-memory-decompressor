#include "napi/native_api.h"
#include "zstd.h"

static napi_value ZstdCompress(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2] = {nullptr};
    napi_value this_arg = nullptr;
    void* data = nullptr;
    
    napi_status status = napi_get_cb_info(env, info, &argc, args, &this_arg, nullptr);
    if (status != napi_ok) {
        napi_throw_error(env, nullptr, "Failed to get callback info");
        return nullptr;
    }
    
    if (argc < 1) {
        napi_throw_error(env, nullptr, "Missing required argument: data");
        return nullptr;
    }
    
    napi_valuetype valuetype0;
    status = napi_typeof(env, args[0], &valuetype0);
    if (status != napi_ok || valuetype0 != napi_object) {
        napi_throw_error(env, nullptr, "First argument must be Uint8Array");
        return nullptr;
    }
    
    uint8_t* srcData = nullptr;
    size_t srcSize = 0;
    napi_value arraybuffer0 = nullptr;
    size_t byte_offset0 = 0;
    
    status = napi_get_typedarray_info(env, args[0], nullptr, &srcSize, (void**)&srcData, &arraybuffer0, &byte_offset0);
    if (status != napi_ok) {
        napi_throw_error(env, nullptr, "Failed to get typedarray info");
        return nullptr;
    }
    
    int32_t level = ZSTD_CLEVEL_DEFAULT;
    if (argc >= 2) {
        status = napi_get_value_int32(env, args[1], &level);
        if (status != napi_ok) {
            napi_throw_error(env, nullptr, "Invalid compression level");
            return nullptr;
        }
    }
    
    size_t maxDstSize = ZSTD_compressBound(srcSize);
    if (maxDstSize == 0) {
        napi_throw_error(env, nullptr, "Invalid source size");
        return nullptr;
    }
    
    uint8_t* dstData = nullptr;
    napi_value arraybuffer = nullptr;
    
    status = napi_create_arraybuffer(env, maxDstSize, (void**)&dstData, &arraybuffer);
    if (status != napi_ok) {
        napi_throw_error(env, nullptr, "Failed to create arraybuffer");
        return nullptr;
    }
    
    size_t compressedSize = ZSTD_compress(dstData, maxDstSize, srcData, srcSize, level);
    
    if (ZSTD_isError(compressedSize)) {
        napi_throw_error(env, nullptr, ZSTD_getErrorName(compressedSize));
        return nullptr;
    }
    
    napi_value result;
    status = napi_create_typedarray(env, napi_uint8_array, compressedSize, arraybuffer, 0, &result);
    if (status != napi_ok) {
        napi_throw_error(env, nullptr, "Failed to create typedarray");
        return nullptr;
    }
    
    return result;
}

static napi_value ZstdDecompress(napi_env env, napi_callback_info info) {
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
    
    uint8_t* srcData = nullptr;
    size_t srcSize = 0;
    napi_value arraybuffer0 = nullptr;
    size_t byte_offset0 = 0;
    
    status = napi_get_typedarray_info(env, args[0], nullptr, &srcSize, (void**)&srcData, &arraybuffer0, &byte_offset0);
    if (status != napi_ok) {
        napi_throw_error(env, nullptr, "Failed to get typedarray info");
        return nullptr;
    }
    
    size_t dstSize = ZSTD_getFrameContentSize(srcData, srcSize);
    if (dstSize == ZSTD_CONTENTSIZE_UNKNOWN) {
        napi_throw_error(env, nullptr, "Cannot determine decompressed size: unknown");
        return nullptr;
    }
    if (dstSize == ZSTD_CONTENTSIZE_ERROR) {
        napi_throw_error(env, nullptr, "Cannot determine decompressed size: error");
        return nullptr;
    }
    
    uint8_t* dstData = nullptr;
    napi_value arraybuffer = nullptr;
    
    status = napi_create_arraybuffer(env, dstSize, (void**)&dstData, &arraybuffer);
    if (status != napi_ok) {
        napi_throw_error(env, nullptr, "Failed to create arraybuffer");
        return nullptr;
    }
    
    size_t decompressedSize = ZSTD_decompress(dstData, dstSize, srcData, srcSize);
    
    if (ZSTD_isError(decompressedSize)) {
        napi_throw_error(env, nullptr, ZSTD_getErrorName(decompressedSize));
        return nullptr;
    }
    
    napi_value result;
    status = napi_create_typedarray(env, napi_uint8_array, decompressedSize, arraybuffer, 0, &result);
    if (status != napi_ok) {
        napi_throw_error(env, nullptr, "Failed to create typedarray");
        return nullptr;
    }
    
    return result;
}

static napi_value ZstdVersion(napi_env env, napi_callback_info info) {
    napi_value result;
    napi_status status = napi_create_string_utf8(env, ZSTD_versionString(), NAPI_AUTO_LENGTH, &result);
    if (status != napi_ok) {
        napi_throw_error(env, nullptr, "Failed to create version string");
        return nullptr;
    }
    return result;
}

EXTERN_C_START
static napi_value Init(napi_env env, napi_value exports) {
    napi_property_descriptor desc[] = {
        { "compress", nullptr, ZstdCompress, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "decompress", nullptr, ZstdDecompress, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "version", nullptr, ZstdVersion, nullptr, nullptr, nullptr, napi_default, nullptr }
    };
    
    napi_status status = napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
    if (status != napi_ok) {
        napi_throw_error(env, nullptr, "Failed to define properties");
        return nullptr;
    }
    
    return exports;
}
EXTERN_C_END

static napi_module demoModule = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = Init,
    .nm_modname = "memory_decompressor",
    .nm_priv = ((void*)0),
    .reserved = { 0 },
};

extern "C" __attribute__((constructor)) void RegisterMemory_decompressorModule(void) {
    napi_module_register(&demoModule);
}