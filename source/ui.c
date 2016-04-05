// Copyright 2013 Normmatt
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include "font.h"
#include "ui.h"
#include "hid.h"

void ClearScreen(u8* screen, int width, int color)
{
    if (color == COLOR_TRANSPARENT) color = COLOR_BLACK;
    for (int i = 0; i < (width * SCREEN_HEIGHT); i++) {
        *(screen++) = color >> 16;  // B
        *(screen++) = color >> 8;   // G
        *(screen++) = color & 0xFF; // R
    }
}

void ClearScreenF(bool clear_top, bool clear_bottom, int color)
{
    if (clear_top) {
        ClearScreen(TOP_SCREEN0, SCREEN_WIDTH_TOP, color);
        ClearScreen(TOP_SCREEN1, SCREEN_WIDTH_TOP, color);
    }
    if (clear_bottom) {
        ClearScreen(BOT_SCREEN0, SCREEN_WIDTH_BOT, color);
        ClearScreen(BOT_SCREEN1, SCREEN_WIDTH_BOT, color);
    }
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

void DrawRectangleF(bool use_top, int x, int y, int width, int height, int color)
{
    if (use_top) {
        DrawRectangle(TOP_SCREEN0, x, y, width, height, color);
        DrawRectangle(TOP_SCREEN1, x, y, width, height, color);
    } else {
        DrawRectangle(BOT_SCREEN0, x, y, width, height, color);
        DrawRectangle(BOT_SCREEN1, x, y, width, height, color);
    }
}

void DrawCharacter(u8* screen, int character, int x, int y, int color, int bgcolor)
{
    for (int yy = 0; yy < 8; yy++) {
        int xDisplacement = (x * BYTES_PER_PIXEL * SCREEN_HEIGHT);
        int yDisplacement = ((SCREEN_HEIGHT - (y + yy) - 1) * BYTES_PER_PIXEL);
        u8* screenPos = screen + xDisplacement + yDisplacement;

        u8 charPos = font[character * 8 + yy];
        for (int xx = 7; xx >= 0; xx--) {
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

void DrawString(u8* screen, const char *str, int x, int y, int color, int bgcolor)
{
    for (int i = 0; i < strlen(str); i++)
        DrawCharacter(screen, str[i], x + i * 8, y, color, bgcolor);
}

void DrawStringF(bool use_top, int x, int y, int color, int bgcolor, const char *format, ...)
{
    char str[512] = {}; // 512 should be more than enough
    va_list va;

    va_start(va, format);
    vsnprintf(str, 512, format, va);
    va_end(va);

    for (char* text = strtok(str, "\n"); text != NULL; text = strtok(NULL, "\n"), y += 10) {
        if (use_top) {
            DrawString(TOP_SCREEN0, text, x, y, color, bgcolor);
            DrawString(TOP_SCREEN1, text, x, y, color, bgcolor);
        } else {
            DrawString(BOT_SCREEN0, text, x, y, color, bgcolor);
            DrawString(BOT_SCREEN1, text, x, y, color, bgcolor);
        }
    }
}

u32 GetDrawStringHeight(const char* str) {
    u32 height = 8;
    for (char* lf = strchr(str, '\n'); (lf != NULL); lf = strchr(lf + 1, '\n'))
        height += 10;
    return height;
}

u32 GetDrawStringWidth(char* str) {
    u32 width = 0;
    char* old_lf = str;
    char* str_end = str + strnlen(str, 512);
    for (char* lf = strchr(str, '\n'); lf != NULL; lf = strchr(lf + 1, '\n')) {
        if ((lf - old_lf) > width) width = lf - old_lf;
        old_lf = lf;
    }
    if (str_end - old_lf > width)
        width = str_end - old_lf;
    width *= 8;
    return width;
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
    } if (nsize <= 3) {
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
    const char* units[] = {" byte", " kB", " MB", " GB"};
    
    if (bytes == (u64) -1) snprintf(str, 32, "INVALID");
    else if (bytes < 1024) snprintf(str, 32, "%llu%s", bytes, units[0]);
    else {
        u32 scale = 1;
        u64 bytes100 = (bytes * 100) >> 10;
        for(; (bytes100 >= 1024*100) && (scale < 3); scale++, bytes100 >>= 10);
        snprintf(str, 32, "%llu.%llu%s", bytes100 / 100, (bytes100 % 100) / 10, units[scale]);
    }
}

bool ShowPrompt(bool ask, const char *format, ...)
{
    u32 str_width, str_height;
    u32 x, y;
    bool ret = true;
    
    char str[512] = {}; // 512 should be more than enough
    va_list va;

    va_start(va, format);
    vsnprintf(str, 512, format, va);
    va_end(va);
    
    str_width = GetDrawStringWidth(str);
    str_height = GetDrawStringHeight(str) + (2 * 10);
    if (str_width < 18*8) str_width = 18 * 8;
    x = (str_width >= SCREEN_WIDTH_TOP) ? 0 : (SCREEN_WIDTH_TOP - str_width) / 2;
    y = (str_height >= SCREEN_HEIGHT) ? 0 : (SCREEN_HEIGHT - str_height) / 2;
    
    ClearScreenF(true, false, COLOR_STD_BG);
    DrawStringF(true, x, y, COLOR_STD_FONT, COLOR_STD_BG, str);
    DrawStringF(true, x, y + str_height - (1*10), COLOR_STD_FONT, COLOR_STD_BG, (ask) ? "(<A> yes, <B> no)" : "(<A> to continue)");
    
    while (true) {
        u32 pad_state = InputWait();
        if (pad_state & BUTTON_A) break;
        else if (ask && (pad_state & BUTTON_B)) {
            ret = false;
            break;
        }
    }
    
    ClearScreenF(true, false, COLOR_STD_BG);
    
    return ret;
}

bool ShowUnlockSequence(u32 seqlvl, const char *format, ...) {
    const u32 seqcolors[4] = { COLOR_STD_FONT, COLOR_GREEN, COLOR_YELLOW, COLOR_RED };
    const u32 sequences[4][5] = {
        { BUTTON_RIGHT, BUTTON_DOWN, BUTTON_RIGHT, BUTTON_DOWN, BUTTON_A },
        { BUTTON_LEFT, BUTTON_DOWN, BUTTON_RIGHT, BUTTON_UP, BUTTON_A },
        { BUTTON_LEFT, BUTTON_RIGHT, BUTTON_DOWN, BUTTON_UP, BUTTON_A },
        { BUTTON_LEFT, BUTTON_UP, BUTTON_RIGHT, BUTTON_UP, BUTTON_A }
    };
    const char seqsymbols[4][5] = { 
        { '\x1A', '\x19', '\x1A', '\x19', 'A' },
        { '\x1B', '\x19', '\x1A', '\x18', 'A' },
        { '\x1B', '\x1A', '\x19', '\x18', 'A' },
        { '\x1B', '\x18', '\x1A', '\x18', 'A' },
    };
    const u32 len = 5;
    u32 lvl = 0;
    
    u32 str_width, str_height;
    u32 x, y;
    
    char str[512] = {}; // 512 should be more than enough
    va_list va;

    va_start(va, format);
    vsnprintf(str, 512, format, va);
    va_end(va);
    
    str_width = GetDrawStringWidth(str);
    str_height = GetDrawStringHeight(str) + (4*10);
    if (str_width < 24 * 8) str_width = 24 * 8;
    x = (str_width >= SCREEN_WIDTH_TOP) ? 0 : (SCREEN_WIDTH_TOP - str_width) / 2;
    y = (str_height >= SCREEN_HEIGHT) ? 0 : (SCREEN_HEIGHT - str_height) / 2;
    
    ClearScreenF(true, false, COLOR_STD_BG);
    DrawStringF(true, x, y, COLOR_STD_FONT, COLOR_STD_BG, str);
    DrawStringF(true, x, y + str_height - 28, COLOR_STD_FONT, COLOR_STD_BG, "To proceed, enter this:");
    
    while (true) {
        for (u32 n = 0; n < len; n++) {
            DrawStringF(true, x + (n*4*8), y + str_height - 18,
                (lvl > n) ? seqcolors[seqlvl] : COLOR_GREY, COLOR_STD_BG, "<%c>", seqsymbols[seqlvl][n]);
        }
        if (lvl == len)
            break;
        u32 pad_state = InputWait();
        if (!(pad_state & BUTTON_ANY))
            continue;
        else if (pad_state & sequences[seqlvl][lvl])
            lvl++;
        else if (pad_state & BUTTON_B)
            break;
        else if (lvl == 0 || !(pad_state & sequences[seqlvl][lvl-1]))
            lvl = 0;
    }
    
    ClearScreenF(true, false, COLOR_STD_BG);
    
    return (lvl >= len);
}

bool ShowInputPrompt(char* inputstr, u32 max_size, const char *format, ...) {
    const char* alphabet = " ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz(){}[]'`^,~!@#$%&0123456789=+-_.";
    const u32 alphabet_size = strnlen(alphabet, 256);
    const u32 input_shown = 22;
    const u32 fast_scroll = 4;
    
    u32 str_width, str_height;
    u32 x, y;
    
    char str[512] = {}; // 512 should be more than enough
    va_list va;

    va_start(va, format);
    vsnprintf(str, 512, format, va);
    va_end(va);
    
    // check / fix up the inputstring if required
    if (max_size < 2) return false; // catching this, too
    if (*inputstr == '\0') snprintf(inputstr, 2, "%c", alphabet[0]); // set the string if it is not set
    
    str_width = GetDrawStringWidth(str);
    str_height = GetDrawStringHeight(str) + (8*10);
    if (str_width < 24) str_width = 24;
    x = (str_width >= SCREEN_WIDTH_TOP) ? 0 : (SCREEN_WIDTH_TOP - str_width) / 2;
    y = (str_height >= SCREEN_HEIGHT) ? 0 : (SCREEN_HEIGHT - str_height) / 2;
    
    ClearScreenF(true, false, COLOR_STD_BG);
    DrawStringF(true, x, y, COLOR_STD_FONT, COLOR_STD_BG, str);
    DrawStringF(true, x + 8, y + str_height - 38, COLOR_STD_FONT, COLOR_STD_BG, "R - (\x18\x19) fast scroll\nL - clear string\nX - remove char\nY - insert char");
    
    int cursor_s = 0;
    int cursor_a = -1;
    int scroll = 0;
    bool ret = false;
    
    while (true) {
        u32 inputstr_size = strnlen(inputstr, max_size - 1);
        if (cursor_s < scroll) scroll = cursor_s;
        else if (cursor_s - scroll >= input_shown) scroll = cursor_s - input_shown + 1;
        while (scroll && (inputstr_size - scroll < input_shown)) scroll--;
        DrawStringF(true, x, y + str_height - 68, COLOR_STD_FONT, COLOR_STD_BG, "%c%-*.*s%c%-*.*s\n%-*.*s^%-*.*s",
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
        u32 pad_state = InputWait();
        if (pad_state & BUTTON_A) {
            ret = true;
            break;
        } else if (pad_state & BUTTON_B) {
            break;
        } else if (pad_state & BUTTON_L1) {
            cursor_a = 0;
            cursor_s = 0;
            inputstr[0] = alphabet[0];
            inputstr[1] = '\0';
        } else if (pad_state & BUTTON_X) {
            if (inputstr_size > 1) {
                memmove(&inputstr[cursor_s], &inputstr[cursor_s + 1], (max_size - 1) - cursor_s);
                inputstr_size--;
                if (cursor_s >= inputstr_size) {
                    cursor_s--;
                    cursor_a = -1;
                }
            } else inputstr[0] = alphabet[0];
        } else if (pad_state & BUTTON_Y) {
            if (inputstr_size < max_size - 1) {
                memmove(&inputstr[cursor_s + 1], &inputstr[cursor_s], (max_size - 1 )- cursor_s);
                inputstr_size++;
                inputstr[cursor_s] = alphabet[0];
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
                inputstr[cursor_s] = alphabet[0];
                inputstr[cursor_s+1] = '\0';
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

bool ShowProgress(u64 current, u64 total, const char* opstr)
{
    static u32 last_prog_width = 0;
    const u32 bar_width = 240;
    const u32 bar_height = 12;
    const u32 bar_pos_x = (SCREEN_WIDTH_TOP - bar_width) / 2;
    const u32 bar_pos_y = (SCREEN_HEIGHT / 2) - bar_height - 2 - 10;
    const u32 text_pos_y = bar_pos_y + bar_height + 2;
    u32 prog_width = ((total > 0) && (current <= total)) ? (current * (bar_width-4)) / total : 0;
    char tempstr[64];
    
    if (!current || last_prog_width > prog_width) { 
        ClearScreenF(true, false, COLOR_STD_BG);
        DrawRectangleF(true, bar_pos_x, bar_pos_y, bar_width, bar_height, COLOR_STD_FONT);
        DrawRectangleF(true, bar_pos_x + 1, bar_pos_y + 1, bar_width - 2, bar_height - 2, COLOR_STD_BG);
    }
    DrawRectangleF(true, bar_pos_x + 2, bar_pos_y + 2, prog_width, bar_height - 4, COLOR_STD_FONT);
    
    ResizeString(tempstr, opstr, 28, 8, false);
    DrawString(TOP_SCREEN0, tempstr, bar_pos_x, text_pos_y, COLOR_STD_FONT, COLOR_STD_BG);
    DrawString(TOP_SCREEN1, tempstr, bar_pos_x, text_pos_y, COLOR_STD_FONT, COLOR_STD_BG);
    DrawString(TOP_SCREEN0, "(hold B to cancel)", bar_pos_x + 2, text_pos_y + 14, COLOR_STD_FONT, COLOR_STD_BG);
    DrawString(TOP_SCREEN1, "(hold B to cancel)", bar_pos_x + 2, text_pos_y + 14, COLOR_STD_FONT, COLOR_STD_BG);
    
    last_prog_width = prog_width;
    
    return !(~HID_STATE & BUTTON_B);
}
