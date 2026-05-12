# libmemory_decompressor NAPI 接口使用文档

## 导入模块

```arkts
import zstd from 'libmemory_decompressor.so'
```

## API 参考

### compressAsync

异步压缩数据。压缩操作在后台线程执行，不会阻塞 UI 线程。

**签名**：
```arkts
compressAsync(data: ArrayBuffer, level?: number): Promise<ArrayBuffer>
```

**参数**：

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| data | `ArrayBuffer` | 是 | - | 待压缩的原始数据 |
| level | `number` | 否 | `3` | 压缩级别（1=最快, 3=默认, 22=最高压缩率） |

**返回值**：`Promise<ArrayBuffer>` - 解析为压缩后的数据

**示例**：
```arkts
import { hilog } from '@kit.PerformanceAnalysisKit'

const DOMAIN = 0x0000
const originalText: string = 'Hello HarmonyOS ZSTD!'
const inputBuffer: Uint8Array = new Uint8Array(originalText.length)
for (let i = 0; i < originalText.length; i++) {
  inputBuffer[i] = originalText.charCodeAt(i)
}

zstd.compressAsync(inputBuffer.buffer)
  .then((compressed: ArrayBuffer) => {
    hilog.info(DOMAIN, 'ZstdDemo', '异步压缩完成, 大小: %{public}d 字节', compressed.byteLength)
  })
  .catch((e: Error) => {
    hilog.error(DOMAIN, 'ZstdDemo', '压缩失败: %{public}s', e.message)
  })
```

---

### decompressAsync

异步解压缩数据。解压操作在后台线程执行，不会阻塞 UI 线程。

**签名**：
```arkts
decompressAsync(data: ArrayBuffer): Promise<ArrayBuffer>
```

**参数**：

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| data | `ArrayBuffer` | 是 | 待解压的压缩数据 |

**返回值**：`Promise<ArrayBuffer>` - 解析为解压后的原始数据

**示例**：
```arkts
import { hilog } from '@kit.PerformanceAnalysisKit'

const DOMAIN = 0x0000
zstd.decompressAsync(compressedData)
  .then((decompressed: ArrayBuffer) => {
    const view: Uint8Array = new Uint8Array(decompressed)
    hilog.info(DOMAIN, 'ZstdDemo', '异步解压完成, 大小: %{public}d 字节', decompressed.byteLength)
  })
  .catch((e: Error) => {
    hilog.error(DOMAIN, 'ZstdDemo', '解压失败: %{public}s', e.message)
  })
```

---

### version

获取版本信息。

**签名**：
```arkts
version(): string
```

**返回值**：`string` - 版本信息字符串

**示例**：
```arkts
import { hilog } from '@kit.PerformanceAnalysisKit'

const DOMAIN = 0x0000
const ver: string = zstd.version()
hilog.info(DOMAIN, 'ZstdDemo', 'ZSTD 版本: %{public}s', ver)
// 输出示例: "MemoryDecompressor v1.0.0 (ZSTD 1.5.6)"
```

---

### getSupportedFormats

获取支持的压缩格式列表。

**签名**：
```arkts
getSupportedFormats(): string[]
```

**返回值**：`string[]` - 支持的压缩格式名称数组

**示例**：
```arkts
import { hilog } from '@kit.PerformanceAnalysisKit'

const DOMAIN = 0x0000
const formats: string[] = zstd.getSupportedFormats()
hilog.info(DOMAIN, 'ZstdDemo', '支持的格式: %{public}s', JSON.stringify(formats))
// 输出: ["ZSTD"]
```

---

### detectFormat

检测压缩数据的格式。

**签名**：
```arkts
detectFormat(data: ArrayBuffer): string
```

**参数**：

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| data | `ArrayBuffer` | 是 | 待检测的压缩数据 |

**返回值**：`string` - 压缩格式名称（如 `"ZSTD"`、`"UNKNOWN"`）

**示例**：
```arkts
import { hilog } from '@kit.PerformanceAnalysisKit'

const DOMAIN = 0x0000
const format: string = zstd.detectFormat(compressedData)
hilog.info(DOMAIN, 'ZstdDemo', '检测格式: %{public}s', format)
// 输出: "ZSTD"
```

---

## 完整使用示例

### 异步压缩与解压文本

```arkts
import { hilog } from '@kit.PerformanceAnalysisKit'
import zstd from 'libmemory_decompressor.so'

const DOMAIN = 0x0000

function stringToUint8Array(str: string): Uint8Array {
  const arr: Uint8Array = new Uint8Array(str.length)
  for (let i = 0; i < str.length; i++) {
    arr[i] = str.charCodeAt(i)
  }
  return arr
}

function uint8ArrayToString(arr: Uint8Array): string {
  let str: string = ''
  for (let i = 0; i < arr.length; i++) {
    str += String.fromCharCode(arr[i])
  }
  return str
}

const originalText: string = 'Hello HarmonyOS ZSTD Async!'
const originalData: Uint8Array = stringToUint8Array(originalText)

zstd.compressAsync(originalData.buffer)
  .then((compressed: ArrayBuffer) => {
    hilog.info(DOMAIN, 'ZstdDemo', '异步压缩完成: %{public}d → %{public}d 字节',
      originalData.length, compressed.byteLength)
    return zstd.decompressAsync(compressed)
  })
  .then((decompressed: ArrayBuffer) => {
    const view: Uint8Array = new Uint8Array(decompressed)
    const resultText: string = uint8ArrayToString(view)
    hilog.info(DOMAIN, 'ZstdDemo', '异步解压结果: %{public}s', resultText)
  })
  .catch((e: Error) => {
    hilog.error(DOMAIN, 'ZstdDemo', '异步操作失败: %{public}s', e.message)
  })
```

### 异步大数据处理

```arkts
import { hilog } from '@kit.PerformanceAnalysisKit'
import zstd from 'libmemory_decompressor.so'

const DOMAIN = 0x0000
const size: number = 100000
const data: Uint8Array = new Uint8Array(size)
for (let i = 0; i < size; i++) {
  data[i] = (i * 73 + 137) & 0xFF
}

zstd.compressAsync(data.buffer)
  .then((compressed: ArrayBuffer) => {
    hilog.info(DOMAIN, 'ZstdDemo', '异步大数据压缩: %{public}d → %{public}d 字节',
      size, compressed.byteLength)
    return zstd.decompressAsync(compressed)
  })
  .then((decompressed: ArrayBuffer) => {
    const view: Uint8Array = new Uint8Array(decompressed)
    let match: boolean = view.length === size
    if (match) {
      for (let i = 0; i < size; i++) {
        if (view[i] !== data[i]) {
          match = false
          break
        }
      }
    }
    hilog.info(DOMAIN, 'ZstdDemo', '异步大数据解压验证: %{public}s',
      match ? '通过' : '失败')
  })
  .catch((e: Error) => {
    hilog.error(DOMAIN, 'ZstdDemo', '异步大数据处理失败: %{public}s', e.message)
  })
```