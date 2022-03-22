#include "paint9.h"
#include "vram0.h"
#include "png.h"
#include "hid.h"
#include "ui.h"
#include "language.h"


#define PAINT9_BRUSH_SIZE       15  // don't change!
#define PAINT9_N_BRUSHES        6   // don't change!
#define PAINT9_COLSEL_WIDTH     16
#define PAINT9_COLSEL_HEIGHT    220 // don't change!

enum P9BOXES {
    P9BOX_CANVAS = 1,
    P9BOX_PICKER = 2,
    P9BOX_BRUSH_N = 3
};

static const TouchBox paint9_boxes[] = {
    { 30, 0, SCREEN_WIDTH_BOT - (2*30), SCREEN_HEIGHT, P9BOX_CANVAS },
    { SCREEN_WIDTH_BOT - PAINT9_COLSEL_WIDTH - 7, 10, PAINT9_COLSEL_WIDTH, PAINT9_COLSEL_HEIGHT, P9BOX_PICKER },
    { 7, 10 + ((PAINT9_BRUSH_SIZE+3)*0), PAINT9_BRUSH_SIZE, PAINT9_BRUSH_SIZE, P9BOX_BRUSH_N+0 },
    { 7, 10 + ((PAINT9_BRUSH_SIZE+3)*1), PAINT9_BRUSH_SIZE, PAINT9_BRUSH_SIZE, P9BOX_BRUSH_N+1 },
    { 7, 10 + ((PAINT9_BRUSH_SIZE+3)*2), PAINT9_BRUSH_SIZE, PAINT9_BRUSH_SIZE, P9BOX_BRUSH_N+2 },
    { 7, 10 + ((PAINT9_BRUSH_SIZE+3)*3), PAINT9_BRUSH_SIZE, PAINT9_BRUSH_SIZE, P9BOX_BRUSH_N+3 },
    { 7, 10 + ((PAINT9_BRUSH_SIZE+3)*4), PAINT9_BRUSH_SIZE, PAINT9_BRUSH_SIZE, P9BOX_BRUSH_N+4 },
    { 7, 10 + ((PAINT9_BRUSH_SIZE+3)*5), PAINT9_BRUSH_SIZE, PAINT9_BRUSH_SIZE, P9BOX_BRUSH_N+5 }
};

static const u16 color_picker_tmp[] = { // count must be a divisor of PAINT9_COLSEL_HEIGHT
    RGB(0xE6, 0x19, 0x4B), RGB(0x3C, 0xB4, 0x4B), RGB(0xFF, 0xE1, 0x19), RGB(0x43, 0x63, 0xD8),
    RGB(0xF5, 0x82, 0x31), RGB(0x91, 0x1E, 0xB4), RGB(0x46, 0xF0, 0xF0), RGB(0xF0, 0x32, 0xE6),
    RGB(0xBC, 0xF6, 0x0C), RGB(0xFA, 0xBE, 0xBE), RGB(0x00, 0x80, 0x80), RGB(0xE6, 0xBE, 0xFF),
    RGB(0x9A, 0x63, 0x24), RGB(0xFF, 0xFA, 0xC8), RGB(0x80, 0x00, 0x00), RGB(0xAA, 0xFF, 0xC3),
    RGB(0x80, 0x80, 0x00), RGB(0xFF, 0xD8, 0xB1), RGB(0x00, 0x00, 0x75), RGB(0x80, 0x80, 0x80),
    RGB(0xFF, 0xFF, 0xFF), RGB(0x00, 0x00, 0x00)
};

static const u16 brushes_tmp[PAINT9_N_BRUSHES][PAINT9_BRUSH_SIZE] = {
    { 0x0FE0, 0x1FF0, 0x3FF8, 0x7FFC, 0xFFFE, 0xFFFE, 0xFFFE,0xFFFE,
        0xFFFE, 0xFFFE, 0xFFFE, 0x7FFC, 0x3FF8, 0x1FF0, 0x0FE0 },
    { 0x0000, 0x0000, 0x07C0, 0x0FE0, 0x1FF0, 0x3FF8, 0x3FF8, 0x3FF8,
        0x3FF8, 0x3FF8, 0x1FF0, 0x0FE0, 0x07C0, 0x0000, 0x0000 },
    { 0x0000, 0x0000, 0x0000, 0x0000, 0x0380, 0x07C0, 0x0FE0, 0x0FE0,
        0x0FE0, 0x07C0, 0x0380, 0x0000, 0x0000, 0x0000, 0x0000 },
    { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0380, 0x07C0, 0x07C0,
        0x07C0, 0x0380, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 },
    { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0100, 0x0380,
        0x0100, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 },
    { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0100,
        0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 }
};


void Paint9_DrawBrush(u16 px, u16 py, u32 color_fg, u32 color_bg, u32 id) {
    const u16* brush = brushes_tmp[id];

    // fix px / py
    s16 pxf = px - (PAINT9_BRUSH_SIZE/2);
    s16 pyf = py - (PAINT9_BRUSH_SIZE/2);

    // draw brush, pixel by pixel
    for (s16 y = 0; y < PAINT9_BRUSH_SIZE; y++) {
        s16 pyc = pyf + y;
        if ((pyc < 0) || (pyc >= SCREEN_HEIGHT)) continue;

        for (s16 x = 0; x < PAINT9_BRUSH_SIZE; x++) {
            s16 pxc = pxf + x;
            if ((pxc < 0) || (pxc >= SCREEN_WIDTH_BOT)) continue;

            if ((brush[y]>>(PAINT9_BRUSH_SIZE-x))&0x1) {
                DrawPixel(BOT_SCREEN, pxc, pyc, color_fg);
            } else if (color_bg != COLOR_TRANSPARENT) {
                DrawPixel(BOT_SCREEN, pxc, pyc, color_bg);
            }
        }
    }
}

// Bresenham algorithm, see here:
// https://rosettacode.org/wiki/Bitmap/Bresenham%27s_line_algorithm#C
void Paint9_DrawLine(u16 px0, u16 py0, u16 px1, u16 py1, u32 color_fg, u32 id) {
    const int dx = abs((s16) px1-px0), sx = (px0 < px1) ? 1 : -1;
    const int dy = abs((s16) py1-py0), sy = (py0 < py1) ? 1 : -1;
    int err = ((dx > dy) ? dx : -dy) / 2;

    while (true) {
        Paint9_DrawBrush(px0, py0, color_fg, COLOR_TRANSPARENT, id);
        if ((px0 == px1) && (py0 == py1)) break;
        int e2 = err;
        if (e2 >-dx) { err -= dy; px0 += sx; }
        if (e2 < dy) { err += dx; py0 += sy; }
    }
}

u32 Paint9(void) {
    static u32 brush_bg = RGB(0x20, 0x20, 0x20);
    static u32 outline_bg = RGB(0x18, 0x18, 0x18);
    u16 x_cb = paint9_boxes[2].x + (PAINT9_BRUSH_SIZE/2) + 1;
    u16 y_cb = SCREEN_HEIGHT - 10 - (PAINT9_BRUSH_SIZE/2) - 1;

    u16 color = *color_picker_tmp;
    u32 brush_id = 0;

    // clear screens, draw logo
    const char* snapstr = STR_USE_L_R_TO_SAVE;
    u64 logo_size;
    u8* logo = FindVTarFileInfo(VRAM0_EASTER_BIN, &logo_size);
    ClearScreenF(true, true, COLOR_STD_BG);
    if (logo) {
        u32 logo_width, logo_height;
        u16* bitmap = PNG_Decompress(logo, logo_size, &logo_width, &logo_height);
        if (bitmap) {
            DrawBitmap(TOP_SCREEN, -1, -1, logo_width, logo_height, bitmap);
            free(bitmap);
        }
    } else DrawStringF(TOP_SCREEN, 10, 10, COLOR_STD_FONT, COLOR_TRANSPARENT, STR_EASTER_NOT_FOUND, VRAM0_EASTER_BIN);
    DrawStringF(TOP_SCREEN, SCREEN_WIDTH_TOP - 10 - GetDrawStringWidth(snapstr),
        SCREEN_HEIGHT - 10 - GetDrawStringHeight(snapstr), COLOR_STD_FONT, COLOR_TRANSPARENT, "%s", snapstr);

    // outline canvas
    DrawRectangle(BOT_SCREEN, 0, 0, 30, SCREEN_HEIGHT, outline_bg);
    DrawRectangle(BOT_SCREEN, SCREEN_WIDTH_BOT - 30, 0, 30, SCREEN_HEIGHT, outline_bg);

    // draw color picker
    u32 pick_x = paint9_boxes[1].x;
    u32 pick_y = paint9_boxes[1].y;
    const u32 palbox_height = PAINT9_COLSEL_HEIGHT / countof(color_picker_tmp);
    for (u32 y = 0; y < PAINT9_COLSEL_HEIGHT; y++) {
        const u16* color = color_picker_tmp + (y / palbox_height);
        for (u32 x = 0; x < PAINT9_COLSEL_WIDTH; x++)
            DrawPixel(BOT_SCREEN, pick_x + x, pick_y + y, *color);
    }

    // draw brushes
    for (u32 i = 0; i < PAINT9_N_BRUSHES; i++) {
        u32 color_fg = COLOR_STD_FONT;
        u16 x = paint9_boxes[2+i].x + (PAINT9_BRUSH_SIZE/2) + 1;
        u16 y = paint9_boxes[2+i].y + (PAINT9_BRUSH_SIZE/2) + 1;
        Paint9_DrawBrush(x, y, color_fg, brush_bg, i);
    }

    // Paint9 main loop
    while (1) {
        Paint9_DrawBrush(x_cb, y_cb, color, brush_bg, brush_id);
        if (InputWait(0) & BUTTON_B) break;

        u16 tx, ty;
        u32 tb_id;
        u16 tx_prev = 0;
        u16 ty_prev = 0;
        u32 tb_id_prev = 0;
        while (HID_ReadTouchState(&tx, &ty)) {
            TouchBoxGet(&tb_id, tx, ty, paint9_boxes, 8);
            if (tb_id == P9BOX_CANVAS) {
                if (tb_id_prev == P9BOX_CANVAS)
                    Paint9_DrawLine(tx_prev, ty_prev, tx, ty, color, brush_id);
                else Paint9_DrawBrush(tx, ty, color, COLOR_TRANSPARENT, brush_id);
            } else if (tb_id == P9BOX_PICKER) {
                color = GetColor(BOT_SCREEN, tx, ty);
            } else if (tb_id >= P9BOX_BRUSH_N) {
                brush_id = tb_id - P9BOX_BRUSH_N;
            }
            if ((tb_id == P9BOX_PICKER) || (tb_id >= P9BOX_BRUSH_N))
                Paint9_DrawBrush(x_cb, y_cb, color, brush_bg, brush_id);
            tb_id_prev = tb_id;
            tx_prev = tx;
            ty_prev = ty;
        }
    }

    ClearScreenF(true, true, COLOR_STD_BG);
    return 0;
}
