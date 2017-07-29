#include "godmode.h"
#include "ui.h"
#include "hid.h"
#include "fsinit.h"
#include "fsdrive.h"
#include "fsutil.h"
#include "fsperm.h"
#include "fsgame.h"
#include "fsscript.h"
#include "gameutil.h"
#include "keydbutil.h"
#include "nandutil.h"
#include "filetype.h"
#include "unittype.h"
#include "nand.h"
#include "virtual.h"
#include "vcart.h"
#include "game.h"
#include "nandcmac.h"
#include "ctrtransfer.h"
#include "ncchinfo.h"
#include "image.h"
#include "chainload.h"
#include "qlzcomp.h"
#include "timer.h"
#include "power.h"
#include "i2c.h"
#include QLZ_SPLASH_H

#define N_PANES 2

#define COLOR_TOP_BAR   (PERM_RED ? COLOR_RED : PERM_ORANGE ? COLOR_ORANGE : PERM_BLUE ? COLOR_BRIGHTBLUE : \
                         PERM_YELLOW ? COLOR_BRIGHTYELLOW : PERM_GREEN ? COLOR_GREEN : COLOR_WHITE)   
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

#define COLOR_TVOFFS    RGB(0x40, 0x60, 0x50)
#define COLOR_TVOFFSL   RGB(0x20, 0x40, 0x30)
#define COLOR_TVTEXT    RGB(0x30, 0x85, 0x30)

typedef struct {
    char path[256];
    u32 cursor;
    u32 scroll;
} PaneData;

static inline u32 LineLen(const char* text, u32 len, u32 ww, const char* line) {
    char* line0 = (char*) line;
    char* line1 = (char*) line;
    u32 llen = 0;
    
    // non wordwrapped length
    while ((line1 < (text + len)) && (*line1 != '\n') && *line1) line1++;
    while ((line1 > line0) && (*(line1-1) <= ' ')) line1--;
    llen = line1 - line0;
    if (ww && (llen > ww)) { // wordwrapped length
        for (llen = ww; (llen > 0) && (line[llen] != ' '); llen--); 
        if (!llen) llen = ww; // workaround for long strings
    }
    return llen;
}

static inline char* LineSeek(const char* text, u32 len, u32 ww, const char* line, int add) {
    // safety checks / 
    if (line < text) return NULL;
    if ((line >= (text + len)) && (add >= 0)) return (char*) line;
    
    if (!ww) { // non wordwrapped mode
        char* lf = ((char*) line - 1);
    
        // ensure we are at the start of the line
        while ((lf > text) && (*lf != '\n')) lf--;
        
        // handle backwards search
        for (; (add < 0) && (lf >= text); add++)
            for (lf--; (lf >= text) && (*lf != '\n'); lf--);
        
        // handle forwards search
        for (; (add > 0) && (lf < text + len); add--)
            for (lf++; (lf < text + len) && (*lf != '\n'); lf++);
        
        return lf + 1;
    } else { // wordwrapped mode
        char* l0 = (char*) line;
        
        // handle forwards wordwrapped search
        while ((add > 0) && (l0 < text + len)) {
            u32 llen = LineLen(text, len, 0, l0);
            for (; (add > 0) && (llen > ww); add--) {
                u32 llenww = LineLen(text, len, ww, l0);
                llen -= llenww;
                l0 += llenww;
            }
            if (add > 0) {
                l0 = LineSeek(text, len, 0, l0, 1);
                add--;
            }
        }
        
        // handle backwards wordwrapped search
        while ((add < 0) && (l0 > text)) {
            char* l1 = LineSeek(text, len, 0, l0, -1);
            int nlww = 0; // count wordwrapped lines in paragraph
            for (char* ld = l1; ld < l0; ld = LineSeek(text, len, ww, ld, 1), nlww++);
            if (add + nlww < 0) {
                add += nlww;
                l0 = l1;
            } else {
                l0 = LineSeek(text, len, ww, l1, nlww + add);
                add = 0;
            }
        }
        
        return l0;
    }
}

void DrawUserInterface(const char* curr_path, DirEntry* curr_entry, DirStruct* clipboard, u32 curr_pane) {
    const u32 n_cb_show = 8;
    const u32 bartxt_start = (FONT_HEIGHT_EXT == 10) ? 1 : 2;
    const u32 bartxt_x = 2;
    const u32 bartxt_rx = SCREEN_WIDTH_TOP - (19*FONT_WIDTH_EXT) - bartxt_x;
    const u32 info_start = (MAIN_SCREEN == TOP_SCREEN) ? 18 : 2; // leave space for the topbar when required
    const u32 instr_x = (SCREEN_WIDTH_MAIN - (34*FONT_WIDTH_EXT)) / 2;
    const u32 len_path = SCREEN_WIDTH_TOP - 120;
    const u32 len_info = (SCREEN_WIDTH_MAIN - ((SCREEN_WIDTH_MAIN >= 400) ? 80 : 20)) / 2;
    char tempstr[64];
    
    static u32 state_prev = 0xFFFFFFFF;
    u32 state_curr =
        ((*curr_path) ? (1<<0) : 0) |
        ((clipboard->n_entries) ? (1<<1) : 0) |
        ((CheckSDMountState()) ? (1<<2) : 0) |
        ((GetMountState()) ? (1<<3) : 0) |
        ((GetWritePermissions() > PERM_BASE) ? (1<<4) : 0) |
        (curr_pane<<5);
    
    if (state_prev != state_curr) {
        ClearScreenF(true, false, COLOR_STD_BG);
        state_prev = state_curr;
    }
    
    // top bar - current path & free/total storage
    DrawRectangle(TOP_SCREEN, 0, 0, SCREEN_WIDTH_TOP, 12, COLOR_TOP_BAR);
    if (strncmp(curr_path, "", 256) != 0) {
        char bytestr0[32];
        char bytestr1[32];
        TruncateString(tempstr, curr_path, len_path / FONT_WIDTH_EXT, 8);
        DrawStringF(TOP_SCREEN, bartxt_x, bartxt_start, COLOR_STD_BG, COLOR_TOP_BAR, tempstr);
        DrawStringF(TOP_SCREEN, bartxt_rx, bartxt_start, COLOR_STD_BG, COLOR_TOP_BAR, "%19.19s", "LOADING...");
        FormatBytes(bytestr0, GetFreeSpace(curr_path));
        FormatBytes(bytestr1, GetTotalSpace(curr_path));
        snprintf(tempstr, 64, "%s/%s", bytestr0, bytestr1);
        DrawStringF(TOP_SCREEN, bartxt_rx, bartxt_start, COLOR_STD_BG, COLOR_TOP_BAR, "%19.19s", tempstr);
    } else {
        DrawStringF(TOP_SCREEN, bartxt_x, bartxt_start, COLOR_STD_BG, COLOR_TOP_BAR, "[root]");
        DrawStringF(TOP_SCREEN, bartxt_rx, bartxt_start, COLOR_STD_BG, COLOR_TOP_BAR, "%19.19s", FLAVOR);
    }
    
    // left top - current file info
    if (curr_pane) snprintf(tempstr, 63, "PANE #%lu", curr_pane);
    else snprintf(tempstr, 63, "CURRENT");
    DrawStringF(MAIN_SCREEN, 2, info_start, COLOR_STD_FONT, COLOR_STD_BG, "[%s]", tempstr);
    // file / entry name
    ResizeString(tempstr, curr_entry->name, len_info / FONT_WIDTH_EXT, 8, false);
    u32 color_current = COLOR_ENTRY(curr_entry);
    DrawStringF(MAIN_SCREEN, 4, info_start + 12, color_current, COLOR_STD_BG, "%s", tempstr);
    // size (in Byte) or type desc
    if (curr_entry->type == T_DIR) {
        ResizeString(tempstr, "(dir)", len_info / FONT_WIDTH_EXT, 8, false);
    } else if (curr_entry->type == T_DOTDOT) {
        snprintf(tempstr, 21, "%20s", "");
    } else if (curr_entry->type == T_ROOT) {
        int drvtype = DriveType(curr_entry->path);
        char drvstr[32];
        snprintf(drvstr, 31, "(%s%s)", 
            ((drvtype & DRV_SDCARD) ? "SD" : (drvtype & DRV_RAMDRIVE) ? "RAMdrive" : (drvtype & DRV_GAME) ? "Game" :
            (drvtype & DRV_SYSNAND) ? "SysNAND" : (drvtype & DRV_EMUNAND) ? "EmuNAND" : (drvtype & DRV_IMAGE) ? "Image" :
            (drvtype & DRV_XORPAD) ? "XORpad" : (drvtype & DRV_MEMORY) ? "Memory" : (drvtype & DRV_ALIAS) ? "Alias" :
            (drvtype & DRV_CART) ? "Gamecart" : (drvtype & DRV_SEARCH) ? "Search" : ""),
            ((drvtype & DRV_FAT) ? " FAT" : (drvtype & DRV_VIRTUAL) ? " Virtual" : ""));
        ResizeString(tempstr, drvstr, len_info / FONT_WIDTH_EXT, 8, false);
    } else {
        char numstr[32];
        char bytestr[32];
        FormatNumber(numstr, curr_entry->size);
        snprintf(bytestr, 31, "%s Byte", numstr);
        ResizeString(tempstr, bytestr, len_info / FONT_WIDTH_EXT, 8, false);
    }
    DrawStringF(MAIN_SCREEN, 4, info_start + 12 + 10, color_current, COLOR_STD_BG, tempstr);
    // path of file (if in search results)
    if ((DriveType(curr_path) & DRV_SEARCH) && strrchr(curr_entry->path, '/')) {
        char dirstr[256];
        strncpy(dirstr, curr_entry->path, 256);
        *(strrchr(dirstr, '/')+1) = '\0';
        ResizeString(tempstr, dirstr, len_info / FONT_WIDTH_EXT, 8, false);
        DrawStringF(MAIN_SCREEN, 4, info_start + 12 + 10 + 10, color_current, COLOR_STD_BG, tempstr);
    } else {
        ResizeString(tempstr, "", len_info / FONT_WIDTH_EXT, 8, false);
        DrawStringF(MAIN_SCREEN, 4, info_start + 12 + 10 + 10, color_current, COLOR_STD_BG, tempstr);
    }
    
    // right top - clipboard
    DrawStringF(MAIN_SCREEN, SCREEN_WIDTH_MAIN - len_info, info_start, COLOR_STD_FONT, COLOR_STD_BG, "%*s",
        len_info / FONT_WIDTH_EXT, (clipboard->n_entries) ? "[CLIPBOARD]" : "");
    for (u32 c = 0; c < n_cb_show; c++) {
        u32 color_cb = COLOR_ENTRY(&(clipboard->entry[c]));
        ResizeString(tempstr, (clipboard->n_entries > c) ? clipboard->entry[c].name : "", len_info / FONT_WIDTH_EXT, 8, true);
        DrawStringF(MAIN_SCREEN, SCREEN_WIDTH_MAIN - len_info - 4, info_start + 12 + (c*10), color_cb, COLOR_STD_BG, tempstr);
    }
    *tempstr = '\0';
    if (clipboard->n_entries > n_cb_show) snprintf(tempstr, 60, "+ %lu more", clipboard->n_entries - n_cb_show);
    DrawStringF(MAIN_SCREEN, SCREEN_WIDTH_MAIN - len_info - 4, info_start + 12 + (n_cb_show*10), COLOR_DARKGREY, COLOR_STD_BG,
        "%*s", len_info / FONT_WIDTH_EXT, tempstr);
    
    // bottom: inctruction block
    char instr[512];
    snprintf(instr, 512, "%s\n%s%s%s%s%s%s%s%s",
        FLAVOR " Explorer v"VERSION, // generic start part
        (*curr_path) ? ((clipboard->n_entries == 0) ? "L - MARK files (use with \x18\x19\x1A\x1B)\nX - DELETE / [+R] RENAME file(s)\nY - COPY file(s) / [+R] CREATE dir\n" :
        "L - MARK files (use with \x18\x19\x1A\x1B)\nX - DELETE / [+R] RENAME file(s)\nY - PASTE file(s) / [+R] CREATE dir\n") :
        ((GetWritePermissions() > PERM_BASE) ? "R+Y - Relock write permissions\n" : ""),
        (*curr_path) ? "" : (GetMountState()) ? "R+X - Unmount image\n" : "",
        (*curr_path) ? "" : (CheckSDMountState()) ? "R+B - Unmount SD card\n" : "R+B - Remount SD card\n",
        (*curr_path) ? "R+A - Directory options\n" : "R+A - Drive options\n", 
        "R+L - Make a Screenshot\n",
        "R+\x1B\x1A - Switch to prev/next pane\n",
        (clipboard->n_entries) ? "SELECT - Clear Clipboard\n" : "SELECT - Restore Clipboard\n", // only if clipboard is full
        "START - Reboot / [+R] Poweroff\nHOME button for HOME menu"); // generic end part
    DrawStringF(MAIN_SCREEN, instr_x, SCREEN_HEIGHT - 4 - GetDrawStringHeight(instr), COLOR_STD_FONT, COLOR_STD_BG, instr);
}

void DrawDirContents(DirStruct* contents, u32 cursor, u32* scroll) {
    const int str_width = (SCREEN_WIDTH_ALT-3) / FONT_WIDTH_EXT;
    const u32 bar_height_min = 32;
    const u32 bar_width = 2;
    const u32 stp_y = 12;
    const u32 start_y = (MAIN_SCREEN == TOP_SCREEN) ? 2 : 2 + stp_y;
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
        DrawStringF(ALT_SCREEN, pos_x, pos_y, color_font, COLOR_STD_BG, tempstr);
        pos_y += stp_y;
    }
    
    if (contents->n_entries > lines) { // draw position bar at the right      
        u32 bar_height = (lines * SCREEN_HEIGHT) / contents->n_entries;
        if (bar_height < bar_height_min) bar_height = bar_height_min;
        u32 bar_pos = ((u64) *scroll * (SCREEN_HEIGHT - bar_height)) / (contents->n_entries - lines);
        
        DrawRectangle(ALT_SCREEN, SCREEN_WIDTH_ALT - bar_width, 0, bar_width, bar_pos, COLOR_STD_BG);
        DrawRectangle(ALT_SCREEN, SCREEN_WIDTH_ALT - bar_width, bar_pos + bar_height, bar_width, SCREEN_WIDTH_ALT - (bar_pos + bar_height), COLOR_STD_BG);
        DrawRectangle(ALT_SCREEN, SCREEN_WIDTH_ALT - bar_width, bar_pos, bar_width, bar_height, COLOR_SIDE_BAR);
    } else DrawRectangle(ALT_SCREEN, SCREEN_WIDTH_ALT - bar_width, 0, bar_width, SCREEN_HEIGHT, COLOR_STD_BG);
}

u32 SdFormatMenu(void) {
    const u32 cluster_size_table[5] = { 0x0, 0x0, 0x4000, 0x8000, 0x10000 };
    const char* option_emunand_size[6] = { "No EmuNAND", "RedNAND size (min)", "EmuNAND size (full)", "User input..." };
    const char* option_cluster_size[4] = { "Auto", "16KB Clusters", "32KB Clusters", "64KB Clusters" };
    u64 sysnand_size_mb = (((u64)GetNandSizeSectors(NAND_SYSNAND) * 0x200) + 0xFFFFF) / 0x100000;
    u64 sysnand_min_size_mb = (((u64)GetNandMinSizeSectors(NAND_SYSNAND) * 0x200) + 0xFFFFF) / 0x100000;
    char label[16] = "0:GM9SD";
    u32 cluster_size = 0;
    u64 sdcard_size_mb = 0;
    u64 emunand_size_mb = (u64) -1;
    u32 user_select;
    
    // check actual SD card size
    sdcard_size_mb = GetSDCardSize() / 0x100000;
    if (!sdcard_size_mb) {
        ShowPrompt(false, "Error: SD card not detected.");
        return 1;
    }
    
    user_select = ShowSelectPrompt(4, option_emunand_size, "Format SD card (%lluMB)?\nChoose EmuNAND size:", sdcard_size_mb);
    if (user_select && (user_select < 4)) {
        emunand_size_mb = (user_select == 2) ? sysnand_min_size_mb : (user_select == 3) ? sysnand_size_mb : 0;
    } else if (user_select == 4) do {
        emunand_size_mb = ShowNumberPrompt(sysnand_min_size_mb, "SD card size is %lluMB.\nEnter EmuNAND size (MB) below:", sdcard_size_mb);
        if (emunand_size_mb == (u64) -1) break;
    } while (emunand_size_mb > sdcard_size_mb);
    if (emunand_size_mb == (u64) -1) return 1;
    
    user_select = ShowSelectPrompt(4, option_cluster_size, "Format SD card (%lluMB)?\nChoose cluster size:", sdcard_size_mb);
    if (!user_select) return 1;
    else cluster_size = cluster_size_table[user_select];
    
    if (!ShowStringPrompt(label + 2, 9, "Format SD card (%lluMB)?\nEnter label:", sdcard_size_mb))
        return 1;
    
    if (!FormatSDCard(emunand_size_mb, cluster_size, label)) {
        ShowPrompt(false, "Format SD: failed!");
        return 1;
    }
    
    if (emunand_size_mb >= sysnand_min_size_mb) {
        const char* option_emunand_type[3] = { "RedNAND type", "GW EmuNAND type", "Don't set up" };
        if (emunand_size_mb >= sysnand_size_mb)
            user_select = ShowSelectPrompt(3, option_emunand_type, "Choose EmuNAND type to set up:");
        else user_select = ShowPrompt(true, "Clone SysNAND to RedNAND now?") ? 1 : 0;
        if (!user_select || (user_select > 2)) return 0;
        
        u8 ncsd[0x200];
        u32 flags = OVERRIDE_PERM;
        InitSDCardFS(); // this has to be initialized for EmuNAND to work
        SetEmuNandBase((user_select == 2) ? 0 : 1); // 0 -> GW EmuNAND
        if ((ReadNandSectors(ncsd, 0, 1, 0xFF, NAND_SYSNAND) != 0) ||
            (WriteNandSectors(ncsd, 0, 1, 0xFF, NAND_EMUNAND) != 0) ||
            (!PathCopy("E:", "S:/nand_minsize.bin", &flags)))
            ShowPrompt(false, "Cloning SysNAND to EmuNAND: failed!");
        DeinitSDCardFS();
    }
    
    return 0;
}

u32 FileTextViewer(const char* path) {
    const u32 vpad = 1;
    const u32 hpad = 0;
    const u32 lnos = 4;
    u32 ww = 0;
    
    // load text file (completely into memory)
    char* text = (char*) TEMP_BUFFER;
    u32 flen = FileGetData(path, text, TEMP_BUFFER_SIZE, 0);
    u32 len = 0; // actual length may be shorter due to zero symbol
    for (len = 0; (len < flen) && text[len]; len++);
    
    // check if this really is a text file
    if (!ValidateText(text, len)) {
        ShowPrompt(false, "Error: Not a valid text file");
        return 1;
    }
    
    // clear screens
    ClearScreenF(true, true, COLOR_STD_BG);
    
    // instructions
    static const char* instr = "Textviewer Controls:\n \n\x18\x19\x1A\x1B(+R) - Scroll\nR+Y - Toggle wordwrap\nR+X - Goto line #\nB - Exit\n";
    ShowString(instr);
    
    // no of lines and length to display
    u32 nlin_disp = SCREEN_HEIGHT / (FONT_HEIGHT_EXT + (2*vpad));
    u32 llen_disp = (SCREEN_WIDTH_TOP - (2*hpad)) / FONT_WIDTH_EXT;
    
    // block placements
    const char* al_str = "<< ";
    const char* ar_str = " >>";
    if (lnos) llen_disp -= (lnos + 1); // make room for line numbers
    u32 x_txt = (lnos) ? hpad + ((lnos+1)*FONT_WIDTH_EXT) : hpad;
    u32 x_lno = hpad;
    u32 p_al = 0;
    u32 p_ar = llen_disp - strnlen(ar_str, 16);
    u32 x_al = x_txt + (p_al * FONT_WIDTH_EXT);
    u32 x_ar = x_txt + (p_ar * FONT_WIDTH_EXT);
    
    // find maximum line len
    u32 llen_max = 0;
    for (char* ptr = (char*) text; ptr < (text + len); ptr = LineSeek(text, len, 0, ptr, 1)) {
        u32 llen = LineLen(text, len, 0, ptr);
        if (llen > llen_max) llen_max = llen;
    }
    
    // find last allowed lines (ww and nonww)
    char* llast_nww = LineSeek(text, len, 0, text + len, -nlin_disp);
    char* llast_ww = LineSeek(text, len, llen_disp, text + len, -nlin_disp);
    
    char* line0 = (char*) text;
    int lcurr = 1;
    int off_disp = 0;
    while (true) {
        // display text on screen
        char txtstr[128]; // should be more than enough
        char* ptr = line0;
        u32 nln = lcurr;
        for (u32 y = vpad; y < SCREEN_HEIGHT; y += FONT_HEIGHT_EXT + (2*vpad)) {
            char* ptr_next = LineSeek(text, len, ww, ptr, 1);
            u32 llen = LineLen(text, len, ww, ptr);
            u32 ncpy = ((int) llen < off_disp) ? 0 : (llen - off_disp);
            if (ncpy > llen_disp) ncpy = llen_disp;
            bool al = !ww && off_disp;
            bool ar = !ww && (llen > off_disp + llen_disp);
            
            // build text string
            snprintf(txtstr, llen_disp + 1, "%-*.*s", (int) llen_disp, (int) llen_disp, "");
            if (ncpy) memcpy(txtstr, ptr + off_disp, ncpy);
            for (char* d = txtstr; *d; d++) if (*d < ' ') *d = ' ';
            if (al) memcpy(txtstr + p_al, al_str, strnlen(al_str, 16));
            if (ar) memcpy(txtstr + p_ar, ar_str, strnlen(ar_str, 16));
            
            // draw line number & text
            DrawStringF(TOP_SCREEN, x_txt, y, COLOR_TVTEXT, COLOR_STD_BG, txtstr);
            if (lnos && (ptr != ptr_next)) DrawStringF(TOP_SCREEN, x_lno, y,
                ((ptr == text) || (*(ptr-1) == '\n')) ? COLOR_TVOFFS : COLOR_TVOFFSL, COLOR_STD_BG, "%0*lu", lnos, nln);
            else DrawStringF(TOP_SCREEN, x_lno, y, COLOR_TVOFFSL, COLOR_STD_BG, "%*.*s", lnos, lnos, " ");
            
            // colorize arrows
            if (al) DrawStringF(TOP_SCREEN, x_al, y, COLOR_TVOFFS, COLOR_TRANSPARENT, al_str);
            if (ar) DrawStringF(TOP_SCREEN, x_ar, y, COLOR_TVOFFS, COLOR_TRANSPARENT, ar_str);
            
            // advance pointer / line number
            for (char* c = ptr; c < ptr_next; c++) if (*c == '\n') ++nln;
            ptr = ptr_next;
        }
        
        // handle user input
        u32 pad_state = InputWait();
        if ((pad_state & BUTTON_R1) && (pad_state & BUTTON_L1)) CreateScreenshot();
        else { // standard viewer mode
            char* line0_next = line0;
            u32 step_ud = (pad_state & BUTTON_R1) ? nlin_disp : 1;
            u32 step_lr = (pad_state & BUTTON_R1) ? llen_disp : 1;
            bool switched = (pad_state & BUTTON_R1);
            if (pad_state & BUTTON_DOWN) line0_next = LineSeek(text, len, ww, line0, step_ud);
            else if (pad_state & BUTTON_UP) line0_next = LineSeek(text, len, ww, line0, -step_ud);
            else if (pad_state & BUTTON_RIGHT) off_disp += step_lr;
            else if (pad_state & BUTTON_LEFT) off_disp -= step_lr;
            else if (switched && (pad_state & BUTTON_X)) {
                u64 lnext64 = ShowNumberPrompt(lcurr, "Current line: %i\nEnter new line below.", lcurr);
                if (lnext64 && (lnext64 != (u64) -1)) line0_next = LineSeek(text, len, 0, line0, (int) lnext64 - lcurr);
                ShowString(instr);
            } else if (switched && (pad_state & BUTTON_Y)) {
                ww = ww ? 0 : llen_disp;
                line0_next = LineSeek(text, len, ww, line0, 0);
            } else if (pad_state & (BUTTON_B|BUTTON_START)) break;
            
            // check for problems, apply changes
            if (!ww && (line0_next > llast_nww)) line0_next = llast_nww;
            else if (ww && (line0_next > llast_ww)) line0_next = llast_ww;
            if (line0_next < line0) { // fix line number for decrease
                do if (*(--line0) == '\n') lcurr--;
                while (line0 > line0_next);
            } else { // fix line number for increase / same
                for (; line0_next > line0; line0++)
                    if (*line0 == '\n') lcurr++;
            }
            if (off_disp + llen_disp > llen_max) off_disp = llen_max - llen_disp;
            if ((off_disp < 0) || ww) off_disp = 0;
        }
    }
    
    // clear screens
    ClearScreenF(true, true, COLOR_STD_BG);
    
    return 0;
}

u32 FileHexViewer(const char* path) {
    static const u32 max_data = (SCREEN_HEIGHT / 8) * 16;
    static u32 mode = 0;
    u8* data = TEMP_BUFFER;
    u8* bottom_cpy = TEMP_BUFFER + 0xC0000; // a copy of the bottom screen framebuffer
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
    u8* edit_buffer = TEMP_BUFFER;
    u8* edit_buffer_cpy = TEMP_BUFFER + edit_bsize;
    u32 edit_start;
    int cursor = 0;
    
    static bool show_instr = true;
    static const char* instr = "Hexeditor Controls:\n \n\x18\x19\x1A\x1B(+R) - Scroll\nR+Y - Switch view\nX - Search / goto...\nA - Enter edit mode\nA+\x18\x19\x1A\x1B - Edit value\nB - Exit\n";
    if (show_instr) { // show one time instructions
        ShowPrompt(false, instr);
        show_instr = false;
    }
    
    if (MAIN_SCREEN != TOP_SCREEN) ShowString(instr);
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
            ClearScreen(TOP_SCREEN, COLOR_STD_BG);
            if (dual_screen) ClearScreen(BOT_SCREEN, COLOR_STD_BG);
            else memcpy(BOT_SCREEN, bottom_cpy, (SCREEN_HEIGHT * SCREEN_WIDTH_BOT * 3));
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
                if (found_offset == (u32) -1) {
                    ShowPrompt(false, "Not found!");
                    found_size = 0;
                } else offset = found_offset;
                if (MAIN_SCREEN == TOP_SCREEN) ClearScreen(TOP_SCREEN, COLOR_STD_BG);
                else if (dual_screen) ClearScreen(BOT_SCREEN, COLOR_STD_BG);
                else memcpy(BOT_SCREEN, bottom_cpy, (SCREEN_HEIGHT * SCREEN_WIDTH_BOT * 3));
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
                        if (found_offset == (u32) -1) {
                            ShowPrompt(false, "Not found!");
                            found_size = 0;
                        } else offset = found_offset;
                        if (MAIN_SCREEN == TOP_SCREEN) ClearScreen(TOP_SCREEN, COLOR_STD_BG);
                        else if (dual_screen) ClearScreen(BOT_SCREEN, COLOR_STD_BG);
                        else memcpy(BOT_SCREEN, bottom_cpy, (SCREEN_HEIGHT * SCREEN_WIDTH_BOT * 3));
                    }
                } else if (user_select == 3) {
                    u8 data[64] = { 0 };
                    u32 size = 0;
                    if (found_size) size = FileGetData(path, data, (found_size <= 64) ? found_size : 64, found_offset);
                    if (ShowDataPrompt(data, &size, "Enter search data below.\n(R+X to repeat search)", (unsigned int) offset)) {
                        found_size = size;
                        found_offset = FileFindData(path, data, size, offset);
                        if (found_offset == (u32) -1) {
                            ShowPrompt(false, "Not found!");
                            found_size = 0;
                        } else offset = found_offset;
                        if (MAIN_SCREEN == TOP_SCREEN) ClearScreen(TOP_SCREEN, COLOR_STD_BG);
                        else if (dual_screen) ClearScreen(BOT_SCREEN, COLOR_STD_BG);
                        else memcpy(BOT_SCREEN, bottom_cpy, (SCREEN_HEIGHT * SCREEN_WIDTH_BOT * 3));
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
                data = TEMP_BUFFER;
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
    
    ClearScreen(TOP_SCREEN, COLOR_STD_BG);
    if (MAIN_SCREEN == TOP_SCREEN) memcpy(BOT_SCREEN, bottom_cpy, (SCREEN_HEIGHT * SCREEN_WIDTH_BOT * 3));
    else ClearScreen(BOT_SCREEN, COLOR_STD_BG);
    
    return 0;
}

u32 Sha256Calculator(const char* path) {
    u32 drvtype = DriveType(path);
    char pathstr[32 + 1];
    u8 sha256[32];
    TruncateString(pathstr, path, 32, 8);
    if (!FileGetSha256(path, sha256)) {
        ShowPrompt(false, "Calculating SHA-256: failed!");
        return 1;
    } else {
        static char pathstr_prev[32 + 1] = { 0 };
        static u8 sha256_prev[32] = { 0 };
        char sha_path[256];
        u8 sha256_file[32];
        
        snprintf(sha_path, 256, "%s.sha", path);
        bool have_sha = (FileGetData(sha_path, sha256_file, 32, 0) == 32);
        bool write_sha = !have_sha && (drvtype & DRV_SDCARD); // writing only on SD
        if (ShowPrompt(write_sha, "%s\n%016llX%016llX\n%016llX%016llX%s%s%s%s%s",
            pathstr, getbe64(sha256 + 0), getbe64(sha256 + 8), getbe64(sha256 + 16), getbe64(sha256 + 24),
            (have_sha) ? "\nSHA verification: " : "",
            (have_sha) ? ((memcmp(sha256, sha256_file, 32) == 0) ? "passed!" : "failed!") : "",
            (memcmp(sha256, sha256_prev, 32) == 0) ? "\n \nIdentical with previous file:\n" : "",
            (memcmp(sha256, sha256_prev, 32) == 0) ? pathstr_prev : "",
            (write_sha) ? "\n \nWrite .SHA file?" : "") && !have_sha && write_sha) {
            FileSetData(sha_path, sha256, 32, 0, true);
        }
        
        strncpy(pathstr_prev, pathstr, 32 + 1);
        memcpy(sha256_prev, sha256, 32);
    }
    
    return 0;
}

u32 CmacCalculator(const char* path) {
    char pathstr[32 + 1];
    u8 cmac[16];
    TruncateString(pathstr, path, 32, 8);
    if (CalculateFileCmac(path, cmac) != 0) {
        ShowPrompt(false, "Calculating CMAC: failed!");
        return 1;
    } else {
        u8 cmac_file[16];
        bool identical = ((ReadFileCmac(path, cmac_file) == 0) && (memcmp(cmac, cmac_file, 16) == 0));
        if (ShowPrompt(!identical, "%s\n%016llX%016llX\n%s%s%s",
            pathstr, getbe64(cmac + 0), getbe64(cmac + 8),
            "CMAC verification: ", (identical) ? "passed!" : "failed!",
            (!identical) ? "\n \nFix CMAC in file?" : "") &&
            !identical && (WriteFileCmac(path, cmac) != 0)) {
            ShowPrompt(false, "Fixing CMAC: failed!");
        }
    }
    
    return 0;
}

u32 StandardCopy(u32* cursor, DirStruct* current_dir) {
    DirEntry* curr_entry = &(current_dir->entry[*cursor]);
    u32 n_marked = 0;
    if (curr_entry->marked) {
        for (u32 i = 0; i < current_dir->n_entries; i++) 
            if (current_dir->entry[i].marked) n_marked++;
    }
    
    u32 flags = BUILD_PATH;
    if ((n_marked > 1) && ShowPrompt(true, "Copy all %lu selected items?", n_marked)) {
        u32 n_success = 0;
        for (u32 i = 0; i < current_dir->n_entries; i++) {
            const char* path = current_dir->entry[i].path;
            if (!current_dir->entry[i].marked) 
                continue;
            flags |= ASK_ALL;
            current_dir->entry[i].marked = false;
            if (PathCopy(OUTPUT_PATH, path, &flags)) n_success++;
            else { // on failure: set cursor on failed item, break;
                char currstr[32+1];
                TruncateString(currstr, path, 32, 12);
                ShowPrompt(false, "%s\nFailed copying item", currstr);
                *cursor = i;
                break;
            }
        }
        if (n_success) ShowPrompt(false, "%lu items copied to %s", n_success, OUTPUT_PATH);
    } else {
        char pathstr[32+1];
        TruncateString(pathstr, curr_entry->path, 32, 8);
        if (!PathCopy(OUTPUT_PATH, curr_entry->path, &flags))
            ShowPrompt(false, "%s\nFailed copying item", pathstr);
        else ShowPrompt(false, "%s\nCopied to %s", pathstr, OUTPUT_PATH);
    }
    
    return 0;
}

u32 FileHandlerMenu(char* current_path, u32* cursor, u32* scroll, DirStruct* current_dir, DirStruct* clipboard) {
    DirEntry* curr_entry = &(current_dir->entry[*cursor]);
    const char* optionstr[16];
    
    // check for file lock
    if (!FileUnlock(curr_entry->path)) return 1;
    
    u32 filetype = IdentifyFileType(curr_entry->path);
    u32 drvtype = DriveType(curr_entry->path);
    
    bool in_output_path = (strncmp(current_path, OUTPUT_PATH, 256) == 0);
    
    // special stuff, only available for known filetypes (see int special below)
    bool mountable = (FTYPE_MOUNTABLE(filetype) && !(drvtype & DRV_IMAGE));
    bool verificable = (FYTPE_VERIFICABLE(filetype));
    bool decryptable = (FYTPE_DECRYPTABLE(filetype));
    bool encryptable = (FYTPE_ENCRYPTABLE(filetype));
    bool cryptable_inplace = ((encryptable||decryptable) && !in_output_path && (drvtype & DRV_FAT));
    bool cia_buildable = (FTYPE_CIABUILD(filetype));
    bool cia_buildable_legit = (FTYPE_CIABUILD_L(filetype));
    bool cxi_dumpable = (FTYPE_CXIDUMP(filetype));
    bool tik_buildable = (FTYPE_TIKBUILD(filetype)) && !in_output_path;
    bool key_buildable = (FTYPE_KEYBUILD(filetype)) && !in_output_path;
    bool titleinfo = (FTYPE_TITLEINFO(filetype));
    bool renamable = (FTYPE_RENAMABLE(filetype));
    bool transferable = (FTYPE_TRANSFERABLE(filetype) && IS_A9LH && (drvtype & DRV_FAT));
    bool hsinjectable = (FTYPE_HSINJECTABLE(filetype));
    bool restorable = (FTYPE_RESTORABLE(filetype) && IS_A9LH && !(drvtype & DRV_SYSNAND));
    bool ebackupable = (FTYPE_EBACKUP(filetype));
    bool xorpadable = (FTYPE_XORPAD(filetype));
    bool scriptable = (FTYPE_SCRIPT(filetype));
    bool launchable = ((FTYPE_PAYLOAD(filetype)) && (drvtype & DRV_FAT) && !IS_SIGHAX);
    bool bootable = ((FTYPE_BOOTABLE(filetype)) && !PathExist("0:/bootonce.firm") && IS_SIGHAX); // works only with boot9strap nightly
    bool special_opt = mountable || verificable || decryptable || encryptable || cia_buildable || cia_buildable_legit || cxi_dumpable ||
        tik_buildable || key_buildable || titleinfo || renamable || transferable || hsinjectable || restorable || xorpadable ||
        ebackupable || launchable || bootable || scriptable;
    
    char pathstr[32+1];
    TruncateString(pathstr, curr_entry->path, 32, 8);
    
    u32 n_marked = 0;
    if (curr_entry->marked) {
        for (u32 i = 0; i < current_dir->n_entries; i++) 
            if (current_dir->entry[i].marked) n_marked++;
    }
    
    // main menu processing
    int n_opt = 0;
    int special = (special_opt) ? ++n_opt : -1;
    int hexviewer = ++n_opt;
    int textviewer = (filetype & TXT_GENERIC) ? ++n_opt : -1;
    int calcsha = ++n_opt;
    int calccmac = (CheckCmacPath(curr_entry->path) == 0) ? ++n_opt : -1;
    int copystd = (!in_output_path) ? ++n_opt : -1;
    int inject = ((clipboard->n_entries == 1) &&
        (clipboard->entry[0].type == T_FILE) &&
        (drvtype & DRV_FAT) &&
        (strncmp(clipboard->entry[0].path, curr_entry->path, 256) != 0)) ?
        (int) ++n_opt : -1;
    int searchdrv = (DriveType(current_path) & DRV_SEARCH) ? ++n_opt : -1;
    if (special > 0) optionstr[special-1] =
        (filetype & IMG_NAND  ) ? "NAND image options..." :
        (filetype & IMG_FAT   ) ? (transferable) ? "CTRNAND options..." : "Mount as FAT image" :
        (filetype & GAME_CIA  ) ? "CIA image options..."  :
        (filetype & GAME_NCSD ) ? "NCSD image options..." :
        (filetype & GAME_NCCH ) ? "NCCH image options..." :
        (filetype & GAME_EXEFS) ? "Mount as EXEFS image"  :
        (filetype & GAME_ROMFS) ? "Mount as ROMFS image"  :
        (filetype & GAME_TMD  ) ? "TMD file options..."   :
        (filetype & GAME_BOSS ) ? "BOSS file options..."  :
        (filetype & GAME_NUSCDN)? "Decrypt NUS/CDN file"  :
        (filetype & GAME_SMDH)  ? "Show SMDH title info"  :
        (filetype & GAME_NDS)   ? "NDS image options..."  :
        (filetype & GAME_TICKET)? "Ticket options..."     :
        (filetype & SYS_FIRM  ) ? "FIRM image options..." :
        (filetype & SYS_TICKDB) ? (tik_buildable) ? "Ticket.db options..." : "Mount as ticket.db" :
        (filetype & BIN_TIKDB)  ? "Titlekey options..."   :
        (filetype & BIN_KEYDB)  ? "AESkeydb options..."   :
        (filetype & BIN_LEGKEY) ? "Build " KEYDB_NAME     :
        (filetype & BIN_NCCHNFO)? "NCCHinfo options..."   :
        (filetype & BIN_LAUNCH) ? "Launch as arm9 payload" :
        (filetype & TXT_SCRIPT) ? "Execute GM9 script" : "???";
    optionstr[hexviewer-1] = "Show in Hexeditor";
    optionstr[calcsha-1] = "Calculate SHA-256";
    if (textviewer > 0) optionstr[textviewer-1] = "Show in Textviewer";
    if (calccmac > 0) optionstr[calccmac-1] = "Calculate CMAC";
    if (copystd > 0) optionstr[copystd-1] = "Copy to " OUTPUT_PATH;
    if (inject > 0) optionstr[inject-1] = "Inject data @offset";
    if (searchdrv > 0) optionstr[searchdrv-1] = "Open containing folder";
    
    int user_select = ShowSelectPrompt(n_opt, optionstr, (n_marked > 1) ?
        "%s\n%(%lu files selected)" : "%s", pathstr, n_marked);
    if (user_select == hexviewer) { // -> show in hex viewer
        FileHexViewer(curr_entry->path);
        return 0;
    } else if (user_select == textviewer) { // -> show in text viewer
        FileTextViewer(curr_entry->path);
        return 0;
    } else if (user_select == calcsha) { // -> calculate SHA-256
        Sha256Calculator(curr_entry->path);
        GetDirContents(current_dir, current_path);
        return 0;
    } else if (user_select == calccmac) { // -> calculate CMAC
        optionstr[0] = "Check current CMAC only";
        optionstr[1] = "Verify CMAC for all";
        optionstr[2] = "Fix CMAC for all";
        user_select = (n_marked > 1) ? ShowSelectPrompt(3, optionstr, "%s\n%(%lu files selected)", pathstr, n_marked) : 1;
        if (user_select == 1) {
            CmacCalculator(curr_entry->path);
            return 0;
        } else if ((user_select == 2) || (user_select == 3)) {
            bool fix = (user_select == 3);
            u32 n_processed = 0;
            u32 n_success = 0;
            u32 n_fixed = 0;
            u32 n_nocmac = 0;
            for (u32 i = 0; i < current_dir->n_entries; i++) {
                const char* path = current_dir->entry[i].path;
                if (!current_dir->entry[i].marked) continue;
                if (!ShowProgress(n_processed++, n_marked, path)) break;
                if (CheckCmacPath(path) != 0) {
                    n_nocmac++;
                    continue;
                }
                if (CheckFileCmac(path) == 0) n_success++;
                else if (fix && (FixFileCmac(path) == 0)) n_fixed++;
                else { // on failure: set cursor on failed file
                    *cursor = i;
                    continue;
                }
                current_dir->entry[i].marked = false;
            }
            if (n_fixed) {
                if (n_nocmac) ShowPrompt(false, "%lu/%lu/%lu files ok/fixed/total\n%lu/%lu have no CMAC",
                    n_success, n_fixed, n_marked, n_nocmac, n_marked);
                 else ShowPrompt(false, "%lu/%lu files verified ok\n%lu/%lu files fixed",
                    n_success, n_marked, n_fixed, n_marked);
            } else {
                if (n_nocmac) ShowPrompt(false, "%lu/%lu files verified ok\n%lu/%lu have no CMAC",
                    n_success, n_marked, n_nocmac, n_marked);
                else ShowPrompt(false, "%lu/%lu files verified ok", n_success, n_marked);
            }
            return 0;
        }
        return FileHandlerMenu(current_path, cursor, scroll, current_dir, clipboard);
    } else if (user_select == copystd) { // -> copy to OUTPUT_PATH
        StandardCopy(cursor, current_dir);
        return 0;
    } else if (user_select == inject) { // -> inject data from clipboard
        char origstr[18 + 1];
        TruncateString(origstr, clipboard->entry[0].name, 18, 10);
        u64 offset = ShowHexPrompt(0, 8, "Inject data from %s?\nSpecifiy offset below.", origstr);
        if (offset != (u64) -1) {
            if (!FileInjectFile(curr_entry->path, clipboard->entry[0].path, (u32) offset, 0, 0, NULL))
                ShowPrompt(false, "Failed injecting %s", origstr);
            clipboard->n_entries = 0;
        }
        return 0;
    } else if (user_select == searchdrv) { // -> search drive, open containing path
        char* last_slash = strrchr(curr_entry->path, '/');
        if (last_slash) {
            snprintf(current_path, last_slash - curr_entry->path + 1, "%s", curr_entry->path);
            GetDirContents(current_dir, current_path);
            *cursor = 1;
            *scroll = 0;
        }
        return 0;
    } else if (user_select != special) {
        return 1;
    }
    
    // stuff for special menu starts here
    n_opt = 0;
    int show_info = (titleinfo) ? ++n_opt : -1;
    int mount = (mountable) ? ++n_opt : -1;
    int restore = (restorable) ? ++n_opt : -1;
    int ebackup = (ebackupable) ? ++n_opt : -1;
    int decrypt = (decryptable) ? ++n_opt : -1;
    int encrypt = (encryptable) ? ++n_opt : -1;
    int cia_build = (cia_buildable) ? ++n_opt : -1;
    int cia_build_legit = (cia_buildable_legit) ? ++n_opt : -1;
    int cxi_dump = (cxi_dumpable) ? ++n_opt : -1;
    int tik_build_enc = (tik_buildable) ? ++n_opt : -1;
    int tik_build_dec = (tik_buildable) ? ++n_opt : -1;
    int key_build = (key_buildable) ? ++n_opt : -1;
    int verify = (verificable) ? ++n_opt : -1;
    int ctrtransfer = (transferable) ? ++n_opt : -1;
    int hsinject = (hsinjectable) ? ++n_opt : -1;
    int rename = (renamable) ? ++n_opt : -1;
    int xorpad = (xorpadable) ? ++n_opt : -1;
    int xorpad_inplace = (xorpadable) ? ++n_opt : -1;
    int launch = (launchable) ? ++n_opt : -1;
    int boot = (bootable) ? ++n_opt : -1;
    int script = (scriptable) ? ++n_opt : -1;
    if (mount > 0) optionstr[mount-1] = "Mount image to drive";
    if (restore > 0) optionstr[restore-1] = "Restore SysNAND (safe)";
    if (ebackup > 0) optionstr[ebackup-1] = "Update embedded backup";
    if (show_info > 0) optionstr[show_info-1] = "Show title info";
    if (decrypt > 0) optionstr[decrypt-1] = (cryptable_inplace) ? "Decrypt file (...)" : "Decrypt file (" OUTPUT_PATH ")";
    if (encrypt > 0) optionstr[encrypt-1] = (cryptable_inplace) ? "Encrypt file (...)" : "Encrypt file (" OUTPUT_PATH ")";
    if (cia_build > 0) optionstr[cia_build-1] = (cia_build_legit < 0) ? "Build CIA from file" : "Build CIA (standard)";
    if (cia_build_legit > 0) optionstr[cia_build_legit-1] = "Build CIA (legit)";
    if (cxi_dump > 0) optionstr[cxi_dump-1] = "Dump CXI/NDS file";
    if (tik_build_enc > 0) optionstr[tik_build_enc-1] = "Build " TIKDB_NAME_ENC;
    if (tik_build_dec > 0) optionstr[tik_build_dec-1] = "Build " TIKDB_NAME_DEC;
    if (key_build > 0) optionstr[key_build-1] = "Build " KEYDB_NAME;
    if (verify > 0) optionstr[verify-1] = "Verify file";
    if (ctrtransfer > 0) optionstr[ctrtransfer-1] = "Transfer image to CTRNAND";
    if (hsinject > 0) optionstr[hsinject-1] = "Inject to H&S";
    if (rename > 0) optionstr[rename-1] = "Rename file";
    if (xorpad > 0) optionstr[xorpad-1] = "Build XORpads (SD output)";
    if (xorpad_inplace > 0) optionstr[xorpad_inplace-1] = "Build XORpads (inplace)";
    if (launch > 0) optionstr[launch-1] = "Launch as ARM9 payload";
    if (boot > 0) optionstr[boot-1] = "Boot FIRM";
    if (script > 0) optionstr[script-1] = "Execute GM9 script";
    
    // auto select when there is only one option
    user_select = (n_opt <= 1) ? n_opt : (int) ShowSelectPrompt(n_opt, optionstr, (n_marked > 1) ?
        "%s\n%(%lu files selected)" : "%s", pathstr, n_marked);
    if (user_select == mount) { // -> mount file as image
        if (clipboard->n_entries && (DriveType(clipboard->entry[0].path) & DRV_IMAGE))
            clipboard->n_entries = 0; // remove last mounted image clipboard entries
        InitImgFS(curr_entry->path);
        if (!(DriveType("7:")||DriveType("G:")||DriveType("K:")||DriveType("T:"))) {
            ShowPrompt(false, "Mounting image: failed");
            InitImgFS(NULL);
        } else {
            *cursor = 0;
            *current_path = '\0';
            GetDirContents(current_dir, current_path);
            for (u32 i = 0; i < current_dir->n_entries; i++) {
                if (strspn(current_dir->entry[i].path, "7GKTI") == 0)
                    continue;
                strncpy(current_path, current_dir->entry[i].path, 256);
                GetDirContents(current_dir, current_path);
                *cursor = 1;
                *scroll = 0;
                break;
            }
        }
        return 0;
    } else if (user_select == decrypt) { // -> decrypt game file
        if (cryptable_inplace) {
            optionstr[0] = "Decrypt to " OUTPUT_PATH;
            optionstr[1] = "Decrypt inplace";
            user_select = (int) ShowSelectPrompt(2, optionstr, (n_marked > 1) ?
                "%s\n%(%lu files selected)" : "%s", pathstr, n_marked);
        } else user_select = 1;
        bool inplace = (user_select == 2);
        if (!user_select) { // do nothing when no choice is made
        } else if ((n_marked > 1) && ShowPrompt(true, "Try to decrypt all %lu selected files?", n_marked)) {
            u32 n_success = 0;
            u32 n_unencrypted = 0;
            u32 n_other = 0;
            ShowString("Trying to decrypt %lu files...", n_marked);
            for (u32 i = 0; i < current_dir->n_entries; i++) {
                const char* path = current_dir->entry[i].path;
                if (!current_dir->entry[i].marked) 
                    continue;
                if (!(IdentifyFileType(path) & filetype & TYPE_BASE)) {
                    n_other++;
                    continue;
                }
                if (!(filetype & BIN_KEYDB) && (CheckEncryptedGameFile(path) != 0)) {
                    n_unencrypted++;
                    continue;
                }
                current_dir->entry[i].marked = false;
                if (!(filetype & BIN_KEYDB) && (CryptGameFile(path, inplace, false) == 0)) n_success++;
                else if ((filetype & BIN_KEYDB) && (CryptAesKeyDb(path, inplace, false) == 0)) n_success++;
                else { // on failure: set cursor on failed title, break;
                    TruncateString(pathstr, path, 32, 8);
                    ShowPrompt(false, "%s\nDecryption failed", pathstr);
                    *cursor = i;
                    break;
                }
            }
            if (n_other || n_unencrypted) {
                ShowPrompt(false, "%lu/%lu files decrypted ok\n%lu/%lu not encrypted\n%lu/%lu not of same type",
                    n_success, n_marked, n_unencrypted, n_marked, n_other, n_marked);
            } else ShowPrompt(false, "%lu/%lu files decrypted ok", n_success, n_marked);
            if (!inplace && n_success) ShowPrompt(false, "%lu files written to %s", n_success, OUTPUT_PATH);
        } else {
            if (!(filetype & BIN_KEYDB) && (CheckEncryptedGameFile(curr_entry->path) != 0)) {
                ShowPrompt(false, "%s\nFile is not encrypted", pathstr);
            } else {
                u32 ret = (filetype & BIN_KEYDB) ? CryptAesKeyDb(curr_entry->path, inplace, false) :
                    CryptGameFile(curr_entry->path, inplace, false);
                if (inplace || (ret != 0)) ShowPrompt(false, "%s\nDecryption %s", pathstr, (ret == 0) ? "success" : "failed");
                else ShowPrompt(false, "%s\nDecrypted to %s", pathstr, OUTPUT_PATH);
            }
        }
        return 0;
    } else if (user_select == encrypt) { // -> encrypt game file
        if (cryptable_inplace) {
            optionstr[0] = "Encrypt to " OUTPUT_PATH;
            optionstr[1] = "Encrypt inplace";
            user_select = (int) ShowSelectPrompt(2, optionstr,  (n_marked > 1) ?
                "%s\n%(%lu files selected)" : "%s", pathstr, n_marked);
        } else user_select = 1;
        bool inplace = (user_select == 2);
        if (!user_select) { // do nothing when no choice is made
        } else if ((n_marked > 1) && ShowPrompt(true, "Try to encrypt all %lu selected files?", n_marked)) {
            u32 n_success = 0;
            u32 n_other = 0;
            ShowString("Trying to encrypt %lu files...", n_marked);
            for (u32 i = 0; i < current_dir->n_entries; i++) {
                const char* path = current_dir->entry[i].path;
                if (!current_dir->entry[i].marked) 
                    continue;
                if (!(IdentifyFileType(path) & filetype & TYPE_BASE)) {
                    n_other++;
                    continue;
                }
                current_dir->entry[i].marked = false;
                if (!(filetype & BIN_KEYDB) && (CryptGameFile(path, inplace, true) == 0)) n_success++;
                else if ((filetype & BIN_KEYDB) && (CryptAesKeyDb(path, inplace, true) == 0)) n_success++;
                else { // on failure: set cursor on failed title, break;
                    TruncateString(pathstr, path, 32, 8);
                    ShowPrompt(false, "%s\nEncryption failed", pathstr);
                    *cursor = i;
                    break;
                }
            }
            if (n_other) {
                ShowPrompt(false, "%lu/%lu files encrypted ok\n%lu/%lu not of same type",
                    n_success, n_marked, n_other, n_marked);
            } else ShowPrompt(false, "%lu/%lu files encrypted ok", n_success, n_marked);
            if (!inplace && n_success) ShowPrompt(false, "%lu files written to %s", n_success, OUTPUT_PATH);
        } else {
            u32 ret = (filetype & BIN_KEYDB) ? CryptAesKeyDb(curr_entry->path, inplace, true) :
                CryptGameFile(curr_entry->path, inplace, true);
            if (inplace || (ret != 0)) ShowPrompt(false, "%s\nEncryption %s", pathstr, (ret == 0) ? "success" : "failed");
            else ShowPrompt(false, "%s\nEncrypted to %s", pathstr, OUTPUT_PATH);
        }
        return 0;
    } else if ((user_select == cia_build) || (user_select == cia_build_legit) || (user_select == cxi_dump)) { // -> build CIA / dump CXI
        char* type = (user_select == cxi_dump) ? "CXI" : "CIA";
        bool force_legit = (user_select == cia_build_legit);
        if ((n_marked > 1) && ShowPrompt(true, "Try to process all %lu selected files?", n_marked)) {
            u32 n_success = 0;
            u32 n_other = 0; 
            for (u32 i = 0; i < current_dir->n_entries; i++) {
                const char* path = current_dir->entry[i].path;
                if (!current_dir->entry[i].marked) 
                    continue;
                if (!(IdentifyFileType(path) & filetype & TYPE_BASE)) {
                    n_other++;
                    continue;
                }
                current_dir->entry[i].marked = false;
                if (((user_select != cxi_dump) && (BuildCiaFromGameFile(path, force_legit) == 0)) ||
                    ((user_select == cxi_dump) && (DumpCxiSrlFromTmdFile(path) == 0))) n_success++;
                else { // on failure: set *cursor on failed title, break;
                    TruncateString(pathstr, path, 32, 8);
                    ShowPrompt(false, "%s\nBuild %s failed", pathstr, type);
                    *cursor = i;
                    break;
                }
            }
            if (n_other) ShowPrompt(false, "%lu/%lu %ss built ok\n%lu/%lu not of same type",
                n_success, n_marked, type, n_other, n_marked);
            else ShowPrompt(false, "%lu/%lu %ss built ok", n_success, n_marked, type);
            if (n_success) ShowPrompt(false, "%lu files written to %s", n_success, OUTPUT_PATH);
            if (n_success && in_output_path) GetDirContents(current_dir, current_path);
        } else {
            if (((user_select != cxi_dump) && (BuildCiaFromGameFile(curr_entry->path, force_legit) == 0)) ||
                ((user_select == cxi_dump) && (DumpCxiSrlFromTmdFile(curr_entry->path) == 0))) {
                ShowPrompt(false, "%s\n%s built to %s", pathstr, type, OUTPUT_PATH);
                if (in_output_path) GetDirContents(current_dir, current_path);
            } else ShowPrompt(false, "%s\n%s build failed", pathstr, type);
        }
        return 0;
    } else if (user_select == verify) { // -> verify game / nand file
        if ((n_marked > 1) && ShowPrompt(true, "Try to verify all %lu selected files?", n_marked)) {
            u32 n_success = 0;
            u32 n_other = 0; 
            u32 n_processed = 0;
            for (u32 i = 0; i < current_dir->n_entries; i++) {
                const char* path = current_dir->entry[i].path;
                if (!current_dir->entry[i].marked) 
                    continue;
                if (!(filetype & (GAME_CIA|GAME_TMD)) &&
                    !ShowProgress(n_processed++, n_marked, path)) break;
                if (!(IdentifyFileType(path) & filetype & TYPE_BASE)) {
                    n_other++;
                    continue;
                }
                current_dir->entry[i].marked = false;
                if (filetype & IMG_NAND) {
                    if (ValidateNandDump(path) == 0) n_success++;
                } else if (VerifyGameFile(path) == 0) n_success++;
                else { // on failure: set *cursor on failed title, break;
                    TruncateString(pathstr, path, 32, 8);
                    ShowPrompt(false, "%s\nVerification failed", pathstr);
                    *cursor = i;
                    break;
                }
            }
            if (n_other) ShowPrompt(false, "%lu/%lu files verified ok\n%lu/%lu not of same type",
                n_success, n_marked, n_other, n_marked);
            else ShowPrompt(false, "%lu/%lu files verified ok", n_success, n_marked); 
        } else {
            ShowString("%s\nVerifying file, please wait...", pathstr);
            if (filetype & IMG_NAND) {
                ShowPrompt(false, "%s\nNAND validation %s", pathstr,
                    (ValidateNandDump(curr_entry->path) == 0) ? "success" : "failed");
            } else ShowPrompt(false, "%s\nVerification %s", pathstr,
                (VerifyGameFile(curr_entry->path) == 0) ? "success" : "failed");
        }
        return 0;
    } else if ((user_select == tik_build_enc) || (user_select == tik_build_dec)) { // -> (Re)Build titlekey database
        bool dec = (user_select == tik_build_dec);
        const char* path_out = (dec) ? OUTPUT_PATH "/" TIKDB_NAME_DEC : OUTPUT_PATH "/" TIKDB_NAME_ENC;
        if (BuildTitleKeyInfo(NULL, dec, false) != 0) return 1; // init database
        ShowString("Building %s...", (dec) ? TIKDB_NAME_DEC : TIKDB_NAME_ENC);
        if (n_marked > 1) {
            u32 n_success = 0;
            u32 n_other = 0;
            for (u32 i = 0; i < current_dir->n_entries; i++) {
                const char* path = current_dir->entry[i].path;
                if (!current_dir->entry[i].marked)
                    continue;
                if (!FTYPE_TIKBUILD(IdentifyFileType(path))) {
                    n_other++;
                    continue;
                }
                current_dir->entry[i].marked = false;
                if (BuildTitleKeyInfo(path, dec, false) == 0) n_success++; // ignore failures for now
            }
            if (BuildTitleKeyInfo(NULL, dec, true) == 0) {
                if (n_other) ShowPrompt(false, "%s\n%lu/%lu files processed\n%lu/%lu files ignored",
                    path_out, n_success, n_marked, n_other, n_marked);
                else ShowPrompt(false, "%s\n%lu/%lu files processed", path_out, n_success, n_marked); 
            } else ShowPrompt(false, "%s\nBuild database failed.", path_out);
        } else ShowPrompt(false, "%s\nBuild database %s.", path_out, 
            (BuildTitleKeyInfo(curr_entry->path, dec, true) == 0) ? "success" : "failed");
        return 0;
    } else if (user_select == key_build) { // -> (Re)Build AES key database
        const char* path_out = OUTPUT_PATH "/" KEYDB_NAME;
        if (BuildKeyDb(NULL, false) != 0) return 1; // init database
        ShowString("Building %s...", KEYDB_NAME);
        if (n_marked > 1) {
            u32 n_success = 0;
            u32 n_other = 0;
            for (u32 i = 0; i < current_dir->n_entries; i++) {
                const char* path = current_dir->entry[i].path;
                if (!current_dir->entry[i].marked)
                    continue;
                if (!FTYPE_KEYBUILD(IdentifyFileType(path))) {
                    n_other++;
                    continue;
                }
                current_dir->entry[i].marked = false;
                if (BuildKeyDb(path, false) == 0) n_success++; // ignore failures for now
            }
            if (BuildKeyDb(NULL, true) == 0) {
                if (n_other) ShowPrompt(false, "%s\n%lu/%lu files processed\n%lu/%lu files ignored",
                    path_out, n_success, n_marked, n_other, n_marked);
                else ShowPrompt(false, "%s\n%lu/%lu files processed", path_out, n_success, n_marked); 
            } else ShowPrompt(false, "%s\nBuild database failed.", path_out);
        } else ShowPrompt(false, "%s\nBuild database %s.", path_out, 
            (BuildKeyDb(curr_entry->path, true) == 0) ? "success" : "failed");
        return 0;
    } else if (user_select == rename) { // -> Game file renamer
        if ((n_marked > 1) && ShowPrompt(true, "Try to rename all %lu selected files?", n_marked)) {
            u32 n_success = 0;
            ShowProgress(0, 0, "");
            for (u32 i = 0; i < current_dir->n_entries; i++) {
                DirEntry* entry = &(current_dir->entry[i]);
                if (!current_dir->entry[i].marked) continue;
                ShowProgress(i+1, current_dir->n_entries, entry->name);
                if (!GoodRenamer(entry, false)) continue;
                n_success++;
                current_dir->entry[i].marked = false;
            }
            ShowPrompt(false, "%lu/%lu renamed ok", n_success, n_marked);
        } else if (!GoodRenamer(&(current_dir->entry[*cursor]), true)) {
            ShowPrompt(false, "%s\nCould not rename\n(Maybe try decrypt?)", pathstr);
        }
        return 0;
    } else if (user_select == show_info) { // -> Show title info
        if (ShowGameFileTitleInfo(curr_entry->path) != 0)
            ShowPrompt(false, "Title info: not found");
        return 0;
    } else if (user_select == hsinject) { // -> Inject to Health & Safety
        char* destdrv[2] = { NULL };
        n_opt = 0;
        if (DriveType("1:")) {
            optionstr[n_opt] = "SysNAND H&S inject";
            destdrv[n_opt++] = "1:";
        }
        if (DriveType("4:")) {
            optionstr[n_opt] = "EmuNAND H&S inject";
            destdrv[n_opt++] = "4:";
        }
        user_select = (n_opt > 1) ? (int) ShowSelectPrompt(n_opt, optionstr, pathstr) : n_opt;
        if (user_select) {
            ShowPrompt(false, "%s\nH&S inject %s", pathstr,
                (InjectHealthAndSafety(curr_entry->path, destdrv[user_select-1]) == 0) ? "success" : "failed");
        }
        return 0;
    } else if (user_select == ctrtransfer) { // -> transfer CTRNAND image to SysNAND
        char* destdrv[2] = { NULL };
        n_opt = 0;
        if (DriveType("1:")) {
            optionstr[n_opt] = "Transfer to SysNAND";
            destdrv[n_opt++] = "1:";
        }
        if (DriveType("4:")) {
            optionstr[n_opt] = "Transfer to EmuNAND";
            destdrv[n_opt++] = "4:";
        }
        user_select = (n_opt > 1) ? (int) ShowSelectPrompt(n_opt, optionstr, pathstr) : n_opt;
        if (user_select) {
            ShowPrompt(false, "%s\nCTRNAND transfer %s", pathstr,
                (TransferCtrNandImage(curr_entry->path, destdrv[user_select-1]) == 0) ? "success" : "failed");
        }
        return 0;
    } else if (user_select == restore) { // -> restore SysNAND (A9LH preserving)
        ShowPrompt(false, "%s\nNAND restore %s", pathstr,
            (SafeRestoreNandDump(curr_entry->path) == 0) ? "success" : "failed");
        return 0;
    } else if ((user_select == xorpad) || (user_select == xorpad_inplace)) {
        bool inplace = (user_select == xorpad_inplace);
        bool success = (BuildNcchInfoXorpads((inplace) ? current_path : OUTPUT_PATH, curr_entry->path) == 0);
        ShowPrompt(false, "%s\nNCCHinfo padgen %s%s", pathstr,
            (success) ? "success" : "failed", 
            (!success || inplace) ? "" : "\nOutput dir: " OUTPUT_PATH);
        GetDirContents(current_dir, current_path);
        for (; *cursor < current_dir->n_entries; (*cursor)++) {
            curr_entry = &(current_dir->entry[*cursor]);
            if (strncasecmp(curr_entry->name, NCCHINFO_NAME, 32) == 0) break;
        }
        if (*cursor >= current_dir->n_entries) {
            *scroll = 0;
            *cursor = 1;
        }
        return 0;
    } else if (user_select == ebackup) {
        ShowString("%s\nUpdating embedded backup...", pathstr);
        bool required = (CheckEmbeddedBackup(curr_entry->path) != 0);
        bool success = (required && (EmbedEssentialBackup(curr_entry->path) == 0));
        ShowPrompt(false, "%s\nBackup update: %s", pathstr, (!required) ? "not required" :
            (success) ? "completed" : "failed!");
        GetDirContents(current_dir, current_path);
        return 0;
    } else if ((user_select == launch)) {
        size_t payload_size = FileGetSize(curr_entry->path);
        if (ShowUnlockSequence(3, "%s (%dkB)\nLaunch as arm9 payload?", pathstr, payload_size / 1024)) {
            if (FileGetData(curr_entry->path, TEMP_BUFFER, payload_size, 0) == payload_size) {
                Chainload(TEMP_BUFFER, payload_size);
                while(1);
            } // failed load is basically impossible here
        }
        return 0;
    } else if ((user_select == boot)) {
        size_t firm_size = FileGetSize(curr_entry->path);
        if (firm_size > TEMP_BUFFER_SIZE) {
            ShowPrompt(false, "FIRM too big, can't launch"); // unlikely
        } else if (ShowUnlockSequence(3, "%s (%dkB)\nBoot FIRM via chainloader?", pathstr, firm_size / 1024)) {
            if ((FileGetData(curr_entry->path, TEMP_BUFFER, firm_size, 0) == firm_size) &&
                (ValidateFirm(TEMP_BUFFER, firm_size) != 0)) {
                BootFirm((FirmHeader*)(void*)TEMP_BUFFER, curr_entry->path);
                while(1);
            } else ShowPrompt(false, "Not a vaild FIRM, can't launch");
        }
        return 0;
    } else if ((user_select == script)) {
        static bool show_disclaimer = true;
        if (show_disclaimer) ShowPrompt(false, "Warning: Do not run scripts\nfrom untrusted sources.");
        show_disclaimer = false;
        if (ShowPrompt(true, "%s\nExecute script?", pathstr))
            ShowPrompt(false, "%s\nScript execute %s", pathstr, ExecuteGM9Script(curr_entry->path) ? "success" : "failure");
        GetDirContents(current_dir, current_path);
        return 0;
    }
    
    return FileHandlerMenu(current_path, cursor, scroll, current_dir, clipboard);
}

u32 HomeMoreMenu(char* current_path, DirStruct* current_dir, DirStruct* clipboard) {
    NandPartitionInfo np_info;
    if (GetNandPartitionInfo(&np_info, NP_TYPE_BONUS, NP_SUBTYPE_CTR, 0, NAND_SYSNAND) != 0) np_info.count = 0;
    
    const char* optionstr[8];
    const char* promptstr = "HOME more... menu.\nSelect action:";
    u32 n_opt = 0;
    int sdformat = ++n_opt;
    int bonus = (np_info.count > 0x2000) ? (int) ++n_opt : -1; // 4MB minsize
    int multi = (CheckMultiEmuNand()) ? (int) ++n_opt : -1;
    int bsupport = ++n_opt;
    int hsrestore = ((CheckHealthAndSafetyInject("1:") == 0) || (CheckHealthAndSafetyInject("4:") == 0)) ? (int) ++n_opt : -1;
    int scripts = (PathExist(SCRIPT_PATH)) ? (int) ++n_opt : -1;
    
    if (sdformat > 0) optionstr[sdformat - 1] = "SD format menu";
    if (bonus > 0) optionstr[bonus - 1] = "Bonus drive setup";
    if (multi > 0) optionstr[multi - 1] = "Switch EmuNAND";
    if (bsupport > 0) optionstr[bsupport - 1] = "Build support files";
    if (hsrestore > 0) optionstr[hsrestore - 1] = "Restore H&S";
    if (scripts > 0) optionstr[scripts - 1] = "Scripts...";
    
    int user_select = ShowSelectPrompt(n_opt, optionstr, promptstr);
    if (user_select == sdformat) { // format SD card
        bool sd_state = CheckSDMountState();
        if (clipboard->n_entries && (DriveType(clipboard->entry[0].path) & (DRV_SDCARD|DRV_ALIAS|DRV_EMUNAND|DRV_IMAGE)))
            clipboard->n_entries = 0; // remove SD clipboard entries
        DeinitExtFS();
        DeinitSDCardFS();
        if ((SdFormatMenu() == 0) || sd_state) {;
            while (!InitSDCardFS() &&
                ShowPrompt(true, "Initializing SD card failed! Retry?"));
        }
        ClearScreenF(true, true, COLOR_STD_BG);
        AutoEmuNandBase(true);
        InitExtFS();
        GetDirContents(current_dir, current_path);
        return 0;
    } else if (user_select == bonus) { // setup bonus drive
        if (clipboard->n_entries && (DriveType(clipboard->entry[0].path) & (DRV_BONUS|DRV_IMAGE)))
            clipboard->n_entries = 0; // remove bonus drive clipboard entries
        if (!SetupBonusDrive()) ShowPrompt(false, "Setup failed!");
        ClearScreenF(true, true, COLOR_STD_BG);
        GetDirContents(current_dir, current_path);
        return 0;
    } else if (user_select == multi) { // switch EmuNAND offset
        while (ShowPrompt(true, "Current EmuNAND offset is %06X.\nSwitch to next offset?", GetEmuNandBase())) {
            if (clipboard->n_entries && (DriveType(clipboard->entry[0].path) & DRV_EMUNAND))
                clipboard->n_entries = 0; // remove EmuNAND clipboard entries
            DismountDriveType(DRV_EMUNAND);
            AutoEmuNandBase(false);
            InitExtFS();
        }
        GetDirContents(current_dir, current_path);
        return 0;
    } else if (user_select == bsupport) { // build support files
        bool tik_enc_sys = false;
        bool tik_enc_emu = false;
        if (BuildTitleKeyInfo(NULL, false, false) == 0) {
            ShowString("Building " TIKDB_NAME_ENC "...");
            tik_enc_sys = (BuildTitleKeyInfo("1:/dbs/ticket.db", false, false) == 0);
            tik_enc_emu = (BuildTitleKeyInfo("4:/dbs/ticket.db", false, false) == 0);
            if (BuildTitleKeyInfo(NULL, false, true) != 0)
                tik_enc_sys = tik_enc_emu = false;
        }
        bool tik_dec_sys = false;
        bool tik_dec_emu = false;
        if (BuildTitleKeyInfo(NULL, true, false) == 0) {
            ShowString("Building " TIKDB_NAME_DEC "...");
            tik_dec_sys = (BuildTitleKeyInfo("1:/dbs/ticket.db", true, false) == 0);
            tik_dec_emu = (BuildTitleKeyInfo("4:/dbs/ticket.db", true, false) == 0);
            if (!tik_dec_sys || BuildTitleKeyInfo(NULL, true, true) != 0)
                tik_dec_sys = tik_dec_emu = false;
        }
        bool seed_sys = false;
        bool seed_emu = false;
        if (BuildSeedInfo(NULL, false) == 0) {
            ShowString("Building " SEEDDB_NAME "...");
            seed_sys = (BuildSeedInfo("1:", false) == 0);
            seed_emu = (BuildSeedInfo("4:", false) == 0);
            if (!seed_sys || BuildSeedInfo(NULL, true) != 0)
                seed_sys = seed_emu = false;
        }
        ShowPrompt(false, "Built in " OUTPUT_PATH ":\n \n%18.18-s %s\n%18.18-s %s\n%18.18-s %s",
            TIKDB_NAME_ENC, tik_enc_sys ? tik_enc_emu ? "OK (Sys&Emu)" : "OK (Sys)" : "Failed",
            TIKDB_NAME_DEC, tik_dec_sys ? tik_dec_emu ? "OK (Sys&Emu)" : "OK (Sys)" : "Failed",
            SEEDDB_NAME, seed_sys ? seed_emu ? "OK (Sys&Emu)" : "OK (Sys)" : "Failed");
        GetDirContents(current_dir, current_path);
        return 0;
    } else if (user_select == hsrestore) { // restore Health & Safety
        n_opt = 0;
        int sys = (CheckHealthAndSafetyInject("1:") == 0) ? (int) ++n_opt : -1;
        int emu = (CheckHealthAndSafetyInject("4:") == 0) ? (int) ++n_opt : -1;
        if (sys > 0) optionstr[sys - 1] = "Restore H&S (SysNAND)";
        if (emu > 0) optionstr[emu - 1] = "Restore H&S (EmuNAND)";
        user_select = (n_opt > 1) ? ShowSelectPrompt(n_opt, optionstr, promptstr) : n_opt;
        if (user_select > 0) {
            InjectHealthAndSafety(NULL, (user_select == sys) ? "1:" : "4:");
            GetDirContents(current_dir, current_path);
            return 0;
        }
    } else if (user_select == scripts) { // scripts menu
        char script[256];
        if (FileSelector(script, "HOME scripts... menu.\nSelect action:", SCRIPT_PATH, "*.gm9", true, false)) {
            ExecuteGM9Script(script);
            return 0;
        }
    } else return 1;
    
    return HomeMoreMenu(current_path, current_dir, clipboard);
}

u32 SplashInit() {
    const char* namestr = FLAVOR " Explorer v" VERSION;
    const char* loadstr = "loading...";
    const u32 pos_xb = 10;
    const u32 pos_yb = 10;
    const u32 pos_xu = SCREEN_WIDTH_BOT - 10 - GetDrawStringWidth(loadstr);
    const u32 pos_yu = SCREEN_HEIGHT - 10 - GetDrawStringHeight(loadstr);
    
    ClearScreenF(true, true, COLOR_STD_BG);
    QlzDecompress(TOP_SCREEN, QLZ_SPLASH, 0);
    DrawStringF(BOT_SCREEN, pos_xb, pos_yb, COLOR_STD_FONT, COLOR_STD_BG, "%s\n%*.*s\n%s\n \n%s\n%s\n \n%s\n%s",
        namestr, strnlen(namestr, 64), strnlen(namestr, 64),
        "------------------------------", "https://github.com/d0k3/GodMode9",
        "Releases:", "https://github.com/d0k3/GodMode9/releases/", // this won't fit with a 8px width font
        "Hourlies:", "https://d0k3.secretalgorithm.com/");
    DrawStringF(BOT_SCREEN, pos_xu, pos_yu, COLOR_STD_FONT, COLOR_STD_BG, loadstr);
    
    return 0;
}

u32 GodMode() {
    const u32 quick_stp = (MAIN_SCREEN == TOP_SCREEN) ? 20 : 19;
    u32 exit_mode = GODMODE_EXIT_REBOOT;
    
    // reserve 480kB for DirStruct, 64kB for PaneData, just to be safe
    static DirStruct* current_dir = (DirStruct*) (DIR_BUFFER + 0x00000);
    static DirStruct* clipboard   = (DirStruct*) (DIR_BUFFER + 0x78000);
    static PaneData* panedata     = (PaneData*)  (DIR_BUFFER + 0xF0000);
    PaneData* pane = panedata;
    char current_path[256] = { 0x00 };
    
    int mark_next = -1;
    u32 last_write_perm = GetWritePermissions();
    u32 last_clipboard_size = 0;
    u32 cursor = 0;
    u32 scroll = 0;
    
    ClearScreenF(true, true, COLOR_STD_BG);
    if ((sizeof(DirStruct) > 0x78000) || (N_PANES * sizeof(PaneData) > 0x10000)) {
        ShowPrompt(false, "Out of memory!"); // just to be safe
        return exit_mode;
    }
    
    SplashInit();
    u64 timer = timer_start(); // show splash for at least 1 sec
    
    InitSDCardFS();
    AutoEmuNandBase(true);
    InitNandCrypto();
    InitExtFS();
    
    // this takes long - do it while splash is displayed
    GetFreeSpace("0:");
    
    GetDirContents(current_dir, "");
    clipboard->n_entries = 0;
    memset(panedata, 0x00, 0x10000);
    
    // I2C init
    // I2C_init();
    
    // check for embedded essential backup
    if (IS_SIGHAX && !PathExist("S:/essential.exefs") && CheckGenuineNandNcsd() &&
        ShowPrompt(true, "Essential files backup not found.\nCreate one now?")) {
        if (EmbedEssentialBackup("S:/nand.bin") == 0) {
            u32 flags = BUILD_PATH | SKIP_ALL;
            PathCopy(OUTPUT_PATH, "S:/essential.exefs", &flags);
            ShowPrompt(false, "Backup embedded in SysNAND\nand written to " OUTPUT_PATH ".");
        }
    }
    
    while(timer_sec( timer ) < 1); // show splash for at least 1 sec
    ClearScreenF(true, true, COLOR_STD_BG); // clear splash
    
    while (true) { // this is the main loop
        int curr_drvtype = DriveType(current_path);
        
        // basic sanity checking
        if (!current_dir->n_entries) { // current dir is empty -> revert to root
            ShowPrompt(false, "Invalid directory object");
            *current_path = '\0';
            DeinitExtFS(); // deinit and...
            InitExtFS(); // reinitialize extended file system
            GetDirContents(current_dir, current_path);
            cursor = 0;
            if (!current_dir->n_entries) { // should not happen, if it does fail gracefully
                ShowPrompt(false, "Invalid root directory");
                return exit_mode;
            }
        }
        if (cursor >= current_dir->n_entries) // cursor beyond allowed range
            cursor = current_dir->n_entries - 1;
        DirEntry* curr_entry = &(current_dir->entry[cursor]);
        if ((mark_next >= 0) && (curr_entry->type != T_DOTDOT)) {
            curr_entry->marked = mark_next;
            mark_next = -2;
        }
        DrawDirContents(current_dir, cursor, &scroll);
        DrawUserInterface(current_path, curr_entry, clipboard, N_PANES ? pane - panedata + 1 : 0);
        
        // check write permissions
        if (~last_write_perm & GetWritePermissions()) {
            if (ShowPrompt(true, "Write permissions were changed.\nRelock them?")) SetWritePermissions(last_write_perm, false);
            last_write_perm = GetWritePermissions();
            continue;
        }
        
        // handle user input
        u32 pad_state = InputWait();
        bool switched = (pad_state & BUTTON_R1);
        
        // basic navigation commands
        if ((pad_state & BUTTON_A) && (curr_entry->type != T_FILE) && (curr_entry->type != T_DOTDOT)) { // for dirs
            if (switched && !(DriveType(curr_entry->path) & DRV_SEARCH)) { // search directory
                const char* optionstr[4] = { NULL };
                int n_opt = 0;
                int srch_t = (strncmp(curr_entry->path + 1, ":/title", 7) == 0) ? ++n_opt : -1;
                int srch_f = ++n_opt;
                int dirnfo = ++n_opt;
                int stdcpy = (strncmp(current_path, OUTPUT_PATH, 256) != 0) ? ++n_opt : -1;
                if (srch_t > 0) optionstr[srch_t-1] = "Search for titles";
                if (srch_f > 0) optionstr[srch_f-1] = "Search for files...";
                if (dirnfo > 0) optionstr[dirnfo-1] = "Directory info";
                if (stdcpy > 0) optionstr[stdcpy-1] = "Copy to " OUTPUT_PATH;
                char namestr[32+1];
                TruncateString(namestr, (*current_path) ? curr_entry->path : curr_entry->name, 32, 8);
                int user_select = ShowSelectPrompt(n_opt, optionstr, "%s", namestr);
                if ((user_select == srch_f) || (user_select == srch_t)) {
                    char searchstr[256];
                    snprintf(searchstr, 256, (user_select == srch_t) ? "*.tmd" : "*");
                    TruncateString(namestr, curr_entry->name, 20, 8);
                    if ((user_select == srch_t) || ShowStringPrompt(searchstr, 256, "Search %s?\nEnter search below.", namestr)) {
                        SetFSSearch(searchstr, curr_entry->path, (user_select == srch_t));
                        snprintf(current_path, 256, "Z:");
                        GetDirContents(current_dir, current_path);
                        if (current_dir->n_entries) ShowPrompt(false, "Found %lu results.", current_dir->n_entries - 1);
                        cursor = 1;
                        scroll = 0;
                    }
                } else if (user_select == dirnfo) {
                    u64 tsize = 0;
                    u32 tdirs = 0;
                    u32 tfiles = 0;
                    if (DirInfo(curr_entry->path, &tsize, &tdirs, &tfiles)) {
                        char bytestr[32];
                        FormatBytes(bytestr, tsize);
                        ShowPrompt(false, "%s\n%lu files & %lu subdirs\n%s total", namestr, tfiles, tdirs, bytestr);
                    } else ShowPrompt(false, "Analyze dir: failed!");
                } else if (user_select == stdcpy) {
                    StandardCopy(&cursor, current_dir);
                }
            } else { // one level up
                u32 user_select = 1;
                if (curr_drvtype & DRV_SEARCH) { // special menu for search drive
                    const char* optionstr[2] = { "Open this folder", "Open containing folder" };
                    char pathstr[32 + 1];
                    TruncateString(pathstr, curr_entry->path, 32, 8);
                    user_select = ShowSelectPrompt(2, optionstr, pathstr);
                }
                if (user_select) {
                    strncpy(current_path, curr_entry->path, 256);
                    if (user_select == 2) {
                        char* last_slash = strrchr(current_path, '/');
                        if (last_slash) *last_slash = '\0'; 
                    } 
                    GetDirContents(current_dir, current_path);
                    if (*current_path && (current_dir->n_entries > 1)) {
                        cursor = 1;
                        scroll = 0;
                    } else cursor = 0;
                }
            }
        } else if ((pad_state & BUTTON_A) && (curr_entry->type == T_FILE)) { // process a file
            FileHandlerMenu(current_path, &cursor, &scroll, current_dir, clipboard); // processed externally
        } else if (*current_path && ((pad_state & BUTTON_B) || // one level down
            ((pad_state & BUTTON_A) && (curr_entry->type == T_DOTDOT)))) {
            if (switched) { // use R+B to return to root fast
                *current_path = '\0';
                GetDirContents(current_dir, current_path);
                cursor = scroll = 0;
            } else {
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
            }
        } else if (switched && (pad_state & BUTTON_B)) { // unmount SD card
            DeinitExtFS();
            if (!CheckSDMountState()) {
                while (!InitSDCardFS() &&
                    ShowPrompt(true, "Initialising SD card failed! Retry?"));
            } else {
                DeinitSDCardFS();
                if (clipboard->n_entries && (DriveType(clipboard->entry[0].path) &
                    (DRV_SDCARD|DRV_ALIAS|DRV_EMUNAND|DRV_IMAGE)))
                    clipboard->n_entries = 0; // remove SD clipboard entries
            }
            ClearScreenF(true, true, COLOR_STD_BG);
            AutoEmuNandBase(true);
            InitExtFS();
            GetDirContents(current_dir, current_path);
            if (cursor >= current_dir->n_entries) cursor = 0;
        } else if ((pad_state & BUTTON_DOWN) && (cursor + 1 < current_dir->n_entries))  { // cursor down
            if (pad_state & BUTTON_L1) mark_next = curr_entry->marked;
            cursor++;
        } else if ((pad_state & BUTTON_UP) && cursor) { // cursor up
            if (pad_state & BUTTON_L1) mark_next = curr_entry->marked;
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
        } else if ((pad_state & BUTTON_RIGHT) && !(pad_state & BUTTON_L1)) { // cursor down (quick)
            cursor += quick_stp;
        } else if ((pad_state & BUTTON_LEFT) && !(pad_state & BUTTON_L1)) { // cursor up (quick)
            cursor = (cursor >= quick_stp) ? cursor - quick_stp : 0;
        } else if (pad_state & BUTTON_RIGHT) { // mark all entries
            for (u32 c = 1; c < current_dir->n_entries; c++) current_dir->entry[c].marked = 1;
            mark_next = 1;
        } else if (pad_state & BUTTON_LEFT) { // unmark all entries
            for (u32 c = 1; c < current_dir->n_entries; c++) current_dir->entry[c].marked = 0;
            mark_next = 0;
        } else if (switched && (pad_state & BUTTON_L1)) { // switched L -> screenshot
            CreateScreenshot();
            ClearScreenF(true, true, COLOR_STD_BG);
        } else if (*current_path && (pad_state & BUTTON_L1) && (curr_entry->type != T_DOTDOT)) {
            // unswitched L - mark/unmark single entry
            if (mark_next < -1) mark_next = -1;
            else curr_entry->marked ^= 0x1;
        } else if (pad_state & BUTTON_SELECT) { // clear/restore clipboard
            clipboard->n_entries = (clipboard->n_entries > 0) ? 0 : last_clipboard_size;
        }

        // highly specific commands
        if (!*current_path) { // in the root folder...
            if (switched && (pad_state & BUTTON_X)) { // unmount image
                if (clipboard->n_entries && (DriveType(clipboard->entry[0].path) & DRV_IMAGE))
                    clipboard->n_entries = 0; // remove last mounted image clipboard entries
                InitImgFS(NULL);
                ClearScreenF(false, true, COLOR_STD_BG);
                GetDirContents(current_dir, current_path);
            } else if (switched && (pad_state & BUTTON_Y)) {
                SetWritePermissions(PERM_BASE, false);
                ClearScreenF(false, true, COLOR_STD_BG);
            }
        } else if (!switched) { // standard unswitched command set
            if ((curr_drvtype & DRV_VIRTUAL) && (pad_state & BUTTON_X)) {
                ShowPrompt(false, "Not allowed in virtual path");
            } else if (pad_state & BUTTON_X) { // delete a file 
                u32 n_marked = 0;
                if (curr_entry->marked) {
                    for (u32 c = 0; c < current_dir->n_entries; c++)
                        if (current_dir->entry[c].marked) n_marked++;
                }
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
                        ShowString("Deleting files, please wait...");
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
            } else if ((curr_drvtype & DRV_SEARCH) && (pad_state & BUTTON_Y)) {
                ShowPrompt(false, "Not allowed in search drive");
            } else if ((curr_drvtype & DRV_GAME) && (pad_state & BUTTON_Y)) {
                ShowPrompt(false, "Not allowed in virtual game path");
            } else if ((curr_drvtype & DRV_XORPAD) && (pad_state & BUTTON_Y)) {
                ShowPrompt(false, "Not allowed in XORpad drive");
            } else if ((curr_drvtype & DRV_CART) && (pad_state & BUTTON_Y)) {
                ShowPrompt(false, "Not allowed in gamecart drive");
            }else if (pad_state & BUTTON_Y) { // paste files
                const char* optionstr[2] = { "Copy path(s)", "Move path(s)" };
                char promptstr[64];
                u32 flags = 0;
                u32 user_select;
                if (clipboard->n_entries == 1) {
                    char namestr[20+1];
                    TruncateString(namestr, clipboard->entry[0].name, 20, 12);
                    snprintf(promptstr, 64, "Paste \"%s\" here?", namestr);
                } else snprintf(promptstr, 64, "Paste %lu paths here?", clipboard->n_entries);
                user_select = ((DriveType(clipboard->entry[0].path) & curr_drvtype & DRV_STDFAT)) ?
                    ShowSelectPrompt(2, optionstr, promptstr) : (ShowPrompt(true, promptstr) ? 1 : 0);
                if (user_select) {
                    for (u32 c = 0; c < clipboard->n_entries; c++) {
                        char namestr[36+1];
                        TruncateString(namestr, clipboard->entry[c].name, 36, 12);
                        flags &= ~ASK_ALL;
                        if (c < clipboard->n_entries - 1) flags |= ASK_ALL;
                        if ((user_select == 1) && !PathCopy(current_path, clipboard->entry[c].path, &flags)) {    
                            if (c + 1 < clipboard->n_entries) {
                                if (!ShowPrompt(true, "Failed copying path:\n%s\nProcess remaining?", namestr)) break;
                            } else ShowPrompt(false, "Failed copying path:\n%s", namestr);
                        } else if ((user_select == 2) && !PathMove(current_path, clipboard->entry[c].path, &flags)) {    
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
            if ((curr_drvtype & DRV_VIRTUAL) && (pad_state & (BUTTON_X|BUTTON_Y))) {
                ShowPrompt(false, "Not allowed in virtual path");
            } else if ((curr_drvtype & DRV_ALIAS) && (pad_state & (BUTTON_X))) {
                ShowPrompt(false, "Not allowed in alias path");
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
        } else if (pad_state & BUTTON_POWER) {
            exit_mode = GODMODE_EXIT_POWEROFF;
            break;
        } else if (pad_state & BUTTON_HOME) { // Home menu
            const char* optionstr[] = { "Poweroff system", "Reboot system", "More..." };
            const char* promptstr = "HOME button pressed.\nSelect action:";
            u32 n_opt = 3;
            u32 user_select = 0;
            while (((user_select = ShowSelectPrompt(n_opt, optionstr, promptstr)) == 3) &&
                (HomeMoreMenu(current_path, current_dir, clipboard) == 1)); // more... menu
            if (user_select == 1) { 
                exit_mode = GODMODE_EXIT_POWEROFF;
                break;
            } else if (user_select == 2) { 
                exit_mode = GODMODE_EXIT_REBOOT;
                break;
            }
        } else if (pad_state & (CART_INSERT|CART_EJECT)) {
            if (!InitVCartDrive() && (pad_state & CART_INSERT)) // reinit virtual cart drive
                ShowPrompt(false, "Cart init failed!");
            if (!(*current_path) || (curr_drvtype & DRV_CART))
                GetDirContents(current_dir, current_path); // refresh dir contents
        } else if (pad_state & SD_INSERT) {
            while (!InitSDCardFS() && ShowPrompt(true, "Initialising SD card failed! Retry?"));
            ClearScreenF(true, true, COLOR_STD_BG);
            AutoEmuNandBase(true);
            InitExtFS();
            GetDirContents(current_dir, current_path);
        } else if ((pad_state & SD_EJECT) && CheckSDMountState()) {
            ShowPrompt(false, "!Unexpected SD card removal!\n \nTo prevent data loss, unmount\nbefore ejecting the SD card.");
            DeinitExtFS();
            DeinitSDCardFS();
            InitExtFS();
            if (clipboard->n_entries && (DriveType(clipboard->entry[0].path) &
                (DRV_SDCARD|DRV_ALIAS|DRV_EMUNAND|DRV_IMAGE)))
                clipboard->n_entries = 0; // remove SD clipboard entries
            GetDirContents(current_dir, current_path);
        }
    }
    
    DeinitExtFS();
    DeinitSDCardFS();
    
    return exit_mode;
}
