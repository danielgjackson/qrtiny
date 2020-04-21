# Tiny QR Code Generator

Generates a "V1" QR Code.

This generates the smallest standard QR Code, "V1", at 21x21 modules.  The software is optimized for very low RAM use, suitable for embedded environments.  In particular, the output modules are computed on demand rather than stored to a buffer in one go.  This method sacrifices run-time performance for a minimal non-stack RAM state of as little as 26 bytes.

For a full QR Code generator, see: [qrcode](https://github.com/danielgjackson/qrcode).

## Use

Required files: [`qrtiny.h`](qrtiny.h) [`qrtiny.c`](qrtiny.c)

```c
// Use a (26 byte) buffer for holding the encoded payload and ECC calculations
uint8_t buffer[QRTINY_BUFFER_SIZE];
```

```c
// Encode one or more segments text to the buffer
size_t payloadLength = 0;
payloadLength += QrTinyWriteAlphanumeric(buffer, payloadLength, "TESTING-");
payloadLength += QrTinyWriteNumeric(buffer, payloadLength, "1234567890");
payloadLength += QrTinyWrite8Bit(buffer, payloadLength, "!");
```

```c
// Choose a format for the QR Code: a mask pattern (binary `000` to `111`) and an error correction level (`LOW`, `MEDIUM`, `QUARTILE`, `HIGH`).
uint16_t formatInfo = QRTINY_FORMATINFO_MASK_000_ECC_MEDIUM;
```

```c
// Compute the remaining buffer contents: any required padding and the calculated error-correction information
bool result = QrTinyGenerate(buffer, payloadLength, formatInfo);
```

```c
// For each coordinate (optionally including a quiet margin), get the module at the given coordinate
int quiet = QRTINY_QUIET_STANDARD;  // or QRTINY_QUIET_NONE for none
for (int y = -quiet; y < QRTINY_DIMENSION + quiet; y++)
{
    for (int x = -quiet; x < QRTINY_DIMENSION + quiet; x++)
    {
        int module = QrTinyModuleGet(buffer, formatInfo, x, y);
        // TODO: output module: 0=light, 1=dark
    }
}
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

Alphanumeric is upper-case A-Z, numeric 0-9, or one of the allowed symbols: *space* `$` `%` `*` `+` `-` `.` `/` `:`

Example uses:

* 26 alphanumeric (URL, upper-case): `HTTPS://XYZXYZ.DEV/-ABCDEF`

* 12 alphanumeric (MAC hex caps):    `ABCDEFABCDEF`

* 12 8-bit (MAC hex lower):          `abcdefabcdef`

* 15 numeric (MAC as a decimal):     `281474976710655`


QR Code feature notes:

* Finder origins at: top-left at 3; top right at (version * 4 + 13, 3); bottom left at (3, version * 4 + 13).

* Timing pattern at row/col 6.

* V1 has no alignment pattern (V2-V6 has one alignment pattern with origin at (version * 4 + 10); V7+ have multiple alignment patterns).

* V1-6 have no version information (V7+ do).

* Format info (can be pre-calculated lookup, based only on ECL and mask pattern).

* Only one error-correction block is needed for low (V1-V5), medium (V1-V3), quartile (V1-V2), high (V1-V2).

* Error-correction codewords (per block): low (7/10/15/20/26), medium (10/16/26), quartile (13/22), high (17/28).

