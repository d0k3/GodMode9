#pragma once

#include "common.h"

// special key ids
enum {
    KEY_DUMMY   = 0x80,
    KEY_BKSPC   = 0x81,
    KEY_ENTER   = 0x82,
    KEY_CAPS    = 0x83,
    KEY_SPECIAL = 0x84,
    KEY_ALPHA   = 0x85,
    KEY_LEFT    = 0x86,
    KEY_RIGHT   = 0x87
};

// special key strings
#define SWKBD_KEYSTR "", "DEL", "SUBMIT", "CAPS", "#$@", "ABC", "\x1b", "\x1a"

#define COLOR_SWKBD_NORMAL  COLOR_GREY
#define COLOR_SWKBD_PRESSED COLOR_LIGHTGREY
#define COLOR_SWKBD_BOX     COLOR_DARKGREY
#define COLOR_SWKBD_TEXT    COLOR_BLACK
#define COLOR_SWKBD_ENTER   COLOR_TINTEDBLUE
#define COLOR_SWKBD_CAPS    COLOR_TINTEDYELLOW

#define SWKBD_STDKEY_WIDTH  18
#define SWKBD_STDKEY_HEIGHT 20
#define SWKDB_KEY_SPACING   1

#define SWKBD_KEYS_ALPHABET \
    '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '+', '-', KEY_BKSPC, \
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '@', KEY_ENTER, \
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', '(', ')', '[', ']', \
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '_', '#', '!', \
    KEY_CAPS, KEY_SPECIAL, ' ', KEY_LEFT, KEY_RIGHT

#define SWKBD_KEYS_SPECIAL \
    '(', ')', '{', '}', '[', ']', \
    '.', ',', '?', '!', '`', '\'', \
    '^', '*', '+', '-', '_', '=', \
    '@', '#', '$', '%', '&', '~', \
    KEY_ALPHA, ' ', KEY_LEFT, KEY_RIGHT

/*#define SWKBD_KEYS_NUMPAD \
    '7', '8', '9', 'E', 'F', \
    '4', '5', '6', 'C', 'D', \
    '3', '2', '1', 'A', 'B', \*/

// offset, num of keys in row, width of special keys (...), 0
#define SWKBD_LAYOUT_ALPHABET \
    13, 32, 0, \
    12, 51, 0, \
    13, 0, \
    12, 0, \
    5, 32, 32, 156, 18, 18, 0, \
    0

#define SWKBD_LAYOUT_SPECIAL \
    6, 0, \
    6, 0, \
    6, 0, \
    6, 0, \
    4, 32, 42, 18, 18, 0, \
    0

bool ShowKeyboard(char* inputstr, u32 max_size, const char *format, ...);
