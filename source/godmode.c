#include "godmode.h"
#include "draw.h"
#include "hid.h"
#include "fs.h"

#define COLOR_TOP_BAR   COLOR_WHITE
#define COLOR_SIDE_BAR  COLOR_DARKGREY
#define COLOR_MARKED    COLOR_TINTEDYELLOW
#define COLOR_FILE      COLOR_TINTEDGREEN
#define COLOR_DIR       COLOR_TINTEDBLUE
#define COLOR_ROOT      COLOR_GREY

void DrawUserInterface(const char* curr_path, DirEntry* curr_entry) {
    const u32 info_start = 16;
    char bytestr0[32];
    char bytestr1[32];
    char tempstr[64];
    
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
    DrawStringF(true, 2, info_start + 12, (curr_entry->marked) ? COLOR_MARKED : COLOR_STD_FONT, COLOR_STD_BG, "%s", tempstr);
    if (curr_entry->type == T_FAT_DIR) {
        ResizeString(tempstr, "(dir)", 20, 8, false);
        DrawStringF(true, 4, info_start + 12 + 10, COLOR_DIR, COLOR_STD_BG, tempstr);
    } else {
        FormatBytes(bytestr0, curr_entry->size);
        ResizeString(tempstr, bytestr0, 20, 8, false);
        DrawStringF(true, 4, info_start + 12 + 10, (curr_entry->type == T_FAT_FILE) ? COLOR_FILE : COLOR_ROOT, COLOR_STD_BG, tempstr);
    }
    
    // bottom: inctruction block
    char* instr = "GodMode 9 v0.0.1\n<A>/<B>/<\x18\x19\x1A\x1B> - Navigation\n<L> - Mark (multiple) file(s)\n<X> - Make a Screenshot\n<START/+\x1B> - Reboot / Power off";
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
        u32 bar_pos = (((u64) cursor * (SCREEN_HEIGHT - bar_height)) / contents->n_entries);
        
        DrawRectangleF(false, SCREEN_WIDTH_BOT - bar_width, 0, bar_width, bar_pos, COLOR_STD_BG);
        DrawRectangleF(false, SCREEN_WIDTH_BOT - bar_width, bar_pos + bar_height, bar_width, SCREEN_WIDTH_BOT - (bar_pos + bar_height), COLOR_STD_BG);
        DrawRectangleF(false, SCREEN_WIDTH_BOT - bar_width, bar_pos, bar_width, bar_height, COLOR_SIDE_BAR);
    }
}

u32 GodMode() {
    static const u32 quick_stp = 20;
    u32 exit_mode = GODMODE_EXIT_REBOOT;
    char current_path[256] = { 0x00 };
    DirStruct* contents;
    u32 cursor = 0;
    
    ClearScreenF(true, true, COLOR_BLACK);
    if (!InitFS()) return exit_mode;
    
    contents = GetDirContents("");
    while (true) { // this is the main loop
        DrawUserInterface(current_path, &contents->entry[cursor]); // no need to fully do this everytime!
        DrawDirContents(contents, cursor);
        u32 pad_state = InputWait();
        if (pad_state & BUTTON_DOWN) {
            cursor++;
            if (cursor >= contents->n_entries)
                cursor = contents->n_entries - 1;
        } else if ((pad_state & BUTTON_UP) && cursor) {
            cursor--;
        } else if (pad_state & BUTTON_RIGHT) {
            cursor += quick_stp;
            if (cursor >= contents->n_entries)
                cursor = contents->n_entries - 1;
        } else if (pad_state & BUTTON_LEFT) {
            cursor = (cursor >= quick_stp) ? cursor - quick_stp : 0;
        } else if ((pad_state & BUTTON_L1) && *current_path) {
            contents->entry[cursor].marked ^= 0x1;
        } else if ((pad_state & BUTTON_A) && (contents->entry[cursor].type != T_FAT_FILE)) {
            strncpy(current_path, contents->entry[cursor].path, 256);
            contents = GetDirContents(current_path);
            cursor = 0;
            ClearScreenF(true, true, COLOR_STD_BG); // not really required
        } else if (pad_state & BUTTON_B) {
            char* last_slash = strrchr(current_path, '/');
            if (last_slash) *last_slash = '\0'; 
            else *current_path = '\0';
            contents = GetDirContents(current_path);
            cursor = 0;
            ClearScreenF(true, true, COLOR_STD_BG); // not really required
        } else if (pad_state & BUTTON_X) {
            CreateScreenshot();
        }
        if (pad_state & BUTTON_START) {
            exit_mode = (pad_state & BUTTON_LEFT) ? GODMODE_EXIT_POWEROFF : GODMODE_EXIT_REBOOT;
            break;
        } 
    }
    
    DeinitFS();
    
    return exit_mode;
}
