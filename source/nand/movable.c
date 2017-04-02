#include "vff.h"
#include "sha.h"
#include "movable.h"

u8 GetMovableKeyY(const char* path, u8* keyY) {
    UINT br;
    if ((fvx_qread(path, keyY, 0x110, 0x10, &br) != FR_OK) || (br != 0x10))
      return 1;
    
    return 0;
}

u8 GetMovableID0(const char* path, char* id0) {
  u8 keyY[16];
  if(GetMovableKeyY(path, keyY) != 0)
      return 1;
  
  u32 sha256sum[8];
  sha_quick(sha256sum, keyY, 0x10, SHA256_MODE);
  
  snprintf(id0, 33, "%08lx%08lx%08lx%08lx",
      sha256sum[0], sha256sum[1], sha256sum[2], sha256sum[3]);
  return 0;
}