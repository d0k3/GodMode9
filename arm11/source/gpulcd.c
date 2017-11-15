#include <vram.h>
#include <gpulcd.h>
#include <types.h>

void LCD_SetBrightness(u32 screen, u8 brightness)
{
    vu32 *lcd_reg;
    if (screen & 1) {
        lcd_reg = LCD_CFG(0xA40);
    } else {
        lcd_reg = LCD_CFG(0x240);
    }
    *lcd_reg = brightness;
    return;
}

void LCD_Initialize(u8 brightness)
{
    *LCD_CFG(0x014) = 0x00000001;
    *LCD_CFG(0x00C) &= 0xFFFEFFFE;
    *LCD_CFG(0x240) = brightness;
    *LCD_CFG(0xA40) = brightness;
    *LCD_CFG(0x244) = 0x1023E;
    *LCD_CFG(0xA44) = 0x1023E;
    return;
}

void LCD_Deinitialize(void)
{
    *LCD_CFG(0x244) = 0;
    *LCD_CFG(0xA44) = 0;
    *LCD_CFG(0x00C) = 0;
    *LCD_CFG(0x014) = 0;
    return;
}

void GPU_PSCFill(u32 start, u32 end, u32 fv)
{
    u32 mp;
    if (start > end)
        return;

    start = GPU_ADDR(start);
    end   = GPU_ADDR(end);
    mp    = (start+end)/2;

    *GPU_PSC0(PSC_SADDR) = start;
    *GPU_PSC0(PSC_EADDR) = mp;
    *GPU_PSC0(PSC_FILL)  = fv;

    *GPU_PSC1(PSC_SADDR) = mp;
    *GPU_PSC1(PSC_EADDR) = end;
    *GPU_PSC1(PSC_FILL)  = fv;

    *GPU_PSC0(PSC_CNT)   = PSC_START | PSC_32BIT;
    *GPU_PSC1(PSC_CNT)   = PSC_START | PSC_32BIT;

    while(!((*GPU_PSC0(PSC_CNT) & PSC_DONE) && (*GPU_PSC1(PSC_CNT) & PSC_DONE)));
    return;
}

void GPU_SetFramebuffers(const u32 *framebuffers)
{
    *GPU_PDC0(0x68) = framebuffers[0];
    *GPU_PDC0(0x6C) = framebuffers[1];
    *GPU_PDC0(0x94) = framebuffers[2];
    *GPU_PDC0(0x98) = framebuffers[3];
    *GPU_PDC1(0x68) = framebuffers[4];
    *GPU_PDC1(0x6C) = framebuffers[5];
    *GPU_PDC0(0x78) = 0;
    *GPU_PDC1(0x78) = 0;
    return;
}

void GPU_SetFramebufferMode(u32 screen, u8 mode)
{
    u32 stride, cfg;
    vu32 *fbcfg_reg, *fbstr_reg;

    mode &= 7;
    screen &= 1;
    cfg = PDC_FIXSTRIP | mode;
    if (screen) {
        fbcfg_reg = GPU_PDC1(0x70);
        fbstr_reg = GPU_PDC1(0x90);
    } else {
        fbcfg_reg = GPU_PDC0(0x70);
        fbstr_reg = GPU_PDC0(0x90);
        cfg |= PDC_MAINSCREEN;
    }

    switch(mode) {
    case PDC_RGBA8:
        stride = 960;
        break;
    case PDC_RGB24:
        stride = 720;
        break;
    default:
        stride = 480;
        break;
    }

    *fbcfg_reg = cfg;
    *fbstr_reg = stride;
    return;
}

void GPU_Init(void)
{
    LCD_Initialize(0x20);

    if (*GPU_CNT != 0x1007F) {
        *GPU_CNT = 0x1007F;
        *GPU_PDC0(0x00) = 0x000001C2;
        *GPU_PDC0(0x04) = 0x000000D1;
        *GPU_PDC0(0x08) = 0x000001C1;
        *GPU_PDC0(0x0C) = 0x000001C1;
        *GPU_PDC0(0x10) = 0x00000000;
        *GPU_PDC0(0x14) = 0x000000CF;
        *GPU_PDC0(0x18) = 0x000000D1;
        *GPU_PDC0(0x1C) = 0x01C501C1;
        *GPU_PDC0(0x20) = 0x00010000;
        *GPU_PDC0(0x24) = 0x0000019D;
        *GPU_PDC0(0x28) = 0x00000002;
        *GPU_PDC0(0x2C) = 0x00000192;
        *GPU_PDC0(0x30) = 0x00000192;
        *GPU_PDC0(0x34) = 0x00000192;
        *GPU_PDC0(0x38) = 0x00000001;
        *GPU_PDC0(0x3C) = 0x00000002;
        *GPU_PDC0(0x40) = 0x01960192;
        *GPU_PDC0(0x44) = 0x00000000;
        *GPU_PDC0(0x48) = 0x00000000;
        *GPU_PDC0(0x5C) = 0x00F00190;
        *GPU_PDC0(0x60) = 0x01C100D1;
        *GPU_PDC0(0x64) = 0x01920002;
        *GPU_PDC0(0x68) = VRAM_START;
        *GPU_PDC0(0x6C) = VRAM_START;
        *GPU_PDC0(0x70) = 0x00080340;
        *GPU_PDC0(0x74) = 0x00010501;
        *GPU_PDC0(0x78) = 0x00000000;
        *GPU_PDC0(0x90) = 0x000003C0;
        *GPU_PDC0(0x94) = VRAM_START;
        *GPU_PDC0(0x98) = VRAM_START;
        *GPU_PDC0(0x9C) = 0x00000000;

        for (u32 i=0; i<256; i++)
            *GPU_PDC0(0x84) = 0x10101 * i;

        *GPU_PDC1(0x00) = 0x000001C2;
        *GPU_PDC1(0x04) = 0x000000D1;
        *GPU_PDC1(0x08) = 0x000001C1;
        *GPU_PDC1(0x0C) = 0x000001C1;
        *GPU_PDC1(0x10) = 0x000000CD;
        *GPU_PDC1(0x14) = 0x000000CF;
        *GPU_PDC1(0x18) = 0x000000D1;
        *GPU_PDC1(0x1C) = 0x01C501C1;
        *GPU_PDC1(0x20) = 0x00010000;
        *GPU_PDC1(0x24) = 0x0000019D;
        *GPU_PDC1(0x28) = 0x00000052;
        *GPU_PDC1(0x2C) = 0x00000192;
        *GPU_PDC1(0x30) = 0x00000192;
        *GPU_PDC1(0x34) = 0x0000004F;
        *GPU_PDC1(0x38) = 0x00000050;
        *GPU_PDC1(0x3C) = 0x00000052;
        *GPU_PDC1(0x40) = 0x01980194;
        *GPU_PDC1(0x44) = 0x00000000;
        *GPU_PDC1(0x48) = 0x00000011;
        *GPU_PDC1(0x5C) = 0x00F00140;
        *GPU_PDC1(0x60) = 0x01C100d1;
        *GPU_PDC1(0x64) = 0x01920052;
        *GPU_PDC1(0x68) = VRAM_START;
        *GPU_PDC1(0x6C) = VRAM_START;
        *GPU_PDC1(0x70) = 0x00080300;
        *GPU_PDC1(0x74) = 0x00010501;
        *GPU_PDC1(0x78) = 0x00000000;
        *GPU_PDC1(0x90) = 0x000003C0;
        *GPU_PDC1(0x9C) = 0x00000000;

        for (u32 i=0; i<256; i++)
            *GPU_PDC1(0x84) = 0x10101 * i;
    }
    return;
}
