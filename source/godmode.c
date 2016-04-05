#include "godmode.h"
#include "ui.h"
#include "hid.h"
#include "fs.h"
#include "platform.h"
#include "nand.h"
#include "virtual.h"
#include "image.h"

#define VERSION "0.2.3"

#define COLOR_TOP_BAR   ((GetWritePermissions() == 0) ? COLOR_WHITE : (GetWritePermissions() == 1) ? COLOR_BRIGHTGREEN : (GetWritePermissions() == 2) ? COLOR_BRIGHTYELLOW : COLOR_RED)
#define COLOR_SIDE_BAR  COLOR_DARKGREY
#define COLOR_MARKED    COLOR_TINTEDYELLOW
#define COLOR_FILE      COLOR_TINTEDGREEN
#define COLOR_DIR       COLOR_TINTEDBLUE
#define COLOR_ROOT      COLOR_GREY
#define COLOR_ENTRY(e)  (((e)->marked) ? COLOR_MARKED : ((e)->type == T_DIR) ? COLOR_DIR : ((e)->type == T_FILE) ? COLOR_FILE : ((e)->type == T_ROOT) ?  COLOR_ROOT : COLOR_GREY)


void DrawUserInterface(const char* curr_path, DirEntry* curr_entry, DirStruct* clipboard) {
    const u32 n_cb_show = 8;
    const u32 info_start = 18;
    const u32 instr_x = 56;
    char tempstr[64];
    
    static u32 state_prev = 0xFFFFFFFF;
    u32 state_curr =
        ((*curr_path) ? (1<<0) : 0) |
        ((clipboard->n_entries) ? (1<<1) : 0);
    
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
    DrawStringF(true, 2, info_start, COLOR_STD_FONT, COLOR_STD_BG, "[CURRENT]");
    ResizeString(tempstr, curr_entry->name, 20, 8, false);
    u32 color_current = COLOR_ENTRY(curr_entry);
    DrawStringF(true, 4, info_start + 12, color_current, COLOR_STD_BG, "%s", tempstr);
    if (curr_entry->type == T_DIR) {
        ResizeString(tempstr, "(dir)", 20, 8, false);
    } else if (curr_entry->type == T_DOTDOT) {
        snprintf(tempstr, 21, "%20s", "");
    } else {
        char bytestr[32];
        FormatBytes(bytestr, curr_entry->size);
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
    snprintf(instr, 256, "%s%s\n%s%s%s%s",
        "GodMode9 Explorer v", VERSION, // generic start part
        (*curr_path) ? ((clipboard->n_entries == 0) ? "L - MARK files (use with \x18\x19\x1A\x1B)\nX - DELETE / [+R] RENAME file(s)\nY - COPY file(s) / [+R] CREATE dir\n" :
        "L - MARK files (use with \x18\x19\x1A\x1B)\nX - DELETE / [+R] RENAME file(s)\nY - PASTE file(s) / [+R] CREATE dir\n") :
        ((GetWritePermissions() <= 1) ? "X - Unlock EmuNAND/image writing\nY - Unlock SysNAND writing\nR+B - Unmount SD card\n" :
        (GetWritePermissions() == 2) ? "X - Relock EmuNAND/image writing\nY - Unlock SysNAND writing\nR+B - Unmount SD card\n" :
        "X - Relock EmuNAND writing\nY - Relock SysNAND writing\nR+B - Unmount SD card\n"),
        "R+L - Make a Screenshot\n",
        (clipboard->n_entries) ? "SELECT - Clear Clipboard\n" : "SELECT - Restore Clipboard\n", // only if clipboard is full
        "START - Reboot / [+\x1B] Poweroff"); // generic end part
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
            color_font = (cursor != offset_i) ? COLOR_ENTRY(&(contents->entry[offset_i])) : COLOR_STD_FONT;
            ResizeString(tempstr, contents->entry[offset_i].name, str_width, str_width - 10, false);
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

u32 GodMode() {
    static const u32 quick_stp = 20;
    u32 exit_mode = GODMODE_EXIT_REBOOT;
    
    // reserve 512kB for each, just to be safe
    static DirStruct* current_dir = (DirStruct*)0x21000000;
    static DirStruct* clipboard   = (DirStruct*)0x21080000;
    char current_path[256] = { 0x00 };
    
    int mark_setting = -1;
    u32 last_clipboard_size = 0;
    u32 cursor = 0;
    u32 scroll = 0;
    
    ClearScreenF(true, true, COLOR_STD_BG);
    while (!InitSDCardFS()) {
        if (!ShowPrompt(true, "Initialising SD card failed! Retry?"))
            return exit_mode;
    }
    InitEmuNandBase();
    InitNandCrypto();
    InitExtFS();
    
    if ((GetUnitPlatform() == PLATFORM_N3DS) && !CheckSlot0x05Crypto()) {
        if (!ShowPrompt(true, "Warning: slot0x05 crypto fail\nslot0x05keyY.bin is either corrupt\nor does not exist. Continue?")) {
            DeinitExtFS();
            DeinitSDCardFS();
            return exit_mode;
        }
    }
    
    GetDirContents(current_dir, "");
    clipboard->n_entries = 0;
    while (true) { // this is the main loop
        DrawUserInterface(current_path, &(current_dir->entry[cursor]), clipboard);
        DrawDirContents(current_dir, cursor, &scroll);
        u32 pad_state = InputWait();
        bool switched = (pad_state & BUTTON_R1);
        if (!(*current_path) || switched || !(pad_state & BUTTON_L1)) {
            mark_setting = -1;
        }
        
        // commands which are valid anywhere
        if ((pad_state & BUTTON_A) && (current_dir->entry[cursor].type != T_FILE)) { // one level up
            if (current_dir->entry[cursor].type == T_DOTDOT) {
                char* last_slash = strrchr(current_path, '/');
                if (last_slash) *last_slash = '\0'; 
                else *current_path = '\0';
            } else { // type == T_DIR || type == T_ROOT
                strncpy(current_path, current_dir->entry[cursor].path, 256);
            }
            GetDirContents(current_dir, current_path);
            if (*current_path && (current_dir->n_entries > 1)) {
                cursor = 1;
                scroll = 0;
            } else cursor = 0;
        } else if ((pad_state & BUTTON_A) && (current_dir->entry[cursor].type == T_FILE) &&
            (PathToNumFS(current_dir->entry[cursor].path) == 0)) { // try to mount image
            u32 file_type = IdentifyImage(current_dir->entry[cursor].path);
            if (file_type && ShowPrompt(true, "This looks like a %s image\nTry to mount it?", (file_type == IMG_NAND) ? "NAND" : "FAT")) {
                DeinitExtFS();
                u32 mount_state = MountImage(current_dir->entry[cursor].path);
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
                if (clipboard->n_entries && (strcspn(clipboard->entry[0].path, "789I") == 0))
                    clipboard->n_entries = 0; // remove invalid clipboard stuff
            }
        } else if ((pad_state & BUTTON_B) && *current_path) { // one level down
            char old_path[256];
            char* last_slash = strrchr(current_path, '/');
            strncpy(old_path, current_path, 256);
            if (last_slash) *last_slash = '\0'; 
            else *current_path = '\0';
            GetDirContents(current_dir, current_path);
            if (*old_path) {
                for (cursor = current_dir->n_entries - 1;
                    (cursor > 1) && (strncmp(current_dir->entry[cursor].path, old_path, 256) != 0); cursor--);
                scroll = 0;
            }
        } else if ((pad_state & BUTTON_B) && (pad_state & BUTTON_R1)) { // unmount SD card
            DeinitExtFS();
            DeinitSDCardFS();
            clipboard->n_entries = 0;
            ShowPrompt(false, "SD card unmounted, you can eject now.\nPut it back in before you press <A>.");
            while (!InitSDCardFS()) {
                if (!ShowPrompt(true, "Reinitialising SD card failed! Retry?"))
                    return exit_mode;
            }
            InitEmuNandBase();
            InitExtFS();
            GetDirContents(current_dir, current_path);
            if (cursor >= current_dir->n_entries) cursor = 0;
        } else if ((pad_state & BUTTON_DOWN) && (cursor + 1 < current_dir->n_entries))  { // cursor up
            cursor++;
        } else if ((pad_state & BUTTON_UP) && cursor) { // cursor down
            cursor--;
        } else if ((pad_state & BUTTON_RIGHT) && (mark_setting < 0)) { // cursor down (quick)
            cursor += quick_stp;
            if (cursor >= current_dir->n_entries)
                cursor = current_dir->n_entries - 1;
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
        } else if (*current_path && (pad_state & BUTTON_L1) && cursor) { // unswitched L - mark/unmark single entry
            if (mark_setting >= 0) {
                current_dir->entry[cursor].marked = mark_setting;
            } else {
                current_dir->entry[cursor].marked ^= 0x1;
                mark_setting = current_dir->entry[cursor].marked;
            }
        } else if (pad_state & BUTTON_SELECT) { // clear/restore clipboard
            clipboard->n_entries = (clipboard->n_entries > 0) ? 0 : last_clipboard_size;
        }

        // highly specific commands
        if (!(*current_path)) { // in the root folder...
            if (pad_state & BUTTON_X) {
                SetWritePermissions((GetWritePermissions() >= 2) ? 1 : 2);
            } else if (pad_state & BUTTON_Y) {
                SetWritePermissions((GetWritePermissions() >= 3) ? 2 : 3);
            }
        } else if (!switched) { // standard unswitched command set
            if (IsVirtualPath(current_path) && (pad_state & BUTTON_X)) {
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
                } else if (cursor) {
                    char namestr[36+1];
                    TruncateString(namestr, current_dir->entry[cursor].name, 36, 12);
                    if ((ShowPrompt(true, "Delete \"%s\"?", namestr)) && !PathDelete(current_dir->entry[cursor].path))
                        ShowPrompt(false, "Failed deleting:\n%s", namestr);
                }
                GetDirContents(current_dir, current_path);
                if (cursor >= current_dir->n_entries)
                    cursor = current_dir->n_entries - 1;
            } else if ((pad_state & BUTTON_Y) && (clipboard->n_entries == 0)) { // fill clipboard
                for (u32 c = 0; c < current_dir->n_entries; c++) {
                    if (current_dir->entry[c].marked) {
                        current_dir->entry[c].marked = 0;
                        DirEntryCpy(&(clipboard->entry[clipboard->n_entries]), &(current_dir->entry[c]));
                        clipboard->n_entries++;
                    }
                }
                if ((clipboard->n_entries == 0) && cursor) {
                    DirEntryCpy(&(clipboard->entry[0]), &(current_dir->entry[cursor]));
                    clipboard->n_entries = 1;
                }
                if (clipboard->n_entries)
                    last_clipboard_size = clipboard->n_entries;
            } else if (pad_state & BUTTON_Y) { // paste files
                char promptstr[64];
                if (clipboard->n_entries == 1) {
                    char namestr[20+1];
                    TruncateString(namestr, clipboard->entry[0].name, 20, 12);
                    snprintf(promptstr, 64, "Copy \"%s\" here?", namestr);
                } else snprintf(promptstr, 64, "Copy %lu path(s) here?", clipboard->n_entries);
                if (ShowPrompt(true, promptstr)) {
                    for (u32 c = 0; c < clipboard->n_entries; c++) {
                        if (!PathCopy(current_path, clipboard->entry[c].path)) {
                            char namestr[36+1];
                            TruncateString(namestr, clipboard->entry[c].name, 36, 12);
                            if (c + 1 < clipboard->n_entries) {
                                if (!ShowPrompt(true, "Failed copying path:\n%s\nProcess remaining?", namestr)) break;
                            } else ShowPrompt(false, "Failed copying path:\n%s", namestr);
                        }
                    }
                }
                clipboard->n_entries = 0;
                GetDirContents(current_dir, current_path);
                ClearScreenF(true, false, COLOR_STD_BG);
            }
        } else { // switched command set
            if (IsVirtualPath(current_path) && (pad_state & (BUTTON_X|BUTTON_Y))) {
                ShowPrompt(false, "Not allowed in virtual path");
            } else if ((pad_state & BUTTON_X) && cursor) { // rename a file
                char newname[256];
                char namestr[20+1];
                TruncateString(namestr, current_dir->entry[cursor].name, 20, 12);
                snprintf(newname, 255, current_dir->entry[cursor].name);
                if (ShowInputPrompt(newname, 256, "Rename %s?\nEnter new name below.", namestr)) {
                    if (!PathRename(current_dir->entry[cursor].path, newname))
                        ShowPrompt(false, "Failed renaming path:\n%s", namestr);
                    else {
                        GetDirContents(current_dir, current_path);
                        for (cursor = current_dir->n_entries - 1;
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
                        for (cursor = current_dir->n_entries - 1;
                            (cursor > 1) && (strncmp(current_dir->entry[cursor].name, dirname, 256) != 0); cursor--);
                    }
                }
            }
        }
        
        if (pad_state & BUTTON_START) {
            exit_mode = (pad_state & BUTTON_LEFT) ? GODMODE_EXIT_POWEROFF : GODMODE_EXIT_REBOOT;
            break;
        } 
    }
    
    DeinitExtFS();
    DeinitSDCardFS();
    
    return exit_mode;
}
