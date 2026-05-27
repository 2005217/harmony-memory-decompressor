import { ICompressor } from './ICompressor';
import { ZstdCompressor } from './ZstdCompressor';
import { Lz4Compressor } from './Lz4Compressor';
import { SnappyCompressor } from './SnappyCompressor';

export class CompressorFactory {
    static create(type: 'zstd' | 'lz4' | 'snappy'): ICompressor {
        switch (type) {
            case 'zstd':
                return new ZstdCompressor();
            case 'lz4':
                return new Lz4Compressor();
            case 'snappy':
                return new SnappyCompressor();
            default:
                throw new Error('Unsupported compressor type: ' + type);
        }
    }
}