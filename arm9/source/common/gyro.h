#pragma once

#include "common.h"

typedef enum {
    GYRO_1_WHO_AM_I = 0x00,
    GYRO_1_PWR_MGM = 0x3E
} GyroModel1Register;

typedef enum {
    GYRO_2_PWR_MGM_1 = 0x6B,
    GYRO_2_WHO_AM_I = 0x75
} GyroModel2Register;

typedef enum {
    GYRO_3_PWR_MGM = 0x39 // Unconfirmed
} GyroModel3Register;

u32 GetGyroModel();
const char* GetGyroModelString();
