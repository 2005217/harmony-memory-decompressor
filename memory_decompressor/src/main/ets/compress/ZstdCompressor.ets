import compressor from 'libmemory_decompressor.so';
import { ICompressor } from './ICompressor';

export class ZstdCompressor implements ICompressor {
    compressAsync(data: ArrayBuffer, level?: number): Promise<ArrayBuffer> {
        return compressor.zstdCompressAsync(data, level);
    }

    decompressAsync(data: ArrayBuffer): Promise<ArrayBuffer> {
        return compressor.zstdDecompressAsync(data);
    }

    getVersion(): string {
        return compressor.version();
    }

    getFormatName(): string {
        return 'ZSTD';
    }
}