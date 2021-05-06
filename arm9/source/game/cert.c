#include "cert.h"
#include "disadiff.h"
#include "rsa.h"

typedef struct {
    char magic[4]; // "CERT"
    u8 unk[4]; // afaik, always 0
    u8 used_size[4]; // size used after this header
    u8 garbage[4]; // literally garbage values
} PACKED_STRUCT CertsDbPartitionHeader;

static inline void GetCertDBPath(char* path, bool emunand) {
    path[0] = emunand ? '4' : '1';
    strcpy(&path[1], ":/dbs/certs.db");
}

#define CERT_RETAIL_CA3_IDENT BIT(0)
#define CERT_RETAIL_XSc_IDENT BIT(1)
#define CERT_RETAIL_CPb_IDENT BIT(2)
#define CERT_DEV_CA4_IDENT    BIT(3)
#define CERT_DEV_XS9_IDENT    BIT(4)
#define CERT_DEV_CPa_IDENT    BIT(5)
#define CERT_NO_STORE_SPACE   (0xFF)

static struct {
    u32 loaded_certs_flg;
    u8 retail_CA3_raw[CERT_RSA4096_SIG_SIZE + CERT_RSA2048_BODY_SIZE];
    u8 retail_XSc_raw[CERT_RSA2048_SIG_SIZE + CERT_RSA2048_BODY_SIZE];
    u8 retail_CPb_raw[CERT_RSA2048_SIG_SIZE + CERT_RSA2048_BODY_SIZE];
    u8 dev_CA4_raw[CERT_RSA4096_SIG_SIZE + CERT_RSA2048_BODY_SIZE];
    u8 dev_XS9_raw[CERT_RSA2048_SIG_SIZE + CERT_RSA2048_BODY_SIZE];
    u8 dev_CPa_raw[CERT_RSA2048_SIG_SIZE + CERT_RSA2048_BODY_SIZE];
    Certificate retail_CA3;
    Certificate retail_XSc;
    Certificate retail_CPb;
    Certificate dev_CA4;
    Certificate dev_XS9;
    Certificate dev_CPa;
} _CommonCertsStorage = {
    0, // none loaded yet, ident defines used to say what's loaded
    {0}, {0}, {0}, {0}, {0}, {0}, // no data yet
    // cert structs pre-point already to raw certs
    {
        (CertificateSignature*)&_CommonCertsStorage.retail_CA3_raw[0],
        (CertificateBody*)&_CommonCertsStorage.retail_CA3_raw[CERT_RSA4096_SIG_SIZE]
    },
    {
        (CertificateSignature*)&_CommonCertsStorage.retail_XSc_raw[0],
        (CertificateBody*)&_CommonCertsStorage.retail_XSc_raw[CERT_RSA2048_SIG_SIZE]
    },
    {
        (CertificateSignature*)&_CommonCertsStorage.retail_CPb_raw[0],
        (CertificateBody*)&_CommonCertsStorage.retail_CPb_raw[CERT_RSA2048_SIG_SIZE]
    },
    {
        (CertificateSignature*)&_CommonCertsStorage.dev_CA4_raw[0],
        (CertificateBody*)&_CommonCertsStorage.dev_CA4_raw[CERT_RSA4096_SIG_SIZE]
    },
    {
        (CertificateSignature*)&_CommonCertsStorage.dev_XS9_raw[0],
        (CertificateBody*)&_CommonCertsStorage.dev_XS9_raw[CERT_RSA2048_SIG_SIZE]
    },
    {
        (CertificateSignature*)&_CommonCertsStorage.dev_CPa_raw[0],
        (CertificateBody*)&_CommonCertsStorage.dev_CPa_raw[CERT_RSA2048_SIG_SIZE]
    }
};

static inline void _Certificate_CleanupImpl(Certificate* cert);

bool Certificate_IsValid(const Certificate* cert) {
    if (!cert || !cert->sig || !cert->data)
        return false;

    u32 sig_type = getbe32(cert->sig->sig_type);
    if (sig_type < 0x10000 || sig_type > 0x10005)
        return false;

    u32 keytype = getbe32(cert->data->keytype);
    if (keytype > 2)
        return false;

    size_t issuer_len = strnlen(cert->data->issuer, 0x40);
    size_t name_len = strnlen(cert->data->name, 0x40);
    // if >= 0x40, cert can't fit as issuer for other objects later
    // since later objects using the certificate as their issuer will have them use it as certissuer-certname
    if (!issuer_len || !name_len || (issuer_len + name_len + 1) >= 0x40)
        return false;

    return true;
}

bool Certificate_IsRSA(const Certificate* cert) {
    if (!Certificate_IsValid(cert)) return false;
    if (getbe32(cert->data->keytype) >= 2) return false;
    return true;
}

bool Certificate_IsECC(const Certificate* cert) {
    if (!Certificate_IsValid(cert)) return false;
    if (getbe32(cert->data->keytype) != 2) return false;
    return true;
}

u32 Certificate_GetSignatureSize(const Certificate* cert, u32* size) {
    if (!size || !Certificate_IsValid(cert)) return 1;

    u32 sig_type = getbe32(cert->sig->sig_type);

    if (sig_type == 0x10000 || sig_type == 0x10003)
        *size = 0x200;
    else if (sig_type == 0x10001 || sig_type == 0x10004)
        *size = 0x100;
    else if (sig_type == 0x10002 || sig_type == 0x10005)
        *size = 0x3C;
    else
        return 1;

    return 0;
}

u32 Certificate_GetModulusSize(const Certificate* cert, u32* size) {
    if (!size || !Certificate_IsRSA(cert)) return 1;

    u32 keytype = getbe32(cert->data->keytype);

    if (keytype == 0)
        *size = 4096 / 8;
    else if (keytype == 1)
        *size = 2048 / 8;
    else return 1;

    return 0;
}

u32 Certificate_GetModulus(const Certificate* cert, void* mod) {
    u32 size;
    if (!mod || Certificate_GetModulusSize(cert, &size)) return 1;

    memcpy(mod, cert->data->pub_key_data, size);

    return 0;
}

u32 Certificate_GetExponent(const Certificate* cert, void* exp) {
    u32 size;
    if (!exp || Certificate_GetModulusSize(cert, &size)) return 1;

    memcpy(exp, &cert->data->pub_key_data[size], 4);

    return 0;
}

u32 Certificate_GetEccSingleCoordinateSize(const Certificate* cert, u32* size) {
    if (!size || !Certificate_IsECC(cert)) return 1;

    u32 keytype = getbe32(cert->data->keytype);

    if (keytype == 2)
        *size = 0x3C / 2;
    else return 1;

    return 0;
}

u32 Certificate_GetEccXY(const Certificate* cert, void* X, void* Y) {
    u32 size;
    if (!X || !Y || Certificate_GetEccSingleCoordinateSize(cert, &size)) return 1;

    memcpy(X, cert->data->pub_key_data, size);
    memcpy(Y, &cert->data->pub_key_data[size], size);

    return 0;
}

static inline u32 _Certificate_GetSignatureChunkSizeFromType(u32 sig_type) {
    if (sig_type == 0x10000 || sig_type == 0x10003)
        return CERT_RSA4096_SIG_SIZE;
    else if (sig_type == 0x10001 || sig_type == 0x10004)
        return CERT_RSA2048_SIG_SIZE;
    else if (sig_type == 0x10002 || sig_type == 0x10005)
        return CERT_ECC_SIG_SIZE;
    return 0;
}

u32 Certificate_GetSignatureChunkSize(const Certificate* cert, u32* size) {
    if (!size || !Certificate_IsValid(cert)) return 1;

    u32 _size = _Certificate_GetSignatureChunkSizeFromType(getbe32(cert->sig->sig_type));

    if (_size == 0) return 1;

    *size = _size;

    return 0;
}

static inline u32 _Certificate_GetDataChunkSizeFromType(u32 keytype) {
    if (keytype == 0)
        return CERT_RSA4096_BODY_SIZE;
    else if (keytype == 1)
        return CERT_RSA2048_BODY_SIZE;
    else if (keytype == 2)
        return CERT_ECC_BODY_SIZE;
    return 0;
}

u32 Certificate_GetDataChunkSize(const Certificate* cert, u32* size) {
    if (!size || !Certificate_IsValid(cert)) return 1;

    u32 _size = _Certificate_GetDataChunkSizeFromType(getbe32(cert->data->keytype));

    if (_size == 0) return 1;

    *size = _size;

    return 0;
}

u32 Certificate_GetFullSize(const Certificate* cert, u32* size) {
    if (!size || !Certificate_IsValid(cert)) return 1;

    u32 sig_size = _Certificate_GetSignatureChunkSizeFromType(getbe32(cert->sig->sig_type));
    u32 data_size = _Certificate_GetDataChunkSizeFromType(getbe32(cert->data->keytype));

    if (sig_size == 0 || data_size == 0)
        return 1;

    *size = sig_size + data_size;

    return 0;
}

static inline u32 _Certificate_KeytypeSignatureSize(u32 keytype) {
    if (keytype == 0)
        return 0x200;
    else if (keytype == 1)
        return 0x100;
    else if (keytype == 2)
        return 0x3C;
    return 0;
}

static inline u32 _Certificate_VerifyRSA4096(const Certificate* cert, const void* sig, const void* data, u32 data_size, bool sha256) {
    (void)cert; (void)sig; (void)data; (void)data_size; (void)sha256;
    return 2; // not implemented
}

// noipa, to avoid form of inlining, cloning, etc, to avoid the extra stack usage when unneeded
static __attribute__((noipa)) bool _Certificate_SetKey2048Misaligned(const Certificate* cert) {
    u32 mod[2048/8];
    u32 exp;

    memcpy(mod, cert->data->pub_key_data, 2048/8);
    exp = getle32(&cert->data->pub_key_data[2048/8]);

    return RSA_setKey2048(3, mod, exp);
}

static inline u32 _Certificate_VerifyRSA2048(const Certificate* cert, const void* sig, const void* data, u32 data_size, bool sha256) {
    if (!sha256)
        return 2; // not implemented

    int ret;

    if (((u32)&cert->data->pub_key_data[0]) & 0x3)
        ret = !_Certificate_SetKey2048Misaligned(cert);
    else
        ret = !RSA_setKey2048(3, (const u32*)(const void*)&cert->data->pub_key_data[0], getle32(&cert->data->pub_key_data[2048/8]));

    if (ret)
        return ret;

    return !RSA_verify2048(sig, data, data_size);
}

static inline u32 _Certificate_VerifyECC(const Certificate* cert, const void* sig, const void* data, u32 data_size, bool sha256) {
    (void)cert; (void)sig; (void)data; (void)data_size; (void)sha256;
    return 2; // not implemented
}

u32 Certificate_VerifySignatureBlock(const Certificate* cert, const void* sig, u32 sig_size, const void* data, u32 data_size, bool sha256) {
    if (!sig || !sig_size || (!data && data_size) || !Certificate_IsValid(cert))
        return 1;

    u32 keytype = getbe32(cert->data->keytype);
    u32 _sig_size = _Certificate_KeytypeSignatureSize(keytype);

    if (sig_size != _sig_size)
        return 1;

    if (keytype == 0)
        return _Certificate_VerifyRSA4096(cert, sig, data, data_size, sha256);
    if (keytype == 1)
        return _Certificate_VerifyRSA2048(cert, sig, data, data_size, sha256);
    if (keytype == 2)
        return _Certificate_VerifyECC(cert, sig, data, data_size, sha256);
    return 1;
}

static inline void* _Certificate_SafeRealloc(void* ptr, size_t size, size_t oldsize) {
    void* new_ptr;
    size_t min_size = min(oldsize, size);

    if ((u32)ptr >= (u32)&_CommonCertsStorage && (u32)ptr < (u32)&_CommonCertsStorage + sizeof(_CommonCertsStorage)) {
        new_ptr = malloc(size);
        if (new_ptr) memcpy(new_ptr, ptr, min_size);
    } else {
        new_ptr = realloc(ptr, size);
    }
    if (!new_ptr) return NULL;

    memset(&((u8*)new_ptr)[min_size], 0, size - min_size);
    return new_ptr;
}

// will reallocate memory for certificate signature and body to fit the max possible size.
// will also allocate an empty object if Certificate is NULL initialized.
// if certificate points to static storage, an allocated version will be created.
// if function fails, the Certificate, even if previously NULL initialized, still has to be passed to cleaned up after use.
u32 Certificate_MakeEditSafe(Certificate* cert) {
    if (!cert) return 1;
    bool isvalid = Certificate_IsValid(cert);
    if ((cert->sig || cert->data) && !isvalid) return 1;

    Certificate cert_local;

    u32 sig_size = isvalid ? _Certificate_GetSignatureChunkSizeFromType(getbe32(cert->sig->sig_type)) : 0;
    u32 data_size = isvalid ? _Certificate_GetDataChunkSizeFromType(getbe32(cert->data->keytype)) : 0;

    if (isvalid && (sig_size == 0 || data_size == 0)) return 1;

    cert_local.sig = _Certificate_SafeRealloc(cert->sig, CERT_RSA4096_SIG_SIZE, sig_size);
    if (!cert_local.sig) return 1;
    cert->sig = cert_local.sig;
    cert_local.data = _Certificate_SafeRealloc(cert->data, CERT_RSA4096_BODY_SIZE, data_size);
    if (!cert_local.data) return 1;
    cert->data = cert_local.data;

    return 0;
}

static u32 _Certificate_AllocCopyOutImpl(const Certificate* cert, Certificate* out_cert) {
    u32 sig_size = _Certificate_GetSignatureChunkSizeFromType(getbe32(cert->sig->sig_type));
    u32 data_size = _Certificate_GetDataChunkSizeFromType(getbe32(cert->data->keytype));

    if (sig_size == 0 || data_size == 0)
        return 1;

    out_cert->sig = (CertificateSignature*)malloc(sig_size);
    out_cert->data = (CertificateBody*)malloc(data_size);

    if (!out_cert->sig || !out_cert->data) {
        _Certificate_CleanupImpl(out_cert);
        return 1;
    }

    memcpy(out_cert->sig, cert->sig, sig_size);
    memcpy(out_cert->data, cert->data, data_size);

    return 0;
}

u32 Certificate_AllocCopyOut(const Certificate* cert, Certificate* out_cert) {
    if (!out_cert || !Certificate_IsValid(cert)) return 1;

    return _Certificate_AllocCopyOutImpl(cert, out_cert);
}

static u32 _Certificate_RawCopyImpl(const Certificate* cert, void* raw) {
    u32 sig_size = _Certificate_GetSignatureChunkSizeFromType(getbe32(cert->sig->sig_type));
    u32 data_size = _Certificate_GetDataChunkSizeFromType(getbe32(cert->data->keytype));

    if (sig_size == 0 || data_size == 0)
        return 1;

    memcpy(raw, cert->sig, sig_size);
    memcpy(&((u8*)raw)[sig_size], cert->data, data_size);

    return 0;
}

u32 Certificate_RawCopy(const Certificate* cert, void* raw) {
    if (!raw || !Certificate_IsValid(cert)) return 1;

    return _Certificate_RawCopyImpl(cert, raw);
}

// ptr free check, to not free if ptr is pointing to static storage!!
static inline void _Certificate_SafeFree(void* ptr) {
    if ((u32)ptr >= (u32)&_CommonCertsStorage && (u32)ptr < (u32)&_CommonCertsStorage + sizeof(_CommonCertsStorage))
        return;

    free(ptr);
}

static inline void _Certificate_CleanupImpl(Certificate* cert) {
    _Certificate_SafeFree(cert->sig);
    _Certificate_SafeFree(cert->data);
    cert->sig = NULL;
    cert->data = NULL;
}

u32 Certificate_Cleanup(Certificate* cert) {
    if (!cert) return 1;

    _Certificate_CleanupImpl(cert);

    return 0;
}

static u32 _Issuer_To_StorageIdent(const char* issuer) {
    if (strncmp(issuer, "Root-CA0000000", 14) != 0)
        return CERT_NO_STORE_SPACE;

    if (issuer[14] == '3') { // retail
        if (issuer[15] == 0)
            return CERT_RETAIL_CA3_IDENT;
        if (issuer[15] != '-')
            return CERT_NO_STORE_SPACE;
        if (!strcmp(&issuer[16], "XS0000000c"))
            return CERT_RETAIL_XSc_IDENT;
        if (!strcmp(&issuer[16], "CP0000000b"))
            return CERT_RETAIL_CPb_IDENT;
    }

    if (issuer[14] == '4') { // dev
        if (issuer[15] == 0)
            return CERT_DEV_CA4_IDENT;
        if (issuer[15] != '-')
            return CERT_NO_STORE_SPACE;
        if (!strcmp(&issuer[16], "XS00000009"))
            return CERT_DEV_XS9_IDENT;
        if (!strcmp(&issuer[16], "CP0000000a"))
            return CERT_DEV_CPa_IDENT;
    }

    return CERT_NO_STORE_SPACE;
}

static bool _LoadFromCertStorage(Certificate* cert, u32 ident) {
    if (ident == CERT_NO_STORE_SPACE)
        return false;

    Certificate* _cert = NULL;

    switch (ident) {
    case CERT_RETAIL_CA3_IDENT:
        if (_CommonCertsStorage.loaded_certs_flg & CERT_RETAIL_CA3_IDENT)
            _cert = &_CommonCertsStorage.retail_CA3;
        break;
    case CERT_RETAIL_XSc_IDENT:
        if (_CommonCertsStorage.loaded_certs_flg & CERT_RETAIL_XSc_IDENT)
            _cert = &_CommonCertsStorage.retail_XSc;
        break;
    case CERT_RETAIL_CPb_IDENT:
        if (_CommonCertsStorage.loaded_certs_flg & CERT_RETAIL_CPb_IDENT)
            _cert = &_CommonCertsStorage.retail_CPb;
        break;
    case CERT_DEV_CA4_IDENT:
        if (_CommonCertsStorage.loaded_certs_flg & CERT_DEV_CA4_IDENT)
            _cert = &_CommonCertsStorage.dev_CA4;
        break;
    case CERT_DEV_XS9_IDENT:
        if (_CommonCertsStorage.loaded_certs_flg & CERT_DEV_XS9_IDENT)
            _cert = &_CommonCertsStorage.dev_XS9;
        break;
    case CERT_DEV_CPa_IDENT:
        if (_CommonCertsStorage.loaded_certs_flg & CERT_DEV_CPa_IDENT)
            _cert = &_CommonCertsStorage.dev_CPa;
        break;
    default:
        break;
    }

    if (!_cert)
        return false;

    *cert = *_cert;
    return true;
}

static void _SaveToCertStorage(const Certificate* cert, u32 ident) {
    if (ident == CERT_NO_STORE_SPACE)
        return;

    Certificate* _cert = NULL;
    u8* raw_space = NULL;
    u32 raw_size = 0;

    switch (ident) {
    case CERT_RETAIL_CA3_IDENT:
        if (!(_CommonCertsStorage.loaded_certs_flg & CERT_RETAIL_CA3_IDENT)) {
            _cert = &_CommonCertsStorage.retail_CA3;
            raw_space = &_CommonCertsStorage.retail_CA3_raw[0];
            raw_size = sizeof(_CommonCertsStorage.retail_CA3_raw);
        }
        break;
    case CERT_RETAIL_XSc_IDENT:
        if (!(_CommonCertsStorage.loaded_certs_flg & CERT_RETAIL_XSc_IDENT)) {
            _cert = &_CommonCertsStorage.retail_XSc;
            raw_space = &_CommonCertsStorage.retail_XSc_raw[0];
            raw_size = sizeof(_CommonCertsStorage.retail_XSc_raw);
        }
        break;
    case CERT_RETAIL_CPb_IDENT:
        if (!(_CommonCertsStorage.loaded_certs_flg & CERT_RETAIL_CPb_IDENT)) {
            _cert = &_CommonCertsStorage.retail_CPb;
            raw_space = &_CommonCertsStorage.retail_CPb_raw[0];
            raw_size = sizeof(_CommonCertsStorage.retail_CPb_raw);
        }
        break;
    case CERT_DEV_CA4_IDENT:
        if (!(_CommonCertsStorage.loaded_certs_flg & CERT_DEV_CA4_IDENT)) {
            _cert = &_CommonCertsStorage.dev_CA4;
            raw_space = &_CommonCertsStorage.dev_CA4_raw[0];
            raw_size = sizeof(_CommonCertsStorage.dev_CA4_raw);
        }
        break;
    case CERT_DEV_XS9_IDENT:
        if (!(_CommonCertsStorage.loaded_certs_flg & CERT_DEV_XS9_IDENT)) {
            _cert = &_CommonCertsStorage.dev_XS9;
            raw_space = &_CommonCertsStorage.dev_XS9_raw[0];
            raw_size = sizeof(_CommonCertsStorage.dev_XS9_raw);
        }
        break;
    case CERT_DEV_CPa_IDENT:
        if (!(_CommonCertsStorage.loaded_certs_flg & CERT_DEV_CPa_IDENT)) {
            _cert = &_CommonCertsStorage.dev_CPa;
            raw_space = &_CommonCertsStorage.dev_CPa_raw[0];
            raw_size = sizeof(_CommonCertsStorage.dev_CPa_raw);
        }
        break;
    default:
        break;
    }

    if (!_cert || !raw_space || !raw_size)
        return;

    u32 sig_size = _Certificate_GetSignatureChunkSizeFromType(getbe32(cert->sig->sig_type));
    u32 data_size = _Certificate_GetDataChunkSizeFromType(getbe32(cert->data->keytype));

    if (sig_size == 0 || data_size == 0)
        return;

    if (sig_size + data_size != raw_size)
        return;

    if (!_Certificate_RawCopyImpl(cert, raw_space)) {
        _CommonCertsStorage.loaded_certs_flg |= ident;
    }
}

// grumble grumble, gotta avoid repeated code when possible or at least if significant enough

static u32 _DisaOpenCertDb(char (*path)[16], bool emunand, DisaDiffRWInfo* info, u8** cache, u32* offset, u32* max_offset) {
    GetCertDBPath(*path, emunand);

    u8* _cache = NULL;
    if (GetDisaDiffRWInfo(*path, info, false) != 0) return 1;
    _cache = (u8*)malloc(info->size_dpfs_lvl2);
    if (!_cache) return 1;
    if (BuildDisaDiffDpfsLvl2Cache(*path, info, _cache, info->size_dpfs_lvl2) != 0) {
        free(_cache);
        return 1;
    }

    CertsDbPartitionHeader header;

    if (ReadDisaDiffIvfcLvl4(*path, info, 0, sizeof(CertsDbPartitionHeader), &header) != sizeof(CertsDbPartitionHeader)) {
        free(_cache);
        return 1;
    }

    if (getbe32(header.magic) != 0x43455254 /* 'CERT' */ ||
      getbe32(header.unk) != 0 ||
      getle32(header.used_size) & 0xFF) {
        free(_cache);
        return 1;
    }

    *cache = _cache;

    *offset = sizeof(CertsDbPartitionHeader);
    *max_offset = getle32(header.used_size) + sizeof(CertsDbPartitionHeader);

    return 0;
}

static u32 _ProcessNextCertDbEntry(const char* path, DisaDiffRWInfo* info, Certificate* cert, u32 *full_size, char (*full_issuer)[0x41], u32* offset, u32 max_offset) {
    u8 sig_type_data[4];
    u8 keytype_data[4];

    if (*offset + 4 > max_offset) return 1;

    if (ReadDisaDiffIvfcLvl4(path, info, *offset, 4, sig_type_data) != 4)
        return 1;

    u32 sig_type = getbe32(sig_type_data);

    if (sig_type == 0x10002 || sig_type == 0x10005) return 1; // ECC signs not allowed on db

    u32 sig_size = _Certificate_GetSignatureChunkSizeFromType(sig_type);
    if (sig_size == 0) return 1;

    u32 keytype_off = *offset + sig_size + offsetof(CertificateBody, keytype);
    if (keytype_off + 4 > max_offset) return 1;

    if (ReadDisaDiffIvfcLvl4(path, info, keytype_off, 4, keytype_data) != 4)
        return 1;

    u32 keytype = getbe32(keytype_data);

    if (keytype == 2) return 1; // ECC keys not allowed on db

    u32 data_size = _Certificate_GetDataChunkSizeFromType(keytype);
    if (data_size == 0) return 1;

    *full_size = sig_size + data_size;
    if (*offset + *full_size > max_offset) return 1;

    cert->sig = (CertificateSignature*)malloc(sig_size);
    cert->data = (CertificateBody*)malloc(data_size);
    if (!cert->sig || !cert->data)
        return 1;

    if (ReadDisaDiffIvfcLvl4(path, info, *offset, sig_size, cert->sig) != sig_size)
        return 1;

    if (ReadDisaDiffIvfcLvl4(path, info, *offset + sig_size, data_size, cert->data) != data_size)
        return 1;

    if (!Certificate_IsValid(cert))
        return 1;

    if (snprintf(*full_issuer, 0x41, "%s-%s", cert->data->issuer, cert->data->name) > 0x40)
        return 1;

    return 0;
}

// certificates returned by this call are not to be deemed safe to edit, pointers or pointed data
u32 LoadCertFromCertDb(Certificate* cert, const char* issuer) {
    if (!issuer || !cert) return 1;

    u32 _ident = _Issuer_To_StorageIdent(issuer);
    if (_LoadFromCertStorage(cert, _ident)) {
        return 0;
    }

    int ret = 1;

    for (int i = 0; i < 2 && ret; ++i) {
        Certificate cert_local = CERTIFICATE_NULL_INIT;

        char path[16];
        DisaDiffRWInfo info;
        u8* cache;

        u32 offset, max_offset;

        if (_DisaOpenCertDb(&path, i ? true : false, &info, &cache, &offset, &max_offset))
            return 1;

        // certs.db has no filesystem.. its pretty plain, certificates after another
        // but also, certificates are not equally sized
        // so most cases of bad data, leads to giving up
        while (offset < max_offset) {
            char full_issuer[0x41];
            u32 full_size;

            if (_ProcessNextCertDbEntry(path, &info, &cert_local, &full_size, &full_issuer, &offset, max_offset))
                break;

            if (!strcmp(full_issuer, issuer)) {
                ret = 0;
                break;
            }

            _Certificate_CleanupImpl(&cert_local);

            offset += full_size;
        }

        if (ret) {
            _Certificate_CleanupImpl(&cert_local);
        } else {
            *cert = cert_local;
            _SaveToCertStorage(&cert_local, _ident);
        }

        free(cache);
    }

    return ret;
}

// I dont expect many certs on a cert bundle, so I'll cap it to 8
u32 BuildRawCertBundleFromCertDb(void* rawout, size_t* size, const char* const* cert_issuers, int count) {
    if (!rawout || !size || !cert_issuers || count < 0 || count > 8) return 1;
    if (!*size && count) return 1;
    if (!count) { // *shrug*
        *size = 0;
        return 0;
    }

    for (int i = 0; i < count; ++i) {
        if (!cert_issuers[i])
            return 1;
    }

    Certificate certs[8];
    u8 certs_loaded = 0;

    memset(certs, 0, sizeof(certs));

    int loaded_count = 0;

    // search static storage first
    for (int i = 0; i < count; ++i) {
        u32 _ident = _Issuer_To_StorageIdent(cert_issuers[i]);
        if (_LoadFromCertStorage(&certs[i], _ident)) {
            certs_loaded |= BIT(i);
            ++loaded_count;
        }
    }

    int ret = 0;

    for (int i = 0; i < 2 && loaded_count != count && !ret; ++i) {
        Certificate cert_local = CERTIFICATE_NULL_INIT;

        char path[16];
        DisaDiffRWInfo info;
        u8* cache;

        u32 offset, max_offset;

        if (_DisaOpenCertDb(&path, i ? true : false, &info, &cache, &offset, &max_offset))
            continue;

        while (offset < max_offset) {
            char full_issuer[0x41];
            u32 full_size;

            if (_ProcessNextCertDbEntry(path, &info, &cert_local, &full_size, &full_issuer, &offset, max_offset))
                break;

            for (int j = 0; j < count; j++) {
                if (certs_loaded & BIT(j)) continue;
                if (!strcmp(full_issuer, cert_issuers[j])) {
                    ret = _Certificate_AllocCopyOutImpl(&cert_local, &certs[j]);
                    if (ret) break;
                    certs_loaded |= BIT(j);
                    ++loaded_count;
                }
            }

            // while at it, try to save to static storage, if applicable
            u32 _ident = _Issuer_To_StorageIdent(full_issuer);
            _SaveToCertStorage(&cert_local, _ident);

            _Certificate_CleanupImpl(&cert_local);

            if (loaded_count == count || ret) // early exit
                break;

            offset += full_size;
        }

        free(cache);
    }

    if (!ret && loaded_count == count) {
        u8* out = (u8*)rawout;
        size_t limit = *size, written = 0;

        for (int i = 0; i < count; ++i) {
            u32 sig_size = _Certificate_GetSignatureChunkSizeFromType(getbe32(certs[i].sig->sig_type));
            u32 data_size = _Certificate_GetDataChunkSizeFromType(getbe32(certs[i].data->keytype));

            if (sig_size == 0 || data_size == 0) {
                ret = 1;
                break;
            }

            u32 full_size = sig_size + data_size;

            if (written + full_size > limit) {
                ret = 1;
                break;
            }

            if (_Certificate_RawCopyImpl(&certs[i], out)) {
                ret = 1;
                break;
            }

            out += full_size;
            written += full_size;
        }

        if (!ret)
            *size = written;
    } else {
        ret = 1;
    }

    for (int i = 0; i < count; ++i) {
        if (certs_loaded & BIT(i))
            _Certificate_CleanupImpl(&certs[i]);
    }

    return ret;
}
