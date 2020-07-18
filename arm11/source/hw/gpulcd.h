/*
 *   This file is part of GodMode9
 *   Copyright (C) 2017-2019 Wolfvak
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#include <types.h>

#define VBLANK_INTERRUPT	(0x2A)

enum
{
	PDN_GPU_CNT_RST_REGS           = 1u,    // And more?
	PDN_GPU_CNT_RST_PSC            = 1u<<1, // ?
	PDN_GPU_CNT_RST_GEOSHADER      = 1u<<2, // ?
	PDN_GPU_CNT_RST_RASTERIZER     = 1u<<3, // ?
	PDN_GPU_CNT_RST_PPF            = 1u<<4,
	PDN_GPU_CNT_RST_PDC            = 1u<<5, // ?
	PDN_GPU_CNT_RST_PDC2           = 1u<<6, // Maybe pixel pipeline or so?

	PDN_GPU_CNT_RST_ALL            = (PDN_GPU_CNT_RST_PDC2<<1) - 1
};

typedef enum
{
	GFX_RGBA8  = 0,  ///< RGBA8. (4 bytes)
	GFX_BGR8   = 1,  ///< BGR8. (3 bytes)
	GFX_RGB565 = 2,  ///< RGB565. (2 bytes)
	GFX_RGB5A1 = 3,  ///< RGB5A1. (2 bytes)
	GFX_RGBA4  = 4   ///< RGBA4. (2 bytes)
} GfxFbFmt;

typedef enum
{
	GFX_EVENT_PSC0   = 0u,
	GFX_EVENT_PSC1   = 1u,
	GFX_EVENT_PDC0   = 2u,
	GFX_EVENT_PDC1   = 3u,
	GFX_EVENT_PPF    = 4u,
	GFX_EVENT_P3D    = 5u
} GfxEvent;

typedef enum
{
	GFX_BLIGHT_BOT   = 1u<<2,
	GFX_BLIGHT_TOP   = 1u<<4,
	GFX_BLIGHT_BOTH  = GFX_BLIGHT_TOP | GFX_BLIGHT_BOT
} GfxBlight;

#define REG_CFG11_GPUPROT        *((vu16*)(0x10140140))
#define REG_PDN_GPU_CNT          *((vu32*)(0x10141200))
#define PDN_GPU_CNT_CLK_E         (1u<<16)
#define PDN_VRAM_CNT_CLK_E        (1u)



#define GX_REGS_BASE             (0x10400000)
#define REG_GX_GPU_CLK           *((vu32*)(GX_REGS_BASE + 0x0004)) // ?

// PSC (memory fill) regs.
#define REG_GX_PSC_FILL0_S_ADDR  *((vu32*)(GX_REGS_BASE + 0x0010)) // Start address
#define REG_GX_PSC_FILL0_E_ADDR  *((vu32*)(GX_REGS_BASE + 0x0014)) // End address
#define REG_GX_PSC_FILL0_VAL     *((vu32*)(GX_REGS_BASE + 0x0018)) // Fill value
#define REG_GX_PSC_FILL0_CNT     *((vu32*)(GX_REGS_BASE + 0x001C))

#define REG_GX_PSC_FILL1_S_ADDR  *((vu32*)(GX_REGS_BASE + 0x0020))
#define REG_GX_PSC_FILL1_E_ADDR  *((vu32*)(GX_REGS_BASE + 0x0024))
#define REG_GX_PSC_FILL1_VAL     *((vu32*)(GX_REGS_BASE + 0x0028))
#define REG_GX_PSC_FILL1_CNT     *((vu32*)(GX_REGS_BASE + 0x002C))

#define REG_GX_PSC_VRAM          *((vu32*)(GX_REGS_BASE + 0x0030)) // gsp mudule only changes bit 8-11.
#define REG_GX_PSC_STAT          *((vu32*)(GX_REGS_BASE + 0x0034))

// PDC0/1 regs see lcd.h.

// PPF (transfer engine) regs.
#define REG_GX_PPF_IN_ADDR       *((vu32*)(GX_REGS_BASE + 0x0C00))
#define REG_GX_PPF_OUT_ADDR      *((vu32*)(GX_REGS_BASE + 0x0C04))
#define REG_GX_PPF_DT_OUTDIM     *((vu32*)(GX_REGS_BASE + 0x0C08)) // Display transfer output dimensions.
#define REG_GX_PPF_DT_INDIM      *((vu32*)(GX_REGS_BASE + 0x0C0C)) // Display transfer input dimensions.
#define REG_GX_PPF_FlAGS         *((vu32*)(GX_REGS_BASE + 0x0C10))
#define REG_GX_PPF_UNK14         *((vu32*)(GX_REGS_BASE + 0x0C14)) // Transfer interval?
#define REG_GX_PPF_CNT           *((vu32*)(GX_REGS_BASE + 0x0C18))
#define REG_GX_PPF_IRQ_POS       *((vu32*)(GX_REGS_BASE + 0x0C1C)) // ?
#define REG_GX_PPF_LEN           *((vu32*)(GX_REGS_BASE + 0x0C20)) // Texture copy size in bytes.
#define REG_GX_PPF_TC_INDIM      *((vu32*)(GX_REGS_BASE + 0x0C24)) // Texture copy input width and gap in 16 byte units.
#define REG_GX_PPF_TC_OUTDIM     *((vu32*)(GX_REGS_BASE + 0x0C28)) // Texture copy output width and gap in 16 byte units.

// P3D (GPU internal) regs. See gpu_regs.h.
#define REG_GX_P3D(reg)          *((vu32*)(GX_REGS_BASE + 0x1000 + ((reg) * 4)))

// LCD/ABL regs.
#define LCD_REGS_BASE            (0x10202000)
#define REG_LCD_PARALLAX_CNT     *((vu32*)(LCD_REGS_BASE + 0x000)) // Controls PWM for the parallax barrier?
#define REG_LCD_PARALLAX_PWM     *((vu32*)(LCD_REGS_BASE + 0x004)) // Frequency/other PWM stuff maybe?
#define REG_LCD_UNK00C           *((vu32*)(LCD_REGS_BASE + 0x00C)) // Wtf is "FIX"?
#define REG_LCD_RST              *((vu32*)(LCD_REGS_BASE + 0x014)) // Reset active low.

#define REG_LCD_ABL0_CNT         *((vu32*)(LCD_REGS_BASE + 0x200)) // Bit 0 enables ABL aka power saving mode.
#define REG_LCD_ABL0_FILL        *((vu32*)(LCD_REGS_BASE + 0x204))
#define REG_LCD_ABL0_LIGHT       *((vu32*)(LCD_REGS_BASE + 0x240))
#define REG_LCD_ABL0_LIGHT_PWM   *((vu32*)(LCD_REGS_BASE + 0x244))

#define REG_LCD_ABL1_CNT         *((vu32*)(LCD_REGS_BASE + 0xA00)) // Bit 0 enables ABL aka power saving mode.
#define REG_LCD_ABL1_FILL        *((vu32*)(LCD_REGS_BASE + 0xA04))
#define REG_LCD_ABL1_LIGHT       *((vu32*)(LCD_REGS_BASE + 0xA40))
#define REG_LCD_ABL1_LIGHT_PWM   *((vu32*)(LCD_REGS_BASE + 0xA44))


// Technically these regs belong in gx.h but they are used for LCD configuration so...
// Pitfall warning: The 3DS LCDs are physically rotated 90° CCW.

// PDC0 (top screen display controller) regs.
#define REG_LCD_PDC0_HTOTAL      *((vu32*)(GX_REGS_BASE + 0x400))
#define REG_LCD_PDC0_VTOTAL      *((vu32*)(GX_REGS_BASE + 0x424))
#define REG_LCD_PDC0_HPOS        *((const vu32*)(GX_REGS_BASE + 0x450))
#define REG_LCD_PDC0_VPOS        *((const vu32*)(GX_REGS_BASE + 0x454))
#define REG_LCD_PDC0_FB_A1       *((vu32*)(GX_REGS_BASE + 0x468))
#define REG_LCD_PDC0_FB_A2       *((vu32*)(GX_REGS_BASE + 0x46C))
#define REG_LCD_PDC0_FMT         *((vu32*)(GX_REGS_BASE + 0x470))
#define REG_LCD_PDC0_CNT         *((vu32*)(GX_REGS_BASE + 0x474))
#define REG_LCD_PDC0_SWAP        *((vu32*)(GX_REGS_BASE + 0x478))
#define REG_LCD_PDC0_STAT        *((const vu32*)(GX_REGS_BASE + 0x47C))
#define REG_LCD_PDC0_GTBL_IDX    *((vu32*)(GX_REGS_BASE + 0x480)) // Gamma table index.
#define REG_LCD_PDC0_GTBL_FIFO   *((vu32*)(GX_REGS_BASE + 0x484)) // Gamma table FIFO.
#define REG_LCD_PDC0_STRIDE      *((vu32*)(GX_REGS_BASE + 0x490))
#define REG_LCD_PDC0_FB_B1       *((vu32*)(GX_REGS_BASE + 0x494))
#define REG_LCD_PDC0_FB_B2       *((vu32*)(GX_REGS_BASE + 0x498))

// PDC1 (bottom screen display controller) regs.
#define REG_LCD_PDC1_HTOTAL      *((vu32*)(GX_REGS_BASE + 0x500))
#define REG_LCD_PDC1_VTOTAL      *((vu32*)(GX_REGS_BASE + 0x524))
#define REG_LCD_PDC1_HPOS        *((const vu32*)(GX_REGS_BASE + 0x550))
#define REG_LCD_PDC1_VPOS        *((const vu32*)(GX_REGS_BASE + 0x554))
#define REG_LCD_PDC1_FB_A1       *((vu32*)(GX_REGS_BASE + 0x568))
#define REG_LCD_PDC1_FB_A2       *((vu32*)(GX_REGS_BASE + 0x56C))
#define REG_LCD_PDC1_FMT         *((vu32*)(GX_REGS_BASE + 0x570))
#define REG_LCD_PDC1_CNT         *((vu32*)(GX_REGS_BASE + 0x574))
#define REG_LCD_PDC1_SWAP        *((vu32*)(GX_REGS_BASE + 0x578))
#define REG_LCD_PDC1_STAT        *((const vu32*)(GX_REGS_BASE + 0x57C))
#define REG_LCD_PDC1_GTBL_IDX    *((vu32*)(GX_REGS_BASE + 0x580)) // Gamma table index.
#define REG_LCD_PDC1_GTBL_FIFO   *((vu32*)(GX_REGS_BASE + 0x584)) // Gamma table FIFO.
#define REG_LCD_PDC1_STRIDE      *((vu32*)(GX_REGS_BASE + 0x590))
#define REG_LCD_PDC1_FB_B1       *((vu32*)(GX_REGS_BASE + 0x594))
#define REG_LCD_PDC1_FB_B2       *((vu32*)(GX_REGS_BASE + 0x598))


// REG_LCD_PDC_CNT
#define PDC_CNT_E           (1u)
#define PDC_CNT_I_MASK_H    (1u<<8)  // Disables H(Blank?) IRQs.
#define PDC_CNT_I_MASK_V    (1u<<9)  // Disables VBlank IRQs.
#define PDC_CNT_I_MASK_ERR  (1u<<10) // Disables error IRQs. What kind of errors?
#define PDC_CNT_OUT_E       (1u<<16) // Output enable?

// REG_LCD_PDC_SWAP
// Masks
#define PDC_SWAP_NEXT       (1u)     // Next framebuffer.
#define PDC_SWAP_CUR        (1u<<4)  // Currently displaying framebuffer?
// Bits
#define PDC_SWAP_RST_FIFO   (1u<<8)  // Which FIFO?
#define PDC_SWAP_I_H        (1u<<16) // H(Blank?) IRQ bit.
#define PDC_SWAP_I_V        (1u<<17) // VBlank IRQ bit.
#define PDC_SWAP_I_ERR      (1u<<18) // Error IRQ bit?
#define PDC_SWAP_I_ALL      (PDC_SWAP_I_ERR | PDC_SWAP_I_V | PDC_SWAP_I_H)


unsigned GFX_init(GfxFbFmt mode);
void GFX_setForceBlack(bool top, bool bot);
void GFX_powerOnBacklights(GfxBlight mask);
void GFX_powerOffBacklights(GfxBlight mask);

u8 GFX_getBrightness(void);
void GFX_setBrightness(u8 top, u8 bot);
