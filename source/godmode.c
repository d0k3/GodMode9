#include "godmode.h"
#include "ui.h"
#include "hid.h"
#include "fs.h"
#include "platform.h"
#include "nand.h"
#include "virtual.h"
#include "image.h"

#define VERSION "0.3.9"

#define N_PANES 2
#define IMG_DRV "789I"

#define COLOR_TOP_BAR   ((GetWritePermissions() == 0) ? COLOR_WHITE : (GetWritePermissions() == 1) ? COLOR_BRIGHTGREEN : (GetWritePermissions() == 2) ? COLOR_BRIGHTYELLOW : COLOR_RED)
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
    const u32 info_start = 18;
    const u32 instr_x = 56;
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
    DrawRectangleF(true, 0, 0, SCREEN_WIDTH_TOP, 12, COLOR_TOP_BAR);
    if (strncmp(curr_path, "", 256) != 0) {
        char bytestr0[32];
        char bytestr1[32];
        TruncateString(tempstr, curr_path, 30, 8);
        DrawStringF(true, 2, 2, COLOR_STD_BG, COLOR_TOP_BAR, tempstr);
        DrawStringF(true, 30 * 8 + 4, 2, COLOR_STD_BG, COLOR_TOP_BAR, "%19.19s", "LOADING...");
        FormatBytes(bytestr0, GetFreeSpace(curr_path));
        FormatBytes(bytestr1, GetTotalSpace(curr_path));
        snprintf(tempstr, 64, "%s/%s", bytestr0, bytestr1);
        DrawStringF(true, 30 * 8 + 4, 2, COLOR_STD_BG, COLOR_TOP_BAR, "%19.19s", tempstr);
    } else {
        DrawStringF(true, 2, 2, COLOR_STD_BG, COLOR_TOP_BAR, "[root]");
        DrawStringF(true, 30 * 8 + 6, 2, COLOR_STD_BG, COLOR_TOP_BAR, "%19.19s", "GodMode9");
    }
    
    // left top - current file info
    if (curr_pane) snprintf(tempstr, 63, "PANE #%lu", curr_pane);
    else snprintf(tempstr, 63, "CURRENT");
    DrawStringF(true, 2, info_start, COLOR_STD_FONT, COLOR_STD_BG, "[%s]", tempstr);
    // file / entry name
    ResizeString(tempstr, curr_entry->name, 20, 8, false);
    u32 color_current = COLOR_ENTRY(curr_entry);
    DrawStringF(true, 4, info_start + 12, color_current, COLOR_STD_BG, "%s", tempstr);
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
    DrawStringF(true, 4, info_start + 12 + 10, color_current, COLOR_STD_BG, tempstr);
    
    // right top - clipboard
    DrawStringF(true, SCREEN_WIDTH_TOP - (20*8), info_start, COLOR_STD_FONT, COLOR_STD_BG, "%20s", (clipboard->n_entries) ? "[CLIPBOARD]" : "");
    for (u32 c = 0; c < n_cb_show; c++) {
        u32 color_cb = COLOR_ENTRY(&(clipboard->entry[c]));
        ResizeString(tempstr, (clipboard->n_entries > c) ? clipboard->entry[c].name : "", 20, 8, true);
        DrawStringF(true, SCREEN_WIDTH_TOP - (20*8) - 4, info_start + 12 + (c*10), color_cb, COLOR_STD_BG, tempstr);
    }
    *tempstr = '\0';
    if (clipboard->n_entries > n_cb_show) snprintf(tempstr, 60, "+ %lu more", clipboard->n_entries - n_cb_show);
    DrawStringF(true, SCREEN_WIDTH_TOP - (20*8) - 4, info_start + 12 + (n_cb_show*10), COLOR_DARKGREY, COLOR_STD_BG, "%20s", tempstr);
    
    // bottom: inctruction block
    char instr[256];
    snprintf(instr, 256, "%s%s\n%s%s%s%s%s%s",
        "GodMode9 Explorer v", VERSION, // generic start part
        (*curr_path) ? ((clipboard->n_entries == 0) ? "L - MARK files (use with \x18\x19\x1A\x1B)\nX - DELETE / [+R] RENAME file(s)\nY - COPY file(s) / [+R] CREATE dir\n" :
        "L - MARK files (use with \x18\x19\x1A\x1B)\nX - DELETE / [+R] RENAME file(s)\nY - PASTE file(s) / [+R] CREATE dir\n") :
        ((GetWritePermissions() <= 1) ? "X - Unlock EmuNAND / image writing\nY - Unlock SysNAND writing\nR+B - Unmount SD card\n" :
        (GetWritePermissions() == 2) ? "X - Relock EmuNAND / image writing\nY - Unlock SysNAND writing\nR+B - Unmount SD card\n" :
        "X - Relock EmuNAND writing\nY - Relock SysNAND writing\nR+B - Unmount SD card\n"),
        (*curr_path) ? "" : ((GetMountState() == IMG_RAMDRV) ? "R+X - Unmount RAM drive\n" :
        (GetMountState()) ? "R+X - Unmount image\n" : "R+X - Mount RAM drive\n"),
        "R+L - Make a Screenshot\n",
        "R+\x1B\x1A - Switch to prev/next pane\n",
        (clipboard->n_entries) ? "SELECT - Clear Clipboard\n" : "SELECT - Restore Clipboard\n", // only if clipboard is full
        "START - Reboot / [+R] Poweroff"); // generic end part
    DrawStringF(true, instr_x, SCREEN_HEIGHT - 4 - GetDrawStringHeight(instr), COLOR_STD_FONT, COLOR_STD_BG, instr);
}

void DrawDirContents(DirStruct* contents, u32 cursor, u32* scroll) {
    const int str_width = 39;
    const u32 bar_height_min = 32;
    const u32 bar_width = 2;
    const u32 start_y = 2;
    const u32 stp_y = 12;
    const u32 pos_x = 0;
    const u32 lines = (SCREEN_HEIGHT-start_y+stp_y-1) / stp_y;
    u32 pos_y = start_y;
     
    if (*scroll > cursor) *scroll = cursor;
    else if (*scroll + lines <= cursor) *scroll = cursor - lines + 1;
    
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
        DrawStringF(false, pos_x, pos_y, color_font, COLOR_STD_BG, tempstr);
        pos_y += stp_y;
    }
    
    if (contents->n_entries > lines) { // draw position bar at the right      
        u32 bar_height = (lines * SCREEN_HEIGHT) / contents->n_entries;
        if (bar_height < bar_height_min) bar_height = bar_height_min;
        u32 bar_pos = ((u64) *scroll * (SCREEN_HEIGHT - bar_height)) / (contents->n_entries - lines);
        
        DrawRectangleF(false, SCREEN_WIDTH_BOT - bar_width, 0, bar_width, bar_pos, COLOR_STD_BG);
        DrawRectangleF(false, SCREEN_WIDTH_BOT - bar_width, bar_pos + bar_height, bar_width, SCREEN_WIDTH_BOT - (bar_pos + bar_height), COLOR_STD_BG);
        DrawRectangleF(false, SCREEN_WIDTH_BOT - bar_width, bar_pos, bar_width, bar_height, COLOR_SIDE_BAR);
    } else DrawRectangleF(false, SCREEN_WIDTH_BOT - bar_width, 0, bar_width, SCREEN_HEIGHT, COLOR_STD_BG);
}

u32 HexViewer(const char* path) {
    static u32 mode = 0;
    u8 data[(SCREEN_HEIGHT / 8) * 16]; // this is the maximum size
    u32 fsize = FileGetSize(path);
    
    int x_off, x_hex, x_ascii;
    u32 vpad, hlpad, hrpad;
    u32 rows, cols;
    u32 total_shown;
    u32 total_data;
    
    u32 last_mode = 0xFF;
    u32 offset = 0;
    
    while (true) {
        if (mode != last_mode) {
            switch (mode) { // display mode
                case 1:
                    vpad = hlpad = hrpad = 1;
                    cols = 12;
                    x_off = 0;
                    x_ascii = SCREEN_WIDTH_TOP - (8 * cols);
                    x_hex = x_off + (8*8) + 12;
                    break;
                case 2:
                    vpad = 1;
                    hlpad = 0;
                    hrpad = 1;
                    cols = 16;
                    x_off = -1;
                    x_ascii = SCREEN_WIDTH_TOP - (8 * cols);
                    x_hex = 0;
                    break;
                case 3:
                    vpad = hlpad = hrpad = 1;
                    cols = 16;
                    x_off = 20;
                    x_ascii = -1;
                    x_hex = x_off + (8*8) + 12;
                    break;
                default:
                    vpad = hlpad = hrpad = 2;
                    cols = 8;
                    x_off = (SCREEN_WIDTH_TOP - SCREEN_WIDTH_BOT) / 2;
                    x_ascii = SCREEN_WIDTH_TOP - x_off - (8 * cols);
                    x_hex = (SCREEN_WIDTH_TOP - ((hlpad + 16 + hrpad) * cols)) / 2;
                    break;
            }
            rows = SCREEN_HEIGHT / (8 + (2*vpad));
            total_shown = rows * cols;
            if (offset % cols) offset -= (offset % cols); // fix offset (align to cols)
            last_mode = mode;
            ClearScreenF(true, false, COLOR_STD_BG);
        }
        // fix offset (if required)
        if (offset + total_shown > fsize + cols)
            offset = (total_shown > fsize) ? 0 : (fsize + cols - total_shown - (fsize % cols));
        total_data = FileGetData(path, data, total_shown, offset); // get data
        
        // display data on screen
        for (u32 row = 0; row < rows; row++) {
            char ascii[16 + 1] = { 0 };
            u32 y = row * (8 + (2*vpad)) + vpad;
            u32 curr_pos = row * cols;
            u32 cutoff = (curr_pos >= total_data) ? 0 : (total_data >= curr_pos + cols) ? cols : total_data - curr_pos;
            
            memcpy(ascii, data + curr_pos, cols);
            for (u32 col = 0; col < cols; col++)
                if ((col >= cutoff) || (ascii[col] == 0x00)) ascii[col] = ' ';
            
            // draw offset / ASCII representation
            if (x_off >= 0) DrawStringF(true, x_off, y, cutoff ? COLOR_HVOFFS : COLOR_HVOFFSI, COLOR_STD_BG,
                "%08X", (unsigned int) offset + curr_pos);
            if (x_ascii >= 0) {
                DrawString(TOP_SCREEN0, ascii, x_ascii, y, COLOR_HVASCII, COLOR_STD_BG);
                DrawString(TOP_SCREEN1, ascii, x_ascii, y, COLOR_HVASCII, COLOR_STD_BG);
            }
            
            // draw HEX values
            for (u32 col = 0; (col < cols) && (x_hex >= 0); col++) {
                u32 x = (x_hex + hlpad) + ((16 + hrpad + hlpad) * col);
                if (col < cutoff)
                    DrawStringF(true, x, y, COLOR_HVHEX(col), COLOR_STD_BG, "%02X", (unsigned int) data[curr_pos + col]);
                else DrawStringF(true, x, y, COLOR_HVHEX(col), COLOR_STD_BG, "  ");
            }
        }
        
        // handle user input
        u32 pad_state = InputWait();
        u32 step_ud = (pad_state & BUTTON_R1) ? (0x1000  - (0x1000  % cols))  : cols;
        u32 step_lr = (pad_state & BUTTON_R1) ? (0x10000 - (0x10000 % cols)) : total_shown;
        if (pad_state & BUTTON_DOWN) offset += step_ud;
        else if (pad_state & BUTTON_RIGHT) offset += step_lr;
        else if (pad_state & BUTTON_UP) offset = (offset > step_ud) ? offset - step_ud : 0;
        else if (pad_state & BUTTON_LEFT) offset = (offset > step_lr) ? offset - step_lr : 0;
        else if ((pad_state & BUTTON_R1) && (pad_state & BUTTON_Y)) mode = (mode + 1) % 4;
        else if ((pad_state & BUTTON_R1) && (pad_state & BUTTON_L1)) CreateScreenshot();
        else if (pad_state & BUTTON_B) break;
    }
    
    ClearScreenF(true, false, COLOR_STD_BG);
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
        if ((pad_state & BUTTON_A) && (curr_entry->type != T_FILE) && (curr_entry->type != T_DOTDOT)) { // one level up
            strncpy(current_path, curr_entry->path, 256);
            GetDirContents(current_dir, current_path);
            if (*current_path && (current_dir->n_entries > 1)) {
                cursor = 1;
                scroll = 0;
            } else cursor = 0;
        } else if ((pad_state & BUTTON_A) && (curr_entry->type == T_FILE)) { // process a file
            u32 file_type = IdentifyImage(curr_entry->path);
            char pathstr[32 + 1];
            const char* optionstr[4];
            u32 n_opt = 2;
            
            TruncateString(pathstr, curr_entry->path, 32, 8);
            optionstr[0] = "Show in Hexviewer";
            optionstr[1] = "Calculate SHA-256";
            if (file_type && (PathToNumFS(curr_entry->path) == 0)) {
                optionstr[2] = (file_type == IMG_NAND) ? "Mount as NAND image" : "Mount as FAT image";
                n_opt = 3;
            }
            
            u32 user_select = ShowSelectPrompt(n_opt, optionstr, pathstr);
            if (user_select == 1) { // -> show in hex viewer
                static bool show_instr = true;
                if (show_instr) {
                    ShowPrompt(false, "HexViewer Controls:\n \n\x18\x19\x1A\x1B(+R) - Scroll\nR+Y - Switch view\nB - Exit\n");
                    show_instr = false;
                }
                HexViewer(curr_entry->path);
            } else if (user_select == 2) { // -> calculate SHA-256
                static char pathstr_prev[32 + 1] = { 0 };
                static u8 sha256_prev[32] = { 0 };
                u8 sha256[32];
                if (!FileGetSha256(curr_entry->path, sha256)) {
                    ShowPrompt(false, "Calculating SHA-256: failed!");
                } else {
                    ShowPrompt(false, "%s\n%08X%08X%08X%08X\n%08X%08X%08X%08X%s%s",
                        pathstr,
                        getbe32(sha256 +  0), getbe32(sha256 +  4),
                        getbe32(sha256 +  8), getbe32(sha256 + 12),
                        getbe32(sha256 + 16), getbe32(sha256 + 20),
                        getbe32(sha256 + 24), getbe32(sha256 + 28),
                        (memcmp(sha256, sha256_prev, 32) == 0) ? "\n \nIdentical with previous file:\n" : "",
                        (memcmp(sha256, sha256_prev, 32) == 0) ? pathstr_prev : "");
                    strncpy(pathstr_prev, pathstr, 32 + 1);
                    memcpy(sha256_prev, sha256, 32);
                }
            } else if (user_select == 3) { // -> mount as image
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
            MountImage(NULL);
            DeinitSDCardFS();
            clipboard->n_entries = 0;
            memset(panedata, 0x00, N_PANES * sizeof(PaneData));
            ShowPrompt(false, "SD card unmounted, you can eject now.\nPut it back in before you press <A>.");
            while (!InitSDCardFS()) {
                if (!ShowPrompt(true, "Reinitialising SD card failed! Retry?"))
                    return exit_mode;
            }
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
        if (!(*current_path)) { // in the root folder...
            if (switched && !*current_path && (pad_state & BUTTON_X)) { // unmount image
                DeinitExtFS();
                if (!GetMountState()) MountRamDrive();
                else MountImage(NULL);
                InitExtFS();
                GetDirContents(current_dir, current_path);
                if (clipboard->n_entries && (strcspn(clipboard->entry[0].path, IMG_DRV) == 0))
                    clipboard->n_entries = 0; // remove invalid clipboard stuff
            } else if (pad_state & BUTTON_X) {
                SetWritePermissions((GetWritePermissions() >= 2) ? 1 : 2);
            } else if (pad_state & BUTTON_Y) {
                SetWritePermissions((GetWritePermissions() >= 3) ? 2 : 3);
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
                        for (u32 c = 0; c < current_dir->n_entries; c++)
                            if (current_dir->entry[c].marked && !PathDelete(current_dir->entry[c].path))
                                n_errors++;
                        if (n_errors) ShowPrompt(false, "Failed deleting %u/%u path(s)", n_errors, n_marked);
                    }
                } else if (curr_entry->type != T_DOTDOT) {
                    char namestr[36+1];
                    TruncateString(namestr, curr_entry->name, 36, 12);
                    if ((ShowPrompt(true, "Delete \"%s\"?", namestr)) && !PathDelete(curr_entry->path))
                        ShowPrompt(false, "Failed deleting:\n%s", namestr);
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
                    snprintf(promptstr, 64, "Copy / Move \"%s\" here?", namestr);
                } else snprintf(promptstr, 64, "Copy / Move %lu paths here?", clipboard->n_entries);
                if ((user_select = ShowSelectPrompt(2, optionstr, promptstr))) {
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
                }
                clipboard->n_entries = 0;
                GetDirContents(current_dir, current_path);
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
                if (ShowInputPrompt(newname, 256, "Rename %s?\nEnter new name below.", namestr)) {
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
                if (ShowInputPrompt(dirname, 256, "Create a new folder here?\nEnter name below.")) {
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
            exit_mode = switched ? GODMODE_EXIT_POWEROFF : GODMODE_EXIT_REBOOT;
            break;
        } 
    }
    
    DeinitExtFS();
    MountImage(NULL);
    DeinitSDCardFS();
    
    return exit_mode;
}
