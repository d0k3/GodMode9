#include "godmode.h"
#include "draw.h"
#include "hid.h"
#include "fs.h"

#define COLOR_TOP_BAR   ((GetWritePermissions() == 0) ? COLOR_WHITE : (GetWritePermissions() == 1) ? COLOR_BRIGHTGREEN : (GetWritePermissions() == 2) ? COLOR_BRIGHTYELLOW : COLOR_BRIGHTRED)
#define COLOR_SIDE_BAR  COLOR_DARKGREY
#define COLOR_MARKED    COLOR_TINTEDYELLOW
#define COLOR_FILE      COLOR_TINTEDGREEN
#define COLOR_DIR       COLOR_TINTEDBLUE
#define COLOR_ROOT      COLOR_GREY

void DrawUserInterface(const char* curr_path, DirEntry* curr_entry, DirStruct* clipboard, bool switched) {
    const u32 info_start = 18;
    
    static u32 state_prev = 0xFFFFFFFF;
    u32 state_curr =
        ((*curr_path) ? (1<<0) : 0) |
        ((clipboard->n_entries) ? (1<<1) : 0) |
        ((switched) ? (1<<2) : 0) |
        (GetWritePermissions()<<3);
    
    char bytestr0[32];
    char bytestr1[32];
    char tempstr[64];
    
    if (state_prev != state_curr) {
        ClearScreenF(true, false, COLOR_STD_BG);
        state_prev = state_curr;
    }
    
    // top bar - current path & free/total storage
    DrawRectangleF(true, 0, 0, SCREEN_WIDTH_TOP, 12, COLOR_TOP_BAR);
    if (strncmp(curr_path, "", 256) != 0) {
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
    u32 color_current = (curr_entry->marked) ? COLOR_MARKED : (curr_entry->type == T_FAT_ROOT) ? COLOR_ROOT : (curr_entry->type == T_FAT_DIR) ? COLOR_DIR : COLOR_FILE;
    DrawStringF(true, 4, info_start + 12, color_current, COLOR_STD_BG, "%s", tempstr);
    if (curr_entry->type == T_FAT_DIR) {
        ResizeString(tempstr, "(dir)", 20, 8, false);
    } else {
        FormatBytes(bytestr0, curr_entry->size);
        ResizeString(tempstr, bytestr0, 20, 8, false);
    }
    DrawStringF(true, 4, info_start + 12 + 10, color_current, COLOR_STD_BG, tempstr);
    
    // right top - clipboard
    DrawStringF(true, SCREEN_WIDTH_TOP - (20*8), info_start, COLOR_STD_FONT, COLOR_STD_BG, "%20s", (clipboard->n_entries) ? "[CLIPBOARD]" : "");
    for (u32 c = 0; c < 10; c++) {
        ResizeString(tempstr, (clipboard->n_entries > c) ? clipboard->entry[c].name : "", 20, 8, true);
        DrawStringF(true, SCREEN_WIDTH_TOP - (20*8) - 4, info_start + 12 + (c*10), (clipboard->entry[c].type == T_FAT_FILE) ? COLOR_FILE : COLOR_DIR, COLOR_STD_BG, tempstr);
    }
    *tempstr = '\0';
    if (clipboard->n_entries > 10) snprintf(tempstr, 60, "+ %lu more", clipboard->n_entries - 10);
    DrawStringF(true, SCREEN_WIDTH_TOP - (20*8) - 4, info_start + 12 + (10*10), COLOR_DARKGREY, COLOR_STD_BG, "%20s", tempstr);
    
    
    // bottom: inctruction block
    char instr[256];
    snprintf(instr, 256, "%s%s%s%s%s%s",
        "GodMode 9 v0.0.4\n", // generic start part
        (*curr_path && !switched) ? "<R> (hold) - Switch commands\n<L> (+<\x18\x19\x1A\x1B>) - Mark entries\n" :
        (*curr_path && switched) ? "<R> (rel.) - Switch commands\n<L> - Make a Screenshot\n" :
        "<R+L> - Make a Screenshot\n",
        (!(*curr_path)) ? "" :
        (!switched) ? "<X> - DELETE file(s)\n<Y> - ADD file(s) to clipboard\n" :
        "<X> - RENAME file\n<Y> - CREATE directory\n",
        (*curr_path) ? "" :
        (GetWritePermissions() <= 1) ? "<X> - Unlock EmuNAND writing\n<Y> - Unlock SysNAND writing\n" :
        (GetWritePermissions() == 2) ? "<X> - Relock EmuNAND writing\n<Y> - Unlock SysNAND writing\n" :
        "<X> - Relock EmuNAND writing\n<Y> - Relock SysNAND writing\n",
        (clipboard->n_entries) ? "<SELECT> - Clear Clipboard\n" : "", // only if clipboard is full
        "<START/+\x1B> - Reboot/Poweroff"); // generic end part
    DrawStringF(true, (SCREEN_WIDTH_TOP - GetDrawStringWidth(instr)) / 2, SCREEN_HEIGHT - 2 - GetDrawStringHeight(instr), COLOR_STD_FONT, COLOR_STD_BG, instr);
}

void DrawDirContents(DirStruct* contents, u32 cursor) {
    static u32 offset_disp = 0;
    const int str_width = 39;
    const u32 start_y = 2;
    const u32 stp_y = 12;
    const u32 pos_x = 0;
    const u32 lines = (SCREEN_HEIGHT-start_y+stp_y-1) / stp_y;
    u32 pos_y = start_y;
    
    if (offset_disp > cursor) offset_disp = cursor;
    else if (offset_disp + lines <= cursor) offset_disp = cursor - lines + 1;
    
    for (u32 i = 0; pos_y < SCREEN_HEIGHT; i++) {
        char tempstr[str_width + 1];
        u32 offset_i = offset_disp + i;
        u32 color_font;
        if (offset_i < contents->n_entries) {
            if (cursor != offset_i) {
                color_font = (contents->entry[offset_i].marked) ? COLOR_MARKED : (contents->entry[offset_i].type == T_FAT_DIR) ? COLOR_DIR : (contents->entry[offset_i].type == T_FAT_FILE) ? COLOR_FILE : COLOR_ROOT;
            } else {
                color_font = COLOR_STD_FONT;
            }
            ResizeString(tempstr, contents->entry[offset_i].name, str_width, str_width - 10, false);
        } else {
            color_font = COLOR_WHITE;
            snprintf(tempstr, str_width + 1, "%-*.*s", str_width, str_width, "");
        }
        DrawStringF(false, pos_x, pos_y, color_font, COLOR_STD_BG, tempstr);
        pos_y += stp_y;
    }
    
    if (contents->n_entries > lines) { // draw position bar at the right
        const u32 bar_height_min = 32;
        const u32 bar_width = 2;
        
        u32 bar_height = (lines * SCREEN_HEIGHT) / contents->n_entries;
        if (bar_height < bar_height_min) bar_height = bar_height_min;
        u32 bar_pos = ((u64) offset_disp * (SCREEN_HEIGHT - bar_height)) / (contents->n_entries - lines);
        
        DrawRectangleF(false, SCREEN_WIDTH_BOT - bar_width, 0, bar_width, bar_pos, COLOR_STD_BG);
        DrawRectangleF(false, SCREEN_WIDTH_BOT - bar_width, bar_pos + bar_height, bar_width, SCREEN_WIDTH_BOT - (bar_pos + bar_height), COLOR_STD_BG);
        DrawRectangleF(false, SCREEN_WIDTH_BOT - bar_width, bar_pos, bar_width, bar_height, COLOR_SIDE_BAR);
    }
}

u32 GodMode() {
    static const u32 quick_stp = 20;
    u32 exit_mode = GODMODE_EXIT_REBOOT;
    
    // reserve 512kB for each, just to be safe
    static DirStruct* current_dir = (DirStruct*)0x21000000;
    static DirStruct* clipboard   = (DirStruct*)0x21080000;
    char current_path[256] = { 0x00 };
    
    int mark_setting = -1;
    bool switched = false;
    u32 cursor = 0;
    
    ClearScreenF(true, true, COLOR_STD_BG);
    if (!InitFS()) return exit_mode;
    
    GetDirContents(current_dir, "");
    clipboard->n_entries = 0;
    while (true) { // this is the main loop
        DrawUserInterface(current_path, &(current_dir->entry[cursor]), clipboard, switched);
        DrawDirContents(current_dir, cursor);
        u32 pad_state = InputWait();
        switched = (pad_state & BUTTON_R1);
        if (!(*current_path) || switched || !(pad_state & BUTTON_L1)) {
            mark_setting = -1;
        }
        
        // commands which are valid anywhere
        if ((pad_state & BUTTON_A) && (current_dir->entry[cursor].type != T_FAT_FILE)) { // one level up
            strncpy(current_path, current_dir->entry[cursor].path, 256);
            GetDirContents(current_dir, current_path);
            cursor = 0;
        } else if (pad_state & BUTTON_B) { // one level down
            char* last_slash = strrchr(current_path, '/');
            if (last_slash) *last_slash = '\0'; 
            else *current_path = '\0';
            GetDirContents(current_dir, current_path);
            cursor = 0;
        } else if ((pad_state & BUTTON_DOWN) && (cursor < current_dir->n_entries - 1))  { // cursor up
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
            for (u32 c = 0; c < current_dir->n_entries; c++) current_dir->entry[c].marked = 1;
        } else if (pad_state & BUTTON_LEFT) { // unmark all entries
            for (u32 c = 0; c < current_dir->n_entries; c++) current_dir->entry[c].marked = 0;
        } else if (switched && (pad_state & BUTTON_L1)) { // switched L -> screenshot
            CreateScreenshot();
            ClearScreenF(true, true, COLOR_STD_BG);
        } else if (*current_path && (pad_state & BUTTON_L1)) { // unswitched L - mark/unmark single entry
            if (mark_setting >= 0) {
                current_dir->entry[cursor].marked = mark_setting;
            } else {
                current_dir->entry[cursor].marked ^= 0x1;
                mark_setting = current_dir->entry[cursor].marked;
            }
        } else if ((pad_state & BUTTON_SELECT) && (clipboard->n_entries > 0)) { // clear clipboard
            clipboard->n_entries = 0;
        }

        // highly specific commands
        if (!(*current_path)) { // in the root folder...
            if (pad_state & BUTTON_X) {
                SetWritePermissions((GetWritePermissions() >= 2) ? 1 : 2);
            } else if (pad_state & BUTTON_Y) {
                SetWritePermissions((GetWritePermissions() >= 3) ? 2 : 3);
            }
        } else if (!switched) { // standard unswitched command set
            if (pad_state & BUTTON_X) { // delete a file 
                // not implemented yet
            } else if ((pad_state & BUTTON_Y) && (clipboard->n_entries == 0)) { // fill clipboard
                for (u32 c = 0; c < current_dir->n_entries; c++) {
                    if (current_dir->entry[c].marked) {
                        DirEntryCpy(&(clipboard->entry[clipboard->n_entries]), &(current_dir->entry[c]));
                        current_dir->entry[c].marked = 0;
                        clipboard->n_entries++;
                    }
                }
                if (clipboard->n_entries == 0) {
                    DirEntryCpy(&(clipboard->entry[0]), &(current_dir->entry[cursor]));
                    clipboard->n_entries = 1;
                }
            }
        } else { // switched command set
            // not implemented yet
        }
        
        if (pad_state & BUTTON_START) {
            exit_mode = (pad_state & BUTTON_LEFT) ? GODMODE_EXIT_POWEROFF : GODMODE_EXIT_REBOOT;
            break;
        } 
    }
    
    DeinitFS();
    
    return exit_mode;
}
