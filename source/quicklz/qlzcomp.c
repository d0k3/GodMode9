#include "qlzcomp.h"
#include "quicklz.h"

u32 QlzCompress(void* out, const void* in, u32 size_in) {
    if (TEMP_BUFFER_SIZE < sizeof(qlz_state_compress)) return 1;
    qlz_state_compress *state_compress = (qlz_state_compress*) (void*) TEMP_BUFFER;
    memset(state_compress, 0, sizeof(qlz_state_compress));
    return (qlz_compress(in, out, size_in, state_compress) > 0) ? 0 : 1;
}

u32 QlzDecompress(void* out, const void* in, u32 size_out) {
    if (TEMP_BUFFER_SIZE < sizeof(qlz_state_decompress)) return 1;
    if (size_out && (qlz_size_decompressed(in) != size_out)) return 1;
    qlz_state_decompress *state_decompress = (qlz_state_decompress*) (void*) TEMP_BUFFER;
    memset(state_decompress, 0, sizeof(qlz_state_decompress));
    return (qlz_decompress(in, out, state_decompress) > 0) ? 0 : 1;
}
