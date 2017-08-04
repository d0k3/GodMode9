#include "fsscript.h"
#include "fsutil.h"
#include "fsinit.h"
#include "fsperm.h"
#include "nandutil.h"
#include "gameutil.h"
#include "keydbutil.h"
#include "filetype.h"
#include "power.h"
#include "vff.h"
#include "rtc.h"
#include "sha.h"
#include "ui.h"

#define _MAX_ARGS       2
#define _VAR_CNT_LEN    256
#define _VAR_NAME_LEN   32
#define _ERR_STR_LEN    32

#define VAR_BUFFER      (SCRIPT_BUFFER + SCRIPT_BUFFER_SIZE - VAR_BUFFER_SIZE)

// some useful macros
#define IS_WHITESPACE(c)    ((c == ' ') || (c == '\t') || (c == '\r') || (c == '\n'))
#define _FLG(c)             (1 << (c - 'a'))

// command ids
typedef enum {
    CMD_ID_NONE = 0,
    CMD_ID_ECHO,
    CMD_ID_ASK,
    CMD_ID_INPUT,
    CMD_ID_SET,
    CMD_ID_ALLOW,
    CMD_ID_CP,
    CMD_ID_MV,
    CMD_ID_INJECT,
    CMD_ID_RM,
    CMD_ID_MKDIR,
    CMD_ID_MOUNT,
    CMD_ID_UMOUNT,
    CMD_ID_FIND,
    CMD_ID_FINDNOT,
    CMD_ID_SHA,
    CMD_ID_FIXCMAC,
    CMD_ID_VERIFY,
    CMD_ID_DECRYPT,
    CMD_ID_ENCRYPT,
    CMD_ID_BUILDCIA,
    CMD_ID_REBOOT,
    CMD_ID_POWEROFF
} cmd_id;

typedef struct {
    cmd_id id;
    char cmd[16];
    u32 n_args;
    u32 allowed_flags;
} Gm9ScriptCmd;

typedef struct {
    char name[_VAR_NAME_LEN]; // variable name
    char content[_VAR_CNT_LEN];
} Gm9ScriptVar;

Gm9ScriptCmd cmd_list[] = {
    { CMD_ID_ECHO    , "echo"    , 1, 0 },
    { CMD_ID_ASK     , "ask"     , 1, 0 },
    { CMD_ID_INPUT   , "input"   , 2, 0 },
    { CMD_ID_SET     , "set"     , 2, 0 },
    { CMD_ID_ALLOW   , "allow"   , 1, _FLG('a') },
    { CMD_ID_CP      , "cp"      , 2, _FLG('h') | _FLG('w') | _FLG('k') | _FLG('s') | _FLG('n')},
    { CMD_ID_MV      , "mv"      , 2, _FLG('w') | _FLG('k') | _FLG('s') | _FLG('n') },
    { CMD_ID_INJECT  , "inject"  , 2, _FLG('n') },
    { CMD_ID_RM      , "rm"      , 1, 0 },
    { CMD_ID_MKDIR   , "mkdir"   , 1, 0 },
    { CMD_ID_MOUNT   , "imgmount", 1, 0 },
    { CMD_ID_UMOUNT  , "imgumount",0, 0 },
    { CMD_ID_FIND    , "find"    , 2, 0 },
    { CMD_ID_FINDNOT , "findnot" , 2, 0 },
    { CMD_ID_SHA     , "sha"     , 2, 0 },
    // { CMD_ID_FIXCMAC , "fixcmac" , 1, 0 }, // not supported yet
    { CMD_ID_VERIFY  , "verify"  , 1, 0 },
    { CMD_ID_DECRYPT , "decrypt" , 1, 0 },
    { CMD_ID_ENCRYPT , "encrypt" , 1, 0 },
    { CMD_ID_BUILDCIA, "buildcia", 1, _FLG('l') },
    { CMD_ID_REBOOT  , "reboot"  , 0, 0 },
    { CMD_ID_POWEROFF, "poweroff", 0, 0 }
};    

static inline bool strntohex(const char* str, u8* hex, u32 len) {
    if (!len) {
        len = strlen(str); 
        if (len%1) return false;
        else len >>= 1;
    } else if (len*2 != strnlen(str, (len*2)+1)) {
        return false;
    }
    for (u32 i = 0; i < len; i++) {
        char bytestr[2+1] = { 0 };
        u32 bytehex;
        memcpy(bytestr, str + (i*2), 2);
        if (sscanf(bytestr, "%02lx", &bytehex) != 1)
            return false;
        hex[i] = (u8) bytehex;
    }
    return true;
}

char* get_var(const char* name, char** endptr) {
    Gm9ScriptVar* vars = (Gm9ScriptVar*) VAR_BUFFER;
    u32 max_vars = VAR_BUFFER_SIZE / sizeof(Gm9ScriptVar);
    
    u32 name_len = 0;
    char* vname = NULL;
    if (!endptr) { // no endptr, varname is verbatim
        vname = (char*) name;
        name_len = strnlen(vname, _VAR_NAME_LEN);
    } else { // endptr given, varname is in [VAR] format
        vname = (char*) name + 1;
        if (*name != '[') return NULL;
        for (name_len = 0; vname[name_len] != ']'; name_len++)
            if ((name_len >= _VAR_NAME_LEN) || !vname[name_len]) return NULL;
        *endptr = vname + name_len + 1;
    }
    
    u32 n_var = 0;
    for (Gm9ScriptVar* var = vars; n_var < max_vars; n_var++, var++) {
        if (!*(var->name) || (strncmp(var->name, vname, name_len) == 0)) break;
    }
    
    if (n_var >= max_vars || !*(vars[n_var].name)) n_var = 0;
    if (n_var && (strncmp(vars[n_var].name, "DATESTAMP", _VAR_NAME_LEN) == 0)) {
        DsTime dstime;
        get_dstime(&dstime);
        snprintf(vars[n_var].content, _VAR_CNT_LEN, "%02lX%02lX%02lX", (u32) dstime.bcd_Y, (u32) dstime.bcd_M, (u32) dstime.bcd_D);
    } else if (n_var && (strncmp(vars[n_var].name, "TIMESTAMP", _VAR_NAME_LEN) == 0)) {
        DsTime dstime;
        get_dstime(&dstime);
        snprintf(vars[n_var].content, _VAR_CNT_LEN, "%02lX%02lX%02lX", (u32) dstime.bcd_h, (u32) dstime.bcd_m, (u32) dstime.bcd_s);
    }
    
    return vars[n_var].content;
}

char* set_var(const char* name, const char* content) {
    Gm9ScriptVar* vars = (Gm9ScriptVar*) VAR_BUFFER;
    u32 max_vars = VAR_BUFFER_SIZE / sizeof(Gm9ScriptVar);
    
    if ((strnlen(name, _VAR_NAME_LEN) > (_VAR_NAME_LEN-1)) || (strnlen(content, _VAR_CNT_LEN) > (_VAR_CNT_LEN-1)) ||
        (strchr(name, '[') || strchr(name, ']')))
        return NULL;
    
    u32 n_var = 0;
    for (Gm9ScriptVar* var = vars; n_var < max_vars; n_var++, var++)
        if (!*(var->name) || (strncmp(var->name, name, _VAR_NAME_LEN) == 0)) break;
    if (n_var >= max_vars) return NULL;
    strncpy(vars[n_var].name, name, _VAR_NAME_LEN);
    strncpy(vars[n_var].content, content, _VAR_CNT_LEN);
    if (!n_var) *(vars[n_var].content) = '\0'; // NULL var
    
    return vars[n_var].content;
}

bool init_vars(const char* path_script) {
    // reset var buffer
    memset(VAR_BUFFER, 0x00, VAR_BUFFER_SIZE);
    
    // device serial
    char env_serial[16] = { 0 };
    if ((FileGetData("1:/rw/sys/SecureInfo_A", (u8*) env_serial, 0xF, 0x102) != 0xF) &&
        (FileGetData("1:/rw/sys/SecureInfo_B", (u8*) env_serial, 0xF, 0x102) != 0xF))
        snprintf(env_serial, 0xF, "UNKNOWN");
    
    // current path
    char curr_dir[_VAR_CNT_LEN];
    strncpy(curr_dir, path_script, _VAR_CNT_LEN);
    char* slash = strrchr(curr_dir, '/');
    if (slash) *slash = '\0';
    
    // device sysnand / emunand id0
    char env_sys_id0[32+1];
    char env_emu_id0[32+1];
    for (u32 emu = 0; emu <= 1; emu++) {
        u8 sd_keyy[0x10];
        char* env_id0 = emu ? env_emu_id0 : env_sys_id0;
        char* path = emu ? "4:/private/movable.sed" : "1:/private/movable.sed";
        if (FileGetData(path, sd_keyy, 0x10, 0x110) == 0x10) {
            u32 sha256sum[8];
            sha_quick(sha256sum, sd_keyy, 0x10, SHA256_MODE);
            snprintf(env_id0, 32+1, "%08lx%08lx%08lx%08lx",
                sha256sum[0], sha256sum[1], sha256sum[2], sha256sum[3]);
        } else snprintf(env_id0, 0xF, "UNKNOWN");
    }
    
    // set env vars
    set_var("NULL", ""); // this one is special and should not be changed later 
    set_var("SERIAL", env_serial);
    set_var("GM9OUT", OUTPUT_PATH);
    set_var("CURRDIR", curr_dir);
    set_var("SYSID0", env_sys_id0);
    set_var("EMUID0", env_emu_id0);
    set_var("DATESTAMP", ""); // needs to be set on every access
    set_var("TIMESTAMP", ""); // needs to be set on every access
    
    return true;
}

bool expand_arg(char* argex, const char* arg, u32 len) {
    char* out = argex;
    
    for (char* in = (char*) arg; in - arg < (int) len; in++) {
        u32 out_len = out - argex;
        if (out_len >= (_VAR_CNT_LEN-1)) return false; // maximum arglen reached
        
        if (*in == '\\') { // escape line breaks (no other escape is handled)
            if (*(++in) == 'n') *(out++) = '\n';
            else {
                *(out++) = '\\';
                *(out++) = *in;
            }
        } else if (*in == '$') { // replace vars
            char* content = get_var(in + 1, &in);
            if (content) {
                u32 clen = strnlen(content, 256);
                strncpy(out, content, clen);
                out += clen;
                in--; // go back one char
            } else *(out++) = *in;
        } else *(out++) = *in;
    }
    *out = '\0';
    
    return true;
}

cmd_id get_cmd_id(char* cmd, u32 len, u32 flags, u32 argc, char* err_str) {
    Gm9ScriptCmd* cmd_entry = NULL;
    
    for (u32 i = 0; i < (sizeof(cmd_list)/sizeof(Gm9ScriptCmd)); i++) {
        if (strncmp(cmd_list[i].cmd, cmd, len) == 0) {
            cmd_entry = cmd_list + i;
            break;
        }
    }
    
    if (!cmd_entry) {
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "unknown cmd");
    } else if (cmd_entry->n_args != argc) {
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "bad # of args");
    } else if (~(cmd_entry->allowed_flags|_FLG('o')|_FLG('s')) & flags) {
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "unrecognized flags");
    } else return cmd_entry->id;
    
    return 0;
}

u32 get_flag(char* str, u32 len, char* err_str) {
    char flag_char = '\0';
    
    if ((len < 2) || (*str != '-')) flag_char = '\0';
    else if (len == 2) flag_char = str[1];
    else if (strncmp(str, "--all", len) == 0) flag_char = 'a';
    else if (strncmp(str, "--hash", len) == 0) flag_char = 'h';
    else if (strncmp(str, "--skip", len) == 0) flag_char = 'k';
    else if (strncmp(str, "--legit", len) == 0) flag_char = 'l';
    else if (strncmp(str, "--no_cancel", len) == 0) flag_char = 'n';
    else if (strncmp(str, "--optional", len) == 0) flag_char = 'o';
    else if (strncmp(str, "--silent", len) == 0) flag_char = 's';
    else if (strncmp(str, "--overwrite", len) == 0) flag_char = 'w';
    
    if ((flag_char < 'a') && (flag_char > 'z')) {
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "illegal flag");
        return 0;
    }
    
    return _FLG(flag_char);
}

char* get_string(char* ptr, const char* line_end, u32* len, char** next, char* err_str) {
    char* str = NULL;
    *len = 0;
    
    // skip whitespaces
    for (; IS_WHITESPACE(*ptr) && (ptr < line_end); ptr++);
    if (ptr >= line_end) return (*next = (char*) line_end); // end reached, all whitespaces
    
    // handle string
    if (*ptr == '\"') { // quotes
        str = ++ptr;
        for (; (*ptr != '\"') && (ptr < line_end); ptr++, (*len)++);
        if (ptr >= line_end) { // failed if unresolved quotes
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "unresolved quotes");
            return NULL;
        }
        *next = ptr + 1;
    } else { // no quotes, no whitespace
        str = ptr; 
        for (; !IS_WHITESPACE(*ptr) && (ptr < line_end); ptr++, (*len)++);
        *next = ptr;
    }
    
    return str;
}

bool parse_line(const char* line_start, const char* line_end, cmd_id* cmdid, u32* flags, u32* argc, char** argv, char* err_str) {
    char* ptr = (char*) line_start;
    char* str;
    u32 len;
    
    // set everything to initial values
    *cmdid = 0;
    *flags = 0;
    *argc = 0;
    
    // search for cmd
    char* cmd = NULL;
    u32 cmd_len = 0;
    if (!(cmd = get_string(ptr, line_end, &cmd_len, &ptr, err_str))) return false; // string error
    if ((cmd >= line_end) || (*cmd == '#')) return true; // empty line or comment
    
    // got cmd, now parse flags & args
    while ((str = get_string(ptr, line_end, &len, &ptr, err_str))) {
        if ((str >= line_end) || (*str == '#')) // end of line or comment
            return (*cmdid = get_cmd_id(cmd, cmd_len, *flags, *argc, err_str));
        if (*str == '-') { // flag
            u32 flag_add = get_flag(str, len, err_str);
            if (!flag_add) return false; // not a proper flag
            *flags |= flag_add;
        } else if (*argc >= _MAX_ARGS) {
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "too many arguments");
            return false; // too many arguments
        } else if (!expand_arg(argv[(*argc)++], str, len)) {
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "argument expand failed");
            return false; // arg expand failed
        }
    }
    
    // end reached with a failed get_string()
    return false;
}

bool run_cmd(cmd_id id, u32 flags, char** argv, char* err_str) {
    bool ret = true; // true unless some cmd messes up
    
    // perform command
    if (id == CMD_ID_ECHO) {
        ShowPrompt(false, argv[0]);
    } else if (id == CMD_ID_ASK) {
        ret = ShowPrompt(true, argv[0]);
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "user abort");
    } else if (id == CMD_ID_INPUT) {
        char* var = get_var(argv[1], NULL);
        if (!*var) set_var(argv[1], ""); // make sure the var exists
        ret = ShowStringPrompt(var, _VAR_CNT_LEN, argv[0]);
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "user abort");
    } else if (id == CMD_ID_SET) {
        ret = set_var(argv[0], argv[1]);
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "set fail");
    } else if (id == CMD_ID_ALLOW) {
        if (flags & _FLG('a')) ret = CheckDirWritePermissions(argv[0]);
        else ret = CheckWritePermissions(argv[0]);
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "permission fail");
    } else if (id == CMD_ID_CP) {
        u32 flags_ext = BUILD_PATH;
        if (flags & _FLG('h')) flags_ext |= CALC_SHA;
        if (flags & _FLG('n')) flags_ext |= NO_CANCEL;
        if (flags & _FLG('s')) flags_ext |= SILENT;
        if (flags & _FLG('w')) flags_ext |= OVERWRITE_ALL;
        else if (flags & _FLG('k')) flags_ext |= SKIP_ALL;
        ret = PathMoveCopy(argv[1], argv[0], &flags_ext, false);
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "copy fail");
    } else if (id == CMD_ID_MV) {
        u32 flags_ext = BUILD_PATH;
        if (flags & _FLG('n')) flags_ext |= NO_CANCEL;
        if (flags & _FLG('s')) flags_ext |= SILENT;
        if (flags & _FLG('w')) flags_ext |= OVERWRITE_ALL;
        else if (flags & _FLG('k')) flags_ext |= SKIP_ALL;
        ret = PathMoveCopy(argv[1], argv[0], &flags_ext, true);
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "move fail");
    } else if (id == CMD_ID_INJECT) {
        char* atstr_dst = strrchr(argv[1], '@');
        char* atstr_org = strrchr(argv[0], '@');
        u64 at_org = 0;
        u64 sz_org = 0;
        if (atstr_org) {
            *(atstr_org++) = '\0';
            if (sscanf(atstr_org, "%llX:%llX", &at_org, &sz_org) != 2) {
                if (sscanf(atstr_org, "%llX", &at_org) != 1) at_org = 0;
                sz_org = 0;
            }
        }
        if (!atstr_dst) {
            ret = false;
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "missing dest offset");
        } else {
            u32 flags_ext = 0;
            u64 at_dst = 0;
            *(atstr_dst++) = '\0';
            if (flags & _FLG('n')) flags_ext |= NO_CANCEL;
            ret = ((sscanf(atstr_dst, "%llX", &at_dst) == 1) && FileInjectFile(argv[1], argv[0], at_dst, at_org, sz_org, &flags_ext));
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "inject fail");
        }
    } else if (id == CMD_ID_RM) {
        char pathstr[_ERR_STR_LEN];
        TruncateString(pathstr, argv[0], 24, 8);
        ShowString("Deleting %s...", pathstr);
        ret = PathDelete(argv[0]);
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "remove fail");
    } else if (id == CMD_ID_MKDIR) {
        ret = (fvx_rmkdir(argv[0]) == FR_OK);
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "makedir fail");
    } else if (id == CMD_ID_MOUNT) {
        ret = InitImgFS(argv[0]);
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "mount fail");
    } else if (id == CMD_ID_UMOUNT) {
        InitImgFS(NULL);
    } else if (id == CMD_ID_FIND) {
        char* path = set_var(argv[1], ""); // setup the variable, get pointer
        if (!path) {
            ret = false;
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "var fail");
        } else {
            ret = (fvx_findpath(path, argv[0]) == FR_OK);
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "find fail");
        }
    } else if (id == CMD_ID_FINDNOT) {
        char* path = set_var(argv[1], ""); // setup the variable, get pointer
        if (!path) {
            ret = false;
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "var fail");
        } else {
            ret = (fvx_findnopath(path, argv[0]) == FR_OK);
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "findnot fail");
        }
    } else if (id == CMD_ID_SHA) {
        u8 sha256_fil[0x20];
        u8 sha256_cmp[0x20];
        if (!FileGetSha256(argv[0], sha256_fil)) {
            ret = false;
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "sha arg0 fail");
        } else if ((FileGetData(argv[1], sha256_cmp, 0x20, 0) != 0x20) && !strntohex(argv[1], sha256_cmp, 0x20)) {
            ret = false;
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "sha arg1 fail");
        } else {
            ret = (memcmp(sha256_fil, sha256_cmp, 0x20) == 0);
            if (err_str) snprintf(err_str, _ERR_STR_LEN, "sha does not match");
        }
    } else if (id == CMD_ID_VERIFY) {
        u32 filetype = IdentifyFileType(argv[0]);
        if (filetype & IMG_NAND) ret = (ValidateNandDump(argv[0]) == 0);
        else ret = (VerifyGameFile(argv[0]) == 0);
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "verification failed");
    } else if (id == CMD_ID_DECRYPT) {
        u32 filetype = IdentifyFileType(argv[0]);
        if (filetype & BIN_KEYDB) ret = (CryptAesKeyDb(argv[0], true, false) == 0);
        else ret = (CryptGameFile(argv[0], true, false) == 0);
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "decrypt failed");
    } else if (id == CMD_ID_ENCRYPT) {
        u32 filetype = IdentifyFileType(argv[0]);
        if (filetype & BIN_KEYDB) ret = (CryptAesKeyDb(argv[0], true, true) == 0);
        else ret = (CryptGameFile(argv[0], true, true) == 0);
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "encrypt failed");
    } else if (id == CMD_ID_BUILDCIA) {
        ret = (BuildCiaFromGameFile(argv[0], (flags & _FLG('n'))) == 0);
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "build CIA failed");
    } else if (id == CMD_ID_REBOOT) {
        Reboot();
    } else if (id == CMD_ID_POWEROFF) {
        PowerOff();
    } else { // command not recognized / bad number of arguments
        ret = false;
        if (err_str) snprintf(err_str, _ERR_STR_LEN, "unknown error");
    }
    
    return ret;
}

bool run_line(const char* line_start, const char* line_end, u32* flags, char* err_str) {
    char* argv[_MAX_ARGS] = { NULL };
    u32 argc = 0;
    cmd_id cmdid;
    
    // flags handling (if no pointer given)
    u32 lflags;
    if (!flags) flags = &lflags;
    *flags = 0;
    
    // set up argv vars (if not already done)
    for (u32 i = 0; i < _MAX_ARGS; i++) {
        char name[16];
        snprintf(name, 16, "ARGV%01lu", i);
        argv[i] = set_var(name, "");
        if (!argv[i]) return false;
    }
    
    // parse current line, grab cmd / flags / args
    if (!parse_line(line_start, line_end, &cmdid, flags, &argc, argv, err_str)) {
        *flags &= ~(_FLG('o')|_FLG('s')); // parsing errors are never silent or optional
        return false; 
    }
    
    // run the command (if available)
    if (cmdid && !run_cmd(cmdid, *flags, argv, err_str)) {
        char* msg_fail = get_var("ERRORMSG", NULL);
        if (msg_fail && *msg_fail) *err_str = '\0'; // use custom error message
        return false;
    }
    
    // success if we arrive here
    return true;
}

// checks for illegal ASCII symbols
bool ValidateText(const char* text, u32 len) {
    for (u32 i = 0; i < len; i++) {
        char c = text[i];
        if ((c == '\r') && ((i+1) < len) && (text[i+1] != '\n')) return false; // CR without LF
        if ((c < 0x20) && (c != '\t') && (c != '\r') && (c != '\n')) return false; // illegal control char
        if ((c == 0x7F) || (c == 0xFF)) return false; // other illegal char
    }
    return true;
}

bool ExecuteGM9Script(const char* path_script) {
    // revert mount state?
    char* script = (char*) SCRIPT_BUFFER;
    char* ptr = script;
    
    // fetch script
    u32 script_size;
    if (!(script_size = FileGetData(path_script, (u8*) script, SCRIPT_MAX_SIZE, 0)))
        return false;
    char* end = script + script_size;
    *end = '\0';
    
    // initialise variables
    init_vars(path_script);
    
    for (u32 line = 1; ptr < end; line++) {
        u32 flags = 0;
        
        // find line end
        char* line_end = strchr(ptr, '\n');
        if (!line_end) line_end = ptr + strlen(ptr);
        
        // run command
        char err_str[_ERR_STR_LEN+1] = { 0 };
        if (!run_line(ptr, line_end, &flags, err_str)) { // error handling
            if (!(flags & _FLG('s'))) { // not silent
                if (!*err_str) {
                    char* msg_fail = get_var("ERRORMSG", NULL);
                    if (msg_fail && *msg_fail) ShowPrompt(false, msg_fail);
                    else snprintf(err_str, _ERR_STR_LEN, "error message fail");
                }
                if (*err_str) {
                    char line_str[32+1];
                    char* lptr0 = ptr;
                    char* lptr1 = line_end;
                    for (; IS_WHITESPACE(*lptr0) && (lptr0 < lptr1); lptr0++); // skip whitespaces
                    if ((lptr1 > lptr0) && (*(lptr1-1) == '\r')) lptr1--; // handle \r
                    if (lptr1 - lptr0 > 32) snprintf(line_str, 32+1, "%.29s...", lptr0);
                    else snprintf(line_str, 32+1, "%.*s", lptr1 - lptr0, lptr0);
                    char path_str[32+1];
                    TruncateString(path_str, path_script, 32, 12);
                    ShowPrompt(false, "%s\nline %lu: %s\n%s", path_str, line, err_str, line_str);
                }
            }
            if (!(flags & _FLG('o'))) return false; // failed if not optional
        }
        
        // reposition pointer
        ptr = line_end + 1;
    }
    
    char* msg_okay = get_var("SUCCESSMSG", NULL);
    if (msg_okay && *msg_okay) ShowPrompt(false, msg_okay);
    return true;
}
