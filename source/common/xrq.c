/*
 Written by Wolfvak, specially sublicensed under the GPLv2
 Read LICENSE for more details
*/

#include "common.h"
#include "fsinit.h"
#include "fsutil.h"
#include "ui.h"

#include <arm.h>

/* Code will be dumped from PC-PC_DUMPRAD to PC+PC_DUMPRAD */
#define PC_DUMPRAD (0x20)

#define XRQ_DUMPDATAFUNC(type, size) \
int XRQ_DumpData_##type(char *b, u32 s, u32 e) \
{ \
    char *c = b; \
    while(s<e) { \
        b+=sprintf(b, "%08lX: ",s); \
        type *dl = (type*)s; \
        for (u32 i=0; i<(16/((size)/2)); i++) { \
            b+=sprintf(b, "%0" #size "lX ", (u32)dl[i]); \
        } \
        b+=sprintf(b, "\n"); \
        s+=16; \
    } \
    return (int)(b-c); \
}
XRQ_DUMPDATAFUNC(u8,  2)
XRQ_DUMPDATAFUNC(u16, 4)
XRQ_DUMPDATAFUNC(u32, 8)


// Last 3 should never happen
const char *XRQ_Name[] = {
    "Reset", "Undefined", "SWI", "Prefetch Abort",
    "Data Abort", "", "", ""
};

extern char __stack_top;

void XRQ_DumpRegisters(u32 xrq, u32 *regs)
{
    int y;
    u32 sp, st, pc;
    char *wstr = (char*)TEMP_BUFFER, *dumpstr = wstr;

    ClearScreen(MAIN_SCREEN, COLOR_BLACK);
    wstr += sprintf(wstr, "Exception: %s (%lu)\n", XRQ_Name[xrq&7], xrq);

    for (int i = 0; i < 8; i++) {
        int i_ = i*2;
        wstr += sprintf(wstr,
        "R%02d: %08lX | R%02d: %08lX\n", i_, regs[i_], i_+1, regs[i_+1]);
    }

    wstr += sprintf(wstr, "CPSR: %08lX\n", regs[16]);

    DrawStringF(MAIN_SCREEN, 10, 0, COLOR_WHITE, COLOR_BLACK,
        dumpstr);

    y = GetDrawStringHeight(dumpstr);
    DrawStringF(MAIN_SCREEN, 10, y, COLOR_WHITE, COLOR_BLACK,
        "Reinitializing SD subsystem...");
    y+=FONT_HEIGHT_EXT;

    while(!InitSDCardFS()) {
        DeinitSDCardFS();
    }

    DrawStringF(MAIN_SCREEN, 10, y, COLOR_WHITE, COLOR_BLACK,
        "Done");
    y+=FONT_HEIGHT_EXT;

    sp = regs[13] & ~0xF;
    st = (u32)&__stack_top;
    if (sp >= 0x20000000 && sp <= st) {
        wstr += sprintf(wstr, "\nStack dump:\n\n");
        wstr += XRQ_DumpData_u8(wstr, sp, st);
    }

    pc = regs[15];
    wstr += sprintf(wstr, "\nCode dump:\n\n");
    if (regs[16] & SR_THUMB) {
        wstr += XRQ_DumpData_u16(wstr, pc-PC_DUMPRAD, pc+PC_DUMPRAD);
    } else {
        wstr += XRQ_DumpData_u32(wstr, pc-PC_DUMPRAD, pc+PC_DUMPRAD);
    }

    DrawStringF(MAIN_SCREEN, 10, y, COLOR_WHITE, COLOR_BLACK,
        "Dumping state to SD...");
    y+=FONT_HEIGHT_EXT;

    FileSetData(OUTPUT_PATH"/dump.txt", dumpstr, wstr - dumpstr, 0, true);
    DrawStringF(MAIN_SCREEN, 10, y, COLOR_WHITE, COLOR_BLACK,
        "Done");
    return;
}
