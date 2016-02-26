#include "godmode.h"
#include "draw.h"
#include "hid.h"
#include "fs.h"

void DrawDirContents(DirStruct* contents, u32* offset, u32 cursor) {
    const int str_width = 40;
    const u32 stp_y = 12;
    const u32 pos_x = 0;
    u32 pos_y = 2;
    
    for (u32 i = 0; pos_y < SCREEN_HEIGHT; i++) {
        char tempstr[str_width + 1];
        u32 offset_i = *offset + i;
        u32 color_font;
        u32 color_bg;
        if (offset_i < contents->n_entries) {
            if (cursor != offset_i) {
                color_font = COLOR_GREY;
                color_bg = COLOR_BLACK;
            } else {
                color_font = COLOR_WHITE;
                color_bg = COLOR_BLACK;
            }
            snprintf(tempstr, str_width + 1, "%-*.*s", str_width, str_width, contents->entry[offset_i].name);
        } else {
            color_font = COLOR_WHITE;
            color_bg = COLOR_BLACK;
            snprintf(tempstr, str_width + 1, "%-*.*s", str_width, str_width, "");
        }
        DrawStringF(false, pos_x, pos_y, color_font, color_bg, tempstr);
        pos_y += stp_y;
    }
}

u32 GodMode() {
    u32 exit_mode = GODMODE_EXIT_REBOOT;
    char current_path[256] = { 0x00 };
    DirStruct* contents;
    u32 cursor = 0;
    u32 offset_disp = 0;
    
    ClearScreenFull(true, true, COLOR_BLACK);
    if (!InitFS()) return exit_mode;
    
    contents = GetDirContents("");
    while (true) { // this is the main loop
        DrawDirContents(contents, &offset_disp, cursor);
        u32 pad_state = InputWait();
        if (pad_state & BUTTON_DOWN) {
            cursor++;
            if (cursor >= contents->n_entries)
                cursor = contents->n_entries - 1;
        } else if ((pad_state & BUTTON_UP) && cursor) {
            cursor--;
        } else if ((pad_state & BUTTON_A) && (contents->entry[cursor].type == T_FAT_DIR)) {
            strncpy(current_path, contents->entry[cursor].path, 256);
            contents = GetDirContents(current_path);
            cursor = offset_disp = 0;
            ShowError(current_path);
        } else if (pad_state & BUTTON_B) {
            char* last_slash = strrchr(current_path, '/');
            if (last_slash) *last_slash = '\0'; 
            else *current_path = '\0';
            contents = GetDirContents(current_path);
            cursor = offset_disp = 0;
        }
        if (pad_state & BUTTON_START) {
            exit_mode = (pad_state & BUTTON_LEFT) ? GODMODE_EXIT_POWEROFF : GODMODE_EXIT_REBOOT;
            break;
        } 
    }
    
    DeinitFS();
    
    return exit_mode;
}
