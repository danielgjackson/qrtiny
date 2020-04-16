// Generates a V1 QR Code
// Dan Jackson, 2020

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS // This is an example program only
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

//#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "qrtiny.h"

typedef enum {
    OUTPUT_TEXT_MEDIUM,
} output_mode_t;

void OutputQrTinyTextMedium(uint8_t *buffer, uint16_t formatInfo, FILE* fp, int quiet, bool invert)
{
    for (int y = -quiet; y < QRTINY_DIMENSION + quiet; y += 2)
    {
        for (int x = -quiet; x < QRTINY_DIMENSION + quiet; x++)
        {
            int bitU = QrTinyModuleGet(buffer, formatInfo, x, y) & 1;
            int bitL = (y + 1 < QRTINY_DIMENSION + quiet) ? (QrTinyModuleGet(buffer, formatInfo, x, y + 1) & 1) : (invert ? 1 : 0);
            int value = ((bitL ? 2 : 0) + (bitU ? 1 : 0)) ^ (invert ? 0x3 : 0x0);
            switch (value)
            {
                case 0: fprintf(fp, " "); break; // '\u{0020}' space
                case 1: fprintf(fp, "▀"); break; // '\u{2580}' upper half block
                case 2: fprintf(fp, "▄"); break; // '\u{2584}' lower half block
                case 3: fprintf(fp, "█"); break; // '\u{2588}' block
            }
        }
        fprintf(fp, "\n");
    }
}


int main(int argc, char *argv[])
{
    FILE *ofp = stdout;
    const char *value = NULL;
    bool help = false;
    bool invert = false;
    int quiet = QRTINY_QUIET_STANDARD;
    output_mode_t outputMode = OUTPUT_TEXT_MEDIUM;
    uint16_t formatInfo = QRTINY_FORMATINFO_MASK_000_ECC_MEDIUM;
    
    for (int i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i], "--help")) { help = true; }
        else if (!strcmp(argv[i], "--ecl:m")) { formatInfo = QRTINY_FORMATINFO_MASK_000_ECC_MEDIUM; }
        else if (!strcmp(argv[i], "--ecl:l")) { formatInfo = QRTINY_FORMATINFO_MASK_000_ECC_LOW; }
        else if (!strcmp(argv[i], "--ecl:h")) { formatInfo = QRTINY_FORMATINFO_MASK_000_ECC_HIGH; }
        else if (!strcmp(argv[i], "--ecl:q")) { formatInfo = QRTINY_FORMATINFO_MASK_000_ECC_QUARTILE; }
        else if (!strcmp(argv[i], "--quiet")) { quiet = atoi(argv[++i]); }
        else if (!strcmp(argv[i], "--invert")) { invert = !invert; }
        else if (!strcmp(argv[i], "--file"))
        {
            ofp = fopen(argv[++i], "wb");
            if (ofp == NULL) { fprintf(stderr, "ERROR: Unable to open output filename: %s\n", argv[i]); return -1; }
        }
        else if (!strcmp(argv[i], "--output:medium")) { outputMode = OUTPUT_TEXT_MEDIUM; }
        else if (argv[i][0] == '-')
        {
            fprintf(stderr, "ERROR: Unrecognized parameter: %s\n", argv[i]); 
            help = true;
            break;
        }
        else if (value == NULL)
        {
            value = argv[i];
        }
        else
        {
            fprintf(stderr, "ERROR: Unrecognized positional parameter: %s\n", argv[i]); 
            help = true;
            break;
        }
    }

    if (value == NULL)
    {
        fprintf(stderr, "ERROR: Value not specified.\n"); 
        help = true;
    }

    if (help)
    {
        fprintf(stderr, "Usage:  qrtiny [--ecl:<l|m|q|h>] [--invert] [--quiet 4] [--output:<medium>] [--file filename] <value>\n");
        fprintf(stderr, "\n");
        return -1;
    }

    // Create a buffer for holding the encoded payload and ECC calculations
    uint8_t *buffer = malloc(QRTINY_BUFFER_SIZE);

    // Encode the text into the buffer
    size_t payloadLength = QrTinyWriteAlphanumeric(buffer, 0, value);

    // Generate the QR Code bitmap
    bool result = QrTinyGenerate(buffer, payloadLength, formatInfo);

    if (result)
    {
#ifdef _WIN32
        if (outputMode == OUTPUT_TEXT_MEDIUM) SetConsoleOutputCP(CP_UTF8);
        _setmode(_fileno(stdout), O_BINARY);
#endif
        switch (outputMode)
        {
            case OUTPUT_TEXT_MEDIUM: OutputQrTinyTextMedium(buffer, formatInfo, ofp, quiet, invert); break;
            default: fprintf(ofp, "<error>"); break;
        }
    }
    else
    {
        fprintf(stderr, "ERROR: Could not generate QR Code (data too long).\n");
    }

    if (ofp != stdout) fclose(ofp);
    return 0;
}
