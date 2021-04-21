#include "cert.h"
#include "disadiff.h"

typedef struct {
    char magic[4]; // "CERT"
    u8 unk[4]; // afaik, always 0
    u8 used_size[4]; // size used after this header
    u8 garbage[4]; // literally garbage values
} PACKED_STRUCT CertsDbPartitionHeader;

static void GetCertDBPath(char* path, bool emunand) {
    path[0] = emunand ? '4' : '1';
    strcpy(&path[1], ":/dbs/certs.db");
}

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

u32 Certificate_RawCopy(const Certificate* cert, void* raw) {
    if (!raw || !Certificate_IsValid(cert)) return 1;

    u32 sig_size = _Certificate_GetSignatureChunkSizeFromType(getbe32(cert->sig->sig_type));
    u32 data_size = _Certificate_GetDataChunkSizeFromType(getbe32(cert->data->keytype));

    if (sig_size == 0 || data_size == 0)
        return 1;

    memcpy(raw, cert->sig, sig_size);
    memcpy(&((u8*)raw)[sig_size], cert->data, data_size);

    return 0;
}

u32 Certificate_Cleanup(Certificate* cert) {
    if (!cert) return 1;

    free(cert->sig);
    free(cert->data);
    cert->sig = NULL;
    cert->data = NULL;

    return 0;
}

u32 LoadCertFromCertDb(bool emunand, Certificate* cert, const char* issuer) {
    if (!issuer || !cert) return 1;

    Certificate cert_local = {NULL, NULL};

    char path[16];
    GetCertDBPath(path, emunand);

    DisaDiffRWInfo info;
    u8* cache = NULL;
    if (GetDisaDiffRWInfo(path, &info, false) != 0) return 1;
    cache = malloc(info.size_dpfs_lvl2);
    if (!cache) return 1;
    if (BuildDisaDiffDpfsLvl2Cache(path, &info, cache, info.size_dpfs_lvl2) != 0) {
        free(cache);
        return 1;
    }

    CertsDbPartitionHeader header;

    if (ReadDisaDiffIvfcLvl4(path, &info, 0, sizeof(CertsDbPartitionHeader), &header) != sizeof(CertsDbPartitionHeader)) {
        free(cache);
        return 1;
    }

    if (getbe32(header.magic) != 0x43455254 /* 'CERT' */ ||
      getbe32(header.unk) != 0 ||
      getle32(header.used_size) & 0xFF) {
        free(cache);
        return 1;
    }

    u32 offset = sizeof(CertsDbPartitionHeader);
    u32 max_offset = getle32(header.used_size) + sizeof(CertsDbPartitionHeader);

    u32 ret = 1;

    // certs.db has no filesystem.. its pretty plain, certificates after another
    // but also, certificates are not equally sized
    // so most cases of bad data, leads to giving up
    while (offset < max_offset) {
        char full_issuer[0x41];
        u8 sig_type_data[4];
        u8 keytype_data[4];

        if (offset + 4 > max_offset) break;

        if (ReadDisaDiffIvfcLvl4(path, &info, offset, 4, sig_type_data) != 4)
            break;

        u32 sig_type = getbe32(sig_type_data);

        if (sig_type == 0x10002 || sig_type == 0x10005) break; // ECC signs not allowed on db

        u32 sig_size = _Certificate_GetSignatureChunkSizeFromType(sig_type);
        if (sig_size == 0) break;

        u32 keytype_off = offset + sig_size + offsetof(CertificateBody, keytype);
        if (keytype_off + 4 > max_offset) break;

        if (ReadDisaDiffIvfcLvl4(path, &info, keytype_off, 4, keytype_data) != 4)
            break;

        u32 keytype = getbe32(keytype_data);

        if (keytype == 2) break; // ECC keys not allowed on db

        u32 data_size = _Certificate_GetDataChunkSizeFromType(keytype);
        if (data_size == 0) break;

        u32 full_size = sig_size + data_size;
        if (offset + full_size > max_offset) break;

        cert_local.sig = (CertificateSignature*)malloc(sig_size);
        cert_local.data = (CertificateBody*)malloc(data_size);
        if (!cert_local.sig || !cert_local.data)
            break;

        if (ReadDisaDiffIvfcLvl4(path, &info, offset, sig_size, cert_local.sig) != sig_size)
            break;

        if (ReadDisaDiffIvfcLvl4(path, &info, offset + sig_size, data_size, cert_local.data) != data_size)
            break;

        if (!Certificate_IsValid(&cert_local))
            break;

        if (snprintf(full_issuer, 0x41, "%s-%s", cert_local.data->issuer, cert_local.data->name) > 0x40)
            break;

        if (!strcmp(full_issuer, issuer)) {
            ret = 0;
            break;
        }

        Certificate_Cleanup(&cert_local);

        offset += full_size;
    }

    if (ret) {
        Certificate_Cleanup(&cert_local);
    }
    
    *cert = cert_local;

    free(cache);
    return ret;
}
