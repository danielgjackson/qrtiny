// QR Code Generator
// Dan Jackson, 2020

#ifndef QRTINY_H
#define QRTINY_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Quiet space around marker
#define QRCODE_QUIET_NONE 0
#define QRCODE_QUIET_STANDARD 4

// This tiny creator only supports V1 QR Codes, 21x21 modules
#define QRCODE_VERSION 1
#define QRCODE_DIMENSION 21

// Total data modules (raw: data, ecc and remainder) minus function pattern and format/version = data capacity in bits
#define QRCODE_TOTAL_CAPACITY (((16 * (size_t)(QRCODE_VERSION) + 128) * (size_t)(QRCODE_VERSION)) + 64 - ((size_t)(QRCODE_VERSION) < 2 ? 0 : (25 * ((size_t)(QRCODE_VERSION) / 7 + 2) - 10) * (size_t)((QRCODE_VERSION) / 7 + 2) - 55) - ((size_t)(QRCODE_VERSION) < 7 ? 0 : 36))

// Required buffer sizes
#define QRCODE_MIN_BYTES(_bits) (((_bits) + 7) >> 3)
#define QRCODE_BUFFER_SIZE QRCODE_MIN_BYTES(QRCODE_DIMENSION * QRCODE_DIMENSION)
#define QRCODE_SCRATCH_BUFFER_SIZE QRCODE_MIN_BYTES(QRCODE_TOTAL_CAPACITY)


// 0b00 Error-Correction Medium   (~15%, 10 codewords), V1 fits: 14 full characters / 20 alphanumeric / 34 numeric
#define QRCODE_FORMATINFO_MASK_000_ECC_MEDIUM   0x5412
#define QRCODE_FORMATINFO_MASK_001_ECC_MEDIUM   0x5125
#define QRCODE_FORMATINFO_MASK_010_ECC_MEDIUM   0x5e7c
#define QRCODE_FORMATINFO_MASK_011_ECC_MEDIUM   0x5b4b
#define QRCODE_FORMATINFO_MASK_100_ECC_MEDIUM   0x45f9
#define QRCODE_FORMATINFO_MASK_101_ECC_MEDIUM   0x40ce
#define QRCODE_FORMATINFO_MASK_110_ECC_MEDIUM   0x4f97
#define QRCODE_FORMATINFO_MASK_111_ECC_MEDIUM   0x4aa0
// 0b01 Error-Correction Low      (~ 7%,  7 codewords), V1 fits: 17 full characters / 26 alphanumeric / 41 numeric
#define QRCODE_FORMATINFO_MASK_000_ECC_LOW      0x77c4
#define QRCODE_FORMATINFO_MASK_001_ECC_LOW      0x72f3
#define QRCODE_FORMATINFO_MASK_010_ECC_LOW      0x7daa
#define QRCODE_FORMATINFO_MASK_011_ECC_LOW      0x789d
#define QRCODE_FORMATINFO_MASK_100_ECC_LOW      0x662f
#define QRCODE_FORMATINFO_MASK_101_ECC_LOW      0x6318
#define QRCODE_FORMATINFO_MASK_110_ECC_LOW      0x6c41
#define QRCODE_FORMATINFO_MASK_111_ECC_LOW      0x6976
// 0b10 Error-Correction High     (~30%, 17 codewords), V1 fits:  7 full characters / 10 alphanumeric / 17 numeric
#define QRCODE_FORMATINFO_MASK_000_ECC_HIGH     0x1689
#define QRCODE_FORMATINFO_MASK_001_ECC_HIGH     0x13be
#define QRCODE_FORMATINFO_MASK_010_ECC_HIGH     0x1ce7
#define QRCODE_FORMATINFO_MASK_011_ECC_HIGH     0x19d0
#define QRCODE_FORMATINFO_MASK_100_ECC_HIGH     0x0762
#define QRCODE_FORMATINFO_MASK_101_ECC_HIGH     0x0255
#define QRCODE_FORMATINFO_MASK_110_ECC_HIGH     0x0d0c
#define QRCODE_FORMATINFO_MASK_111_ECC_HIGH     0x083b
// 0b11 Error-Correction Quartile (~25%, 13 codewords), V1 fits: 11 full characters / 16 alphanumeric / 27 numeric
#define QRCODE_FORMATINFO_MASK_000_ECC_QUARTILE 0x355f
#define QRCODE_FORMATINFO_MASK_001_ECC_QUARTILE 0x3068
#define QRCODE_FORMATINFO_MASK_010_ECC_QUARTILE 0x3f31
#define QRCODE_FORMATINFO_MASK_011_ECC_QUARTILE 0x3a06
#define QRCODE_FORMATINFO_MASK_100_ECC_QUARTILE 0x24b4
#define QRCODE_FORMATINFO_MASK_101_ECC_QUARTILE 0x2183
#define QRCODE_FORMATINFO_MASK_110_ECC_QUARTILE 0x2eda
#define QRCODE_FORMATINFO_MASK_111_ECC_QUARTILE 0x2bed

// Write text to the scratchBuffer, returning the payloadLength
size_t QrTinyWriteNumeric(void *scratchBuffer, size_t offset, const char *text);
size_t QrTinyWriteAlphanumeric(void *scratchBuffer, size_t offset, const char *text);
size_t QrTinyWrite8Bit(void *scratchBuffer, size_t offset, const char *text);

// Generate the code for the given encoded text
bool QrTinyGenerate(uint8_t *buffer, uint8_t *scratchBuffer, size_t payloadLength, int formatInfo);

// Get the module at the given coordinate (0=light, 1=dark)
int QrTinyModuleGet(uint8_t *buffer, int x, int y);


#ifdef __cplusplus
}
#endif

#endif
