# libmemory_decompressor NAPI 接口使用文档

## 导入模块

```arkts
import zstd from 'libmemory_decompressor.so'
```

## API 参考

### compress

压缩数据。

**签名**：
```arkts
compress(data: ArrayBuffer, level?: number): ArrayBuffer
```

**参数**：

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| data | `ArrayBuffer` | 是 | - | 待压缩的原始数据 |
| level | `number` | 否 | `3` | 压缩级别（1=最快, 3=默认, 22=最高压缩率） |

**返回值**：`ArrayBuffer` - 压缩后的数据

**示例**：
```arkts
import { hilog } from '@kit.PerformanceAnalysisKit'

const DOMAIN = 0x0000
const originalText: string = 'Hello HarmonyOS ZSTD!'
const inputBuffer: Uint8Array = new Uint8Array(originalText.length)
for (let i = 0; i < originalText.length; i++) {
  inputBuffer[i] = originalText.charCodeAt(i)
}

// 基本压缩
const compressed: ArrayBuffer = zstd.compress(inputBuffer.buffer)

// 指定压缩级别
const compressedFast: ArrayBuffer = zstd.compress(inputBuffer.buffer, 1)
const compressedMax: ArrayBuffer = zstd.compress(inputBuffer.buffer, 22)

hilog.info(DOMAIN, 'ZstdDemo', '压缩完成, 大小: %{public}d 字节', compressed.byteLength)
```

---

### decompress

解压缩数据。

**签名**：
```arkts
decompress(data: ArrayBuffer): ArrayBuffer
```

**参数**：

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| data | `ArrayBuffer` | 是 | 待解压的压缩数据 |

**返回值**：`ArrayBuffer` - 解压后的原始数据

**示例**：
```arkts
import { hilog } from '@kit.PerformanceAnalysisKit'

const DOMAIN = 0x0000
const decompressed: ArrayBuffer = zstd.decompress(compressedData)
const decompressedView: Uint8Array = new Uint8Array(decompressed)

// 转为字符串
let text: string = ''
for (let i = 0; i < decompressedView.length; i++) {
  text += String.fromCharCode(decompressedView[i])
}

hilog.info(DOMAIN, 'ZstdDemo', '解压完成, 大小: %{public}d 字节', decompressed.byteLength)
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

### 压缩与解压文本

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

// 压缩
const originalText: string = 'Hello HarmonyOS ZSTD!'
const originalData: Uint8Array = stringToUint8Array(originalText)
const compressed: ArrayBuffer = zstd.compress(originalData.buffer)
const ratio: string = ((compressed.byteLength / originalData.length) * 100).toFixed(2)
hilog.info(DOMAIN, 'ZstdDemo', '压缩率: %{public}s%%', ratio)

// 解压
const decompressed: ArrayBuffer = zstd.decompress(compressed)
const decompressedView: Uint8Array = new Uint8Array(decompressed)
const resultText: string = uint8ArrayToString(decompressedView)
hilog.info(DOMAIN, 'ZstdDemo', '解压结果: %{public}s', resultText)
```

### 压缩二进制数据

```arkts
import { hilog } from '@kit.PerformanceAnalysisKit'
import zstd from 'libmemory_decompressor.so'

const DOMAIN = 0x0000
const size: number = 100000
const data: Uint8Array = new Uint8Array(size)
for (let i = 0; i < size; i++) {
  data[i] = (i * 73 + 137) & 0xFF
}

// 压缩
const compressed: ArrayBuffer = zstd.compress(data.buffer)
hilog.info(DOMAIN, 'ZstdDemo', '%{public}d → %{public}d 字节', size, compressed.byteLength)

// 解压并验证
const decompressed: ArrayBuffer = zstd.decompress(compressed)
const decompressedView: Uint8Array = new Uint8Array(decompressed)

let match: boolean = decompressedView.length === data.length
if (match) {
  for (let i = 0; i < data.length; i++) {
    if (decompressedView[i] !== data[i]) {
      match = false
      break
    }
  }
}
hilog.info(DOMAIN, 'ZstdDemo', '逐字节一致性验证: %{public}s', match ? '通过' : '失败')
```

### 空数据处理

```arkts
import { hilog } from '@kit.PerformanceAnalysisKit'
import zstd from 'libmemory_decompressor.so'

const DOMAIN = 0x0000
const emptyData: Uint8Array = new Uint8Array(0)
const compressed: ArrayBuffer = zstd.compress(emptyData.buffer)
hilog.info(DOMAIN, 'ZstdDemo', '空数据压缩后大小: %{public}d 字节', compressed.byteLength)

const decompressed: ArrayBuffer = zstd.decompress(compressed)
hilog.info(DOMAIN, 'ZstdDemo', '空数据解压后大小: %{public}d 字节', decompressed.byteLength)
```