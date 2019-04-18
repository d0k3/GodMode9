#include "hid.h"
#include "i2c.h"
#include "timer.h"
#include "screenshot.h" // for screenshots

#include "arm.h"
#include "shmem.h"

// there's some weird thing going on when reading this
// with an LDRD instruction so for now they'll be two
// separate things - hopefully LTO won't get in the way
u32 HID_ReadState(void)
{
    return ARM_GetSHMEM()->hid_state;
}

u32 HID_ReadRawTouchState(void)
{
    return ARM_GetSHMEM()->hid_state >> 32;
}

u32 InputWait(u32 timeout_sec) {
    static u64 delay = 0;
    u64 timer = timer_start();

    u32 oldpad = HID_ReadState();
    u32 oldcart = CART_STATE;
    u32 oldsd = SD_STATE;

    delay = delay ? 72 : 128;

    do {
        u32 newpad = HID_ReadState();

        if (!newpad) { // no buttons pressed, check for I/O changes instead
            u32 state = CART_STATE;
            if (state != oldcart)
                return state ? CART_INSERT : CART_EJECT;

            state = SD_STATE;
            if (state != oldsd)
                return state ? SD_INSERT : SD_EJECT;

            oldpad = 0;
            delay = 0;
            continue;
        }

        // special case for dpad keys
        // if any of those are held, don't wait for key changes
        // but do insert a small latency to make
        // sure any menus don't go flying off
        if ((newpad == oldpad) &&
            (!(newpad & BUTTON_ARROW) ||
            (delay && (timer_msec(timer) < delay))))
            continue;

        u32 t_pressed = 0;
        while((t_pressed++ < 0x13000) && (newpad == HID_ReadState()));
        if (t_pressed >= 0x13000) {
            if ((newpad & BUTTON_ANY) == (BUTTON_R1 | BUTTON_L1))
                CreateScreenshot(); // screenshot handling
            return newpad;
        }
    } while (!timeout_sec || (timeout_sec && (timer_sec(timer) < timeout_sec)));

    return TIMEOUT_HID;
}

bool CheckButton(u32 button) {
    return (HID_ReadState() & button) == button;
}

void ButtonToString(u32 button, char* str) {
    const char* strings[] = { BUTTON_STRINGS };

    *str = '\0';
    if (button) {
        u32 b = 0;
        for (b = 0; !((button>>b)&0x1); b++);
        if (b < countof(strings)) strcpy(str, strings[b]);
    }
}

u32 StringToButton(char* str) {
    const char* strings[] = { BUTTON_STRINGS };

    u32 b = 0;
    for (b = 0; b < countof(strings); b++)
        if (strcmp(str, strings[b]) == 0) break;

    return (b == countof(strings)) ? 0 : 1<<b;
}
