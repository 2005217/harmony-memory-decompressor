import compressor from 'libmemory_decompressor.so';
import { ICompressor } from './ICompressor';

export class Lz4Compressor implements ICompressor {
    compressAsync(data: ArrayBuffer, level?: number): Promise<ArrayBuffer> {
        return compressor.lz4CompressAsync(data, level);
    }

    decompressAsync(data: ArrayBuffer): Promise<ArrayBuffer> {
        return compressor.lz4DecompressAsync(data);
    }

    getVersion(): string {
        return compressor.version();
    }

    getFormatName(): string {
        return 'LZ4';
    }
}