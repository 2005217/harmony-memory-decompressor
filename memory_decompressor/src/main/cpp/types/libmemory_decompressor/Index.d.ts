export const compress: (data: Uint8Array, level?: number) => Uint8Array;
export const decompress: (data: Uint8Array) => Uint8Array;
export const version: () => string;