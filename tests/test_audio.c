#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "passport_adpcm.h"

static void test_adpcm_basic(void) {
    passport_adpcm_state_t state;
    passport_adpcm_state_reset(&state);

    assert(state.valprev == 0);
    assert(state.index == 0);

    // Decode silence (nibble 0 at index 0 produces 0 difference since 7 >> 3 == 0)
    int16_t s0 = passport_adpcm_decode_nibble(&state, 0);
    assert(s0 == 0);
    assert(state.index == 0);

    // Decode large jump
    passport_adpcm_decode_nibble(&state, 7); // Max positive jump
    assert(state.index > 0);

    printf("test_adpcm_basic: PASS\n");
}

static void test_chunk_boundaries(void) {
    passport_adpcm_state_t state;
    passport_adpcm_state_reset(&state);

    // 10 bytes = 20 samples
    uint8_t adpcm_data[10];
    for (int i = 0; i < 10; i++) adpcm_data[i] = (uint8_t)(i * 17);

    int16_t pcm[32];
    size_t decoded = passport_adpcm_decode_chunk(&state, adpcm_data, 10, pcm, 32);
    assert(decoded == 20);

    // Limit output capacity to 7 samples
    passport_adpcm_state_reset(&state);
    decoded = passport_adpcm_decode_chunk(&state, adpcm_data, 10, pcm, 7);
    assert(decoded == 7);

    // Zero input
    decoded = passport_adpcm_decode_chunk(&state, adpcm_data, 0, pcm, 32);
    assert(decoded == 0);

    // Null safety
    assert(passport_adpcm_decode_chunk(NULL, adpcm_data, 10, pcm, 32) == 0);
    assert(passport_adpcm_decode_chunk(&state, NULL, 10, pcm, 32) == 0);
    assert(passport_adpcm_decode_chunk(&state, adpcm_data, 10, NULL, 32) == 0);

    printf("test_chunk_boundaries: PASS\n");
}

static void test_streaming_and_loop(void) {
    passport_adpcm_stream_t stream;
    passport_adpcm_state_t state;

    // 16 bytes = 32 samples
    uint8_t dummy_data[16];
    for (int i = 0; i < 16; i++) dummy_data[i] = (uint8_t)(i * 13 + 5);

    passport_adpcm_state_reset(&state);
    passport_adpcm_stream_init(&stream, dummy_data, sizeof(dummy_data), true);

    assert(stream.offset == 0);
    assert(stream.loop == true);
    assert(stream.loop_count == 0);

    int16_t pcm_buf[64];

    // Read 20 samples (10 bytes)
    size_t read1 = passport_adpcm_stream_read(&stream, &state, pcm_buf, 20);
    assert(read1 == 20);
    assert(stream.offset == 10);
    assert(stream.loop_count == 0);

    // Read 24 samples: needs 12 samples to EOF (6 bytes), then loops to read remaining 12 samples
    size_t read2 = passport_adpcm_stream_read(&stream, &state, pcm_buf + 20, 24);
    assert(read2 == 24);
    assert(stream.loop_count == 1);
    assert(stream.offset == 6);

    // Read without loop
    passport_adpcm_stream_init(&stream, dummy_data, sizeof(dummy_data), false);
    passport_adpcm_state_reset(&state);
    size_t read_all = passport_adpcm_stream_read(&stream, &state, pcm_buf, 100);
    assert(read_all == 32); // Stopped at EOF
    assert(stream.offset == 16);
    assert(stream.loop_count == 0);

    // Reading at EOF without loop returns 0
    size_t read_eof = passport_adpcm_stream_read(&stream, &state, pcm_buf, 10);
    assert(read_eof == 0);

    printf("test_streaming_and_loop: PASS\n");
}

static void test_clamping_and_corruption(void) {
    passport_adpcm_state_t state;
    passport_adpcm_state_reset(&state);

    // Sequence of maximum positive increments to force clamping
    for (int i = 0; i < 100; i++) {
        int16_t sample = passport_adpcm_decode_nibble(&state, 7);
        assert(sample <= 32767);
    }
    assert(state.valprev == 32767);

    // Sequence of maximum negative increments to force clamping
    for (int i = 0; i < 100; i++) {
        int16_t sample = passport_adpcm_decode_nibble(&state, 0x0F);
        assert(sample >= -32768);
    }
    assert(state.valprev == -32768);

    // Nibbles with high bits out of 4-bit range
    int16_t s_corrupt = passport_adpcm_decode_nibble(&state, 0xFF);
    assert(s_corrupt >= -32768 && s_corrupt <= 32767);

    printf("test_clamping_and_corruption: PASS\n");
}

int main(void) {
    printf("--- Running test_audio ---\n");
    test_adpcm_basic();
    test_chunk_boundaries();
    test_streaming_and_loop();
    test_clamping_and_corruption();
    return 0;
}
