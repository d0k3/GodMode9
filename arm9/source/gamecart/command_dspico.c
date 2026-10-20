// DSpico SD protocol based on the open DSpico firmware and DLDI driver
// (LNH-team/dspico-firmware, LNH-team/dspico-dldi), zlib license.
#include "command_dspico.h"
#include "protocol.h"
#include "protocol_ntr.h"
#include "timer.h"

#define DSPICO_LEN_0    NTRCARD_PAGESIZE_0
#define DSPICO_LEN_4    NTRCARD_PAGESIZE_4
#define DSPICO_LEN_512  NTRCARD_PAGESIZE_512

#define DSPICO_PROBE_TICKS ((u64)(TICKS_PER_SEC * 2))
#define DSPICO_SETTLE_MS    500 // GodMode9i waits ~30 frames after card init

static u64 sd_size = 0;
static bool sd_dirty = false; // not in ROM mode, needs InitCartRead
static bool sd_ready = false;
static bool probing = false;
static u64 probe_begin;

// Normal transfers retain the DLDI driver's waits; initialization is bounded.
static bool DSpico_CartPresent(void) {
    return !(REG_CARDSTATUS & 0x1);
}

static bool DSpico_CanWait(void) {
    return DSpico_CartPresent() &&
        (!probing || (timer_ticks(probe_begin) < DSPICO_PROBE_TICKS));
}

static void DSpico_SetCmd(u64 cmd) {
    for (u32 i = 0; i < 8; i++)
        REG_NTRCARDCMD[i] = (u8) (cmd >> (56 - (8 * i)));
}

static u32 DSpico_Flags(u32 len, u32 latency2, u32 latency1, bool data_descramble) {
    return NTRCARD_ACTIVATE | NTRCARD_nRESET | len | NTRCARD_SEC_CMD |
        NTRCARD_DELAY2(latency2) | NTRCARD_SEC_EN | NTRCARD_DELAY1(latency1) |
        (data_descramble ? NTRCARD_SEC_DAT : 0);
}

static bool DSpico_WaitBusy(void) {
    while (REG_NTRCARDROMCNT & NTRCARD_BUSY)
        if (!DSpico_CanWait()) return false;
    return true;
}

// polled transfer, words are read from or written to the data FIFO
static bool DSpico_Transfer(u64 cmd, u32 flags, void* buffer, u32 words, bool write) {
    u8* data = (u8*) buffer;
    u32* data32 = (u32*) buffer;
    bool aligned = !(((u32) buffer) & 3);
    u32 done = 0;
    REG_NTRCARDMCNT = NTRCARD_CR1_ENABLE;
    DSpico_SetCmd(cmd);
    REG_NTRCARDROMCNT = flags | (write ? NTRCARD_WR : 0);
    u32 ctrl;
    do {
        if (probing && !DSpico_CanWait()) return false;
        ctrl = REG_NTRCARDROMCNT;
        if (ctrl & NTRCARD_DATA_READY) {
            if (write) {
                u32 word = 0;
                if (done < words) {
                    if (aligned) word = data32[done];
                    else memcpy(&word, data + (done * 4), 4);
                }
                REG_NTRCARDFIFO = word;
            } else {
                u32 word = REG_NTRCARDFIFO;
                if (done < words) {
                    if (aligned) data32[done] = word;
                    else memcpy(data + (done * 4), &word, 4);
                }
            }
            done++;
        } else if (!DSpico_CartPresent()) return false;
    } while (ctrl & NTRCARD_BUSY);
    return done >= words;
}

static bool DSpico_RequestRead(u32 sector) {
    return DSpico_Transfer(0xE300000000000000ull | sector,
        DSpico_Flags(DSPICO_LEN_0, 0, 0, true), NULL, 0, false);
}

static bool DSpico_PollReady(void) {
    u32 result = 0;
    while (true) {
        if (probing && !DSpico_CanWait()) return false;
        if (!DSpico_Transfer(0xE400000000000000ull,
            DSpico_Flags(DSPICO_LEN_4, 4, 0, true), &result, 1, false)) return false;
        if (result == 1) return true;
        if (!DSpico_CartPresent()) return false;
    }
}

static bool DSpico_GetData(void* buffer) {
    return DSpico_Transfer(0xE500000000000000ull,
        DSpico_Flags(DSPICO_LEN_512, 4, 0, false), buffer, 128, false);
}

static bool DSpico_SendWrite(const void* buffer, u32 sector, bool first, bool last) {
    u64 cmd = 0xF6E10D9800000000ull | ((first ? 1ull : 0ull) << 33) |
        ((last ? 1ull : 0ull) << 32) | sector;
    return DSpico_Transfer(cmd, DSpico_Flags(DSPICO_LEN_512, 8, 0, true),
        (void*) buffer, 128, true);
}

bool DSpico_CheckGameCode(const u8* header) {
    return (memcmp(header + 0x0C, "DSPI", 4) == 0) || (memcmp(header + 0x0C, "NTRJ", 4) == 0);
}

// the DSpico has no command for the SD size yet, so it is derived from the partition table
// (see https://github.com/LNH-team/dspico-firmware/pull/7)
static u64 DSpico_DetectSize(const u8* sector0) {
    if ((sector0[0x1FE] != 0x55) || (sector0[0x1FF] != 0xAA)) return 0;
    u64 size = 0;
    // MBR partition table
    for (u32 i = 0; i < 4; i++) {
        const u8* p = sector0 + 0x1BE + (i * 16);
        if (((p[0] != 0x00) && (p[0] != 0x80)) || !p[4]) continue; // type must be set
        u64 start = getle32(p + 8);
        u64 count = getle32(p + 12);
        if (count && ((start + count) > size)) size = start + count;
    }
    if (size) return size * 0x200;
    // FAT boot sector without partition table
    u32 total = getle16(sector0 + 0x13);
    if (!total) total = getle32(sector0 + 0x20);
    return (u64) total * 0x200;
}

static bool DSpico_Probe(void) {
    // disable the KEY2 scrambler
    if (!DSpico_Transfer(0xFC00000000000000ull,
        DSpico_Flags(DSPICO_LEN_0, 0, 24, false), NULL, 0, false)) return false;
    if (!DSpico_WaitBusy()) return false;

    // zero seeds, the scrambler now passes data through unchanged
    REG_NTRCARDROMCNT = 0;
    REG_NTRCARDSEEDX_L = 0;
    REG_NTRCARDSEEDY_L = 0;
    REG_NTRCARDSEEDX_H = 0;
    REG_NTRCARDSEEDY_H = 0;
    REG_NTRCARDROMCNT = NTRCARD_nRESET | NTRCARD_SEC_SEED | NTRCARD_SEC_EN | NTRCARD_SEC_DAT;
    if (!DSpico_WaitBusy()) return false;

    // probe: a DSpico serves a valid MBR or FAT boot sector at sector 0
    u8 ALIGN(32) sector0[0x200];
    int probe_res = DSpico_ReadSectors(sector0, 0, 1);
    if (probe_res != 0) return false;
    sd_size = DSpico_DetectSize(sector0);
    if (!sd_size) return false;
    sd_ready = true;
    return true;
}

bool DSpico_Enter(void) {
    sd_size = 0;
    sd_ready = false;
    sd_dirty = true;

    wait_msec(DSPICO_SETTLE_MS);
    probe_begin = timer_start();
    probing = true;
    bool ready = DSpico_Probe();
    probing = false;
    return ready;
}

void DSpico_Reset(void) {
    sd_dirty = sd_ready = false;
}

bool DSpico_IsReady(void) {
    return sd_ready;
}

bool DSpico_IsDirty(void) {
    return sd_dirty;
}

u64 DSpico_GetSdSize(void) {
    return sd_size;
}

int DSpico_ReadSectors(void* buffer, u32 sector, u32 count) {
    u8* ptr = (u8*) buffer;
    if (!count) return 0;
    if (!DSpico_RequestRead(sector)) return -1;
    for (u32 i = 0; i < count; i++) {
        if (!DSpico_PollReady() || !DSpico_GetData(ptr + (i * 0x200))) return -1;
    }
    // return the card to its idle state
    return DSpico_PollReady() ? 0 : -1;
}

int DSpico_WriteSectors(const void* buffer, u32 sector, u32 count) {
    const u8* ptr = (const u8*) buffer;
    if (!count) return 0;
    if (count == 1) {
        if (!DSpico_SendWrite(ptr, sector, true, true)) return -1;
    } else {
        if (!DSpico_SendWrite(ptr, sector, true, false)) return -1;
        for (u32 i = 1; i < count - 1; i++) {
            if (!DSpico_SendWrite(ptr + (i * 0x200), sector + i, false, false)) return -1;
            if (!DSpico_PollReady()) return -1;
        }
        if (!DSpico_SendWrite(ptr + ((count - 1) * 0x200), sector + count - 1, false, true)) return -1;
        if (!DSpico_PollReady()) return -1;
    }
    return DSpico_PollReady() ? 0 : -1;
}
