/* original version by megazig */
#include "aes.h"

void setup_aeskeyX(u8 keyslot, void* keyx)
{
    u32 * _keyx = (u32*)keyx;
    *REG_AESKEYCNT = (*REG_AESKEYCNT >> 6 << 6) | keyslot | 0x80;
    if (keyslot > 3) {
        *REG_AESKEYXFIFO = _keyx[0];
        *REG_AESKEYXFIFO = _keyx[1];
        *REG_AESKEYXFIFO = _keyx[2];
        *REG_AESKEYXFIFO = _keyx[3];
    } else {
        u32 old_aescnt = *REG_AESCNT;
        vu32* reg_aeskeyx = REG_AESKEY0123 + (((0x30*keyslot) + 0x10)/4);
        *REG_AESCNT = (*REG_AESCNT & ~(AES_CNT_INPUT_ENDIAN | AES_CNT_INPUT_ORDER));
        for (u32 i = 0; i < 4; i++)
            reg_aeskeyx[i] = _keyx[i];
        *REG_AESCNT = old_aescnt;
    }
}

void setup_aeskeyY(u8 keyslot, void* keyy)
{
    u32 * _keyy = (u32*)keyy;
    *REG_AESKEYCNT = (*REG_AESKEYCNT >> 6 << 6) | keyslot | 0x80;
    if (keyslot > 3) {
        *REG_AESKEYYFIFO = _keyy[0];
        *REG_AESKEYYFIFO = _keyy[1];
        *REG_AESKEYYFIFO = _keyy[2];
        *REG_AESKEYYFIFO = _keyy[3];
    } else {
        u32 old_aescnt = *REG_AESCNT;
        vu32* reg_aeskeyy = REG_AESKEY0123 + (((0x30*keyslot) + 0x20)/4);
        *REG_AESCNT = (*REG_AESCNT & ~(AES_CNT_INPUT_ENDIAN | AES_CNT_INPUT_ORDER));
        for (u32 i = 0; i < 4; i++)
            reg_aeskeyy[i] = _keyy[i];
        *REG_AESCNT = old_aescnt;
    }
}

void setup_aeskey(u8 keyslot, void* key)
{
    u32 * _key = (u32*)key;
    *REG_AESKEYCNT = (*REG_AESKEYCNT >> 6 << 6) | keyslot | 0x80;
    if (keyslot > 3) {
        *REG_AESKEYFIFO = _key[0];
        *REG_AESKEYFIFO = _key[1];
        *REG_AESKEYFIFO = _key[2];
        *REG_AESKEYFIFO = _key[3];
    } else {
        u32 old_aescnt = *REG_AESCNT;
        vu32* reg_aeskey = REG_AESKEY0123 + ((0x30*keyslot)/4);
        *REG_AESCNT = (*REG_AESCNT & ~(AES_CNT_INPUT_ENDIAN | AES_CNT_INPUT_ORDER));
        for (u32 i = 0; i < 4; i++)
            reg_aeskey[i] = _key[i];
        *REG_AESCNT = old_aescnt;
    }
}

void use_aeskey(u32 keyno)
{
    if (keyno > 0x3F)
        return;
    *REG_AESKEYSEL = keyno;
    *REG_AESCNT    = *REG_AESCNT | 0x04000000; /* mystery bit */
}

void set_ctr(void* iv)
{
    u32 * _iv = (u32*)iv;
    *REG_AESCNT = (*REG_AESCNT) | AES_CNT_INPUT_ENDIAN | AES_CNT_INPUT_ORDER;
    *(REG_AESCTR + 0) = _iv[3];
    *(REG_AESCTR + 1) = _iv[2];
    *(REG_AESCTR + 2) = _iv[1];
    *(REG_AESCTR + 3) = _iv[0];
}

void add_ctr(void* ctr, u32 carry)
{
    u32 counter[4];
    u8 *outctr = (u8 *) ctr;
    u32 sum;
    int32_t i;

    for(i=0; i<4; i++) {
        counter[i] = (outctr[i*4+0]<<24) | (outctr[i*4+1]<<16) | (outctr[i*4+2]<<8) | (outctr[i*4+3]<<0);
    }

    for(i=3; i>=0; i--)
    {
        sum = counter[i] + carry;
        if (sum < counter[i]) {
            carry = 1;
        }
        else {
            carry = 0;
        }
        counter[i] = sum;
    }

    for(i=0; i<4; i++)
    {
        outctr[i*4+0] = counter[i]>>24;
        outctr[i*4+1] = counter[i]>>16;
        outctr[i*4+2] = counter[i]>>8;
        outctr[i*4+3] = counter[i]>>0;
    }
}

void aes_decrypt(void* inbuf, void* outbuf, size_t size, u32 mode)
{
    u32 in  = (u32)inbuf;
    u32 out = (u32)outbuf;
    size_t block_count = size;
    size_t blocks;
    while (block_count != 0)
    {
        blocks = (block_count >= 0xFFFF) ? 0xFFFF : block_count;
        *REG_AESCNT = 0;
        *REG_AESBLKCNT = blocks << 16;
        *REG_AESCNT = mode |
                      AES_CNT_START |
                      AES_CNT_FLUSH_READ |
                      AES_CNT_FLUSH_WRITE;
        aes_fifos((void*)in, (void*)out, blocks);
        in  += blocks * AES_BLOCK_SIZE;
        out += blocks * AES_BLOCK_SIZE;
        block_count -= blocks;
    }
}

void aes_fifos(void* inbuf, void* outbuf, size_t blocks)
{
    u32 in  = (u32)inbuf;
    if (!in) return;

    u32 out = (u32)outbuf;
    size_t curblock = 0;
    while (curblock != blocks)
    {
        while (aescnt_checkwrite());

        int ii = 0;
        for (ii = in; ii != in + AES_BLOCK_SIZE; ii += 4)
        {
            set_aeswrfifo( *(u32*)(ii) );
        }
        if (out)
        {
            while (aescnt_checkread()) ;
            for (ii = out; ii != out + AES_BLOCK_SIZE; ii += 4)
            {
                *(u32*)ii = read_aesrdfifo();
            }
        }
        curblock++;
    }
}

void set_aeswrfifo(u32 value)
{
    *REG_AESWRFIFO = value;
}

u32 read_aesrdfifo(void)
{
    return *REG_AESRDFIFO;
}

u32 aes_getwritecount()
{
    return *REG_AESCNT & 0x1F;
}

u32 aes_getreadcount()
{
    return (*REG_AESCNT >> 5) & 0x1F;
}

u32 aescnt_checkwrite()
{
    size_t ret = aes_getwritecount();
    return (ret > 0xF);
}

u32 aescnt_checkread()
{
    size_t ret = aes_getreadcount();
    return (ret <= 3);
}
