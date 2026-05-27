export interface ICompressor {
    compressAsync(data: ArrayBuffer, level?: number): Promise<ArrayBuffer>;
    decompressAsync(data: ArrayBuffer): Promise<ArrayBuffer>;
    getVersion(): string;
    getFormatName(): string;
}