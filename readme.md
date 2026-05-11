# The WAV File Format — Exhaustive Reference

---

## 1. Origins and Lineage

WAV (Waveform Audio File Format) was developed jointly by **Microsoft and IBM** in 1991 as part of the Windows 3.1 multimedia specification. It uses the RIFF file format structure, and as such is closely related to the AIFF and IFF formats. [edn](https://www.edn.com/how-audio-codecs-work-part-2)

Back in the late 1980s, Electronic Arts came up with a general container file format that could be used to store different types of data — audio, graphics, etc. It was called IFF, for Interchange File Format. Microsoft then took this format, switched the byte order from big-endian to little-endian to better suit Intel processors, and dubbed it RIFF (Resource Interchange File Format). The RIFF format was then used for the WAV file format. [Wavefilegem](https://wavefilegem.com/how_wave_files_work.html)

This Intel heritage is why WAV is **little-endian throughout** — the least significant byte comes first in every multi-byte field. This matters when parsing or constructing the binary manually.

---

## 2. What RIFF Is

Before understanding WAV, you must understand RIFF, because WAV is simply a specific use of RIFF.

A RIFF file is composed of multiple discrete sections of data called chunks. The type of data in a chunk is indicated by a four-character code (FOURCC) identifier. A FOURCC is a 32-bit unsigned integer created by concatenating four ASCII characters used to identify chunk types in a RIFF file. FOURCCs can contain space characters, so `" abc"` is a valid FOURCC. [Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/xaudio2/resource-interchange-file-format--riff-)

The first chunk is the root entry and must have an ID of `'RIFF'` or `'RIFX'`, the prior being the most common version. `'RIFX'` specifies Motorola byte order (most significant byte first), whereas `'RIFF'` specifies Intel byte ordering (least significant byte first). [Daubnet](https://www.daubnet.com/en/file-format-riff)

Every single chunk, without exception, has this structure:

```
┌─────────────────────────────────────────┐
│  ChunkID     [4 bytes]  ASCII FourCC    │
│  ChunkSize   [4 bytes]  uint32 LE       │
│  ChunkData   [N bytes]  content         │
└─────────────────────────────────────────┘
```

chunkSize gives the size of the valid data in the chunk. The data is always padded to the nearest WORD boundary. [Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/xaudio2/resource-interchange-file-format--riff-) This means if your data chunk has an odd byte count, you append a zero byte — but that byte is **not** counted in ChunkSize.

The only chunks that may contain subchunks are the RIFF file chunk `'RIFF'` and the list chunk `'LIST'`. All other chunks may contain only data. A RIFF file is one RIFF chunk. All other chunks and subchunks in the file are contained within this chunk. [edn](https://www.edn.com/how-audio-codecs-work-part-2)

---

## 3. The Canonical WAV Structure

A WAVE file is often just a RIFF file with a single `"WAVE"` chunk which consists of two sub-chunks — a `"fmt "` chunk specifying the data format and a `"data"` chunk containing the actual sample data. Call this form the "Canonical form". [Sapp](http://soundfile.sapp.org/doc/WaveFormat/)

```
File on disk:
┌──────────────────────────────────────────────────────────┐
│ "RIFF"  [4]  ← magic, identifies RIFF container         │
│ fileSize[4]  ← total_file_size - 8                      │
│ "WAVE"  [4]  ← identifies this RIFF as a WAV            │
│                                                          │
│   ┌──────────────────────────────────────────────────┐   │
│   │ "fmt " [4]  ← format sub-chunk tag              │   │
│   │ 16     [4]  ← fmt body is 16 bytes (PCM)        │   │
│   │ [16 bytes of format fields]                      │   │
│   └──────────────────────────────────────────────────┘   │
│                                                          │
│   ┌──────────────────────────────────────────────────┐   │
│   │ "data" [4]  ← data sub-chunk tag                │   │
│   │ dataSize[4] ← byte count of raw samples         │   │
│   │ [raw PCM samples...]                             │   │
│   └──────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────┘
```

---

## 4. Full Header Byte Map

Every byte at every offset, for a canonical 16-bit mono PCM WAV file:

```
Offset  Size  Type      Field Name           Value / Notes
------  ----  --------  -------------------  -------------------------
  0      4    char[4]   ChunkID              "RIFF"  (0x52494646)
  4      4    uint32    ChunkSize            36 + DataSize
  8      4    char[4]   Format               "WAVE"  (0x57415645)

 ── fmt sub-chunk ──────────────────────────────────────────────────
 12      4    char[4]   Subchunk1ID          "fmt "  (0x666D7420)
 16      4    uint32    Subchunk1Size        16  (for PCM)
 20      2    uint16    AudioFormat          1   (PCM)
 22      2    uint16    NumChannels          1 = mono, 2 = stereo
 24      4    uint32    SampleRate           44100, 48000, etc.
 28      4    uint32    ByteRate             SampleRate × NumChannels × BitsPerSample/8
 32      2    uint16    BlockAlign           NumChannels × BitsPerSample/8
 34      2    uint16    BitsPerSample        8, 16, 24, 32

 ── data sub-chunk ─────────────────────────────────────────────────
 36      4    char[4]   Subchunk2ID          "data"  (0x64617461)
 40      4    uint32    Subchunk2Size        NumSamples × NumChannels × BitsPerSample/8
 44      *    int16[]   Data                 raw PCM samples
```

Total header = **44 bytes** exactly for canonical PCM WAV.

---

## 5. The C Struct — Field by Field

```c
typedef struct {
    char     riff_tag[4];        // offset  0, size 4
    uint32_t chunk_size;         // offset  4, size 4
    char     wave_form[4];       // offset  8, size 4
    char     fmt_tag[4];         // offset 12, size 4
    uint32_t fmt_subchunk_size;  // offset 16, size 4
    uint16_t audio_format;       // offset 20, size 2
    uint16_t num_channels;       // offset 22, size 2
    uint32_t sample_rate;        // offset 24, size 4
    uint32_t byte_rate;          // offset 28, size 4
    uint16_t block_align;        // offset 32, size 2
    uint16_t bits_per_sample;    // offset 34, size 2
    char     data_tag[4];        // offset 36, size 4
    uint32_t data_subchunk_size; // offset 40, size 4
} __attribute__((packed)) WAVHeader; // total: 44 bytes
```

The struct fields tile perfectly: `4+4+4+4+4+2+2+4+4+2+2+4+4 = 44`. No padding occurs naturally here, but `__attribute__((packed))` makes that contractual — the compiler is forbidden from inserting alignment bytes regardless of architecture.

---

### Field 1 — `riff_tag[4]` — offset 0

```c
memcpy(hdr->riff_tag, "RIFF", 4);
// bytes: 0x52 0x49 0x46 0x46
```

When inspecting a WAV file's data using any hex viewer, we can see it starts with a signature RIFF (hex: 52, 49, 46, 46). [File Recovery](https://www.file-recovery.com/wav-signature-format.htm)

This is the magic number. Every RIFF parser checks these 4 bytes first. If they don't match, the file is rejected immediately before reading anything else.

**Why `memcpy` and never `strcpy`:**

```c
strcpy(hdr->riff_tag, "RIFF");  // WRONG
// copies: 'R','I','F','F','\0' = 5 bytes
// '\0' overflows into chunk_size, corrupting it silently

memcpy(hdr->riff_tag, "RIFF", 4);  // CORRECT
// copies exactly 4 bytes, no null terminator
```

---

### Field 2 — `chunk_size` — offset 4

```c
hdr->chunk_size = 36 + data_size;
```

ChunkSize = 36 + SubChunk2Size, or more precisely: 4 + (8 + SubChunk1Size) + (8 + SubChunk2Size). This is the size of the entire file in bytes minus 8 bytes for the two fields not included in this count: ChunkID and ChunkSize. [sapp](http://soundfile.sapp.org/doc/WaveFormat/)

Breaking the 36 down:

```
"WAVE" tag           = 4 bytes
"fmt " tag           = 4 bytes
fmt_subchunk_size    = 4 bytes
fmt body (16 bytes)  = 16 bytes
"data" tag           = 4 bytes
data_subchunk_size   = 4 bytes
                     ─────────
Total overhead       = 36 bytes
```

The 8 excluded bytes are `"RIFF"` (4) + `chunk_size` itself (4). The RIFF spec defines chunk_size as counting everything **after** those two fields.

---

### Field 3 — `wave_form[4]` — offset 8

```c
memcpy(hdr->wave_form, "WAVE", 4);
// bytes: 0x57 0x41 0x56 0x45
```

At offset 8 there is a signature of Waveform Audio RIFF Type WAVE (hex: 57, 41, 56, 45). [File Recovery](https://www.file-recovery.com/wav-signature-format.htm)

This distinguishes a WAV from other RIFF subtypes. The same RIFF container is used by AVI video (`"AVI "`), CD audio (`"CDXA"`), and others. `"WAVE"` is the identifier that says this RIFF file specifically contains waveform audio.

---

### Field 4 — `fmt_tag[4]` — offset 12

```c
memcpy(hdr->fmt_tag, "fmt ", 4);  // trailing space is mandatory
// bytes: 0x66 0x6D 0x74 0x20
```

The trailing space (0x20) is part of the FourCC spec — FourCC codes are always exactly 4 bytes, padded with spaces if the identifier is shorter. `"fmt"` is 3 characters, so it becomes `"fmt "`. Parsers match the full 4-byte sequence including the space.

---

### Field 5 — `fmt_subchunk_size` — offset 16

```c
hdr->fmt_subchunk_size = 16;
```

Subchunk1Size = 16 for PCM. This is the size of the rest of the subchunk which follows this number. [sapp](http://soundfile.sapp.org/doc/WaveFormat/)

The 16 counts only the body fields of the fmt chunk — the 6 fields from `audio_format` through `bits_per_sample`. It does **not** count the `"fmt "` tag (4 bytes) or this size field itself (4 bytes), consistent with how RIFF chunk sizes always work.

For non-PCM formats this value grows because extra fields are appended. For `WAVE_FORMAT_EXTENSIBLE` it is 40.

---

### Field 6 — `audio_format` — offset 20

```c
hdr->audio_format = 1;  // PCM
```

This 2-byte field tells the parser what encoding the samples use. The most important values:

| Code | Hex    | Format |
|------|--------|--------|
| 1    | 0x0001 | PCM — raw uncompressed integers |
| 3    | 0x0003 | IEEE 754 float (32 or 64-bit) |
| 6    | 0x0006 | ITU G.711 A-law (telephony) |
| 7    | 0x0007 | ITU G.711 µ-law (telephony) |
| 17   | 0x0011 | IMA ADPCM (4:1 compression) |
| 65534| 0xFFFE | WAVE_FORMAT_EXTENSIBLE |

While WAV files can store numerous formats of data, the common vernacular usage of 'WAV' typically refers to audio encoded with pulse-code modulation (PCM). [edn](https://www.edn.com/how-audio-codecs-work-part-2)

**What PCM actually means:**

PCM stands for Pulse Code Modulation. Invented at Bell Labs, it is the direct digital representation of an analog signal. An ADC (Analog-to-Digital Converter) samples the air pressure level at regular time intervals and quantizes each measurement to the nearest integer in the available range. No compression, no prediction, no transformation — just raw amplitude snapshots.

---

### Field 7 — `num_channels` — offset 22

```c
hdr->num_channels = 1;  // mono
```

| Value | Meaning |
|-------|---------|
| 1 | Mono |
| 2 | Stereo |
| 6 | 5.1 surround |
| 8 | 7.1 surround |

For stereo and above, samples are **interleaved** in the data chunk. In a stereo file, one sample for the left channel will be followed by one sample for the right channel, followed by another sample for the left channel, then right channel, and so forth. One set of interleaved samples is called a sample frame (also called a block). [Wavefilegem](https://wavefilegem.com/how_wave_files_work.html)

Stereo data layout on disk for 16-bit:

```
[L0_low][L0_high][R0_low][R0_high][L1_low][L1_high][R1_low][R1_high]...
  ─── frame 0 ────────────────────  ─── frame 1 ────────────────────
```

For stereo, `block_align = 4` (2 channels × 2 bytes), and the samples array in C must be constructed with interleaved values:

```c
// Stereo generation example
for (int i = 0; i < num_frames; i++) {
    double t = (double)i / SAMPLE_RATE;
    int16_t left  = (int16_t)(32767.0 * sin(2.0 * M_PI * 440.0 * t));
    int16_t right = (int16_t)(32767.0 * sin(2.0 * M_PI * 880.0 * t));
    samples[i * 2 + 0] = left;   // interleaved L
    samples[i * 2 + 1] = right;  // interleaved R
}
```

---

### Field 8 — `sample_rate` — offset 24

```c
hdr->sample_rate = 44100;
```

Common values and their use cases:

| Rate    | Use case |
|---------|----------|
| 8000    | Telephone (narrowband) |
| 11025   | Low quality, legacy |
| 22050   | Half CD quality |
| 44100   | CD audio, consumer standard |
| 48000   | Professional broadcast, video |
| 96000   | High-resolution audio |
| 192000  | Studio archival |

**Why 44100 specifically:** The Nyquist–Shannon theorem requires a sample rate of at least 2× the highest frequency to be reproduced. Human hearing caps at ~20,000 Hz, so the minimum is ~40,000 Hz. The 44100 figure traces back to early digital audio stored on video tape — NTSC video runs at 30 frames/sec with 245 scanlines per field × 3 samples per line × 2 fields = 44,100. It fit neatly and provided margin above 40,000.

---

### Field 9 — `byte_rate` — offset 28

```c
hdr->byte_rate = SAMPLE_RATE * block_align;
// = 44100 * 2 = 88200 bytes/sec  (mono 16-bit)
// = 44100 * 4 = 176400 bytes/sec (stereo 16-bit)
```

Formula: `SampleRate × NumChannels × BitsPerSample / 8`

This tells the player how many bytes it will consume per second of playback. Used for:
- Estimating buffer sizes without per-frame calculation
- Seeking to a time position: `seek_offset = byte_rate × time_seconds`
- Estimating playback duration without counting samples

---

### Field 10 — `block_align` — offset 32

```c
hdr->block_align = sizeof(int16_t);  // = 2 for mono 16-bit
```

Formula: `NumChannels × BitsPerSample / 8`

This is the size in bytes of one **complete sample frame** — all channels at a single instant in time. The block alignment (in bytes) of the data. Playback software needs to process a multiple of `nBlockAlign` bytes of data at a time, so that the value of `nBlockAlign` can be used for buffer alignment. [RecordingBlogs](https://www.recordingblogs.com/wiki/format-chunk-of-a-wave-file)

| Format | block_align |
|--------|-------------|
| Mono 8-bit | 1 |
| Mono 16-bit | 2 |
| Stereo 16-bit | 4 |
| 5.1 24-bit | 18 |

---

### Field 11 — `bits_per_sample` — offset 34

```c
hdr->bits_per_sample = 16;
```

Bit depth defines the amplitude resolution — how many discrete levels the sample can represent.

| Depth | Levels | Dynamic range | Use |
|-------|--------|---------------|-----|
| 8-bit | 256 | ~48 dB | Legacy, retro effects |
| 16-bit | 65,536 | ~96 dB | CD, consumer, standard |
| 24-bit | 16,777,216 | ~144 dB | Studio recording, mastering |
| 32-bit | 4,294,967,296 | ~192 dB | Float processing, DAW internal |

**Critical sign convention:** 16-bit PCM data is signed and zero represents silence (no sound pressure). 8-bit PCM data is unsigned and the halfway point at 0x80 represents silence (no sound pressure). [Joenord](https://www.joenord.com/audio-wav-file-format/)

This asymmetry is a WAV-specific quirk. 8-bit WAV uses unsigned samples with silence at 128. 16-bit WAV uses signed two's-complement with silence at 0. Mixing these up produces maximum-volume noise instead of silence.

```c
// 8-bit WAV: silence = 128 (0x80)
uint8_t silence_8bit = 128;

// 16-bit WAV: silence = 0
int16_t silence_16bit = 0;
```

---

### Field 12 — `data_tag[4]` — offset 36

```c
memcpy(hdr->data_tag, "data", 4);
// bytes: 0x64 0x61 0x74 0x61
```

The chunk ID for the data sub-chunk. Everything following the next 4-byte size field is raw audio. A conforming WAV parser should be prepared for optional chunks between `fmt ` and `data` (like `fact`, `cue `, `LIST`) and skip them until it finds `"data"`. Assuming `data` always starts at offset 36 is a common but incorrect assumption.

---

### Field 13 — `data_subchunk_size` — offset 40

```c
hdr->data_subchunk_size = num_samples * sizeof(int16_t);
// = 88200 * 2 = 176400 bytes  (1 second, mono, 16-bit)
```

Subchunk2Size = NumSamples × NumChannels × BitsPerSample/8. This is the number of bytes in the data. You can also think of this as the size of the read of the subchunk following this number. [sapp](http://soundfile.sapp.org/doc/WaveFormat/)

---

## 6. The Raw Sample Data

Starting at byte 44, the raw PCM samples follow with no framing, no delimiters, no compression — just a flat binary array.

### Little-endian byte order

All multi-byte numbers in a RIFF file are stored as little-endian. [Wavefilegem](https://wavefilegem.com/how_wave_files_work.html)

A 16-bit sample with value `0x1234` (decimal 4660) is stored as:

```
byte[0] = 0x34  ← least significant byte first
byte[1] = 0x12  ← most significant byte second
```

On an x86 Linux system, `int16_t` is natively little-endian — so writing the array directly with `fwrite` produces correctly-ordered bytes. On a big-endian architecture (some embedded systems, older SPARC/PowerPC), you'd need to byteswap each sample before writing.

### Two's complement for signed samples

16-bit signed samples use two's complement:

```
 0x7FFF =  32767  ← maximum positive (loudest positive pressure)
 0x0001 =      1
 0x0000 =      0  ← silence
 0xFFFF =     -1
 0x8001 = -32767
 0x8000 = -32768  ← maximum negative (loudest negative pressure)
```

The asymmetry (`+32767` vs `-32768`) is inherent to two's complement. If a PCM type is signed, the sign encoding is almost always two's complement. [MultimediaWiki](https://wiki.multimedia.cx/index.php/PCM)

### Why the amplitude upper bound must be 32767, not 32768

```c
// WRONG: sin(x) = 1.0 → 1.0 × 32768 = 32768 → overflows → -32768 (click)
samples[i] = (int16_t)(32768.0 * sin(...));

// CORRECT: sin(x) = 1.0 → 1.0 × 32767 = 32767 → fits perfectly
samples[i] = (int16_t)(32767.0 * sin(...));
```

The overflow wraps via two's complement to `-32768` — a sudden massive negative spike at every positive peak, audible as a harsh crackling distortion.

---

## 7. Hex Dump — Real File Example

Here are the opening 72 bytes of a WAVE file with bytes shown as hexadecimal numbers: [sapp](http://soundfile.sapp.org/doc/WaveFormat/)

```
52 49 46 46  24 08 00 00  57 41 56 45  66 6d 74 20
R  I  F  F   chunk_size   W  A  V  E   f  m  t  ' '

10 00 00 00  01 00  02 00  22 56 00 00  88 58 01 00
fmt_size=16  fmt=1  ch=2   rate=22050   byte_rate

04 00  10 00  64 61 74 61  00 08 00 00
blk=4  bps=16 d  a  t  a   data_size

00 00 00 00 24 17 1e f3 3c 13 3c 14 16 f9 18 f9 ...
← silence → ← stereo PCM samples begin here ──────
```

Reading the hex: `22 56 00 00` stored little-endian = `0x00005622` = 22050 decimal (22050 Hz sample rate). `88 58 01 00` = `0x00015888` = 88200 = `22050 × 2 × 2` (stereo 16-bit byte rate).

---

## 8. Beyond Canonical — Optional Chunks

The canonical PCM WAV uses only `fmt ` and `data`. Real-world WAV files often include additional chunks.

### `fact` chunk

A fact chunk in the WAV file format contains the following information. Fact chunks exist in all wave files that are compressed or that have a wave list chunk. A fact chunk is not required in an uncompressed PCM file that does not have a wave list chunk. According to the fact chunk's initial specification, the data portion of the fact chunk will contain only one 4-byte number that specifies the number of samples in the data chunk of the WAV file. [RecordingBlogs](https://www.recordingblogs.com/wiki/fact-chunk-of-a-wave-file)

```
"fact"  [4 bytes]  chunk tag
4       [4 bytes]  chunk size
N       [4 bytes]  total sample frames
```

The non-PCM formats must have a fact chunk. [Mcgill](https://www.mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/WAVE.html) For standard PCM it is optional and usually omitted.

### `cue ` chunk

The `<cue-ck>` chunk identifies some significant sample numbers in the wave file. [Wikipedia](https://en.wikipedia.org/wiki/WAV) Used by audio editors to store markers and loop points — for example, where a drum hit begins, or where a loop should restart.

### `LIST` / `INFO` chunk

RIFF files may include a `LIST` chunk with a HeaderID of `INFO`. [Daubnet](https://www.daubnet.com/en/file-format-riff) This is where metadata lives: artist name, album, copyright, creation date, description. Example fields: `IART` (artist), `INAM` (name/title), `ICOP` (copyright), `ICRD` (creation date).

```c
// INFO chunk embedded in a WAV:
// "LIST" "INFO" "INAM" <size> "My Sound\0" "IART" <size> "Me\0"
```

### `JUNK` chunk

To align RIFF chunks to certain boundaries (e.g., 2048 bytes for CD-ROMs) the RIFF specification includes a JUNK chunk. Its contents are to be skipped when reading. [Daubnet](https://www.daubnet.com/en/file-format-riff) Used for padding — some applications reserve space in the header to rewrite size fields later without shifting data.

---

## 9. WAVE_FORMAT_EXTENSIBLE

Beginning with Windows 2000, a WAVE_FORMAT_EXTENSIBLE header was defined, which specifies multiple audio channel data along with speaker positions, eliminates ambiguity regarding sample types and container sizes in the standard WAV format, and supports defining custom extensions to the format. [Wikipedia](https://en.wikipedia.org/wiki/WAV)

The WAVE_FORMAT_EXTENSIBLE format should be used whenever: PCM data has more than 16 bits per sample; the number of channels is more than 2; the actual number of bits per sample is not equal to the container size; or the mapping from channels to speakers needs to be specified. [Mcgill](https://www.mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/WAVE.html)

The fmt chunk body grows from 16 to 40 bytes:

```c
typedef struct {
    // standard 16-byte fmt body
    uint16_t audio_format;       // must be 0xFFFE for extensible
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;    // container size

    // extension: 24 more bytes
    uint16_t cb_size;            // size of extension = 22
    uint16_t valid_bits;         // actual bits used (e.g. 20 in 24-bit container)
    uint32_t channel_mask;       // speaker position bitmask
    uint8_t  sub_format[16];     // GUID of actual format
} WAVFormatExtensible;
```

The extension has one field which declares the number of valid bits per sample (wValidBitsPerSample). Another field (dwChannelMask) contains bits which indicate the mapping from channels to loudspeaker positions. The last field (SubFormat) is a 16-byte globally unique identifier (GUID). [Wavefilegem](https://wavefilegem.com/how_wave_files_work.html)

---

## 10. Size Arithmetic — Full Derivation

For a 1-second, mono, 16-bit, 44100 Hz WAV file:

```
num_samples      = 44100 × 1          = 44100
data_size        = 44100 × 2          = 88200 bytes
data_subchunk    = "data" + size + data
                 = 4 + 4 + 88200      = 88208 bytes
fmt_subchunk     = "fmt " + size + 16B body
                 = 4 + 4 + 16         = 24 bytes
WAVE_tag         = 4 bytes
RIFF_header      = "RIFF" + size
                 = 4 + 4              = 8 bytes

total file size  = 8 + 4 + 24 + 88208 = 88244 bytes
chunk_size field = 88244 - 8          = 88236
                 = 36 + 88200 ✓
```

---

## 11. What Happens If You Get It Wrong

Common mistakes and their audible/structural consequences:

| Mistake | Consequence |
|---------|-------------|
| `strcpy` instead of `memcpy` for tags | null byte overwrites next field, corrupt header |
| Wrong `chunk_size` | player may refuse to open file or truncate early |
| `bits_per_sample = 8` but writing int16_t samples | player reads wrong byte count, hissing noise |
| Wrong `byte_rate` | seeking by time lands on wrong position |
| No `__attribute__((packed))` on wrong arch | struct has invisible padding, header is malformed |
| Amplitude scaled by 32768 not 32767 | two's complement overflow, crackle at peaks |
| Text mode `fopen` (`"w"` not `"wb"`) | 0x0A bytes expanded to 0x0D 0x0A on Windows, shifts all data |
| Forgetting odd-byte padding after data | non-compliant file, some strict parsers reject it |
| `SampleRate` mismatch with actual data | plays at wrong pitch and speed |

---

## 12. The Struct in Full With Derivations

```c
#include <stdint.h>
#include <string.h>

typedef struct {
    // ── RIFF chunk header ──────────────────────────────────────
    char     riff_tag[4];        // "RIFF" — RIFF magic, 0x52494646
    uint32_t chunk_size;         // file_size - 8 = 36 + data_size

    // ── WAVE identifier ────────────────────────────────────────
    char     wave_form[4];       // "WAVE" — marks this RIFF as WAV

    // ── fmt sub-chunk ──────────────────────────────────────────
    char     fmt_tag[4];         // "fmt " — including trailing space
    uint32_t fmt_subchunk_size;  // 16 for PCM (counts body only)
    uint16_t audio_format;       // 1 = PCM, 3 = float, 0xFFFE = extensible
    uint16_t num_channels;       // 1 = mono, 2 = stereo
    uint32_t sample_rate;        // samples per second: 44100, 48000, etc.
    uint32_t byte_rate;          // sample_rate × num_channels × bits_per_sample/8
    uint16_t block_align;        // num_channels × bits_per_sample/8
    uint16_t bits_per_sample;    // 8, 16, 24, 32

    // ── data sub-chunk ─────────────────────────────────────────
    char     data_tag[4];        // "data" — 0x64617461
    uint32_t data_subchunk_size; // num_samples × num_channels × bits_per_sample/8

    // raw PCM samples follow immediately at offset 44
} __attribute__((packed)) WAVHeader;

void make_header(WAVHeader *hdr, int num_samples, int sample_rate,
                 int num_channels, int bits_per_sample) {

    int block_align = num_channels * (bits_per_sample / 8);
    int data_size   = num_samples * block_align;

    memcpy(hdr->riff_tag,   "RIFF", 4);
    hdr->chunk_size         = 36 + data_size;
    memcpy(hdr->wave_form,  "WAVE", 4);
    memcpy(hdr->fmt_tag,    "fmt ", 4);
    hdr->fmt_subchunk_size  = 16;
    hdr->audio_format       = 1;
    hdr->num_channels       = (uint16_t)num_channels;
    hdr->sample_rate        = (uint32_t)sample_rate;
    hdr->byte_rate          = (uint32_t)(sample_rate * block_align);
    hdr->block_align        = (uint16_t)block_align;
    hdr->bits_per_sample    = (uint16_t)bits_per_sample;
    memcpy(hdr->data_tag,   "data", 4);
    hdr->data_subchunk_size = (uint32_t)data_size;
}
```

This is the generalized version: pass in any sample rate, channel count, and bit depth and the derivations are computed correctly from the one source of truth — `block_align`.


## Writeups

For understanding the basic wavform generation in PCM format, see the writeup for [wavpcmv1.c](./wavpcmv1-writeup.md).
This is meant for learning the C internals alongside with binary data headers. 
WAV is a good start if you want to build some **beep generator** something like that.
