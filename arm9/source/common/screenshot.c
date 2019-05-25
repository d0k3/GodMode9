#include "common.h"
#include "ui.h"
#include "rtc.h"
#include "vff.h"
#include "png.h"

static void Screenshot_CvtAndTranspose(u8 *dest, u16 *fb, u32 w, u32 stride)
{
    for (u32 y = 0; y < SCREEN_HEIGHT; y++) {
        for (u32 x = 0; x < w; x++) {
            u8 r, g, b;
            u16 rgb_s = GetColor(fb, x, y);

            r = ((rgb_s >> 11) & 0x1F) << 3;
            g = ((rgb_s >> 5) & 0x3F) << 2;
            b = (rgb_s & 0x1F) << 3;

            *(dest++) = r;
            *(dest++) = g;
            *(dest++) = b;
        }

        dest += stride;
    }
}

void CreateScreenshot(void) {
    DsTime dstime;
    size_t png_size;
    u8 *png, *buffer;
    char filename[64];
    u32 snapbuf_size, snap_w, snap_h, bot_offset;

    snapbuf_size = (SCREEN_WIDTH_TOP * SCREEN_HEIGHT * 3) * 2;
    snap_w = SCREEN_WIDTH_TOP;
    snap_h = SCREEN_HEIGHT * 2;

    fvx_rmkdir(OUTPUT_PATH);
    get_dstime(&dstime);
    snprintf(filename, 64, OUTPUT_PATH "/snap_%02X%02X%02X%02X%02X%02X.png",
        dstime.bcd_Y, dstime.bcd_M, dstime.bcd_D,
        dstime.bcd_h, dstime.bcd_m, dstime.bcd_s);
    filename[63] = '\0';

    buffer = malloc(snapbuf_size);
    if (!buffer) return;

    memset(buffer, 0x1F, snapbuf_size); // gray background

    bot_offset = ((400 * SCREEN_HEIGHT) + 40) * 3;

    Screenshot_CvtAndTranspose(buffer, TOP_SCREEN, SCREEN_WIDTH_TOP, 0);
    Screenshot_CvtAndTranspose(buffer + bot_offset, BOT_SCREEN, SCREEN_WIDTH_BOT, 80 * 3);

    png = PNG_Compress(buffer, snap_w, snap_h, &png_size);

    if (png && png_size) {
        fvx_qwrite(filename, png, 0, png_size, NULL);

        // "snap effect"
        /*memcpy(buffer_b, BOT_SCREEN, SCREEN_SIZE_BOT);
        memcpy(buffer_t, TOP_SCREEN, SCREEN_SIZE_TOP);
        memset(BOT_SCREEN, 0, SCREEN_SIZE_BOT);
        memset(TOP_SCREEN, 0, SCREEN_SIZE_TOP);
        memcpy(BOT_SCREEN, buffer_b, SCREEN_SIZE_BOT);
        memcpy(TOP_SCREEN, buffer_t, SCREEN_SIZE_TOP);*/
    }
    // what to do on error...?

    free(buffer);
    free(png);
}
