#include "common.h"
#include "i2c.h"
#include "itcm.h"
#include "region.h"
#include "unittype.h"
#include "essentials.h" // For SecureInfo / movable
#include "sdmmc.h" // for NAND / SD CID
#include "vff.h"
#include "sha.h"
#include <ctype.h>
#include <limits.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

// Table of system models.
// https://www.3dbrew.org/wiki/Cfg:GetSystemModel#System_Model_Values
static const struct {
    char name[12];
    char product_code[4];
} s_modelNames[] = {
    { "Old 3DS",    "CTR" }, // 0
    { "Old 3DS XL", "SPR" }, // 1
    { "New 3DS",    "KTR" }, // 2
    { "Old 2DS",    "FTR" }, // 3
    { "New 3DS XL", "RED" }, // 4
    { "New 2DS XL", "JAN" }, // 5
};
STATIC_ASSERT(countof(s_modelNames) == NUM_MODELS);

// Table of sales regions.
static const struct {
    char serial_char;
    const char* name;
} s_salesRegions[] = {
    // Typical regions.
    { 'J', "Japan" },
    { 'W', "Americas" },    // "W" = worldwide?
    { 'E', "Europe" },
    { 'C', "China" },
    { 'K', "Korea" },
    { 'T', "Taiwan" },
    // Manufacturing regions that have another region's region lock.
    { 'S', "Middle East" }, // "S" = Saudi Arabia?  Singapore?  (Southeast Asia included.)
    { 'A', "Australia" },
};

// Structure of system information.
typedef struct _SysInfo {
    // Internal data to pass among these subroutines.
    uint8_t int_model;

    // From hardware information.
    char model[15 + 1];
    char product_code[3 + 1];
    // From OTP.
    char soc_date[19 + 1];
    // From SecureInfo_A/B
    char sub_model[15 + 1];
    char serial[15 + 1];
    char system_region[15 + 1];
    char sales_region[15 + 1];
    // From movable.sed
    char friendcodeseed[16 + 1];
    char movablekeyy[32 + 1];
    char nand_id0[32 + 1];
    // from emmc
    char nand_cid[32 + 1];
    char sd_cid[32 + 1];
    char nand_id1[32 + 1];
    // From twln:
    char assembly_date[19 + 1];
    char original_firmware[15 + 1];
    char preinstalled_titles[16][4];
} SysInfo;


// Read hardware information.
void GetSysInfo_Hardware(SysInfo* info, char nand_drive) {
    (void) nand_drive;

    info->int_model = 0xFF;
    strncpy(info->model, "<unknown>", countof(info->model));
    strncpy(info->product_code, "???", countof(info->product_code));

    // Get MCU system information.
    uint8_t mcu_sysinfo[0x13];
    if (I2C_readRegBuf(I2C_DEV_MCU, 0x7F, mcu_sysinfo, sizeof(mcu_sysinfo))) {
        // System model.
        info->int_model = mcu_sysinfo[0x09];
        if (info->int_model < NUM_MODELS) {
            strncpy(info->model, s_modelNames[info->int_model].name, countof(info->model));
            strncpy(info->product_code, s_modelNames[info->int_model].product_code, countof(info->product_code));
        }
    }
}


// Read OTP.
void GetSysInfo_OTP(SysInfo* info, char nand_drive) {
    (void) nand_drive;

    strncpy(info->soc_date, "<unknown>", countof(info->soc_date));

    const Otp* otp = &ARM9_ITCM->otp;

    // SoC manufacturing date, we think.
    do {
        unsigned year = otp->timestampYear + 1900;

        if (year < 2000)
            break;
        if ((otp->timestampMonth == 0) || (otp->timestampMonth > 12))
            break;
        if ((otp->timestampDay == 0) || (otp->timestampDay > 31))
            break;
        if (otp->timestampHour >= 24)
            break;
        if (otp->timestampMinute >= 60)
            break;
        if (otp->timestampSecond >= 61)
            break;

        snprintf(info->soc_date, countof(info->soc_date), "%04u/%02hhu/%02hhu %02hhu:%02hhu:%02hhu",
            year, otp->timestampMonth, otp->timestampDay,
            otp->timestampHour, otp->timestampMinute, otp->timestampSecond);
    } while (false);
}


// Read SecureInfo_A.
void GetSysInfo_SecureInfo(SysInfo* info, char nand_drive) {
    static char path[] = "_:/rw/sys/SecureInfo__";

    SecureInfo data;

    path[0] = nand_drive;

    strncpy(info->sub_model, "<unknown>", countof(info->sub_model));
    strncpy(info->serial, "<unknown>", countof(info->serial));
    strncpy(info->system_region, "<unknown>", countof(info->system_region));
    strncpy(info->sales_region, "<unknown>", countof(info->sales_region));

    // Try SecureInfo_A then SecureInfo_B.
    bool got_data = false;
    for (char which = 'A'; which <= 'B'; ++which) {
        path[countof(path) - 2] = which;

        UINT got_size;
        if (fvx_qread(path, &data, 0, sizeof(data), &got_size) == FR_OK) {
            if (got_size == sizeof(data)) {
                got_data = true;
                break;
            }
        }
    }

    if (!got_data) {
        return;
    }

    // Decode region.
    if (data.region < SMDH_NUM_REGIONS) {
        strncpy(info->system_region, g_regionNamesLong[data.region], countof(info->system_region));
    }

    // Retrieve serial number.  Set up calculation of check digit.
    STATIC_ASSERT(countof(info->serial) > countof(data.serial));

    bool got_serial = true;
    char second_letter = '\0';
    char first_digit = '\0';
    char second_digit = '\0';
    unsigned digits = 0;
    unsigned letters = 0;
    unsigned odds = 0;
    unsigned evens = 0;

    for (unsigned x = 0; x < 15; ++x) {
        char ch = data.serial[x];
        info->serial[x] = ch;

        if (ch == '\0') {
            break;
        } else if ((ch < ' ') || (ch > '~')) {
            got_serial = false;
            break;
        } else if ((ch >= '0') && (ch <= '9')) {
            // Track the sum of "odds" and "evens" based on their position.
            // The first digit is "odd".
            ++digits;
            if (digits % 2)
                odds += ch - '0';
            else
                evens += ch - '0';

            // Remember the first two digits for submodel check.
            if (digits == 1)
                first_digit = ch;
            else if (digits == 2)
                second_digit = ch;
        } else {
            // Remember the second letter, because that's the sales region.
            ++letters;
            if (letters == 2) {
                second_letter = ch;
            }
        }
    }

    if (!got_serial) {
        return;
    }

    // Copy the serial out.
    strncpy(info->serial, data.serial, countof(data.serial));
    info->serial[countof(data.serial)] = '\0';

    // Append the check digit if the format appears valid.
    size_t length = strlen(info->serial);
    if ((length < countof(info->serial) - 1) && (digits == 8)) {
        unsigned check_value = 10 - (((3 * evens) + odds) % 10);
        char check_digit = (check_value == 10) ? '0' : (char) (check_value + '0');

        info->serial[length] = check_digit;
        info->serial[length + 1] = '\0';
    }

    // Determine the sales region from the second letter of the prefix.
    if (second_letter != '\0') {
        for (unsigned x = 0; x < countof(s_salesRegions); ++x) {
            if (s_salesRegions[x].serial_char == second_letter) {
                strncpy(info->sales_region, s_salesRegions[x].name, countof(info->sales_region));
                break;
            }
        }
    }

    // Determine the sub-model from the first two digits of the digit part.
    if (first_digit && second_digit) {
        if (IS_DEVKIT) {
            if ((first_digit == '9') && (second_digit == '0') && (info->int_model == MODEL_OLD_3DS)) {
                strncpy(info->sub_model, "Partner-CTR", countof(info->sub_model));
            } else if ((first_digit == '9') && (second_digit == '1') && (info->int_model == MODEL_OLD_3DS)) {
                strncpy(info->sub_model, "IS-CTR-BOX", countof(info->sub_model));
            } else if ((first_digit == '9') && (second_digit == '1') && (info->int_model == MODEL_OLD_3DS_XL)) {
                strncpy(info->sub_model, "IS-SPR-BOX", countof(info->sub_model));
            } else if ((first_digit == '9') && (second_digit == '1') && (info->int_model == MODEL_NEW_3DS)) {
                strncpy(info->sub_model, "IS-SNAKE-BOX", countof(info->sub_model));
            } else {
                strncpy(info->sub_model, "panda", countof(info->sub_model));
            }
        } else {
            if ((first_digit == '0') && (second_digit == '1') && !IS_O3DS) {
                strncpy(info->sub_model, "press", countof(info->sub_model));
            } else {
                strncpy(info->sub_model, "retail", countof(info->sub_model));
            }
        }
    }
}


// Read movable.sed.
void GetSysInfo_Movable(SysInfo* info, char nand_drive) {
    static char path[] = "_:/private/movable.sed";

    MovableSed data;

    path[0] = nand_drive;

    strncpy(info->friendcodeseed, "<unknown>", countof(info->friendcodeseed));
    strncpy(info->movablekeyy, "<unknown>", countof(info->movablekeyy));
    strncpy(info->nand_id0, "<unknown>", countof(info->nand_id0));
    
    if (fvx_qread(path, &data, 0, 0x120 /* sizeof(data) */, NULL) != FR_OK) // whatever, we don't need the last 0x20 byte here
        return;
    
    // The LocalFriendCodeSeed.
    snprintf(info->friendcodeseed, 16 + 1, "%016llX", getbe64(data.codeseed_data.codeseed));
    
    // The Movable KeyY
    snprintf(info->movablekeyy, 32 + 1, "%s%016llX", info->friendcodeseed, getbe64(data.keyy_high));
    
    // SysNAND ID0
    unsigned int sha256sum[8];
    sha_quick(sha256sum, data.codeseed_data.codeseed, 16, SHA256_MODE);
    snprintf(info->nand_id0, 32 + 1, "%08X%08X%08X%08X", sha256sum[0], sha256sum[1], sha256sum[2], sha256sum[3]);
}


// Read sdmmc.
void GetSysInfo_SDMMC(SysInfo* info, char nand_drive) {
    (void) nand_drive;
    
    u8 nand_cid[16] = { 0 };
    u8 sd_cid[16] = { 0 };
    
    strncpy(info->nand_cid, "<unknown>", countof(info->nand_cid));
    strncpy(info->sd_cid, "<unknown>", countof(info->sd_cid));
    strncpy(info->nand_id1, "<unknown>", countof(info->nand_id1));
    
    sdmmc_get_cid(1, (u32*) (void*) nand_cid);
    snprintf(info->nand_cid, 32 + 1, "%016llX%016llX", getbe64(nand_cid), getbe64(nand_cid+8));
    
    sdmmc_get_cid(0, (u32*) (void*) sd_cid);
    snprintf(info->sd_cid, 32 + 1, "%016llX%016llX", getbe64(sd_cid), getbe64(sd_cid+8));
    snprintf(info->nand_id1, 32 + 1, "%08lX%08lX%08lX%08lX",
        getle32(sd_cid+0), getle32(sd_cid+4), getle32(sd_cid+8), getle32(sd_cid+12));
}


// Log file parser helper function.
void SysInfo_ParseText(FIL* file, void (*line_parser)(void*, const char*, size_t), void* context) {
    // Buffer we store lines into.
    char buffer[512];
    UINT filled = 0;
    bool skip_line = false;
    bool prev_cr = false;

    // Main loop.
    for (;;) {
        // How much to read now.
        UINT now = (UINT) (sizeof(buffer) - filled);

        // If now is zero, it means that our buffer is full.
        if (now == 0) {
            // Reset the buffer, but skip this line.
            filled = 0;
            skip_line = true;
            continue;
        }

        // Read this chunk.
        UINT actual = 0;
        if (fvx_read(file, &buffer[filled], now, &actual) != FR_OK)
            break;

        // actual == 0 means read past end of file.
        if (actual == 0)
            break;

        filled += actual;

        // Search for line terminators.
        char* current = buffer;
        UINT remaining = filled;
        for (;;) {
            char* carriage = memchr(current, '\r', remaining);
            char* newline = memchr(current, '\n', remaining);

            // If neither is present, we have an unfinished line.
            if (!carriage && !newline) {
                // Move stuff down and return to the outer loop.
                memmove(buffer, current, remaining);
                filled = remaining;
                break;
            }

            // We have a complete line, yay.
            // Use whichever separator came first.
            // Comparing non-null pointers to null is undefined behavior,
            // hence the if maze here.
            char* found;
            if (!carriage)
                found = newline;
            else if (!newline)
                found = carriage;
            else
                found = min(carriage, newline);

            size_t length = (size_t) (found - current);

            // If this isn't an empty line between a carriage return and
            // a linefeed, it's a real line.  However, we might need to
            // skip lines if they overflow the buffer.
            if (!skip_line && ((length != 0) || (found != newline) || !prev_cr)) {
                // Report this line.
                line_parser(context, current, length);
            }

            // Clear the skip_line flag and set prev_cr as needed.
            skip_line = false;
            prev_cr = (found == carriage);

            // Move on from this line.
            remaining -= (found + 1) - current;
            current = found + 1;
        }
    }

    // If we have a partial line at this point, report it as a line.
    if (filled > 0) {
        line_parser(context, buffer, filled);
    }
}


// Helper function.
bool SysInfo_IsOnlyDigits(const char* str, size_t length) {
    for (; length > 0; ++str, --length) {
        if (!isdigit((unsigned char) *str)) {
            return false;
        }
    }

    return true;
}


// Split a comma-delimited list.  Used for twln:/sys/product.log.
unsigned SysInfo_CommaSplit(const char** starts, size_t* lengths, unsigned max_entries, const char* line, size_t line_length) {
    unsigned index = 0;

    if (max_entries == 0)
        return 0;

    for (;;) {
        // Search for the next comma.
        const char* comma = memchr(line, ',', line_length);

        starts[index] = line;

        // If no comma, we're done.
        // If we maxed out the entry list, put the rest of the line
        // into the last entry.
        if (!comma || (index == max_entries - 1)) {
            lengths[index] = line_length;
            return index + 1;
        }

        // Add this entry.
        lengths[index] = (size_t) (comma - line);
        ++index;

        // Skip over the comma.
        line_length -= (size_t) (1 + comma - line);
        line = comma + 1;
    }
}


// Line parser for twln:/sys/log/inspect.log.
void SysInfoLineParser_InspectLog(void* context, const char* line, size_t length) {
    SysInfo* info = (SysInfo*) context;

    static const char s_commentUpdated[] = "CommentUpdated=";

    if (length < countof(s_commentUpdated) - 1)
        return;
    if (memcmp(line, s_commentUpdated, sizeof(s_commentUpdated) - sizeof(s_commentUpdated[0])) != 0)
        return;

    length -= countof(s_commentUpdated) - 1;
    line += countof(s_commentUpdated) - 1;
    length = min(length, countof(info->assembly_date) - 1);

    memcpy(info->assembly_date, line, length * sizeof(*line));
    info->assembly_date[length] = '\0';
}


// Line parser for twln:/sys/log/product.log.
// NOTE: product.log is parsed linearly so that only the last entry in the file
// takes effect.  This is important for 3DS's that were reflashed by Nintendo -
// we want whichever information is the latest.
void SysInfoLineParser_ProductLog(void* context, const char* line, size_t length) {
    SysInfo* info = (SysInfo*) context;

    const char* entries[10];
    size_t entry_lengths[countof(entries)];

    unsigned entry_count = SysInfo_CommaSplit(entries, entry_lengths, countof(entries), line, length);

    // Ignore lines that don't have at least 2 entries.
    if (entry_count < 2)
        return;

    // Ignore lines in which the first entry isn't a number.
    if ((entry_lengths[0] == 0) || !SysInfo_IsOnlyDigits(entries[0], entry_lengths[0]))
        return;

    // Look for entries we want.
    if ((entry_lengths[1] == 8) && (memcmp(entries[1], "DataList", 8) == 0)) {
        // Original firmware version is written here.
        if ((entry_count < 8) || (entry_lengths[2] != 2) || (memcmp(entries[2], "OK", 2) != 0))
            return;

        // Format: nup:20U cup:9.0.0 preInstall:RA1
        const char* verinfo = entries[7];
        size_t remaining = entry_lengths[7];
        if ((remaining < 4) || (memcmp(verinfo, "nup:", 4) != 0))
            return;

        verinfo += 4;
        remaining -= 4;

        // Find whitespace afterward.
        const char* nup_start = verinfo;
        while ((remaining > 0) && (*verinfo != ' ')) {
            ++verinfo;
            --remaining;
        }
        const char* nup_end = verinfo;

        // Skip whitespace until cup:.
        while ((remaining > 0) && (*verinfo == ' ')) {
            ++verinfo;
            --remaining;
        }

        if ((remaining < 4) || (memcmp(verinfo, "cup:", 4) != 0))
            return;

        verinfo += 4;
        remaining -= 4;

        // Find whitespace afterward.
        const char* cup_start = verinfo;
        while ((remaining > 0) && (*verinfo != ' ')) {
            ++verinfo;
            --remaining;
        }
        const char* cup_end = verinfo;

        // Calculate lengths.
        size_t nup_length = (size_t) (nup_end - nup_start);
        size_t cup_length = (size_t) (cup_end - cup_start);

        if (nup_length + cup_length < nup_length)
            return;
        if (nup_length + cup_length > SIZE_MAX - 1 - 1)
            return;

        if (cup_length + 1 + nup_length + 1 > countof(info->original_firmware))
            return;

        memcpy(&info->original_firmware[0], cup_start, cup_length);
        info->original_firmware[cup_length] = '-';
        memcpy(&info->original_firmware[cup_length + 1], nup_start, nup_length);
        info->original_firmware[cup_length + 1 + nup_length] = '\0';
    }
}


// Get information from the factory setup log.
void GetSysInfo_TWLN(SysInfo* info, char nand_drive) {
    char twln_drive = (char) (nand_drive + 1);

    static char inspect_path[] = " :/sys/log/inspect.log";
    static char product_path[] = " :/sys/log/product.log";

    inspect_path[0] = twln_drive;
    product_path[0] = twln_drive;

    strncpy(info->assembly_date, "<unknown>", countof(info->assembly_date));
    strncpy(info->original_firmware, "<unknown>", countof(info->original_firmware));

    FIL file;
    if (fvx_open(&file, inspect_path, FA_READ | FA_OPEN_EXISTING) == FR_OK) {
        SysInfo_ParseText(&file, SysInfoLineParser_InspectLog, info);
        fvx_close(&file);
    }

    if (fvx_open(&file, product_path, FA_READ | FA_OPEN_EXISTING) == FR_OK) {
        SysInfo_ParseText(&file, SysInfoLineParser_ProductLog, info);
        fvx_close(&file);
    }
}


void MeowSprintf(char** text, const char* format, ...)
{
    char buffer[256];

    va_list args;
    va_start(args, format);
    vsnprintf(*text, countof(buffer), format, args);
    va_end(args);

    *text += strlen(*text);
}


void MyriaSysinfo(char* sysinfo_txt) {
    SysInfo info;
    GetSysInfo_Hardware(&info, '1');
    GetSysInfo_OTP(&info, '1');
    GetSysInfo_SecureInfo(&info, '1');
    GetSysInfo_Movable(&info, '1');
    GetSysInfo_SDMMC(&info, '1');
    GetSysInfo_TWLN(&info, '1');

    char** meow = &sysinfo_txt;
    MeowSprintf(meow, "Model: %s (%s)\r\n", info.model, info.sub_model);
    MeowSprintf(meow, "Serial: %s\r\n", info.serial);
    MeowSprintf(meow, "Region (system): %s\r\n", info.system_region);
    MeowSprintf(meow, "Region (sales): %s\r\n", info.sales_region);
    MeowSprintf(meow, "SoC manufacturing date: %s\r\n", info.soc_date);
    MeowSprintf(meow, "System assembly date: %s\r\n", info.assembly_date);
    MeowSprintf(meow, "Original firmware: %s\r\n", info.original_firmware);
    MeowSprintf(meow, "\r\n");
    MeowSprintf(meow, "Friendcode seed: %s\r\n", info.friendcodeseed);
    MeowSprintf(meow, "SD keyY: %s\r\n", info.movablekeyy);
    MeowSprintf(meow, "NAND CID: %s\r\n", info.nand_cid);
    MeowSprintf(meow, "SD CID: %s\r\n", info.sd_cid);
    MeowSprintf(meow, "System ID0: %s\r\n", info.nand_id0);
    MeowSprintf(meow, "System ID1: %s\r\n", info.nand_id1);
}
