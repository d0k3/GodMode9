#pragma once


#define RGB(r,g,b) ((r)<<24|(b)<<16|(g)<<8|(r))

// a base set of colors below
#define COLOR_BLACK         RGB(0x00, 0x00, 0x00)
#define COLOR_WHITE         RGB(0xFF, 0xFF, 0xFF)
#define COLOR_GREY          RGB(0x80, 0x80, 0x80)

#define COLOR_RED           RGB(0xFF, 0x00, 0x00)
#define COLOR_GREEN         RGB(0x00, 0xFF, 0x00)
#define COLOR_BLUE          RGB(0x00, 0x00, 0xFF)
#define COLOR_YELLOW        RGB(0xFF, 0xFF, 0x00)
#define COLOR_CYAN          RGB(0xFF, 0x00, 0xFF)
#define COLOR_ORANGE        RGB(0xFF, 0xA5, 0x00)

#define COLOR_BRIGHTRED     RGB(0xFF, 0x30, 0x30)
#define COLOR_DARKRED       RGB(0x80, 0x00, 0x00)
#define COLOR_BRIGHTYELLOW  RGB(0xFF, 0xFF, 0x30)
#define COLOR_BRIGHTGREEN   RGB(0x30, 0xFF, 0x30)
#define COLOR_BRIGHTBLUE    RGB(0x30, 0x30, 0xFF)

#define COLOR_TINTEDBLUE    RGB(0x60, 0x60, 0x80)
#define COLOR_TINTEDYELLOW  RGB(0xD0, 0xD0, 0x60)
#define COLOR_TINTEDGREEN   RGB(0x70, 0x80, 0x70)
#define COLOR_LIGHTGREY     RGB(0xB0, 0xB0, 0xB0)
#define COLOR_LIGHTERGREY   RGB(0xD0, 0xD0, 0xD0)
#define COLOR_DARKGREY      RGB(0x50, 0x50, 0x50)
#define COLOR_DARKESTGREY   RGB(0x20, 0x20, 0x20)
#define COLOR_SUPERFUCHSIA  RGB(0xFF, 0x00, 0xEF)

// standard colors - used everywhere
#define COLOR_STD_BG        COLOR_BLACK
#define COLOR_STD_FONT      COLOR_WHITE

// colors for GodMode9 file browser
#define COLOR_SIDE_BAR      COLOR_DARKGREY
#define COLOR_MARKED        COLOR_TINTEDYELLOW
#define COLOR_FILE          COLOR_TINTEDGREEN
#define COLOR_DIR           COLOR_TINTEDBLUE
#define COLOR_ROOT          COLOR_GREY

// hex viewer colors
#define COLOR_HVOFFS        RGB(0x40, 0x60, 0x50)
#define COLOR_HVOFFSI       COLOR_DARKESTGREY
#define COLOR_HVASCII       RGB(0x40, 0x80, 0x50)
#define COLOR_HVHEX(i)      ((i % 2) ? RGB(0x30, 0x90, 0x30) : RGB(0x30, 0x80, 0x30))

// text viewer / script viewer colors
#define COLOR_TVOFFS        RGB(0x40, 0x40, 0x40)
#define COLOR_TVOFFSL       RGB(0x30, 0x30, 0x30)
#define COLOR_TVTEXT        RGB(0xA0, 0xA0, 0xA0)
#define COLOR_TVRUN         RGB(0xC0, 0x00, 0x00)
#define COLOR_TVCMT         RGB(0x60, 0x60, 0x70)
#define COLOR_TVCMD         RGB(0xA0, 0xA0, 0xA0)

// battery symbol colors
#define COLOR_BATTERY_CHARGING  RGB(0x3D, 0xB7, 0xE4)
#define COLOR_BATTERY_FULL      RGB(0x0F, 0xB0, 0x1B)
#define COLOR_BATTERY_MEDIUM    RGB(0xFF, 0x88, 0x49)
#define COLOR_BATTERY_LOW       RGB(0xB4, 0x00, 0x00)
