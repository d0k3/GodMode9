#include "touchui.h"
#include "hid.h"


bool TouchBoxGet(u16* x, u16* y, u32* id, const TouchBox* tbs, const u32 tbn) {
    *id = 0;

    // read coordinates, check if inside touchbox
    if (!HID_ReadTouchState(x, y)) return false;
    for (u32 i = 0; i < tbn; i++) {
        const TouchBox* tb = tbs + i;
        if ((*x >= tb->x) && (*y >= tb->y) &&
            (*x < tb->x + tb->w) && (*y < tb->y + tb->h)) {
            *id = tb->id;
            break;
        }
    }

    return true;
}
