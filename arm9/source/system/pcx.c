#include <stddef.h>
#include <stdint.h>

#include "pcx.h"

// Define __PCX_INVERT_RGB to have the buffer output in BGR24
#define __PCX_INVERT_RGB


static inline int PCX_Validate(const PCXHdr *hdr) {
    return (hdr != NULL &&
            hdr->manufacturer == 10 &&
            PCX_Encoding(hdr) == 1 &&
            PCX_PlaneCount(hdr) == 1 &&
            PCX_BitsPerPixel(hdr) == 8) ? 1 : 0;
}

int PCX_Decompress(u8 *out_buf, size_t out_maxlen, const u8 *pcx_data, size_t pcx_len) {
    const PCXHdr *pcx = (const PCXHdr*)pcx_data;
    const u8 *pcx_palette, *out_start, *out_end, *pcx_bitmap, *pcx_end;
    int mrk, palind;

    if (PCX_Validate(pcx) == 0 || (size_t) (PCX_BitmapSize(pcx) * 3) > out_maxlen)
        return -1;

    pcx_end = pcx_data + pcx_len - 769;
    out_end = out_buf + out_maxlen;
    out_start = out_buf;
    pcx_bitmap = PCX_Bitmap(pcx);
    pcx_palette = PCX_Palette(pcx, pcx_len);

    while (pcx_bitmap < pcx_end && out_buf < out_end) {
        mrk = *(pcx_bitmap++);

        if (mrk >= 0xC0) {
            mrk -= 0xC0;
            palind = *(pcx_bitmap++) * 3;

            if ((out_buf + mrk) > out_end) {
                // TODO: report the buffer overflow somehow
                return -1;
            }
        } else {
            palind = mrk * 3;
            mrk = 1;
        }

        while (mrk--) {
            #ifdef __PCX_INVERT_RGB
            *(out_buf++) = pcx_palette[palind + 2];
            *(out_buf++) = pcx_palette[palind + 1];
            *(out_buf++) = pcx_palette[palind + 0];
            #else
            *(out_buf++) = pcx_palette[palind + 0];
            *(out_buf++) = pcx_palette[palind + 1];
            *(out_buf++) = pcx_palette[palind + 2];
            #endif
        }
    }

    return (out_buf - out_start);
}
