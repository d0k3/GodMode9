// Copyright 2013 Normmatt / 2018 d0k3
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include "qrcodegen.h"
#include "vram0.h"
#include "ui.h"
#include "rtc.h"
#include "timer.h"
#include "power.h"
#include "hid.h"

#define STRBUF_SIZE 512 // maximum size of the string buffer
#define FONT_MAX_WIDTH 8
#define FONT_MAX_HEIGHT 10

static u32 font_width = 0;
static u32 font_height = 0;
static u32 line_height = 0;
static u8 font_bin[FONT_MAX_HEIGHT * 256];


u8* GetFontFromPbm(const void* pbm, const u32 pbm_size, u32* w, u32* h) {
    char* hdr = (char*) pbm;
    u32 hdr_max_size = min(512, pbm_size);
    u32 pbm_w = 0;
    u32 pbm_h = 0;
    
    // minimum size
    if (hdr_max_size < 7) return NULL;
    
    // check header magic, then skip over
    if (strncmp(hdr, "P4\n", 3) != 0) return NULL;
    
    // skip any comments
    u32 p = 3;
    while (hdr[p] == '#') {
        while (hdr[p++] != '\n') {
            if (p >= hdr_max_size) return NULL;
        }
    }
    
    // parse width
    while ((hdr[p] >= '0') && (hdr[p] <= '9')) {
        if (p >= hdr_max_size) return NULL;
        pbm_w *= 10;
        pbm_w += hdr[p++] - '0';
    }
    
    // whitespace
    if ((hdr[p++] != ' ') || (p >= hdr_max_size))
        return NULL;
    
    // parse height
    while ((hdr[p] >= '0') && (hdr[p] <= '9')) {
        if (p >= hdr_max_size) return NULL;
        pbm_h *= 10;
        pbm_h += hdr[p++] - '0';
    }
    
    // line break
    if ((hdr[p++] != '\n') || (p >= hdr_max_size))
        return NULL;
    
    // check sizes
    if (pbm_w <= 8) { // 1x256 format
        if ((pbm_w > FONT_MAX_WIDTH) || (pbm_h % 256) ||
            ((pbm_h / 256) > FONT_MAX_HEIGHT) ||
            (pbm_h != (pbm_size - p)))
            return NULL;
    } else { // 16x16 format
        if ((pbm_w % 16) || (pbm_h % 16) ||
            ((pbm_w / 16) > FONT_MAX_WIDTH) ||
            ((pbm_h / 16) > FONT_MAX_HEIGHT) ||
            ((pbm_h * pbm_w / 8) != (pbm_size - p)))
            return NULL;
    }
    
    // all good
    if (w) *w = pbm_w;
    if (h) *h = pbm_h;
    return (u8*) pbm + p;
}

// sets the font from a given PBM
// if no PBM is given, the PBM is fetched from the default VRAM0 location
bool SetFontFromPbm(const void* pbm, u32 pbm_size) {
    u32 w, h;
    u8* ptr = NULL;
    
    if (!pbm) {
        u64 pbm_size64 = 0;
        pbm = FindVTarFileInfo(VRAM0_FONT_PBM, &pbm_size64);
        pbm_size = (u32) pbm_size64;
    }
    
    if (pbm)
        ptr = GetFontFromPbm(pbm, pbm_size, &w, &h);
    
    if (!ptr) {
        return false;
    } else if (w > 8) {
        font_width = w / 16;
        font_height = h / 16;
        memset(font_bin, 0x00, w * h / 8);
        
        for (u32 cy = 0; cy < 16; cy++) {
            for (u32 row = 0; row < font_height; row++) {
                for (u32 cx = 0; cx < 16; cx++) {
                    u32 bp0 = (cx * font_width) >> 3;
                    u32 bm0 = (cx * font_width) % 8;
                    u8 byte = ((ptr[bp0] << bm0) | (ptr[bp0+1] >> (8 - bm0))) & (0xFF << (8 - font_width));
                    font_bin[(((cy << 4) + cx) * font_height) + row] = byte;
                }
                ptr += font_width << 1;
            }
        }
    } else {
        font_width = w;
        font_height = h / 256;
        memcpy(font_bin, ptr, h);
    }
    
    line_height = min(10, font_height + 2);
    return true;
}

void ClearScreen(u8* screen, int color)
{
    int width = (screen == TOP_SCREEN) ? SCREEN_WIDTH_TOP : SCREEN_WIDTH_BOT;
    if (color == COLOR_TRANSPARENT) color = COLOR_BLACK;
    if (!color) memset(screen, 0x00, (width * SCREEN_HEIGHT * 3));
    else for (int i = 0; i < (width * SCREEN_HEIGHT); i++) {
        *(screen++) = color >> 16;  // B
        *(screen++) = color >> 8;   // G
        *(screen++) = color & 0xFF; // R
    }
}

void ClearScreenF(bool clear_main, bool clear_alt, int color)
{
    if (clear_main) ClearScreen(MAIN_SCREEN, color);
    if (clear_alt) ClearScreen(ALT_SCREEN, color);
}

void DrawRectangle(u8* screen, int x, int y, int width, int height, int color)
{
    for (int yy = 0; yy < height; yy++) {
        int xDisplacement = (x * BYTES_PER_PIXEL * SCREEN_HEIGHT);
        int yDisplacement = ((SCREEN_HEIGHT - (y + yy) - 1) * BYTES_PER_PIXEL);
        u8* screenPos = screen + xDisplacement + yDisplacement;
        for (int xx = width - 1; xx >= 0; xx--) {
            *(screenPos + 0) = color >> 16;  // B
            *(screenPos + 1) = color >> 8;   // G
            *(screenPos + 2) = color & 0xFF; // R
            screenPos += BYTES_PER_PIXEL * SCREEN_HEIGHT;
        }
    }
}

void DrawBitmap(u8* screen, int x, int y, int w, int h, u8* bitmap)
{
    // on negative values: center the bitmap
    if (x < 0) x = (SCREEN_WIDTH(screen) - w) >> 1;
    if (y < 0) y = (SCREEN_HEIGHT - h) >> 1;
    
    // bug out on too big bitmaps / too large dimensions
    if ((x < 0) || (y < 0) || (w > SCREEN_WIDTH(screen)) || (h > SCREEN_HEIGHT))
        return;
    
    u8* bitmapPos = bitmap;
    for (int yy = 0; yy < h; yy++) {
        int xDisplacement = (x * BYTES_PER_PIXEL * SCREEN_HEIGHT);
        int yDisplacement = ((SCREEN_HEIGHT - (y + yy) - 1) * BYTES_PER_PIXEL);
        u8* screenPos = screen + xDisplacement + yDisplacement;
        for (int xx = 0; xx < w; xx++) {
            memcpy(screenPos, bitmapPos, BYTES_PER_PIXEL);
            bitmapPos += BYTES_PER_PIXEL;
            screenPos += BYTES_PER_PIXEL * SCREEN_HEIGHT;
        }
    }
}

void DrawQrCode(u8* screen, u8* qrcode)
{
    const u32 size_qr = qrcodegen_getSize(qrcode);
    u32 size_qr_s = size_qr;
    u32 size_canvas = size_qr + 8;
    
    // handle scaling
    u32 scale = 1;
    for (; size_canvas * (scale+1) < SCREEN_HEIGHT; scale++);
    size_qr_s *= scale;
    size_canvas *= scale;
    
    // clear screen, draw the canvas
    u32 x_canvas = (SCREEN_WIDTH(screen) - size_canvas) / 2;
    u32 y_canvas = (SCREEN_HEIGHT - size_canvas) / 2;
    ClearScreen(screen, COLOR_STD_BG);
    DrawRectangle(screen, x_canvas, y_canvas, size_canvas, size_canvas, COLOR_WHITE);
    
    // draw the QR code
    u32 x_qr = (SCREEN_WIDTH(screen) - size_qr_s) / 2;
    u32 y_qr = (SCREEN_HEIGHT - size_qr_s) / 2;
    int xDisplacement = (x_qr * BYTES_PER_PIXEL * SCREEN_HEIGHT);
    for (u32 y = 0; y < size_qr_s; y++) {
        int yDisplacement = ((SCREEN_HEIGHT - (y_qr + y) - 1) * BYTES_PER_PIXEL);
        u8* screenPos = screen + xDisplacement + yDisplacement;
        for (u32 x = 0; x < size_qr_s; x++) {
            u8 c = qrcodegen_getModule(qrcode, x/scale, y/scale) ? 0x00 : 0xFF;
            memset(screenPos, c, BYTES_PER_PIXEL);
            screenPos += BYTES_PER_PIXEL * SCREEN_HEIGHT;
        }
    }
}

void DrawCharacter(u8* screen, int character, int x, int y, int color, int bgcolor)
{
    for (int yy = 0; yy < (int) font_height; yy++) {
        int xDisplacement = (x * BYTES_PER_PIXEL * SCREEN_HEIGHT);
        int yDisplacement = ((SCREEN_HEIGHT - (y + yy) - 1) * BYTES_PER_PIXEL);
        u8* screenPos = screen + xDisplacement + yDisplacement;

        u8 charPos = font_bin[character * font_height + yy];
        for (int xx = 7; xx >= (8 - (int) font_width); xx--) {
            if ((charPos >> xx) & 1) {
                *(screenPos + 0) = color >> 16;  // B
                *(screenPos + 1) = color >> 8;   // G
                *(screenPos + 2) = color & 0xFF; // R
            } else if (bgcolor != COLOR_TRANSPARENT) {
                *(screenPos + 0) = bgcolor >> 16;  // B
                *(screenPos + 1) = bgcolor >> 8;   // G
                *(screenPos + 2) = bgcolor & 0xFF; // R
            }
            screenPos += BYTES_PER_PIXEL * SCREEN_HEIGHT;
        }
    }
}

void DrawString(u8* screen, const char *str, int x, int y, int color, int bgcolor, bool fix_utf8)
{
    size_t max_len = (((screen == TOP_SCREEN) ? SCREEN_WIDTH_TOP : SCREEN_WIDTH_BOT) - x) / font_width;
    size_t len = (strlen(str) > max_len) ? max_len : strlen(str);
    for (size_t i = 0; i < len; i++) {
        char c = (char) (fix_utf8 && str[i] >= 0x80) ? '?' : str[i]; 
        DrawCharacter(screen, c, x + i * font_width, y, color, bgcolor);
    }
}

void DrawStringF(u8* screen, int x, int y, int color, int bgcolor, const char *format, ...)
{
    char str[STRBUF_SIZE] = { 0 };
    va_list va;
    va_start(va, format);
    vsnprintf(str, STRBUF_SIZE, format, va);
    va_end(va);

    for (char* text = strtok(str, "\n"); text != NULL; text = strtok(NULL, "\n"), y += line_height)
        DrawString(screen, text, x, y, color, bgcolor, true);
}

void DrawStringCenter(u8* screen, int color, int bgcolor, const char *format, ...)
{
    char str[STRBUF_SIZE] = { 0 };
    va_list va;
    va_start(va, format);
    vsnprintf(str, STRBUF_SIZE, format, va);
    va_end(va);
    
    u32 w = GetDrawStringWidth(str);
    u32 h = GetDrawStringHeight(str);
    int x = (w >= SCREEN_WIDTH(screen)) ? 0 : (SCREEN_WIDTH(screen) - w) >> 1;
    int y = (h >= SCREEN_HEIGHT) ? 0 : (SCREEN_HEIGHT - h) >> 1;
    
    DrawStringF(screen, x, y, color, bgcolor, str);
}

u32 GetDrawStringHeight(const char* str) {
    u32 height = font_height;
    for (char* lf = strchr(str, '\n'); (lf != NULL); lf = strchr(lf + 1, '\n'))
        height += line_height;
    return height;
}

u32 GetDrawStringWidth(const char* str) {
    u32 width = 0;
    char* old_lf = (char*) str;
    char* str_end = (char*) str + strnlen(str, STRBUF_SIZE);
    for (char* lf = strchr(str, '\n'); lf != NULL; lf = strchr(lf + 1, '\n')) {
        if ((u32) (lf - old_lf) > width) width = lf - old_lf;
        old_lf = lf;
    }
    if ((u32) (str_end - old_lf) > width)
        width = str_end - old_lf;
    width *= font_width;
    return width;
}

u32 GetFontWidth(void) {
    return font_width;
}

u32 GetFontHeight(void) {
    return font_height;
}

void WordWrapString(char* str, int llen) {
    char* last_brk = str - 1;
    char* last_spc = str - 1;
    if (!llen) llen = (SCREEN_WIDTH_MAIN / font_width);
    for (char* str_ptr = str;; str_ptr++) {
        if (!*str_ptr || (*str_ptr == ' ')) { // on space or string_end
            if (str_ptr - last_brk > llen) { // if maximum line lenght is exceeded
                if (last_spc > last_brk) { // put a line_brk at the last space
                    *last_spc = '\n';
                    last_brk = last_spc;
                    last_spc = str_ptr;
                } else if (*str_ptr) { // if we have no applicable space
                    *str_ptr = '\n';
                    last_brk = str_ptr;
                }
            } else if (*str_ptr) last_spc = str_ptr;
        } else if (*str_ptr == '\n') last_brk = str_ptr;
        if (!*str_ptr) break;
    }
}

void ResizeString(char* dest, const char* orig, int nsize, int tpos, bool align_right) {
    int osize = strnlen(orig, 256);
    if (nsize < osize) {
        TruncateString(dest, orig, nsize, tpos);
    } else if (!align_right) {
        snprintf(dest, nsize + 1, "%-*.*s", nsize, nsize, orig);
    } else {
        snprintf(dest, nsize + 1, "%*.*s", nsize, nsize, orig);
    }
}

void TruncateString(char* dest, const char* orig, int nsize, int tpos) {
    int osize = strnlen(orig, 256);
    if (nsize < 0) {
        return;
    } else if (nsize <= 3) {
        snprintf(dest, nsize, orig);
    } else if (nsize >= osize) {
        snprintf(dest, nsize + 1, orig);
    } else {
        if (tpos + 3 > nsize) tpos = nsize - 3;
        snprintf(dest, nsize + 1, "%-.*s...%-.*s", tpos, orig, nsize - (3 + tpos), orig + osize - (nsize - (3 + tpos)));
    }
}

void FormatNumber(char* str, u64 number) { // str should be 32 byte in size
    u64 mag1000 = 1;
    *str = '\0';
    for (; number / (mag1000 * 1000) > 0; mag1000 *= 1000);
    for (; mag1000 > 0; mag1000 /= 1000) {
        u32 pos = strnlen(str, 31);
        snprintf(str + pos, 31 - pos, "%0*llu%c", (pos) ? 3 : 1, (number / mag1000) % 1000, (mag1000 > 1) ? ',' : '\0');
    }
}

void FormatBytes(char* str, u64 bytes) { // str should be 32 byte in size, just to be safe
    const char* units[] = {" Byte", " kB", " MB", " GB"};
    
    if (bytes == (u64) -1) snprintf(str, 32, "INVALID");
    else if (bytes < 1024) snprintf(str, 32, "%llu%s", bytes, units[0]);
    else {
        u32 scale = 1;
        u64 bytes100 = (bytes * 100) >> 10;
        for(; (bytes100 >= 1024*100) && (scale < 3); scale++, bytes100 >>= 10);
        snprintf(str, 32, "%llu.%llu%s", bytes100 / 100, (bytes100 % 100) / 10, units[scale]);
    }
}

void ShowString(const char *format, ...)
{
    if (format && *format) { // only if there is something in there
        char str[STRBUF_SIZE] = { 0 };
        va_list va;
        va_start(va, format);
        vsnprintf(str, STRBUF_SIZE, format, va);
        va_end(va);
        
        ClearScreenF(true, false, COLOR_STD_BG);
        DrawStringCenter(MAIN_SCREEN, COLOR_STD_FONT, COLOR_STD_BG, str);
    } else ClearScreenF(true, false, COLOR_STD_BG);
}

void ShowIconString(u8* icon, int w, int h, const char *format, ...)
{
    static const u32 icon_offset = 10;
    u32 str_width, str_height, tot_height;
    u32 x_str, y_str, x_bmp, y_bmp;
    
    ClearScreenF(true, false, COLOR_STD_BG);
    if (!format || !*format) return; // only if there is something in there
    
    char str[STRBUF_SIZE] = { 0 };
    va_list va;
    va_start(va, format);
    vsnprintf(str, STRBUF_SIZE, format, va);
    va_end(va);
    
    str_width = GetDrawStringWidth(str);
    str_height = GetDrawStringHeight(str);
    tot_height = h + icon_offset + str_height;
    x_str = (str_width >= SCREEN_WIDTH_MAIN) ? 0 : (SCREEN_WIDTH_MAIN - str_width) / 2;
    y_str = (str_height >= SCREEN_HEIGHT) ? 0 : h + icon_offset + (SCREEN_HEIGHT - tot_height) / 2;
    x_bmp = (w >= SCREEN_WIDTH_MAIN) ? 0 : (SCREEN_WIDTH_MAIN - w) / 2;
    y_bmp = (tot_height >= SCREEN_HEIGHT) ? 0 : (SCREEN_HEIGHT - tot_height) / 2;
    
    DrawBitmap(MAIN_SCREEN, x_bmp, y_bmp, w, h, icon);
    DrawStringF(MAIN_SCREEN, x_str, y_str, COLOR_STD_FONT, COLOR_STD_BG, str);
}

bool ShowPrompt(bool ask, const char *format, ...)
{
    bool ret = true;
    
    char str[STRBUF_SIZE] = { 0 };
    va_list va;
    va_start(va, format);
    vsnprintf(str, STRBUF_SIZE, format, va);
    va_end(va);
    
    ClearScreenF(true, false, COLOR_STD_BG);
    DrawStringCenter(MAIN_SCREEN, COLOR_STD_FONT, COLOR_STD_BG, "%s\n \n%s", str,
        (ask) ? "(<A> yes, <B> no)" : "(<A> to continue)");
    
    while (true) {
        u32 pad_state = InputWait(0);
        if (pad_state & BUTTON_A) break;
        else if (pad_state & BUTTON_B) {
            ret = false;
            break;
        }
    }
    
    ClearScreenF(true, false, COLOR_STD_BG);
    
    return ret;
}

#ifndef AUTO_UNLOCK
#define PRNG (*(volatile u32*)0x10011000)
bool ShowUnlockSequence(u32 seqlvl, const char *format, ...) {
    const int seqcolors[7] = { COLOR_STD_FONT, COLOR_BRIGHTGREEN, COLOR_BRIGHTYELLOW,
        COLOR_ORANGE, COLOR_BRIGHTBLUE, COLOR_RED, COLOR_DARKRED };
    const u32 seqlen_max = 7;
    const u32 seqlen = seqlen_max - ((seqlvl < 3) ? 2 : (seqlvl < 4) ? 1 : 0);
    
    
    u32 color_bg = COLOR_STD_BG;
    u32 color_font = COLOR_STD_FONT;
    u32 color_off = COLOR_GREY;
    u32 color_on = seqcolors[seqlvl];
    u32 lvl = 0;
    
    u32 str_width, str_height;
    u32 x, y;
    
    
    char str[STRBUF_SIZE] = { 0 };
    va_list va;
    va_start(va, format);
    vsnprintf(str, STRBUF_SIZE, format, va);
    va_end(va);
    
    str_width = GetDrawStringWidth(str);
    str_height = GetDrawStringHeight(str) + (4*line_height);
    if (str_width < 24 * font_width) str_width = 24 * font_width;
    x = (str_width >= SCREEN_WIDTH_MAIN) ? 0 : (SCREEN_WIDTH_MAIN - str_width) / 2;
    y = (str_height >= SCREEN_HEIGHT) ? 0 : (SCREEN_HEIGHT - str_height) / 2;
    
    if (seqlvl >= 6) { // special handling
        color_bg = seqcolors[seqlvl];
        color_font = COLOR_BLACK;
        color_off = COLOR_BLACK;
        color_on = COLOR_DARKGREY;
    }
    
    ClearScreenF(true, false, color_bg);
    DrawStringF(MAIN_SCREEN, x, y, color_font, color_bg, str);
    #ifndef TIMER_UNLOCK
    DrawStringF(MAIN_SCREEN, x, y + str_height - 28, color_font, color_bg, "To proceed, enter this:");
    
    // generate sequence
    const char dpad_symbols[] = { '\x1A', '\x1B', '\x18', '\x19' }; // R L U D
    
    u32 sequence[seqlen_max];
    char seqsymbols[seqlen_max];
    u32 lastlsh = (u32) -1;
    for (u32 n = 0; n < (seqlen-1); n++) {
        u32 lsh = lastlsh;
        while (lsh == lastlsh) lsh = (PRNG & 0x3);
        lastlsh = lsh;
        sequence[n] = BUTTON_RIGHT << lsh;
        seqsymbols[n] = dpad_symbols[lsh];
    }
    sequence[seqlen-1] = BUTTON_A;
    seqsymbols[seqlen-1] = 'A';
    
    
    while (true) {
        for (u32 n = 0; n < seqlen; n++) {
            DrawStringF(MAIN_SCREEN, x + (n*4*FONT_WIDTH_EXT), y + str_height - 28 + line_height,
                (lvl > n) ? color_on : color_off, color_bg, "<%c>", seqsymbols[n]);
        }
        if (lvl == seqlen)
            break;
        u32 pad_state = InputWait(0);
        if (!(pad_state & BUTTON_ANY))
            continue;
        else if ((pad_state & BUTTON_ANY) == sequence[lvl])
            lvl++;
        else if (pad_state & BUTTON_B)
            break;
        else if (lvl == 0 || !(pad_state & sequence[lvl-1]))
            lvl = 0;
    }
    #else
    DrawStringF(MAIN_SCREEN, x, y + str_height - 28, color_font, color_bg, "To proceed, hold <X>:");
    
    while (!CheckButton(BUTTON_B)) {
        for (u32 n = 0; n < seqlen; n++) {
            DrawStringF(MAIN_SCREEN, x + (n*4*FONT_WIDTH_EXT), y + str_height - 18,
                (lvl > n) ? color_on : color_off, color_bg, "<%c>", (lvl > n) ? 'X' : ' ');
        }
        if (lvl == seqlen)
            break;
        
        u32 prev_lvl = lvl;
        while (lvl == prev_lvl) {
            u64 timer = timer_start();
            while ((lvl != 0) && CheckButton(BUTTON_X) && (timer_msec(timer) < 400));
            
            if (CheckButton(BUTTON_B)) break;
            else if (CheckButton(BUTTON_X)) lvl++;
            else lvl = 0;
        }
    }
    #endif
    
    ClearScreenF(true, false, COLOR_STD_BG);
    
    return (lvl >= seqlen);
}
#endif

u32 ShowSelectPrompt(u32 n, const char** options, const char *format, ...) {
    u32 str_width, str_height;
    u32 x, y, yopt;
    u32 sel = 0;
    
    char str[STRBUF_SIZE] = { 0 };
    va_list va;
    va_start(va, format);
    vsnprintf(str, STRBUF_SIZE, format, va);
    va_end(va);
    
    if (n == 0) return 0; // check for low number of options
    // else if (n == 1) return ShowPrompt(true, "%s\n%s?", str, options[0]) ? 1 : 0;
    
    str_width = GetDrawStringWidth(str);
    str_height = GetDrawStringHeight(str) + (n * (line_height + 2)) + (3 * line_height);
    if (str_width < 24 * font_width) str_width = 24 * font_width;
    for (u32 i = 0; i < n; i++) if (str_width < GetDrawStringWidth(options[i]))
        str_width = GetDrawStringWidth(options[i]);
    x = (str_width >= SCREEN_WIDTH_MAIN) ? 0 : (SCREEN_WIDTH_MAIN - str_width) / 2;
    y = (str_height >= SCREEN_HEIGHT) ? 0 : (SCREEN_HEIGHT - str_height) / 2;
    yopt = y + GetDrawStringHeight(str) + 8;
    
    ClearScreenF(true, false, COLOR_STD_BG);
    DrawStringF(MAIN_SCREEN, x, y, COLOR_STD_FONT, COLOR_STD_BG, str);
    DrawStringF(MAIN_SCREEN, x, yopt + (n*(line_height+2)) + line_height, COLOR_STD_FONT, COLOR_STD_BG, "(<A> select, <B> cancel)");
    while (true) {
        for (u32 i = 0; i < n; i++) {
            DrawStringF(MAIN_SCREEN, x, yopt + ((line_height+2)*i), (sel == i) ? COLOR_STD_FONT : COLOR_LIGHTGREY, COLOR_STD_BG, "%2.2s %s",
                (sel == i) ? "->" : "", options[i]);
        }
        u32 pad_state = InputWait(0);
        if (pad_state & BUTTON_DOWN) sel = (sel+1) % n;
        else if (pad_state & BUTTON_UP) sel = (sel+n-1) % n;
        else if (pad_state & BUTTON_A) break;
        else if (pad_state & BUTTON_B) {
            sel = n;
            break;
        }
    }
    
    ClearScreenF(true, false, COLOR_STD_BG);
    
    return (sel >= n) ? 0 : sel + 1;
}

bool ShowInputPrompt(char* inputstr, u32 max_size, u32 resize, const char* alphabet, const char *format, va_list va) {
    const u32 alphabet_size = strnlen(alphabet, 256);
    const u32 input_shown = 22;
    const u32 fast_scroll = 4;
    
    u32 str_width, str_height;
    u32 x, y;
    
    char str[STRBUF_SIZE] = { 0 };
    vsnprintf(str, STRBUF_SIZE, format, va);
    
    // check / fix up the input string if required
    if (max_size < 2) return false; // catching this, too
    if ((*inputstr == '\0') || (resize && (strnlen(inputstr, max_size - 1) % resize))) {
        memset(inputstr, alphabet[0], resize); // set the string if it is not set or invalid
        inputstr[resize] = '\0';
    }
    
    str_width = GetDrawStringWidth(str);
    str_height = GetDrawStringHeight(str) + 88;
    if (str_width < (24 * font_width)) str_width = 24 * font_width;
    x = (str_width >= SCREEN_WIDTH_MAIN) ? 0 : (SCREEN_WIDTH_MAIN - str_width) / 2;
    y = (str_height >= SCREEN_HEIGHT) ? 0 : (SCREEN_HEIGHT - str_height) / 2;
    
    ClearScreenF(true, false, COLOR_STD_BG);
    DrawStringF(MAIN_SCREEN, x, y, COLOR_STD_FONT, COLOR_STD_BG, str);
    DrawStringF(MAIN_SCREEN, x + 8, y + str_height - 40, COLOR_STD_FONT, COLOR_STD_BG,
        "R - (\x18\x19) fast scroll\nL - clear data%s", resize ? "\nX - remove char\nY - insert char" : "");
    
    int cursor_a = -1;
    u32 cursor_s = 0;
    u32 scroll = 0;
    bool aprv = false;
    bool ret = false;
    
    while (true) {
        u32 inputstr_size = strnlen(inputstr, max_size - 1);
        if (cursor_s < scroll) scroll = cursor_s;
        else if (cursor_s - scroll >= input_shown) scroll = cursor_s - input_shown + 1;
        while (scroll && (inputstr_size - scroll < input_shown)) scroll--;
        DrawStringF(MAIN_SCREEN, x, y + str_height - 76, COLOR_STD_FONT, COLOR_STD_BG, "%c%-*.*s%c%-*.*s\n%-*.*s^%-*.*s",
            (scroll) ? '<' : '|',
            (inputstr_size > input_shown) ? input_shown : inputstr_size,
            (inputstr_size > input_shown) ? input_shown : inputstr_size,
            inputstr + scroll,
            (inputstr_size - scroll > input_shown) ? '>' : '|',
            (inputstr_size > input_shown) ? 0 : input_shown - inputstr_size,
            (inputstr_size > input_shown) ? 0 : input_shown - inputstr_size,
            "",
            1 + cursor_s - scroll,
            1 + cursor_s - scroll,
            "",
            input_shown - (cursor_s - scroll),
            input_shown - (cursor_s - scroll),
            ""
        );
        if (cursor_a < 0) {
            for (cursor_a = alphabet_size - 1; (cursor_a > 0) && (alphabet[cursor_a] != inputstr[cursor_s]); cursor_a--);
        }
        
        // alphabet preview
        if (alphabet_size > (SCREEN_WIDTH(MAIN_SCREEN) / font_width)) {
            const u32 aprv_y = y + str_height - 60;
            if (aprv) {
                const u32 aprv_pad = 1;
                const u32 aprv_cx = x + ((1 + cursor_s - scroll) * font_width);
                u32 aprv_x = aprv_cx % (font_width + aprv_pad);
                u32 aprv_n = ((SCREEN_WIDTH(MAIN_SCREEN) - aprv_x) / (font_width + aprv_pad)) - 1;
                int aprv_a = cursor_a - ((aprv_cx - aprv_x) / (font_width + aprv_pad));
                while (aprv_a < 0) aprv_a += alphabet_size;
                for (u32 i = 0; i < aprv_n; i++) {
                    DrawCharacter(MAIN_SCREEN, alphabet[aprv_a], aprv_x, aprv_y,
                        (aprv_a == cursor_a) ? COLOR_WHITE : COLOR_GREY, COLOR_STD_BG);
                    if (++aprv_a >= (int) alphabet_size) aprv_a -= alphabet_size;
                    aprv_x += font_width + aprv_pad;
                }
            } else DrawRectangle(MAIN_SCREEN, 0, aprv_y, SCREEN_WIDTH(MAIN_SCREEN), font_height, COLOR_STD_BG);
        }
        
        u32 pad_state = InputWait(3);
        aprv = (pad_state & (BUTTON_UP|BUTTON_DOWN|BUTTON_R1)) && !(pad_state & (BUTTON_RIGHT|BUTTON_LEFT));
        if (pad_state & BUTTON_A) {
            ret = true;
            break;
        } else if (pad_state & BUTTON_B) {
            break;
        } else if (pad_state & BUTTON_L1) {
            cursor_a = 0;
            memset(inputstr, alphabet[0], inputstr_size);
            if (resize) {
                cursor_s = 0;
                inputstr[1] = '\0';
            }
        } else if (pad_state & BUTTON_X) {
            if (resize && (inputstr_size > resize)) {
                char* inputfrom = inputstr + cursor_s - (cursor_s % resize) + resize;
                char* inputto = inputstr + cursor_s - (cursor_s % resize);
                memmove(inputto, inputfrom, max_size - (inputfrom - inputstr));
                inputstr_size -= resize;
                while (cursor_s >= inputstr_size)
                    cursor_s--;
                cursor_a = -1;
            } else if (resize == 1) {
                inputstr[0] = alphabet[0];
                cursor_a = 0;
            }
        } else if (pad_state & BUTTON_Y) {
            if (resize && (inputstr_size < max_size - resize)) {
                char* inputfrom = inputstr + cursor_s - (cursor_s % resize);
                char* inputto = inputstr + cursor_s - (cursor_s % resize) + resize;
                memmove(inputto, inputfrom, max_size - (inputto - inputstr));
                inputstr_size += resize;
                memset(inputfrom, alphabet[0], resize);
                cursor_a = 0;
            }
        } else if (pad_state & BUTTON_UP) {
            cursor_a += (pad_state & BUTTON_R1) ? fast_scroll : 1;
            cursor_a = cursor_a % alphabet_size;
            inputstr[cursor_s] = alphabet[cursor_a];
        } else if (pad_state & BUTTON_DOWN) {
            cursor_a -= (pad_state & BUTTON_R1) ? fast_scroll : 1;
            if (cursor_a < 0) cursor_a = alphabet_size + cursor_a;
            inputstr[cursor_s] = alphabet[cursor_a];
        } else if (pad_state & BUTTON_LEFT) {
            if (cursor_s > 0) cursor_s--;
            cursor_a = -1;
        } else if (pad_state & BUTTON_RIGHT) {
            if (cursor_s < max_size - 2) cursor_s++;
            if (cursor_s >= inputstr_size) {
                memset(inputstr + cursor_s, alphabet[0], resize);
                inputstr[cursor_s + resize] = '\0';
            }
            cursor_a = -1;
        }
    }
    // remove any trailing spaces
    for (char* cc = inputstr + strnlen(inputstr, max_size) - 1;
        (*cc == ' ') && (cc > inputstr); *(cc--) = '\0');
    
    ClearScreenF(true, false, COLOR_STD_BG);
    
    return ret;
}

bool ShowStringPrompt(char* inputstr, u32 max_size, const char *format, ...) {
    const char* alphabet = " ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz(){}[]'`^,~*?!@#$%&0123456789=+-_.";
    bool ret = false;
    va_list va;
    
    va_start(va, format);
    ret = ShowInputPrompt(inputstr, max_size, 1, alphabet, format, va);
    va_end(va);
    
    return ret; 
}

u64 ShowHexPrompt(u64 start_val, u32 n_digits, const char *format, ...) {
    const char* alphabet = "0123456789ABCDEF";
    char inputstr[16 + 1] = { 0 };
    u64 ret = 0;
    va_list va;
    
    if (n_digits > 16) n_digits = 16;
    snprintf(inputstr, 16 + 1, "%0*llX", (int) n_digits, start_val);
    
    va_start(va, format);
    if (ShowInputPrompt(inputstr, n_digits + 1, 0, alphabet, format, va)) {
        sscanf(inputstr, "%llX", &ret);
    } else ret = (u64) -1;
    va_end(va);
    
    return ret; 
}

u64 ShowNumberPrompt(u64 start_val, const char *format, ...) {
    const char* alphabet = "0123456789";
    char inputstr[20 + 1] = { 0 };
    u64 ret = 0;
    va_list va;
    
    snprintf(inputstr, 20 + 1, "%llu", start_val);
    
    va_start(va, format);
    if (ShowInputPrompt(inputstr, 20 + 1, 1, alphabet, format, va)) {
        sscanf(inputstr, "%llu", &ret);
    } else ret = (u64) -1;
    va_end(va);
    
    return ret; 
}

bool ShowDataPrompt(u8* data, u32* size, const char *format, ...) {
    const char* alphabet = "0123456789ABCDEF";
    char inputstr[128 + 1] = { 0 }; // maximum size of data: 64 byte
    bool ret = false;
    va_list va;
    
    if (*size > 64) *size = 64;
    for (u32 i = 0; i < *size; i++)
        snprintf(inputstr + (2*i), 128 + 1 - (2*i), "%02X", (unsigned int) data[i]);
    
    va_start(va, format);
    if (ShowInputPrompt(inputstr, 128 + 1, 2, alphabet, format, va)) {
        *size = strnlen(inputstr, 128) / 2;
        for (u32 i = 0; i < *size; i++) {
            char bytestr[2 + 1] = { 0 };
            unsigned int byte;
            strncpy(bytestr, inputstr + (2*i), 2);
            sscanf(bytestr, "%02X", &byte);
            data[i] = (u8) byte;
        }
        ret = true;
    }
    va_end(va);
    
    return ret; 
}


bool ShowRtcSetterPrompt(void* time, const char *format, ...) {
    DsTime* dstime = (DsTime*) time;
    u32 str_width, str_height;
    u32 x, y;
    
    char str[STRBUF_SIZE] = { 0 };
    va_list va;
    va_start(va, format);
    vsnprintf(str, STRBUF_SIZE, format, va);
    va_end(va);
    
    // check the initial time
    if (!is_valid_dstime(dstime)) {
        dstime->bcd_h = dstime->bcd_m = dstime->bcd_s = 0x00;
        dstime->bcd_D = dstime->bcd_M = 0x01;
        dstime->bcd_Y = 0x00;
    }
    
    str_width = GetDrawStringWidth(str);
    str_height = GetDrawStringHeight(str) + (4*line_height);
    if (str_width < (19 * font_width)) str_width = 19 * font_width;
    x = (str_width >= SCREEN_WIDTH_MAIN) ? 0 : (SCREEN_WIDTH_MAIN - str_width) / 2;
    y = (str_height >= SCREEN_HEIGHT) ? 0 : (SCREEN_HEIGHT - str_height) / 2;
    
    ClearScreenF(true, false, COLOR_STD_BG);
    DrawStringF(MAIN_SCREEN, x, y, COLOR_STD_FONT, COLOR_STD_BG, str);
    
    int cursor = 0;
    bool ret = false;
    while (true) {
        static const int val_max[] = { 99, 12, 31, 23, 59, 59 };
        static const int val_min[] = {  0,  1,  1,  0,  0,  0 };
        u8* bcd = &(((u8*)dstime)[(cursor<3) ? (6-cursor) : (6-1-cursor)]);
        int val = BCD2NUM(*bcd);
        int max = val_max[cursor];
        int min = val_min[cursor];
        DrawStringF(MAIN_SCREEN, x, y + str_height - 28, COLOR_STD_FONT, COLOR_STD_BG, "YYYY-MM-DD hh:mm:ss");
        DrawStringF(MAIN_SCREEN, x, y + str_height - 18, COLOR_STD_FONT, COLOR_STD_BG, "20%02lX-%02lX-%02lX %02lX:%02lX:%02lX\n  %-*.*s^^%-*.*s",
            (u32) dstime->bcd_Y, (u32) dstime->bcd_M, (u32) dstime->bcd_D, (u32) dstime->bcd_h, (u32) dstime->bcd_m, (u32) dstime->bcd_s,
            cursor * 3, cursor * 3, "", 17 - 2 - (cursor * 3), 17 - 2 - (cursor * 3), "");
            
        // user input
        u32 pad_state = InputWait(0);
        if ((pad_state & BUTTON_A) && is_valid_dstime(dstime)) {
            ret = true;
            break;
        } else if (pad_state & BUTTON_B) {
            break;
        } else if (pad_state & BUTTON_UP) {
            val += (pad_state & BUTTON_R1) ? 10 : 1;
            if (val > max) val = max;
        } else if (pad_state & BUTTON_DOWN) {
            val -= (pad_state & BUTTON_R1) ? 10 : 1;
            if (val < min) val = min;
        } else if (pad_state & BUTTON_LEFT) {
            if (--cursor < 0) cursor = 5;
        } else if (pad_state & BUTTON_RIGHT) {
            if (++cursor > 5) cursor = 0;
        }
        
        // update bcd
        *bcd = NUM2BCD(val);
    }
    
    ClearScreenF(true, false, COLOR_STD_BG);
    
    return ret;
}

bool ShowProgress(u64 current, u64 total, const char* opstr)
{
    static u32 last_prog_width = 0;
    static u64 timer = 0;
    const u32 bar_width = 240;
    const u32 bar_height = 12;
    const u32 bar_pos_x = (SCREEN_WIDTH_MAIN - bar_width) / 2;
    const u32 bar_pos_y = (SCREEN_HEIGHT / 2) - bar_height - 2 - 10;
    const u32 text_pos_y = bar_pos_y + bar_height + 2;
    u32 prog_width = ((total > 0) && (current <= total)) ? (current * (bar_width-4)) / total : 0;
    u32 prog_percent = ((total > 0) && (current <= total)) ? (current * 100) / total : 0;
    char tempstr[64];
    char progstr[64];
    
    static u64 last_sec_remain = 0;
    if (!current) {
        timer = timer_start();
        last_sec_remain = 0;
    }
    u64 sec_elapsed = (total > 0) ? timer_sec( timer ) : 0;
    u64 sec_total = (current > 0) ? (sec_elapsed * total) / current : 0;
    u64 sec_remain = (!last_sec_remain) ? (sec_total - sec_elapsed) : ((last_sec_remain + (sec_total - sec_elapsed) + 1) / 2);
    if (sec_remain >= 60 * 60) sec_remain = 60 * 60 - 1;
    last_sec_remain = sec_remain;
    
    if (!current || last_prog_width > prog_width) {
        ClearScreenF(true, false, COLOR_STD_BG);
        DrawRectangle(MAIN_SCREEN, bar_pos_x, bar_pos_y, bar_width, bar_height, COLOR_STD_FONT);
        DrawRectangle(MAIN_SCREEN, bar_pos_x + 1, bar_pos_y + 1, bar_width - 2, bar_height - 2, COLOR_STD_BG);
    }
    DrawRectangle(MAIN_SCREEN, bar_pos_x + 2, bar_pos_y + 2, prog_width, bar_height - 4, COLOR_STD_FONT);
    DrawRectangle(MAIN_SCREEN, bar_pos_x + 2 + prog_width, bar_pos_y + 2, (bar_width-4) - prog_width, bar_height - 4, COLOR_STD_BG);
    
    TruncateString(progstr, opstr, (bar_width / FONT_WIDTH_EXT) - 7, 8);
    snprintf(tempstr, 64, "%s (%lu%%)", progstr, prog_percent);
    ResizeString(progstr, tempstr, bar_width / FONT_WIDTH_EXT, 8, false);
    DrawString(MAIN_SCREEN, progstr, bar_pos_x, text_pos_y, COLOR_STD_FONT, COLOR_STD_BG, true);
    if (sec_elapsed >= 1) {
        snprintf(tempstr, 16, "ETA %02llum%02llus", sec_remain / 60, sec_remain % 60);
        ResizeString(progstr, tempstr, 16, 8, true);
        DrawString(MAIN_SCREEN, progstr, bar_pos_x + bar_width - 1 - (FONT_WIDTH_EXT * 16),
            bar_pos_y - line_height - 1, COLOR_STD_FONT, COLOR_STD_BG, true);
    }
    DrawString(MAIN_SCREEN, "(hold B to cancel)", bar_pos_x + 2, text_pos_y + 14, COLOR_STD_FONT, COLOR_STD_BG, false);
    
    last_prog_width = prog_width;
    
    CheckBrightness();
    return !CheckButton(BUTTON_B);
}
