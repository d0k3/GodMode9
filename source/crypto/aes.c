/* original version by megazig */
#include "aes.h"

//FIXME some things make assumptions about alignemnts!

void setup_aeskeyX(uint8_t keyslot, const void* keyx)
{
    const uint32_t * _keyx = (const uint32_t*)keyx;
    *REG_AESCNT = (*REG_AESCNT) | AES_CNT_INPUT_ENDIAN | AES_CNT_INPUT_ORDER;
    *REG_AESKEYCNT = (*REG_AESKEYCNT >> 6 << 6) | keyslot | 0x80;
    if (keyslot > 3) {
        *REG_AESKEYXFIFO = _keyx[0];
        *REG_AESKEYXFIFO = _keyx[1];
        *REG_AESKEYXFIFO = _keyx[2];
        *REG_AESKEYXFIFO = _keyx[3];
    } else {
        uint32_t old_aescnt = *REG_AESCNT;
        volatile uint32_t* reg_aeskeyx = REG_AESKEY0123 + (((0x30u * keyslot) + 0x10u)/4u);
        *REG_AESCNT = (*REG_AESCNT & ~(AES_CNT_INPUT_ENDIAN | AES_CNT_INPUT_ORDER));
        for (uint32_t i = 0; i < 4u; i++)
            reg_aeskeyx[i] = _keyx[i];
        *REG_AESCNT = old_aescnt;
    }
}

void setup_aeskeyY(uint8_t keyslot, const void* keyy)
{
    const uint32_t * _keyy = (const uint32_t*)keyy;
    *REG_AESCNT = (*REG_AESCNT) | AES_CNT_INPUT_ENDIAN | AES_CNT_INPUT_ORDER;
    *REG_AESKEYCNT = (*REG_AESKEYCNT >> 6 << 6) | keyslot | 0x80;
    if (keyslot > 3) {
        *REG_AESKEYYFIFO = _keyy[0];
        *REG_AESKEYYFIFO = _keyy[1];
        *REG_AESKEYYFIFO = _keyy[2];
        *REG_AESKEYYFIFO = _keyy[3];
    } else {
        uint32_t old_aescnt = *REG_AESCNT;
        volatile uint32_t* reg_aeskeyy = REG_AESKEY0123 + (((0x30u * keyslot) + 0x20u)/4u);
        *REG_AESCNT = (*REG_AESCNT & ~(AES_CNT_INPUT_ENDIAN | AES_CNT_INPUT_ORDER));
        for (uint32_t i = 0; i < 4u; i++)
            reg_aeskeyy[i] = _keyy[i];
        *REG_AESCNT = old_aescnt;
    }
}

void setup_aeskey(uint8_t keyslot, const void* key)
{
    const uint32_t * _key = (const uint32_t*)key;
    *REG_AESCNT = (*REG_AESCNT) | AES_CNT_INPUT_ENDIAN | AES_CNT_INPUT_ORDER;
    *REG_AESKEYCNT = (*REG_AESKEYCNT >> 6 << 6) | keyslot | 0x80;
    if (keyslot > 3) {
        *REG_AESKEYFIFO = _key[0];
        *REG_AESKEYFIFO = _key[1];
        *REG_AESKEYFIFO = _key[2];
        *REG_AESKEYFIFO = _key[3];
    } else {
        uint32_t old_aescnt = *REG_AESCNT;
        volatile uint32_t* reg_aeskey = REG_AESKEY0123 + ((0x30u * keyslot)/4u);
        *REG_AESCNT = (*REG_AESCNT & ~(AES_CNT_INPUT_ENDIAN | AES_CNT_INPUT_ORDER));
        for (uint32_t i = 0; i < 4u; i++)
            reg_aeskey[i] = _key[i];
        *REG_AESCNT = old_aescnt;
    }
}

void use_aeskey(uint32_t keyno)
{
    if (keyno > 0x3F)
        return;
    *REG_AESKEYSEL = keyno;
    *REG_AESCNT    = *REG_AESCNT | 0x04000000; /* mystery bit */
}

void set_ctr(void* iv)
{
    uint32_t * _iv = (uint32_t*)iv;
    *REG_AESCNT = (*REG_AESCNT) | AES_CNT_INPUT_ENDIAN | AES_CNT_INPUT_ORDER;
    *(REG_AESCTR + 0) = _iv[3];
    *(REG_AESCTR + 1) = _iv[2];
    *(REG_AESCTR + 2) = _iv[1];
    *(REG_AESCTR + 3) = _iv[0];
}

void add_ctr(void* ctr, uint32_t carry)
{
    uint32_t counter[4];
    uint8_t *outctr = (uint8_t *) ctr;
    uint32_t sum;
    int32_t i;

    for(i = 0; i < 4; i++) {
		//FIXME this assumes alignment...
        counter[i] = ((uint32_t)outctr[i*4+0]<<24) | ((uint32_t)outctr[i*4+1]<<16) | ((uint32_t)outctr[i*4+2]<<8) | ((uint32_t)outctr[i*4+3]<<0);
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

void subtract_ctr(void* ctr, uint32_t carry)
{
    //ctr is in big endian format, 16 bytes
    uint32_t counter[4];
    uint8_t *outctr = (uint8_t *) ctr;

    //Convert each 4 byte part of ctr to uint32_t equivalents
    for(size_t i = 0; i < 4; i++) {
        //FIXME this assumes alignment...
        counter[i] = ((uint32_t)outctr[i*4+0]<<24) | ((uint32_t)outctr[i*4+1]<<16) | ((uint32_t)outctr[i*4+2]<<8) | ((uint32_t)outctr[i*4+3]<<0);
    }

    for(size_t i = 0; i < 4; ++i)
    {
        uint32_t sub;
        //using modular arithmetic to handle carry
        sub = counter[3-i] - carry;
        carry = counter[3-i] < carry;

        counter[3-i] = sub;
    }

    for(size_t i = 0; i < 4; i++)
    {
        outctr[i*4+0] = counter[i]>>24;
        outctr[i*4+1] = counter[i]>>16;
        outctr[i*4+2] = counter[i]>>8;
        outctr[i*4+3] = counter[i]>>0;
    }
}

void ecb_decrypt(void *inbuf, void *outbuf, size_t size, uint32_t mode)
{
    aes_decrypt(inbuf, outbuf, size, mode);
}

void cbc_decrypt(void *inbuf, void *outbuf, size_t size, uint32_t mode, uint8_t *ctr)
{
    size_t blocks_left = size;
    size_t blocks;
    uint8_t *in  = inbuf;
    uint8_t *out = outbuf;
    uint32_t i;

    while (blocks_left)
    {
        set_ctr(ctr);
        blocks = (blocks_left >= 0xFFFF) ? 0xFFFF : blocks_left;
        for (i=0; i<AES_BLOCK_SIZE; i++)
            ctr[i] = in[((blocks - 1) * AES_BLOCK_SIZE) + i];
        aes_decrypt(in, out, blocks, mode);
        in += blocks * AES_BLOCK_SIZE;
        out += blocks * AES_BLOCK_SIZE;
        blocks_left -= blocks;
    }
}

void cbc_encrypt(void *inbuf, void *outbuf, size_t size, uint32_t mode, uint8_t *ctr)
{
    size_t blocks_left = size;
    size_t blocks;
    uint8_t *in  = inbuf;
    uint8_t *out = outbuf;
    uint32_t i;

    while (blocks_left)
    {
        set_ctr(ctr);
        blocks = (blocks_left >= 0xFFFF) ? 0xFFFF : blocks_left;
        aes_decrypt(in, out, blocks, mode);
        for (i=0; i<AES_BLOCK_SIZE; i++)
            ctr[i] = in[((blocks - 1) * AES_BLOCK_SIZE) + i];
        in += blocks * AES_BLOCK_SIZE;
        out += blocks * AES_BLOCK_SIZE;
        blocks_left -= blocks;
    }
}

void ctr_decrypt_byte(void *inbuf, void *outbuf, size_t size, size_t off, uint32_t mode, uint8_t *ctr)
{
    size_t bytes_left = size;
    size_t off_fix = off % AES_BLOCK_SIZE;
    uint8_t __attribute__((aligned(32))) temp[AES_BLOCK_SIZE];
    uint8_t __attribute__((aligned(32))) ctr_local[AES_BLOCK_SIZE];
    uint8_t *in  = inbuf;
    uint8_t *out = outbuf;
    uint32_t i;
    
    for (i=0; i<AES_BLOCK_SIZE; i++) // setup local ctr
        ctr_local[i] = ctr[i];
    add_ctr(ctr_local, off / AES_BLOCK_SIZE);
    
    if (off_fix) // handle misaligned offset (at beginning)
    {
        size_t last_byte = ((off_fix + bytes_left) >= AES_BLOCK_SIZE) ?
            AES_BLOCK_SIZE : off_fix + bytes_left;
        for (i=off_fix; i<last_byte; i++)
            temp[i] = *(in++);
        ctr_decrypt(temp, temp, 1, mode, ctr_local);
        for (i=off_fix; i<last_byte; i++)
            *(out++) = temp[i];
        bytes_left -= last_byte - off_fix;
    }
    
    if (bytes_left >= AES_BLOCK_SIZE)
    {
        size_t blocks = bytes_left / AES_BLOCK_SIZE;
        ctr_decrypt(in, out, blocks, mode, ctr_local);
        in += AES_BLOCK_SIZE * blocks;
        out += AES_BLOCK_SIZE * blocks;
        bytes_left -= AES_BLOCK_SIZE * blocks;
    }
    
    if (bytes_left) // handle misaligned offset (at end)
    {
        for (i=0; i<bytes_left; i++)
            temp[i] = *(in++);
        ctr_decrypt(temp, temp, 1, mode, ctr_local);
        for (i=0; i<bytes_left; i++)
            *(out++) = temp[i];
        bytes_left = 0;
    }
}

void ctr_decrypt(void *inbuf, void *outbuf, size_t size, uint32_t mode, uint8_t *ctr)
{
    size_t blocks_left = size;
    size_t blocks;
    uint8_t *in  = inbuf;
    uint8_t *out = outbuf;

    while (blocks_left)
    {
        set_ctr(ctr);
        blocks = (blocks_left >= 0xFFFF) ? 0xFFFF : blocks_left;
        aes_decrypt(in, out, blocks, mode);
        add_ctr(ctr, blocks);
        in += blocks * AES_BLOCK_SIZE;
        out += blocks * AES_BLOCK_SIZE;
        blocks_left -= blocks;
    }
}

void aes_decrypt(void* inbuf, void* outbuf, size_t size, uint32_t mode)
{
    uint8_t *in  = inbuf;
    uint8_t *out = outbuf;
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

void aes_cmac(void* inbuf, void* outbuf, size_t size)
{
    // only works for full blocks
    uint32_t zeroes[4] __attribute__((aligned(32))) = { 0 };
    uint32_t xorpad[4] __attribute__((aligned(32))) = { 0 };
    uint32_t mode = AES_CBC_ENCRYPT_MODE | AES_CNT_INPUT_ORDER | AES_CNT_OUTPUT_ORDER |
        AES_CNT_INPUT_ENDIAN | AES_CNT_OUTPUT_ENDIAN;
    uint32_t* out = (uint32_t*) outbuf;
    uint32_t* in  = (uint32_t*) inbuf;
        
    // create xorpad for last block
    set_ctr(zeroes);
    aes_decrypt(xorpad, xorpad, 1, mode);
    char* xorpadb = (void*) xorpad;
    char finalxor = (xorpadb[0] & 0x80) ? 0x87 : 0x00;
    for (uint32_t i = 0; i < 15; i++) {
		xorpadb[i] <<= 1;
        xorpadb[i] |= xorpadb[i+1] >> 7;
    }
    xorpadb[15] <<= 1;
    xorpadb[15] ^= finalxor;
    
    // process blocks
    for (uint32_t i = 0; i < 4; i++)
        out[i] = 0;
    while (size-- > 0) {
        for (uint32_t i = 0; i < 4; i++)
			out[i] ^= *(in++);
        if (!size) { // last block
            for (uint32_t i = 0; i < 4; i++)
                out[i] ^= xorpad[i];
        }
        set_ctr(zeroes);
        aes_decrypt(out, out, 1, mode);
    }
}

void aes_fifos(void* inbuf, void* outbuf, size_t blocks)
{
    if (!inbuf || !outbuf) return;

    uint8_t *in = inbuf;
    uint8_t *out = outbuf;

    size_t curblock = 0;
    while (curblock != blocks)
    {
        while (aescnt_checkwrite());

        size_t blocks_to_read = blocks - curblock > 4 ? 4 : blocks - curblock;

        for (size_t wblocks = 0; wblocks < blocks_to_read; ++wblocks)
        for (uint8_t *ii = in + AES_BLOCK_SIZE * wblocks; ii != in + (AES_BLOCK_SIZE * (wblocks + 1)); ii += 4)
        {
            uint32_t data = ii[0];
            data |= (uint32_t)(ii[1]) << 8;
            data |= (uint32_t)(ii[2]) << 16;
            data |= (uint32_t)(ii[3]) << 24;
            set_aeswrfifo(data);
        }

        if (out)
        {
            for (size_t rblocks = 0; rblocks < blocks_to_read; ++rblocks)
            {
                while (aescnt_checkread()) ;
                for (uint8_t *ii = out + AES_BLOCK_SIZE * rblocks; ii != out + (AES_BLOCK_SIZE * (rblocks + 1)); ii += 4)
                {
                    uint32_t data = read_aesrdfifo();
                    ii[0] = data;
                    ii[1] = data >> 8;
                    ii[2] = data >> 16;
                    ii[3] = data >> 24;
                }
            }
        }

        in += blocks_to_read * AES_BLOCK_SIZE;
        out += blocks_to_read * AES_BLOCK_SIZE;
        curblock += blocks_to_read;
    }
}

void set_aeswrfifo(uint32_t value)
{
    *REG_AESWRFIFO = value;
}

uint32_t read_aesrdfifo(void)
{
    return *REG_AESRDFIFO;
}

uint32_t aes_getwritecount()
{
    return *REG_AESCNT & 0x1F;
}

uint32_t aes_getreadcount()
{
    return (*REG_AESCNT >> 5) & 0x1F;
}

uint32_t aescnt_checkwrite()
{
    size_t ret = aes_getwritecount();
    return (ret > 0xF);
}

uint32_t aescnt_checkread()
{
    size_t ret = aes_getreadcount();
    return (ret <= 3);
}

