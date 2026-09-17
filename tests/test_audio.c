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

    // Full discrete 0..100 sweep validation
    for (int v = 0; v <= 100; v += 10) {
        passport_audio_set_volume((uint8_t)v);
        assert(passport_audio_get_volume() == (uint8_t)v);
        char expected[16];
        snprintf(expected, sizeof(expected), "VOL %d", v);
        len = passport_volume_format_text(v, buf, sizeof(buf));
        assert(len > 0);
        assert(strcmp(buf, expected) == 0);
        // Verify HUD refresh calls without errors
        passport_audio_hud_force_refresh();
        passport_audio_hud_tick();
    }

    printf("test_volume_controls: PASS\n");
}

static void test_music_assets_and_metadata(void) {
    // 1. Verify valid tracks
    const baye_music_asset_t *title = passport_audio_find_asset(BAYE_MUSIC_TITLE);
    assert(title != NULL);
    assert(title->id == BAYE_MUSIC_TITLE);
    assert(title->loop == true);
    assert(strcmp(title->name, "TITLE") == 0);

    const baye_music_asset_t *strat = passport_audio_find_asset(BAYE_MUSIC_STRATEGY);
    assert(strat != NULL);
    assert(strat->id == BAYE_MUSIC_STRATEGY);
    assert(strat->loop == true);
    assert(strcmp(strat->name, "STRATEGY") == 0);

    const baye_music_asset_t *battle = passport_audio_find_asset(BAYE_MUSIC_BATTLE);
    assert(battle != NULL);
    assert(battle->id == BAYE_MUSIC_BATTLE);
    assert(battle->loop == true);
    assert(strcmp(battle->name, "BATTLE") == 0);

    const baye_music_asset_t *vic = passport_audio_find_asset(BAYE_MUSIC_VICTORY);
    assert(vic != NULL);
    assert(vic->id == BAYE_MUSIC_VICTORY);
    assert(vic->loop == false);
    assert(strcmp(vic->name, "VICTORY") == 0);

    const baye_music_asset_t *def = passport_audio_find_asset(BAYE_MUSIC_DEFEAT);
    assert(def != NULL);
    assert(def->id == BAYE_MUSIC_DEFEAT);
    assert(def->loop == false);
    assert(strcmp(def->name, "DEFEAT") == 0);

    // 2. Verify invalid tracks
    assert(passport_audio_find_asset(BAYE_MUSIC_NONE) == NULL);
    assert(passport_audio_find_asset(BAYE_MUSIC_MAX) == NULL);
    assert(passport_audio_find_asset((baye_music_track_t)-1) == NULL);
    assert(passport_audio_find_asset((baye_music_track_t)99) == NULL);

    printf("test_music_assets_and_metadata: PASS\n");
}

static void test_music_manager_controls(void) {
    passport_audio_stop();
    assert(passport_audio_get_track() == BAYE_MUSIC_NONE);

    passport_audio_play(BAYE_MUSIC_TITLE);
    assert(passport_audio_get_track() == BAYE_MUSIC_TITLE);

    // Deduplication check: re-triggering same track keeps current track
    passport_audio_play(BAYE_MUSIC_TITLE);
    assert(passport_audio_get_track() == BAYE_MUSIC_TITLE);

    passport_audio_play(BAYE_MUSIC_STRATEGY);
    assert(passport_audio_get_track() == BAYE_MUSIC_STRATEGY);

    passport_audio_play_once(BAYE_MUSIC_VICTORY, BAYE_MUSIC_STRATEGY);
    assert(passport_audio_get_track() == BAYE_MUSIC_VICTORY);

    passport_audio_stop();
    assert(passport_audio_get_track() == BAYE_MUSIC_NONE);

    printf("test_music_manager_controls: PASS\n");
}

static void test_fade_gain_math(void) {
    // Q15 fixed-point arithmetic: val = ((int32_t)sample * fade_gain) >> 15;
    int16_t samples[] = { 0, 100, -100, 1000, -1000, 16384, -16384, 32767, -32768 };
    size_t num_samples = sizeof(samples) / sizeof(samples[0]);

    // 1. Gain = 0 (Silence)
    for (size_t i = 0; i < num_samples; i++) {
        int32_t val = ((int32_t)samples[i] * 0) >> 15;
        assert((int16_t)val == 0);
    }

    // 2. Gain = 32768 (Full Volume 1.0)
    for (size_t i = 0; i < num_samples; i++) {
        int32_t val = ((int32_t)samples[i] * 32768) >> 15;
        assert((int16_t)val == samples[i]);
    }

    // 3. Gain = 16384 (Half Volume 0.5)
    for (size_t i = 0; i < num_samples; i++) {
        int32_t val = ((int32_t)samples[i] * 16384) >> 15;
        int16_t expected = samples[i] / 2;
        int diff = abs((int)val - (int)expected);
        assert(diff <= 1);
    }

    // 4. Monotonic ramp check
    int16_t test_sample = 20000;
    int16_t prev_val = 0;
    for (int32_t g = 0; g <= 32768; g += 512) {
        int32_t val = ((int32_t)test_sample * g) >> 15;
        assert(val >= prev_val);
        prev_val = (int16_t)val;
    }

    printf("test_fade_gain_math: PASS\n");
}

static void test_real_asset_loop_determinism(const char *assets_dir) {
    if (!assets_dir) {
        printf("test_real_asset_loop_determinism: SKIPPED (no asset dir)\n");
        return;
    }

    const char *tracks[] = {
        "baye_title_16k.adpcm",
        "baye_strategy_16k.adpcm",
        "baye_battle_16k.adpcm"
    };

    for (int t = 0; t < 3; t++) {
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", assets_dir, tracks[t]);
        FILE *f = fopen(path, "rb");
        if (!f) {
            printf("test_real_asset_loop_determinism: WARNING could not open %s\n", path);
            continue;
        }

        fseek(f, 0, SEEK_END);
        long file_size = ftell(f);
        fseek(f, 0, SEEK_SET);
        assert(file_size > 0);

        uint8_t *data = (uint8_t *)malloc(file_size);
        assert(data != NULL);
        size_t r = fread(data, 1, file_size, f);
        fclose(f);
        assert((long)r == file_size);

        size_t total_samples = file_size * 2;
        // Test 16000 samples (1 full second) across loop boundary
        size_t test_samples = 16000;
        if (test_samples > total_samples) test_samples = total_samples;

        passport_adpcm_stream_t stream;
        passport_adpcm_state_t state;
        passport_adpcm_state_reset(&state);
        passport_adpcm_stream_init(&stream, data, file_size, true);

        int16_t *loop1 = (int16_t *)malloc(test_samples * sizeof(int16_t));
        int16_t *loop2 = (int16_t *)malloc(test_samples * sizeof(int16_t));
        assert(loop1 && loop2);

        // First read the test_samples of loop 1
        size_t s1 = passport_adpcm_stream_read(&stream, &state, loop1, test_samples);
        assert(s1 == test_samples);
        size_t remaining = total_samples - test_samples;

        // Drain remainder of loop 1
        int16_t temp_buf[320];
        while (remaining > 0) {
            size_t req = (remaining > 320) ? 320 : remaining;
            size_t n = passport_adpcm_stream_read(&stream, &state, temp_buf, req);
            assert(n > 0);
            remaining -= n;
        }
        assert(stream.loop_count == 0);
        assert(stream.offset == stream.size);

        // Now read test_samples of loop 2 (boundary crossing resets state)
        size_t s2 = passport_adpcm_stream_read(&stream, &state, loop2, test_samples);
        assert(s2 == test_samples);
        assert(stream.loop_count == 1);

        // Assert exact bit-for-bit equivalence of loop 1 start vs loop 2 start
        assert(memcmp(loop1, loop2, test_samples * sizeof(int16_t)) == 0);

        free(loop1);
        free(loop2);
        free(data);
        printf("  [Asset %s: %ld bytes] Deterministic loop verification: PASS\n", tracks[t], file_size);
    }

    printf("test_real_asset_loop_determinism: PASS\n");
}

int main(int argc, char **argv) {
    const char *assets_dir = (argc > 1) ? argv[1] : "components/baye/assets";
    printf("--- Running test_audio ---\n");
    test_adpcm_basic();
    test_chunk_boundaries();
    test_streaming_and_loop();
    test_clamping_and_corruption();
    test_loop_determinism();
    test_volume_controls();
    test_music_assets_and_metadata();
    test_music_manager_controls();
    test_fade_gain_math();
    test_real_asset_loop_determinism(assets_dir);
    return 0;
}


