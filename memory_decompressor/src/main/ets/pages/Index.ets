import { hilog } from '@kit.PerformanceAnalysisKit';
import zstd from 'libmemory_decompressor.so'

const DOMAIN = 0x0000;

function stringToUint8Array(str: string): Uint8Array {
  const arr = new Uint8Array(str.length);
  for (let i = 0; i < str.length; i++) {
    arr[i] = str.charCodeAt(i);
  }
  return arr;
}

function uint8ArrayToString(arr: Uint8Array): string {
  let str = '';
  for (let i = 0; i < arr.length; i++) {
    str += String.fromCharCode(arr[i]);
  }
  return str;
}

function uint8ArrayEquals(a: Uint8Array, b: Uint8Array): boolean {
  if (a.length !== b.length) {
    return false;
  }
  for (let i = 0; i < a.length; i++) {
    if (a[i] !== b[i]) {
      return false;
    }
  }
  return true;
}

function generateTestData(size: number): Uint8Array {
  const data = new Uint8Array(size);
  for (let i = 0; i < size; i++) {
    data[i] = (i * 73 + 137) & 0xFF;
  }
  return data;
}

// 辅助函数：Uint8Array 转十六进制字符串（限制最大长度）
function uint8ArrayToHex(arr: Uint8Array, maxBytes: number = 64): string {
  const len = Math.min(arr.length, maxBytes);
  let hex = '';
  for (let i = 0; i < len; i++) {
    hex += arr[i].toString(16).padStart(2, '0');
    if ((i + 1) % 16 === 0) {
      hex += '\n    ';
    } else if ((i + 1) % 4 === 0) {
      hex += ' ';
    }
  }
  if (arr.length > maxBytes) {
    hex += ' ... (' + arr.length + ' 字节)';
  }
  return hex;
}

@Entry
@Component
struct Index {
  @State testInput: string = 'HarmonyOS ZSTD compression test. ' + 
    'This is a larger test message to verify compression and decompression functionality. '.repeat(20);
  @State logText: string = '';
  private compressedData: ArrayBuffer | undefined = undefined;
  @State originalSize: number = 0;
  @State compressedSize: number = 0;
  @State compressionRatio: string = '';

  aboutToAppear(): void {
    this.appendLog('页面加载完成，开始检测NAPI模块...');
    this.checkModule();
  }

  checkModule(): void {
    try {
      if (!zstd) {
        this.appendLog('[错误] zstd 模块未加载');
        return;
      }
      this.appendLog('zstd 模块已加载');
      this.appendLog('zstd 类型: ' + typeof zstd);
      this.appendLog('可用接口: ' + Object.keys(zstd).join(', '));
    } catch (e) {
      const errMsg = (e instanceof Error) ? e.message : JSON.stringify(e);
      this.appendLog('[异常] 模块检测失败: ' + errMsg);
    }
  }

  appendLog(msg: string): void {
    this.logText += '[' + new Date().toLocaleTimeString() + '] ' + msg + '\n';
    hilog.info(DOMAIN, 'ZstdTest', '%{public}s', msg);
  }

  testVersion(): void {
    try {
      if (typeof zstd.version !== 'function') {
        this.appendLog('[错误] version 不是函数');
        return;
      }
      const version: string = zstd.version();
      this.appendLog('ZSTD 版本: ' + version);
    } catch (e) {
      const errMsg = (e instanceof Error) ? e.message : JSON.stringify(e);
      this.appendLog('[异常] version 调用失败: ' + errMsg);
    }
  }

  testGetSupportedFormats(): void {
    try {
      if (typeof zstd.getSupportedFormats !== 'function') {
        this.appendLog('[错误] getSupportedFormats 不是函数');
        return;
      }
      const formats: string[] = zstd.getSupportedFormats();
      this.appendLog('支持的压缩格式: ' + JSON.stringify(formats));
    } catch (e) {
      const errMsg = (e instanceof Error) ? e.message : JSON.stringify(e);
      this.appendLog('[异常] getSupportedFormats 调用失败: ' + errMsg);
    }
  }

  testCompress(): void {
    try {
      if (typeof zstd.compress !== 'function') {
        this.appendLog('[错误] compress 不是函数');
        return;
      }
      const inputText: string = this.testInput;
      if (!inputText) {
        this.appendLog('[错误] 请输入测试数据');
        return;
      }
      const originalData: Uint8Array = stringToUint8Array(inputText);
      this.originalSize = originalData.length;

      const compressed: ArrayBuffer = zstd.compress(originalData.buffer);
      this.compressedData = compressed;
      this.compressedSize = compressed.byteLength;

      const ratio: string = ((compressed.byteLength / originalData.length) * 100).toFixed(2);
      this.compressionRatio = ratio;

      this.appendLog('压缩完成');
      this.appendLog('  原始大小: ' + originalData.length + ' 字节');
      this.appendLog('  压缩后大小: ' + compressed.byteLength + ' 字节');
      this.appendLog('  压缩率: ' + ratio + '%');
      this.appendLog('  压缩数据(Hex, 前64字节):');
      this.appendLog('    ' + uint8ArrayToHex(new Uint8Array(compressed), 64));
    } catch (e) {
      const errMsg = (e instanceof Error) ? e.message : JSON.stringify(e);
      this.appendLog('[异常] compress 调用失败: ' + errMsg);
    }
  }

  testCompressWithLevel(level: number): void {
    try {
      const inputText: string = this.testInput;
      if (!inputText) {
        this.appendLog('[错误] 请输入测试数据');
        return;
      }
      const originalData: Uint8Array = stringToUint8Array(inputText);
      const compressed: ArrayBuffer = zstd.compress(originalData.buffer, level);
      const ratio: string = ((compressed.byteLength / originalData.length) * 100).toFixed(2);
      this.appendLog('压缩级别 ' + level + ': ' + originalData.length + ' → ' + compressed.byteLength + ' 字节, 压缩率 ' + ratio + '%');
    } catch (e) {
      const errMsg = (e instanceof Error) ? e.message : JSON.stringify(e);
      this.appendLog('[异常] compress(level=' + level + ') 失败: ' + errMsg);
    }
  }

  testDecompress(): void {
    try {
      if (typeof zstd.decompress !== 'function') {
        this.appendLog('[错误] decompress 不是函数');
        return;
      }
      if (!this.compressedData) {
        this.appendLog('[错误] 请先执行压缩测试');
        return;
      }
      const decompressed: ArrayBuffer = zstd.decompress(this.compressedData);
      const decompressedView: Uint8Array = new Uint8Array(decompressed);
      const originalData: Uint8Array = stringToUint8Array(this.testInput);
      const match: boolean = uint8ArrayEquals(decompressedView, originalData);

      this.appendLog('解压完成');
      this.appendLog('  解压后大小: ' + decompressed.byteLength + ' 字节');
      this.appendLog('  逐字节一致性验证: ' + (match ? '通过' : '失败'));
      this.appendLog('  解压数据(文本):');
      this.appendLog('    ' + uint8ArrayToString(decompressedView));

      if (!match) {
        this.appendLog('  原始大小: ' + originalData.length + ' 字节');
        this.appendLog('  解压大小: ' + decompressed.byteLength + ' 字节');
      }
    } catch (e) {
      const errMsg = (e instanceof Error) ? e.message : JSON.stringify(e);
      this.appendLog('[异常] decompress 调用失败: ' + errMsg);
    }
  }

  testDetectFormat(): void {
    try {
      if (typeof zstd.detectFormat !== 'function') {
        this.appendLog('[错误] detectFormat 不是函数');
        return;
      }
      if (!this.compressedData) {
        this.appendLog('[错误] 请先执行压缩测试');
        return;
      }
      const format: string = zstd.detectFormat(this.compressedData);
      this.appendLog('检测压缩格式: ' + format);
    } catch (e) {
      const errMsg = (e instanceof Error) ? e.message : JSON.stringify(e);
      this.appendLog('[异常] detectFormat 调用失败: ' + errMsg);
    }
  }

  testEmptyData(): void {
    try {
      const emptyData: Uint8Array = new Uint8Array(0);
      const compressed: ArrayBuffer = zstd.compress(emptyData.buffer);
      this.appendLog('空数据压缩完成, 大小: ' + compressed.byteLength + ' 字节');
      const decompressed: ArrayBuffer = zstd.decompress(compressed);
      this.appendLog('空数据解压完成, 大小: ' + decompressed.byteLength + ' 字节');
    } catch (e) {
      const errMsg = (e instanceof Error) ? e.message : JSON.stringify(e);
      this.appendLog('[异常] 空数据处理失败: ' + errMsg);
    }
  }

  testLargeData(): void {
    try {
      const size: number = 100000;
      const arr: Uint8Array = generateTestData(size);
      const compressed: ArrayBuffer = zstd.compress(arr.buffer);
      const ratio: string = ((compressed.byteLength / size) * 100).toFixed(2);
      this.appendLog('大数据压缩 (100KB): ' + size + ' → ' + compressed.byteLength + ' 字节, 压缩率 ' + ratio + '%');
      this.appendLog('  压缩数据(Hex, 前32字节):');
      this.appendLog('    ' + uint8ArrayToHex(new Uint8Array(compressed), 32));
      const decompressed: ArrayBuffer = zstd.decompress(compressed);
      const decompressedView: Uint8Array = new Uint8Array(decompressed);
      const match: boolean = uint8ArrayEquals(decompressedView, arr);
      this.appendLog('大数据解压完成, 大小: ' + decompressed.byteLength + ' 字节, 逐字节一致性: ' + (match ? '通过' : '失败'));
      this.appendLog('  解压数据(Hex, 前32字节):');
      this.appendLog('    ' + uint8ArrayToHex(decompressedView, 32));
    } catch (e) {
      const errMsg = (e instanceof Error) ? e.message : JSON.stringify(e);
      this.appendLog('[异常] 大数据处理失败: ' + errMsg);
    }
  }

  testCompressAsync(): void {
    try {
      if (typeof zstd.compressAsync !== 'function') {
        this.appendLog('[错误] compressAsync 不是函数');
        return;
      }
      const inputText: string = this.testInput;
      if (!inputText) {
        this.appendLog('[错误] 请输入测试数据');
        return;
      }
      const originalData: Uint8Array = stringToUint8Array(inputText);
      this.originalSize = originalData.length;
      this.appendLog('异步压缩开始...');

      zstd.compressAsync(originalData.buffer).then((compressed: ArrayBuffer) => {
        this.compressedData = compressed;
        this.compressedSize = compressed.byteLength;
        const ratio: string = ((compressed.byteLength / originalData.length) * 100).toFixed(2);
        this.compressionRatio = ratio;
        this.appendLog('异步压缩完成');
        this.appendLog('  原始大小: ' + originalData.length + ' 字节');
        this.appendLog('  压缩后大小: ' + compressed.byteLength + ' 字节');
        this.appendLog('  压缩率: ' + ratio + '%');
      }).catch((e: Error) => {
        this.appendLog('[异常] 异步压缩失败: ' + e.message);
      });
    } catch (e) {
      const errMsg = (e instanceof Error) ? e.message : JSON.stringify(e);
      this.appendLog('[异常] compressAsync 调用失败: ' + errMsg);
    }
  }

  testDecompressAsync(): void {
    try {
      if (typeof zstd.decompressAsync !== 'function') {
        this.appendLog('[错误] decompressAsync 不是函数');
        return;
      }
      if (!this.compressedData) {
        this.appendLog('[错误] 请先执行压缩测试');
        return;
      }
      this.appendLog('异步解压开始...');

      zstd.decompressAsync(this.compressedData).then((decompressed: ArrayBuffer) => {
        const decompressedView: Uint8Array = new Uint8Array(decompressed);
        const originalData: Uint8Array = stringToUint8Array(this.testInput);
        const match: boolean = uint8ArrayEquals(decompressedView, originalData);
        this.appendLog('异步解压完成');
        this.appendLog('  解压后大小: ' + decompressed.byteLength + ' 字节');
        this.appendLog('  逐字节一致性验证: ' + (match ? '通过' : '失败'));
        this.appendLog('  解压数据(文本):');
        this.appendLog('    ' + uint8ArrayToString(decompressedView));
      }).catch((e: Error) => {
        this.appendLog('[异常] 异步解压失败: ' + e.message);
      });
    } catch (e) {
      const errMsg = (e instanceof Error) ? e.message : JSON.stringify(e);
      this.appendLog('[异常] decompressAsync 调用失败: ' + errMsg);
    }
  }

  testLargeDataAsync(): void {
    try {
      if (typeof zstd.compressAsync !== 'function') {
        this.appendLog('[错误] compressAsync 不是函数');
        return;
      }
      const size: number = 100000;
      const arr: Uint8Array = generateTestData(size);
      this.appendLog('异步大数据压缩开始 (100KB)...');

      zstd.compressAsync(arr.buffer).then((compressed: ArrayBuffer): Promise<ArrayBuffer> => {
        const ratio: string = ((compressed.byteLength / size) * 100).toFixed(2);
        this.appendLog('异步大数据压缩完成: ' + size + ' → ' + compressed.byteLength + ' 字节, 压缩率 ' + ratio + '%');
        return zstd.decompressAsync(compressed);
      }).then((decompressed: ArrayBuffer) => {
        const decompressedView: Uint8Array = new Uint8Array(decompressed);
        const match: boolean = uint8ArrayEquals(decompressedView, arr);
        this.appendLog('异步大数据解压完成, 大小: ' + decompressed.byteLength + ' 字节, 逐字节一致性: ' + (match ? '通过' : '失败'));
      }).catch((e: Error) => {
        this.appendLog('[异常] 异步大数据处理失败: ' + e.message);
      });
    } catch (e) {
      const errMsg = (e instanceof Error) ? e.message : JSON.stringify(e);
      this.appendLog('[异常] 异步大数据调用失败: ' + errMsg);
    }
  }

  clearLog(): void {
    this.logText = '';
    this.compressedData = undefined;
    this.originalSize = 0;
    this.compressedSize = 0;
    this.compressionRatio = '';
  }

  build() {
    Column() {
      Scroll() {
        Column() {
          Text('ZSTD NAPI 模块测试')
            .fontSize(24)
            .fontWeight(FontWeight.Bold)
            .width('100%')
            .textAlign(TextAlign.Center)
            .margin({ top: 16, bottom: 8 })

          Text('libmemory_decompressor 接口验证')
            .fontSize(14)
            .fontColor('#888888')
            .width('100%')
            .textAlign(TextAlign.Center)
            .margin({ bottom: 16 })

          TextArea({
            text: this.testInput,
            placeholder: '输入要压缩的测试数据...'
          })
            .onChange((val: string) => {
              this.testInput = val;
            })
            .width('100%')
            .height(80)
            .backgroundColor('#F5F5F5')
            .borderRadius(8)
            .padding(8)
            .margin({ bottom: 12 })

          Row() {
            Button('版本信息')
              .onClick(() => this.testVersion())
              .backgroundColor('#007BFF')
              .flexGrow(1)
              .margin({ right: 4 })

            Button('支持格式')
              .onClick(() => this.testGetSupportedFormats())
              .backgroundColor('#6C757D')
              .flexGrow(1)
              .margin({ left: 4 })
          }
          .width('100%')
          .margin({ bottom: 8 })

          Row() {
            Button('压缩测试')
              .onClick(() => this.testCompress())
              .backgroundColor('#28A745')
              .flexGrow(1)
              .margin({ right: 4 })

            Button('解压验证')
              .onClick(() => this.testDecompress())
              .backgroundColor('#17A2B8')
              .flexGrow(1)
              .margin({ left: 4 })
          }
          .width('100%')
          .margin({ bottom: 8 })

          Row() {
            Button('格式检测')
              .onClick(() => this.testDetectFormat())
              .backgroundColor('#FFC107')
              .flexGrow(1)
              .fontColor('#000000')
              .margin({ right: 4 })

            Button('空数据处理')
              .onClick(() => this.testEmptyData())
              .backgroundColor('#DC3545')
              .flexGrow(1)
              .margin({ left: 4 })
          }
          .width('100%')
          .margin({ bottom: 8 })

          Row() {
            Button('大数据测试(100KB)')
              .onClick(() => this.testLargeData())
              .backgroundColor('#6610F2')
              .flexGrow(1)
              .margin({ right: 4 })

            Button('清除日志')
              .onClick(() => this.clearLog())
              .backgroundColor('#6C757D')
              .flexGrow(1)
              .margin({ left: 4 })
          }
          .width('100%')
          .margin({ bottom: 8 })

          Text('异步操作测试')
            .fontSize(16)
            .fontWeight(FontWeight.Bold)
            .width('100%')
            .margin({ top: 8, bottom: 8 })

          Row() {
            Button('异步压缩')
              .onClick(() => this.testCompressAsync())
              .backgroundColor('#E83E8C')
              .flexGrow(1)
              .margin({ right: 4 })

            Button('异步解压')
              .onClick(() => this.testDecompressAsync())
              .backgroundColor('#E83E8C')
              .flexGrow(1)
              .margin({ left: 4 })
          }
          .width('100%')
          .margin({ bottom: 8 })

          Row() {
            Button('异步大数据(100KB)')
              .onClick(() => this.testLargeDataAsync())
              .backgroundColor('#E83E8C')
              .flexGrow(1)
              .margin({ right: 4 })

            Button('')
              .backgroundColor('#E83E8C')
              .flexGrow(1)
              .margin({ left: 4 })
              .enabled(false)
              .opacity(0)
          }
          .width('100%')
          .margin({ bottom: 12 })

          Text('不同压缩级别对比')
            .fontSize(16)
            .fontWeight(FontWeight.Bold)
            .width('100%')
            .margin({ top: 8, bottom: 8 })

          Row() {
            Button('级别 1')
              .onClick(() => this.testCompressWithLevel(1))
              .backgroundColor('#20C997')
              .flexGrow(1)
              .margin({ right: 4 })

            Button('级别 3')
              .onClick(() => this.testCompressWithLevel(3))
              .backgroundColor('#20C997')
              .flexGrow(1)
              .margin({ right: 4 })

            Button('级别 6')
              .onClick(() => this.testCompressWithLevel(6))
              .backgroundColor('#20C997')
              .flexGrow(1)
              .margin({ right: 4 })

            Button('级别 9')
              .onClick(() => this.testCompressWithLevel(9))
              .backgroundColor('#20C997')
              .flexGrow(1)
              .margin({ left: 4 })
          }
          .width('100%')
          .margin({ bottom: 12 })

          if (this.compressedSize > 0) {
            Text('压缩结果摘要')
              .fontSize(16)
              .fontWeight(FontWeight.Bold)
              .width('100%')
              .margin({ bottom: 4 })

            Text('原始: ' + this.originalSize + ' 字节 | 压缩: ' + this.compressedSize + ' 字节 | 压缩率: ' + this.compressionRatio + '%')
              .fontSize(14)
              .fontColor('#28A745')
              .width('100%')
              .margin({ bottom: 12 })
          }

          Text('运行日志')
            .fontSize(16)
            .fontWeight(FontWeight.Bold)
            .width('100%')
            .margin({ bottom: 4 })

          TextArea({ text: this.logText })
            .width('100%')
            .height(300)
            .backgroundColor('#1E1E1E')
            .fontColor('#00FF00')
            .fontSize(12)
            .borderRadius(8)
            .padding(8)
            .enabled(false)
        }
        .width('100%')
        .padding(16)
      }
      .width('100%')
    }
    .width('100%')
    .height('100%')
    .backgroundColor('#FFFFFF')
  }
}