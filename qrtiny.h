// QR Code V1 Generator
// Dan Jackson, 2020

#ifndef QRTINY_H
#define QRTINY_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Quiet space around marker
#define QRTINY_QUIET_NONE 0
#define QRTINY_QUIET_STANDARD 4

// This tiny creator only supports V1 QR Codes, 21x21 modules
#define QRTINY_VERSION 1
#define QRTINY_DIMENSION 21

// Total data modules (raw: data, ecc and remainder) minus function pattern and format/version = data capacity in bits
#define QRTINY_TOTAL_CAPACITY (((16 * (size_t)(QRTINY_VERSION) + 128) * (size_t)(QRTINY_VERSION)) + 64 - ((size_t)(QRTINY_VERSION) < 2 ? 0 : (25 * ((size_t)(QRTINY_VERSION) / 7 + 2) - 10) * (size_t)((QRTINY_VERSION) / 7 + 2) - 55) - ((size_t)(QRTINY_VERSION) < 7 ? 0 : 36))

// Required buffer size
#define QRTINY_BUFFER_SIZE (((QRTINY_TOTAL_CAPACITY) + 7) >> 3)

// 0b00 Error-Correction Medium   (~15%, 10 codewords), V1 fits: 14 full characters / 20 alphanumeric / 34 numeric
#define QRTINY_FORMATINFO_MASK_000_ECC_MEDIUM   0x5412
#define QRTINY_FORMATINFO_MASK_001_ECC_MEDIUM   0x5125
#define QRTINY_FORMATINFO_MASK_010_ECC_MEDIUM   0x5e7c
#define QRTINY_FORMATINFO_MASK_011_ECC_MEDIUM   0x5b4b
#define QRTINY_FORMATINFO_MASK_100_ECC_MEDIUM   0x45f9
#define QRTINY_FORMATINFO_MASK_101_ECC_MEDIUM   0x40ce
#define QRTINY_FORMATINFO_MASK_110_ECC_MEDIUM   0x4f97
#define QRTINY_FORMATINFO_MASK_111_ECC_MEDIUM   0x4aa0
// 0b01 Error-Correction Low      (~ 7%,  7 codewords), V1 fits: 17 full characters / 26 alphanumeric / 41 numeric
#define QRTINY_FORMATINFO_MASK_000_ECC_LOW      0x77c4
#define QRTINY_FORMATINFO_MASK_001_ECC_LOW      0x72f3
#define QRTINY_FORMATINFO_MASK_010_ECC_LOW      0x7daa
#define QRTINY_FORMATINFO_MASK_011_ECC_LOW      0x789d
#define QRTINY_FORMATINFO_MASK_100_ECC_LOW      0x662f
#define QRTINY_FORMATINFO_MASK_101_ECC_LOW      0x6318
#define QRTINY_FORMATINFO_MASK_110_ECC_LOW      0x6c41
#define QRTINY_FORMATINFO_MASK_111_ECC_LOW      0x6976
// 0b10 Error-Correction High     (~30%, 17 codewords), V1 fits:  7 full characters / 10 alphanumeric / 17 numeric
#define QRTINY_FORMATINFO_MASK_000_ECC_HIGH     0x1689
#define QRTINY_FORMATINFO_MASK_001_ECC_HIGH     0x13be
#define QRTINY_FORMATINFO_MASK_010_ECC_HIGH     0x1ce7
#define QRTINY_FORMATINFO_MASK_011_ECC_HIGH     0x19d0
#define QRTINY_FORMATINFO_MASK_100_ECC_HIGH     0x0762
#define QRTINY_FORMATINFO_MASK_101_ECC_HIGH     0x0255
#define QRTINY_FORMATINFO_MASK_110_ECC_HIGH     0x0d0c
#define QRTINY_FORMATINFO_MASK_111_ECC_HIGH     0x083b
// 0b11 Error-Correction Quartile (~25%, 13 codewords), V1 fits: 11 full characters / 16 alphanumeric / 27 numeric
#define QRTINY_FORMATINFO_MASK_000_ECC_QUARTILE 0x355f
#define QRTINY_FORMATINFO_MASK_001_ECC_QUARTILE 0x3068
#define QRTINY_FORMATINFO_MASK_010_ECC_QUARTILE 0x3f31
#define QRTINY_FORMATINFO_MASK_011_ECC_QUARTILE 0x3a06
#define QRTINY_FORMATINFO_MASK_100_ECC_QUARTILE 0x24b4
#define QRTINY_FORMATINFO_MASK_101_ECC_QUARTILE 0x2183
#define QRTINY_FORMATINFO_MASK_110_ECC_QUARTILE 0x2eda
#define QRTINY_FORMATINFO_MASK_111_ECC_QUARTILE 0x2bed

// Encode one or more segments of text to the buffer (at bit offset specified), returning the number of bits written. Caller must ensure buffer has capacity.
size_t QrTinyWriteNumeric(void *buffer, size_t offset, const char *text);       // 17-41 digits, depending on ECC.
size_t QrTinyWriteAlphanumeric(void *buffer, size_t offset, const char *text);  // 10-26 characters (upper-case/digits/symbols), depending on ECC.
size_t QrTinyWrite8Bit(void *buffer, size_t offset, const char *text);          //  7-17 8-bit characters, depending on ECC.

// Compute the remaining buffer contents: any required padding and the calculated error-correction information
bool QrTinyGenerate(uint8_t *buffer, size_t payloadLength, uint16_t formatInfo);

// Get the module at the given coordinate (0=light, 1=dark)
int QrTinyModuleGet(uint8_t *buffer, uint16_t formatInfo, int x, int y);

#ifdef __cplusplus
}
#endif

#endif
