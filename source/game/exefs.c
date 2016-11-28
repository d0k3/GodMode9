#include "exefs.h"

u32 ValidateExeFsHeader(ExeFsHeader* exefs, u32 size) {
    u32 data_size = 0;
    for (u32 i = 0; i < 10; i++) {
        ExeFsFileHeader* file = exefs->files + i;
        if ((file->offset == 0) && (file->size == 0))
            continue;
        if (file->offset < data_size)
            return 1; // overlapping data, failed
        data_size = file->offset + file->size;
    }
    if (size && (data_size > (size - sizeof(ExeFsHeader)))) // exefs header not included in table
        return 1;
    return 0;
}
