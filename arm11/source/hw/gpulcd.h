#pragma once
#include <types.h>

#define VBLANK_INTERRUPT	(0x2A)

void LCD_SetBrightness(u8 brightness);
void LCD_Deinitialize(void);

void GPU_PSCFill(u32 start, u32 end, u32 fv);

enum {
	PDC_RGBA8 = 0,
	PDC_RGB24 = 1,
	PDC_RGB565 = 2,
	PDC_RGB5A1 = 3,
	PDC_RGBA4 = 4,
};

void GPU_SetFramebufferMode(u32 screen, u8 mode);
void GPU_SetFramebuffers(const u32 *framebuffers);
void GPU_Init(void);
