#include "hid.h"
#include "i2c.h"
#include "timer.h"
#include "screenshot.h" // for screenshots

#include "arm.h"
#include "fixp.h"
#include "shmem.h"

#define HID_TOUCH_MAXPOINT  (0x1000)
#define HID_TOUCH_MIDPOINT  (HID_TOUCH_MAXPOINT / 2)

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

// ts_mult indicates a scalar for each axis
// if |ts_mult| > 1 => point must be "scaled out"
// if |ts_mult| < 1 => point must be "scaled in"
// if ts_mult < 0 => axis is inverted
static fixp_t ts_mult[2];

// ts_org indicates the coordinate system origin
static int ts_org[2];
bool HID_ReadTouchState(u16 *x, u16 *y)
{
    u32 ts;
    int xc, yc;
    fixp_t tx, ty;

    ts = HID_ReadRawTouchState();
    if (ts & BIT(31))
        return false;

    tx = INT_TO_FIXP(HID_RAW_TX(ts) - HID_TOUCH_MIDPOINT);
    ty = INT_TO_FIXP(HID_RAW_TY(ts) - HID_TOUCH_MIDPOINT);

    xc = FIXP_TO_INT(fixp_round(fixp_product(tx, ts_mult[0]))) + ts_org[0];
    yc = FIXP_TO_INT(fixp_round(fixp_product(ty, ts_mult[1]))) + ts_org[1];

    *x = clamp(xc, 0, (ts_org[0] * 2) - 1);
    *y = clamp(yc, 0, (ts_org[1] * 2) - 1);
    return true;
}

bool HID_SetCalibrationData(const HID_CalibrationData *calibs, int point_cnt, int screen_w, int screen_h)
{
    int mid_x, mid_y;
    fixp_t avg_x, avg_y;

    if (!screen_w || !screen_h || point_cnt <= 0 || point_cnt > 7)
        return false;

    mid_x = screen_w / 2;
    mid_y = screen_h / 2;

    avg_x = 0;
    avg_y = 0;

    for (int i = 0; i < point_cnt; i++) {
        const HID_CalibrationData *data = &calibs[i];
        fixp_t screen_x, screen_y, touch_x, touch_y;

        // translate the [0, screen_w] x [0, screen_h] system
        // to [-screen_w/2, screen_w/2] x [-screen_h/2, screen_h/2]
        screen_x = INT_TO_FIXP(data->screen_x - mid_x);
        screen_y = INT_TO_FIXP(data->screen_y - mid_y);

        // same thing for raw touchscreen data
        touch_x = INT_TO_FIXP(HID_RAW_TX(data->ts_raw) - HID_TOUCH_MIDPOINT);
        touch_y = INT_TO_FIXP(HID_RAW_TY(data->ts_raw) - HID_TOUCH_MIDPOINT);

        // if the data retrieved came right in the middle, it's invalid
        if (!screen_x || !screen_y || !touch_x || !touch_y)
            return false;

        // prevent integer overflows by dividing in this step
        avg_x += fixp_quotient(screen_x, touch_x * point_cnt);
        avg_y += fixp_quotient(screen_y, touch_y * point_cnt);
    }

    // set state variables
    ts_mult[0] = avg_x;
    ts_mult[1] = avg_y;
    ts_org[0] = mid_x;
    ts_org[1] = mid_y;
    return true;
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
