#include "passport_adpcm.h"
#include <string.h>

// Standard IMA / DVI ADPCM step size table
static const int16_t STEP_TABLE[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
    19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

// Standard IMA index adjustment table for 4-bit nibbles
static const int8_t INDEX_TABLE[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8
};

void passport_adpcm_state_reset(passport_adpcm_state_t *state) {
    if (!state) return;
    state->valprev = 0;
    state->index = 0;
}

void passport_adpcm_stream_init(passport_adpcm_stream_t *stream, const uint8_t *data, size_t size, bool loop) {
    if (!stream) return;
    stream->data = data;
    stream->size = size;
    stream->offset = 0;
    stream->loop = loop;
    stream->loop_count = 0;
}

void passport_adpcm_stream_rewind(passport_adpcm_stream_t *stream) {
    if (!stream) return;
    stream->offset = 0;
}

int16_t passport_adpcm_decode_nibble(passport_adpcm_state_t *state, uint8_t nibble) {
    if (!state) return 0;

    int8_t index = state->index;
    if (index < 0) index = 0;
    if (index > 88) index = 88;

    int16_t step = STEP_TABLE[index];
    int32_t valprev = state->valprev;

    // Standard IMA difference formula:
    // diff = (nibble + 0.5) * step / 4
    int32_t diff = step >> 3;
    if (nibble & 1) diff += step >> 2;
    if (nibble & 2) diff += step >> 1;
    if (nibble & 4) diff += step;

    if (nibble & 8) {
        valprev -= diff;
    } else {
        valprev += diff;
    }

    // Clamp output sample to 16-bit signed range
    if (valprev > 32767) valprev = 32767;
    else if (valprev < -32768) valprev = -32768;

    // Update index
    index += INDEX_TABLE[nibble & 0x0F];
    if (index < 0) index = 0;
    else if (index > 88) index = 88;

    state->valprev = (int16_t)valprev;
    state->index = index;

    return (int16_t)valprev;
}

size_t passport_adpcm_decode_chunk(
    passport_adpcm_state_t *state,
    const uint8_t *in_adpcm,
    size_t in_bytes,
    int16_t *out_pcm,
    size_t out_max_samples
) {
    if (!state || !in_adpcm || !out_pcm || in_bytes == 0 || out_max_samples == 0) {
        return 0;
    }

    size_t samples_written = 0;

    for (size_t i = 0; i < in_bytes && samples_written < out_max_samples; i++) {
        uint8_t byte_val = in_adpcm[i];

        // Low nibble (sample 0)
        out_pcm[samples_written++] = passport_adpcm_decode_nibble(state, byte_val & 0x0F);
        if (samples_written >= out_max_samples) break;

        // High nibble (sample 1)
        out_pcm[samples_written++] = passport_adpcm_decode_nibble(state, (byte_val >> 4) & 0x0F);
    }

    return samples_written;
}

size_t passport_adpcm_stream_read(
    passport_adpcm_stream_t *stream,
    passport_adpcm_state_t *state,
    int16_t *out_pcm,
    size_t samples_requested
) {
    if (!stream || !state || !out_pcm || !stream->data || stream->size == 0 || samples_requested == 0) {
        return 0;
    }

    size_t total_samples = 0;

    while (total_samples < samples_requested) {
        if (stream->offset >= stream->size) {
            if (stream->loop) {
                stream->offset = 0;
                stream->loop_count++;
            } else {
                break; // EOF
            }
        }

        size_t samples_needed = samples_requested - total_samples;
        size_t bytes_needed = (samples_needed + 1) / 2;
        size_t bytes_available = stream->size - stream->offset;
        size_t bytes_to_read = (bytes_needed < bytes_available) ? bytes_needed : bytes_available;

        if (bytes_to_read == 0) break;

        size_t decoded = passport_adpcm_decode_chunk(
            state,
            stream->data + stream->offset,
            bytes_to_read,
            out_pcm + total_samples,
            samples_needed
        );

        total_samples += decoded;
        stream->offset += (decoded + 1) / 2;
    }

    return total_samples;
}
