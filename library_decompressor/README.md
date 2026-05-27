# library_decompressor

基于 N-API 的 HarmonyOS 内存压缩/解压缩库，支持 ZSTD、LZ4 Frame 和 Snappy 三种压缩格式。

## 功能特性

- **ZSTD** — Zstandard 标准压缩/解压缩，异步 N-API 接口
- **LZ4 Frame** — LZ4 Frame 格式压缩/解压缩，异步 N-API 接口
- **Snappy** — Google Snappy 压缩/解压缩，异步 N-API 接口

## 安装

```bash
ohpm install library_decompressor
```

## 使用示例

```typescript
import { CompressorFactory } from 'library_decompressor';

// ZSTD 压缩
const zstd = CompressorFactory.create('zstd');
zstd.compressAsync(data).then(result => {
  console.log('压缩后大小:', result.byteLength);
});

// LZ4 压缩
const lz4 = CompressorFactory.create('lz4');
lz4.compressAsync(data).then(result => {
  console.log('压缩后大小:', result.byteLength);
});

// Snappy 压缩
const snappy = CompressorFactory.create('snappy');
snappy.compressAsync(data).then(result => {
  console.log('压缩后大小:', result.byteLength);
});
```

## API

### ICompressor

- `compressAsync(data: ArrayBuffer, level?: number): Promise<ArrayBuffer>` — 异步压缩数据
- `decompressAsync(data: ArrayBuffer): Promise<ArrayBuffer>` — 异步解压缩数据

## 开源协议

Apache-2.0