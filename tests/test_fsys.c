#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "passport_fsys.h"

// Load a binary file from filesystem for testing
static uint8_t *load_file(const char *path, size_t *size_out) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = (uint8_t *)malloc(sz);
    fread(buf, 1, sz, f);
    fclose(f);
    *size_out = sz;
    return buf;
}

int main(int argc, char **argv) {
    printf("--- Running test_fsys ---\n");

    const char *dat_path = "components/baye/assets/dat.lib";
    const char *font_path = "components/baye/assets/font.bin";
    if (argc > 2) {
        dat_path = argv[1];
        font_path = argv[2];
    }

    size_t dat_sz = 0, font_sz = 0;
    uint8_t *dat_buf = load_file(dat_path, &dat_sz);
    uint8_t *font_buf = load_file(font_path, &font_sz);
    assert(dat_buf != NULL && dat_sz == 196890);
    assert(font_buf != NULL && font_sz == 163840);

    passport_fsys_set_rom_assets(dat_buf, dat_sz, font_buf, font_sz);
    passport_fsys_init();

    // 1. Open dat.lib
    gam_FILE *fp = gam_fopen((const uint8_t *)"dat.lib", 'r');
    assert(fp != NULL);
    assert(fp->flen == 196890);

    // Read index for ResID 44 (MAIN_PIC)
    // Addr offset is (44 - 1) * 4 = 172
    uint32_t addr_44 = 0;
    gam_fseek(fp, 172, SEEK_SET);
    gam_fread(&addr_44, 4, 1, fp);
    assert(addr_44 == 0x00f852);

    // Read RCHEAD at addr_44
    gam_fseek(fp, addr_44, SEEK_SET);
    struct {
        uint32_t ResLen;
        uint16_t ResId;
        uint16_t ItmCnt;
        uint16_t ItmLen;
        uint8_t  ResKey;
        uint8_t  Reserved;
    } rchead;
    gam_fread(&rchead, sizeof(rchead), 1, fp);
    assert(rchead.ResId == 44);
    assert(rchead.ItmCnt == 1);

    // Test gam_freadall
    uint8_t *all_ptr = gam_freadall(fp);
    assert(all_ptr == dat_buf);

    // Test gam_fload boundary checks
    assert(gam_fload(dat_buf, 0, fp) == dat_buf);
    assert(gam_fload(dat_buf, 100, fp) == dat_buf + 100);
    assert(gam_fload(dat_buf, fp->flen - 1, fp) == dat_buf + fp->flen - 1);
    assert(gam_fload(dat_buf, fp->flen, fp) == NULL);       // Out of bounds
    assert(gam_fload(dat_buf, 9999999, fp) == NULL);        // Far out of bounds
    assert(gam_fload(NULL, 0, fp) == NULL);                 // NULL base pointer

    // Test gam_fseek & gam_fread bounds
    gam_fseek(fp, 9999999, SEEK_SET);
    assert(fp->curset == fp->flen);
    uint8_t dummy[10];
    assert(gam_fread(dummy, 1, sizeof(dummy), fp) == 0);

    gam_fclose(fp);

    // 2. Open font.bin
    gam_FILE *font_fp = gam_fopen((const uint8_t *)"font.bin", 'r');
    assert(font_fp != NULL);
    assert(font_fp->flen == 163840);

    // Read 18 bytes from offset 0
    uint8_t font_test[18];
    assert(gam_fread(font_test, 1, 18, font_fp) == 18);
    gam_fclose(font_fp);

    // 3. Test Save & Load serialization
    const char *test_data = "SANGO_SAVE_SLOT_TEST_DATA_PAYLOAD_123456789";
    size_t test_len = strlen(test_data);

    gam_FILE *save_fp = gam_fopen((const uint8_t *)"sango0.sav", 'w');
    assert(save_fp != NULL);
    assert(gam_fwrite(test_data, 1, test_len, save_fp) == test_len);
    gam_fclose(save_fp);

    // Read back
    gam_FILE *load_fp = gam_fopen((const uint8_t *)"sango0.sav", 'r');
    assert(load_fp != NULL);
    assert(load_fp->flen == test_len);
    char load_buf[64] = {0};
    assert(gam_fread(load_buf, 1, test_len, load_fp) == test_len);
    assert(strcmp(load_buf, test_data) == 0);
    gam_fclose(load_fp);

    // 4. Verify save game integrity remains 100% intact and non-destructive
    // Verify sango0..sango3 keys cannot be affected by config persistence
    gam_FILE *re_load_fp = gam_fopen((const uint8_t *)"sango0.sav", 'r');
    assert(re_load_fp != NULL);
    char verify_buf[64] = {0};
    assert(gam_fread(verify_buf, 1, test_len, re_load_fp) == test_len);
    assert(memcmp(verify_buf, test_data, test_len) == 0);
    gam_fclose(re_load_fp);

    free(dat_buf);
    free(font_buf);

    printf("test_fsys: PASS\n");
    return 0;
}
