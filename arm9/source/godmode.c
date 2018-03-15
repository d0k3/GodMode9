#include "godmode.h"
#include "memmap.h"
#include "support.h"
#include "ui.h"
#include "hid.h"
#include "fs.h"
#include "utils.h"
#include "nand.h"
#include "virtual.h"
#include "vcart.h"
#include "game.h"
#include "disadiff.h"
#include "unittype.h"
#include "entrypoints.h"
#include "bootfirm.h"
#include "pcx.h"
#include "timer.h"
#include "rtc.h"
#include "power.h"
#include "vram0.h"
#include "i2c.h"


#define N_PANES 2

#define COLOR_TOP_BAR   (PERM_RED ? COLOR_RED : PERM_ORANGE ? COLOR_ORANGE : PERM_BLUE ? COLOR_BRIGHTBLUE : \
                         PERM_YELLOW ? COLOR_BRIGHTYELLOW : PERM_GREEN ? COLOR_GREEN : COLOR_WHITE)
#define COLOR_ENTRY(e)  (((e)->marked) ? COLOR_MARKED : ((e)->type == T_DIR) ? COLOR_DIR : ((e)->type == T_FILE) ? COLOR_FILE : ((e)->type == T_ROOT) ?  COLOR_ROOT : COLOR_GREY)

#define BOOTPAUSE_KEY   (BUTTON_R1|BUTTON_UP)
#define BOOTMENU_KEY    (BUTTON_R1|BUTTON_LEFT)
#define BOOTFIRM_PATHS  "0:/bootonce.firm", "0:/boot.firm", "1:/boot.firm"
#define BOOTFIRM_TEMPS  0x1 // bits mark paths as temporary

#ifdef SALTMODE // ShadowHand's own bootmenu key override
#undef  BOOTMENU_KEY
#define BOOTMENU_KEY    BUTTON_START
#endif


typedef struct {
    char path[256];
    u32 cursor;
    u32 scroll;
} PaneData;


u32 SplashInit(const char* modestr) {
    u64 splash_size;
    u8* splash = FindVTarFileInfo(VRAM0_SPLASH_PCX, &splash_size);
    u8* bitmap = (u8*) malloc(SCREEN_SIZE_TOP);
    const char* namestr = FLAVOR " " VERSION;
    const char* loadstr = "booting...";
    const u32 pos_xb = 10;
    const u32 pos_yb = 10;
    const u32 pos_xu = SCREEN_WIDTH_BOT - 10 - GetDrawStringWidth(loadstr);
    const u32 pos_yu = SCREEN_HEIGHT - 10 - GetDrawStringHeight(loadstr);
    
    ClearScreenF(true, true, COLOR_STD_BG);
    
    if (splash && bitmap && PCX_Decompress(bitmap, SCREEN_SIZE_TOP, splash, splash_size)) {
        PCXHdr* hdr = (PCXHdr*) (void*) splash;
        DrawBitmap(TOP_SCREEN, -1, -1, PCX_Width(hdr), PCX_Height(hdr), bitmap);
    } else DrawStringF(TOP_SCREEN, 10, 10, COLOR_STD_FONT, COLOR_TRANSPARENT, "(" VRAM0_SPLASH_PCX " not found)");
    if (modestr) DrawStringF(TOP_SCREEN, SCREEN_WIDTH_TOP - 10 - GetDrawStringWidth(modestr),
        SCREEN_HEIGHT - 10 - GetDrawStringHeight(modestr), COLOR_STD_FONT, COLOR_TRANSPARENT, modestr);
    
    DrawStringF(BOT_SCREEN, pos_xb, pos_yb, COLOR_STD_FONT, COLOR_STD_BG, "%s\n%*.*s\n%s\n \n \n%s\n%s\n \n%s\n%s",
        namestr, strnlen(namestr, 64), strnlen(namestr, 64),
        "------------------------------", "https://github.com/d0k3/GodMode9",
        "Releases:", "https://github.com/d0k3/GodMode9/releases/", // this won't fit with a 8px width font
        "Hourlies:", "https://d0k3.secretalgorithm.com/");
    DrawStringF(BOT_SCREEN, pos_xu, pos_yu, COLOR_STD_FONT, COLOR_STD_BG, loadstr);
    DrawStringF(BOT_SCREEN, pos_xb, pos_yu, COLOR_STD_FONT, COLOR_STD_BG, "built: " DBUILTL);
    
    if (bitmap) free(bitmap);
    return 0;
}

#ifndef SCRIPT_RUNNER
static DirStruct* current_dir = NULL;
static DirStruct* clipboard   = NULL;
static PaneData* panedata     = NULL;

void GetTimeString(char* timestr, bool forced_update, bool full_year) {
    static DsTime dstime;
    static u64 timer = (u64) -1; // this ensures we don't check the time too often
    if (forced_update || (timer == (u64) -1) || (timer_sec(timer) > 30)) {
        get_dstime(&dstime);
        timer = timer_start();
    }
    if (timestr) snprintf(timestr, 31, "%s%02lX-%02lX-%02lX %02lX:%02lX", full_year ? "20" : "",
        (u32) dstime.bcd_Y, (u32) dstime.bcd_M, (u32) dstime.bcd_D, (u32) dstime.bcd_h, (u32) dstime.bcd_m);
}

void CheckBattery(u32* battery, bool* is_charging) {
    if (battery) {
        static u32 battery_l = 0;
        static u64 timer_b = (u64) -1; // this ensures we don't check too often
        if ((timer_b == (u64) -1) || (timer_sec(timer_b) >= 120)) {
            battery_l = GetBatteryPercent();
            timer_b = timer_start();
        }
        *battery = battery_l;
    }
    
    if (is_charging) {
        static bool is_charging_l = false;
        static u64 timer_c = (u64) -1;
        if ((timer_c == (u64) -1) || (timer_sec(timer_c) >= 1)) {
            is_charging_l = IsCharging();
            timer_c = timer_start();
        }
        *is_charging = is_charging_l;
    }
}

void GenerateBatteryBitmap(u8* bitmap, u32 width, u32 height, u32 color_bg) {
    const u32 color_outline = COLOR_BLACK;
    const u32 color_inline = COLOR_LIGHTGREY;
    const u32 color_inside = COLOR_LIGHTERGREY;
    
    if ((width < 8) || (height < 6)) return;
    
    u32 battery;
    bool is_charging;
    CheckBattery(&battery, &is_charging);
    
    u32 color_battery = (is_charging) ? COLOR_BATTERY_CHARGING :
        (battery > 70) ? COLOR_BATTERY_FULL : (battery > 30) ? COLOR_BATTERY_MEDIUM : COLOR_BATTERY_LOW;
    u32 nub_size = (height < 12) ? 1 : 2;
    u32 width_inside = width - 4 - nub_size;
    u32 width_battery = (battery >= 100) ? width_inside : ((battery * width_inside) + 50) / 100;
    
    for (u32 y = 0; y < height; y++) {
        const u32 mirror_y = (y >= (height+1) / 2) ? height - 1 - y : y;
        for (u32 x = 0; x < width; x++) {
            const u32 rev_x = width - x - 1;
            u32 color = 0;
            if (mirror_y == 0) color = (rev_x >= nub_size) ? color_outline : color_bg;
            else if (mirror_y == 1) color = ((x == 0) || (rev_x == nub_size)) ? color_outline : (rev_x < nub_size) ? color_bg : color_inline;
            else if (mirror_y == 2) color = ((x == 0) || (rev_x <= nub_size)) ? color_outline : ((x == 1) || (rev_x == (nub_size+1))) ? color_inline : color_inside;
            else color = ((x == 0) || (rev_x == 0)) ? color_outline : ((x == 1) || (rev_x <= (nub_size+1))) ? color_inline : color_inside;
            if ((color == color_inside) && (x < (2 + width_battery))) color = color_battery;
            *(bitmap++) = color >> 16;  // B
            *(bitmap++) = color >> 8;   // G
            *(bitmap++) = color & 0xFF; // R
        }
    }
}

void DrawTopBar(const char* curr_path) {
    const u32 bartxt_start = (FONT_HEIGHT_EXT >= 10) ? 1 : (FONT_HEIGHT_EXT >= 7) ? 2 : 3;
    const u32 bartxt_x = 2;
    const u32 len_path = SCREEN_WIDTH_TOP - 120;
    char tempstr[64];
    
    // top bar - current path
    DrawRectangle(TOP_SCREEN, 0, 0, SCREEN_WIDTH_TOP, 12, COLOR_TOP_BAR);
    if (*curr_path) TruncateString(tempstr, curr_path, len_path / FONT_WIDTH_EXT, 8);
    else snprintf(tempstr, 16, "[root]");
    DrawStringF(TOP_SCREEN, bartxt_x, bartxt_start, COLOR_STD_BG, COLOR_TOP_BAR, tempstr);
    bool show_time = true;
    
    #ifdef SHOW_FREE
    if (*curr_path) { // free & total storage
        const u32 bartxt_rx = SCREEN_WIDTH_TOP - (19*FONT_WIDTH_EXT) - bartxt_x;
        char bytestr0[32];
        char bytestr1[32];
        DrawStringF(TOP_SCREEN, bartxt_rx, bartxt_start, COLOR_STD_BG, COLOR_TOP_BAR, "%19.19s", "LOADING...");
        FormatBytes(bytestr0, GetFreeSpace(curr_path));
        FormatBytes(bytestr1, GetTotalSpace(curr_path));
        snprintf(tempstr, 64, "%s/%s", bytestr0, bytestr1);
        DrawStringF(TOP_SCREEN, bartxt_rx, bartxt_start, COLOR_STD_BG, COLOR_TOP_BAR, "%19.19s", tempstr);
        show_time = false;
    }
    #endif
    
    #ifdef MONITOR_HEAP
    if (true) { // allocated mem
        const u32 bartxt_rx = SCREEN_WIDTH_TOP - (9*FONT_WIDTH_EXT) - bartxt_x;
        char bytestr[32];
        FormatBytes(bytestr, mem_allocated());
        DrawStringF(TOP_SCREEN, bartxt_rx, bartxt_start, COLOR_STD_BG, COLOR_TOP_BAR, "%9.9s", bytestr);
        show_time = false;
    }
    #endif
    
    if (show_time) { // clock & battery
        const u32 battery_width = 16;
        const u32 battery_height = 9;
        const u32 battery_x = SCREEN_WIDTH_TOP - battery_width - bartxt_x;
        const u32 battery_y = (12 - battery_height) / 2;
        const u32 clock_x = battery_x - (15*FONT_WIDTH_EXT);
        
        char timestr[32];
        GetTimeString(timestr, false, false);
        DrawStringF(TOP_SCREEN, clock_x, bartxt_start, COLOR_STD_BG, COLOR_TOP_BAR, "%14.14s", timestr);
        
        u8 bitmap[battery_width * battery_height * BYTES_PER_PIXEL];
        GenerateBatteryBitmap(bitmap, battery_width, battery_height, COLOR_TOP_BAR);
        DrawBitmap(TOP_SCREEN, battery_x, battery_y, battery_width, battery_height, bitmap);
    }
}

void DrawUserInterface(const char* curr_path, DirEntry* curr_entry, u32 curr_pane) {
    const u32 n_cb_show = 8;
    const u32 info_start = (MAIN_SCREEN == TOP_SCREEN) ? 18 : 2; // leave space for the topbar when required
    const u32 instr_x = (SCREEN_WIDTH_MAIN - (34*FONT_WIDTH_EXT)) / 2;
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
            (drvtype & DRV_CART) ? "Gamecart" : (drvtype & DRV_VRAM) ? "VRAM" : (drvtype & DRV_SEARCH) ? "Search" : ""),
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
        FLAVOR " " VERSION, // generic start part
        (*curr_path) ? ((clipboard->n_entries == 0) ? "L - MARK files (use with \x18\x19\x1A\x1B)\nX - DELETE / [+R] RENAME file(s)\nY - COPY files / [+R] CREATE entry\n" :
        "L - MARK files (use with \x18\x19\x1A\x1B)\nX - DELETE / [+R] RENAME file(s)\nY - PASTE files / [+R] CREATE entry\n") :
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
    const u32 stp_y = min(12, FONT_HEIGHT_EXT + 4);
    const u32 start_y = (MAIN_SCREEN == TOP_SCREEN) ? 0 : 12;
    const u32 pos_x = 0;
    const u32 lines = (SCREEN_HEIGHT-(start_y+2)+(stp_y-1)) / stp_y;
    u32 pos_y = start_y + 2;
    
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
    
    const u32 flist_height = (SCREEN_HEIGHT - start_y);
    const u32 bar_width = 2;
    if (contents->n_entries > lines) { // draw position bar at the right
        const u32 bar_height_min = 32;
        u32 bar_height = (lines * flist_height) / contents->n_entries;
        if (bar_height < bar_height_min) bar_height = bar_height_min;
        const u32 bar_pos = ((u64) *scroll * (flist_height - bar_height)) / (contents->n_entries - lines) + start_y;
        
        DrawRectangle(ALT_SCREEN, SCREEN_WIDTH_ALT - bar_width, start_y, bar_width, (bar_pos - start_y), COLOR_STD_BG);
        DrawRectangle(ALT_SCREEN, SCREEN_WIDTH_ALT - bar_width, bar_pos + bar_height, bar_width, SCREEN_HEIGHT - (bar_pos + bar_height), COLOR_STD_BG);
        DrawRectangle(ALT_SCREEN, SCREEN_WIDTH_ALT - bar_width, bar_pos, bar_width, bar_height, COLOR_SIDE_BAR);
    } else DrawRectangle(ALT_SCREEN, SCREEN_WIDTH_ALT - bar_width, start_y, bar_width, flist_height, COLOR_STD_BG);
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
    
    if (!ShowStringPrompt(label + 2, 11 + 1, "Format SD card (%lluMB)?\nEnter label:", sdcard_size_mb))
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

u32 FileGraphicsViewer(const char* path) {
    const u32 max_size = SCREEN_SIZE(ALT_SCREEN);
    u64 filetype = IdentifyFileType(path);
    u8* bitmap = (u8*) malloc(max_size);
    u8* input = (u8*) malloc(max_size);
    u32 w = 0;
    u32 h = 0;
    u32 ret = 1;
    
    if (!bitmap || !input) {
        if (bitmap) free(bitmap);
        if (input) free(input);
        return 1;
    }
    
    u32 input_size = FileGetData(path, input, max_size, 0);
    if (input_size && (input_size < max_size)) {
        if (filetype & GFX_PCX) {
            if (PCX_Decompress(bitmap, max_size, input, input_size)) {
                PCXHdr* hdr = (PCXHdr*) (void*) input;
                w = PCX_Width(hdr);
                h = PCX_Height(hdr);
                ret = 0;
            }
        }
    }
    
    if ((ret == 0) && w && h && (w < SCREEN_WIDTH(ALT_SCREEN)) && (h < SCREEN_HEIGHT)) {
        ClearScreenF(true, true, COLOR_STD_BG);
        DrawBitmap(ALT_SCREEN, -1, -1, w, h, bitmap);
        ShowString("Press <A> to continue");
        while(!(InputWait(0) & (BUTTON_A | BUTTON_B)));
        ClearScreenF(true, true, COLOR_STD_BG);
    } else ret = 1;
    
    free(bitmap);
    free(input);
    return ret;
}

u32 FileHexViewer(const char* path) {
    const u32 max_data = (SCREEN_HEIGHT / FONT_HEIGHT_EXT) * 16 * ((FONT_WIDTH_EXT > 4) ? 1 : 2);
    static u32 mode = 0;
    u8* data = NULL;
    u8* bottom_cpy = (u8*) malloc(SCREEN_SIZE_BOT); // a copy of the bottom screen framebuffer
    u32 fsize = FileGetSize(path);
    
    bool dual_screen = 0;
    int x_off = 0, x_hex = 0, x_ascii = 0;
    u32 vpad = 0, hlpad = 0, hrpad = 0;
    u32 rows = 0, cols = 0;
    u32 total_shown = 0;
    u32 total_data = 0;
    
    u32 last_mode = 0xFF;
    u32 last_offset = (u32) -1;
    u32 offset = 0;
    
    u8  found_data[64 + 1] = { 0 };
    u32 found_offset = (u32) -1;
    u32 found_size = 0;
    
    static const u32 edit_bsize = 0x4000; // should be multiple of 0x200 * 2
    bool edit_mode = false;
    u8* buffer = (u8*) malloc(edit_bsize);
    u8* buffer_cpy = (u8*) malloc(edit_bsize);
    u32 edit_start = 0;
    int cursor = 0;
    
    if (!bottom_cpy || !buffer || !buffer_cpy) {
        if (bottom_cpy) free(bottom_cpy);
        if (buffer) free(buffer);
        if (buffer_cpy) free(buffer_cpy);
        return 1;
    }
    
    static bool show_instr = true;
    static const char* instr = "Hexeditor Controls:\n \n\x18\x19\x1A\x1B(+R) - Scroll\nR+Y - Switch view\nX - Search / goto...\nA - Enter edit mode\nA+\x18\x19\x1A\x1B - Edit value\nB - Exit\n";
    if (show_instr) { // show one time instructions
        ShowPrompt(false, instr);
        show_instr = false;
    }
    
    if (MAIN_SCREEN != TOP_SCREEN) ShowString(instr);
    memcpy(bottom_cpy, BOT_SCREEN, SCREEN_SIZE_BOT);
    
    data = buffer;
    while (true) {
        if (mode != last_mode) {
            if (FONT_WIDTH_EXT <= 5) {
                mode = 0; 
                vpad = 1;
                hlpad = hrpad = 2;
                cols = 16;
                x_off = (SCREEN_WIDTH_TOP - SCREEN_WIDTH_BOT) / 2;
                x_ascii = SCREEN_WIDTH_TOP - x_off - (FONT_WIDTH_EXT * cols);
                x_hex = ((SCREEN_WIDTH_TOP - ((hlpad + (2*FONT_WIDTH_EXT) + hrpad) * cols)) / 2) -
                    (((cols - 8) / 2) * FONT_WIDTH_EXT);
                dual_screen = true;
            } else if (FONT_WIDTH_EXT <= 6) {
                if (mode == 1) {
                    vpad = 0;
                    hlpad = hrpad = 1;
                    cols = 16;
                    x_off = 0;
                    x_ascii = SCREEN_WIDTH_TOP - (FONT_WIDTH_EXT * cols);
                    x_hex = x_off + (8*FONT_WIDTH_EXT) + 16;
                    dual_screen = false;
                } else {
                    mode = 0; 
                    vpad = 0;
                    hlpad = hrpad = 3;
                    cols = 8;
                    x_off = 30 + (SCREEN_WIDTH_TOP - SCREEN_WIDTH_BOT) / 2;
                    x_ascii = SCREEN_WIDTH_TOP - x_off - (FONT_WIDTH_EXT * cols);
                    x_hex = (SCREEN_WIDTH_TOP - ((hlpad + (2*FONT_WIDTH_EXT) + hrpad) * cols)) / 2;
                    dual_screen = true;
                }
            } else switch (mode) { // display mode
                case 1:
                    vpad = hlpad = hrpad = 1;
                    cols = 12;
                    x_off = 0;
                    x_ascii = SCREEN_WIDTH_TOP - (FONT_WIDTH_EXT * cols);
                    x_hex = ((SCREEN_WIDTH_TOP - ((hlpad + (2*FONT_WIDTH_EXT) + hrpad) * cols) -
                        ((cols - 8) * FONT_WIDTH_EXT)) / 2);
                    dual_screen = false;
                    break;
                case 2:
                    vpad = 1;
                    hlpad = 0;
                    hrpad = 1 + 8 - FONT_WIDTH_EXT;
                    cols = 16;
                    x_off = -1;
                    x_ascii = SCREEN_WIDTH_TOP - (FONT_WIDTH_EXT * cols);
                    x_hex = 0;
                    dual_screen = false;
                    break;
                case 3:
                    vpad = hlpad = hrpad = 1;
                    cols = 16;
                    x_off = ((SCREEN_WIDTH_TOP - ((hlpad + (2*FONT_WIDTH_EXT) + hrpad) * cols)
                        - 12 - (8*FONT_WIDTH_EXT)) / 2);
                    x_ascii = -1;
                    x_hex = x_off + (8*FONT_WIDTH_EXT) + 12;
                    dual_screen = false;
                    break;
                default:
                    mode = 0; 
                    vpad = hlpad = hrpad = 2;
                    cols = 8;
                    x_off = (SCREEN_WIDTH_TOP - SCREEN_WIDTH_BOT) / 2;
                    x_ascii = SCREEN_WIDTH_TOP - x_off - (FONT_WIDTH_EXT * cols);
                    x_hex = (SCREEN_WIDTH_TOP - ((hlpad + (2*FONT_WIDTH_EXT) + hrpad) * cols)) / 2;
                    dual_screen = true;
                    break;
            }
            rows = (dual_screen ? 2 : 1) * SCREEN_HEIGHT / (FONT_HEIGHT_EXT + (2*vpad));
            total_shown = rows * cols;
            last_mode = mode;
            ClearScreen(TOP_SCREEN, COLOR_STD_BG);
            if (dual_screen) ClearScreen(BOT_SCREEN, COLOR_STD_BG);
            else memcpy(BOT_SCREEN, bottom_cpy, SCREEN_SIZE_BOT);
        }
        // fix offset (if required)
        if (offset % cols) offset -= (offset % cols); // fix offset (align to cols)
        if (offset + total_shown - cols > fsize) // if offset too big
            offset = (total_shown > fsize) ? 0 : (fsize + cols - total_shown - (fsize % cols));
        // get data, using max data size (if new offset)
        if (offset != last_offset) {
            if (!edit_mode) {
                total_data = FileGetData(path, data, max_data, offset);
            } else { // edit mode - read from memory
                if ((offset < edit_start) || (offset + max_data > edit_start + edit_bsize))
                    offset = last_offset; // we don't expect this to happen
                total_data = (fsize - offset >= max_data) ? max_data : fsize - offset;
                data = buffer + (offset - edit_start);
            }
            last_offset = offset;
        }
        
        // display data on screen
        for (u32 row = 0; row < rows; row++) {
            char ascii[16 + 1] = { 0 };
            u32 y = row * (FONT_HEIGHT_EXT + (2*vpad)) + vpad;
            u32 curr_pos = row * cols;
            u32 cutoff = (curr_pos >= total_data) ? 0 : (total_data >= curr_pos + cols) ? cols : total_data - curr_pos;
            u8* screen = TOP_SCREEN;
            u32 x0 = 0;
            
            // marked offsets handling
            s32 marked0 = 0, marked1 = 0;
            if ((found_size > 0) &&
                (found_offset + found_size > offset + curr_pos) && 
                (found_offset < offset + curr_pos + cols)) {
                marked0 = (s32) found_offset - (offset + curr_pos);
                marked1 = marked0 + found_size;
                if (marked0 < 0) marked0 = 0;
                if (marked1 > cols) marked1 = cols;
            }
            
            // switch to bottom screen
            if (y >= SCREEN_HEIGHT) {
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
                DrawString(screen, ascii, x_ascii - x0, y, COLOR_HVASCII, COLOR_STD_BG, false);
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
        u32 pad_state = InputWait(0);
        if (!edit_mode) { // standard viewer mode
            u32 step_ud = (pad_state & BUTTON_R1) ? (0x1000  - (0x1000  % cols)) : cols;
            u32 step_lr = (pad_state & BUTTON_R1) ? (0x10000 - (0x10000 % cols)) : total_shown;
            if (pad_state & BUTTON_DOWN) offset += step_ud;
            else if (pad_state & BUTTON_RIGHT) offset += step_lr;
            else if (pad_state & BUTTON_UP) offset = (offset > step_ud) ? offset - step_ud : 0;
            else if (pad_state & BUTTON_LEFT) offset = (offset > step_lr) ? offset - step_lr : 0;
            else if ((pad_state & BUTTON_R1) && (pad_state & BUTTON_Y)) mode++;
            else if ((pad_state & BUTTON_A) && total_data) edit_mode = true;
            else if (pad_state & (BUTTON_B|BUTTON_START)) break;
            else if (found_size && (pad_state & BUTTON_R1) && (pad_state & BUTTON_X)) {
                found_offset = FileFindData(path, found_data, found_size, found_offset + 1);
                if (found_offset == (u32) -1) {
                    ShowPrompt(false, "Not found!");
                    found_size = 0;
                } else offset = found_offset;
                if (MAIN_SCREEN == TOP_SCREEN) ClearScreen(TOP_SCREEN, COLOR_STD_BG);
                else if (dual_screen) ClearScreen(BOT_SCREEN, COLOR_STD_BG);
                else memcpy(BOT_SCREEN, bottom_cpy, SCREEN_SIZE_BOT);
            } else if (pad_state & BUTTON_X) {
                const char* optionstr[3] = { "Go to offset", "Search for string", "Search for data" };
                u32 user_select = ShowSelectPrompt(3, optionstr, "Current offset: %08X\nSelect action:", 
                    (unsigned int) offset);
                if (user_select == 1) { // -> goto offset
                    u64 new_offset = ShowHexPrompt(offset, 8, "Current offset: %08X\nEnter new offset below.",
                        (unsigned int) offset);
                    if (new_offset != (u64) -1) offset = new_offset;
                } else if (user_select == 2) {
                    if (!found_size) *found_data = 0;
                    if (ShowStringPrompt((char*) found_data, 64 + 1, "Enter search string below.\n(R+X to repeat search)", (unsigned int) offset)) {
                        found_size = strnlen((char*) found_data, 64);
                        found_offset = FileFindData(path, found_data, found_size, offset);
                        if (found_offset == (u32) -1) {
                            ShowPrompt(false, "Not found!");
                            found_size = 0;
                        } else offset = found_offset;
                    }
                } else if (user_select == 3) {
                    u32 size = found_size;
                    if (ShowDataPrompt(found_data, &size, "Enter search data below.\n(R+X to repeat search)", (unsigned int) offset)) {
                        found_size = size;
                        found_offset = FileFindData(path, found_data, size, offset);
                        if (found_offset == (u32) -1) {
                            ShowPrompt(false, "Not found!");
                            found_size = 0;
                        } else offset = found_offset;
                    }
                }
                if (MAIN_SCREEN == TOP_SCREEN) ClearScreen(TOP_SCREEN, COLOR_STD_BG);
                else if (dual_screen) ClearScreen(BOT_SCREEN, COLOR_STD_BG);
                else memcpy(BOT_SCREEN, bottom_cpy, SCREEN_SIZE_BOT);
            }
            if (edit_mode && CheckWritePermissions(path)) { // setup edit mode
                found_size = 0;
                found_offset = (u32) -1;
                cursor = 0;
                edit_start = ((offset - (offset % 0x200) <= (edit_bsize / 2)) || (fsize < edit_bsize)) ? 0 : 
                    offset - (offset % 0x200) - (edit_bsize / 2);
                FileGetData(path, buffer, edit_bsize, edit_start);
                memcpy(buffer_cpy, buffer, edit_bsize);
                data = buffer + (offset - edit_start);
            } else edit_mode = false;
        } else { // editor mode
            if (pad_state & (BUTTON_B|BUTTON_START)) {
                edit_mode = false;
                // check for user edits
                u32 diffs = 0;
                for (u32 i = 0; i < edit_bsize; i++) if (buffer[i] != buffer_cpy[i]) diffs++;
                if (diffs && ShowPrompt(true, "You made edits in %i place(s).\nWrite changes to file?", diffs))
                    if (!FileSetData(path, buffer, min(edit_bsize, (fsize - edit_start)), edit_start, false))
                        ShowPrompt(false, "Failed writing to file!");
                data = buffer;
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
    if (MAIN_SCREEN == TOP_SCREEN) memcpy(BOT_SCREEN, bottom_cpy, SCREEN_SIZE_BOT);
    else ClearScreen(BOT_SCREEN, COLOR_STD_BG);
    
    free(bottom_cpy);
    free(buffer);
    free(buffer_cpy);
    return 0;
}

u32 Sha256Calculator(const char* path) {
    u32 drvtype = DriveType(path);
    char pathstr[32 + 1];
    u8 sha256[32];
    TruncateString(pathstr, path, 32, 8);
    if (!FileGetSha256(path, sha256, 0, 0)) {
        ShowPrompt(false, "Calculating SHA-256: failed!");
        return 1;
    } else {
        static char pathstr_prev[32 + 1] = { 0 };
        static u8 sha256_prev[32] = { 0 };
        char sha_path[256];
        u8 sha256_file[32];
        
        snprintf(sha_path, 256, "%s.sha", path);
        bool have_sha = (FileGetData(sha_path, sha256_file, 32, 0) == 32);
        bool match_sha = have_sha && (memcmp(sha256, sha256_file, 32) == 0);
        bool match_prev = (memcmp(sha256, sha256_prev, 32) == 0);
        bool write_sha = (!have_sha || !match_sha) && (drvtype & DRV_SDCARD); // writing only on SD
        if (ShowPrompt(write_sha, "%s\n%016llX%016llX\n%016llX%016llX%s%s%s%s%s",
            pathstr, getbe64(sha256 + 0), getbe64(sha256 + 8), getbe64(sha256 + 16), getbe64(sha256 + 24),
            (have_sha) ? "\nSHA verification: " : "",
            (have_sha) ? ((match_sha) ? "passed!" : "failed!") : "",
            (match_prev) ? "\n \nIdentical with previous file:\n" : "",
            (match_prev) ? pathstr_prev : "",
            (write_sha) ? "\n \nWrite .SHA file?" : "") && write_sha) {
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

u32 StandardCopy(u32* cursor, u32* scroll) {
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
            DrawDirContents(current_dir, (*cursor = i), scroll);
            if (PathCopy(OUTPUT_PATH, path, &flags)) n_success++;
            else { // on failure: show error, break
                char currstr[32+1];
                TruncateString(currstr, path, 32, 12);
                ShowPrompt(false, "%s\nFailed copying item", currstr);
                break;
            }
            current_dir->entry[i].marked = false;
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

u32 BootFirmHandler(const char* bootpath, bool verbose, bool delete) {
    char pathstr[32+1];
    TruncateString(pathstr, bootpath, 32, 8);
    
    size_t firm_size = FileGetSize(bootpath);
    if (!firm_size) return 1;
    if (firm_size > FIRM_MAX_SIZE) {
        if (verbose) ShowPrompt(false, "%s\nFIRM too big, can't boot", pathstr); // unlikely
        return 1;
    }
    
    if (verbose && !ShowPrompt(true, "%s (%dkB)\nWarning: Do not boot FIRMs\nfrom untrusted sources.\n \nBoot FIRM?",
        pathstr, firm_size / 1024))
        return 1;
    
    void* firm = (void*) malloc(FIRM_MAX_SIZE);
    if (!firm) return 1;
    if ((FileGetData(bootpath, firm, firm_size, 0) != firm_size) ||
        !IsBootableFirm(firm, firm_size)) {
        if (verbose) ShowPrompt(false, "%s\nNot a bootable FIRM.", pathstr);
        free(firm);
        return 1;
    }
    
    // encrypted firm handling
    FirmSectionHeader* arm9s = FindFirmArm9Section(firm);
    FirmA9LHeader* a9l = (FirmA9LHeader*)(void*) ((u8*) firm + arm9s->offset);
    if (verbose && (ValidateFirmA9LHeader(a9l) == 0) &&
        ShowPrompt(true, "%s\nFIRM is encrypted.\n \nDecrypt before boot?", pathstr) &&
        (DecryptFirmFull(firm, firm_size) != 0)) {
        free(firm);
        return 1;
    }
        
    // unsupported location handling
    char fixpath[256] = { 0 };
    if (verbose && (*bootpath != '0') && (*bootpath != '1')) {
        const char* optionstr[2] = { "Make a copy at " OUTPUT_PATH "/temp.firm", "Try to boot anyways" };
        u32 user_select = ShowSelectPrompt(2, optionstr, "%s\nWarning: Trying to boot from an\nunsupported location.", pathstr);
        if (user_select == 1) {
            FileSetData(OUTPUT_PATH "/temp.firm", firm, firm_size, 0, true);
            bootpath = OUTPUT_PATH "/temp.firm";
        } else if (!user_select) bootpath = "";
    }
    
    // fix the boot path ("sdmc"/"nand" for Luma et al, hacky af)
    if ((*bootpath == '0') || (*bootpath == '1'))
        snprintf(fixpath, 256, "%s%s", (*bootpath == '0') ? "sdmc" : "nand", bootpath + 1);
    else strncpy(fixpath, bootpath, 256);
    
    // boot the FIRM (if we got a proper fixpath)
    if (*fixpath) {
        if (delete) PathDelete(bootpath);
        BootFirm((FirmHeader*) firm, fixpath);
        while(1);
    }
    
    // a return was not intended
    free(firm);
    return 1;
}

u32 FileAttrMenu(const char* file_path) {
    FILINFO fno;
    if (fvx_stat(file_path, &fno) != FR_OK) {
        char pathstr[32 + 1];
        TruncateString(pathstr, file_path, 32, 8);
        ShowPrompt(false, "%s\nFile info failed!", pathstr);
        return 1;
    }

    char namestr[32];
    char sizestr[32];
    TruncateString(namestr, fno.fname, 32, 8);
    FormatNumber(sizestr, fno.fsize);
    const bool vrt = (fno.fattrib & AM_VRT);
    u8 new_attrib = fno.fattrib;

    while (true) {
        ShowString(
            "%s\n"
            " \n"
            "filesize: %s byte\n"
            "modified: %04lu-%02lu-%02lu %02lu:%02lu:%02lu\n"
            " \n"
            "[%c] %sread-only  [%c] %shidden\n"
            "[%c] %ssystem     [%c] %sarchive\n"
            "[%c] %svirtual\n"
            " \n"
            "%s"
            "%s",
            namestr, sizestr,
            1980 + ((fno.fdate >> 9) & 0x7F), (fno.fdate >> 5) & 0x0F, (fno.fdate >> 0) & 0x1F,
            (fno.ftime >> 11) & 0x1F, (fno.ftime >> 5) & 0x3F, ((fno.ftime >> 0) & 0x1F) << 1,
            (new_attrib & AM_RDO) ? 'X' : ' ', (vrt ? "" : "\x18 "),
            (new_attrib & AM_HID) ? 'X' : ' ', (vrt ? "" : "\x19 "),
            (new_attrib & AM_SYS) ? 'X' : ' ', (vrt ? "" : "\x1A "),
            (new_attrib & AM_ARC) ? 'X' : ' ', (vrt ? "" : "\x1B "),
            vrt ? 'X' : ' ', (vrt ? "" : "  "),
            vrt ? "" : "(\x18\x19\x1A\x1B to change attributes)\n",
            (vrt || (new_attrib == fno.fattrib)) ? "(<A> to continue)" : "(<A> to apply, <B> to cancel)");

        while (true) {
            u32 pad_state = InputWait(0);
            if (pad_state & (BUTTON_A | BUTTON_B)) {
                bool apply = !vrt && (new_attrib != fno.fattrib) && (pad_state & BUTTON_A);
                const u8 mask = (AM_RDO | AM_HID | AM_SYS | AM_ARC);
                if (apply && !PathAttr(file_path, new_attrib & mask, mask)) {
                    ShowPrompt(false, "%s\nFailed to set attributes!", namestr);
                }
                ClearScreenF(true, false, COLOR_STD_BG);
                return 0;
            } else if (vrt) continue;

            switch (pad_state) {
                case BUTTON_UP:
                    new_attrib ^= AM_RDO;
                    break;
                case BUTTON_DOWN:
                    new_attrib ^= AM_HID;
                    break;
                case BUTTON_RIGHT:
                    new_attrib ^= AM_SYS;
                    break;
                case BUTTON_LEFT:
                    new_attrib ^= AM_ARC;
                    break;
                default:
                    continue;
            }
            break;
        }
    }
}

u32 FileHandlerMenu(char* current_path, u32* cursor, u32* scroll, PaneData** pane) {
    const char* file_path = (&(current_dir->entry[*cursor]))->path;
    const char* optionstr[16];
    
    // check for file lock
    if (!FileUnlock(file_path)) return 1;
    
    u64 filetype = IdentifyFileType(file_path);
    u32 drvtype = DriveType(file_path);
    
    bool in_output_path = (strncasecmp(current_path, OUTPUT_PATH, 256) == 0);
    
    // don't handle TMDs inside the game drive, won't work properly anyways
    if ((filetype & GAME_TMD) && (drvtype & DRV_GAME)) filetype &= ~GAME_TMD;
    
    // special stuff, only available for known filetypes (see int special below)
    bool mountable = (FTYPE_MOUNTABLE(filetype) && !(drvtype & DRV_IMAGE) &&
        !((drvtype & (DRV_SYSNAND|DRV_EMUNAND)) && (drvtype & DRV_VIRTUAL) && (filetype & IMG_FAT)));
    bool verificable = (FYTPE_VERIFICABLE(filetype));
    bool decryptable = (FYTPE_DECRYPTABLE(filetype));
    bool encryptable = (FYTPE_ENCRYPTABLE(filetype));
    bool cryptable_inplace = ((encryptable||decryptable) && !in_output_path && (drvtype & DRV_FAT));
    bool cia_buildable = (FTYPE_CIABUILD(filetype));
    bool cia_buildable_legit = (FTYPE_CIABUILD_L(filetype));
    bool cxi_dumpable = (FTYPE_CXIDUMP(filetype));
    bool tik_buildable = (FTYPE_TIKBUILD(filetype)) && !in_output_path;
    bool key_buildable = (FTYPE_KEYBUILD(filetype)) && !in_output_path && !((drvtype & DRV_VIRTUAL) && (drvtype & DRV_SYSNAND));
    bool titleinfo = (FTYPE_TITLEINFO(filetype));
    bool renamable = (FTYPE_RENAMABLE(filetype));
    bool transferable = (FTYPE_TRANSFERABLE(filetype) && IS_A9LH && (drvtype & DRV_FAT));
    bool hsinjectable = (FTYPE_HASCODE(filetype));
    bool extrcodeable = (FTYPE_HASCODE(filetype));
    bool extrdiffable = (FTYPE_ISDISADIFF(filetype));
    bool restorable = (FTYPE_RESTORABLE(filetype) && IS_A9LH && !(drvtype & DRV_SYSNAND));
    bool ebackupable = (FTYPE_EBACKUP(filetype));
    bool ncsdfixable = (FTYPE_NCSDFIXABLE(filetype));
    bool xorpadable = (FTYPE_XORPAD(filetype));
    bool keyinitable = (FTYPE_KEYINIT(filetype)) && !((drvtype & DRV_VIRTUAL) && (drvtype & DRV_SYSNAND));
    bool keyinstallable = (FTYPE_KEYINSTALL(filetype)) && !((drvtype & DRV_VIRTUAL) && (drvtype & DRV_SYSNAND));
    bool scriptable = (FTYPE_SCRIPT(filetype));
    bool fontable = (FTYPE_FONT(filetype));
    bool viewable = (FTYPE_GFX(filetype));
    bool bootable = (FTYPE_BOOTABLE(filetype));
    bool installable = (FTYPE_INSTALLABLE(filetype));
    bool agbexportable = (FTPYE_AGBSAVE(filetype) && (drvtype & DRV_VIRTUAL) && (drvtype & DRV_SYSNAND));
    bool agbimportable = (FTPYE_AGBSAVE(filetype) && (drvtype & DRV_VIRTUAL) && (drvtype & DRV_SYSNAND));
    
    char cxi_path[256] = { 0 }; // special options for TMD
    if ((filetype & GAME_TMD) && !(filetype & FLAG_NUSCDN) &&
        (GetTmdContentPath(cxi_path, file_path) == 0) &&
        (PathExist(cxi_path))) {
        u64 filetype_cxi = IdentifyFileType(cxi_path);
        mountable = (FTYPE_MOUNTABLE(filetype_cxi) && !(drvtype & DRV_IMAGE));
        extrcodeable = (FTYPE_HASCODE(filetype_cxi));
    }
    
    bool special_opt = mountable || verificable || decryptable || encryptable || cia_buildable || cia_buildable_legit || cxi_dumpable || tik_buildable || key_buildable || titleinfo || renamable || transferable || hsinjectable || restorable || xorpadable || ebackupable || ncsdfixable || extrcodeable || extrdiffable || keyinitable || keyinstallable || bootable || scriptable || fontable || viewable || installable || agbexportable || agbimportable;
    
    char pathstr[32+1];
    TruncateString(pathstr, file_path, 32, 8);
    
    u32 n_marked = 0;
    if ((&(current_dir->entry[*cursor]))->marked) {
        for (u32 i = 0; i < current_dir->n_entries; i++) 
            if (current_dir->entry[i].marked) n_marked++;
    }
    
    // main menu processing
    int n_opt = 0;
    int special = (special_opt) ? ++n_opt : -1;
    int hexviewer = ++n_opt;
    int textviewer = (filetype & TXT_GENERIC) ? ++n_opt : -1;
    int calcsha = ++n_opt;
    int calccmac = (CheckCmacPath(file_path) == 0) ? ++n_opt : -1;
    int fileinfo = ++n_opt;
    int copystd = (!in_output_path) ? ++n_opt : -1;
    int inject = ((clipboard->n_entries == 1) &&
        (clipboard->entry[0].type == T_FILE) &&
        (strncmp(clipboard->entry[0].path, file_path, 256) != 0)) ?
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
        (filetype & GAME_GBA)   ? "GBA image options..."  :
        (filetype & GAME_TICKET)? "Ticket options..."     :
        (filetype & GAME_TAD)   ? "TAD image options..."  :
        (filetype & GAME_3DSX)  ? "Show 3DSX title info"  :
        (filetype & SYS_FIRM  ) ? "FIRM image options..." :
        (filetype & SYS_AGBSAVE)? (agbimportable) ? "AGBSAVE options..." : "Dump GBA VC save" :
        (filetype & SYS_TICKDB) ? "Ticket.db options..."  :
        (filetype & SYS_DIFF)   ? "Extract DIFF data"     :
        (filetype & BIN_TIKDB)  ? "Titlekey options..."   :
        (filetype & BIN_KEYDB)  ? "AESkeydb options..."   :
        (filetype & BIN_LEGKEY) ? "Build " KEYDB_NAME     :
        (filetype & BIN_NCCHNFO)? "NCCHinfo options..."   :
        (filetype & TXT_SCRIPT) ? "Execute GM9 script"    :
        (filetype & FONT_PBM)   ? "Set as active font"    :
        (filetype & GFX_PCX)    ? "View PCX bitmap file"  :
        (filetype & HDR_NAND)   ? "Rebuild NCSD header"   :
        (filetype & NOIMG_NAND) ? "Rebuild NCSD header" : "???";
    optionstr[hexviewer-1] = "Show in Hexeditor";
    optionstr[calcsha-1] = "Calculate SHA-256";
    optionstr[fileinfo-1] = "Show file info";
    if (textviewer > 0) optionstr[textviewer-1] = "Show in Textviewer";
    if (calccmac > 0) optionstr[calccmac-1] = "Calculate CMAC";
    if (copystd > 0) optionstr[copystd-1] = "Copy to " OUTPUT_PATH;
    if (inject > 0) optionstr[inject-1] = "Inject data @offset";
    if (searchdrv > 0) optionstr[searchdrv-1] = "Open containing folder";
    
    int user_select = ShowSelectPrompt(n_opt, optionstr, (n_marked > 1) ?
        "%s\n%(%lu files selected)" : "%s", pathstr, n_marked);
    if (user_select == hexviewer) { // -> show in hex viewer
        FileHexViewer(file_path);
        GetDirContents(current_dir, current_path);
        return 0;
    }
    else if (user_select == textviewer) { // -> show in text viewer
        FileTextViewer(file_path, scriptable);
        return 0;
    }
    else if (user_select == calcsha) { // -> calculate SHA-256
        Sha256Calculator(file_path);
        GetDirContents(current_dir, current_path);
        return 0;
    }
    else if (user_select == calccmac) { // -> calculate CMAC
        optionstr[0] = "Check current CMAC only";
        optionstr[1] = "Verify CMAC for all";
        optionstr[2] = "Fix CMAC for all";
        user_select = (n_marked > 1) ? ShowSelectPrompt(3, optionstr, "%s\n%(%lu files selected)", pathstr, n_marked) : 1;
        if (user_select == 1) {
            CmacCalculator(file_path);
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
        return FileHandlerMenu(current_path, cursor, scroll, pane);
    }
    else if (user_select == fileinfo) { // -> show file info
        FileAttrMenu(file_path);
        return 0;
    }
    else if (user_select == copystd) { // -> copy to OUTPUT_PATH
        StandardCopy(cursor, scroll);
        return 0;
    }
    else if (user_select == inject) { // -> inject data from clipboard
        char origstr[18 + 1];
        TruncateString(origstr, clipboard->entry[0].name, 18, 10);
        u64 offset = ShowHexPrompt(0, 8, "Inject data from %s?\nSpecifiy offset below.", origstr);
        if (offset != (u64) -1) {
            if (!FileInjectFile(file_path, clipboard->entry[0].path, (u32) offset, 0, 0, NULL))
                ShowPrompt(false, "Failed injecting %s", origstr);
            clipboard->n_entries = 0;
        }
        return 0;
    }
    else if (user_select == searchdrv) { // -> search drive, open containing path
        char* last_slash = strrchr(file_path, '/');
        if (last_slash) {
            if (N_PANES) { // switch to next pane
                memcpy((*pane)->path, current_path, 256);  // store current pane state
                (*pane)->cursor = *cursor;
                (*pane)->scroll = *scroll;
                if (++*pane >= panedata + N_PANES) *pane -= N_PANES;
            }
            snprintf(current_path, last_slash - file_path + 1, "%s", file_path);
            GetDirContents(current_dir, current_path);
            *scroll = 0;
            for (*cursor = 1; *cursor < current_dir->n_entries; (*cursor)++) {
                DirEntry* entry = &(current_dir->entry[*cursor]);
                if (strncasecmp(entry->path, file_path, 256) == 0) break;
            }
            if (*cursor >= current_dir->n_entries)
                *cursor = 1;
        }
        return 0;
    }
    else if (user_select != special) {
        return 1;
    }
    
    // stuff for special menu starts here
    n_opt = 0;
    int show_info = (titleinfo) ? ++n_opt : -1;
    int mount = (mountable) ? ++n_opt : -1;
    int restore = (restorable) ? ++n_opt : -1;
    int ebackup = (ebackupable) ? ++n_opt : -1;
    int ncsdfix = (ncsdfixable) ? ++n_opt : -1;
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
    int extrcode = (extrcodeable) ? ++n_opt : -1;
    int extrdiff = (extrdiffable) ? ++n_opt : -1;
    int rename = (renamable) ? ++n_opt : -1;
    int xorpad = (xorpadable) ? ++n_opt : -1;
    int xorpad_inplace = (xorpadable) ? ++n_opt : -1;
    int keyinit = (keyinitable) ? ++n_opt : -1;
    int keyinstall = (keyinstallable) ? ++n_opt : -1;
    int install = (installable) ? ++n_opt : -1;
    int boot = (bootable) ? ++n_opt : -1;
    int script = (scriptable) ? ++n_opt : -1;
    int font = (fontable) ? ++n_opt : -1;
    int view = (viewable) ? ++n_opt : -1;
    int agbexport = (agbexportable) ? ++n_opt : -1;
    int agbimport = (agbimportable) ? ++n_opt : -1;
    if (mount > 0) optionstr[mount-1] = (filetype & GAME_TMD) ? "Mount CXI/NDS to drive" : "Mount image to drive";
    if (restore > 0) optionstr[restore-1] = "Restore SysNAND (safe)";
    if (ebackup > 0) optionstr[ebackup-1] = "Update embedded backup";
    if (ncsdfix > 0) optionstr[ncsdfix-1] = "Rebuild NCSD header";
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
    if (extrcode > 0) optionstr[extrcode-1] = "Extract " EXEFS_CODE_NAME;
    if (extrdiff > 0) optionstr[extrdiff-1] = "Extract DIFF data";
    if (keyinit > 0) optionstr[keyinit-1] = "Init " KEYDB_NAME;
    if (keyinstall > 0) optionstr[keyinstall-1] = "Install " KEYDB_NAME;
    if (install > 0) optionstr[install-1] = "Install FIRM";
    if (boot > 0) optionstr[boot-1] = "Boot FIRM";
    if (script > 0) optionstr[script-1] = "Execute GM9 script";
    if (view > 0) optionstr[font-1] = "View PCX bitmap file";
    if (font > 0) optionstr[font-1] = "Set as active font";
    if (agbexport > 0) optionstr[agbexport-1] = "Dump GBA VC save";
    if (agbimport > 0) optionstr[agbimport-1] = "Inject GBA VC save";
    
    // auto select when there is only one option
    user_select = (n_opt <= 1) ? n_opt : (int) ShowSelectPrompt(n_opt, optionstr, (n_marked > 1) ?
        "%s\n%(%lu files selected)" : "%s", pathstr, n_marked);
    if (user_select == mount) { // -> mount file as image
        const char* mnt_drv_paths[] = { "7:", "G:", "K:", "T:", "I:" }; // maybe move that to fsdrive.h
        if (clipboard->n_entries && (DriveType(clipboard->entry[0].path) & DRV_IMAGE))
            clipboard->n_entries = 0; // remove last mounted image clipboard entries
        InitImgFS((filetype & GAME_TMD) ? cxi_path : file_path);
        
        const char* drv_path = NULL; // find path of mounted drive
        for (u32 i = 0; i < (sizeof(mnt_drv_paths) / sizeof(const char*)); i++) {
            if (DriveType((drv_path = mnt_drv_paths[i]))) break;
            drv_path = NULL;
        }
        
        if (!drv_path) {
            ShowPrompt(false, "Mounting image: failed");
            InitImgFS(NULL);
        } else { // open in next pane?
            if (ShowPrompt(true, "%s\nMounted as drive %s\nEnter path now?", pathstr, drv_path)) {
                if (N_PANES) {
                    memcpy((*pane)->path, current_path, 256);  // store current pane state
                    (*pane)->cursor = *cursor;
                    (*pane)->scroll = *scroll;
                    if (++*pane >= panedata + N_PANES) *pane -= N_PANES;
                }
                
                strncpy(current_path, drv_path, 256);
                GetDirContents(current_dir, current_path);
                *cursor = 1;
                *scroll = 0;
            }
        }
        return 0;
    }
    else if (user_select == decrypt) { // -> decrypt game file
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
                DrawDirContents(current_dir, (*cursor = i), scroll);
                if (!(filetype & BIN_KEYDB) && (CryptGameFile(path, inplace, false) == 0)) n_success++;
                else if ((filetype & BIN_KEYDB) && (CryptAesKeyDb(path, inplace, false) == 0)) n_success++;
                else { // on failure: show error, continue
                    char lpathstr[32+1];
                    TruncateString(lpathstr, path, 32, 8);
                    if (ShowPrompt(true, "%s\nDecryption failed\n \nContinue?", lpathstr)) continue;
                    else break;
                }
                current_dir->entry[i].marked = false;
            }
            if (n_other || n_unencrypted) {
                ShowPrompt(false, "%lu/%lu files decrypted ok\n%lu/%lu not encrypted\n%lu/%lu not of same type",
                    n_success, n_marked, n_unencrypted, n_marked, n_other, n_marked);
            } else ShowPrompt(false, "%lu/%lu files decrypted ok", n_success, n_marked);
            if (!inplace && n_success) ShowPrompt(false, "%lu files written to %s", n_success, OUTPUT_PATH);
        } else {
            if (!(filetype & BIN_KEYDB) && (CheckEncryptedGameFile(file_path) != 0)) {
                ShowPrompt(false, "%s\nFile is not encrypted", pathstr);
            } else {
                u32 ret = (filetype & BIN_KEYDB) ? CryptAesKeyDb(file_path, inplace, false) :
                    CryptGameFile(file_path, inplace, false);
                if (inplace || (ret != 0)) ShowPrompt(false, "%s\nDecryption %s", pathstr, (ret == 0) ? "success" : "failed");
                else ShowPrompt(false, "%s\nDecrypted to %s", pathstr, OUTPUT_PATH);
            }
        }
        return 0;
    }
    else if (user_select == encrypt) { // -> encrypt game file
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
                DrawDirContents(current_dir, (*cursor = i), scroll);
                if (!(filetype & BIN_KEYDB) && (CryptGameFile(path, inplace, true) == 0)) n_success++;
                else if ((filetype & BIN_KEYDB) && (CryptAesKeyDb(path, inplace, true) == 0)) n_success++;
                else { // on failure: show error, continue
                    char lpathstr[32+1];
                    TruncateString(lpathstr, path, 32, 8);
                    if (ShowPrompt(true, "%s\nEncryption failed\n \nContinue?", lpathstr)) continue;
                    else break;
                }
                current_dir->entry[i].marked = false;
            }
            if (n_other) {
                ShowPrompt(false, "%lu/%lu files encrypted ok\n%lu/%lu not of same type",
                    n_success, n_marked, n_other, n_marked);
            } else ShowPrompt(false, "%lu/%lu files encrypted ok", n_success, n_marked);
            if (!inplace && n_success) ShowPrompt(false, "%lu files written to %s", n_success, OUTPUT_PATH);
        } else {
            u32 ret = (filetype & BIN_KEYDB) ? CryptAesKeyDb(file_path, inplace, true) :
                CryptGameFile(file_path, inplace, true);
            if (inplace || (ret != 0)) ShowPrompt(false, "%s\nEncryption %s", pathstr, (ret == 0) ? "success" : "failed");
            else ShowPrompt(false, "%s\nEncrypted to %s", pathstr, OUTPUT_PATH);
        }
        return 0;
    }
    else if ((user_select == cia_build) || (user_select == cia_build_legit) || (user_select == cxi_dump)) { // -> build CIA / dump CXI
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
                DrawDirContents(current_dir, (*cursor = i), scroll);
                if (((user_select != cxi_dump) && (BuildCiaFromGameFile(path, force_legit) == 0)) ||
                    ((user_select == cxi_dump) && (DumpCxiSrlFromTmdFile(path) == 0))) n_success++;
                else { // on failure: show error, continue
                    char lpathstr[32+1];
                    TruncateString(lpathstr, path, 32, 8);
                    if (ShowPrompt(true, "%s\nBuild %s failed\n \nContinue?", lpathstr, type)) continue;
                    else break;
                }
                current_dir->entry[i].marked = false;
            }
            if (n_other) ShowPrompt(false, "%lu/%lu %ss built ok\n%lu/%lu not of same type",
                n_success, n_marked, type, n_other, n_marked);
            else ShowPrompt(false, "%lu/%lu %ss built ok", n_success, n_marked, type);
            if (n_success) ShowPrompt(false, "%lu files written to %s", n_success, OUTPUT_PATH);
            if (n_success && in_output_path) GetDirContents(current_dir, current_path);
        } else {
            if (((user_select != cxi_dump) && (BuildCiaFromGameFile(file_path, force_legit) == 0)) ||
                ((user_select == cxi_dump) && (DumpCxiSrlFromTmdFile(file_path) == 0))) {
                ShowPrompt(false, "%s\n%s built to %s", pathstr, type, OUTPUT_PATH);
                if (in_output_path) GetDirContents(current_dir, current_path);
            } else ShowPrompt(false, "%s\n%s build failed", pathstr, type);
        }
        return 0;
    }
    else if (user_select == verify) { // -> verify game / nand file
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
                DrawDirContents(current_dir, (*cursor = i), scroll);
                if ((filetype & IMG_NAND) && (ValidateNandDump(path) == 0)) n_success++;
                else if (VerifyGameFile(path) == 0) n_success++;
                else { // on failure: show error, continue
                    char lpathstr[32+1];
                    TruncateString(lpathstr, path, 32, 8);
                    if (ShowPrompt(true, "%s\nVerification failed\n \nContinue?", lpathstr)) {
                        if (!(filetype & (GAME_CIA|GAME_TMD))) ShowProgress(0, n_marked, path); // restart progress bar
                        continue;
                    } else break;
                }
                current_dir->entry[i].marked = false;
            }
            if (n_other) ShowPrompt(false, "%lu/%lu files verified ok\n%lu/%lu not of same type",
                n_success, n_marked, n_other, n_marked);
            else ShowPrompt(false, "%lu/%lu files verified ok", n_success, n_marked); 
        } else {
            ShowString("%s\nVerifying file, please wait...", pathstr);
            if (filetype & IMG_NAND) {
                ShowPrompt(false, "%s\nNAND validation %s", pathstr,
                    (ValidateNandDump(file_path) == 0) ? "success" : "failed");
            } else ShowPrompt(false, "%s\nVerification %s", pathstr,
                (VerifyGameFile(file_path) == 0) ? "success" : "failed");
        }
        return 0;
    }
    else if ((user_select == tik_build_enc) || (user_select == tik_build_dec)) { // -> (Re)Build titlekey database
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
            (BuildTitleKeyInfo(file_path, dec, true) == 0) ? "success" : "failed");
        return 0;
    }
    else if (user_select == key_build) { // -> (Re)Build AES key database
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
            (BuildKeyDb(file_path, true) == 0) ? "success" : "failed");
        return 0;
    }
    else if (user_select == rename) { // -> Game file renamer
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
    }
    else if (user_select == show_info) { // -> Show title info
        if (ShowGameFileTitleInfo(file_path) != 0)
            ShowPrompt(false, "Title info: not found");
        return 0;
    }
    else if (user_select == hsinject) { // -> Inject to Health & Safety
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
                (InjectHealthAndSafety(file_path, destdrv[user_select-1]) == 0) ? "success" : "failed");
        }
        return 0;
    }
    else if ((user_select == extrcode) || (user_select == extrdiff)) { // -> Extract .code or DIFF partition
        if ((n_marked > 1) && ShowPrompt(true, "Try to extract all %lu selected files?", n_marked)) {
            u32 n_success = 0;
            u32 n_other = 0;
            u32 n_processed = 0;
            for (u32 i = 0; i < current_dir->n_entries; i++) {
                const char* path = current_dir->entry[i].path;
                if (!current_dir->entry[i].marked) 
                    continue;
                if (!ShowProgress(n_processed++, n_marked, path)) break;
                if (!(IdentifyFileType(path) & filetype & TYPE_BASE)) {
                    n_other++;
                    continue;
                }
                DrawDirContents(current_dir, (*cursor = i), scroll);
                if (filetype & SYS_DIFF) {
                    if (ExtractDataFromDisaDiff(path) == 0) n_success++;
                    else continue;
                } else if (filetype & GAME_TMD) {
                    char cxi_pathl[256] = { 0 };
                    if ((GetTmdContentPath(cxi_pathl, path) == 0) && PathExist(cxi_pathl) && 
                        (ExtractCodeFromCxiFile(cxi_pathl, NULL, NULL) == 0)) {
                        n_success++;
                    } else continue;
                } else {
                    if (ExtractCodeFromCxiFile(path, NULL, NULL) == 0) n_success++;
                    else continue;
                }
                current_dir->entry[i].marked = false;
            }
            if (n_other) ShowPrompt(false, "%lu/%lu files extracted ok\n%lu/%lu not of same type",
                n_success, n_marked, n_other, n_marked);
            else ShowPrompt(false, "%lu/%lu files extracted ok", n_success, n_marked); 
        } else if (filetype & SYS_DIFF) {
            ShowString("%s\nExtracting data, please wait...", pathstr);
            if (ExtractDataFromDisaDiff(file_path) == 0) {
                ShowPrompt(false, "%s\ndata extracted to " OUTPUT_PATH, pathstr);
            } else ShowPrompt(false, "%s\ndata extract failed", pathstr);
        } else {
            char extstr[8] = { 0 };
            ShowString("%s\nExtracting .code, please wait...", pathstr);
            if (ExtractCodeFromCxiFile((filetype & GAME_TMD) ? cxi_path : file_path, NULL, extstr) == 0) {
                ShowPrompt(false, "%s\n%s extracted to " OUTPUT_PATH, pathstr, extstr);
            } else ShowPrompt(false, "%s\n.code extract failed", pathstr);
        }
        return 0;
    }
    else if (user_select == ctrtransfer) { // -> transfer CTRNAND image to SysNAND
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
        if (n_opt) {
            user_select = (n_opt > 1) ? (int) ShowSelectPrompt(n_opt, optionstr, pathstr) : 1;
            if (user_select) {
                ShowPrompt(false, "%s\nCTRNAND transfer %s", pathstr,
                    (TransferCtrNandImage(file_path, destdrv[user_select-1]) == 0) ? "success" : "failed");
            }
        } else ShowPrompt(false, "%s\nNo valid destination found");
        return 0;
    }
    else if (user_select == restore) { // -> restore SysNAND (A9LH preserving)
        ShowPrompt(false, "%s\nNAND restore %s", pathstr,
            (SafeRestoreNandDump(file_path) == 0) ? "success" : "failed");
        return 0;
    }
    else if (user_select == ncsdfix) { // -> inject sighaxed NCSD
        ShowPrompt(false, "%s\nRebuild NCSD %s", pathstr,
            (FixNandHeader(file_path, !(filetype == HDR_NAND)) == 0) ? "success" : "failed");
        GetDirContents(current_dir, current_path);
        InitExtFS(); // this might have fixed something, so try this
        return 0;
    }
    else if ((user_select == xorpad) || (user_select == xorpad_inplace)) { // -> build xorpads
        bool inplace = (user_select == xorpad_inplace);
        bool success = (BuildNcchInfoXorpads((inplace) ? current_path : OUTPUT_PATH, file_path) == 0);
        ShowPrompt(false, "%s\nNCCHinfo padgen %s%s", pathstr,
            (success) ? "success" : "failed", 
            (!success || inplace) ? "" : "\nOutput dir: " OUTPUT_PATH);
        GetDirContents(current_dir, current_path);
        for (; *cursor < current_dir->n_entries; (*cursor)++) {
            DirEntry* entry = &(current_dir->entry[*cursor]);
            if (strncasecmp(entry->name, NCCHINFO_NAME, 32) == 0) break;
        }
        if (*cursor >= current_dir->n_entries) {
            *scroll = 0;
            *cursor = 1;
        }
        return 0;
    }
    else if (user_select == ebackup) { // -> update embedded backup
        ShowString("%s\nUpdating embedded backup...", pathstr);
        bool required = (CheckEmbeddedBackup(file_path) != 0);
        bool success = (required && (EmbedEssentialBackup(file_path) == 0));
        ShowPrompt(false, "%s\nBackup update: %s", pathstr, (!required) ? "not required" :
            (success) ? "completed" : "failed!");
        GetDirContents(current_dir, current_path);
        return 0;
    }
    else if (user_select == keyinit) { // -> initialise keys from aeskeydb.bin
        if (ShowPrompt(true, "Warning: Keys are not verified.\nContinue on your own risk?"))
            ShowPrompt(false, "%s\nAESkeydb init %s", pathstr, (InitKeyDb(file_path) == 0) ? "success" : "failed");
        return 0;
    }
    else if (user_select == keyinstall) { // -> install keys from aeskeydb.bin
        ShowPrompt(false, "%s\nAESkeydb install %s", pathstr, (SafeInstallKeyDb(file_path) == 0) ? "success" : "failed");
        return 0;
    }
    else if (user_select == install) { // -> install FIRM
        size_t firm_size = FileGetSize(file_path);
        u32 slots = 1;
        if (GetNandPartitionInfo(NULL, NP_TYPE_FIRM, NP_SUBTYPE_CTR, 1, NAND_SYSNAND) == 0) {
            optionstr[0] = "Install to FIRM0";
            optionstr[1] = "Install to FIRM1";
            optionstr[2] = "Install to both";
            // this only works up to FIRM1
            slots = ShowSelectPrompt(3, optionstr, "%s (%dkB)\nInstall to SysNAND?", pathstr, firm_size / 1024);
        } else slots = ShowPrompt(true, "%s (%dkB)\nInstall to SysNAND?", pathstr, firm_size / 1024) ? 1 : 0;
        if (slots) ShowPrompt(false, "%s (%dkB)\nInstall %s", pathstr, firm_size / 1024,
            (SafeInstallFirm(file_path, slots) == 0) ? "success" : "failed!");
        return 0;
    }
    else if (user_select == boot) { // -> boot FIRM
        BootFirmHandler(file_path, true, false);
        return 0;
    }
    else if (user_select == script) { // execute script
        if (ShowPrompt(true, "%s\nWarning: Do not run scripts\nfrom untrusted sources.\n \nExecute script?", pathstr))
            ShowPrompt(false, "%s\nScript execute %s", pathstr, ExecuteGM9Script(file_path) ? "success" : "failure");
        GetDirContents(current_dir, current_path);
        ClearScreenF(true, true, COLOR_STD_BG);
        return 0;
    }
    else if (user_select == font) { // set font
        u8* pbm = (u8*) malloc(0x10000); // arbitrary, should be enough by far
        if (!pbm) return 1;
        u32 pbm_size = FileGetData(file_path, pbm, 0x10000, 0);
        if (pbm_size) SetFontFromPbm(pbm, pbm_size);
        ClearScreenF(true, true, COLOR_STD_BG);
        free(pbm);
        return 0;
    }
    else if (user_select == view) { // view gfx
        if (FileGraphicsViewer(file_path) != 0)
            ShowPrompt(false, "%s\nError: Cannot view file");
        return 0;
    }
    else if (user_select == agbexport) { // export GBA VC save
        if (DumpGbaVcSavegame(file_path) == 0)
            ShowPrompt(false, "Savegame dumped to " OUTPUT_PATH ".");
        else ShowPrompt(false, "Savegame dump failed!");
        return 0;
    }
    else if (user_select == agbimport) { // import GBA VC save
        if (clipboard->n_entries != 1) {
            ShowPrompt(false, "GBA VC savegame has to\nbe in the clipboard.");
        } else {
            ShowPrompt(false, "Savegame inject %s.",
                (InjectGbaVcSavegame(file_path, clipboard->entry[0].path) == 0) ? "success" : "failed!");
            clipboard->n_entries = 0;
        }
        return 0;
    }
    
    return FileHandlerMenu(current_path, cursor, scroll, pane);
}

u32 HomeMoreMenu(char* current_path) {
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
    int clock = ++n_opt;
    int sysinfo = ++n_opt;
    int readme = (FindVTarFileInfo(VRAM0_README_MD, NULL)) ? (int) ++n_opt : -1;
    
    if (sdformat > 0) optionstr[sdformat - 1] = "SD format menu";
    if (bonus > 0) optionstr[bonus - 1] = "Bonus drive setup";
    if (multi > 0) optionstr[multi - 1] = "Switch EmuNAND";
    if (bsupport > 0) optionstr[bsupport - 1] = "Build support files";
    if (hsrestore > 0) optionstr[hsrestore - 1] = "Restore H&S";
    if (clock > 0) optionstr[clock - 1] = "Set RTC date&time";
    if (sysinfo > 0) optionstr[sysinfo - 1] = "System info";
    if (readme > 0) optionstr[readme - 1] = "Show ReadMe";
    
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
    }
    else if (user_select == bonus) { // setup bonus drive
        if (clipboard->n_entries && (DriveType(clipboard->entry[0].path) & (DRV_BONUS|DRV_IMAGE)))
            clipboard->n_entries = 0; // remove bonus drive clipboard entries
        if (!SetupBonusDrive()) ShowPrompt(false, "Setup failed!");
        ClearScreenF(true, true, COLOR_STD_BG);
        GetDirContents(current_dir, current_path);
        return 0;
    }
    else if (user_select == multi) { // switch EmuNAND offset
        while (ShowPrompt(true, "Current EmuNAND offset is %06X.\nSwitch to next offset?", GetEmuNandBase())) {
            if (clipboard->n_entries && (DriveType(clipboard->entry[0].path) & DRV_EMUNAND))
                clipboard->n_entries = 0; // remove EmuNAND clipboard entries
            DismountDriveType(DRV_EMUNAND);
            AutoEmuNandBase(false);
            InitExtFS();
        }
        GetDirContents(current_dir, current_path);
        return 0;
    }
    else if (user_select == bsupport) { // build support files
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
    }
    else if (user_select == hsrestore) { // restore Health & Safety
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
    }
    else if (user_select == clock) { // RTC clock setter
        DsTime dstime;
        get_dstime(&dstime);
        if (ShowRtcSetterPrompt(&dstime, "Set RTC date&time:")) {
            char timestr[32];
            set_dstime(&dstime);
            GetTimeString(timestr, true, true);
            ShowPrompt(false, "New RTC date&time is:\n%s\n \nHint: HOMEMENU time needs\nmanual adjustment after\nsetting the RTC.",
                timestr);
        }
        return 0;
    }
    else if (user_select == sysinfo) { // Myria's system info
        char* sysinfo_txt = (char*) malloc(STD_BUFFER_SIZE);
        if (!sysinfo_txt) return 1;
        MyriaSysinfo(sysinfo_txt);
        MemTextViewer(sysinfo_txt, strnlen(sysinfo_txt, STD_BUFFER_SIZE), 1, false);
        free(sysinfo_txt);
        return 0;
    }
    else if (user_select == readme) { // Display GodMode9 readme
        u64 README_md_size;
        char* README_md = FindVTarFileInfo(VRAM0_README_MD, &README_md_size);
        MemToCViewer(README_md, README_md_size, "GodMode9 ReadMe Table of Contents");
        return 0;
    } else return 1;
    
    return HomeMoreMenu(current_path);
}

u32 GodMode(int entrypoint) {
    const u32 quick_stp = (MAIN_SCREEN == TOP_SCREEN) ? 20 : 19;
    u32 exit_mode = GODMODE_EXIT_POWEROFF;
    
    char current_path[256] = { 0x00 };
    u32 cursor = 0;
    u32 scroll = 0;
    
    int mark_next = -1;
    u32 last_write_perm = GetWritePermissions();
    u32 last_clipboard_size = 0;
    
    bool bootloader = IS_SIGHAX && (entrypoint == ENTRY_NANDBOOT);
    bool bootmenu = bootloader && (BOOTMENU_KEY != BUTTON_START) && CheckButton(BOOTMENU_KEY);
    bool godmode9 = !bootloader;
    
    // FIRM from FCRAM handling
    FirmHeader* firm_in_mem = (FirmHeader*) __FIRMTMP_ADDR; // should be safe here
    if (bootloader) { // check for FIRM in FCRAM, but prevent bootloops
        void* addr = (void*) __FIRMRAM_ADDR;
        u32 firm_size = GetFirmSize((FirmHeader*) addr);
        memcpy(firm_in_mem, "NOPE", 4); // overwrite header to prevent bootloops
        if (firm_size && (firm_size <= (__FIRMRAM_END - __FIRMRAM_ADDR))) {
            memcpy(firm_in_mem, addr, firm_size);
            memcpy(addr, "NOPE", 4); // to prevent bootloops
        }
    }
    
    // get mode string for splash screen
    const char* disp_mode = NULL;
    if (bootloader) disp_mode = "bootloader mode\nR+LEFT for menu";
    else if (!IS_SIGHAX && (entrypoint == ENTRY_NANDBOOT)) disp_mode = "oldloader mode";
    else if (entrypoint == ENTRY_NTRBOOT) disp_mode = "ntrboot mode";
    else if (entrypoint == ENTRY_UNKNOWN) disp_mode = "unknown mode";
    
    bool show_splash = true;
    #ifdef SALTMODE
    show_splash = !bootloader;
    #endif
    
    // init font
    if (!SetFontFromPbm(NULL, 0)) return exit_mode;
    
    // show splash screen (if enabled)
    ClearScreenF(true, true, COLOR_STD_BG);
    if (show_splash) SplashInit(disp_mode);
    u64 timer = timer_start(); // for splash delay
    
    InitSDCardFS();
    AutoEmuNandBase(true);
    InitNandCrypto(entrypoint != ENTRY_B9S);
    InitExtFS();
    
    // custom font handling
    if (CheckSupportFile("font.pbm")) {
        u8* pbm = (u8*) malloc(0x10000); // arbitrary, should be enough by far
        if (pbm) {
            u32 pbm_size = LoadSupportFile("font.pbm", pbm, 0x10000);
            if (pbm_size) SetFontFromPbm(pbm, pbm_size);
            free(pbm);
        }
    }
    
    // check for embedded essential backup
    if (((entrypoint == ENTRY_NANDBOOT) || (entrypoint == ENTRY_B9S)) &&
        !PathExist("S:/essential.exefs") && CheckGenuineNandNcsd() &&
        ShowPrompt(true, "Essential files backup not found.\nCreate one now?")) {
        if (EmbedEssentialBackup("S:/nand.bin") == 0) {
            u32 flags = BUILD_PATH | SKIP_ALL;
            PathCopy(OUTPUT_PATH, "S:/essential.exefs", &flags);
            ShowPrompt(false, "Backup embedded in SysNAND\nand written to " OUTPUT_PATH ".");
        }
    }
    
    // check internal clock
    if (IS_SIGHAX) { // we could actually do this on any entrypoint
        DsTime dstime;
        get_dstime(&dstime);
        if ((DSTIMEGET(&dstime, bcd_Y) < 18) &&
             ShowPrompt(true, "RTC date&time seems to be\nwrong. Set it now?") &&
             ShowRtcSetterPrompt(&dstime, "Set RTC date&time:")) {
            char timestr[32];
            set_dstime(&dstime);
            GetTimeString(timestr, true, true);
            ShowPrompt(false, "New RTC date&time is:\n%s\n \nHint: HOMEMENU time needs\nmanual adjustment after\nsetting the RTC.", timestr);
        }
    }
    
    // check aeskeydb.bin / key state
    if ((entrypoint != ENTRY_B9S) && (CheckRecommendedKeyDb(NULL) != 0)) {
        ShowPrompt(false, "WARNING:\nNot running from a boot9strap\ncompatible entrypoint. Not\neverything may work as expected.\n \nProvide the recommended\naeskeydb.bin file to make this\nwarning go away.");
    }
    
    #if defined(SALTMODE)
    show_splash = bootmenu = (bootloader && CheckButton(BOOTMENU_KEY));
    if (show_splash) SplashInit("saltmode");
    #else // standard behaviour
    bootmenu = bootmenu || (bootloader && CheckButton(BOOTMENU_KEY)); // second check for boot menu keys
    #endif
    while (CheckButton(BOOTPAUSE_KEY)); // don't continue while these keys is held
    if (show_splash) while (timer_msec( timer ) < 500); // show splash for at least 0.5 sec
    
    // bootmenu handler
    if (bootmenu) {
        bootloader = false;
        while (HID_STATE); // wait until no buttons are pressed
        while (!bootloader && !godmode9) {
            const char* optionstr[6] = { "Resume GodMode9", "Resume bootloader", "Select payload...", "Select script...",
                "Poweroff system", "Reboot system" };
            int user_select = ShowSelectPrompt(6, optionstr, FLAVOR " bootloader menu.\nSelect action:");
            char loadpath[256];
            if (user_select == 1) {
                godmode9 = true;
            } else if (user_select == 2) {
                bootloader = true;
            } else if ((user_select == 3) && (FileSelectorSupport(loadpath, "Bootloader payloads menu.\nSelect payload:", PAYLOADS_DIR, "*.firm"))) {
                BootFirmHandler(loadpath, false, false);
            } else if ((user_select == 4) && (FileSelectorSupport(loadpath, "Bootloader scripts menu.\nSelect script:", SCRIPTS_DIR, "*.gm9"))) {
                ExecuteGM9Script(loadpath);
            } else if (user_select == 5) {
                exit_mode = GODMODE_EXIT_POWEROFF;
            } else if (user_select == 6) {
                exit_mode = GODMODE_EXIT_REBOOT;
            } else if (user_select) continue;
            break;
        }
    }
    
    // bootloader handler
    if (bootloader) {
        const char* bootfirm_paths[] = { BOOTFIRM_PATHS };
        if (IsBootableFirm(firm_in_mem, FIRM_MAX_SIZE)) BootFirm(firm_in_mem, "sdmc:/bootonce.firm");
        for (u32 i = 0; i < sizeof(bootfirm_paths) / sizeof(char*); i++) {
            BootFirmHandler(bootfirm_paths[i], false, (BOOTFIRM_TEMPS >> i) & 0x1);
        }
        ShowPrompt(false, "No bootable FIRM found.\nNow resuming GodMode9...");
        godmode9 = true;
    }
    
    if (godmode9) {
        current_dir = (DirStruct*) malloc(sizeof(DirStruct));
        clipboard = (DirStruct*) malloc(sizeof(DirStruct));
        panedata = (PaneData*) malloc(N_PANES * sizeof(PaneData));
        if (!current_dir || !clipboard || !panedata) {
            ShowPrompt(false, "Out of memory."); // just to be safe
            return exit_mode;
        }
        
        GetDirContents(current_dir, "");
        clipboard->n_entries = 0;
        memset(panedata, 0x00, N_PANES * sizeof(PaneData));
        ClearScreenF(true, true, COLOR_STD_BG); // clear splash
    }
    
    PaneData* pane = panedata;
    while (godmode9) { // this is the main loop
        // basic sanity checking
        if (!current_dir->n_entries) { // current dir is empty -> revert to root
            ShowPrompt(false, "Invalid directory object");
            *current_path = '\0';
            DeinitExtFS(); // deinit and...
            InitExtFS(); // reinitialize extended file system
            GetDirContents(current_dir, current_path);
            cursor = 0;
            if (!current_dir->n_entries) { // should not happen, if it does fail gracefully
                ShowPrompt(false, "Invalid root directory.");
                return exit_mode;
            }
        }
        if (cursor >= current_dir->n_entries) // cursor beyond allowed range
            cursor = current_dir->n_entries - 1;
        
        int curr_drvtype = DriveType(current_path);
        DirEntry* curr_entry = &(current_dir->entry[cursor]);
        if ((mark_next >= 0) && (curr_entry->type != T_DOTDOT)) {
            curr_entry->marked = mark_next;
            mark_next = -2;
        }
        DrawDirContents(current_dir, cursor, &scroll);
        DrawUserInterface(current_path, curr_entry, N_PANES ? pane - panedata + 1 : 0);
        DrawTopBar(current_path);
        
        // check write permissions
        if (~last_write_perm & GetWritePermissions()) {
            if (ShowPrompt(true, "Write permissions were changed.\nRelock them?")) SetWritePermissions(last_write_perm, false);
            last_write_perm = GetWritePermissions();
            continue;
        }
        
        // handle user input
        u32 pad_state = InputWait(3);
        bool switched = (pad_state & BUTTON_R1);
        
        // basic navigation commands
        if ((pad_state & BUTTON_A) && (curr_entry->type != T_FILE) && (curr_entry->type != T_DOTDOT)) { // for dirs
            if (switched && !(DriveType(curr_entry->path) & DRV_SEARCH)) { // search directory
                const char* optionstr[8] = { NULL };
                char tpath[16] = { 0 };
                if (!*current_path) snprintf(tpath, 15, "%s/title", curr_entry->path);
                int n_opt = 0;
                int srch_t = ((strncmp(curr_entry->path + 1, ":/title", 7) == 0) ||
                    (*tpath && PathExist(tpath))) ? ++n_opt : -1;
                int srch_f = ++n_opt;
                int fixcmac = (!*current_path && (strspn(curr_entry->path, "14AB") == 1)) ? ++n_opt : -1;
                int dirnfo = ++n_opt;
                int stdcpy = (*current_path && strncmp(current_path, OUTPUT_PATH, 256) != 0) ? ++n_opt : -1;
                if (srch_t > 0) optionstr[srch_t-1] = "Search for titles";
                if (srch_f > 0) optionstr[srch_f-1] = "Search for files...";
                if (fixcmac > 0) optionstr[fixcmac-1] = "Fix CMACs for drive";
                if (dirnfo > 0) optionstr[dirnfo-1] = (*current_path) ? "Show directory info" : "Show drive info";
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
                } else if (user_select == fixcmac) {
                    RecursiveFixFileCmac(curr_entry->path);
                    ShowPrompt(false, "Fix CMACs for drive finished.");
                } else if (user_select == dirnfo) {
                    bool is_drive = (!*current_path);
                    FILINFO fno;
                    u64 tsize = 0;
                    u32 tdirs = 0;
                    u32 tfiles = 0;
                    
                    ShowString("Analyzing %s, please wait...", is_drive ? "drive" : "dir");
                    if ((is_drive || (fvx_stat(curr_entry->path, &fno) == FR_OK)) &&
                        DirInfo(curr_entry->path, &tsize, &tdirs, &tfiles)) {
                        char bytestr[32];
                        FormatBytes(bytestr, tsize);
                        if (is_drive) {
                            char freestr[32];
                            char drvsstr[32];
                            char usedstr[32];
                            FormatBytes(freestr, GetFreeSpace(curr_entry->path));
                            FormatBytes(drvsstr, GetTotalSpace(curr_entry->path));
                            FormatBytes(usedstr, GetTotalSpace(curr_entry->path) - GetFreeSpace(curr_entry->path));
                            ShowPrompt(false, "%s\n \n%lu files & %lu subdirs\n%s total size\n \nspace free: %s\nspace used: %s\nspace total: %s",
                                namestr, tfiles, tdirs, bytestr, freestr, usedstr, drvsstr);
                        } else {
                            ShowPrompt(false, "%s\n \ncreated: %04lu-%02lu-%02lu %02lu:%02lu:%02lu\n%lu files & %lu subdirs\n%s total size\n \n[%c] read-only [%c] hidden\n[%c] system    [%c] archive\n[%c] virtual",
                                namestr,
                                1980 + ((fno.fdate >> 9) & 0x7F), (fno.fdate >> 5) & 0x0F, (fno.fdate >> 0) & 0x1F,
                                (fno.ftime >> 11) & 0x1F, (fno.ftime >> 5) & 0x3F, ((fno.ftime >> 0) & 0x1F) << 1,
                                tfiles, tdirs, bytestr,
                                (fno.fattrib & AM_RDO) ? 'X' : ' ', (fno.fattrib & AM_HID) ? 'X' : ' ', (fno.fattrib & AM_SYS) ? 'X' : ' ' ,
                                (fno.fattrib & AM_ARC) ? 'X' : ' ', (fno.fattrib & AM_VRT) ? 'X' : ' ');
                        }
                    } else ShowPrompt(false, "Analyze %s: failed!", is_drive ? "drive" : "dir");
                } else if (user_select == stdcpy) {
                    StandardCopy(&cursor, &scroll);
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
            FileHandlerMenu(current_path, &cursor, &scroll, &pane); // processed externally
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
            if (!CheckSDMountState()) {
                while (!InitSDCardFS() &&
                    ShowPrompt(true, "Initialising SD card failed! Retry?"));
            } else {
                DeinitSDCardFS();
                if (clipboard->n_entries && !PathExist(clipboard->entry[0].path))
                    clipboard->n_entries = 0; // remove SD clipboard entries
            }
            ClearScreenF(true, true, COLOR_STD_BG);
            AutoEmuNandBase(true);
            InitExtFS();
            GetDirContents(current_dir, current_path);
            if (cursor >= current_dir->n_entries) cursor = 0;
        } else if (!switched && (pad_state & BUTTON_DOWN) && (cursor + 1 < current_dir->n_entries))  { // cursor down
            if (pad_state & BUTTON_L1) mark_next = curr_entry->marked;
            cursor++;
        } else if (!switched && (pad_state & BUTTON_UP) && cursor) { // cursor up
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
        } else if (switched && (pad_state & BUTTON_DOWN)) { // force reload file list
            GetDirContents(current_dir, current_path);
            ClearScreenF(true, true, COLOR_STD_BG);
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
            // this is handled in hid.h
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
            } else if (pad_state & BUTTON_Y) { // paste files
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
            } else if (pad_state & BUTTON_Y) { // create an entry
                const char* optionstr[] = { "Create a folder", "Create a dummy file" };
                u32 type = ShowSelectPrompt(2, optionstr, "Create a new entry here?\nSelect type.");
                if (type) {
                    const char* typestr = (type == 1) ? "folder" : (type == 2) ? "file" : NULL;
                    char ename[256];
                    u64 fsize = 0;
                    snprintf(ename, 255, (type == 1) ? "newdir" : "dummy.bin");
                    if ((ShowStringPrompt(ename, 256, "Create a new %s here?\nEnter name below.", typestr)) &&
                        ((type != 2) || ((fsize = ShowNumberPrompt(0, "Create a new %s here?\nEnter file size below.", typestr)) != (u64) -1))) {
                        if (((type == 1) && !DirCreate(current_path, ename)) ||
                            ((type == 2) && !FileCreateDummy(current_path, ename, fsize))) {
                            char namestr[36+1];
                            TruncateString(namestr, ename, 36, 12);
                            ShowPrompt(false, "Failed creating %s:\n%s", typestr, namestr);
                        } else {
                            GetDirContents(current_dir, current_path);
                            for (cursor = (current_dir->n_entries) ? current_dir->n_entries - 1 : 0;
                                (cursor > 1) && (strncmp(current_dir->entry[cursor].name, ename, 256) != 0); cursor--);
                        }
                    }
                }
            }
        }
        
        if (pad_state & BUTTON_START) {
            exit_mode = (switched || (pad_state & BUTTON_LEFT)) ? GODMODE_EXIT_POWEROFF : GODMODE_EXIT_REBOOT;
            break;
        } else if (pad_state & (BUTTON_HOME|BUTTON_POWER)) { // Home menu
            const char* optionstr[8];
            const char* buttonstr = (pad_state & BUTTON_HOME) ? "HOME" : "POWER";
            u32 n_opt = 0;
            int poweroff = ++n_opt;
            int reboot = ++n_opt;
            int scripts = ++n_opt;
            int payloads = ++n_opt;
            int more = ++n_opt;
            if (poweroff > 0) optionstr[poweroff - 1] = "Poweroff system";
            if (reboot > 0) optionstr[reboot - 1] = "Reboot system";
            if (scripts > 0) optionstr[scripts - 1] = "Scripts...";
            if (payloads > 0) optionstr[payloads - 1] = "Payloads...";
            if (more > 0) optionstr[more - 1] = "More...";
            
            int user_select = 0;
            while ((user_select = ShowSelectPrompt(n_opt, optionstr, "%s button pressed.\nSelect action:", buttonstr)) &&
                (user_select != poweroff) && (user_select != reboot)) {
                char loadpath[256];
                if ((user_select == more) && (HomeMoreMenu(current_path) == 0)) break; // more... menu
                else if (user_select == scripts) {
                    if (!CheckSupportDir(SCRIPTS_DIR)) {
                        ShowPrompt(false, "Scripts directory not found.\n(default path: 0:/gm9/" SCRIPTS_DIR ")");
                    } else if (FileSelectorSupport(loadpath, "HOME scripts... menu.\nSelect script:", SCRIPTS_DIR, "*.gm9")) {
                        ExecuteGM9Script(loadpath);
                        GetDirContents(current_dir, current_path);
                        ClearScreenF(true, true, COLOR_STD_BG);
                        break;
                    }
                } else if (user_select == payloads) {
                    if (!CheckSupportDir(PAYLOADS_DIR)) ShowPrompt(false, "Payloads directory not found.\n(default path: 0:/gm9/" PAYLOADS_DIR ")");
                    else if (FileSelectorSupport(loadpath, "HOME payloads... menu.\nSelect payload:", PAYLOADS_DIR, "*.firm"))
                        BootFirmHandler(loadpath, false, false);
                }
            }
            
            if (user_select == poweroff) { 
                exit_mode = GODMODE_EXIT_POWEROFF;
                break;
            } else if (user_select == reboot) { 
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
    
    if (current_dir) free(current_dir);
    if (clipboard) free(clipboard);
    if (panedata) free(panedata);
    
    return exit_mode;
}

#else
u32 ScriptRunner(int entrypoint) {
    // init font and show splash
    if (!SetFontFromPbm(NULL, 0)) return GODMODE_EXIT_POWEROFF;
    SplashInit("scriptrunner mode");
    u64 timer = timer_start();
    
    InitSDCardFS();
    AutoEmuNandBase(true);
    InitNandCrypto(entrypoint != ENTRY_B9S);
    InitExtFS();
    
    while (HID_STATE); // wait until no buttons are pressed
    while (timer_msec( timer ) < 500); // show splash for at least 0.5 sec
    
    if (PathExist("V:/" VRAM0_AUTORUN_GM9)) {
        ClearScreenF(true, true, COLOR_STD_BG); // clear splash
        ExecuteGM9Script("V:/" VRAM0_AUTORUN_GM9);
    } else if (PathExist("V:/" VRAM0_SCRIPTS)) {
        char loadpath[256];
        if (FileSelector(loadpath, FLAVOR " scripts menu.\nSelect script:", "V:/" VRAM0_SCRIPTS, "*.gm9", HIDE_EXT))
            ExecuteGM9Script(loadpath);
    } else ShowPrompt(false, "Compiled as script autorunner\nbut no script provided.\n \nDerp!");
    
    // deinit
    DeinitExtFS();
    DeinitSDCardFS();
    
    return GODMODE_EXIT_REBOOT;
}
#endif
