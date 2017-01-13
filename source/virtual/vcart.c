#include "vcart.h"
#include "gamecart.h"

#define VFLAG_PRIV_HDR  (1<<31)

static CartData* cdata = (CartData*) VCART_BUFFER; // 128kB reserved (~64kB required)

bool ReadVCartDir(VirtualFile* vfile, VirtualDir* vdir) {
    if ((vdir->index < 0) &&
        (CheckCartId(cdata->cart_id) != 0) &&
        (InitCardRead(cdata) != 0))
        return false;
    
    if (++vdir->index < 3) {
        char name[24];
        GetCartName(name, cdata);
        memset(vfile, 0, sizeof(VirtualFile));
        vfile->keyslot = 0xFF; // unused
        
        if (vdir->index == 2) { // private header
            if (!(cdata->cart_type & CART_CTR)) return false;
            snprintf(vfile->name, 32, "%s-private.bin", name);
            vfile->size = PRIV_HDR_SIZE;
            vfile->flags = VFLAG_PRIV_HDR;
        } else {
            const char* ext = (cdata->cart_type & CART_CTR) ? "3ds" : "nds";
            snprintf(vfile->name, 32, "%s%s.%s", name, (vdir->index == 1) ? ".trim" : "", ext);
            vfile->size = (vdir->index == 1) ? cdata->data_size : cdata->cart_size;
        }
        
        return true;
    }
    
    return false;
}

int ReadVCartFile(const VirtualFile* vfile, u8* buffer, u32 offset, u32 count) {
    if (vfile->flags & VFLAG_PRIV_HDR)
        return ReadCartPrivateHeader(buffer, offset, count, cdata);
    else return ReadCartBytes(buffer, offset, count, cdata);
}
