// Copyright 2013 Normmatt
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vram.h>
#include "common.h"
#include "colors.h"


#define BYTES_PER_PIXEL 3
#define SCREEN_HEIGHT 240
#define SCREEN_WIDTH(s) ((s == TOP_SCREEN) ? SCREEN_WIDTH_TOP : SCREEN_WIDTH_BOT)
#define SCREEN_WIDTH_TOP 400
#define SCREEN_WIDTH_BOT 320
#define SCREEN_SIZE(s) ((s == TOP_SCREEN) ? SCREEN_SIZE_TOP : SCREEN_SIZE_BOT)
#define SCREEN_SIZE_TOP (SCREEN_WIDTH_TOP * SCREEN_HEIGHT * BYTES_PER_PIXEL)
#define SCREEN_SIZE_BOT (SCREEN_WIDTH_BOT * SCREEN_HEIGHT * BYTES_PER_PIXEL)
#define FONT_WIDTH_EXT   GetFontWidth()
#define FONT_HEIGHT_EXT  GetFontHeight()

#define TOP_SCREEN          ((u8*)VRAM_TOP_LA)
#define BOT_SCREEN          ((u8*)VRAM_BOT_A)

#ifdef SWITCH_SCREENS
#define MAIN_SCREEN         TOP_SCREEN
#define ALT_SCREEN          BOT_SCREEN
#define SCREEN_WIDTH_MAIN   SCREEN_WIDTH_TOP
#define SCREEN_WIDTH_ALT    SCREEN_WIDTH_BOT
#else
#define MAIN_SCREEN         BOT_SCREEN
#define ALT_SCREEN          TOP_SCREEN
#define SCREEN_WIDTH_MAIN   SCREEN_WIDTH_BOT
#define SCREEN_WIDTH_ALT    SCREEN_WIDTH_TOP
#endif

#define COLOR_TRANSPARENT   COLOR_SUPERFUCHSIA


#ifndef AUTO_UNLOCK
bool ShowUnlockSequence(u32 seqlvl, const char *format, ...);
#else
#define ShowUnlockSequence ShowPrompt
#endif

u8* GetFontFromPbm(const void* pbm, const u32 pbm_size, u32* w, u32* h);
bool SetFontFromPbm(const void* pbm, const u32 pbm_size);

void ClearScreen(unsigned char *screen, int color);
void ClearScreenF(bool clear_main, bool clear_alt, int color);
void DrawRectangle(u8* screen, int x, int y, int width, int height, int color);
void DrawBitmap(u8* screen, int x, int y, int w, int h, u8* bitmap);
void DrawQrCode(u8* screen, u8* qrcode);

void DrawCharacter(unsigned char *screen, int character, int x, int y, int color, int bgcolor);
void DrawString(unsigned char *screen, const char *str, int x, int y, int color, int bgcolor, bool fix_utf8);
void DrawStringF(unsigned char *screen, int x, int y, int color, int bgcolor, const char *format, ...);
void DrawStringCenter(u8* screen, int color, int bgcolor, const char *format, ...);

u32 GetDrawStringHeight(const char* str);
u32 GetDrawStringWidth(const char* str);
u32 GetFontWidth(void);
u32 GetFontHeight(void);

void WordWrapString(char* str, int llen);
void ResizeString(char* dest, const char* orig, int nsize, int tpos, bool align_right);
void TruncateString(char* dest, const char* orig, int nsize, int tpos);
void FormatNumber(char* str, u64 number);
void FormatBytes(char* str, u64 bytes);

void ShowString(const char *format, ...);
void ShowIconString(u8* icon, int w, int h, const char *format, ...);
bool ShowPrompt(bool ask, const char *format, ...);
u32 ShowSelectPrompt(u32 n, const char** options, const char *format, ...);
bool ShowStringPrompt(char* inputstr, u32 max_size, const char *format, ...);
u64 ShowHexPrompt(u64 start_val, u32 n_digits, const char *format, ...);
u64 ShowNumberPrompt(u64 start_val, const char *format, ...);
bool ShowDataPrompt(u8* data, u32* size, const char *format, ...);
bool ShowRtcSetterPrompt(void* time, const char *format, ...);
bool ShowProgress(u64 current, u64 total, const char* opstr);
