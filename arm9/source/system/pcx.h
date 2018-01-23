#pragma once

#include <stdint.h>
#include "common.h"

#define PCX_MAGIC   0x0A, 0x05, 0x01, 0x08

typedef struct {
    u8  manufacturer;
    u8  version;
    u8  enc;
    u8  bpp;
    u16 minx;
    u16 miny;
    u16 maxx;
    u16 maxy;
    u16 hres;
    u16 vres;
    u8  egapalette[48];
    u8  reserved;
    u8  clrplanes;
    u16 bytesperline;
    u16 palettetype;
    u8  resv_[58];
} __attribute__((packed)) PCXHdr;

#define PCX_Width(hdr)              (hdr->maxx - hdr->minx + 1)
#define PCX_Height(hdr)             (hdr->maxy - hdr->miny + 1)
#define PCX_BitsPerPixel(hdr)       (hdr->bpp)
#define PCX_BitmapSize(hdr)         (PCX_Width(hdr) * PCX_Height(hdr))
#define PCX_MaxColors(hdr)          (1<<PCX_BitsPerPixel(hdr))
#define PCX_PlaneCount(hdr)         (hdr->clrplanes)
#define PCX_Encoding(hdr)           (hdr->enc)
#define PCX_Palette(hdr, pcx_len)   (((u8*)hdr) + pcx_len - 768)
#define PCX_Bitmap(hdr)             (((u8*)hdr) + sizeof(PCXHdr))

int PCX_Decompress(u8 *out_buf, size_t out_maxlen, const u8 *pcx_data, size_t pcx_len);
