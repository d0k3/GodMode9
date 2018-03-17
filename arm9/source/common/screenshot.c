#include "common.h"
#include "ui.h"
#include "rtc.h"
#include "vff.h"


void CreateScreenshot() {
    const u32 snap_size = 54 + (SCREEN_SIZE_TOP * 2);
    const u8 bmp_header[54] = {
        0x42, 0x4D, 0x36, 0xCA, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x28, 0x00,
        0x00, 0x00, 0x90, 0x01, 0x00, 0x00, 0xE0, 0x01, 0x00, 0x00, 0x01, 0x00, 0x18, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0xCA, 0x08, 0x00, 0x12, 0x0B, 0x00, 0x00, 0x12, 0x0B, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    
    char filename[64];
    DsTime dstime;
    
    fvx_rmkdir(OUTPUT_PATH);
    get_dstime(&dstime);
    snprintf(filename, 64, OUTPUT_PATH "/snap_%02X%02X%02X%02X%02X%02X.bmp",
        dstime.bcd_Y, dstime.bcd_M, dstime.bcd_D,
        dstime.bcd_h, dstime.bcd_m, dstime.bcd_s);
    
    u8* buffer = (u8*) malloc(snap_size);
    if (!buffer) return;
    
    u8* buffer_b = buffer + 54;
    u8* buffer_t = buffer_b + (400 * 240 * 3);
    
    memset(buffer, 0x1F, snap_size); // gray background
    memcpy(buffer, bmp_header, 54);
    for (u32 x = 0; x < 400; x++)
        for (u32 y = 0; y < 240; y++)
            memcpy(buffer_t + (y*400 + x) * 3, TOP_SCREEN + (x*240 + y) * 3, 3);
    for (u32 x = 0; x < 320; x++)
        for (u32 y = 0; y < 240; y++)
            memcpy(buffer_b + (y*400 + x + 40) * 3, BOT_SCREEN + (x*240 + y) * 3, 3);
    fvx_qwrite(filename, buffer, 0, snap_size, NULL);
    
    // "snap effect"
    memcpy(buffer_b, BOT_SCREEN, SCREEN_SIZE_BOT);
    memcpy(buffer_t, TOP_SCREEN, SCREEN_SIZE_TOP);
    memset(BOT_SCREEN, 0, SCREEN_SIZE_BOT);
    memset(TOP_SCREEN, 0, SCREEN_SIZE_TOP);
    memcpy(BOT_SCREEN, buffer_b, SCREEN_SIZE_BOT);
    memcpy(TOP_SCREEN, buffer_t, SCREEN_SIZE_TOP);
    
    free(buffer);
}
