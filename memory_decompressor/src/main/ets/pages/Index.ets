import { hilog } from '@kit.PerformanceAnalysisKit';
import { CompressorFactory } from '../compress/CompressorFactory';
import { ICompressor } from '../compress/ICompressor';
import { ZstdCompressor } from '../compress/ZstdCompressor';

const DOMAIN = 0x0000;

function arraysEqual(a: ArrayBuffer, b: ArrayBuffer): boolean {
  if (a.byteLength !== b.byteLength) return false;
  const va = new Uint8Array(a);
  const vb = new Uint8Array(b);
  for (let i = 0; i < va.length; i++) {
    if (va[i] !== vb[i]) return false;
  }
  return true;
}

function generateTestData(size: number): ArrayBuffer {
  const data = new Uint8Array(size);
  for (let i = 0; i < size; i++) {
    data[i] = (i * 73 + 137) & 0xFF;
  }
  return data.buffer;
}

class FormatInfo {
  name: string = '';
  compressor: ICompressor = new ZstdCompressor();
}

@Entry
@Component
struct Index {
  @State logText: string = '';
  @State testInput: string = 'HarmonyOS compression test. ' +
    'This is a test message for verifying all three compression formats. '.repeat(10);
  @State running: boolean = false;
  @State autoRun: boolean = false;
  private zstdCompressor: ICompressor = CompressorFactory.create('zstd');
  private lz4Compressor: ICompressor = CompressorFactory.create('lz4');
  private snappyCompressor: ICompressor = CompressorFactory.create('snappy');

  aboutToAppear(): void {
    this.appendLog('页面加载完成');
    this.appendLog('ZSTD 压缩器: ' + this.zstdCompressor.getFormatName());
    this.appendLog('LZ4 压缩器: ' + this.lz4Compressor.getFormatName());
    this.appendLog('Snappy 压缩器: ' + this.snappyCompressor.getFormatName());
    this.appendLog('-----------------------------');
  }

  appendLog(msg: string): void {
    this.logText += '[' + new Date().toLocaleTimeString() + '] ' + msg + '\n';
    hilog.info(DOMAIN, 'CompressTest', '%{public}s', msg);
  }

  clearLog(): void {
    this.logText = '';
  }

  async runAll(): Promise<void> {
    if (this.running) return;
    this.running = true;
    this.clearLog();
    this.appendLog('>>> 开始全部测试 <<<');
    this.appendLog('');

    await this.testVersion();
    await this.testSupportedFormats();

    const formats: FormatInfo[] = [
      { name: 'ZSTD', compressor: this.zstdCompressor },
      { name: 'LZ4', compressor: this.lz4Compressor },
      { name: 'Snappy', compressor: this.snappyCompressor },
    ];

    for (const fmt of formats) {
      await this.testCompressDecompress(fmt.name, fmt.compressor);
    }

    for (const fmt of formats) {
      await this.testLargeData(fmt.name, fmt.compressor);
    }

    await this.testEmptyData('ZSTD', this.zstdCompressor);
    await this.testEmptyData('LZ4', this.lz4Compressor);
    await this.testEmptyData('Snappy', this.snappyCompressor);

    this.appendLog('');
    this.appendLog('>>> 全部测试完成 <<<');
    this.running = false;
  }

  // --------------- 版本和格式 ---------------

  async testVersion(): Promise<void> {
    try {
      const version = this.zstdCompressor.getVersion();
      this.appendLog('[版本] ' + version);
      if (version.includes('ZSTD') && version.includes('LZ4') && version.includes('Snappy')) {
        this.appendLog('[版本] ✓ 包含所有三个库的版本信息');
      } else {
        this.appendLog('[版本] ✗ 缺少某些库的版本信息');
      }
    } catch (e) {
      this.appendLog('[版本] ✗ 异常: ' + ((e instanceof Error) ? e.message : JSON.stringify(e)));
    }
    this.appendLog('');
  }

  async testSupportedFormats(): Promise<void> {
    try {
      this.appendLog('[格式] 三种压缩器创建成功: ZSTD, LZ4, Snappy');
      const formatNames = [
        this.zstdCompressor.getFormatName(),
        this.lz4Compressor.getFormatName(),
        this.snappyCompressor.getFormatName(),
      ];
      this.appendLog('[格式] getFormatName(): ' + JSON.stringify(formatNames));
      if (formatNames.indexOf('ZSTD') >= 0 && formatNames.indexOf('LZ4') >= 0 && formatNames.indexOf('SNAPPY') >= 0) {
        this.appendLog('[格式] ✓ 全部正确');
      } else {
        this.appendLog('[格式] ✗ 名称不正确');
      }
    } catch (e) {
      this.appendLog('[格式] ✗ 异常: ' + ((e instanceof Error) ? e.message : JSON.stringify(e)));
    }
    this.appendLog('');
  }

  // --------------- 单格式 压缩→解压→验证 ---------------

  async testCompressDecompress(name: string, compressor: ICompressor): Promise<void> {
    try {
      const inputText = this.testInput;
      const original = new Uint8Array(inputText.length);
      for (let i = 0; i < inputText.length; i++) {
        original[i] = inputText.charCodeAt(i);
      }
      const originalBuf = original.buffer as ArrayBuffer;

      this.appendLog('[' + name + '] 开始压缩解压测试 (原始大小: ' + originalBuf.byteLength + ' 字节)');

      const compressed = await compressor.compressAsync(originalBuf);
      this.appendLog('[' + name + '] 压缩后: ' + compressed.byteLength + ' 字节, 压缩率: ' +
        ((compressed.byteLength / originalBuf.byteLength) * 100).toFixed(2) + '%');

      const decompressed = await compressor.decompressAsync(compressed);
      const match = arraysEqual(decompressed, originalBuf);
      this.appendLog('[' + name + '] 解压后: ' + decompressed.byteLength + ' 字节, 一致性: ' + (match ? '✓ 通过' : '✗ 失败'));

      if (!match) {
        this.appendLog('[' + name + '] ✗ 数据不一致！');
      }
    } catch (e) {
      this.appendLog('[' + name + '] ✗ 异常: ' + ((e instanceof Error) ? e.message : JSON.stringify(e)));
    }
    this.appendLog('');
  }

  // --------------- 大数据 ---------------

  async testLargeData(name: string, compressor: ICompressor): Promise<void> {
    try {
      const size = 65536;
      const originalBuf = generateTestData(size);

      this.appendLog('[' + name + '] 大数据测试 (65KB, 原始: ' + originalBuf.byteLength + ' 字节)');

      const compressed = await compressor.compressAsync(originalBuf);
      this.appendLog('[' + name + '] 压缩后: ' + compressed.byteLength + ' 字节, 压缩率: ' +
        ((compressed.byteLength / size) * 100).toFixed(2) + '%');

      const decompressed = await compressor.decompressAsync(compressed);
      const match = arraysEqual(decompressed, originalBuf);
      this.appendLog('[' + name + '] 解压后: ' + decompressed.byteLength + ' 字节, 一致性: ' + (match ? '✓ 通过' : '✗ 失败'));

      if (!match) {
        this.appendLog('[' + name + '] ✗ 大数据数据不一致！');
      }
    } catch (e) {
      this.appendLog('[' + name + '] ✗ 大数据异常: ' + ((e instanceof Error) ? e.message : JSON.stringify(e)));
    }
    this.appendLog('');
  }

  // --------------- 空数据 ---------------

  async testEmptyData(name: string, compressor: ICompressor): Promise<void> {
    try {
      const originalBuf = new ArrayBuffer(0);
      this.appendLog('[' + name + '] 空数据测试');

      const compressed = await compressor.compressAsync(originalBuf);
      this.appendLog('[' + name + '] 空数据压缩后: ' + compressed.byteLength + ' 字节');

      const decompressed = await compressor.decompressAsync(compressed);
      this.appendLog('[' + name + '] 空数据解压后: ' + decompressed.byteLength + ' 字节');

      const match = decompressed.byteLength === 0;
      this.appendLog('[' + name + '] 空数据一致性: ' + (match ? '✓ 通过' : '✗ 失败'));
    } catch (e) {
      this.appendLog('[' + name + '] ✗ 空数据异常: ' + ((e instanceof Error) ? e.message : JSON.stringify(e)));
    }
    this.appendLog('');
  }

  // --------------- 按格式单独测试 ---------------

  async testSingleFormat(name: string): Promise<void> {
    if (this.running) return;
    this.running = true;

    let compressor: ICompressor;
    switch (name) {
      case 'ZSTD':
        compressor = this.zstdCompressor;
        break;
      case 'LZ4':
        compressor = this.lz4Compressor;
        break;
      case 'Snappy':
        compressor = this.snappyCompressor;
        break;
      default:
        this.appendLog('未知格式: ' + name);
        this.running = false;
        return;
    }

    this.appendLog('>>> ' + name + ' 单独测试 <<<');
    await this.testCompressDecompress(name, compressor);
    await this.testLargeData(name, compressor);
    await this.testEmptyData(name, compressor);
    this.appendLog('>>> ' + name + ' 测试完成 <<<');
    this.running = false;
  }

  // --------------- 构建 UI ---------------

  build() {
    Column() {
      Scroll() {
        Column() {
          Text('压缩解压测试 (通过 CompressorFactory)')
            .fontSize(20)
            .fontWeight(FontWeight.Bold)
            .width('100%')
            .textAlign(TextAlign.Center)
            .margin({ top: 16, bottom: 4 })

          Text('ZSTD + LZ4 + Snappy — 工厂模式完整链路测试')
            .fontSize(13)
            .fontColor('#888888')
            .width('100%')
            .textAlign(TextAlign.Center)
            .margin({ bottom: 16 })

          TextArea({
            text: this.testInput,
            placeholder: '输入测试数据...'
          })
            .onChange((val: string) => {
              this.testInput = val;
            })
            .width('100%')
            .height(70)
            .backgroundColor('#F5F5F5')
            .borderRadius(8)
            .padding(8)
            .margin({ bottom: 12 })

          // ===== 一键运行 =====
          Button(this.running ? '运行中...' : '▶ 运行全部测试')
            .width('100%')
            .height(44)
            .backgroundColor('#007BFF')
            .fontColor('#FFFFFF')
            .fontSize(16)
            .fontWeight(FontWeight.Bold)
            .borderRadius(8)
            .enabled(!this.running)
            .onClick(() => this.runAll())
            .margin({ bottom: 12 })

          // ===== 各格式单独运行 =====
          Row() {
            Button('ZSTD')
              .onClick(() => this.testSingleFormat('ZSTD'))
              .backgroundColor('#28A745')
              .flexGrow(1)
              .margin({ right: 4 })
              .enabled(!this.running)

            Button('LZ4')
              .onClick(() => this.testSingleFormat('LZ4'))
              .backgroundColor('#E83E8C')
              .flexGrow(1)
              .margin({ right: 4 })
              .enabled(!this.running)

            Button('Snappy')
              .onClick(() => this.testSingleFormat('Snappy'))
              .backgroundColor('#17A2B8')
              .flexGrow(1)
              .enabled(!this.running)
          }
          .width('100%')
          .margin({ bottom: 8 })

          Row() {
            Button('版本信息')
              .onClick(() => this.testVersion())
              .backgroundColor('#6C757D')
              .flexGrow(1)
              .margin({ right: 4 })
              .enabled(!this.running)

            Button('支持格式')
              .onClick(() => this.testSupportedFormats())
              .backgroundColor('#6C757D')
              .flexGrow(1)
              .margin({ left: 4 })
              .enabled(!this.running)
          }
          .width('100%')
          .margin({ bottom: 8 })

          Button('清除日志')
            .width('100%')
            .backgroundColor('#DC3545')
            .fontColor('#FFFFFF')
            .onClick(() => this.clearLog())
            .margin({ bottom: 12 })

          // ===== 运行日志 =====
          Text('运行日志')
            .fontSize(16)
            .fontWeight(FontWeight.Bold)
            .width('100%')
            .margin({ bottom: 4 })

          TextArea({ text: this.logText })
            .width('100%')
            .height(350)
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