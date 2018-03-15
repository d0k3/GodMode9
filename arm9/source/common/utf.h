#pragma once

#include "common.h"

// most of the code here shamelessly stolen from:
// https://github.com/smealum/ctrulib/tree/bd34fd59dbf0691e2dba76be65f260303d8ccec7/libctru/source/util/utf
int utf16_to_utf8(u8 *out, const u16 *in, int len_out, int len_in);
int utf8_to_utf16(u16 *out, const u8 *in, int len_out, int len_in);
