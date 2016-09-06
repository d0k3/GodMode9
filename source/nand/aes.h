#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AES_BLOCK_SIZE 0x10

#define AES_CCM_DECRYPT_MODE (0u << 27)
#define AES_CCM_ENCRYPT_MODE (1u << 27)
#define AES_CTR_MODE         (2u << 27)
#define AES_CBC_DECRYPT_MODE (4u << 27)
#define AES_CBC_ENCRYPT_MODE (5u << 27)
#define AES_ECB_DECRYPT_MODE (6u << 27)
#define AES_ECB_ENCRYPT_MODE (7u << 27)

#define REG_AESCNT     ((volatile uint32_t*)0x10009000)
#define REG_AESBLKCNT  ((volatile uint32_t*)0x10009004)
#define REG_AESWRFIFO  ((volatile uint32_t*)0x10009008)
#define REG_AESRDFIFO  ((volatile uint32_t*)0x1000900C)
#define REG_AESKEYSEL  ((volatile uint8_t *)0x10009010)
#define REG_AESKEYCNT  ((volatile uint8_t *)0x10009011)
#define REG_AESCTR     ((volatile uint32_t*)0x10009020)
#define REG_AESKEYFIFO ((volatile uint32_t*)0x10009100)
#define REG_AESKEYXFIFO ((volatile uint32_t*)0x10009104)
#define REG_AESKEYYFIFO ((volatile uint32_t*)0x10009108)
#define REG_AESMAC      ((volatile uint32_t*)0x10009030)
// see https://www.3dbrew.org/wiki/AES_Registers#AES_KEY0.2F1.2F2.2F3
#define REG_AESKEY0123 ((volatile uint32_t*)0x10009040)

#define AES_CNT_START         0x80000000u
#define AES_CNT_INPUT_ORDER   0x02000000u
#define AES_CNT_OUTPUT_ORDER  0x01000000u
#define AES_CNT_INPUT_ENDIAN  0x00800000u
#define AES_CNT_OUTPUT_ENDIAN 0x00400000u
#define AES_CNT_FLUSH_READ    0x00000800u
#define AES_CNT_FLUSH_WRITE   0x00000400u

#define AES_CNT_CTRNAND_MODE (AES_CTR_MODE | AES_CNT_INPUT_ORDER | AES_CNT_OUTPUT_ORDER | AES_CNT_INPUT_ENDIAN | AES_CNT_OUTPUT_ENDIAN)
#define AES_CNT_TWLNAND_MODE AES_CTR_MODE
#define AES_CNT_TITLEKEY_DECRYPT_MODE (AES_CBC_DECRYPT_MODE | AES_CNT_INPUT_ORDER | AES_CNT_OUTPUT_ORDER | AES_CNT_INPUT_ENDIAN | AES_CNT_OUTPUT_ENDIAN)
#define AES_CNT_TITLEKEY_ENCRYPT_MODE (AES_CBC_ENCRYPT_MODE | AES_CNT_INPUT_ORDER | AES_CNT_OUTPUT_ORDER | AES_CNT_INPUT_ENDIAN | AES_CNT_OUTPUT_ENDIAN)
#define AES_CNT_ECB_DECRYPT_MODE (AES_ECB_DECRYPT_MODE | AES_CNT_INPUT_ORDER | AES_CNT_OUTPUT_ORDER | AES_CNT_INPUT_ENDIAN | AES_CNT_OUTPUT_ENDIAN)
#define AES_CNT_ECB_ENCRYPT_MODE (AES_ECB_ENCRYPT_MODE | AES_CNT_INPUT_ORDER | AES_CNT_OUTPUT_ORDER | AES_CNT_INPUT_ENDIAN | AES_CNT_OUTPUT_ENDIAN)

void setup_aeskeyX(uint8_t keyslot, void* keyx);
void setup_aeskeyY(uint8_t keyslot, void* keyy);
void setup_aeskey(uint8_t keyslot, void* keyy);
void use_aeskey(uint32_t keyno);
void set_ctr(void* iv);
void add_ctr(void* ctr, uint32_t carry);
void aes_decrypt(void* inbuf, void* outbuf, size_t size, uint32_t mode);
void ctr_decrypt(void* inbuf, void* outbuf, size_t size, uint32_t mode, uint8_t *ctr);
void aes_cmac(void* inbuf, void* outbuf, size_t size);
void aes_fifos(void* inbuf, void* outbuf, size_t blocks);
void set_aeswrfifo(uint32_t value);
uint32_t read_aesrdfifo(void);
uint32_t aes_getwritecount(void);
uint32_t aes_getreadcount(void);
uint32_t aescnt_checkwrite(void);
uint32_t aescnt_checkread(void);

#ifdef __cplusplus
}
#endif

