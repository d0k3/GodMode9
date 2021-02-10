#include "vcart.h"
#include "gamecart.h"

#define FAT_LIMIT   0x100000000
#define VFLAG_SECURE_AREA_ENC   (1UL<<28)
#define VFLAG_GAMECART_NFO      (1UL<<29)
#define VFLAG_SAVEGAME          (1UL<<30)
#define VFLAG_PRIV_HDR          (1UL<<31)

static CartData* cdata = NULL;
static bool cart_init = false;
static bool cart_checked = false;

u32 InitVCartDrive(void) {
    if (!cart_checked) cart_checked = true;
    if (!cdata) cdata = (CartData*) malloc(sizeof(CartData));
    cart_init = (cdata && (InitCartRead(cdata) == 0) && (cdata->cart_size <= FAT_LIMIT));
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

    while (++vdir->index <= 9) {
        if ((vdir->index == 0) && (cdata->data_size < FAT_LIMIT)) { // standard full rom
            snprintf(vfile->name, 32, "%s.%s", name, ext);
            vfile->size = cdata->cart_size;
            if (vfile->size == FAT_LIMIT) vfile->size--;
            return true;
        } else if ((vdir->index == 1)  && (cdata->data_size < FAT_LIMIT) && cdata->data_size) { // trimmed rom
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
        } else if ((vdir->index == 5) && (cdata->data_size < FAT_LIMIT) &&
            (cdata->cart_type & CART_NTR)) { // encrypted secure area
            snprintf(vfile->name, 32, "%s.nds.enc", name);
            vfile->size = cdata->cart_size;
            if (vfile->size == FAT_LIMIT) vfile->size--;
            vfile->flags = VFLAG_SECURE_AREA_ENC;
            return true;
        } else if ((vdir->index == 6) && (cdata->cart_type & CART_CTR)) { // private header
            snprintf(vfile->name, 32, "%s-priv.bin", name);
            vfile->size = PRIV_HDR_SIZE;
            vfile->flags |= VFLAG_PRIV_HDR;
            return true;
        } else if ((vdir->index == 7) && (cdata->save_type != CARD_SAVE_NONE)) { // savegame
            snprintf(vfile->name, 32, "%s.sav", name);
            vfile->size = cdata->save_size;
            vfile->flags = VFLAG_SAVEGAME;
            if (cdata->save_type == CARD_SAVE_CARD2) {
                vfile->flags |= VFLAG_READONLY;
            }
            return true;
        } else if (vdir->index == 8) { // gamecart info
            char info[256];
            GetCartInfoString(info, sizeof(info), cdata);
            snprintf(vfile->name, 32, "%s.txt", name);
            vfile->size = strnlen(info, 255);
            vfile->flags |= VFLAG_GAMECART_NFO;
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
    else if (vfile->flags & VFLAG_SAVEGAME)
        return ReadCartSave(buffer, foffset, count, cdata);
    else if (vfile->flags & VFLAG_GAMECART_NFO)
        return ReadCartInfo(buffer, foffset, count, cdata);

    SetSecureAreaEncryption(vfile->flags & VFLAG_SECURE_AREA_ENC);
    return ReadCartBytes(buffer, foffset, count, cdata, true);
}

int WriteVCartFile(const VirtualFile* vfile, const void* buffer, u64 offset, u64 count) {
    if (!cdata) return -1;
    if (vfile->flags & VFLAG_SAVEGAME) {
        return WriteCartSave(buffer, offset, count, cdata);
    }
    return -1;
}

u64 GetVCartDriveSize(void) {
    return cart_init ? cdata->cart_size : 0;
}

void GetVCartTypeString(char* typestr) {
    // typestr needs to be at least 11 + 1 chars big
    if (!cart_init || !cdata) sprintf(typestr, cart_checked ? "EMPTY" : "");
    else sprintf(typestr, "%s%08lX",
        (cdata->cart_type & CART_CTR) ? "CTR" :
        (cdata->cart_type & CART_TWL) ? "TWL" :
        (cdata->cart_type & CART_NTR) ? "NTR" : "???",
        cdata->cart_id);
}
