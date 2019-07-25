#include "spi_test.h"
#include "timer.h"
#include "ui.h"
#include "spi.h"

void SPITestWaitWriteEnd(CardType type) {
    while (SPIWaitWriteEnd(type));
}

int SPITestEraseSector(CardType type, u32 offset, u8 actualCmd) {
    u8 cmd[4] = { actualCmd, (u8)(offset >> 16), (u8)(offset >> 8), (u8) offset };
    if(type == NO_CHIP || type == FLASH_8MB) return 0xC8E13404;
    
    int res = SPIWaitWriteEnd(type);
    
    if( (res = SPIEnableWriting(type)) ) return res;
    if( (res = SPIWriteRead(type, cmd, 4, NULL, 0, NULL, 0)) ) return res;
    SPITestWaitWriteEnd(type);
    return 0;
}

u64 SPITestEraseAll(CardType type, u32 size, u8 actualCmd, u32 eraseSize) {
    u32 offset;
    u64 timer = timer_start();
    for(offset = 0; offset < size; offset += eraseSize) {
        SPITestEraseSector(type, offset, actualCmd);
    }
    return timer_ticks(timer);
}

u64 SPITestWriteAll(CardType type, u32 size, u8 actualCmd, u32 pageSize, u8 *buf) {
    memset(buf, 0, size);
    u8 cmd[4] = { actualCmd, 0, 0, 0 };
    u32 offset = 0;
    u64 timer = timer_start();
    for(offset = 0; offset < size; offset += pageSize) {
        cmd[1] = (u8)(offset >> 16);
        cmd[2] = (u8)(offset >> 8);
        cmd[3] = (u8) offset;
        SPIEnableWriting(type);
        SPIWriteRead(type, cmd, 4, NULL, 0, buf, pageSize);
        SPIWaitWriteEnd(type);
    }
    return timer_ticks(timer);
}

int SPITestBytes(CardType t, u32 id, u8 *buf, u8 byte, const char *word, u32 size) {
    u32 offset;
    SPIReadSaveData(t, 0, buf, size);
    for(offset = 0; offset < size; offset++) {
        if(buf[offset] != byte) {
            if(word && !ShowPrompt(true, "ID: 0x%06lX\n1: Could not %s\n*0x%06lX = 0x%02hhX", id, word, offset, buf[offset])) return -1;
            return 1;
        }
    }
    return 0;
}

void SPIFlashTest(void) {
    CardType t = ShowPrompt(true, "Does the cart have IR?") ? FLASH_512KB_INFRARED : FLASH_256KB_1;
    u32 size;
    u32 pageSize;
    u32 jedecid;
    if(SPIReadJEDECIDAndStatusReg(t, &jedecid, NULL)) return;
    if(!ShowPrompt(true, "ID: 0x%06lX\nDo flash test on this cart?\nThis will overwrite it completely\nseveral times!", jedecid)) return;
    size = ShowHexPrompt(0, 8, "Memory size?");
    u8 *buf = calloc(size, 1);
    if (buf == NULL) {
        ShowPrompt(false, "Malloc failed!");
        return;
    }
    pageSize = ShowHexPrompt(0, 8, "Page size?");
    // method 1:
    u64 time1 = SPITestWriteAll(t, size, 0x0A, pageSize, buf);
    int result1 = SPITestBytes(t, jedecid, buf, 0x00, "write", size);
    if(result1 < 0) {
        free(buf);
        return;
    }
    ShowPrompt(false, "ID: 0x%06lX\nWriting all took %llu\nError: %d", jedecid, time1, result1);
    // method 2: Erasing
    u8 eraseCommand;
    u32 eraseSize;
    for(eraseCommand = 0xC0; eraseCommand < 0xE0; eraseCommand++) {
        ShowString("Testing %hhX", eraseCommand);
        SPITestEraseSector(t, 0, eraseCommand);
        SPIReadSaveData(t, 0, buf, size);
        for(eraseSize = 0; eraseSize < size && buf[eraseSize] == 0xff; eraseSize++);
        if(eraseSize) {
            SPITestWriteAll(t, eraseSize, 0x02, pageSize, buf);
            if(SPITestBytes(t, jedecid, buf, 0x00, "reset", size) < 0) {
                free(buf);
                return;
            }

            if(ShowPrompt(true, "ID: 0x%06lX\n0x%02hhX erased %lu bytes\nTest?", jedecid, eraseCommand, eraseSize)) {
                u64 eraseTime = SPITestEraseAll(t, size, eraseCommand, eraseSize);
                int eraseResult = SPITestBytes(t, jedecid, buf, 0xFF, "erase", size);
                if(eraseResult < 0)  {
                    free(buf);
                    return;
                }
                u64 writeTime = SPITestWriteAll(t, size, 0x02, pageSize, buf);
                int writeResult = SPITestBytes(t, jedecid, buf, 0x00, "write", size);
                if(eraseResult < 0)  {
                    free(buf);
                    return;
                }
                ShowPrompt(false, "ID: 0x%06lX\n0x%02hhX erases %lu bytes\nErase: %llu (%d)\nWrite: %llu (%d)\nTotal: %llu", jedecid, eraseCommand, eraseSize, eraseTime, eraseResult, writeTime, writeResult, eraseTime + writeTime);
            }
        }
    }
    free(buf);

}

