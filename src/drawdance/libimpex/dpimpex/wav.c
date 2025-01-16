// SPDX-License-Identifier: GPL-3.0-or-later
#include "wav.h"
#include "math.h"
#include <dpcommon/binary.h>
#include <dpcommon/conversions.h>
#include <dpcommon/file.h>


unsigned char *DP_wav_read_from_path(const char *path, int volume,
                                     size_t *out_length)
{
    size_t length;
    unsigned char *buffer = DP_file_slurp(path, &length);
    if (!buffer) {
        return NULL;
    }

    size_t pos = 0;

#define DP_WAV_BAIL(...)           \
    do {                           \
        DP_error_set(__VA_ARGS__); \
        DP_free(buffer);           \
        return NULL;               \
    } while (0)

#define DP_WAV_SKIP(N)                            \
    do {                                          \
        if (pos + N > length) {                   \
            DP_WAV_BAIL("Premature end of file"); \
        }                                         \
        pos += N;                                 \
    } while (0)

#define DP_WAV_READ(N, DATA, BLOCK)         \
    do {                                    \
        unsigned char *DATA = buffer + pos; \
        DP_WAV_SKIP(N);                     \
        BLOCK                               \
    } while (0)

    unsigned char riff_magic[] = {0x52, 0x49, 0x46, 0x46}; // "RIFF"
    DP_WAV_READ(4, data, {
        if (memcmp(data, riff_magic, 4) != 0) {
            DP_WAV_BAIL("Invalid RIFF header");
        }
    });

    size_t available_size;
    DP_WAV_READ(4, data, {
        size_t reported_size = DP_read_littleendian_uint32(data);
        available_size = reported_size + 8;
        if (available_size > length) {
            DP_WAV_BAIL("Invalid RIFF size %zu + 8 > %zu", reported_size,
                        length);
        }
    });

    unsigned char wave_magic[] = {0x57, 0x41, 0x56, 0x45}; // "WAVE"
    DP_WAV_READ(4, data, {
        if (memcmp(data, wave_magic, 4) != 0) {
            DP_WAV_BAIL("Invalid WAVE header");
        }
    });

    unsigned char fmt_magic[] = {0x66, 0x6d, 0x74, 0x20}; // "fmt "
    DP_WAV_READ(4, data, {
        if (memcmp(data, fmt_magic, 4) != 0) {
            DP_WAV_BAIL("Invalid format chunk header");
        }
    });

    size_t fmt_size;
    DP_WAV_READ(4, data, {
        fmt_size = DP_read_littleendian_uint32(data);
        if (fmt_size < 16) {
            DP_WAV_BAIL("Format chunk size %zu too short", fmt_size);
        }
    });

    DP_WAV_READ(fmt_size, data, {
        int format = DP_read_littleendian_uint16(data);
        if (format != 1) {
            DP_WAV_BAIL("Invalid WAVE format %d", format);
        }
        int bits = DP_read_littleendian_uint16(data + 14);
        if (bits != 16) {
            DP_WAV_BAIL("Invalid bits per sample %d", bits);
        }
    });

    size_t data_size;
    while (true) {
        bool is_data = false;
        unsigned char data_magic[] = {0x64, 0x61, 0x74, 0x61}; // "data"
        DP_WAV_READ(4, data, {
            if (memcmp(data, data_magic, 4) == 0) {
                is_data = true;
            }
        });

        size_t chunk_size;
        DP_WAV_READ(4, data, {
            chunk_size = DP_read_littleendian_uint32(data);
            if (chunk_size > available_size - pos) {
                DP_WAV_BAIL("Chunk size out of bounds");
            }
        });

        if (is_data) {
            data_size = chunk_size;
            break;
        }
        else {
            DP_WAV_SKIP(chunk_size);
        }
    }

    if (volume != 100) {
        double scale = DP_max_double(0.0, DP_int_to_double(volume) / 100.0);
        DP_WAV_READ(data_size, data, {
            for (size_t i = 0; i + 1 < data_size; i += 2) {
                int16_t sample = DP_read_littleendian_int16(data + i);
                int16_t scaled_sample = DP_double_to_int16(DP_max_double(
                    (double)INT16_MIN,
                    DP_min_double((double)INT16_MAX,
                                  round(DP_int16_to_double(sample) * scale))));
                DP_write_littleendian_int16(scaled_sample, data + i);
            }
        });
    }

    if (out_length) {
        *out_length = length;
    }
    return buffer;
}
