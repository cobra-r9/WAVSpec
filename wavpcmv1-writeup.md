# Sine Wave Audio Generator in C

A minimal C program that synthesizes a pure sine wave, encodes it as a valid WAV file, plays it via ALSA, and removes the file — all from scratch with no audio libraries.



## Table of Contents

- [Overview](#overview)
- [The WAV File Format](#the-wav-file-format)
- [Code Walkthrough](#code-walkthrough)
- [Build and Run](#build-and-run)
- [Concepts](#concepts)



## Overview

Most audio programs rely on libraries like libsoundio, PortAudio, or SDL_mixer. This program does none of that. It constructs the WAV binary format by hand, writes it to disk, and invokes `aplay` to play it. Every byte in the output file is intentional and explained.

The audio produced is a pure sine tone — a single frequency with no harmonics, no noise. It is the simplest possible periodic waveform.



## The WAV File Format

WAV is a subformat of **RIFF** (Resource Interchange File Format), a chunked binary container developed by Microsoft. RIFF organizes data into labeled chunks — each chunk has a 4-byte ASCII tag, a 4-byte size, and then its data.

A WAV file has this structure on disk:

```
Offset  Size  Field
------  ----  -----
0       4     "RIFF"                  ← RIFF chunk tag
4       4     Total file size - 8     ← RIFF chunk size
8       4     "WAVE"                  ← Format identifier
12      4     "fmt "                  ← fmt sub-chunk tag
16      4     16                      ← fmt sub-chunk body size (always 16 for PCM)
20      2     Audio format (1 = PCM)
22      2     Num channels
24      4     Sample rate
28      4     Byte rate
32      2     Block align
34      2     Bits per sample
36      4     "data"                  ← data sub-chunk tag
40      4     Data size in bytes      ← data sub-chunk size
44      ...   Raw PCM samples         ← actual audio
```

Total header size: **44 bytes**. Everything after byte 44 is raw audio data.

### Why RIFF uses chunk sizes the way it does

The RIFF spec counts chunk size as the number of bytes **following** the tag+size fields of that chunk. So the top-level RIFF chunk size is `total_file_size - 8` (excluding the 4-byte `"RIFF"` tag and 4-byte size field). Similarly, `fmt_subchunk_size = 16` counts only the 6 body fields of the fmt chunk, not the `"fmt "` tag or the size field itself.

### What PCM means

PCM stands for **Pulse Code Modulation**. It means the audio is stored as a direct sequence of amplitude snapshots — no compression, no encoding. Each sample is an integer representing the air pressure displacement at that instant in time. `audio_format = 1` in the header signals this.

### Why 44100 Hz sample rate

Human hearing tops out at roughly 20,000 Hz. The **Nyquist–Shannon sampling theorem** states that to accurately reconstruct a frequency, you need a sample rate of at least **2×** that frequency. So to reproduce 20,000 Hz you need at minimum 40,000 samples per second. 44,100 Hz was chosen for CD audio as it gives a small margin above that limit while fitting cleanly into the storage constraints of early digital audio hardware.

### Why int16_t for samples

Each sample is a signed 16-bit integer, giving a range of **−32768 to 32767**. This is 16-bit depth — 65536 discrete amplitude levels. The more levels, the finer the amplitude resolution, and the lower the quantization noise. CD audio uses 16-bit. Studio recordings often use 24-bit. 8-bit sounds noticeably grainy.



## Code Walkthrough

### Includes

```c
#include <stdio.h>    // FILE, fopen, fwrite, fclose, fprintf, perror, snprintf
#include <stdlib.h>   // malloc, free, system
#include <math.h>     // sin(), M_PI
#include <string.h>   // memcpy
#include <stdint.h>   // int16_t, uint16_t, uint32_t
```

`stdint.h` is critical here. Plain `int` and `short` have implementation-defined sizes. The WAV format is a binary specification with exact byte counts at exact offsets. Using `uint32_t` guarantees 4 bytes and `uint16_t` guarantees 2 bytes on every platform.



### Constants

```c
static const int    SAMPLE_RATE  = 44100;
static const int    DURATION     = 1;
static const double FREQUENCY    = 10000.0;
static const double AMPLITUDE    = 0.5;
static const char  *OUTPUT_FILE  = "out.wav";
```

`static const` is used instead of `#define` for two reasons. First, `#define` is a preprocessor text substitution — it has no type, no scope, and does not appear in the debugger's symbol table. `static const` gives you a typed, scoped variable that debuggers can inspect. Second, `static` at file scope means the symbol is **not exported** — it is invisible to other translation units and cannot be accidentally linked against via `extern`.

`OUTPUT_FILE` is a pointer to a string literal. The string `"out.wav"` lives in the `.rodata` section (read-only data). The pointer itself is const, and the data it points to is read-only memory — mutating it would be undefined behavior.



### The WAV Header Struct

```c
typedef struct {
    char     riff_tag[4];
    uint32_t chunk_size;
    char     wave_form[4];
    char     fmt_tag[4];
    uint32_t fmt_subchunk_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char     data_tag[4];
    uint32_t data_subchunk_size;
} __attribute__((packed)) WAVHeader;
```

The struct fields map directly onto the 44-byte WAV header in memory. When you do `fwrite(&hdr, sizeof(WAVHeader), 1, f)`, those 44 bytes go to disk in field order — which is exactly what a WAV parser expects to read.

**Why `__attribute__((packed))`**

By default, the C compiler is allowed to insert **padding bytes** between struct fields to satisfy alignment requirements. For example, a `uint16_t` followed by a `uint32_t` might get 2 bytes of padding inserted between them so the `uint32_t` lands on a 4-byte boundary. This struct happens to be naturally well-aligned (no padding is needed), but `packed` makes that explicit and guaranteed — so that even on an unusual architecture, the compiler never inserts phantom bytes that would corrupt the header layout.

**Why `memcpy` and not `strcpy` for the tag fields**

```c
memcpy(hdr->riff_tag, "RIFF", 4);  // correct
strcpy(hdr->riff_tag, "RIFF");     // WRONG — writes 5 bytes, overflows into chunk_size
```

`strcpy` always copies the null terminator `\0` as well. `"RIFF"` is 5 bytes including `\0`. Writing 5 bytes into a 4-byte field overflows into the next field (`chunk_size`), silently corrupting it. `memcpy` lets you specify exactly 4 bytes — no null terminator, no overflow.



### Header Construction

```c
static void make_header(int num_samples, WAVHeader *hdr) {
    uint32_t data_size         = num_samples * sizeof(int16_t);
    uint32_t chunk_size        = 36 + data_size;
    uint32_t fmt_subchunk_size = 16;
    uint16_t audio_format      = 1;
    uint16_t num_channels      = 1;
    uint16_t block_align       = sizeof(int16_t);       // 2 bytes per sample
    uint16_t bits_per_sample   = block_align * 8;       // 16 bits
    uint32_t byte_rate         = SAMPLE_RATE * block_align;

    memcpy(hdr->riff_tag,  "RIFF", 4);
    hdr->chunk_size        = chunk_size;
    memcpy(hdr->wave_form, "WAVE", 4);
    memcpy(hdr->fmt_tag,   "fmt ", 4);  // trailing space is part of the spec
    hdr->fmt_subchunk_size = fmt_subchunk_size;
    hdr->audio_format      = audio_format;
    hdr->num_channels      = num_channels;
    hdr->sample_rate       = SAMPLE_RATE;
    hdr->byte_rate         = byte_rate;
    hdr->block_align       = block_align;
    hdr->bits_per_sample   = bits_per_sample;
    memcpy(hdr->data_tag,  "data", 4);
    hdr->data_subchunk_size = data_size;
}
```

**Field derivations:**

| Field | Formula | Value |
|---|---|---|
| `data_size` | `num_samples × 2` | 88200 bytes |
| `chunk_size` | `36 + data_size` | 88236 |
| `fmt_subchunk_size` | Fixed by PCM spec | 16 |
| `block_align` | `channels × (bits/8)` | 2 |
| `bits_per_sample` | `block_align × 8` | 16 |
| `byte_rate` | `sample_rate × block_align` | 88200 |

`chunk_size = 36 + data_size` because the 36 accounts for: the `"WAVE"` tag (4), the `"fmt "` tag (4), `fmt_subchunk_size` field (4), the 16 bytes of fmt body (16), the `"data"` tag (4), and `data_subchunk_size` field (4). That totals exactly 36 bytes of overhead between the chunk_size field and the raw samples.



### Sample Generation

```c
static int16_t *generate_samples(int num_samples) {
    int16_t *samples = (int16_t *)malloc(num_samples * sizeof(int16_t));
    if (!samples) return NULL;

    for (int i = 0; i < num_samples; i++) {
        double t    = (double)i / SAMPLE_RATE;
        double fade = 1.0;

        if (i > num_samples - 100)
            fade = (double)(num_samples - i) / 100.0;

        samples[i] = (int16_t)(AMPLITUDE * fade * 32767.0 * sin(2.0 * M_PI * FREQUENCY * t));
    }

    return samples;
}
```

**The sine formula:**

```
sample[i] = AMPLITUDE × fade × 32767 × sin(2π × f × t)
```

Where:
- `t = i / SAMPLE_RATE` — time in seconds at sample index `i`
- `2π × f` — angular frequency ω in radians per second
- `sin(ωt)` — produces a value in `[-1.0, 1.0]`
- `× 32767.0` — scales to the full int16 positive range
- `× AMPLITUDE` — applies the volume factor
- `(int16_t)(...)` — truncates double to signed 16-bit integer

**Why 32767 and not 32768:**

int16_t is asymmetric. The range is `−32768` to `+32767`. At `sin = 1.0`, multiplying by 32768 gives 32768, which overflows int16_t and wraps via two's complement to `−32768` — a sudden large negative spike, audible as a crack. Using 32767 keeps the peak exactly within range.

**Why `malloc` instead of a VLA:**

```c
// Original approach — stack allocation
int16_t samples[num_samples];  // 88200 × 2 = ~172 KB on the stack

// This approach — heap allocation
int16_t *samples = malloc(num_samples * sizeof(int16_t));
```

The default stack size on Linux is 8 MB. 172 KB for 1 second is fine, but for longer durations (say, 60 seconds) a VLA would be ~5.2 MB — dangerously close to the limit, and it grows silently without any error. `malloc` allocates from the heap which is limited only by available memory, and crucially, it returns `NULL` on failure so you can handle it.

**The fade-out:**

At `FREQUENCY = 10000.0` and `DURATION = 1`, the wave completes 10,000 cycles in 1 second. The sample rate of 44100 does not divide evenly into 10,000 Hz cycles, so the waveform does not end at a zero crossing — it cuts abruptly mid-cycle. This creates a **discontinuity** in the signal, perceived as a sharp click.

The fix is a linear amplitude ramp over the last 100 samples:

```c
if (i > num_samples - 100)
    fade = (double)(num_samples - i) / 100.0;
```

When `i = num_samples - 100`, `fade = 100/100 = 1.0` (no change). When `i = num_samples - 1`, `fade = 1/100 = 0.01` (nearly silent). This ramps amplitude smoothly to near-zero, eliminating the click.



### File Writing

```c
static int write_wav(const char *path, const WAVHeader *hdr,
                     const int16_t *samples, int num_samples) {
    FILE *f = fopen(path, "wb");
    if (!f) { perror("fopen"); return -1; }

    if (fwrite(hdr, sizeof(WAVHeader), 1, f) != 1) {
        perror("fwrite header");
        fclose(f);
        return -1;
    }

    if (fwrite(samples, sizeof(int16_t), num_samples, f) != (size_t)num_samples) {
        perror("fwrite samples");
        fclose(f);
        return -1;
    }

    fclose(f);
    return 0;
}
```

**Binary mode `"wb"`:** The `b` flag disables newline translation. On Windows, text mode converts `\n` (0x0A) to `\r\n` (0x0D 0x0A). Any sample byte that happens to be 0x0A would be expanded, shifting every subsequent byte and corrupting the file. Binary mode writes bytes exactly as given.

**`fwrite` return value:** `fwrite` returns the number of items successfully written. Checking it against the expected count is the only way to detect a disk-full or I/O error during write. Ignoring it means you'd play a truncated or corrupt file with no indication of what went wrong.

**`perror`:** Prints the last OS error (from `errno`) to stderr with your label prepended. So `perror("fopen")` might print: `fopen: Permission denied`. More informative than a bare `fprintf(stderr, "failed\n")`.

**`fclose` on error path:** The file handle must be closed even when the write fails. Omitting it leaks a file descriptor. On Linux the default fd limit is 1024 — leaking them in a loop would eventually cause `fopen` to return `NULL` with `Too many open files`.



### Playback and Cleanup

```c
static void play_and_remove(const char *path) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "aplay %s && rm %s", path, path);
    system(cmd);
}
```

**`snprintf` instead of `sprintf`:** `sprintf` writes until done with no regard for buffer size — a classic buffer overflow. `snprintf` takes the buffer size as its second argument and truncates safely, never writing past the boundary.

**`&&` in the shell command:** This is shell short-circuit evaluation. `rm` only executes if `aplay` exits with code 0 (success). If playback fails, the WAV file is left on disk — useful for debugging. If it were `aplay %s; rm %s` (semicolon), `rm` would run regardless of whether playback succeeded.

**`system()`:** Forks a shell (`/bin/sh -c`), passes the command string to it, and blocks until the shell exits. It is not the most efficient approach — you could use `fork`/`exec`/`waitpid` directly — but it is simple and correct for this use case.



### main()

```c
int main(void) {
    int num_samples = SAMPLE_RATE * DURATION;

    int16_t *samples = generate_samples(num_samples);
    if (!samples) {
        fprintf(stderr, "malloc failed\n");
        return 1;
    }

    WAVHeader hdr;
    make_header(num_samples, &hdr);

    // Idiomatic pattern: free before checking, no leak on either path
    int result = write_wav(OUTPUT_FILE, &hdr, samples, num_samples);
    free(samples);
    if (result != 0) return 1;

    play_and_remove(OUTPUT_FILE);
    return 0;
}
```

**`free` before the result check:** This is the correct pattern. If you free inside the `if (result != 0)` block only, and then fall through to success, you still need to free below. Putting `free` unconditionally before the check means you never leak regardless of the outcome — and it avoids duplicating the `free` call.

**`WAVHeader hdr` on the stack:** The header is only 44 bytes. Stack allocation is fine and correct here — no reason to `malloc` it. It is passed to `make_header` by pointer so the function can populate its fields in-place.



## Build and Run

```bash
gcc sine.c -o sine -lm && ./sine
```

- `-lm` links `libm`, which provides `sin()` and `M_PI`. GCC does not link it automatically — only `libc` is implicit.
- Flag order matters: source files before library flags. The linker resolves symbols left to right; libraries must come after the objects that reference them.

For strict compilation:

```bash
gcc sine.c -o sine -lm -Wall -Wextra -pedantic -std=c11
```



## Concepts

### Nyquist Theorem
To reconstruct a frequency `f`, you need a sample rate of at least `2f`. At 44100 Hz sample rate, the highest reproducible frequency is 22050 Hz — just above human hearing range.

### Quantization
Converting the continuous sine value (a double) to a discrete int16 introduces **quantization error** — the difference between the true value and the nearest integer. At 16-bit depth this error is inaudible. At 8-bit it becomes audible noise.

### Aliasing
If you set `FREQUENCY` above 22050 Hz (half the sample rate), the sine wave cannot be correctly represented. The samples will encode a different, lower frequency instead — an artifact called **aliasing**. Try `FREQUENCY = 30000.0` and you'll hear a tone lower than expected.

### Two's Complement Overflow
`int16_t` uses two's complement. If a value exceeds 32767, it wraps to the negative side. At full amplitude (`AMPLITUDE = 1.0`), the peak `sin = 1.0` gives `1.0 × 32768 = 32768`, which wraps to `−32768` — a sudden large spike audible as distortion. The upper bound must be `32767`, not `32768`.
