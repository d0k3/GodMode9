#include "godmode.h"
#include "ui.h"
#include "hid.h"
#include "fs.h"
#include "platform.h"
#include "nand.h"
#include "virtual.h"
#include "image.h"
#include "store.h"

#define VERSION "0.6.3"

#define N_PANES 2
#define IMG_DRV "789I"

#define WORK_BUFFER     ((u8*)0x21100000)

#define COLOR_TOP_BAR   ((GetWritePermissions() & (PERM_A9LH&~PERM_SYSNAND)) ? COLOR_DARKRED : (GetWritePermissions() & PERM_SYSNAND) ? COLOR_RED : (GetWritePermissions() & PERM_MEMORY) ? COLOR_BRIGHTBLUE : (GetWritePermissions() & (PERM_EMUNAND|PERM_IMAGE)) ? COLOR_BRIGHTYELLOW : GetWritePermissions() ? COLOR_BRIGHTGREEN : COLOR_WHITE)   
#define COLOR_SIDE_BAR  COLOR_DARKGREY
#define COLOR_MARKED    COLOR_TINTEDYELLOW
#define COLOR_FILE      COLOR_TINTEDGREEN
#define COLOR_DIR       COLOR_TINTEDBLUE
#define COLOR_ROOT      COLOR_GREY
#define COLOR_ENTRY(e)  (((e)->marked) ? COLOR_MARKED : ((e)->type == T_DIR) ? COLOR_DIR : ((e)->type == T_FILE) ? COLOR_FILE : ((e)->type == T_ROOT) ?  COLOR_ROOT : COLOR_GREY)

#define COLOR_HVOFFS    RGB(0x40, 0x60, 0x50)
#define COLOR_HVOFFSI   COLOR_DARKESTGREY
#define COLOR_HVASCII   RGB(0x40, 0x80, 0x50)
#define COLOR_HVHEX(i)  ((i % 2) ? RGB(0x30, 0x90, 0x30) : RGB(0x30, 0x80, 0x30))

typedef struct {
    char path[256];
    u32 cursor;
    u32 scroll;
} PaneData;

void DrawUserInterface(const char* curr_path, DirEntry* curr_entry, DirStruct* clipboard, u32 curr_pane) {
    const u32 n_cb_show = 8;
    const u32 bartxt_start = (FONT_HEIGHT_EXT == 10) ? 1 : 2;
    const u32 bartxt_x = 2;
    const u32 bartxt_rx = SCREEN_WIDTH_TOP - (19*FONT_WIDTH_EXT) - bartxt_x;
    const u32 info_start = 18;
    const u32 instr_x = (SCREEN_WIDTH_TOP - (36*FONT_WIDTH_EXT)) / 2;
    char tempstr[64];
    
    static u32 state_prev = 0xFFFFFFFF;
    u32 state_curr =
        ((*curr_path) ? (1<<0) : 0) |
        ((clipboard->n_entries) ? (1<<1) : 0) |
        (GetMountState()<<2) |
        (curr_pane<<4);
    
    if (state_prev != state_curr) {
        ClearScreenF(true, false, COLOR_STD_BG);
        state_prev = state_curr;
    }
    
    // top bar - current path & free/total storage
    DrawRectangle(TOP_SCREEN, 0, 0, SCREEN_WIDTH_TOP, 12, COLOR_TOP_BAR);
    if (strncmp(curr_path, "", 256) != 0) {
        char bytestr0[32];
        char bytestr1[32];
        TruncateString(tempstr, curr_path, 30, 8);
        DrawStringF(TOP_SCREEN, bartxt_x, bartxt_start, COLOR_STD_BG, COLOR_TOP_BAR, tempstr);
        DrawStringF(TOP_SCREEN, bartxt_rx, bartxt_start, COLOR_STD_BG, COLOR_TOP_BAR, "%19.19s", "LOADING...");
        FormatBytes(bytestr0, GetFreeSpace(curr_path));
        FormatBytes(bytestr1, GetTotalSpace(curr_path));
        snprintf(tempstr, 64, "%s/%s", bytestr0, bytestr1);
        DrawStringF(TOP_SCREEN, bartxt_rx, bartxt_start, COLOR_STD_BG, COLOR_TOP_BAR, "%19.19s", tempstr);
    } else {
        DrawStringF(TOP_SCREEN, bartxt_x, bartxt_start, COLOR_STD_BG, COLOR_TOP_BAR, "[root]");
        DrawStringF(TOP_SCREEN, bartxt_rx, bartxt_start, COLOR_STD_BG, COLOR_TOP_BAR, "%19.19s", "GodMode9");
    }
    
    // left top - current file info
    if (curr_pane) snprintf(tempstr, 63, "PANE #%lu", curr_pane);
    else snprintf(tempstr, 63, "CURRENT");
    DrawStringF(TOP_SCREEN, 2, info_start, COLOR_STD_FONT, COLOR_STD_BG, "[%s]", tempstr);
    // file / entry name
    ResizeString(tempstr, curr_entry->name, 20, 8, false);
    u32 color_current = COLOR_ENTRY(curr_entry);
    DrawStringF(TOP_SCREEN, 4, info_start + 12, color_current, COLOR_STD_BG, "%s", tempstr);
    // size (in Byte) or type desc
    if (curr_entry->type == T_DIR) {
        ResizeString(tempstr, "(dir)", 20, 8, false);
    } else if (curr_entry->type == T_DOTDOT) {
        snprintf(tempstr, 21, "%20s", "");
    } else {
        char numstr[32];
        char bytestr[32];
        FormatNumber(numstr, curr_entry->size);
        snprintf(bytestr, 31, "%s Byte", numstr);
        ResizeString(tempstr, bytestr, 20, 8, false);
    }
    DrawStringF(TOP_SCREEN, 4, info_start + 12 + 10, color_current, COLOR_STD_BG, tempstr);
    
    // right top - clipboard
    DrawStringF(TOP_SCREEN, SCREEN_WIDTH_TOP - (20*FONT_WIDTH_EXT), info_start, COLOR_STD_FONT, COLOR_STD_BG, "%20s", (clipboard->n_entries) ? "[CLIPBOARD]" : "");
    for (u32 c = 0; c < n_cb_show; c++) {
        u32 color_cb = COLOR_ENTRY(&(clipboard->entry[c]));
        ResizeString(tempstr, (clipboard->n_entries > c) ? clipboard->entry[c].name : "", 20, 8, true);
        DrawStringF(TOP_SCREEN, SCREEN_WIDTH_TOP - (20*FONT_WIDTH_EXT) - 4, info_start + 12 + (c*10), color_cb, COLOR_STD_BG, tempstr);
    }
    *tempstr = '\0';
    if (clipboard->n_entries > n_cb_show) snprintf(tempstr, 60, "+ %lu more", clipboard->n_entries - n_cb_show);
    DrawStringF(TOP_SCREEN, SCREEN_WIDTH_TOP - (20*FONT_WIDTH_EXT) - 4, info_start + 12 + (n_cb_show*10), COLOR_DARKGREY, COLOR_STD_BG, "%20s", tempstr);
    
    // bottom: inctruction block
    char instr[256];
    snprintf(instr, 256, "%s%s\n%s%s%s%s%s%s",
        #ifndef SAFEMODE
        "GodMode9 Explorer v", VERSION, // generic start part
        #else
        "SafeMode9 Explorer v", VERSION, // generic start part
        #endif
        (*curr_path) ? ((clipboard->n_entries == 0) ? "L - MARK files (use with \x18\x19\x1A\x1B)\nX - DELETE / [+R] RENAME file(s)\nY - COPY file(s) / [+R] CREATE dir\n" :
        "L - MARK files (use with \x18\x19\x1A\x1B)\nX - DELETE / [+R] RENAME file(s)\nY - PASTE file(s) / [+R] CREATE dir\n") :
        ((GetWritePermissions() > PERM_BASE) ? "R+Y - Relock write permissions\nR+B - Unmount SD card\n" :
        "R+Y - Unlock write permissions\nR+B - Unmount SD card\n"),
        (*curr_path) ? "" : ((GetMountState() == IMG_RAMDRV) ? "R+X - Unmount RAM drive\n" :
        (GetMountState()) ? "R+X - Unmount image\n" : "R+X - Mount RAM drive\n"),
        "R+L - Make a Screenshot\n",
        "R+\x1B\x1A - Switch to prev/next pane\n",
        (clipboard->n_entries) ? "SELECT - Clear Clipboard\n" : "SELECT - Restore Clipboard\n", // only if clipboard is full
        "START - Reboot / [+R] Poweroff"); // generic end part
    DrawStringF(TOP_SCREEN, instr_x, SCREEN_HEIGHT - 4 - GetDrawStringHeight(instr), COLOR_STD_FONT, COLOR_STD_BG, instr);
}

void DrawDirContents(DirStruct* contents, u32 cursor, u32* scroll) {
    const int str_width = (SCREEN_WIDTH_BOT-1) / FONT_WIDTH_EXT;
    const u32 bar_height_min = 32;
    const u32 bar_width = 2;
    const u32 start_y = 2;
    const u32 stp_y = 12;
    const u32 pos_x = 0;
    const u32 lines = (SCREEN_HEIGHT-start_y+stp_y-1) / stp_y;
    u32 pos_y = start_y;
     
    if (*scroll > cursor) *scroll = cursor;
    else if (*scroll + lines <= cursor) *scroll = cursor - lines + 1;
    if (*scroll + lines > contents->n_entries)
        *scroll = (contents->n_entries > lines) ? contents->n_entries - lines : 0;
    
    for (u32 i = 0; pos_y < SCREEN_HEIGHT; i++) {
        char tempstr[str_width + 1];
        u32 offset_i = *scroll + i;
        u32 color_font = COLOR_WHITE;
        if (offset_i < contents->n_entries) {
            DirEntry* curr_entry = &(contents->entry[offset_i]);
            char namestr[str_width - 10 + 1];
            char bytestr[10 + 1];
            color_font = (cursor != offset_i) ? COLOR_ENTRY(curr_entry) : COLOR_STD_FONT;
            FormatBytes(bytestr, curr_entry->size);
            ResizeString(namestr, curr_entry->name, str_width - 10, str_width - 20, false);
            snprintf(tempstr, str_width + 1, "%s%10.10s", namestr,
                (curr_entry->type == T_DIR) ? "(dir)" : (curr_entry->type == T_DOTDOT) ? "(..)" : bytestr);
        } else snprintf(tempstr, str_width + 1, "%-*.*s", str_width, str_width, "");
        DrawStringF(BOT_SCREEN, pos_x, pos_y, color_font, COLOR_STD_BG, tempstr);
        pos_y += stp_y;
    }
    
    if (contents->n_entries > lines) { // draw position bar at the right      
        u32 bar_height = (lines * SCREEN_HEIGHT) / contents->n_entries;
        if (bar_height < bar_height_min) bar_height = bar_height_min;
        u32 bar_pos = ((u64) *scroll * (SCREEN_HEIGHT - bar_height)) / (contents->n_entries - lines);
        
        DrawRectangle(BOT_SCREEN, SCREEN_WIDTH_BOT - bar_width, 0, bar_width, bar_pos, COLOR_STD_BG);
        DrawRectangle(BOT_SCREEN, SCREEN_WIDTH_BOT - bar_width, bar_pos + bar_height, bar_width, SCREEN_WIDTH_BOT - (bar_pos + bar_height), COLOR_STD_BG);
        DrawRectangle(BOT_SCREEN, SCREEN_WIDTH_BOT - bar_width, bar_pos, bar_width, bar_height, COLOR_SIDE_BAR);
    } else DrawRectangle(BOT_SCREEN, SCREEN_WIDTH_BOT - bar_width, 0, bar_width, SCREEN_HEIGHT, COLOR_STD_BG);
}

u32 SdFormatMenu(void) {
    const u32 emunand_size_table[6] = { 0x0, 0x0, 0x3AF, 0x4D8, 0x3FF, 0x7FF };
    const char* optionstr[6] = { "No EmuNAND", "O3DS NAND size", "N3DS NAND size", "1GB (legacy size)", "2GB (legacy size)", "User input..." };
    u64 sdcard_size_mb = 0;
    u64 emunand_size_mb = (u64) -1;
    
    // check actual SD card size
    sdcard_size_mb = GetSDCardSize() / 0x100000;
    if (!sdcard_size_mb) {
        ShowPrompt(false, "ERROR: SD card not detected.");
        return 1;
    }
    
    u32 user_select = ShowSelectPrompt(6, optionstr, "Format SD card (%lluMB)?\nChoose EmuNAND size:", sdcard_size_mb);
    if (user_select && (user_select < 6)) {
        emunand_size_mb = emunand_size_table[user_select];
    } else if (user_select == 6) do {
        emunand_size_mb = ShowNumberPrompt(0, "SD card size is %lluMB.\nEnter EmuNAND size (MB) below:", sdcard_size_mb);
        if (emunand_size_mb == (u64) -1) break;
    } while (emunand_size_mb > sdcard_size_mb);
    if (emunand_size_mb == (u64) -1) return 1;
    
    if (!FormatSDCard((u32) emunand_size_mb)) {
        ShowPrompt(false, "Format SD: failed!");
        return 1;
    }
    
    if (*(vu32*) 0x101401C0 == 0) {
        InitSDCardFS(); // on A9LH: copy the payload from mem to SD root
        FileSetData("0:/arm9loaderhax.bin", (u8*) 0x23F00000, 0x40000, 0, true);
        DeinitSDCardFS();
    }
    
    return 0;
}

u32 HexViewer(const char* path) {
    static const u32 max_data = (SCREEN_HEIGHT / 8) * 16;
    static u32 mode = 0;
    u8* data = WORK_BUFFER;
    u8* bottom_cpy = WORK_BUFFER + 0xC0000; // a copy of the bottom screen framebuffer
    u32 fsize = FileGetSize(path);
    
    bool dual_screen;
    int x_off, x_hex, x_ascii;
    u32 vpad, hlpad, hrpad;
    u32 rows, cols;
    u32 total_shown;
    u32 total_data;
    
    u32 last_mode = 0xFF;
    u32 last_offset = (u32) -1;
    u32 offset = 0;
    
    u32 found_offset = (u32) -1;
    u32 found_size = 0;
    
    static const u32 edit_bsize = 0x4000; // should be multiple of 0x200 * 2
    bool edit_mode = false;
    u8* edit_buffer = WORK_BUFFER;
    u8* edit_buffer_cpy = WORK_BUFFER + edit_bsize;
    u32 edit_start;
    int cursor = 0;
    
    memcpy(bottom_cpy, BOT_SCREEN, (SCREEN_HEIGHT * SCREEN_WIDTH_BOT * 3));
    
    while (true) {
        if (mode != last_mode) {
            switch (mode) { // display mode
                #ifdef FONT_6X10
                case 1:
                    vpad = 0;
                    hlpad = hrpad = 1;
                    cols = 16;
                    x_off = 0;
                    x_ascii = SCREEN_WIDTH_TOP - (FONT_WIDTH_EXT * cols);
                    x_hex = x_off + (8*FONT_WIDTH_EXT) + 16;
                    dual_screen = false;
                    break;
                default:
                    mode = 0; 
                    vpad = 0;
                    hlpad = hrpad = 3;
                    cols = 8;
                    x_off = 30 + (SCREEN_WIDTH_TOP - SCREEN_WIDTH_BOT) / 2;
                    x_ascii = SCREEN_WIDTH_TOP - x_off - (FONT_WIDTH_EXT * cols);
                    x_hex = (SCREEN_WIDTH_TOP - ((hlpad + (2*FONT_WIDTH_EXT) + hrpad) * cols)) / 2;
                    dual_screen = true;
                    break;
                #else
                case 1:
                    vpad = hlpad = hrpad = 1;
                    cols = 12;
                    x_off = 0;
                    x_ascii = SCREEN_WIDTH_TOP - (8 * cols);
                    x_hex = x_off + (8*8) + 12;
                    dual_screen = false;
                    break;
                case 2:
                    vpad = 1;
                    hlpad = 0;
                    hrpad = 1;
                    cols = 16;
                    x_off = -1;
                    x_ascii = SCREEN_WIDTH_TOP - (8 * cols);
                    x_hex = 0;
                    dual_screen = false;
                    break;
                case 3:
                    vpad = hlpad = hrpad = 1;
                    cols = 16;
                    x_off = 20;
                    x_ascii = -1;
                    x_hex = x_off + (8*8) + 12;
                    dual_screen = false;
                    break;
                default:
                    mode = 0; 
                    vpad = hlpad = hrpad = 2;
                    cols = 8;
                    x_off = (SCREEN_WIDTH_TOP - SCREEN_WIDTH_BOT) / 2;
                    x_ascii = SCREEN_WIDTH_TOP - x_off - (8 * cols);
                    x_hex = (SCREEN_WIDTH_TOP - ((hlpad + 16 + hrpad) * cols)) / 2;
                    dual_screen = true;
                    break;
                #endif
            }
            rows = (dual_screen ? 2 : 1) * SCREEN_HEIGHT / (FONT_HEIGHT_EXT + (2*vpad));
            total_shown = rows * cols;
            last_mode = mode;
            ClearScreenF(true, dual_screen, COLOR_STD_BG);
            if (!dual_screen) memcpy(BOT_SCREEN, bottom_cpy, (SCREEN_HEIGHT * SCREEN_WIDTH_BOT * 3));
        }
        // fix offset (if required)
        if (offset % cols) offset -= (offset % cols); // fix offset (align to cols)
        if (offset + total_shown > fsize + cols) // if offset too big
            offset = (total_shown > fsize) ? 0 : (fsize + cols - total_shown - (fsize % cols));
        // get data, using max data size (if new offset)
        if (offset != last_offset) {
            if (!edit_mode) {
                total_data = FileGetData(path, data, max_data, offset);
            } else { // edit mode - read from memory
                if ((offset < edit_start) || (offset + max_data > edit_start + edit_bsize))
                    offset = last_offset; // we don't expect this to happen
                total_data = (fsize - offset >= max_data) ? max_data : fsize - offset;
                data = edit_buffer + (offset - edit_start);
            }
            last_offset = offset;
        }
        
        // display data on screen
        for (u32 row = 0; row < rows; row++) {
            char ascii[16 + 1] = { 0 };
            u32 y = row * (FONT_HEIGHT_EXT + (2*vpad)) + vpad;
            u32 curr_pos = row * cols;
            u32 cutoff = (curr_pos >= total_data) ? 0 : (total_data >= curr_pos + cols) ? cols : total_data - curr_pos;
            u32 marked0 = (found_size && (offset <= found_offset)) ? found_offset - offset : 0;
            u32 marked1 = marked0 + found_size;
            u8* screen = TOP_SCREEN;
            u32 x0 = 0;
            
            // fix marked0 / marked1 offsets for current row
            marked0 = (marked0 < curr_pos) ? 0 : (marked0 >= curr_pos + cols) ? cols : marked0 - curr_pos;
            marked1 = (marked1 < curr_pos) ? 0 : (marked1 >= curr_pos + cols) ? cols : marked1 - curr_pos;
            
            if (y >= SCREEN_HEIGHT) { // switch to bottom screen
                y -= SCREEN_HEIGHT;
                screen = BOT_SCREEN;
                x0 = 40;
            }
            
            memcpy(ascii, data + curr_pos, cols);
            for (u32 col = 0; col < cols; col++)
                if ((col >= cutoff) || (ascii[col] == 0x00)) ascii[col] = ' ';
            
            // draw offset / ASCII representation
            if (x_off >= 0) DrawStringF(screen, x_off - x0, y, cutoff ? COLOR_HVOFFS : COLOR_HVOFFSI,
                COLOR_STD_BG, "%08X", (unsigned int) offset + curr_pos);
            if (x_ascii >= 0) {
                DrawString(screen, ascii, x_ascii - x0, y, COLOR_HVASCII, COLOR_STD_BG);
                for (u32 i = marked0; i < marked1; i++)
                    DrawCharacter(screen, ascii[i % cols], x_ascii - x0 + (FONT_WIDTH_EXT * i), y, COLOR_MARKED, COLOR_STD_BG);
                if (edit_mode && ((u32) cursor / cols == row)) DrawCharacter(screen, ascii[cursor % cols],
                    x_ascii - x0 + FONT_WIDTH_EXT * (cursor % cols), y, COLOR_RED, COLOR_STD_BG);
            }
            
            // draw HEX values
            for (u32 col = 0; (col < cols) && (x_hex >= 0); col++) {
                u32 x = (x_hex + hlpad) + (((2*FONT_WIDTH_EXT) + hrpad + hlpad) * col) - x0;
                u32 hex_color = (edit_mode && ((u32) cursor == curr_pos + col)) ? COLOR_RED :
                    ((col >= marked0) && (col < marked1)) ? COLOR_MARKED : COLOR_HVHEX(col);
                if (col < cutoff)
                    DrawStringF(screen, x, y, hex_color, COLOR_STD_BG, "%02X", (unsigned int) data[curr_pos + col]);
                else DrawStringF(screen, x, y, hex_color, COLOR_STD_BG, "  ");
            }
        }
        
        // handle user input
        u32 pad_state = InputWait();
        if ((pad_state & BUTTON_R1) && (pad_state & BUTTON_L1)) CreateScreenshot();
        else if (!edit_mode) { // standard viewer mode
            u32 step_ud = (pad_state & BUTTON_R1) ? (0x1000  - (0x1000  % cols)) : cols;
            u32 step_lr = (pad_state & BUTTON_R1) ? (0x10000 - (0x10000 % cols)) : total_shown;
            if (pad_state & BUTTON_DOWN) offset += step_ud;
            else if (pad_state & BUTTON_RIGHT) offset += step_lr;
            else if (pad_state & BUTTON_UP) offset = (offset > step_ud) ? offset - step_ud : 0;
            else if (pad_state & BUTTON_LEFT) offset = (offset > step_lr) ? offset - step_lr : 0;
            else if ((pad_state & BUTTON_R1) && (pad_state & BUTTON_Y)) mode++;
            else if (pad_state & BUTTON_A) edit_mode = true;
            else if (pad_state & (BUTTON_B|BUTTON_START)) break;
            else if (found_size && (pad_state & BUTTON_R1) && (pad_state & BUTTON_X)) {
                u8 data[64] = { 0 };
                FileGetData(path, data, found_size, found_offset);
                found_offset = FileFindData(path, data, found_size, found_offset + 1);
                ClearScreenF(true, false, COLOR_STD_BG);
                if (found_offset == (u32) -1) {
                    ShowPrompt(false, "Not found!");
                    found_size = 0;
                } else offset = found_offset;
            } else if (pad_state & BUTTON_X) {
                const char* optionstr[3] = { "Go to offset", "Search for string", "Search for data" };
                u32 user_select = ShowSelectPrompt(3, optionstr, "Current offset: %08X\nSelect action:", 
                    (unsigned int) offset);
                if (user_select == 1) { // -> goto offset
                    u64 new_offset = ShowHexPrompt(offset, 8, "Current offset: %08X\nEnter new offset below.",
                        (unsigned int) offset);
                    if (new_offset != (u64) -1) offset = new_offset;
                } else if (user_select == 2) {
                    char string[64 + 1] = { 0 };
                    if (found_size) FileGetData(path, (u8*) string, (found_size <= 64) ? found_size : 64, found_offset);
                    if (ShowStringPrompt(string, 64 + 1, "Enter search string below.\n(R+X to repeat search)", (unsigned int) offset)) {
                        found_size = strnlen(string, 64);
                        found_offset = FileFindData(path, (u8*) string, found_size, offset);
                        ClearScreenF(true, false, COLOR_STD_BG);
                        if (found_offset == (u32) -1) {
                            ShowPrompt(false, "Not found!");
                            found_size = 0;
                        } else offset = found_offset;
                    }
                } else if (user_select == 3) {
                    u8 data[64] = { 0 };
                    u32 size = 0;
                    if (found_size) size = FileGetData(path, data, (found_size <= 64) ? found_size : 64, found_offset);
                    if (ShowDataPrompt(data, &size, "Enter search data below.\n(R+X to repeat search)", (unsigned int) offset)) {
                        found_size = size;
                        found_offset = FileFindData(path, data, size, offset);
                        ClearScreenF(true, false, COLOR_STD_BG);
                        if (found_offset == (u32) -1) {
                            ShowPrompt(false, "Not found!");
                            found_size = 0;
                        } else offset = found_offset;
                    }
                }
            }
            if (edit_mode && CheckWritePermissions(path)) { // setup edit mode
                found_size = 0;
                found_offset = (u32) -1;
                cursor = 0;
                edit_start = ((offset - (offset % 0x200) <= (edit_bsize / 2)) || (fsize < edit_bsize)) ? 0 : 
                    offset - (offset % 0x200) - (edit_bsize / 2);
                FileGetData(path, edit_buffer, edit_bsize, edit_start);
                memcpy(edit_buffer_cpy, edit_buffer, edit_bsize);
                data = edit_buffer + (offset - edit_start);
            } else edit_mode = false;
        } else { // editor mode
            if (pad_state & (BUTTON_B|BUTTON_START)) {
                edit_mode = false;
                // check for user edits
                u32 diffs = 0;
                for (u32 i = 0; i < edit_bsize; i++) if (edit_buffer[i] != edit_buffer_cpy[i]) diffs++;
                if (diffs && ShowPrompt(true, "You made edits in %i place(s).\nWrite changes to file?", diffs))
                    if (!FileSetData(path, edit_buffer, min(edit_bsize, (fsize - edit_start)), edit_start, false))
                        ShowPrompt(false, "Failed writing to file!");
                data = WORK_BUFFER;
                last_offset = (u32) -1; // force reload from file
            } else if (pad_state & BUTTON_A) {
                if (pad_state & BUTTON_DOWN) data[cursor]--;
                else if (pad_state & BUTTON_UP) data[cursor]++;
                else if (pad_state & BUTTON_RIGHT) data[cursor] += 0x10;
                else if (pad_state & BUTTON_LEFT) data[cursor] -= 0x10;
            } else {
                if (pad_state & BUTTON_DOWN) cursor += cols;
                else if (pad_state & BUTTON_UP) cursor -= cols;
                else if (pad_state & BUTTON_RIGHT) cursor++;
                else if (pad_state & BUTTON_LEFT) cursor--;
                // fix cursor position
                if (cursor < 0) {
                    if (offset >= cols) {
                        offset -= cols;
                        cursor += cols;
                    } else cursor = 0;
                } else if (((u32) cursor >= total_data) && (total_data < total_shown)) {
                    cursor = total_data - 1;
                } else if ((u32) cursor >= total_shown) {
                    if (offset + total_shown == fsize) {
                        cursor = total_shown - 1;
                    } else {
                        offset += cols;
                        cursor = (offset + cursor >= fsize) ? fsize - offset - 1 : cursor - cols;
                    }
                }
            }
        }
    }
    
    ClearScreenF(true, false, COLOR_STD_BG);
    memcpy(BOT_SCREEN, bottom_cpy, (SCREEN_HEIGHT * SCREEN_WIDTH_BOT * 3));
    return 0;
}

u32 GodMode() {
    static const u32 quick_stp = 20;
    u32 exit_mode = GODMODE_EXIT_REBOOT;
    
    // reserve 480kB for DirStruct, 64kB for PaneData, just to be safe
    static DirStruct* current_dir = (DirStruct*) 0x21000000;
    static DirStruct* clipboard   = (DirStruct*) 0x21078000;
    static PaneData* panedata     = (PaneData*)  0x210F0000;
    PaneData* pane = panedata;
    char current_path[256] = { 0x00 };
    
    int mark_setting = -1;
    u32 last_clipboard_size = 0;
    u32 cursor = 0;
    u32 scroll = 0;
    
    ClearScreenF(true, true, COLOR_STD_BG);
    if ((sizeof(DirStruct) > 0x78000) || (N_PANES * sizeof(PaneData) > 0x10000)) {
        ShowPrompt(false, "Out of memory!"); // just to be safe
        return exit_mode;
    }
    while (!InitSDCardFS()) {
        if (!ShowPrompt(true, "Initialising SD card failed! Retry?"))
            return exit_mode;
    }
    InitEmuNandBase();
    InitNandCrypto();
    InitExtFS();
    
    // could also check for a9lh via this: ((*(vu32*) 0x101401C0) == 0) 
    if ((GetUnitPlatform() == PLATFORM_N3DS) && !CheckSlot0x05Crypto()) {
        if (!ShowPrompt(true, "Warning: slot0x05 crypto fail!\nCould not set up slot0x05keyY.\nContinue?")) {
            DeinitExtFS();
            MountImage(NULL);
            DeinitSDCardFS();
            return exit_mode;
        }
    }
    
    GetDirContents(current_dir, "");
    clipboard->n_entries = 0;
    memset(panedata, 0x00, 0x10000);
    while (true) { // this is the main loop
        // basic sanity checking
        if (!current_dir->n_entries) { // current dir is empty -> revert to root
            *current_path = '\0';
            GetDirContents(current_dir, current_path);
            cursor = 0;
            if (!current_dir->n_entries) { // should not happen, if it does fail gracefully
                ShowPrompt(false, "Invalid directory object");
                return exit_mode;
            }
        }
        if (cursor >= current_dir->n_entries) // cursor beyond allowed range
            cursor = current_dir->n_entries - 1;
        DirEntry* curr_entry = &(current_dir->entry[cursor]);
        DrawUserInterface(current_path, curr_entry, clipboard, N_PANES ? pane - panedata + 1 : 0);
        DrawDirContents(current_dir, cursor, &scroll);
        u32 pad_state = InputWait();
        bool switched = (pad_state & BUTTON_R1);
        if (!(*current_path) || switched || !(pad_state & BUTTON_L1)) {
            mark_setting = -1;
        }
        
        // basic navigation commands
        if ((pad_state & BUTTON_A) && (curr_entry->type != T_FILE) && (curr_entry->type != T_DOTDOT)) { // for dirs
            if (switched) { // search directory
                char searchstr[256];
                char namestr[20+1];
                snprintf(searchstr, 256, "*.*");
                TruncateString(namestr, curr_entry->name, 20, 8);
                if (ShowStringPrompt(searchstr, 256, "Search %s?\nEnter search below.", namestr)) {
                    ShowString("Searching path, please wait...");
                    snprintf(current_path, 256, "Z:");
                    SearchDirContents(current_dir, curr_entry->path, searchstr, true);
                    StoreDirContents(current_dir);
                }
            } else { // one level up
                strncpy(current_path, curr_entry->path, 256);
                GetDirContents(current_dir, current_path);
                if (*current_path && (current_dir->n_entries > 1)) {
                    cursor = 1;
                    scroll = 0;
                } else cursor = 0;
            }
        } else if ((pad_state & BUTTON_A) && (curr_entry->type == T_FILE)) { // process a file
            u32 file_type = IdentifyImage(curr_entry->path);
            bool injectable = (clipboard->n_entries == 1) &&
                (clipboard->entry[0].type == T_FILE) &&
                (PathToNumFS(clipboard->entry[0].path) >= 0) &&
                (strncmp(clipboard->entry[0].path, curr_entry->path, 256) != 0);
            char pathstr[32 + 1];
            const char* optionstr[4];
            u32 n_opt = 2;
            
            TruncateString(pathstr, curr_entry->path, 32, 8);
            optionstr[0] = "Show in Hexeditor";
            optionstr[1] = "Calculate SHA-256";
            if (injectable) optionstr[n_opt++] = "Inject data @offset";
            if (file_type && (PathToNumFS(curr_entry->path) == 0))
                optionstr[n_opt++] = (file_type == IMG_NAND) ? "Mount as NAND image" : "Mount as FAT image";
            
            u32 user_select = ShowSelectPrompt(n_opt, optionstr, pathstr);
            if (user_select == 1) { // -> show in hex viewer
                static bool show_instr = true;
                if (show_instr) {
                    ShowPrompt(false, "Hexeditor Controls:\n \n\x18\x19\x1A\x1B(+R) - Scroll\nR+Y - Switch view\nX - Search / goto...\nA - Enter edit mode\nA+\x18\x19\x1A\x1B - Edit value\nB - Exit\n");
                    show_instr = false;
                }
                HexViewer(curr_entry->path);
            } else if (user_select == 2) { // -> calculate SHA-256
                u8 sha256[32];
                if (!FileGetSha256(curr_entry->path, sha256)) {
                    ShowPrompt(false, "Calculating SHA-256: failed!");
                } else {
                    static char pathstr_prev[32 + 1] = { 0 };
                    static u8 sha256_prev[32] = { 0 };
                    char sha_path[256];
                    u8 sha256_file[32];
                    bool have_sha = false;
                    bool write_sha = false;
                    snprintf(sha_path, 256, "%s.sha", curr_entry->path);
                    have_sha = (FileGetData(sha_path, sha256_file, 32, 0) == 32);
                    write_sha = !have_sha && (PathToNumFS(curr_entry->path) == 0); // writing only on SD
                    if (ShowPrompt(write_sha, "%s\n%016llX%016llX\n%016llX%016llX%s%s%s%s%s",
                        pathstr, getbe64(sha256 + 0), getbe64(sha256 + 8), getbe64(sha256 + 16), getbe64(sha256 + 24),
                        (have_sha) ? "\nSHA verification: " : "",
                        (have_sha) ? ((memcmp(sha256, sha256_file, 32) == 0) ? "passed!" : "failed!") : "",
                        (memcmp(sha256, sha256_prev, 32) == 0) ? "\n \nIdentical with previous file:\n" : "",
                        (memcmp(sha256, sha256_prev, 32) == 0) ? pathstr_prev : "",
                        (write_sha) ? "\n \nWrite .SHA file?" : "") && !have_sha) {
                        FileSetData(sha_path, sha256, 32, 0, true);
                        GetDirContents(current_dir, current_path);
                    }
                    strncpy(pathstr_prev, pathstr, 32 + 1);
                    memcpy(sha256_prev, sha256, 32);
                }
            } else if ((!injectable && (user_select == 3)) || (user_select == 4)) { // -> mount as image
                DeinitExtFS();
                u32 mount_state = MountImage(curr_entry->path);
                InitExtFS();
                if (!mount_state || !(IsMountedFS("7:")|IsMountedFS("8:")|IsMountedFS("9:"))) {
                    ShowPrompt(false, "Mounting image: failed");
                    DeinitExtFS();
                    MountImage(NULL);
                    InitExtFS();
                } else {
                    *current_path = '\0';
                    GetDirContents(current_dir, current_path);
                    cursor = 0;
                }
                if (clipboard->n_entries && (strcspn(clipboard->entry[0].path, IMG_DRV) == 0))
                    clipboard->n_entries = 0; // remove invalid clipboard stuff
            } else if (injectable && (user_select == 3)) { // -> inject data from clipboard
                char origstr[18 + 1];
                TruncateString(origstr, clipboard->entry[0].name, 18, 10);
                u64 offset = ShowHexPrompt(0, 8, "Inject data from %s?\nSpecifiy offset below.", origstr);
                if (offset != (u64) -1) {
                    if (!FileInjectFile(curr_entry->path, clipboard->entry[0].path, (u32) offset))
                        ShowPrompt(false, "Failed injecting %s", origstr);
                    clipboard->n_entries = 0;
                }
            }
        } else if (*current_path && ((pad_state & BUTTON_B) || // one level down
            ((pad_state & BUTTON_A) && (curr_entry->type == T_DOTDOT)))) { 
            char old_path[256];
            char* last_slash = strrchr(current_path, '/');
            strncpy(old_path, current_path, 256);
            if (last_slash) *last_slash = '\0'; 
            else *current_path = '\0';
            GetDirContents(current_dir, current_path);
            if (*old_path && current_dir->n_entries) {
                for (cursor = current_dir->n_entries - 1;
                    (cursor > 0) && (strncmp(current_dir->entry[cursor].path, old_path, 256) != 0); cursor--);
                if (*current_path && !cursor && (current_dir->n_entries > 1)) cursor = 1; // don't set it on the dotdot
                scroll = 0;
            }
        } else if (switched && (pad_state & BUTTON_B)) { // unmount SD card
            DeinitExtFS();
            if (GetMountState() != IMG_RAMDRV)
                MountImage(NULL);
            DeinitSDCardFS();
            clipboard->n_entries = 0;
            memset(panedata, 0x00, N_PANES * sizeof(PaneData));
            ShowString("SD card unmounted, you can eject now.\n \n<R+Y+\x1B> for format menu\n<A> to remount SD card");
            while (true) {
                u32 pad_choice = InputWait();
                if ((pad_choice & (BUTTON_R1|BUTTON_Y|BUTTON_LEFT)) == (BUTTON_R1|BUTTON_Y|BUTTON_LEFT))
                    SdFormatMenu();
                else if ((pad_choice & BUTTON_B) && InitSDCardFS()) break;
                else if (pad_choice & (BUTTON_B|BUTTON_START)) return exit_mode;
                else if (!(pad_choice & BUTTON_A)) continue;
                if (InitSDCardFS()) break;
                ShowString("Reinitialising SD card failed!\n \n<R+Y+\x1B> for format menu\n<A> to retry, <B> to reboot");
            }
            ClearScreenF(true, false, COLOR_STD_BG);
            InitEmuNandBase();
            InitExtFS();
            GetDirContents(current_dir, current_path);
            if (cursor >= current_dir->n_entries) cursor = 0;
        } else if ((pad_state & BUTTON_DOWN) && (cursor + 1 < current_dir->n_entries))  { // cursor down
            cursor++;
        } else if ((pad_state & BUTTON_UP) && cursor) { // cursor up
            cursor--;
        } else if (switched && (pad_state & (BUTTON_RIGHT|BUTTON_LEFT))) { // switch pane
            memcpy(pane->path, current_path, 256);  // store state in current pane
            pane->cursor = cursor;
            pane->scroll = scroll;
            (pad_state & BUTTON_LEFT) ? pane-- : pane++; // switch to next
            if (pane < panedata) pane += N_PANES;
            else if (pane >= panedata + N_PANES) pane -= N_PANES;
            memcpy(current_path, pane->path, 256);  // get state from next pane
            cursor = pane->cursor;
            scroll = pane->scroll;
            GetDirContents(current_dir, current_path);
        } else if ((pad_state & BUTTON_RIGHT) && (mark_setting < 0)) { // cursor down (quick)
            cursor += quick_stp;
        } else if ((pad_state & BUTTON_LEFT) && (mark_setting < 0)) { // cursor up (quick)
            cursor = (cursor >= quick_stp) ? cursor - quick_stp : 0;
        } else if (pad_state & BUTTON_RIGHT) { // mark all entries
            for (u32 c = 1; c < current_dir->n_entries; c++) current_dir->entry[c].marked = 1;
            mark_setting = 1;
        } else if (pad_state & BUTTON_LEFT) { // unmark all entries
            for (u32 c = 1; c < current_dir->n_entries; c++) current_dir->entry[c].marked = 0;
            mark_setting = 0;
        } else if (switched && (pad_state & BUTTON_L1)) { // switched L -> screenshot
            CreateScreenshot();
            ClearScreenF(true, true, COLOR_STD_BG);
        } else if (*current_path && (pad_state & BUTTON_L1) && (curr_entry->type != T_DOTDOT)) { // unswitched L - mark/unmark single entry
            if (mark_setting >= 0) {
                curr_entry->marked = mark_setting;
            } else {
                curr_entry->marked ^= 0x1;
                mark_setting = curr_entry->marked;
            }
        } else if (pad_state & BUTTON_SELECT) { // clear/restore clipboard
            clipboard->n_entries = (clipboard->n_entries > 0) ? 0 : last_clipboard_size;
        }

        // highly specific commands
        if (!*current_path) { // in the root folder...
            if (switched && (pad_state & BUTTON_X)) { // unmount image
                DeinitExtFS();
                if (!GetMountState()) MountRamDrive();
                else MountImage(NULL);
                InitExtFS();
                GetDirContents(current_dir, current_path);
                if (clipboard->n_entries && (strcspn(clipboard->entry[0].path, IMG_DRV) == 0))
                    clipboard->n_entries = 0; // remove invalid clipboard stuff
            } else if (switched && (pad_state & BUTTON_Y)) {
                SetWritePermissions((GetWritePermissions() > PERM_BASE) ? PERM_BASE : PERM_ALL, false);
            }
        } else if (!switched) { // standard unswitched command set
            if (GetVirtualSource(current_path) && (pad_state & BUTTON_X)) {
                ShowPrompt(false, "Not allowed in virtual path");
            } else if (pad_state & BUTTON_X) { // delete a file 
                u32 n_marked = 0;
                for (u32 c = 0; c < current_dir->n_entries; c++)
                    if (current_dir->entry[c].marked) n_marked++;
                if (n_marked) {
                    if (ShowPrompt(true, "Delete %u path(s)?", n_marked)) {
                        u32 n_errors = 0;
                        ShowString("Deleting files, please wait...");
                        for (u32 c = 0; c < current_dir->n_entries; c++)
                            if (current_dir->entry[c].marked && !PathDelete(current_dir->entry[c].path))
                                n_errors++;
                        ClearScreenF(true, false, COLOR_STD_BG);
                        if (n_errors) ShowPrompt(false, "Failed deleting %u/%u path(s)", n_errors, n_marked);
                    }
                } else if (curr_entry->type != T_DOTDOT) {
                    char namestr[36+1];
                    TruncateString(namestr, curr_entry->name, 28, 12);
                    if (ShowPrompt(true, "Delete \"%s\"?", namestr)) {
                        ShowString("Deleting %s\nPlease wait...", namestr);
                        if (!PathDelete(curr_entry->path))
                            ShowPrompt(false, "Failed deleting:\n%s", namestr);
                        ClearScreenF(true, false, COLOR_STD_BG);
                    }
                }
                GetDirContents(current_dir, current_path);
            } else if ((pad_state & BUTTON_Y) && (clipboard->n_entries == 0)) { // fill clipboard
                for (u32 c = 0; c < current_dir->n_entries; c++) {
                    if (current_dir->entry[c].marked) {
                        current_dir->entry[c].marked = 0;
                        DirEntryCpy(&(clipboard->entry[clipboard->n_entries]), &(current_dir->entry[c]));
                        clipboard->n_entries++;
                    }
                }
                if ((clipboard->n_entries == 0) && (curr_entry->type != T_DOTDOT)) {
                    DirEntryCpy(&(clipboard->entry[0]), curr_entry);
                    clipboard->n_entries = 1;
                }
                if (clipboard->n_entries)
                    last_clipboard_size = clipboard->n_entries;
            } else if (pad_state & BUTTON_Y) { // paste files
                const char* optionstr[2] = { "Copy path(s)", "Move path(s)" };
                char promptstr[64];
                u32 user_select;
                if (clipboard->n_entries == 1) {
                    char namestr[20+1];
                    TruncateString(namestr, clipboard->entry[0].name, 20, 12);
                    snprintf(promptstr, 64, "Paste \"%s\" here?", namestr);
                } else snprintf(promptstr, 64, "Paste %lu paths here?", clipboard->n_entries);
                user_select = (!GetVirtualSource(clipboard->entry[0].path) && !GetVirtualSource(current_path)) ?
                    ShowSelectPrompt(2, optionstr, promptstr) : (ShowPrompt(true, promptstr) ? 1 : 0);
                if (user_select) {
                    for (u32 c = 0; c < clipboard->n_entries; c++) {
                        char namestr[36+1];
                        TruncateString(namestr, clipboard->entry[c].name, 36, 12);
                        if ((user_select == 1) && !PathCopy(current_path, clipboard->entry[c].path)) {    
                            if (c + 1 < clipboard->n_entries) {
                                if (!ShowPrompt(true, "Failed copying path:\n%s\nProcess remaining?", namestr)) break;
                            } else ShowPrompt(false, "Failed copying path:\n%s", namestr);
                        } else if ((user_select == 2) && !PathMove(current_path, clipboard->entry[c].path)) {    
                            if (c + 1 < clipboard->n_entries) {
                                if (!ShowPrompt(true, "Failed moving path:\n%s\nProcess remaining?", namestr)) break;
                            } else ShowPrompt(false, "Failed moving path:\n%s", namestr);
                        }
                    }                        
                    clipboard->n_entries = 0;
                    GetDirContents(current_dir, current_path);
                }
                ClearScreenF(true, false, COLOR_STD_BG);
            }
        } else { // switched command set
            if (GetVirtualSource(current_path) && (pad_state & (BUTTON_X|BUTTON_Y))) {
                ShowPrompt(false, "Not allowed in virtual path");
            } else if ((pad_state & BUTTON_X) && (curr_entry->type != T_DOTDOT)) { // rename a file
                char newname[256];
                char namestr[20+1];
                TruncateString(namestr, curr_entry->name, 20, 12);
                snprintf(newname, 255, curr_entry->name);
                if (ShowStringPrompt(newname, 256, "Rename %s?\nEnter new name below.", namestr)) {
                    if (!PathRename(curr_entry->path, newname))
                        ShowPrompt(false, "Failed renaming path:\n%s", namestr);
                    else {
                        GetDirContents(current_dir, current_path);
                        for (cursor = (current_dir->n_entries) ? current_dir->n_entries - 1 : 0;
                            (cursor > 1) && (strncmp(current_dir->entry[cursor].name, newname, 256) != 0); cursor--);
                    }
                }
            } else if (pad_state & BUTTON_Y) { // create a folder
                char dirname[256];
                snprintf(dirname, 255, "newdir");
                if (ShowStringPrompt(dirname, 256, "Create a new folder here?\nEnter name below.")) {
                    if (!DirCreate(current_path, dirname)) {
                        char namestr[36+1];
                        TruncateString(namestr, dirname, 36, 12);
                        ShowPrompt(false, "Failed creating folder:\n%s", namestr);
                    } else {
                        GetDirContents(current_dir, current_path);
                        for (cursor = (current_dir->n_entries) ? current_dir->n_entries - 1 : 0;
                            (cursor > 1) && (strncmp(current_dir->entry[cursor].name, dirname, 256) != 0); cursor--);
                    }
                }
            }
        }
        
        if (pad_state & BUTTON_START) {
            exit_mode = (switched || (pad_state & BUTTON_LEFT)) ? GODMODE_EXIT_POWEROFF : GODMODE_EXIT_REBOOT;
            break;
        } 
    }
    
    DeinitExtFS();
    MountImage(NULL);
    DeinitSDCardFS();
    
    return exit_mode;
}
