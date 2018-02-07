#include "codelzss.h"

#define CODE_COMP_SIZE(f)   ((f)->off_size_comp & 0xFFFFFF)
#define CODE_COMP_END(f)    ((int) CODE_COMP_SIZE(f) - (int) (((f)->off_size_comp >> 24) % 0xFF))
#define CODE_DEC_SIZE(f)    (CODE_COMP_SIZE(f) + (f)->addsize_dec)

#define CODE_SEG_OFFSET(s)  (((s) & 0x0FFF) + 2)
#define CODE_SEG_SIZE(s)    ((((s) >> 12) & 0xF) + 3)

typedef struct {
    u32 off_size_comp; // 0xOOSSSSSS, where O == reverse offset and S == size
    u32 addsize_dec; // decompressed size - compressed size
} __attribute__((packed)) CodeLzssFooter;


u32 GetCodeLzssUncompressedSize(void* footer, u32 comp_size) {
    if (comp_size < sizeof(CodeLzssFooter)) return 0;
    
    CodeLzssFooter* f = (CodeLzssFooter*) footer;
    if ((CODE_COMP_SIZE(f) > comp_size) || (CODE_COMP_END(f) < 0)) return 0;
    
    return CODE_DEC_SIZE(f) + (comp_size - CODE_COMP_SIZE(f));
}

// see: https://github.com/zoogie/DSP1/blob/master/source/main.c#L44
u32 DecompressCodeLzss(u8* code, u32* code_size, u32 max_size) {
    u8* data_start = code;
    u8* comp_start = data_start;
    
    // get footer, fix comp_start offset
    if ((*code_size < sizeof(CodeLzssFooter)) || (*code_size > max_size)) return 1;
    CodeLzssFooter* footer = (CodeLzssFooter*) (void*) (data_start + *code_size - sizeof(CodeLzssFooter));
    if (CODE_COMP_SIZE(footer) <= *code_size) comp_start += *code_size - CODE_COMP_SIZE(footer);
    else return 1;
    
    // more sanity checks
    if ((CODE_COMP_END(footer) < 0) || (CODE_DEC_SIZE(footer) > max_size))
        return 1; // not reverse LZSS compressed code or too big uncompressed
    
    // set pointers
    u8* data_end = (u8*) comp_start + CODE_DEC_SIZE(footer);
    u8* ptr_in = (u8*) comp_start + CODE_COMP_END(footer);
    u8* ptr_out = data_end;
    
    // main decompression loop
    while ((ptr_in > comp_start) && (ptr_out > comp_start)) {
        // sanity check
        if (ptr_out < ptr_in) return 1;
        
        // read and process control byte
        u8 ctrlbyte = *(--ptr_in);
        for (int i = 7; i >= 0; i--) {
            // end conditions met?
            if ((ptr_in <= comp_start) || (ptr_out <= comp_start))
                break;
            
            // process control byte
            if ((ctrlbyte >> i) & 0x1) {
                // control bit set, read segment code
                ptr_in -= 2;
                u16 seg_code = getle16(ptr_in);
                if (ptr_in < comp_start) return 1; // corrupted code
                u32 seg_off = CODE_SEG_OFFSET(seg_code);
                u32 seg_len = CODE_SEG_SIZE(seg_code);
                
                // sanity check for output pointer
                if ((ptr_out - seg_len < comp_start) || (ptr_out + seg_off >= data_end))
                    return 1;
                
                // copy data to the correct place
                for (u32 c = 0; c < seg_len; c++) {
                    u8 byte = *(ptr_out + seg_off);
                    *(--ptr_out) = byte;
                }
            } else {
                // sanity check for both pointers
                if ((ptr_out == comp_start) || (ptr_in == comp_start))
                    return 1;
                
                // control bit not set, copy byte verbatim
                *(--ptr_out) = *(--ptr_in);
            }
        }
    }
    
    // check pointers
    if ((ptr_in != comp_start) || (ptr_out != comp_start))
        return 1;
    
    // all fine if arriving here - return the result
    *code_size = data_end - data_start;
    return 0;
}
