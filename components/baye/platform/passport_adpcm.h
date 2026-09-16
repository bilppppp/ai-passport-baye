#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// IMA ADPCM Decoder State
typedef struct {
    int16_t valprev; // Previous output sample (-32768..32767)
    int8_t  index;   // Step table index (0..88)
} passport_adpcm_state_t;

// Audio Stream Cursor
typedef struct {
    const uint8_t *data;
    size_t size;
    size_t offset;
    bool loop;
    uint32_t loop_count;
} passport_adpcm_stream_t;

// Reset decoder state to silence
void passport_adpcm_state_reset(passport_adpcm_state_t *state);

// Initialize stream cursor
void passport_adpcm_stream_init(passport_adpcm_stream_t *stream, const uint8_t *data, size_t size, bool loop);

// Rewind stream cursor
void passport_adpcm_stream_rewind(passport_adpcm_stream_t *stream);

// Decode a single 4-bit nibble
int16_t passport_adpcm_decode_nibble(passport_adpcm_state_t *state, uint8_t nibble);

// Decode a chunk of IMA ADPCM data into 16-bit PCM samples
// Each input byte yields exactly 2 output samples (low nibble first, then high nibble)
// Returns the number of PCM samples written to out_pcm
size_t passport_adpcm_decode_chunk(
    passport_adpcm_state_t *state,
    const uint8_t *in_adpcm,
    size_t in_bytes,
    int16_t *out_pcm,
    size_t out_max_samples
);

// Stream next N PCM samples from stream with automatic looping
// Returns the number of PCM samples written to out_pcm (0 if EOF and loop==false)
size_t passport_adpcm_stream_read(
    passport_adpcm_stream_t *stream,
    passport_adpcm_state_t *state,
    int16_t *out_pcm,
    size_t samples_requested
);

#ifdef __cplusplus
}
#endif
