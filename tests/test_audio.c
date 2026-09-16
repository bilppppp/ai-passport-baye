#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "passport_adpcm.h"
#include "passport_audio.h"

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

static void test_loop_determinism(void) {
    #define STREAM_BYTES 200
    #define TOTAL_SAMPLES (STREAM_BYTES * 2)
    uint8_t adpcm_data[STREAM_BYTES];
    for (int i = 0; i < STREAM_BYTES; i++) {
        adpcm_data[i] = (uint8_t)((i * 37 + 13) & 0xFF);
    }

    passport_adpcm_stream_t stream;
    passport_adpcm_state_t state;
    passport_adpcm_state_reset(&state);
    passport_adpcm_stream_init(&stream, adpcm_data, STREAM_BYTES, true);

    int16_t loop1_pcm[TOTAL_SAMPLES];
    int16_t loop2_pcm[TOTAL_SAMPLES];

    // Read loop 1 in arbitrary chunk sizes (e.g. 50 samples)
    size_t samples_read = 0;
    while (samples_read < TOTAL_SAMPLES) {
        size_t n = passport_adpcm_stream_read(&stream, &state, loop1_pcm + samples_read, 50);
        assert(n > 0);
        samples_read += n;
    }
    assert(samples_read == TOTAL_SAMPLES);
    assert(stream.offset == stream.size);

    // Read loop 2 in different chunk sizes (e.g. 32 samples)
    samples_read = 0;
    while (samples_read < TOTAL_SAMPLES) {
        size_t to_req = (TOTAL_SAMPLES - samples_read > 32) ? 32 : (TOTAL_SAMPLES - samples_read);
        size_t n = passport_adpcm_stream_read(&stream, &state, loop2_pcm + samples_read, to_req);
        assert(n > 0);
        samples_read += n;
    }
    assert(samples_read == TOTAL_SAMPLES);
    assert(stream.loop_count == 1);
    assert(stream.offset == stream.size);

    // Byte-for-byte comparison of Loop 1 PCM vs Loop 2 PCM
    assert(memcmp(loop1_pcm, loop2_pcm, sizeof(loop1_pcm)) == 0);

    // Test chunk reads that cross the boundary in the middle of a chunk
    passport_adpcm_state_reset(&state);
    passport_adpcm_stream_init(&stream, adpcm_data, STREAM_BYTES, true);
    int16_t boundary_test_pcm[TOTAL_SAMPLES * 2];
    size_t r1 = passport_adpcm_stream_read(&stream, &state, boundary_test_pcm, 300);
    assert(r1 == 300);
    assert(stream.loop_count == 0);
    size_t r2 = passport_adpcm_stream_read(&stream, &state, boundary_test_pcm + 300, 200);
    assert(r2 == 200);
    assert(stream.loop_count == 1);
    // Samples 0..99 of loop 2 must match samples 0..99 of loop 1
    assert(memcmp(boundary_test_pcm + 400, boundary_test_pcm, 100 * sizeof(int16_t)) == 0);

    printf("test_loop_determinism: PASS\n");
}

static void test_volume_controls(void) {
    char buf[32];
    int len;

    len = passport_volume_format_text(0, buf, sizeof(buf));
    assert(len == 5);
    assert(strcmp(buf, "VOL 0") == 0);

    len = passport_volume_format_text(50, buf, sizeof(buf));
    assert(len == 6);
    assert(strcmp(buf, "VOL 50") == 0);

    len = passport_volume_format_text(100, buf, sizeof(buf));
    assert(len == 7);
    assert(strcmp(buf, "VOL 100") == 0);

    // Negative / overflow clamping
    len = passport_volume_format_text(-10, buf, sizeof(buf));
    assert(len == 5);
    assert(strcmp(buf, "VOL 0") == 0);

    len = passport_volume_format_text(120, buf, sizeof(buf));
    assert(len == 7);
    assert(strcmp(buf, "VOL 100") == 0);

    // Small buffer guard
    char small_buf[5];
    len = passport_volume_format_text(50, small_buf, sizeof(small_buf));
    assert(len == -1);

    // Bitmap rendering test with canary bounds
    #define CANARY 0xCAFE
    #define PADDING 16
    const int total_pixels = VOLUME_W * VOLUME_H + PADDING * 2;
    uint16_t buffer[total_pixels];

    for (int i = 0; i < total_pixels; i++) buffer[i] = CANARY;
    uint16_t *widget = buffer + PADDING;

    passport_volume_render_bitmap(50, widget, VOLUME_W, VOLUME_H);

    for (int i = 0; i < PADDING; i++) {
        assert(buffer[i] == CANARY);
        assert(widget[VOLUME_W * VOLUME_H + i] == CANARY);
    }

    int fg_count = 0, bg_count = 0;
    for (int i = 0; i < VOLUME_W * VOLUME_H; i++) {
        if (widget[i] == 0xFFFF) fg_count++;
        else if (widget[i] == 0x0000) bg_count++;
        else assert(0 && "Unexpected pixel color in volume widget");
    }
    assert(fg_count > 30 && "Expected reasonable foreground pixel count for VOL 50");
    assert(bg_count > 100 && "Expected black background pixels");

    // Volume adjustment and state logic
    passport_audio_set_volume(50);
    assert(passport_audio_get_volume() == 50);

    passport_audio_adjust_volume(+10);
    assert(passport_audio_get_volume() == 60);

    passport_audio_adjust_volume(+50);
    assert(passport_audio_get_volume() == 100);

    passport_audio_adjust_volume(+10); // Clamped at 100
    assert(passport_audio_get_volume() == 100);

    passport_audio_adjust_volume(-10);
    assert(passport_audio_get_volume() == 90);

    passport_audio_adjust_volume(-100); // Clamped at 0 (mute)
    assert(passport_audio_get_volume() == 0);

    passport_audio_adjust_volume(-10); // Still 0
    assert(passport_audio_get_volume() == 0);

    printf("test_volume_controls: PASS\n");
}

int main(void) {
    printf("--- Running test_audio ---\n");
    test_adpcm_basic();
    test_chunk_boundaries();
    test_streaming_and_loop();
    test_clamping_and_corruption();
    test_loop_determinism();
    test_volume_controls();
    return 0;
}


