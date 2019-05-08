#include "swkbd.h"
#include "ui.h"
#include "hid.h"


static inline char to_uppercase(char c) {
    if ((c >= 'a') && (c <= 'z')) c += ('A'-'a');
    return c;
}

static bool BuildKeyboard(TouchBox* swkbd, const char* keys, const u8* layout) {
    // count # of rows
    u32 n_rows = 0;
    for (u32 i = 0;; i++) {
        if (layout[i] == 0) {
            n_rows++;
            if (layout[i+1] == 0) break;
        }
    }

    // set p_y start position
    u32 height = (n_rows) ? (n_rows * SWKBD_STDKEY_HEIGHT) + ((n_rows-1) * SWKDB_KEY_SPACING) : 0;
    u32 p_y = SCREEN_HEIGHT - height - SWKBD_STDKEY_HEIGHT - SWKDB_KEY_SPACING;

    // set button positions
    TouchBox* tb = swkbd;
    for (u32 l = 0, k = 0; layout[l] != 0; ) {
        // calculate width of current row
        u32 n_keys = layout[l++];
        u32 width = (n_keys * SWKBD_STDKEY_WIDTH) + ((n_keys-1) * SWKDB_KEY_SPACING);
        for (u32 i = 0; layout[l+i] != 0; i++)
            width = width - SWKBD_STDKEY_WIDTH + layout[l+i];

        // set p_x start position
        if (width > SCREEN_WIDTH_BOT) return false;
        u32 p_x = (SCREEN_WIDTH_BOT - width) / 2;

        // set up touchboxes
        for (u32 i = 0; i < n_keys; i++) {
            tb->id = keys[k++];
            tb->x = p_x;
            tb->y = p_y;
            tb->w = ((tb->id >= 0x80) || (tb->id == (u32) ' ')) ? layout[l++] : SWKBD_STDKEY_WIDTH;
            tb->h = SWKBD_STDKEY_HEIGHT;

            p_x += tb->w + SWKDB_KEY_SPACING;
            tb++;
        }

        // next row
        if (layout[l++] != 0) {
            ShowPrompt(false, "Oh shit %lu %lu", k, l);
            return false; // error!
        }
        p_y += SWKBD_STDKEY_HEIGHT + SWKDB_KEY_SPACING;
    }

    // set last touchbox zero (so the end can be detected)
    memset(tb, 0, sizeof(TouchBox));

    return true;
}

static void DrawKey(TouchBox* key, bool pressed, bool uppercase) {
    const char* keystrs[] = { SWKBD_KEYSTR };
    const u32 color = (pressed) ? COLOR_SWKBD_PRESSED : 
        (key->id == KEY_ENTER) ? COLOR_SWKBD_ENTER :
        ((key->id == KEY_CAPS) && uppercase) ? COLOR_SWKBD_CAPS :
        COLOR_SWKBD_NORMAL;

    char keystr[16];
    if (key->id >= 0x80) snprintf(keystr, 16, "%s", keystrs[key->id - 0x80]);
    else {
        keystr[0] = (uppercase) ? to_uppercase(key->id) : key->id;
        keystr[1] = 0;
    }

    const u32 width = GetDrawStringWidth(keystr);
    const u32 f_offs_x = (key->w - width) / 2;
    const u32 f_offs_y = (key->h - FONT_HEIGHT_EXT) / 2;

    DrawRectangle(BOT_SCREEN, key->x, key->y, key->w, key->h, color);
    DrawString(BOT_SCREEN, keystr, key->x + f_offs_x, key->y + f_offs_y, COLOR_SWKBD_TEXT, color, false);
}

static void DrawKeyBoard(TouchBox* swkbd, bool uppercase) {
    // calculate / draw bounding box
    u16 x0 = SCREEN_WIDTH_BOT, x1 = 0;
    u16 y0 = SCREEN_HEIGHT, y1 = 0;
    for (TouchBox* tb = swkbd; tb->id != 0; tb++) {
        if (tb->x < x0) x0 = tb->x;
        if (tb->y < y0) y0 = tb->y;
        if ((tb->x + tb->w) > x1) x1 = tb->x + tb->w;
        if ((tb->y + tb->h) > y1) y1 = tb->y + tb->h;
    }
    DrawRectangle(BOT_SCREEN, x0-1, y0-1, x1-x0+2, y1-y0+2, COLOR_SWKBD_BOX);

    // draw keyboard
    for (TouchBox* tb = swkbd; tb->id != 0; tb++) {
        DrawKey(tb, false, uppercase);
    }
}

static char KeyboardWait(TouchBox* swkbd, bool uppercase) {
    u32 id = 0;
    u16 x, y;

    // wait for touch input
    while(!(InputWait(0) & BUTTON_TOUCH));

    // process touch input
    while(HID_ReadTouchState(&x, &y)) {
        TouchBox* tb = TouchBoxGet(&id, x, y, swkbd, 0);
        if (tb) {
            DrawKey(tb, true, uppercase);
            while(HID_ReadTouchState(&x, &y) && (tb == TouchBoxGet(NULL, x, y, swkbd, 0)));
            DrawKey(tb, false, uppercase);
        }
    }

    return (uppercase) ? to_uppercase((char) id) : (char) id;
}

bool ShowKeyboard(char* inputstr, u32 max_size, const char *format, ...) {
    const char keys_alphabet[] = { SWKBD_KEYS_ALPHABET };
    const char keys_special[] = { SWKBD_KEYS_SPECIAL };
    const u8 layout_alphabet[] = { SWKBD_LAYOUT_ALPHABET };
    const u8 layout_special[] = { SWKBD_LAYOUT_SPECIAL };
    TouchBox swkbd_alphabet[64];
    TouchBox swkbd_special[32];

    // generate keyboards
    if (!BuildKeyboard(swkbd_alphabet, keys_alphabet, layout_alphabet)) return false;
    if (!BuildKeyboard(swkbd_special, keys_special, layout_special)) return false;

    // draw keyboard
    ClearScreen(BOT_SCREEN, COLOR_STD_BG);
    DrawKeyBoard(swkbd_alphabet, false);

    char teststr[512] = { 0 };
    char* ptr = teststr;
    DrawStringF(BOT_SCREEN, 40, 40, COLOR_STD_FONT, COLOR_STD_BG, "> %s", teststr);

    // handle keyboard
    u32 uppercase = 0; // 1 -> uppercase once, 2 -> uppercase always
    while (true) {
        char key = KeyboardWait(swkbd_alphabet, uppercase);
        if (key == KEY_CAPS) {
            uppercase = (uppercase + 1) % 3;
            if (uppercase < 2) DrawKeyBoard(swkbd_alphabet, uppercase);
        } else if (key == KEY_ENTER) {
            break;
        } else if (key == KEY_BKSPC) {
            if (ptr > teststr) *(--ptr) = ' '; // should this really be a space?
        } else if (key && (key < 0x80)) {
            *(ptr++) = key;
            *ptr = '\0';
            if (uppercase == 1) {
                uppercase = 0;
                DrawKeyBoard(swkbd_alphabet, uppercase);
            }
        }

        // update text
        if ((key && (key < 0x80)) || (key == KEY_BKSPC))
            DrawStringF(BOT_SCREEN, 40, 40, COLOR_STD_FONT, COLOR_STD_BG, "> %s", teststr);
    }

    ClearScreen(BOT_SCREEN, COLOR_STD_BG);
    return 0;
}