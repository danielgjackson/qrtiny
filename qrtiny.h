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

// Error correction level
#define QRCODE_SIZE_ECL 2   // 2-bit error correction
typedef enum
{
    QRCODE_ECL_M = 0x00, // 0b00 Medium (~15%)
    QRCODE_ECL_L = 0x01, // 0b01 Low (~7%)
    QRCODE_ECL_H = 0x02, // 0b10 High (~30%)
    QRCODE_ECL_Q = 0x03, // 0b11 Quartile (~25%)
} qrcode_error_correction_level_t;

// Mask pattern reference (i=row, j=column; where true: invert)
#define QRCODE_SIZE_MASK 3  // 3-bit mask size
typedef enum
{
    QRCODE_MASK_000 = 0x00, // 0b000 (i + j) mod 2 = 0
    QRCODE_MASK_001 = 0x01, // 0b001 i mod 2 = 0
    QRCODE_MASK_010 = 0x02, // 0b010 j mod 3 = 0
    QRCODE_MASK_011 = 0x03, // 0b011 (i + j) mod 3 = 0
    QRCODE_MASK_100 = 0x04, // 0b100 ((i div 2) + (j div 3)) mod 2 = 0
    QRCODE_MASK_101 = 0x05, // 0b101 (i j) mod 2 + (i j) mod 3 = 0
    QRCODE_MASK_110 = 0x06, // 0b110 ((i j) mod 2 + (i j) mod 3) mod 2 = 0
    QRCODE_MASK_111 = 0x07, // 0b111 ((i j) mod 3 + (i + j) mod 2) mod 2 = 0
} qrcode_mask_pattern_t;



// Write text to the scratchBuffer, returning the payloadLength
size_t QrTinyWriteNumeric(void *scratchBuffer, size_t offset, const char *text);
size_t QrTinyWriteAlphanumeric(void *scratchBuffer, size_t offset, const char *text);
size_t QrTinyWrite8Bit(void *scratchBuffer, size_t offset, const char *text);

// Generate the code for the given encoded text
bool QrTinyGenerate(uint8_t *buffer, uint8_t *scratchBuffer, size_t payloadLength, qrcode_error_correction_level_t errorCorrectionLevel, qrcode_mask_pattern_t maskPattern);

// Get the module at the given coordinate (0=light, 1=dark)
int QrTinyModuleGet(uint8_t *buffer, int x, int y);


#ifdef __cplusplus
}
#endif

#endif
