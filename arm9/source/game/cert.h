#pragma once

#include "common.h"

#define CERT_SIZE  sizeof(Certificate)

// from: http://3dbrew.org/wiki/Certificates
// all numbers in big endian
typedef struct {
    u8 sig_type[4]; // expected: 0x010004 / RSA_2048 SHA256
    u8 signature[0x100];
    u8 padding0[0x3C];
    u8 issuer[0x40];
    u8 keytype[4]; // expected: 0x01 / RSA_2048
    u8 name[0x40];
    u8 unknown[4];
    u8 mod[0x100];
    u8 exp[0x04];
    u8 padding1[0x34];
} __attribute__((packed)) Certificate;

u32 LoadCertFromCertDb(u64 offset, Certificate* cert, u32* mod, u32* exp);
