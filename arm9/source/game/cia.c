#include "cia.h"
#include "ncch.h"
#include "unittype.h"
#include "ff.h"
#include "aes.h"
#include "sha.h"
#include "cert.h"

u32 ValidateCiaHeader(CiaHeader* header) {
    if ((header->size_header != CIA_HEADER_SIZE) ||
        (header->size_cert != CIA_CERT_SIZE) ||
        (header->size_ticket != TICKET_COMMON_SIZE) ||
        (header->size_tmd < TMD_SIZE_MIN) ||
        (header->size_tmd > TMD_SIZE_MAX) ||
        (header->size_content == 0))
        return 1;
    return 0;
}

u32 GetCiaInfo(CiaInfo* info, CiaHeader* header) {
    if ((u8*) info != (u8*) header) memcpy(info, header, 0x20); // take over first 0x20 byte

    info->offset_cert = align(header->size_header, 64);
    info->offset_ticket = info->offset_cert + align(header->size_cert, 64);
    info->offset_tmd = info->offset_ticket + align(header->size_ticket, 64);
    info->offset_content = info->offset_tmd + align(header->size_tmd, 64);
    info->offset_meta = (header->size_meta) ? info->offset_content + align(header->size_content, 64) : 0;
    info->offset_content_list = info->offset_tmd + sizeof(TitleMetaData);

    info->size_content_list = info->size_tmd - sizeof(TitleMetaData);
    info->size_cia = (header->size_meta) ? info->offset_meta + info->size_meta :
        info->offset_content + info->size_content;

    info->max_contents = (info->size_tmd - sizeof(TitleMetaData)) /  sizeof(TmdContentChunk);

    return 0;
}

u32 FixCiaHeaderForTmd(CiaHeader* header, TitleMetaData* tmd) {
    TmdContentChunk* content_list = (TmdContentChunk*) (tmd + 1);
    u32 content_count = getbe16(tmd->content_count);
    header->size_content = 0;
    header->size_tmd = TMD_SIZE_N(content_count);
    memset(header->content_index, 0, sizeof(header->content_index));
    for (u32 i = 0; i < content_count; i++) {
        u16 index = getbe16(content_list[i].index);
        header->size_content += getbe64(content_list[i].size);
        header->content_index[index/8] |= (1 << (7-(index%8)));
    }
    return 0;
}

u32 BuildCiaCert(u8* ciacert) {
    static const u8 cert_hash_expected[0x20] = {
        0xC7, 0x2E, 0x1C, 0xA5, 0x61, 0xDC, 0x9B, 0xC8, 0x05, 0x58, 0x58, 0x9C, 0x63, 0x08, 0x1C, 0x8A,
        0x10, 0x78, 0xDF, 0x42, 0x99, 0x80, 0x3A, 0x68, 0x58, 0xF0, 0x41, 0xF9, 0xCB, 0x10, 0xE6, 0x35
    };
    static const u8 cert_hash_expected_dev[0x20] = {
        0x79, 0x21, 0xAE, 0x82, 0xC9, 0xDC, 0xF4, 0x11, 0x35, 0x13, 0x14, 0xF2, 0xFE, 0x2C, 0x67, 0x37,
        0x8C, 0x6A, 0x87, 0x2D, 0x25, 0x24, 0xF7, 0x1B, 0x3C, 0x00, 0x2B, 0x4D, 0x4A, 0x56, 0x84, 0x6F
    };

    static const char* const retail_issuers[] = {"Root-CA00000003", "Root-CA00000003-XS0000000c", "Root-CA00000003-CP0000000b"};
    static const char* const dev_issuers[] = {"Root-CA00000004", "Root-CA00000004-XS00000009", "Root-CA00000004-CP0000000a"};

    size_t size = CIA_CERT_SIZE;
    if (BuildRawCertBundleFromCertDb(ciacert, &size, !IS_DEVKIT ? retail_issuers : dev_issuers, 3) ||
        size != CIA_CERT_SIZE) {
        return 1;
    }

    // check the certificate hash
    u8 cert_hash[0x20];
    sha_quick(cert_hash, ciacert, CIA_CERT_SIZE, SHA256_MODE);
    if (memcmp(cert_hash, IS_DEVKIT ? cert_hash_expected_dev : cert_hash_expected, 0x20) != 0)
        return 1;

    return 0;
}

u32 BuildCiaMeta(CiaMeta* meta, void* exthdr, void* smdh) {
    // init metadata with all zeroes and core version
    memset(meta, 0x00, sizeof(CiaMeta));
    meta->core_version = 2;
    // copy dependencies from extheader
    if (exthdr) memcpy(meta->dependencies, ((NcchExtHeader*) exthdr)->dependencies, sizeof(meta->dependencies));
    // copy smdh (icon file in exefs)
    if (smdh) memcpy(meta->smdh, smdh, sizeof(meta->smdh));
    return 0;
}

u32 BuildCiaHeader(CiaHeader* header, u32 ticket_size) {
    memset(header, 0, sizeof(CiaHeader));
    // sizes in header - fill only known sizes, others zero
    header->size_header = sizeof(CiaHeader);
    header->size_cert = CIA_CERT_SIZE;
    header->size_ticket = ticket_size;
    header->size_tmd = 0;
    header->size_meta = 0;
    header->size_content = 0;
    return 0;
}

u32 DecryptCiaContentSequential(void* data, u32 size, u8* ctr, const u8* titlekey) {
    // WARNING: size and offset of data have to be a multiple of 16
    u8 tik[16] __attribute__((aligned(32)));
    u32 mode = AES_CNT_TITLEKEY_DECRYPT_MODE;
    memcpy(tik, titlekey, 16);
    setup_aeskey(0x11, tik);
    use_aeskey(0x11);
    cbc_decrypt(data, data, size / 16, mode, ctr);
    return 0;
}

u32 EncryptCiaContentSequential(void* data, u32 size, u8* ctr, const u8* titlekey) {
    // WARNING: size and offset of data have to be a multiple of 16
    u8 tik[16] __attribute__((aligned(32)));
    u32 mode = AES_CNT_TITLEKEY_ENCRYPT_MODE;
    memcpy(tik, titlekey, 16);
    setup_aeskey(0x11, tik);
    use_aeskey(0x11);
    cbc_encrypt(data, data, size / 16, mode, ctr);
    return 0;
}
