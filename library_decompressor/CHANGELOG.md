# 更新日志

## 1.0.1 (2026-05-24)

- 修复 ZstdCompressAsync 和 Lz4CompressAsync 中 level 参数为 undefined 时抛出 "Invalid compression level" 的问题
- NAPI 层增加 `napi_typeof` 类型检查，仅在 level 为 number 时解析
- 完善 USAGE.md，增加构建 HAR 包说明和常见问题

## 1.0.0 (2026-05-24)

- 初始发布
- 支持 ZSTD、LZ4 Frame、Snappy 三种压缩格式
- 异步 N-API 接口
- 提供统一的 `CompressorFactory` 工厂类