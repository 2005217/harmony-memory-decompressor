import compressor from 'libmemory_decompressor.so';
import { ICompressor } from './ICompressor';

export class SnappyCompressor implements ICompressor {
    compressAsync(data: ArrayBuffer, level?: number): Promise<ArrayBuffer> {
        return compressor.snappyCompressAsync(data);
    }

    decompressAsync(data: ArrayBuffer): Promise<ArrayBuffer> {
        return compressor.snappyDecompressAsync(data);
    }

    getVersion(): string {
        return compressor.version();
    }

    getFormatName(): string {
        return 'SNAPPY';
    }
}