#pragma once
#include <types.h>

#define GPU_ADDR(x)  ((x)>>3)
#define GPU_BOX(w,h) ((w) | (h)<<16)

#define GPU_CNT     ((vu32*)(0x10141200))


#define LCD_CFG(x)  ((vu32*)(0x10202000 + (x)))
void LCD_SetBrightness(u32 screen, u8 brightness);
void LCD_Deinitialize(void);

#define GPU_PSC0(x) ((vu32*)(0x10400010 + (x)))
#define GPU_PSC1(x) ((vu32*)(0x10400020 + (x)))

#define PSC_SADDR   (0x00)
#define PSC_EADDR   (0x04)
#define PSC_FILL    (0x08)
#define PSC_CNT     (0x0C)

#define PSC_START   (BIT(0))
#define PSC_DONE    (BIT(1))
#define PSC_32BIT   (2<<8)
#define PSC_24BIT   (1<<8)
#define PSC_16BIT   (0<<8)

void GPU_PSCWait(void);
void GPU_PSCFill(u32 start, u32 end, u32 fv);


#define GPU_PDC0(x) ((vu32*)(0x10400400 + (x)))
#define GPU_PDC1(x) ((vu32*)(0x10400500 + (x)))

#define PDC_RGBA8   (0<<0)
#define PDC_RGB24   (1<<0)
#define PDC_RGB565  (2<<0)
#define PDC_RGB5A1  (3<<0)
#define PDC_RGBA4   (4<<0)

#define PDC_PARALLAX   (BIT(5))
#define PDC_MAINSCREEN (BIT(6))
#define PDC_FIXSTRIP   (BIT(7))

void GPU_SetFramebuffers(const u32 *framebuffers);
void GPU_SetFramebufferMode(u32 screen, u8 mode);
void GPU_Init();
