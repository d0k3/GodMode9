#pragma once

#include "common.h"


typedef struct _IKEY1{
    u32 iii;
    u32 jjj;
    u32 kkkkk;
    u32 llll;
    u32 mmm;
    u32 nnn;
} IKEY1, *PIKEY1;

void NTR_InitKey (u32 aGameCode, u32* pCardHash, int nCardHash, u32* pKeyCode, int level, int iCardDevice);
void NTR_InitKey1 (u8* aCmdData, IKEY1* pKey1, int iCardDevice);

void NTR_CreateEncryptedCommand (u8 aCommand, u32* pCardHash, u8* aCmdData, IKEY1* pKey1, u32 aBlock);
void NTR_DecryptSecureArea (u32 aGameCode, u32* pCardHash, int nCardHash, u32* pKeyCode, u32* pSecureArea, int iCardDevice);
