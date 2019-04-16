#include "cert.h"
#include "ff.h"

u32 LoadCertFromCertDb(u64 offset, Certificate* cert, u32* mod, u32* exp) {
	Certificate cert_local;
    FIL db;
    UINT bytes_read;

    // not much in terms of error checking here
    if (f_open(&db, "1:/dbs/certs.db", FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return 1;
    f_lseek(&db, offset);
    if (!cert) cert = &cert_local;
    f_read(&db, cert, CERT_SIZE, &bytes_read);
    f_close(&db);

    if (mod) memcpy(mod, cert->mod, 0x100);
    if (exp) *exp = getle32(cert->exp);

    return 0;
}
