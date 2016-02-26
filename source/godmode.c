#include "godmode.h"
#include "draw.h"
#include "hid.h"
#include "fs.h"

void DrawDirContents(DirStruct* contents, u32 offset, u32 cursor) {
    const int str_width = 40;
    const u32 stp_y = 12;
    const u32 pos_x = 0;
    u32 pos_y = 2;
    
    for (u32 i = 0; pos_y < SCREEN_HEIGHT; i++) {
        char tempstr[str_width + 1];
        u32 offset_i = offset + i;
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
    DirStruct* contents;
    
    ClearScreenFull(true, true, COLOR_BLACK);
    if (!InitFS()) {
        // ShowError("Could not initialize fs!");
        InputWait();
        return exit_mode;
    }
    
    contents = GetDirContents("");
    DrawDirContents(contents, 0, 0);
    InputWait();
    
    DeinitFS();
    
    return exit_mode;
}
