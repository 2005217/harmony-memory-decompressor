export const compress: (data: ArrayBuffer, level?: number) => ArrayBuffer;
export const decompress: (data: ArrayBuffer) => ArrayBuffer;
export const version: () => string;
export const getSupportedFormats: () => string[];
export const detectFormat: (data: ArrayBuffer) => string;