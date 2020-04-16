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

//#define QR_DEBUG_DUMP

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


#define QRCODE_MODE_NUMERIC_COUNT_BITS 10           // V1
#define QRCODE_MODE_ALPHANUMERIC_COUNT_BITS 9       // V1
#define QRCODE_MODE_8BIT_COUNT_BITS 8               // V1
// Segment buffer sizes (payload, 4-bit mode indicator, V1 sized char count)
#define QRCODE_SEGMENT_NUMERIC_BUFFER_BITS(_c) (QRCODE_SIZE_MODE_INDICATOR + QRCODE_MODE_NUMERIC_COUNT_BITS + (10 * ((_c) / 3)) + (((_c) % 3) * 4) - (((_c) % 3) / 2))
#define QRCODE_SEGMENT_ALPHANUMERIC_BUFFER_BITS(_c) (QRCODE_SIZE_MODE_INDICATOR + QRCODE_MODE_ALPHANUMERIC_COUNT_BITS + 11 * ((_c) >> 1) + 6 * ((_c) & 1))
#define QRCODE_SEGMENT_8_BIT_BUFFER_BITS(_c) (QRCODE_SIZE_MODE_INDICATOR + QRCODE_MODE_8BIT_COUNT_BITS + 8 * (_c))

#define QRCODE_SIZE_MODE_INDICATOR 4                // 4-bit mode indicator
typedef enum
{
    QRCODE_MODE_INDICATOR_NUMERIC = 0x1,           // 0b0001 Numeric (maximal groups of 3/2/1 digits encoded to 10/7/4-bit binary)
    QRCODE_MODE_INDICATOR_ALPHANUMERIC = 0x2,      // 0b0010 Alphanumeric ('0'-'9', 'A'-'Z', ' ', '$', '%', '*', '+', '-', '.', '/', ':') -> 0-44 index. Pairs combined (a*45+b) encoded as 11-bit; odd remainder encoded as 6-bit.
    QRCODE_MODE_INDICATOR_8_BIT = 0x4,             // 0b0100 8-bit byte
    QRCODE_MODE_INDICATOR_TERMINATOR = 0x0,        // 0b0000 Terminator (End of Message)
} qrcode_mode_indicator_t;


size_t QrTinyWriteNumeric(void *buffer, size_t bitPosition, const char *text)
{
    size_t charCount = strlen(text);
    size_t bitsWritten = 0;
    bitsWritten += QrCodeBufferAppend(buffer, bitPosition + bitsWritten, (uint32_t)charCount, QRCODE_MODE_NUMERIC_COUNT_BITS);
    for (size_t i = 0; i < charCount; )
    {
        size_t remain = (charCount - i) > 3 ? 3 : (charCount - i);
        int value = text[i] - '0';
        int bits = 4;
        // Maximal groups of 3/2/1 digits encoded to 10/7/4-bit binary
        if (remain > 1) { value = value * 10 + text[i + 1] - '0'; bits += 3; }
        if (remain > 2) { value = value * 10 + text[i + 2] - '0'; bits += 3; }
        bitsWritten += QrCodeBufferAppend(buffer, bitPosition + bitsWritten, QRCODE_MODE_INDICATOR_8_BIT, QRCODE_SIZE_MODE_INDICATOR);
        i += remain;
    }
    return bitsWritten;
}

size_t QrTinyWriteAlphanumeric(void *buffer, size_t bitPosition, const char *text)
{
    static const char *symbols = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*+-./:";
    size_t charCount = strlen(text);
    size_t bitsWritten = 0;
    bitsWritten += QrCodeBufferAppend(buffer, bitPosition + bitsWritten, QRCODE_MODE_INDICATOR_ALPHANUMERIC, QRCODE_SIZE_MODE_INDICATOR);
    bitsWritten += QrCodeBufferAppend(buffer, bitPosition + bitsWritten, (uint32_t)charCount, QRCODE_MODE_ALPHANUMERIC_COUNT_BITS);
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
        bitsWritten += QrCodeBufferAppend(buffer, bitPosition + bitsWritten, value, bits);
        i += remain;
    }
    return bitsWritten;
}

size_t QrTinyWrite8Bit(void *buffer, size_t bitPosition, const char *text)
{
    size_t charCount = strlen(text);
    size_t bitsWritten = 0;
    bitsWritten += QrCodeBufferAppend(buffer, bitPosition + bitsWritten, QRCODE_MODE_INDICATOR_8_BIT, QRCODE_SIZE_MODE_INDICATOR);
    bitsWritten += QrCodeBufferAppend(buffer, bitPosition + bitsWritten, (uint32_t)charCount, QRCODE_MODE_8BIT_COUNT_BITS);
    for (size_t i = 0; i < charCount; i++)
    {
        bitsWritten += QrCodeBufferAppend(buffer, bitPosition + bitsWritten, text[i], 8);
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


// Calculate 15-bit format information (2-bit error-correction level, 3-bit mask, 10-bit BCH error-correction; all masked)
static uint16_t QrCodeCalcFormatInfo(qrcode_error_correction_level_t errorCorrectionLevel, qrcode_mask_pattern_t maskPattern)
{
    // TODO: Reframe in terms of QRCODE_SIZE_ECL (2) and QRCODE_SIZE_MASK (3)

    // LLMMM
    int value = ((errorCorrectionLevel & 0x03) << 3) | (maskPattern & 0x07);

    // Calculate 10-bit Bose-Chaudhuri-Hocquenghem (15,5) error-correction
    int bch = value;
    for (int i = 0; i < 10; i++) bch = (bch << 1) ^ ((bch >> 9) * 0x0537);

    // 0LLMMMEEEEEEEEEE
    uint16_t format = (value << 10) | (bch & 0x03ff);
    static uint16_t formatMask = 0x5412;   // 0b0101010000010010
    format ^= formatMask;

    return format;
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
static int QrCodeIdentifyModule(int x, int y, uint16_t formatInfo)
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


static bool QrCodeCalculateMask(qrcode_mask_pattern_t maskPattern, int j, int i)
{
    switch (maskPattern)
    {
        case QRCODE_MASK_000: return ((i + j) & 1) == 0;
        case QRCODE_MASK_001: return (i & 1) == 0;
        case QRCODE_MASK_010: return j % 3 == 0;
        case QRCODE_MASK_011: return (i + j) % 3 == 0;
        case QRCODE_MASK_100: return (((i >> 1) + (j / 3)) & 1) == 0;
        case QRCODE_MASK_101: return ((i * j) & 1) + ((i * j) % 3) == 0;
        case QRCODE_MASK_110: return ((((i * j) & 1) + ((i * j) % 3)) & 1) == 0;
        case QRCODE_MASK_111: return ((((i * j) % 3) + ((i + j) & 1)) & 1) == 0;
        default: return false;
    }
}


// --- Reed-Solomon Error-Correction Code ---
// Product modulo GF(2^8/0x011D)
// These error-correction functions are from https://www.nayuki.io/page/qr-code-generator-library Copyright (c) Project Nayuki. (MIT License)
static uint8_t QrCodeRSMultiply(uint8_t a, uint8_t b)
{
    uint8_t value = 0;
    for (int i = 7; i >= 0; i--)
    {
        value = (uint8_t)((value << 1) ^ ((value >> 7) * 0x011D));
        value ^= ((b >> i) & 1) * a;
    }
    return value;
}

// Reed-Solomon ECC generator polynomial for given degree.
// These error-correction functions are from https://www.nayuki.io/page/qr-code-generator-library Copyright (c) Project Nayuki. (MIT License)
void QrCodeRSDivisor(int degree, uint8_t result[])
{
    memset(result, 0, (size_t)degree * sizeof(result[0]));
    result[degree - 1] = 1;
    uint8_t root = 1;
    for (int i = 0; i < degree; i++)
    {
        for (int j = 0; j < degree; j++)
        {
            result[j] = QrCodeRSMultiply(result[j], root);
            if (j + 1 < degree)
            {
                result[j] ^= result[j + 1];
            }
        }
        root = QrCodeRSMultiply(root, 0x02);
    }
}

// Reed-Solomon ECC.
// These error-correction functions are from https://www.nayuki.io/page/qr-code-generator-library Copyright (c) Project Nayuki. (MIT License)
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
            result[j] ^= QrCodeRSMultiply(generator[j], factor);
        }
    }
}


// Generate the code
bool QrTinyGenerate(uint8_t* buffer, uint8_t* scratchBuffer, size_t payloadLength, qrcode_error_correction_level_t errorCorrectionLevel, qrcode_mask_pattern_t maskPattern)
{
    // Number of error correction blocks (count of error-correction-blocks; for each error-correction level in V1)
    #define qrcode_ecc_block_count 1    // Always 1 for V1
    // [Table 13] Number of error correction codewords (count of data 8-bit codewords in each block; for each error-correction level in V1)
    static const int8_t qrcode_ecc_block_codewords[1 << QRCODE_SIZE_ECL] = {
        10, // 0b00 Medium
        7,  // 0b01 Low
        17, // 0b10 High
        13, // 0b11 Quartile
    };
    #define QRCODE_ECC_CODEWORDS_MAX 17

    int eccCodewords = qrcode_ecc_block_codewords[errorCorrectionLevel];

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

    // Calculate Reed-Solomon divisor
    uint8_t eccDivisor[QRCODE_ECC_CODEWORDS_MAX];
    QrCodeRSDivisor(eccCodewords, eccDivisor);

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
    uint16_t formatInfo = QrCodeCalcFormatInfo(errorCorrectionLevel, maskPattern);
    for (int y = 0; y < QRCODE_DIMENSION; y++)
    {
        for (int x = 0; x < QRCODE_DIMENSION; x++)
        {
            int type = QrCodeIdentifyModule(x, y, formatInfo);
            if (type == QRCODE_MODULE_DATA)
            {
                int index = QrCodeIdentifyIndex(x, y);
                type = (scratchBuffer[index >> 3] & (1 << (index & 7))) ? 1 : 0;
                bool mask = QrCodeCalculateMask(maskPattern, x, y);
                if (mask) type ^= 1;
            }
            QrTinyModuleSet(buffer, x, y, type);
        }
    }

#ifdef QR_DEBUG_DUMP
    QrCodeDebugDump(buffer, formatInfo);
#endif

    return true;
}


#ifdef QR_DEBUG_DUMP
void QrCodeDebugDump(uint8_t *buffer, uint16_t formatInfo)
{    
    int quiet = QRCODE_QUIET_STANDARD;
    for (int y = -quiet; y < QRCODE_DIMENSION + quiet; y++)
    {
        int lastColor = -1;
        for (int x = -quiet; x < QRCODE_DIMENSION + quiet; x++)
        {
            int type = QrCodeIdentifyModule(x, y, formatInfo);
            char buf[3] = "??";
            int color = QrTinyModuleGet(buffer, x, y);
            
            if (type == QRCODE_MODULE_LIGHT) { buf[0] = '#'; buf[1] = '#'; color = 0; }
            else if (type == QRCODE_MODULE_DARK) { buf[0] = '.'; buf[1] = '.'; color = 1; }
            else if (type == QRCODE_MODULE_DATA) {
                int index = QrCodeIdentifyIndex(x, y);
                buf[1] = (index & 7) + '0';
                int upper = (index >> 3) % 64;
                if (upper < 10) buf[0] = upper + '0'; else if (upper < 36) buf[0] = upper - 10 + 'a'; else upper = upper - 36 + 'A';
                color = 3 + ((index >> 3) & 1);
            }

            if (color != lastColor)
            {
                //if (color == 0) printf("\033[30m\033[47;1m");   // White background, black text
                //if (color == 1) printf("\033[37;1m\033[40m");   // Black background, white text
                if (color == 0) printf("\033[37m\033[47;1m");      // Grey text, white background
                else if (color == 1) printf("\033[30;1m\033[40m");  // Dark grey text, black background
                else if (color == 3) printf("\033[92m\033[42m");                     // Debug 1xx Green
                else if (color == 4) printf("\033[93m\033[43m");                     // Debug 2xx 
                else printf("\033[91m\033[41m");                                     // Red
            }
            printf("%s", buf);
            lastColor = color;
        }
        printf("\033[0m \n");    // reset
    }
}
#endif
