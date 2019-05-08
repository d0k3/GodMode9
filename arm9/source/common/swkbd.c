#include <stdarg.h>

#include "swkbd.h"
#include "ui.h"
#include "timer.h"
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

    // set up the textbox
    TouchBox* txtbox = swkbd;
    txtbox->x = (SCREEN_WIDTH_BOT - SWKBD_TEXTBOX_WIDTH) / 2;
    txtbox->y = p_y - 30;
    txtbox->w = SWKBD_TEXTBOX_WIDTH;
    txtbox->h = 30;
    txtbox->id = KEY_TXTBOX;

    // set button positions
    TouchBox* tb = swkbd + 1;
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
            return false; // error!!!! THIS HAS TO GO!
        }
        p_y += SWKBD_STDKEY_HEIGHT + SWKDB_KEY_SPACING;
    }

    // set last touchbox zero (so the end can be detected)
    memset(tb, 0, sizeof(TouchBox));

    return true;
}

static void DrawKey(const TouchBox* key, const bool pressed, const u32 uppercase) {
    const char* keystrs[] = { SWKBD_KEYSTR };
    const u32 color = (pressed) ? COLOR_SWKBD_PRESSED : 
        (key->id == KEY_ENTER) ? COLOR_SWKBD_ENTER :
        ((key->id == KEY_CAPS) && (uppercase > 1)) ? COLOR_SWKBD_CAPS :
        COLOR_SWKBD_NORMAL;

    // don't even try to draw the textbox
    if (key->id == KEY_TXTBOX) return;

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
    DrawString(BOT_SCREEN, keystr, key->x + f_offs_x, key->y + f_offs_y, COLOR_SWKBD_CHARS, color, false);
}

static void DrawKeyBoardBox(TouchBox* swkbd, u32 color) {
    // we need to make sure to skip the textbox here(first entry)

    // calculate / draw keyboard box
    u16 x0 = SCREEN_WIDTH_BOT, x1 = 0;
    u16 y0 = SCREEN_HEIGHT, y1 = 0;
    for (TouchBox* tb = swkbd + 1; tb->id != 0; tb++) {
        if (tb->x < x0) x0 = tb->x;
        if (tb->y < y0) y0 = tb->y;
        if ((tb->x + tb->w) > x1) x1 = tb->x + tb->w;
        if ((tb->y + tb->h) > y1) y1 = tb->y + tb->h;
    }
    DrawRectangle(BOT_SCREEN, 0, y0-1, SCREEN_WIDTH_BOT, y1-y0+2, COLOR_STD_BG);
    DrawRectangle(BOT_SCREEN, x0-1, y0-1, x1-x0+2, y1-y0+2, color);
}

static void DrawKeyBoard(TouchBox* swkbd, const u32 uppercase) {
    // we need to make sure to skip the textbox here(first entry)

    // draw keyboard
    for (TouchBox* tb = swkbd + 1; tb->id != 0; tb++) {
        DrawKey(tb, false, uppercase);
    }
}

static void DrawTextBox(const TouchBox* txtbox, const char* inputstr, const u32 cursor, u32* scroll) {
    const u32 input_shown = (txtbox->w / FONT_WIDTH_EXT) - 2;
    const u32 inputstr_size = strlen(inputstr); // we rely on a zero terminated string
    const u16 x = txtbox->x;
    const u16 y = txtbox->y; 
    
    // fix scroll
    if (cursor < *scroll) *scroll = cursor;
    else if (cursor - *scroll > input_shown) *scroll = cursor - input_shown;
        
    // draw input string
    DrawStringF(BOT_SCREEN, x, y, COLOR_STD_FONT, COLOR_STD_BG, "%c%-*.*s%-*.*s%c",
        (*scroll) ? '<' : '|',
        (inputstr_size > input_shown) ? input_shown : inputstr_size,
        (inputstr_size > input_shown) ? input_shown : inputstr_size,
        inputstr + *scroll,
        (inputstr_size > input_shown) ? 0 : input_shown - inputstr_size,
        (inputstr_size > input_shown) ? 0 : input_shown - inputstr_size,
        "",
        (inputstr_size - (s32) *scroll > input_shown) ? '>' : '|'
    );
    
    // draw cursor
    DrawStringF(BOT_SCREEN, x-(FONT_WIDTH_EXT/2), y+10, COLOR_STD_FONT, COLOR_STD_BG, "%-*.*s^%-*.*s",
        1 + cursor - *scroll,
        1 + cursor - *scroll,
        "",
        input_shown - (cursor - *scroll),
        input_shown - (cursor - *scroll),
        ""
    );
}

static void MoveTextBoxCursor(const TouchBox* txtbox, const char* inputstr, const u32 max_size, u32* cursor, u32* scroll) {
    const u32 input_shown = (txtbox->w / FONT_WIDTH_EXT) - 2;
    const u64 scroll_cooldown = 144;
    u64 timer = timer_start();
    u32 id = 0;
    u16 x, y;

    // process touch input
    while(HID_ReadTouchState(&x, &y)) {
        const TouchBox* tb = TouchBoxGet(&id, x, y, txtbox, 0);
        if (id == KEY_TXTBOX) {
            u16 x_tb = x - tb->x;
            u16 cpos = (x_tb < (FONT_WIDTH_EXT/2)) ? 0 : (x_tb - (FONT_WIDTH_EXT/2)) / FONT_WIDTH_EXT;
            u32 cursor_next = *scroll + ((cpos <= input_shown) ? cpos : input_shown);
            // move cursor to position pointed to
            if (*cursor != cursor_next) {
                if (cursor_next < max_size) *cursor = cursor_next;
                DrawTextBox(txtbox, inputstr, *cursor, scroll);
                timer = timer_start();
            }
            // move beyound visible field
            if (timer_msec(timer) >= scroll_cooldown) {
                if ((cpos == 0) && (*scroll > 0))
                    (*scroll)--;
                else if ((cpos >= input_shown) && (*cursor < (max_size-1)))
                    (*scroll)++;
            }
        }
    }
}

static char KeyboardWait(TouchBox* swkbd, bool uppercase) {
    u32 id = 0;
    u16 x, y;

    // wait for touch input (handle key input, too)
    while (true) {
        u32 pressed = InputWait(0);
        if (pressed & BUTTON_B) return KEY_ESCAPE;
        else if (pressed & BUTTON_A) return KEY_ENTER;
        else if (pressed & BUTTON_X) return KEY_BKSPC;
        else if (pressed & BUTTON_Y) return KEY_INSERT;
        else if (pressed & BUTTON_R1) return KEY_CAPS;
        else if (pressed & BUTTON_RIGHT) return KEY_RIGHT;
        else if (pressed & BUTTON_LEFT) return KEY_LEFT;
        else if (pressed & BUTTON_TOUCH) break;
    }

    // process touch input
    while(HID_ReadTouchState(&x, &y)) {
        const TouchBox* tb = TouchBoxGet(&id, x, y, swkbd, 0);
        if (tb) {
            if (id == KEY_TXTBOX) break; // immediately break on textbox
            DrawKey(tb, true, uppercase);
            while(HID_ReadTouchState(&x, &y) && (tb == TouchBoxGet(NULL, x, y, swkbd, 0)));
            DrawKey(tb, false, uppercase);
        }
    }

    return (uppercase) ? to_uppercase((char) id) : (char) id;
}

bool ShowKeyboard(char* inputstr, const u32 max_size, const char *format, ...) {
    static const char keys_alphabet[] = { SWKBD_KEYS_ALPHABET };
    static const char keys_special[] = { SWKBD_KEYS_SPECIAL };
    static const char keys_numpad[] = { SWKBD_KEYS_NUMPAD };
    static const u8 layout_alphabet[] = { SWKBD_LAYOUT_ALPHABET };
    static const u8 layout_special[] = { SWKBD_LAYOUT_SPECIAL };
    static const u8 layout_numpad[] = { SWKBD_LAYOUT_NUMPAD };
    TouchBox swkbd_alphabet[64];
    TouchBox swkbd_special[32];
    TouchBox swkbd_numpad[32];
    TouchBox* textbox = swkbd_alphabet; // always use this textbox

    // generate keyboards
    if (!BuildKeyboard(swkbd_alphabet, keys_alphabet, layout_alphabet)) return false;
    if (!BuildKeyboard(swkbd_special, keys_special, layout_special)) return false;
    if (!BuildKeyboard(swkbd_numpad, keys_numpad, layout_numpad)) return false;

    // (instructional) text
    char str[512] = { 0 }; // arbitrary limit, should be more than enough
    va_list va;
    va_start(va, format);
    vsnprintf(str, 512, format, va);
    va_end(va);
    u32 str_width = GetDrawStringWidth(str);
    if (str_width < (24 * FONT_WIDTH_EXT)) str_width = 24 * FONT_WIDTH_EXT;
    u32 str_x = (str_width >= SCREEN_WIDTH_BOT) ? 0 : (SCREEN_WIDTH_BOT - str_width) / 2;
    ClearScreen(BOT_SCREEN, COLOR_STD_BG);
    DrawStringF(BOT_SCREEN, str_x, 20, COLOR_STD_FONT, COLOR_STD_BG, "%s", str);

    // handle keyboard
    u32 uppercase = 0; // 1 -> uppercase once, 2 -> uppercase always
    u32 scroll = 0;
    u32 cursor = 0;
    u32 inputstr_size = strnlen(inputstr, max_size);
    TouchBox* swkbd_prev = NULL;
    TouchBox* swkbd = swkbd_alphabet;
    bool ret = false;
    while (true) {
        // draw keyboard if required
        if (swkbd != swkbd_prev) {
            DrawKeyBoardBox(swkbd, COLOR_SWKBD_BOX);
            DrawKeyBoard(swkbd, uppercase);
            DrawTextBox(textbox, inputstr, cursor, &scroll);
            swkbd_prev = swkbd;
        }

        // handle user input
        char key = KeyboardWait(swkbd, uppercase);
        if (key == KEY_INSERT) key = ' '; // impromptu replacement
        if (key == KEY_TXTBOX) {
            MoveTextBoxCursor(textbox, inputstr, max_size, &cursor, &scroll);
        } else if (key == KEY_CAPS) {
            uppercase = (uppercase + 1) % 3;
            DrawKeyBoard(swkbd, uppercase);
            continue;
        } else if (key == KEY_ENTER) {
            ret = true;
            break;
        } else if (key == KEY_ESCAPE) {
            break;
        } else if (key == KEY_BKSPC) {
            if (cursor) {
                memmove(inputstr + cursor - 1, inputstr + cursor, max_size - cursor);
                cursor--;
                inputstr_size--;
            }
        } else if (key == KEY_LEFT) {
            if (cursor) cursor--;
        } else if (key == KEY_RIGHT) {
            if (cursor < (max_size-1)) cursor++;
        } else if (key == KEY_ALPHA) {
            swkbd = swkbd_alphabet;
        } else if (key == KEY_SPECIAL) {
            swkbd = swkbd_special;
        } else if (key == KEY_NUMPAD) {
            swkbd = swkbd_numpad;
        } else if (key && (key < 0x80)) {
            if ((cursor < (max_size-1)) && (inputstr_size < max_size)) {
                // pad string (if cursor beyound string size)
                while (inputstr_size < cursor) {
                    inputstr[inputstr_size++] = ' ';
                    inputstr[inputstr_size] = '\0';
                }
                // make room
                if (inputstr_size < (max_size-1)) { // only if there is still room
                    memmove(inputstr + cursor + 1, inputstr + cursor, max_size - cursor - 1);
                    inputstr_size++;
                }
                // insert char
                inputstr[cursor++] = key;
            }
            if (uppercase == 1) {
                uppercase = 0;
                DrawKeyBoard(swkbd, uppercase);
            }
        }

        // update text
        DrawTextBox(textbox, inputstr, cursor, &scroll);
    }

    ClearScreen(BOT_SCREEN, COLOR_STD_BG);
    return ret;
}