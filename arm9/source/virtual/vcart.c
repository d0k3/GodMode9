#include "vcart.h"
#include "gamecart.h"

#define FAT_LIMIT   0x100000000
#define VFLAG_PRIV_HDR  (1UL<<31)

static CartData* cdata = NULL;
static bool cart_init = false;

u32 InitVCartDrive(void) {
    if (!cdata) cdata = (CartData*) malloc(sizeof(CartData));
    cart_init = (cdata && (InitCardRead(cdata) == 0) && (cdata->cart_size <= FAT_LIMIT));
    if (!cart_init && cdata) {
        free(cdata);
        cdata = NULL;
    }
    return cart_init ? cdata->cart_id : 0;
}

bool ReadVCartDir(VirtualFile* vfile, VirtualDir* vdir) {
    if ((vdir->index < 0) && !cart_init)
        InitVCartDrive();
    if (!cart_init) return false;
    
    const char* ext = (cdata->cart_type & CART_CTR) ? "3ds" : "nds";
    char name[24];
    GetCartName(name, cdata);
    memset(vfile, 0, sizeof(VirtualFile));
    vfile->keyslot = 0xFF; // unused
    vfile->flags = VFLAG_READONLY;
        
    while (++vdir->index <= 5) {
        if ((vdir->index == 0) && (cdata->data_size < FAT_LIMIT)) { // standard full rom
            snprintf(vfile->name, 32, "%s.%s", name, ext);
            vfile->size = cdata->cart_size;
            if (vfile->size == FAT_LIMIT) vfile->size--;
            return true;
        } else if ((vdir->index == 1)  && (cdata->data_size < FAT_LIMIT)) { // trimmed rom
            snprintf(vfile->name, 32, "%s.trim.%s", name, ext);
            vfile->size = cdata->data_size;
            return true;
        } else if ((vdir->index == 3)  && (cdata->cart_size == FAT_LIMIT)) { // split rom .000
            snprintf(vfile->name, 32, "%s.split.000", name);
            vfile->size = (FAT_LIMIT / 2);
            return true;
        } else if ((vdir->index == 4)  && (cdata->cart_size == FAT_LIMIT)) { // split rom .001
            snprintf(vfile->name, 32, "%s.split.001", name);
            vfile->size = (FAT_LIMIT / 2);
            vfile->offset = (FAT_LIMIT / 2);
            return true;
        } else if ((vdir->index == 5) && (cdata->cart_type & CART_CTR)) { // private header
            snprintf(vfile->name, 32, "%s-priv.bin", name);
            vfile->size = PRIV_HDR_SIZE;
            vfile->flags |= VFLAG_PRIV_HDR;
            return true;
        }
    }
    
    return false;
}

int ReadVCartFile(const VirtualFile* vfile, void* buffer, u64 offset, u64 count) {
    u32 foffset = vfile->offset + offset;
    if (!cdata) return -1;
    if (vfile->flags & VFLAG_PRIV_HDR)
        return ReadCartPrivateHeader(buffer, foffset, count, cdata);
    else return ReadCartBytes(buffer, foffset, count, cdata);
}

u64 GetVCartDriveSize(void) {
    return cart_init ? cdata->cart_size : 0;
}
