#include "utf.h"

#define UTF_MAX_UNITS   256
#define ASCII_UNKNOWN   ((u8) '?')

// most of the code here shamelessly stolen from:
// https://github.com/smealum/ctrulib/tree/bd34fd59dbf0691e2dba76be65f260303d8ccec7/libctru/source/util/utf


int decode_utf8(u32 *out, const u8 *in)
{
    u8 code1, code2, code3, code4;

    code1 = *in++;
    if(code1 < 0x80)
    {
        /* 1-byte sequence */
        *out = code1;
        return 1;
    }
    else if(code1 < 0xC2)
    {
        return -1;
    }
    else if(code1 < 0xE0)
    {
        /* 2-byte sequence */
        code2 = *in++;
        if((code2 & 0xC0) != 0x80)
        {
            return -1;
        }

        *out = (code1 << 6) + code2 - 0x3080;
        return 2;
    }
    else if(code1 < 0xF0)
    {
        /* 3-byte sequence */
        code2 = *in++;
        if((code2 & 0xC0) != 0x80)
        {
            return -1;
        }
        if(code1 == 0xE0 && code2 < 0xA0)
        {
            return -1;
        }

        code3 = *in++;
        if((code3 & 0xC0) != 0x80)
        {
            return -1;
        }

        *out = (code1 << 12) + (code2 << 6) + code3 - 0xE2080;
        return 3;
    }
    else if(code1 < 0xF5)
    {
        /* 4-byte sequence */
        code2 = *in++;
        if((code2 & 0xC0) != 0x80)
        {
            return -1;
        }
        if(code1 == 0xF0 && code2 < 0x90)
        {
            return -1;
        }
        if(code1 == 0xF4 && code2 >= 0x90)
        {
            return -1;
        }

        code3 = *in++;
        if((code3 & 0xC0) != 0x80)
        {
            return -1;
        }

        code4 = *in++;
        if((code4 & 0xC0) != 0x80)
        {
            return -1;
        }

        *out = (code1 << 18) + (code2 << 12) + (code3 << 6) + code4 - 0x3C82080;
        return 4;
    }

    return -1;
}

int decode_utf16(u32 *out, const u16 *in)
{
    u16 code1, code2;

    code1 = *in++;
    if(code1 >= 0xD800 && code1 < 0xDC00)
    {
        /* surrogate pair */
        code2 = *in++;
        if(code2 >= 0xDC00 && code2 < 0xE000)
        {
            *out = (code1 << 10) + code2 - 0x35FDC00;
            return 2;
        }

        return -1;
    }

    *out = code1;
    return 1;
}

int encode_utf8(u8 *out, u32 in)
{
    if(in < 0x80)
    {
        if(out != NULL)
            *out++ = in;
        return 1;
    }
    else if(in < 0x800)
    {
        if(out != NULL)
        {
            *out++ = (in >> 6) + 0xC0;
            *out++ = (in & 0x3F) + 0x80;
        }
        return 2;
    }
    else if(in < 0x10000)
    {
        if(out != NULL)
        {
            *out++ = (in >> 12) + 0xE0;
            *out++ = ((in >> 6) & 0x3F) + 0x80;
            *out++ = (in & 0x3F) + 0x80;
        }
        return 3;
    }
    else if(in < 0x110000)
    {
        if(out != NULL)
        {
            *out++ = (in >> 18) + 0xF0;
            *out++ = ((in >> 12) & 0x3F) + 0x80;
            *out++ = ((in >> 6) & 0x3F) + 0x80;
            *out++ = (in & 0x3F) + 0x80;
        }
        return 4;
    }

    return -1;
}

int encode_utf16(u16 *out, u32 in)
{
    if(in < 0x10000)
    {
        if(out != NULL)
            *out++ = in;
        return 1;
    }
    else if(in < 0x110000)
    {
        if(out != NULL)
        {
            *out++ = (in >> 10) + 0xD7C0;
            *out++ = (in & 0x3FF) + 0xDC00;
        }
        return 2;
    }

    return -1;
}

int utf16_to_utf8(u8 *out, const u16 *in, int len_out, int len_in)
{
    int rc = 0;
    int units;
    u32 code;
    u8 encoded[4];

    do
    {
        units = decode_utf16(&code, in);
        if(units == -1)
            return -1;
        
        if (len_in >= units)
            len_in -= units;
        else return -1;

        if(code > 0)
        {
            in += units;

            units = encode_utf8(encoded, code);
            if(units == -1)
                return -1;

            if(out != NULL)
            {
                if(rc + units <= len_out)
                {
                    *out++ = encoded[0];
                    if(units > 1)
                        *out++ = encoded[1];
                    if(units > 2)
                        *out++ = encoded[2];
                    if(units > 3)
                        *out++ = encoded[3];
                }
            }

            if(UTF_MAX_UNITS - units >= rc)
                rc += units;
            else
                return -1;
        }
    } while(code > 0 && len_in > 0);

    return rc;
}

int utf8_to_utf16(u16 *out, const u8 *in, int len_out, int len_in)
{
    int rc = 0;
    int units;
    u32 code;
    u16 encoded[2];

    do
    {
        units = decode_utf8(&code, in);
        if(units == -1)
            return -1;
        
        if (len_in >= units)
            len_in -= units;
        else return -1;

        if(code > 0)
        {
            in += units;

            units = encode_utf16(encoded, code);
            if(units == -1)
                return -1;

            if(out != NULL)
            {
                if(rc + units <= len_out)
                {
                    *out++ = encoded[0];
                    if(units > 1)
                        *out++ = encoded[1];
                }
            }

            if(UTF_MAX_UNITS - units >= rc)
                rc += units;
            else
                return -1;
        }
    } while(code > 0 && len_in > 0);

    return rc;
}
