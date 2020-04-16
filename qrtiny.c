// QR Code V1 Generator
// Dan Jackson, 2020

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// For output functions (TODO: Move these out?)
#include <stdio.h>

#include "qrtiny.h"

#define QRTINY_MODULE_LIGHT 0
#define QRTINY_MODULE_DARK 1
#define QRTINY_MODULE_DATA -1

// Write bits to buffer
static size_t QrTinyBufferAppend(uint8_t *writeBuffer, size_t writePosition, uint32_t value, size_t bitCount)
{
    for (size_t i = 0; i < bitCount; i++)
    {
        uint8_t *writeByte = writeBuffer + ((writePosition + i) >> 3);
        int writeBit = 7 - ((writePosition + i) & 0x07);
        int writeMask = (1 << writeBit);
        int readMask = (1 << (bitCount - 1 - i));
        *writeByte = (*writeByte & ~writeMask) | ((value & readMask) ? writeMask : 0);
    }
    return bitCount;
}

#define QRTINY_FORMATINFO_MASK 0x5412               // 0b0101010000010010
#define QRTINY_FORMATINFO_TO_ECL(_v) ((((_v) ^ QRTINY_FORMATINFO_MASK) >> (QRTINY_SIZE_BCH + QRTINY_SIZE_MASK)) & ((1 << QRTINY_SIZE_ECL) - 1))
#define QRTINY_FORMATINFO_TO_MASKPATTERN(_v) ((((_v) ^ QRTINY_FORMATINFO_MASK) >> (QRTINY_SIZE_BCH)) & ((1 << QRTINY_SIZE_MASK) - 1))

#define QRTINY_SIZE_ECL 2                           // 2-bit code for error correction
#define QRTINY_SIZE_MASK 3                          // 3-bit code for mask size
#define QRTINY_SIZE_BCH 10                          // 10,5 BCH for format information
#define QRTINY_SIZE_MODE_INDICATOR 4                // 4-bit mode indicator

#define QRTINY_MODE_INDICATOR_NUMERIC      0x1      // 0b0001 Numeric (maximal groups of 3/2/1 digits encoded to 10/7/4-bit binary)
#define QRTINY_MODE_INDICATOR_ALPHANUMERIC 0x2      // 0b0010 Alphanumeric ('0'-'9', 'A'-'Z', ' ', '$', '%', '*', '+', '-', '.', '/', ':') -> 0-44 index. Pairs combined (a*45+b) encoded as 11-bit; odd remainder encoded as 6-bit.
#define QRTINY_MODE_INDICATOR_8_BIT        0x4      // 0b0100 8-bit byte
#define QRTINY_MODE_INDICATOR_TERMINATOR   0x0      // 0b0000 Terminator (End of Message)

#define QRTINY_MODE_NUMERIC_COUNT_BITS      10      // for V1
#define QRTINY_MODE_ALPHANUMERIC_COUNT_BITS  9      // for V1
#define QRTINY_MODE_8BIT_COUNT_BITS          8      // for V1
// Segment buffer sizes (payload, 4-bit mode indicator, V1 sized char count)
#define QRTINY_SEGMENT_NUMERIC_BUFFER_BITS(_c) (QRTINY_SIZE_MODE_INDICATOR + QRTINY_MODE_NUMERIC_COUNT_BITS + (10 * ((_c) / 3)) + (((_c) % 3) * 4) - (((_c) % 3) / 2))
#define QRTINY_SEGMENT_ALPHANUMERIC_BUFFER_BITS(_c) (QRTINY_SIZE_MODE_INDICATOR + QRTINY_MODE_ALPHANUMERIC_COUNT_BITS + 11 * ((_c) >> 1) + 6 * ((_c) & 1))
#define QRTINY_SEGMENT_8_BIT_BUFFER_BITS(_c) (QRTINY_SIZE_MODE_INDICATOR + QRTINY_MODE_8BIT_COUNT_BITS + 8 * (_c))

size_t QrTinyWriteNumeric(void *buffer, size_t bitPosition, const char *text)
{
    size_t charCount = strlen(text);
    size_t bitsWritten = 0;
    bitsWritten += QrTinyBufferAppend(buffer, bitPosition + bitsWritten, (uint32_t)charCount, QRTINY_MODE_NUMERIC_COUNT_BITS);
    for (size_t i = 0; i < charCount; )
    {
        size_t remain = (charCount - i) > 3 ? 3 : (charCount - i);
        int value = text[i] - '0';
        int bits = 4;
        // Maximal groups of 3/2/1 digits encoded to 10/7/4-bit binary
        if (remain > 1) { value = value * 10 + text[i + 1] - '0'; bits += 3; }
        if (remain > 2) { value = value * 10 + text[i + 2] - '0'; bits += 3; }
        bitsWritten += QrTinyBufferAppend(buffer, bitPosition + bitsWritten, QRTINY_MODE_INDICATOR_8_BIT, QRTINY_SIZE_MODE_INDICATOR);
        i += remain;
    }
    return bitsWritten;
}

size_t QrTinyWriteAlphanumeric(void *buffer, size_t bitPosition, const char *text)
{
    static const char *symbols = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*+-./:";
    size_t charCount = strlen(text);
    size_t bitsWritten = 0;
    bitsWritten += QrTinyBufferAppend(buffer, bitPosition + bitsWritten, QRTINY_MODE_INDICATOR_ALPHANUMERIC, QRTINY_SIZE_MODE_INDICATOR);
    bitsWritten += QrTinyBufferAppend(buffer, bitPosition + bitsWritten, (uint32_t)charCount, QRTINY_MODE_ALPHANUMERIC_COUNT_BITS);
    for (size_t i = 0; i < charCount; )
    {
        size_t remain = (charCount - i) > 2 ? 2 : (charCount - i);
        char c = text[i];
        int value = (int)(strchr(symbols, (c >= 'a' && c <= 'z') ? c + 'A' - 'a' : c) - symbols);
        int bits = 6;
        // Pairs combined(a * 45 + b) encoded as 11 - bit; odd remainder encoded as 6 - bit.
        if (remain > 1)
        {
            c = text[i + 1];
            value *= 45;
            value += (int)(strchr(symbols, (c >= 'a' && c <= 'z') ? c + 'A' - 'a' : c) - symbols);
            bits += 5;
        }
        bitsWritten += QrTinyBufferAppend(buffer, bitPosition + bitsWritten, value, bits);
        i += remain;
    }
    return bitsWritten;
}

size_t QrTinyWrite8Bit(void *buffer, size_t bitPosition, const char *text)
{
    size_t charCount = strlen(text);
    size_t bitsWritten = 0;
    bitsWritten += QrTinyBufferAppend(buffer, bitPosition + bitsWritten, QRTINY_MODE_INDICATOR_8_BIT, QRTINY_SIZE_MODE_INDICATOR);
    bitsWritten += QrTinyBufferAppend(buffer, bitPosition + bitsWritten, (uint32_t)charCount, QRTINY_MODE_8BIT_COUNT_BITS);
    for (size_t i = 0; i < charCount; i++)
    {
        bitsWritten += QrTinyBufferAppend(buffer, bitPosition + bitsWritten, text[i], 8);
    }
    return bitsWritten;
}

#define QRTINY_FINDER_SIZE 7
#define QRTINY_TIMING_OFFSET 6
#define QRTINY_VERSION_SIZE 3
#define QRTINY_ALIGNMENT_RADIUS 2

// Determine the data bit index for a V1 QR Code at a given coordinate (only valid at data module coordinates).
static size_t QrTinyIdentifyIndex(int x, int y)
{
    // Coordinates skipping timing
    int xx = x - ((x >= QRTINY_TIMING_OFFSET) ? 1 : 0);
    int yy = y - ((y >= QRTINY_TIMING_OFFSET) ? 1 : 0);
    int dir = (xx >> 1) & 1;  // bottom-up?
    int half = xx & 1;  // right-hand-side?
    int h = 9 - (xx >> 1);
    int v = 4 - (yy >> 2);
    int module = -1;
    if (h < 4) module = dir ? h * 3 + v : h * 3 + 2 - v;
    else if (h < 6) module = 12 + (dir ? (h-4) * 5 + v : (h-4) * 5 + 4 - v);
    else module = 22 + h - 6;
    int bit = (((dir ? 0x0 : 0x3) ^ (yy & 3)) << 1) + half;
    return ((module << 3) | bit);
}

// Determines whether a specified module coordinate is light/dark or part of the data
static int QrTinyIdentifyModule(int x, int y, uint16_t formatInfo)
{
    // Quiet zone
    if (x < 0 || y < 0 || x >= QRTINY_DIMENSION || y >= QRTINY_DIMENSION) { return QRTINY_MODULE_LIGHT; } // Outside
    
    // Finders
    for (int f = 0; f < 3; f++)
    {
        int dx = abs(x - (f & 1 ? QRTINY_DIMENSION - 1 - QRTINY_FINDER_SIZE / 2 : QRTINY_FINDER_SIZE / 2));
        int dy = abs(y - (f & 2 ? QRTINY_DIMENSION - 1 - QRTINY_FINDER_SIZE / 2 : QRTINY_FINDER_SIZE / 2));
        if (dx == 0 && dy == 0) return QRTINY_MODULE_DARK;
        if (dx <= 1 + QRTINY_FINDER_SIZE / 2 && dy <= 1 + QRTINY_FINDER_SIZE / 2)
        {
            return (dx > dy ? dx : dy) & 1 ? QRTINY_MODULE_DARK : QRTINY_MODULE_LIGHT;
        }
    }

    // Timing
    if (x == QRTINY_TIMING_OFFSET || y == QRTINY_TIMING_OFFSET) { return ((x^y)&1) ? QRTINY_MODULE_LIGHT : QRTINY_MODULE_DARK; } // Timing: vertical

    // Coordinates skipping timing
    int xx = x - ((x >= QRTINY_TIMING_OFFSET) ? 1 : 0);
    int yy = y - ((y >= QRTINY_TIMING_OFFSET) ? 1 : 0);

    // --- Encoding region ---
    // Format info (2*15+1=31 modules)
    if (x == QRTINY_FINDER_SIZE + 1 && y == QRTINY_DIMENSION - QRTINY_FINDER_SIZE - 1) { return QRTINY_MODULE_DARK; }  // Always-black module, right of bottom-left finder
    int formatIndex = -1;
    if (xx <= QRTINY_FINDER_SIZE && yy <= QRTINY_FINDER_SIZE) { formatIndex = 7 - xx + yy; }
    if (x == QRTINY_FINDER_SIZE + 1 && y >= QRTINY_DIMENSION - QRTINY_FINDER_SIZE - 1) { formatIndex = y + 14 - (QRTINY_DIMENSION - 1); }  // Format info (right of bottom-left finder)
    if (y == QRTINY_FINDER_SIZE + 1 && x >= QRTINY_DIMENSION - QRTINY_FINDER_SIZE - 1) { formatIndex = QRTINY_DIMENSION - 1 - x; }  // Format info (bottom of top-right finder)
    if (formatIndex >= 0)
    {
        return (formatInfo >> formatIndex) & 1 ? QRTINY_MODULE_DARK : QRTINY_MODULE_LIGHT;
    }

    // Codeword
    return QRTINY_MODULE_DATA;
}

static bool QrTinyCalculateMask(uint16_t formatInfo, int j, int i)
{
    switch (QRTINY_FORMATINFO_TO_MASKPATTERN(formatInfo))
    {
        case 0: return ((i + j) & 1) == 0;
        case 1: return (i & 1) == 0;
        case 2: return j % 3 == 0;
        case 3: return (i + j) % 3 == 0;
        case 4: return (((i >> 1) + (j / 3)) & 1) == 0;
        case 5: return ((i * j) & 1) + ((i * j) % 3) == 0;
        case 6: return ((((i * j) & 1) + ((i * j) % 3)) & 1) == 0;
        case 7: return ((((i * j) % 3) + ((i + j) & 1)) & 1) == 0;
        default: return false;
    }
}

// --- Reed-Solomon Error-Correction Code ---
// This error-correction calculation derived from https://www.nayuki.io/page/qr-code-generator-library Copyright (c) Project Nayuki. (MIT License)
static void QrTinyRSRemainder(const uint8_t data[], size_t dataLen, const uint8_t generator[], int degree, uint8_t result[])
{
    memset(result, 0, (size_t)degree * sizeof(result[0]));
    for (size_t i = 0; i < dataLen; i++)
    {
        uint8_t factor = data[i] ^ result[0];
        memmove(&result[0], &result[1], ((size_t)degree - 1) * sizeof(result[0]));
        result[degree - 1] = 0;
        for (int j = 0; j < degree; j++)
        {
            // Product modulo GF(2^8/0x011D)
            uint8_t value = 0;
            for (int k = 7; k >= 0; k--)
            {
                value = (uint8_t)((value << 1) ^ ((value >> 7) * 0x011D));
                value ^= ((factor >> k) & 1) * generator[j];
            }
            result[j] ^= value;
        }
    }
}

#define QRTINY_ECC_CODEWORDS_MAX 17
// [Table 13] Number of error correction codewords (count of data 8-bit codewords in each block; for each error-correction level in V1)
static const int8_t qrcode_ecc_block_codewords[1 << QRTINY_SIZE_ECL] = {
    10, // 0b00 Medium
    7,  // 0b01 Low
    17, // 0b10 High
    13, // 0b11 Quartile
};
static const uint8_t eccDivisorsMedium[] = { 0xd8, 0xc2, 0x9f, 0x6f, 0xc7, 0x5e, 0x5f, 0x71, 0x9d, 0xc1 }; // V1 0b00 Medium ECL
static const uint8_t eccDivisorsLow[] = { 0x7f, 0x7a, 0x9a, 0xa4, 0x0b, 0x44, 0x75 }; // V1 0b01 Low ECL
static const uint8_t eccDivisorsHigh[] = { 0x77, 0x42, 0x53, 0x78, 0x77, 0x16, 0xc5, 0x53, 0xf9, 0x29, 0x8f, 0x86, 0x55, 0x35, 0x7d, 0x63, 0x4f }; // V1 0b10 High ECL
static const uint8_t eccDivisorsQuartile[] = { 0x89, 0x49, 0xe3, 0x11, 0xb1, 0x11, 0x34, 0x0d, 0x2e, 0x2b, 0x53, 0x84, 0x78 }; // V1 0b11 Quartile ECL
static const uint8_t *eccDivisors[1 << QRTINY_SIZE_ECL] = { eccDivisorsMedium, eccDivisorsLow, eccDivisorsHigh, eccDivisorsQuartile };

// Generate the code
bool QrTinyGenerate(uint8_t* buffer, size_t payloadLength, uint16_t formatInfo)
{
    // Number of error correction blocks (count of error-correction-blocks; for each error-correction level in V1)
    int errorCorrectionLevel = QRTINY_FORMATINFO_TO_ECL(formatInfo);
    int eccCodewords = qrcode_ecc_block_codewords[errorCorrectionLevel]; // (sizeof(eccDivisors[errorCorrectionLevel]) / sizeof(eccDivisors[errorCorrectionLevel][0]));
    const uint8_t *eccDivisor = eccDivisors[errorCorrectionLevel];

    // Total number of data bits available in the codewords (cooked: after ecc and remainder)
    size_t dataCapacity = ((QRTINY_TOTAL_CAPACITY / 8) - (size_t)eccCodewords) * 8;

    int spareCapacity = (int)dataCapacity - (int)payloadLength;
    if (spareCapacity < 0) return false;  // Does not fit

    // --- Generate final codewords ---
    // Write data segments
    size_t bitPosition = payloadLength;

    // Add terminator 4-bit (0b0000)
    size_t remaining = dataCapacity - bitPosition;
    if (remaining > 4) remaining = 4;
    bitPosition += QrTinyBufferAppend(buffer, bitPosition, QRTINY_MODE_INDICATOR_TERMINATOR, remaining);

    // Round up to a whole byte
    size_t bits = (8 - (bitPosition & 7)) & 7;
    remaining = dataCapacity - bitPosition;
    if (remaining > bits) remaining = bits;
    bitPosition += QrTinyBufferAppend(buffer, bitPosition, 0, remaining);

    // Fill any remaining data space with padding
    while ((remaining = dataCapacity - bitPosition) > 0)
    {
        #define QRTINY_PAD_CODEWORDS 0xec11 // Pad codewords 0b11101100=0xec 0b00010001=0x11
        if (remaining > 16) remaining = 16;
        bitPosition += QrTinyBufferAppend(buffer, bitPosition, QRTINY_PAD_CODEWORDS >> (16 - remaining), remaining);
    }

    // --- Calculate ECC at end of codewords ---
    // Calculate ECC for the block -- write all consecutively after the data (no interleave required for V1)
    QrTinyRSRemainder(buffer, dataCapacity / 8, eccDivisor, eccCodewords, buffer + (QRTINY_TOTAL_CAPACITY - ((size_t)8 * eccCodewords)) / 8);
    return true;
}

int QrTinyModuleGet(uint8_t *buffer, uint16_t formatInfo, int x, int y)
{
    int type = QrTinyIdentifyModule(x, y, formatInfo);
    if (type == QRTINY_MODULE_DATA)
    {
        int index = QrTinyIdentifyIndex(x, y);
        type = (buffer[index >> 3] & (1 << (index & 7))) ? 1 : 0;
        bool mask = QrTinyCalculateMask(formatInfo, x, y);
        if (mask) type ^= 1;
    }
    return type;
}
