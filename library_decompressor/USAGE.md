# library_decompressor 使用文档

基于 N-API 的 HarmonyOS 内存压缩/解压缩库，支持 **ZSTD**、**LZ4 Frame** 和 **Snappy** 三种压缩格式。

---

## 目录

- [安装](#安装)
- [构建 HAR 包](#构建-har-包)
- [整体架构](#整体架构)
- [NAPI 函数总览](#napi-函数总览)
- [ArkTS 封装层](#arkts-封装层)
- [ZSTD 函数详解](#zstd-函数详解)
- [LZ4 函数详解](#lz4-函数详解)
- [Snappy 函数详解](#snappy-函数详解)
- [工具函数详解](#工具函数详解)
- [完整示例](#完整示例)
- [常见问题](#常见问题)

---

## 安装

### 方式一：通过 OHPM 中心仓安装

```bash
ohpm install library_decompressor
```

### 方式二：本地 HAR 文件引用

```json
"dependencies": {
  "library_decompressor": "file:./libs/library_decompressor.har"
}
```

### 方式三：本地模块引用（源码依赖）

在 `build-profile.json5` 的 `modules` 中添加：

```json5
{
  "name": "library_decompressor",
  "srcPath": "./library_decompressor"
}
```

然后在 `oh-package.json5` 中添加依赖：

```json5
"dependencies": {
  "library_decompressor": {
    "version": "1.0.0",
    "path": "./library_decompressor"
  }
}
```

---

## 构建 HAR 包

如果你修改了源码（如 C++ NAPI 层或 ArkTS 封装层），需要重新构建 HAR 包供其他项目使用。

### 步骤一：修改版本号（可选）

编辑 [oh-package.json5](file:///d:/harmony_memory_decompressor/library_decompressor/oh-package.json5) 中的 `version` 字段。

### 步骤二：构建 HAR

在 DevEco Studio 中：

1. 打开 `library_decompressor` 模块
2. 菜单栏选择 **Build → Build Hap(s)/APP(s) → Build HAR**
3. 构建产物生成在 `library_decompressor/build/default/outputs/default/library_decompressor.har`

或使用命令行：

```bash
cd d:\harmony_memory_decompressor
hvigorw -p module=library_decompressor -p product=default assembleHar
```

### 步骤三：引用 HAR 包

将生成的 `.har` 文件复制到目标项目的 `libs/` 目录，并在 `oh-package.json5` 中添加：

```json5
"dependencies": {
  "library_decompressor": "file:./libs/library_decompressor.har"
}
```

### 步骤四：发布到 OHPM（可选）

```bash
# 登录 ohpm 账号
ohpm login

# 发布
ohpm publish library_decompressor/build/default/outputs/default/library_decompressor.har
```

---

## 整体架构

```
┌───────────────────────────────────────────────┐
│            ArkTS 调用层                        │
│                                               │
│  import { CompressorFactory }                │
│  import compressor from 'libmemory.so'       │
└───────────────────┬───────────────────────────┘
                    │
┌───────────────────▼───────────────────────────┐
│       ArkTS 封装层（ICompressor 接口）         │
│                                               │
│  ZstdCompressor : ICompressor                │
│  Lz4Compressor  : ICompressor                │
│  SnappyCompressor: ICompressor               │
└───────────────────┬───────────────────────────┘
                    │
┌───────────────────▼───────────────────────────┐
│            N-API 桥接层（napi_init.cpp）       │
│                                               │
│  napi_create_async_work → 线程池执行          │
│  napi_create_promise → 返回 Promise          │
│  MemoryDecompressor 统一门面                  │
└───────────────────┬───────────────────────────┘
                    │
┌───────────────────▼───────────────────────────┐
│         C/C++ 原生压缩库                       │
│                                               │
│  ZSTD  →  zstd.h （zstd_compress/decompress） │
│  LZ4   →  lz4frame.h （LZ4F Frame API）       │
│  Snappy→  snappy-c.h （snappy_c/d）           │
└───────────────────────────────────────────────┘
```

**关键说明**：

| 特性 | 说明 |
|------|------|
| **异步** | 所有压缩/解压函数都是异步的，在 NAPI 线程池中执行，不阻塞主线程 |
| **块式（One-Shot）** | ZSTD、Snappy：一次性传入全部数据，一次性返回全部结果 |
| **流式（Streaming）** | LZ4 Frame：内部采用帧结构，使用 `compressBegin → compressUpdate → compressEnd` 流式三段式 API |
| **零拷贝** | 输入 ArrayBuffer 直接引用，不额外拷贝内存 |
| **Promise** | 所有异步函数返回 `Promise<ArrayBuffer>`，可用 await 或 .then() |
| **统一门面** | `MemoryDecompressor` 类封装所有压缩算法的创建和执行，NAPI 层通过门面统一调用 |

---

## NAPI 函数总览

本库通过 `libmemory_decompressor.so` 原生模块暴露以下 9 个函数：

### 异步函数（返回 Promise）

| 函数名 | 格式 | 类型 | 执行模型 |
|--------|:----:|:----:|:--------:|
| `zstdCompressAsync` | ZSTD | 块式（One-Shot） | 异步（线程池） |
| `zstdDecompressAsync` | ZSTD | 块式（One-Shot） | 异步（线程池） |
| `lz4CompressAsync` | LZ4 Frame | 流式（Begin→Update→End） | 异步（线程池） |
| `lz4DecompressAsync` | LZ4 Frame | 流式（循环解块, 动态扩容） | 异步（线程池） |
| `snappyCompressAsync` | Snappy | 块式（One-Shot） | 异步（线程池） |
| `snappyDecompressAsync` | Snappy | 块式（One-Shot） | 异步（线程池） |

### 同步函数（直接返回值）

| 函数名 | 格式 | 类型 | 执行模型 |
|--------|:----:|:----:|:--------:|
| `version` | 通用 | 同步 | 主线程 |
| `getSupportedFormats` | 通用 | 同步 | 主线程 |
| `detectFormat` | 通用 | 同步 | 主线程 |

### 错误处理方式

| 函数类型 | 成功 | 失败 |
|----------|------|------|
| 异步函数 | resolve(ArrayBuffer) | reject(Error) |
| 同步函数 | 直接返回值 | throw Error |

---

## ArkTS 封装层

建议通过封装层调用，它提供了统一的接口和错误处理。

### ICompressor 接口

文件：[ICompressor.ets](file:///d:/harmony_memory_decompressor/library_decompressor/src/main/ets/compress/ICompressor.ets)

```typescript
export interface ICompressor {
  compressAsync(data: ArrayBuffer, level?: number): Promise<ArrayBuffer>;
  decompressAsync(data: ArrayBuffer): Promise<ArrayBuffer>;
  getVersion(): string;
  getFormatName(): string;
}
```

| 方法 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `compressAsync` | `data`: 原始数据, `level?`: 压缩级别 | `Promise<ArrayBuffer>` | 异步压缩，不阻塞主线程 |
| `decompressAsync` | `data`: 压缩后的数据 | `Promise<ArrayBuffer>` | 异步解压，不阻塞主线程 |
| `getVersion` | 无 | `string` | 同步获取库版本号 |
| `getFormatName` | 无 | `string` | 同步获取格式名称 |

### CompressorFactory 工厂类

文件：[CompressorFactory.ets](file:///d:/harmony_memory_decompressor/library_decompressor/src/main/ets/compress/CompressorFactory.ets)

```typescript
export class CompressorFactory {
  static create(type: 'zstd' | 'lz4' | 'snappy'): ICompressor;
}
```

| 参数 type | 返回实例 | 说明 |
|-----------|----------|------|
| `'zstd'` | `ZstdCompressor` | ZSTD 压缩器 |
| `'lz4'` | `Lz4Compressor` | LZ4 Frame 压缩器 |
| `'snappy'` | `SnappyCompressor` | Snappy 压缩器 |

### 三种压缩器实现对比

| 特性 | ZstdCompressor | Lz4Compressor | SnappyCompressor |
|------|:--------------:|:-------------:|:----------------:|
| 底层 API | ZSTD_compress / ZSTD_decompress | LZ4F Frame API（三段式流式） | snappy_compress / snappy_uncompress |
| 异步 | ✅ 线程池 | ✅ 线程池 | ✅ 线程池 |
| 压缩级别 | 支持 `level` 参数 | 支持 `level` 参数 | 忽略 `level` 参数 |
| 缓冲区分配 | 主线程预先分配 | 主线程预先分配 | 主线程预先分配 |
| 输出裁剪 | ✅ 自动裁剪 | ✅ 自动裁剪 | ✅ 自动裁剪 |
| 内部结构 | 单次调用 | Begin→Update→End 三段式 | 单次调用 |
| 输入数据访问 | 零拷贝直接引用 | 零拷贝直接引用 | 零拷贝直接引用 |
| 出厂默认级别 | 3 | 0（LZ4 默认） | 不支持级别 |

### 错误处理

```typescript
import { CompressorFactory } from 'library_decompressor';

async function safeDecompress(data: ArrayBuffer): Promise<ArrayBuffer> {
  try {
    const compressor = CompressorFactory.create('zstd');
    return await compressor.decompressAsync(data);
  } catch (err) {
    // err.message 包含具体错误信息
    // ZSTD: ZSTD_getErrorName(errorCode)
    // LZ4: LZ4F_getErrorName(errorCode)
    // Snappy: "Snappy compression/decompression failed"
    console.error('解压缩失败:', err.message);
    throw err;
  }
}
```

---

## ZSTD 函数详解

### zstdCompressAsync

```typescript
compressor.zstdCompressAsync(data: ArrayBuffer, level?: number): Promise<ArrayBuffer>
```

| 属性 | 说明 |
|------|------|
| **类型** | 异步块式压缩（One-Shot） |
| **执行模型** | NAPI 线程池 |
| **返回值** | Promise，成功 resolve(ArrayBuffer)，失败 reject(Error) |
| **底层 API** | `ZSTD_compress()` |
| **内部流程** | `ZSTD_compressBound` 计算最大大小 → 分配输出缓冲区 → 后台线程执行 ZSTD_compress → 自动裁剪精确大小 |

**参数**：

| 参数 | 类型 | 必须 | 默认值 | 说明 |
|------|------|:----:|:------:|------|
| `data` | ArrayBuffer | 是 | — | 待压缩的原始数据 |
| `level` | number | 否 | 3 | 压缩级别，范围 1~22。越大压缩率越高，但压缩速度越慢 |

> **注意**：`level` 参数可传 `undefined` 或不传，NAPI 层会自动使用默认值 3，不会报错。

**压缩级别参考**：

| 级别 | 速度 | 压缩率 | 场景 |
|:----:|:----:|:------:|------|
| 1 | 最快 | 最低 | 实时性优先 |
| 3（默认） | 快 | 中等 | 通用场景 |
| 6 | 中等 | 较高 | 平衡场景 |
| 10+ | 慢 | 高 | 离线压缩，存储场景 |
| 19~22 | 极慢 | 最高 | 冷数据归档 |

**返回**：成功时 resolve 的 ArrayBuffer 存储压缩后的数据（ZSTD Frame 格式，包含帧头 + 压缩数据块 + 校验和）

**错误示例**：
- `"error (generic)"` — 压缩级别超出范围
- `"dst buffer is too small"` — 输出缓冲区不足

**技术细节**：
- 输入输出 ArrayBuffer 均在主线程创建，后台线程只做指针访问（零拷贝）
- 使用 `napi_ref` 防止 GC 在异步执行期间回收 ArrayBuffer
- 压缩完成后，如果实际大小小于分配的缓冲区，会创建精确大小的新 ArrayBuffer 返回

---

### zstdDecompressAsync

```typescript
compressor.zstdDecompressAsync(data: ArrayBuffer): Promise<ArrayBuffer>
```

| 属性 | 说明 |
|------|------|
| **类型** | 异步块式解压（One-Shot） |
| **执行模型** | NAPI 线程池 |
| **返回值** | Promise，成功 resolve(ArrayBuffer)，失败 reject(Error) |
| **底层 API** | `ZSTD_getFrameContentSize()` + `ZSTD_decompress()` |
| **内部流程** | `ZSTD_getFrameContentSize` 读取帧头获取原始大小 → 分配输出缓冲区 → 后台线程执行 ZSTD_decompress |

**参数**：

| 参数 | 类型 | 必须 | 说明 |
|------|------|:----:|------|
| `data` | ArrayBuffer | 是 | ZSTD Frame 格式的压缩数据 |

**返回**：成功时 resolve 的 ArrayBuffer 存储解压后的原始数据

**错误示例**：
- `"Cannot determine decompressed size"` — ZSTD 帧头损坏或不包含 contentSize
- `"error (generic)"` — 数据损坏或格式不匹配

**技术细节**：
- 必须先通过 `ZSTD_getFrameContentSize` 获取解压后大小以分配输出缓冲区
- 如果帧头不含 contentSize（如流式生成的 ZSTD 数据），会抛错
- 输出缓冲区在主线程预先分配，大小精确

---

## LZ4 函数详解

### lz4CompressAsync

```typescript
compressor.lz4CompressAsync(data: ArrayBuffer, level?: number): Promise<ArrayBuffer>
```

| 属性 | 说明 |
|------|------|
| **类型** | 异步流式压缩（Streaming） |
| **执行模型** | NAPI 线程池 |
| **返回值** | Promise，成功 resolve(ArrayBuffer)，失败 reject(Error) |
| **底层 API** | LZ4F Frame API：`LZ4F_createCompressionContext` → `LZ4F_compressBegin` → `LZ4F_compressUpdate` → `LZ4F_compressEnd` |
| **内部流程** | 创建上下文 → 写入帧头（魔数 0x184D2204 + 元数据） → 压缩数据块 → 写入帧尾（结束标记 + 校验和） → 释放上下文 |

**参数**：

| 参数 | 类型 | 必须 | 默认值 | 说明 |
|------|------|:----:|:------:|------|
| `data` | ArrayBuffer | 是 | — | 待压缩的原始数据 |
| `level` | number | 否 | 0（LZ4 默认） | 压缩级别 1~12。0 表示 LZ4 默认值 |

> **注意**：`level` 参数可传 `undefined` 或不传，NAPI 层会自动使用默认值 0，不会报错。
> LZ4 的 `level=1` 以上会启用 **LZ4 HC**（High Compression）模式，压缩率更高但速度明显变慢。解压速度不受压缩级别影响。

**压缩级别参考**：

| 级别 | 模式 | 速度 | 压缩率 | 场景 |
|:----:|:----:|:----:|:------:|------|
| 0~1 | LZ4 | 极快 | 低 | 实时压缩、日志、网络传输 |
| 3~6 | LZ4 HC | 中等 | 中等 | 通用场景 |
| 9~12 | LZ4 HC | 慢 | 高 | 离线压缩 |

**返回**：LZ4 Frame 格式的压缩数据（魔数 0x184D2204 开头）

**错误示例**：
- `"Max output size ... is reached"` — 输出缓冲区不足
- `"Error ... : request is invalid"` — 无效参数

**技术细节**：
- 输出缓冲区大小通过 `LZ4F_compressBound(input_size, NULL) + 64` 计算（+64 给帧头和帧尾）
- 使用流式三段式 API 而非 `LZ4F_compressFrame`（后者是简化版，限制了流式扩展能力）
- 压缩上下文（`LZ4F_cctx`）在后台线程创建和销毁

---

### lz4DecompressAsync

```typescript
compressor.lz4DecompressAsync(data: ArrayBuffer): Promise<ArrayBuffer>
```

| 属性 | 说明 |
|------|------|
| **类型** | 异步流式解压（Streaming，循环解块） |
| **执行模型** | NAPI 线程池 |
| **返回值** | Promise，成功 resolve(ArrayBuffer)，失败 reject(Error) |
| **底层 API** | `LZ4F_createDecompressionContext` → `LZ4F_getFrameInfo` → `LZ4F_decompress`（循环） |
| **内部流程** | 创建上下文 → 读取帧头获取 contentSize → 分配输出缓冲区 → while 循环解压每个数据块直到 hint==0 → 动态扩容 → 释放上下文 |

**参数**：

| 参数 | 类型 | 必须 | 说明 |
|------|------|:----:|------|
| `data` | ArrayBuffer | 是 | LZ4 Frame 格式的压缩数据 |

**流式解压循环机制**（内部实现）：

```
hint = LZ4F_getFrameInfo(dctx, &frame_info, input_data, &src_pos)
输出缓冲区 = 根据 frame_info.contentSize 分配

while (hint != 0):
  hint = LZ4F_decompress(dctx, output_buffer + dst_offset, &dst_size,
                         input_data + src_offset, &src_size, NULL)
  src_offset += src_size
  dst_offset += dst_size
  
  如果输出缓冲区不够 → 自动 2 倍扩容
```

**返回**：解压后的原始数据

**错误示例**：
- `"Output buffer overflow during LZ4 decompression"` — 输出缓冲区自动扩容超过限制（最多尝试 10 次）
- `"Error ... : corrupt input"` — 输入数据损坏

**技术细节**：
- 输出缓冲区在**后台线程**动态分配（用 `new char[]`），因为在主线程无法确定大小
- 处理完后再通过 `napi_create_arraybuffer` 拷贝到 NAPI 可管理的 ArrayBuffer 返回
- 缓冲区扩容策略：初始按 `contentSize` 或 `input_size * 10`，不足时 2 倍扩容
- 与 ZSTD/Snappy 最大的不同：**输出缓冲区不在主线程预先分配**

---

## Snappy 函数详解

### snappyCompressAsync

```typescript
compressor.snappyCompressAsync(data: ArrayBuffer): Promise<ArrayBuffer>
```

| 属性 | 说明 |
|------|------|
| **类型** | 异步块式压缩（One-Shot） |
| **执行模型** | NAPI 线程池 |
| **返回值** | Promise，成功 resolve(ArrayBuffer)，失败 reject(Error) |
| **底层 API** | `snappy_max_compressed_length()` + `snappy_compress()` |
| **内部流程** | `snappy_max_compressed_length` 计算最大大小 → 分配输出缓冲区 → 后台线程执行 snappy_compress → 自动裁剪 |

**参数**：

| 参数 | 类型 | 必须 | 说明 |
|------|------|:----:|------|
| `data` | ArrayBuffer | 是 | 待压缩的原始数据 |

> **注意**：Snappy **不支持压缩级别**参数。`SnappyCompressor` 实现了 `ICompressor` 接口，但 `compressAsync` 的 `level` 参数会被忽略，传或不传无影响。

**返回**：Snappy 格式的压缩数据

**错误示例**：
- `"Snappy compression failed"` — 压缩执行失败

**技术细节**：
- `snappy_max_compressed_length` 保证输出缓冲区足够容纳任何输入的最大压缩结果
- Snappy 专注于速度而非压缩率，设计目标为 ~250MB/s 的压缩速度和 ~500MB/s 的解压速度
- Snappy 格式不包含帧头元数据（与 ZSTD/LZ4 不同），解压时必须传递完整压缩数据

---

### snappyDecompressAsync

```typescript
compressor.snappyDecompressAsync(data: ArrayBuffer): Promise<ArrayBuffer>
```

| 属性 | 说明 |
|------|------|
| **类型** | 异步块式解压（One-Shot） |
| **执行模型** | NAPI 线程池 |
| **返回值** | Promise，成功 resolve(ArrayBuffer)，失败 reject(Error) |
| **底层 API** | `snappy_uncompressed_length()` + `snappy_uncompress()` |
| **内部流程** | `snappy_uncompressed_length` 读取压缩数据头部获取原始大小 → 分配输出缓冲区 → 后台线程执行 snappy_uncompress |

**参数**：

| 参数 | 类型 | 必须 | 说明 |
|------|------|:----:|------|
| `data` | ArrayBuffer | 是 | Snappy 格式的压缩数据 |

**返回**：解压后的原始数据

**错误示例**：
- `"Snappy decompression failed"` — 数据损坏或格式不匹配

**技术细节**：
- `snappy_uncompressed_length` 是同步函数，在主线程执行，用于确定输出缓冲区大小
- 输出缓冲区在主线程预先分配，大小精确
- Snappy 解压速度极快，设计上比 ZSTD 快约 2 倍，比 LZ4 快约 1.5 倍

---

## 工具函数详解

### version

```typescript
compressor.version(): string
```

| 属性 | 说明 |
|------|------|
| **类型** | 同步纯函数 |
| **执行模型** | 主线程（瞬间完成） |
| **返回值** | 版本信息字符串 |

**返回值示例**：`"MemoryDecompressor v1.0.1 (ZSTD 1.5.6, LZ4 1.10.0, Snappy 1.2.1)"`

### getSupportedFormats

```typescript
compressor.getSupportedFormats(): string[]
```

| 属性 | 说明 |
|------|------|
| **类型** | 同步纯函数 |
| **执行模型** | 主线程（瞬间完成） |
| **返回值** | 支持的压缩格式名称数组 |

**返回值示例**：`["zstd", "lz4", "snappy"]`

### detectFormat

```typescript
compressor.detectFormat(data: ArrayBuffer): string
```

| 属性 | 说明 |
|------|------|
| **类型** | 同步函数 |
| **执行模型** | 主线程（瞬间完成） |
| **返回值** | 检测到的压缩格式名称 |
| **检测方式** | 按魔数（Magic Number）识别 |

**魔数对照表**：

| 格式 | 魔数（16 进制） | 前 4 字节 |
|------|:---------------:|:---------:|
| ZSTD | `0xFD2FB528` | `\xFD\x2F\xB5\x28` |
| LZ4 Frame | `0x184D2204` | `\x18\x4D\x22\x04` |
| Snappy | — | 无固定魔数，返回 UNKNOWN |
| 无法识别 | — | `"UNKNOWN"` |

> **注意**：Snappy 格式没有魔数，`detectFormat` 无法通过前 4 字节识别 Snappy。

**使用示例**：

```typescript
import compressor from 'libmemory_decompressor.so';

function guessAndDecompress(data: ArrayBuffer): Promise<ArrayBuffer> {
  const format = compressor.detectFormat(data);

  switch (format) {
    case 'ZSTD':
      return compressor.zstdDecompressAsync(data);
    case 'LZ4':
      return compressor.lz4DecompressAsync(data);
    case 'SNAPPY':
      return compressor.snappyDecompressAsync(data);
    default:
      throw new Error('无法识别的压缩格式: ' + format);
  }
}
```

---

## 完整示例

### 示例 1：字符串压缩解压

```typescript
import { CompressorFactory } from 'library_decompressor';
import { util } from '@kit.ArkTS';

async function stringDemo() {
  const text = 'Hello HarmonyOS! 这是一段测试文本。';
  const encoder = util.TextEncoder.create('utf-8');
  const decoder = util.TextDecoder.create('utf-8');

  // 1. 字符串 → ArrayBuffer
  const inputBuffer = encoder.encodeInto(text).buffer as ArrayBuffer;
  console.log('原始大小:', inputBuffer.byteLength);

  // 2. 创建 ZSTD 压缩器（可换成 'lz4' 或 'snappy'）
  const compressor = CompressorFactory.create('zstd');

  // 3. 压缩（不传 level 参数，使用默认级别）
  const compressed = await compressor.compressAsync(inputBuffer);
  console.log('压缩后大小:', compressed.byteLength);

  // 4. 解压
  const decompressed = await compressor.decompressAsync(compressed);

  // 5. ArrayBuffer → 字符串
  const result = decoder.decodeToString(new Uint8Array(decompressed));
  console.log('解压结果:', result);
}
```

### 示例 2：二进制数据压缩

```typescript
import { CompressorFactory } from 'library_decompressor';

async function binaryDemo() {
  // 创建 1MB 的测试数据
  const size = 1024 * 1024;
  const data = new ArrayBuffer(size);
  const view = new Uint8Array(data);
  for (let i = 0; i < size; i++) {
    view[i] = i % 256;
  }

  console.log('原始大小:', data.byteLength);

  // LZ4 压缩（速度快，适合实时场景）
  const lz4 = CompressorFactory.create('lz4');
  const compressed = await lz4.compressAsync(data, 1);
  console.log('LZ4 压缩后:', compressed.byteLength);

  const decompressed = await lz4.decompressAsync(compressed);
  console.log('LZ4 解压后:', decompressed.byteLength);
}
```

### 示例 3：三种格式对比

```typescript
import { CompressorFactory } from 'library_decompressor';

interface BenchmarkResult {
  format: string;
  compressedSize: number;
  compressionTime: number;
  decompressionTime: number;
}

async function benchmark(data: ArrayBuffer): Promise<BenchmarkResult[]> {
  const results: BenchmarkResult[] = [];

  for (const type of ['zstd', 'lz4', 'snappy'] as const) {
    const compressor = CompressorFactory.create(type);

    // 压缩计时
    const t1 = performance.now();
    const compressed = await compressor.compressAsync(data);
    const t2 = performance.now();

    // 解压计时
    const decompressed = await compressor.decompressAsync(compressed);
    const t3 = performance.now();

    results.push({
      format: type.toUpperCase(),
      compressedSize: compressed.byteLength,
      compressionTime: t2 - t1,
      decompressionTime: t3 - t2,
    });
  }

  return results;
}

// 使用
async function runBenchmark() {
  const testData = new ArrayBuffer(1024 * 1024); // 1MB
  const results = await benchmark(testData);

  for (const r of results) {
    console.log(`${r.format}: 压缩后=${r.compressedSize}B, ` +
                 `压缩时间=${r.compressionTime.toFixed(1)}ms, ` +
                 `解压时间=${r.decompressionTime.toFixed(1)}ms`);
  }
}
```

### 示例 4：直接使用底层 NAPI

```typescript
import compressor from 'libmemory_decompressor.so';

async function directNapiDemo(data: ArrayBuffer) {
  // 查版本
  console.log('版本:', compressor.version());

  // 查支持的格式
  console.log('支持的格式:', compressor.getSupportedFormats());

  // 直接解压，自动检测格式
  const format = compressor.detectFormat(data);
  console.log('检测到格式:', format);

  let result: ArrayBuffer;
  switch (format) {
    case 'ZSTD':
      result = await compressor.zstdDecompressAsync(data);
      break;
    case 'LZ4':
      result = await compressor.lz4DecompressAsync(data);
      break;
    case 'SNAPPY':
      result = await compressor.snappyDecompressAsync(data);
      break;
    default:
      throw new Error('未知格式');
  }

  return result;
}
```

---

## 常见问题

### Q1: 三种格式怎么选？

| 格式 | 压缩率 | 压缩速度 | 解压速度 | 适用场景 |
|------|:------:|:--------:|:--------:|----------|
| **ZSTD** | ⭐⭐⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐⭐⭐ | 通用场景，需要高压缩比。默认级别 3 兼顾速度和压缩率 |
| **LZ4** | ⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | 实时/高频场景。解压速度极快，压缩率较低 |
| **Snappy** | ⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | Google 生态兼容。速度优先，压缩率中等 |

建议按以下规则选择：
- **存储空间紧张** → ZSTD（级别 3~6）
- **实时响应要求极高** → LZ4（级别 1）或 Snappy
- **与 Google/Android 系统交互** → Snappy（原生格式）
- **不确定** → ZSTD（默认级别 3，各方面均衡）

### Q2: 异步和同步的区别？

所有压缩/解压函数都是**异步**的：
- 在 NAPI 线程池（libuv）中执行
- 不阻塞 UI 线程
- 返回 `Promise<ArrayBuffer>`
- 支持 `await` 或 `.then()`

工具函数（`version`, `getSupportedFormats`, `detectFormat`）是**同步**的：
- 在主线程瞬间完成（只做字符串返回或魔数检查）
- 不需要等待

### Q3: level 参数可以不传吗？

可以。ZSTD 和 LZ4 的 `compressAsync` 的 `level` 参数是**可选的**：

| 格式 | 不传/传 undefined 时的默认值 |
|------|:---------------------------:|
| ZSTD | 3 |
| LZ4 | 0（LZ4 原生默认值） |
| Snappy | 不支持级别，忽略参数 |

内部通过 `napi_typeof` 检查参数类型，仅在参数为 `number` 时才解析，避免因 `undefined` 导致 "Invalid compression level" 错误。

### Q4: 输出缓冲区怎么分配的？

| 格式 | 压缩输出 | 解压输出 |
|------|----------|----------|
| ZSTD | 主线程：`ZSTD_compressBound` | 主线程：`ZSTD_getFrameContentSize` |
| LZ4 | 主线程：`LZ4F_compressBound + 64` | **后台线程**：new[] 动态分配，自动扩容 |
| Snappy | 主线程：`snappy_max_compressed_length` | 主线程：`snappy_uncompressed_length` |

LZ4 解压是唯一一个在后台线程分配缓冲区的，因为其帧头 contentSize 可能缺失。

### Q5: 最大支持多大的数据？

取决于设备可用内存。建议：
- **< 10MB**：无任何顾虑，直接压缩
- **10~100MB**：确认设备可用内存充足
- **> 100MB**：建议分片处理，避免单次 OOM

### Q6: 传入的 ArrayBuffer 会在异步执行期间被回收吗？

不会。NAPI 层通过 `napi_create_reference` 创建引用（ref count = 1），阻止 GC 在异步工作完成前回收 ArrayBuffer。工作完成后在 Complete 回调中 `napi_delete_reference` 释放引用。

### Q7: 支持流式压缩吗？

当前版本的 NAPI 接口是**全量数据一次性处理**（All-in-One）模式：

| 格式 | 底层 API | 是否支持流式 |
|------|----------|:-----------:|
| ZSTD | ZSTD_compress | ❌ 全量 |
| LZ4 | LZ4F Frame API | ✅ 内部流式（但 NAPI 层封装为全量） |
| Snappy | snappy_compress | ❌ 全量 |

如果需要流式处理大数据，需在 ArkTS 层将数据分片后多次调用。

### Q8: LZ4 和 LZ4 Frame 有什么区别？

| 概念 | 说明 |
|------|------|
| **LZ4** | 基础压缩算法，只压缩单个数据块 |
| **LZ4 Frame** | 标准容器格式（RFC 标准），包含帧头 + 多个数据块 + 帧尾 + 校验和 |

本库使用 **LZ4 Frame** 格式（`lz4frame.h`），生成的压缩数据以 `0x184D2204` 魔数开头，与 `lz4` 命令行工具兼容。

---

## 开源协议

Apache-2.0