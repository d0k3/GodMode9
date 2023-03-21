#include "common.h"
#include "ui.h"
#include "rtc.h"
#include "vff.h"
#include "png.h"

static void Screenshot_Transpose(u16 *dest, const u16 *fb, u32 w, u32 stride)
{
    for (u32 y = 0; y < SCREEN_HEIGHT; y++) {
        for (u32 x = 0; x < w; x++)
            *(dest++) = GetColor(fb, x, y);
        dest += stride;
    }
}

void CreateScreenshot(void) {
    u8 *png;
    u16 *buffer;
    DsTime dstime;
    size_t png_size;
    char filename[64];
    u32 snapbuf_size, snap_w, snap_h, bot_offset;

    snapbuf_size = (SCREEN_WIDTH_TOP * SCREEN_HEIGHT * BYTES_PER_PIXEL) * 2;
    snap_w = SCREEN_WIDTH_TOP;
    snap_h = SCREEN_HEIGHT * 2;

    fvx_rmkdir(OUTPUT_PATH);
    get_dstime(&dstime);
    snprintf(filename, sizeof(filename), OUTPUT_PATH "/snap_%02X%02X%02X%02X%02X%02X.png",
        dstime.bcd_Y, dstime.bcd_M, dstime.bcd_D,
        dstime.bcd_h, dstime.bcd_m, dstime.bcd_s);
    filename[63] = '\0';

    buffer = malloc(snapbuf_size);
    if (!buffer) return;

    for (unsigned i = snapbuf_size/4; i < snapbuf_size/2; i++)
        buffer[i] = RGB(0x1F, 0x1F, 0x1F); // gray background

    bot_offset = (SCREEN_WIDTH_TOP * SCREEN_HEIGHT) + 40;

    Screenshot_Transpose(buffer, TOP_SCREEN, SCREEN_WIDTH_TOP, 0);
    Screenshot_Transpose(buffer + bot_offset, BOT_SCREEN, SCREEN_WIDTH_BOT, 80);

    png = PNG_Compress(buffer, snap_w, snap_h, &png_size);

    if (png && png_size) {
        u16 *buffer_top = buffer, *buffer_bottom = buffer + bot_offset;

        // "snap effect"
        memcpy(buffer_bottom, BOT_SCREEN, SCREEN_SIZE_BOT);
        memcpy(buffer_top, TOP_SCREEN, SCREEN_SIZE_TOP);
        memset(BOT_SCREEN, 0, SCREEN_SIZE_BOT);
        memset(TOP_SCREEN, 0, SCREEN_SIZE_TOP);

        fvx_qwrite(filename, png, 0, png_size, NULL);

        memcpy(BOT_SCREEN, buffer_bottom, SCREEN_SIZE_BOT);
        memcpy(TOP_SCREEN, buffer_top, SCREEN_SIZE_TOP);
    }
    // what to do on error...?

    free(buffer);
    free(png);
}
