#pragma once

#include "common.h"

#define CERT_MAX_SIZE          (sizeof(CertificateSignature) + 0x23C + sizeof(CertificateBody) + 0x238)

#define CERT_RSA4096_SIG_SIZE  (sizeof(CertificateSignature) + 0x23C)
#define CERT_RSA2048_SIG_SIZE  (sizeof(CertificateSignature) + 0x13C)
#define CERT_ECC_SIG_SIZE      (sizeof(CertificateSignature) + 0x7C)
#define CERT_RSA4096_BODY_SIZE (sizeof(CertificateBody) + 0x238)
#define CERT_RSA2048_BODY_SIZE (sizeof(CertificateBody) + 0x138)
#define CERT_ECC_BODY_SIZE     (sizeof(CertificateBody) + 0x78)

// from: http://3dbrew.org/wiki/Certificates
// all numbers in big endian
typedef struct {
    u8 sig_type[4];
    u8 signature[];
} PACKED_ALIGN(1) CertificateSignature;

typedef struct {
    char issuer[0x40];
    u8 keytype[4];
    char name[0x40];
    u8 expiration[4];
    u8 pub_key_data[];
} PACKED_ALIGN(1) CertificateBody;

typedef struct {
    CertificateSignature* sig;
    CertificateBody* data;
} Certificate;

bool Certificate_IsValid(const Certificate* cert);
bool Certificate_IsRSA(const Certificate* cert);
bool Certificate_IsECC(const Certificate* cert);
u32 Certificate_GetSignatureSize(const Certificate* cert, u32* size);
u32 Certificate_GetModulusSize(const Certificate* cert, u32* size);
u32 Certificate_GetModulus(const Certificate* cert, void* mod);
u32 Certificate_GetExponent(const Certificate* cert, void* exp);
u32 Certificate_GetEccSingleCoordinateSize(const Certificate* cert, u32* size);
u32 Certificate_GetEccXY(const Certificate* cert, void* X, void* Y);
u32 Certificate_GetSignatureChunkSize(const Certificate* cert, u32* size);
u32 Certificate_GetDataChunkSize(const Certificate* cert, u32* size);
u32 Certificate_GetFullSize(const Certificate* cert, u32* size);
u32 Certificate_AllocCopyOut(const Certificate* cert, Certificate* out_cert);
u32 Certificate_RawCopy(const Certificate* cert, void* raw);
u32 Certificate_Cleanup(Certificate* cert);

u32 LoadCertFromCertDb(Certificate* cert, const char* issuer);
u32 BuildRawCertBundleFromCertDb(void* rawout, size_t* size, const char* const* cert_issuers, int count);
