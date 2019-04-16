#pragma once
#include <types.h>

void LCD_SetBrightness(u8 brightness);
void LCD_Deinitialize(void);

void GPU_PSCFill(u32 start, u32 end, u32 fv);

#define PDC_RGBA8   (0<<0)
#define PDC_RGB24   (1<<0)
#define PDC_RGB565  (2<<0)
#define PDC_RGB5A1  (3<<0)
#define PDC_RGBA4   (4<<0)
void GPU_SetFramebufferMode(u32 screen, u8 mode);
void GPU_SetFramebuffers(const u32 *framebuffers);
void GPU_Init(void);
