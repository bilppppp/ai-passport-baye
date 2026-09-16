#ifndef PASSPORT_FSYS_H
#define PASSPORT_FSYS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Maximum save file size supported in NVS / RAM buffer (16 KB is more than enough for ~4.5KB save)
#define PASSPORT_MAX_SAVE_SIZE (16 * 1024)

typedef struct gam_FILE {
    uint8_t   handle;
    uint8_t   openmode;      // 'r' or 'w'
    char      fname[32];
    uint32_t  curset;        // Current seek position
    uint32_t  flen;          // Total file length
    const uint8_t *ro_data;  // Read-only pointer for Flash-embedded files (dat.lib, font.bin)
    uint8_t  *rw_buf;        // Read-write buffer for save files
    bool      is_dirty;
} gam_FILE;

void passport_fsys_init(void);

gam_FILE *gam_fopen(const uint8_t *fname, uint8_t pmode);
int gam_fclose(gam_FILE *fp);
size_t gam_fread(void *ptr, size_t size, size_t count, gam_FILE *fp);
size_t gam_fwrite(const void *ptr, size_t size, size_t count, gam_FILE *fp);
int gam_fseek(gam_FILE *fp, long offset, int whence);
uint32_t gam_ftell(gam_FILE *fp);
uint8_t *gam_fload(uint8_t *bptr, uint32_t addr, gam_FILE *fhandle);
uint8_t *gam_freadall(gam_FILE *fhandle);

// Helper to set embedded ROM asset pointers
void passport_fsys_set_rom_assets(const uint8_t *dat_lib, size_t dat_lib_len,
                                 const uint8_t *font_bin, size_t font_bin_len);

#ifdef __cplusplus
}
#endif

#endif // PASSPORT_FSYS_H
