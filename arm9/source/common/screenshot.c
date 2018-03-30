#include "common.h"
#include "ui.h"
#include "rtc.h"
#include "vff.h"
#include "png.h"


void CreateScreenshot() {
    const u32 snap_size = SCREEN_SIZE_TOP * 2, snap_width = SCREEN_WIDTH_TOP, snap_height = SCREEN_HEIGHT * 2;
    u8 *png_data = NULL;
    size_t png_size;

    char filename[64];
    DsTime dstime;

    fvx_rmkdir(OUTPUT_PATH);
    get_dstime(&dstime);
    snprintf(filename, 64, OUTPUT_PATH "/snap_%02X%02X%02X%02X%02X%02X.png",
        dstime.bcd_Y, dstime.bcd_M, dstime.bcd_D,
        dstime.bcd_h, dstime.bcd_m, dstime.bcd_s);

    u8* buffer = (u8*) malloc(snap_size);
    if (!buffer) return;

    u8* buffer_t = buffer;
    u8* buffer_b = buffer + SCREEN_SIZE_TOP;

    memset(buffer, 0x1F, snap_size); // gray background

    for (u32 x = 0; x < 400; x++) {
        for (u32 y = 0; y < 240; y++) {
            buffer_t[(y * SCREEN_WIDTH_TOP + x) * 3 + 0] = *(TOP_SCREEN + (((x * 240) + (239 - y)) * 3) + 2);
            buffer_t[(y * SCREEN_WIDTH_TOP + x) * 3 + 1] = *(TOP_SCREEN + (((x * 240) + (239 - y)) * 3) + 1);
            buffer_t[(y * SCREEN_WIDTH_TOP + x) * 3 + 2] = *(TOP_SCREEN + (((x * 240) + (239 - y)) * 3) + 0);
        }
    }

    for (u32 x = 0; x < 320; x++) {
        for (u32 y = 0; y < 240; y++) {
            buffer_b[(y * SCREEN_WIDTH_TOP + x + 40) * 3 + 0] = *(BOT_SCREEN + (((x * 240) + (239 - y)) * 3) + 2);
            buffer_b[(y * SCREEN_WIDTH_TOP + x + 40) * 3 + 1] = *(BOT_SCREEN + (((x * 240) + (239 - y)) * 3) + 1);
            buffer_b[(y * SCREEN_WIDTH_TOP + x + 40) * 3 + 2] = *(BOT_SCREEN + (((x * 240) + (239 - y)) * 3) + 0);
        }
    }

    png_data = PNG_Compress(buffer, snap_width, snap_height, &png_size);

    if (png_data && png_size) {
        fvx_qwrite(filename, png_data, 0, png_size, NULL);

        // "snap effect"
        memcpy(buffer_b, BOT_SCREEN, SCREEN_SIZE_BOT);
        memcpy(buffer_t, TOP_SCREEN, SCREEN_SIZE_TOP);
        memset(BOT_SCREEN, 0, SCREEN_SIZE_BOT);
        memset(TOP_SCREEN, 0, SCREEN_SIZE_TOP);
        memcpy(BOT_SCREEN, buffer_b, SCREEN_SIZE_BOT);
        memcpy(TOP_SCREEN, buffer_t, SCREEN_SIZE_TOP);
    }
    // what to do on error...?

    free(buffer);
    if (png_data) free(png_data);
}
