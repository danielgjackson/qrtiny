// QR Code Generator
// Dan Jackson, 2020

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// For output functions (TODO: Move these out?)
#include <stdio.h>

#include "qrtiny.h"

#define QRCODE_MODULE_LIGHT 0
#define QRCODE_MODULE_DARK 1
#define QRCODE_MODULE_DATA -1

// Write bits to buffer
static size_t QrCodeBufferAppend(uint8_t *writeBuffer, size_t writePosition, uint32_t value, size_t bitCount)
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

#define QRCODE_FORMATINFO_MASK 0x5412               // 0b0101010000010010
#define QRCODE_FORMATINFO_TO_ECL(_v) ((((_v) ^ QRCODE_FORMATINFO_MASK) >> (QRCODE_SIZE_BCH + QRCODE_SIZE_MASK)) & ((1 << QRCODE_SIZE_ECL) - 1))
#define QRCODE_FORMATINFO_TO_MASKPATTERN(_v) ((((_v) ^ QRCODE_FORMATINFO_MASK) >> (QRCODE_SIZE_BCH)) & ((1 << QRCODE_SIZE_MASK) - 1))

#define QRCODE_SIZE_ECL 2                           // 2-bit code for error correction
#define QRCODE_SIZE_MASK 3                          // 3-bit code for mask size
#define QRCODE_SIZE_BCH 10                          // 10,5 BCH for format information
#define QRCODE_SIZE_MODE_INDICATOR 4                // 4-bit mode indicator
typedef enum
{
    QRCODE_MODE_INDICATOR_NUMERIC = 0x1,            // 0b0001 Numeric (maximal groups of 3/2/1 digits encoded to 10/7/4-bit binary)
    QRCODE_MODE_INDICATOR_ALPHANUMERIC = 0x2,       // 0b0010 Alphanumeric ('0'-'9', 'A'-'Z', ' ', '$', '%', '*', '+', '-', '.', '/', ':') -> 0-44 index. Pairs combined (a*45+b) encoded as 11-bit; odd remainder encoded as 6-bit.
    QRCODE_MODE_INDICATOR_8_BIT = 0x4,              // 0b0100 8-bit byte
    QRCODE_MODE_INDICATOR_TERMINATOR = 0x0,         // 0b0000 Terminator (End of Message)
} qrcode_mode_indicator_t;

#define QRCODE_MODE_NUMERIC_COUNT_BITS 10           // for V1
#define QRCODE_MODE_ALPHANUMERIC_COUNT_BITS 9       // for V1
#define QRCODE_MODE_8BIT_COUNT_BITS 8               // for V1
// Segment buffer sizes (payload, 4-bit mode indicator, V1 sized char count)
#define QRCODE_SEGMENT_NUMERIC_BUFFER_BITS(_c) (QRCODE_SIZE_MODE_INDICATOR + QRCODE_MODE_NUMERIC_COUNT_BITS + (10 * ((_c) / 3)) + (((_c) % 3) * 4) - (((_c) % 3) / 2))
#define QRCODE_SEGMENT_ALPHANUMERIC_BUFFER_BITS(_c) (QRCODE_SIZE_MODE_INDICATOR + QRCODE_MODE_ALPHANUMERIC_COUNT_BITS + 11 * ((_c) >> 1) + 6 * ((_c) & 1))
#define QRCODE_SEGMENT_8_BIT_BUFFER_BITS(_c) (QRCODE_SIZE_MODE_INDICATOR + QRCODE_MODE_8BIT_COUNT_BITS + 8 * (_c))

size_t QrTinyWriteNumeric(void *scratchBuffer, size_t bitPosition, const char *text)
{
    size_t charCount = strlen(text);
    size_t bitsWritten = 0;
    bitsWritten += QrCodeBufferAppend(scratchBuffer, bitPosition + bitsWritten, (uint32_t)charCount, QRCODE_MODE_NUMERIC_COUNT_BITS);
    for (size_t i = 0; i < charCount; )
    {
        size_t remain = (charCount - i) > 3 ? 3 : (charCount - i);
        int value = text[i] - '0';
        int bits = 4;
        // Maximal groups of 3/2/1 digits encoded to 10/7/4-bit binary
        if (remain > 1) { value = value * 10 + text[i + 1] - '0'; bits += 3; }
        if (remain > 2) { value = value * 10 + text[i + 2] - '0'; bits += 3; }
        bitsWritten += QrCodeBufferAppend(scratchBuffer, bitPosition + bitsWritten, QRCODE_MODE_INDICATOR_8_BIT, QRCODE_SIZE_MODE_INDICATOR);
        i += remain;
    }
    return bitsWritten;
}

size_t QrTinyWriteAlphanumeric(void *scratchBuffer, size_t bitPosition, const char *text)
{
    static const char *symbols = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*+-./:";
    size_t charCount = strlen(text);
    size_t bitsWritten = 0;
    bitsWritten += QrCodeBufferAppend(scratchBuffer, bitPosition + bitsWritten, QRCODE_MODE_INDICATOR_ALPHANUMERIC, QRCODE_SIZE_MODE_INDICATOR);
    bitsWritten += QrCodeBufferAppend(scratchBuffer, bitPosition + bitsWritten, (uint32_t)charCount, QRCODE_MODE_ALPHANUMERIC_COUNT_BITS);
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
        bitsWritten += QrCodeBufferAppend(scratchBuffer, bitPosition + bitsWritten, value, bits);
        i += remain;
    }
    return bitsWritten;
}

size_t QrTinyWrite8Bit(void *scratchBuffer, size_t bitPosition, const char *text)
{
    size_t charCount = strlen(text);
    size_t bitsWritten = 0;
    bitsWritten += QrCodeBufferAppend(scratchBuffer, bitPosition + bitsWritten, QRCODE_MODE_INDICATOR_8_BIT, QRCODE_SIZE_MODE_INDICATOR);
    bitsWritten += QrCodeBufferAppend(scratchBuffer, bitPosition + bitsWritten, (uint32_t)charCount, QRCODE_MODE_8BIT_COUNT_BITS);
    for (size_t i = 0; i < charCount; i++)
    {
        bitsWritten += QrCodeBufferAppend(scratchBuffer, bitPosition + bitsWritten, text[i], 8);
    }
    return bitsWritten;
}

int QrTinyModuleGet(uint8_t *buffer, int x, int y)
{
    if (x < 0 || y < 0 || x >= QRCODE_DIMENSION || y >= QRCODE_DIMENSION) return 0; // quiet
    int offset = y * QRCODE_DIMENSION + x;
    return buffer[offset >> 3] & (1 << (7 - (offset & 7))) ? 1 : 0;
}

static void QrTinyModuleSet(uint8_t *buffer, int x, int y, int value)
{
    if (x < 0 || y < 0 || x >= QRCODE_DIMENSION || y >= QRCODE_DIMENSION) return; // quiet
    int offset = y * QRCODE_DIMENSION + x;
    if (buffer == NULL || offset < 0 || (offset >> 3) >= QRCODE_BUFFER_SIZE) return;
    uint8_t mask = (1 << (7 - (offset & 7)));
    buffer[offset >> 3] = (buffer[offset >> 3] & ~mask) | (value ? mask : 0);
}

#define QRCODE_FINDER_SIZE 7
#define QRCODE_TIMING_OFFSET 6
#define QRCODE_VERSION_SIZE 3
#define QRCODE_ALIGNMENT_RADIUS 2

// Determine the data bit index for a V1 QR Code at a given coordinate (only valid at data module coordinates).
size_t QrCodeIdentifyIndex(int x, int y)
{
    // Coordinates skipping timing
    int xx = x - ((x >= QRCODE_TIMING_OFFSET) ? 1 : 0);
    int yy = y - ((y >= QRCODE_TIMING_OFFSET) ? 1 : 0);
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
static int QrCodeIdentifyModule(int x, int y, int formatInfo)
{
    // Quiet zone
    if (x < 0 || y < 0 || x >= QRCODE_DIMENSION || y >= QRCODE_DIMENSION) { return QRCODE_MODULE_LIGHT; } // Outside
    
    // Finders
    for (int f = 0; f < 3; f++)
    {
        int dx = abs(x - (f & 1 ? QRCODE_DIMENSION - 1 - QRCODE_FINDER_SIZE / 2 : QRCODE_FINDER_SIZE / 2));
        int dy = abs(y - (f & 2 ? QRCODE_DIMENSION - 1 - QRCODE_FINDER_SIZE / 2 : QRCODE_FINDER_SIZE / 2));
        if (dx == 0 && dy == 0) return QRCODE_MODULE_DARK;
        if (dx <= 1 + QRCODE_FINDER_SIZE / 2 && dy <= 1 + QRCODE_FINDER_SIZE / 2)
        {
            return (dx > dy ? dx : dy) & 1 ? QRCODE_MODULE_DARK : QRCODE_MODULE_LIGHT;
        }
    }

    // Timing
    if (x == QRCODE_TIMING_OFFSET || y == QRCODE_TIMING_OFFSET) { return ((x^y)&1) ? QRCODE_MODULE_LIGHT : QRCODE_MODULE_DARK; } // Timing: vertical

    // Coordinates skipping timing
    int xx = x - ((x >= QRCODE_TIMING_OFFSET) ? 1 : 0);
    int yy = y - ((y >= QRCODE_TIMING_OFFSET) ? 1 : 0);

    // --- Encoding region ---
    // Format info (2*15+1=31 modules)
    if (x == QRCODE_FINDER_SIZE + 1 && y == QRCODE_DIMENSION - QRCODE_FINDER_SIZE - 1) { return QRCODE_MODULE_DARK; }  // Always-black module, right of bottom-left finder
    int formatIndex = -1;
    if (xx <= QRCODE_FINDER_SIZE && yy <= QRCODE_FINDER_SIZE) { formatIndex = 7 - xx + yy; }
    if (x == QRCODE_FINDER_SIZE + 1 && y >= QRCODE_DIMENSION - QRCODE_FINDER_SIZE - 1) { formatIndex = y + 14 - (QRCODE_DIMENSION - 1); }  // Format info (right of bottom-left finder)
    if (y == QRCODE_FINDER_SIZE + 1 && x >= QRCODE_DIMENSION - QRCODE_FINDER_SIZE - 1) { formatIndex = QRCODE_DIMENSION - 1 - x; }  // Format info (bottom of top-right finder)
    if (formatIndex >= 0)
    {
        return (formatInfo >> formatIndex) & 1 ? QRCODE_MODULE_DARK : QRCODE_MODULE_LIGHT;
    }

    // Codeword
    return QRCODE_MODULE_DATA;
}

static bool QrCodeCalculateMask(int formatInfo, int j, int i)
{
    switch (QRCODE_FORMATINFO_TO_MASKPATTERN(formatInfo))
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
static void QrCodeRSRemainder(const uint8_t data[], size_t dataLen, const uint8_t generator[], int degree, uint8_t result[])
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

#define QRCODE_ECC_CODEWORDS_MAX 17
// [Table 13] Number of error correction codewords (count of data 8-bit codewords in each block; for each error-correction level in V1)
static const int8_t qrcode_ecc_block_codewords[1 << QRCODE_SIZE_ECL] = {
    10, // 0b00 Medium
    7,  // 0b01 Low
    17, // 0b10 High
    13, // 0b11 Quartile
};
static const uint8_t eccDivisorsMedium[] = { 0xd8, 0xc2, 0x9f, 0x6f, 0xc7, 0x5e, 0x5f, 0x71, 0x9d, 0xc1 }; // V1 0b00 Medium ECL
static const uint8_t eccDivisorsLow[] = { 0x7f, 0x7a, 0x9a, 0xa4, 0x0b, 0x44, 0x75 }; // V1 0b01 Low ECL
static const uint8_t eccDivisorsHigh[] = { 0x77, 0x42, 0x53, 0x78, 0x77, 0x16, 0xc5, 0x53, 0xf9, 0x29, 0x8f, 0x86, 0x55, 0x35, 0x7d, 0x63, 0x4f }; // V1 0b10 High ECL
static const uint8_t eccDivisorsQuartile[] = { 0x89, 0x49, 0xe3, 0x11, 0xb1, 0x11, 0x34, 0x0d, 0x2e, 0x2b, 0x53, 0x84, 0x78 }; // V1 0b11 Quartile ECL
static const uint8_t *eccDivisors[1 << QRCODE_SIZE_ECL] = { eccDivisorsMedium, eccDivisorsLow, eccDivisorsHigh, eccDivisorsQuartile };

// Generate the code
bool QrTinyGenerate(uint8_t* buffer, uint8_t* scratchBuffer, size_t payloadLength, int formatInfo)
{
    // Number of error correction blocks (count of error-correction-blocks; for each error-correction level in V1)
    #define qrcode_ecc_block_count 1    // Always 1 for V1

    int errorCorrectionLevel = QRCODE_FORMATINFO_TO_ECL(formatInfo);
    int eccCodewords = qrcode_ecc_block_codewords[errorCorrectionLevel]; // (sizeof(eccDivisors[errorCorrectionLevel]) / sizeof(eccDivisors[errorCorrectionLevel][0]));
    const uint8_t *eccDivisor = eccDivisors[errorCorrectionLevel];

    // Total number of data bits available in the codewords (cooked: after ecc and remainder)
    size_t dataCapacity;
    {
        size_t capacityCodewords = QRCODE_TOTAL_CAPACITY / 8;
        size_t totalEccCodewords = (size_t)qrcode_ecc_block_count * eccCodewords;
        size_t dataCapacityCodewords = capacityCodewords - totalEccCodewords;
        dataCapacity = dataCapacityCodewords * 8;
    }

    int spareCapacity = (int)dataCapacity - (int)payloadLength;
    if (spareCapacity < 0) return false;  // Chosen version / none fit

    // --- Generate final codewords ---
    // Write data segments
    size_t bitPosition = payloadLength;

    // Add terminator 4-bit (0b0000)
    size_t remaining = dataCapacity - bitPosition;
    if (remaining > 4) remaining = 4;
    bitPosition += QrCodeBufferAppend(scratchBuffer, bitPosition, QRCODE_MODE_INDICATOR_TERMINATOR, remaining);

    // Round up to a whole byte
    size_t bits = (8 - (bitPosition & 7)) & 7;
    remaining = dataCapacity - bitPosition;
    if (remaining > bits) remaining = bits;
    bitPosition += QrCodeBufferAppend(scratchBuffer, bitPosition, 0, remaining);

    // Fill any remaining data space with padding
    while ((remaining = dataCapacity - bitPosition) > 0)
    {
        #define QRCODE_PAD_CODEWORDS 0xec11 // Pad codewords 0b11101100=0xec 0b00010001=0x11
        if (remaining > 16) remaining = 16;
        bitPosition += QrCodeBufferAppend(scratchBuffer, bitPosition, QRCODE_PAD_CODEWORDS >> (16 - remaining), remaining);
    }

    // --- Calculate ECC at end of codewords ---
    // ECC settings for the level and verions
    int eccBlockCount = qrcode_ecc_block_count;
    size_t totalCapacity = QRCODE_TOTAL_CAPACITY;

    // Position in buffer for ECC data
    size_t eccOffset = (totalCapacity - ((size_t)8 * eccCodewords * eccBlockCount)) / 8;
    //if ((bitPosition != 8 * eccOffset) || (bitPosition != dataCapacity) || (dataCapacity != 8 * eccOffset)) printf("ERROR: Expected current bit position (%d) to match ECC offset *8 (%d) and data capacity (%d).\n", (int)bitPosition, (int)eccOffset * 8, (int)dataCapacity);

    // Calculate ECC for each block -- write all consecutively after the data (will be interleaved later)
    size_t dataCapacityBytes = dataCapacity / 8;
    size_t dataLenShort = dataCapacityBytes / eccBlockCount;
    int countShortBlocks = (int)(eccBlockCount - (dataCapacityBytes - (dataLenShort * eccBlockCount)));
    for (int block = 0; block < eccBlockCount; block++)
    {
        // Calculate offset and length (earlier consecutive blocks may be short by 1 codeword)
        size_t dataOffset;
        if (block < countShortBlocks)
        {
            dataOffset = block * dataLenShort;
        }
        else
        {
            dataOffset = block * dataLenShort + ((size_t)block - countShortBlocks);
        }
        size_t dataLen = dataLenShort + (block < countShortBlocks ? 0 : 1);
        // Calculate this block's ECC
        uint8_t* eccDest = scratchBuffer + eccOffset + (block * (size_t)eccCodewords);
        QrCodeRSRemainder(scratchBuffer + dataOffset, dataLen, eccDivisor, eccCodewords, eccDest);
    }

    // --- Generate pattern ---
    for (int y = 0; y < QRCODE_DIMENSION; y++)
    {
        for (int x = 0; x < QRCODE_DIMENSION; x++)
        {
            int type = QrCodeIdentifyModule(x, y, formatInfo);
            if (type == QRCODE_MODULE_DATA)
            {
                int index = QrCodeIdentifyIndex(x, y);
                type = (scratchBuffer[index >> 3] & (1 << (index & 7))) ? 1 : 0;
                bool mask = QrCodeCalculateMask(formatInfo, x, y);
                if (mask) type ^= 1;
            }
            QrTinyModuleSet(buffer, x, y, type);
        }
    }
    return true;
}
