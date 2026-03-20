#include "gyro.h"
#include "i2c.h"

u32 GetGyroModel() {
    uint8_t buffer;

    if (I2C_readRegBuf(I2C_DEV_GYRO, GYRO_1_PWR_MGM, &buffer, 1)) {
        // ITG-3270 - o3DS and early o2DS
        return 1;
    } else if (I2C_readRegBuf(I2C_DEV_GYRO2, GYRO_2_PWR_MGM_1, &buffer, 1)) {
        // ITG-1010 - late models o2DS and early n3DS
        return 2;
    } else if (I2C_readRegBuf(I2C_DEV_GYRO3, GYRO_3_PWR_MGM, &buffer, 1)) {
        // ?? - later models n3DS
        return 3;
    } else {
        // ????
        return 0;
    }
}

const char* GetGyroModelString() {
    switch (GetGyroModel()) {
        case 1: return "1";
        case 2: return "2";
        case 3: return "3";
        default: return "0";
    }
}
