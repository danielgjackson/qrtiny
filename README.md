# Tiny QR Code Generator

Generates a tiny "V1" QR Code, optimized for very low RAM use on embedded environments.

Required files: [`qrtiny.h`](qrtiny.h) [`qrtiny.c`](qrtiny.c)

## Use

```c
// Use a scratch buffer for holding the encoded payload and ECC calculations
uint8_t *scratchBuffer[QRCODE_SCRATCH_BUFFER_SIZE];
```

```c
// Encode the text into the scratch buffer (alternatively `QrTinyWriteNumeric` or `QrTinyWrite8Bit`.
size_t payloadLength = QrTinyWriteAlphanumeric(scratchBuffer, 0, text);
```

```c
// Use a buffer for holding the QR Code bitmap (0=light, 1=dark)
uint8_t *buffer[QRCODE_BUFFER_SIZE];
```

```c
// Generate the QR Code bitmap
bool result = QrTinyGenerate(buffer, scratchBuffer, payloadLength, errorCorrectionLevel, maskPattern);
```

Where `errorCorrectionLevel` is one of: `QRCODE_ECL_L` (low, ~7%), `QRCODE_ECL_M` (medium, ~15%), `QRCODE_ECL_Q` (quartile, ~25%), `QRCODE_ECL_H` (high, ~30%).

And, where `maskPattern` is one of: `QRCODE_MASK_000` to `QRCODE_MASK_111` (counting in binary).

Finally, read a module at the given coordinates (0) to (QRCODE_DIMENSION-1); (subtract `QRCODE_QUIET_STANDARD` for margin):

```c
// Get the module at the given coordinate (0=light, 1=dark)
int QrTinyModuleGet(uint8_t *buffer, int x, int y);
```


```c
size_t QrTinyWriteAlphanumeric(void *scratchBuffer, size_t offset, const char *text);
```

Add a text segment to the QR Code object, where `mode` is typically `QRCODE_MODE_INDICATOR_AUTOMATIC` to detect minimal encoding, and `charCount` is `QRCODE_TEXT_LENGTH` if null-terminated string.

```c
void QrCodeSegmentAppend(qrcode_t *qrcode, qrcode_segment_t *segment, qrcode_mode_indicator_t mode, const char *text, size_t charCount, bool mayUppercase);
```

Get the decided dimension of the code (0=error) and, if dynamic memory is used, the minimum buffer sizes for the code and scratch area (only used during generation itself). 
If you want to use fixed-size buffers, you can pass `NULL` to ignore the parameters and the maximum buffer sizes can be known at compile time using: `QRCODE_BUFFER_SIZE()` and `QRCODE_SCRATCH_BUFFER_SIZE()`.

```c
int QrCodeSize(qrcode_t *qrcode, size_t *bufferSize, size_t *scratchBufferSize);
```

Generate the QR Code (`scratchBuffer` is only used during generation):

```c
bool QrCodeGenerate(qrcode_t *qrcode, uint8_t *buffer, uint8_t *scratchBuffer);
```

Retrieve the modules (bits/pixels) of the QR code at the given coordinate (0=light, 1=dark), you should ensure there are `QRCODE_QUIET_STANDARD` (4) units of light on all sides of the final presentation:

```c
int QrCodeModuleGet(qrcode_t* qrcode, int x, int y);
```


## Demonstration program

Demonstration program ([`main.c`](main.c)) to generate and output V1 QR Codes.

To display a QR Code the console:

```bash
./qrtiny "HELLO"
```


## Notes

A "V1" QR Code measures 21x21 modules, giving a full 26 data codewords, or 208 data modules.  

By error-correction level and content type, the maximum character counts are:

| ECC/codewords  |  Full/8b |  AN/Caps |  Numeric |
|:---------------|---------:|---------:|---------:|
| Low (7)        |       17 |       26 |       41 |
| Medium (10)    |       14 |       20 |       34 |
| Quartile (13)  |       11 |       16 |       27 |
| High (17)      |        7 |       10 |       17 |

Alphanumeric is upper-case A-Z, numeric 0-9, or one of the allowed symbols " $%*+-./:".

Examples:

* 26 alphanumeric (URL, upper-case): HTTPS://XYZXYZ.DEV/-ABCDEF

* 12 alphanumeric (MAC hex caps):    ABCDEFABCDEF

* 12 8-bit (MAC hex lower):          abcdefabcdef

* 15 numeric (MAC as a decimal):     281474976710655


Features:

* Finder centres at: top-left at 3; top right at (version * 4 + 13, 3); bottom left at (3, version * 4 + 13).

* Timing pattern at row/col 6.

* V1 has no alignment pattern (V2-V6 has one alignment pattern centered at (version * 4 + 10); V7+ have multiple alignment patterns).

* V1-6 have no version information (V7+ do).

* Format info (can be pre-calculated lookup, based only on ECL and mask pattern).

* Only one error-correction block is needed for low (V1-V5), medium (V1-V3), quartile (V1-V2), high (V1-V2).

* Error-correction codewords (per block): low (7/10/15/20/26), medium (10/16/26), quartile (13/22), high (17/28).


