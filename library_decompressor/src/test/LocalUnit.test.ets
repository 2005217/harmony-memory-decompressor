import { describe, it, expect } from '@ohos/hypium';
import { CompressorFactory } from '../main/ets/compress/CompressorFactory';
import { ICompressor } from '../main/ets/compress/ICompressor';

function arrayBufferToHex(buf: ArrayBuffer): string {
  const view = new Uint8Array(buf);
  return Array.from(view).map(b => b.toString(16).padStart(2, '0')).join('');
}

function arraysEqual(a: ArrayBuffer, b: ArrayBuffer): boolean {
  if (a.byteLength !== b.byteLength) return false;
  const va = new Uint8Array(a);
  const vb = new Uint8Array(b);
  for (let i = 0; i < va.length; i++) {
    if (va[i] !== vb[i]) return false;
  }
  return true;
}

export default function localUnitTest() {
  describe('CompressorTest', () => {

    // ===================== ZSTD =====================
    it('ZSTD_compress_decompress_should_match_original', 0, async () => {
      const compressor: ICompressor = CompressorFactory.create('zstd');
      const original = new Uint8Array([72, 101, 108, 108, 111, 32, 90, 83, 84, 68, 33]).buffer;
      const compressed = await compressor.compressAsync(original as ArrayBuffer, 3);
      expect(compressed.byteLength).assertLess(original.byteLength);
      const decompressed = await compressor.decompressAsync(compressed);
      expect(decompressed.byteLength).assertEqual(original.byteLength);
      expect(arraysEqual(decompressed, original as ArrayBuffer)).assertTrue();
    });

    it('ZSTD_compress_without_level_should_work', 0, async () => {
      const compressor: ICompressor = CompressorFactory.create('zstd');
      const original = new Uint8Array([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]).buffer;
      const compressed = await compressor.compressAsync(original as ArrayBuffer);
      const decompressed = await compressor.decompressAsync(compressed);
      expect(arraysEqual(decompressed, original as ArrayBuffer)).assertTrue();
    });

    // ===================== LZ4 =====================
    it('LZ4_compress_decompress_should_match_original', 0, async () => {
      const compressor: ICompressor = CompressorFactory.create('lz4');
      const original = new Uint8Array([72, 101, 108, 108, 111, 32, 76, 90, 52, 33]).buffer;
      const compressed = await compressor.compressAsync(original as ArrayBuffer, 1);
      expect(compressed.byteLength).assertLess(original.byteLength);
      const decompressed = await compressor.decompressAsync(compressed);
      expect(decompressed.byteLength).assertEqual(original.byteLength);
      expect(arraysEqual(decompressed, original as ArrayBuffer)).assertTrue();
    });

    it('LZ4_compress_without_level_should_work', 0, async () => {
      const compressor: ICompressor = CompressorFactory.create('lz4');
      const original = new Uint8Array([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]).buffer;
      const compressed = await compressor.compressAsync(original as ArrayBuffer);
      const decompressed = await compressor.decompressAsync(compressed);
      expect(arraysEqual(decompressed, original as ArrayBuffer)).assertTrue();
    });

    // ===================== Snappy =====================
    it('Snappy_compress_decompress_should_match_original', 0, async () => {
      const compressor: ICompressor = CompressorFactory.create('snappy');
      const original = new Uint8Array([83, 110, 97, 112, 112, 121, 32, 116, 101, 115, 116]).buffer;
      const compressed = await compressor.compressAsync(original as ArrayBuffer);
      expect(compressed.byteLength).assertLess(original.byteLength);
      const decompressed = await compressor.decompressAsync(compressed);
      expect(decompressed.byteLength).assertEqual(original.byteLength);
      expect(arraysEqual(decompressed, original as ArrayBuffer)).assertTrue();
    });

    it('Snappy_compress_with_level_should_ignore_level', 0, async () => {
      const compressor: ICompressor = CompressorFactory.create('snappy');
      const original = new Uint8Array([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]).buffer;
      const compressed = await compressor.compressAsync(original as ArrayBuffer, 99);
      const decompressed = await compressor.decompressAsync(compressed);
      expect(arraysEqual(decompressed, original as ArrayBuffer)).assertTrue();
    });

    // ===================== 大文件测试 =====================
    it('large_data_compress_decompress_all_formats', 0, async () => {
      const data = new Uint8Array(65536);
      for (let i = 0; i < data.length; i++) {
        data[i] = i & 0xFF;
      }
      const original = data.buffer;

      const formats: Array<{ name: string; compressor: ICompressor }> = [
        { name: 'zstd', compressor: CompressorFactory.create('zstd') },
        { name: 'lz4', compressor: CompressorFactory.create('lz4') },
        { name: 'snappy', compressor: CompressorFactory.create('snappy') },
      ];

      for (const fmt of formats) {
        const compressed = await fmt.compressor.compressAsync(original as ArrayBuffer);
        expect(compressed.byteLength).assertLess(original.byteLength);
        const decompressed = await fmt.compressor.decompressAsync(compressed);
        expect(decompressed.byteLength).assertEqual(original.byteLength);
        expect(arraysEqual(decompressed, original as ArrayBuffer)).assertTrue();
      }
    });

    // ===================== 工厂模式 + getVersion =====================
    it('CompressorFactory_create_should_return_correct_type', 0, () => {
      const zstd = CompressorFactory.create('zstd');
      const lz4 = CompressorFactory.create('lz4');
      const snappy = CompressorFactory.create('snappy');
      expect(zstd.getFormatName()).assertEqual('ZSTD');
      expect(lz4.getFormatName()).assertEqual('LZ4');
      expect(snappy.getFormatName()).assertEqual('SNAPPY');
    });

    it('getVersion_should_contain_all_libraries', 0, () => {
      const compressor = CompressorFactory.create('zstd');
      const version = compressor.getVersion();
      expect(version).assertContain('ZSTD');
      expect(version).assertContain('LZ4');
      expect(version).assertContain('Snappy');
    });

  });
}