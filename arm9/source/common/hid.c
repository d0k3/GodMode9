#include "hid.h"
#include "i2c.h"
#include "timer.h"
#include "power.h"

u32 InputWait(u32 timeout_sec) {
    static u64 delay = 0;
    u32 pad_state_old = HID_STATE;
    u32 cart_state_old = CART_STATE;
    u32 sd_state_old = SD_STATE;
    u64 timer = timer_start();
    u64 timer_mcu = timer;
    delay = (delay) ? 72 : 128;
    while (true) {
        u32 pad_state = HID_STATE;
        if (timeout_sec && (timer_sec(timer) >= timeout_sec))
            return TIMEOUT_HID; // HID timeout
        if (!(pad_state & BUTTON_ANY)) { // no buttons pressed
            u32 cart_state = CART_STATE;
            if (cart_state != cart_state_old)
                return cart_state ? CART_INSERT : CART_EJECT;
            u32 sd_state = SD_STATE;
            if (sd_state != sd_state_old)
                return sd_state ? SD_INSERT : SD_EJECT;
            u8 special_key;
            if ((timer_msec(timer_mcu) >= 64) && (I2C_readRegBuf(I2C_DEV_MCU, 0x10, &special_key, 1))) {
                CheckBrightness();
                if (special_key == 0x01)
                    return pad_state | BUTTON_POWER;
                else if (special_key == 0x04)
                    return pad_state | BUTTON_HOME;
                timer_mcu = timer_start();
            }
            pad_state_old = pad_state;
            delay = 0;
            continue;
        }
        if ((pad_state == pad_state_old) &&
            (!(pad_state & BUTTON_ARROW) ||
            (delay && (timer_msec(timer) < delay))))
            continue;
        // make sure the key is pressed
        u32 t_pressed = 0;
        for(; (t_pressed < 0x13000) && (pad_state == HID_STATE); t_pressed++);
        if (t_pressed >= 0x13000)
            return pad_state;
    }
}

bool CheckButton(u32 button) {
    u32 t_pressed = 0;
    for(; (t_pressed < 0x13000) && ((HID_STATE & button) == button); t_pressed++);
    return (t_pressed >= 0x13000);
}
