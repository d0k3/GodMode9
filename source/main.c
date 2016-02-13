#include "common.h"
#include "draw.h"
#include "fs.h"
#include "menu.h"
#include "i2c.h"
#include "decryptor/game.h"
#include "decryptor/nand.h"
#include "decryptor/nandfat.h"
#include "decryptor/titlekey.h"

#define SUBMENU_START 5


MenuInfo menu[] =
{
    {
        "XORpad Generator Options", 7,
        {
            { "NCCH Padgen",                  &NcchPadgen,            0 },
            { "SD Padgen (SDinfo.bin)",       &SdPadgen,              0 },
            { "SD Padgen (SysNAND dir)",      &SdPadgenDirect,        0 },
            { "SD Padgen (EmuNAND dir)",      &SdPadgenDirect,        N_EMUNAND },
            { "CTRNAND Padgen",               &CtrNandPadgen,         0 },
            { "TWLNAND Padgen",               &TwlNandPadgen,         0 },
            { "FIRM0FIRM1 Padgen",            &Firm0Firm1Padgen,      0 }
        }
    },
    {
        "Titlekey Decrypt Options", 3,
        {
            { "Titlekey Decrypt (file)",      &DecryptTitlekeysFile,  0 },
            { "Titlekey Decrypt (SysNAND)",   &DecryptTitlekeysNand,  0 },
            { "Titlekey Decrypt (EmuNAND)",   &DecryptTitlekeysNand,  N_EMUNAND }
        }
    },
    {
        "SysNAND Options", 8,
        {
            { "NAND Backup",                  &DumpNand,              0 },
            { "NAND Restore",                 &RestoreNand,           N_NANDWRITE },
            { "Partition Dump...",            NULL,                   SUBMENU_START + 0 },
            { "Partition Inject...",          NULL,                   SUBMENU_START + 2 },
            { "File Dump...",                 NULL,                   SUBMENU_START + 4 },
            { "File Inject...",               NULL,                   SUBMENU_START + 6 },
            { "Health&Safety Dump",           &DumpHealthAndSafety,   0 },
            { "Health&Safety Inject",         &InjectHealthAndSafety, N_NANDWRITE }
        }
    },
    {
        "EmuNAND Options", 9,
        {
            { "EmuNAND Backup",               &DumpNand,              N_EMUNAND },
            { "EmuNAND Restore",              &RestoreNand,           N_NANDWRITE | N_EMUNAND | N_FORCENAND },
            { "Partition Dump...",            NULL,                   SUBMENU_START + 1 },
            { "Partition Inject...",          NULL,                   SUBMENU_START + 3 },
            { "File Dump...",                 NULL,                   SUBMENU_START + 5 },
            { "File Inject...",               NULL,                   SUBMENU_START + 7 },
            { "Health&Safety Dump",           &DumpHealthAndSafety,   N_EMUNAND },
            { "Health&Safety Inject",         &InjectHealthAndSafety, N_NANDWRITE | N_EMUNAND },
            { "Update SeedDB",                &UpdateSeedDb,          N_EMUNAND }
        }
    },
    {
        "Game Decryptor Options", 10,
        {
            { "NCCH/NCSD Decryptor",          &CryptGameFiles,        GC_NCCH_PROCESS },
            { "NCCH/NCSD Encryptor",          &CryptGameFiles,        GC_NCCH_PROCESS | GC_NCCH_ENCRYPT },
            { "CIA Decryptor (shallow)",      &CryptGameFiles,        GC_CIA_PROCESS },
            { "CIA Decryptor (deep)",         &CryptGameFiles,        GC_CIA_PROCESS | GC_CIA_DEEP },
            { "CIA Decryptor (CXI only)",     &CryptGameFiles,        GC_CIA_PROCESS | GC_CIA_DEEP | GC_CXI_ONLY },
            { "CIA Encryptor (NCCH)",         &CryptGameFiles,        GC_CIA_PROCESS | GC_NCCH_ENCRYPT },
            { "CIA Encryptor (CXI only)",     &CryptGameFiles,        GC_CIA_PROCESS | GC_NCCH_ENCRYPT | GC_CXI_ONLY },
            { "SD Decryptor/Encryptor",       &CryptSdFiles,          0 },
            { "SD Decryptor (SysNAND dir)",   &DecryptSdFilesDirect,  0 },
            { "SD Decryptor (EmuNAND dir)",   &DecryptSdFilesDirect,  N_EMUNAND }
        }
    },
    // everything below is not contained in the main menu
    {
        "Partition Dump... (SysNAND)", 6, // ID 0
        {
            { "Dump TWLN Partition",          &DecryptNandPartition, P_TWLN },
            { "Dump TWLP Partition",          &DecryptNandPartition, P_TWLP },
            { "Dump AGBSAVE Partition",       &DecryptNandPartition, P_AGBSAVE },
            { "Dump FIRM0 Partition",         &DecryptNandPartition, P_FIRM0 },
            { "Dump FIRM1 Partition",         &DecryptNandPartition, P_FIRM1 },
            { "Dump CTRNAND Partition",       &DecryptNandPartition, P_CTRNAND }
        }
    },
    {
        "Partition Dump...(EmuNAND)", 6, // ID 1
        {
            { "Dump TWLN Partition",          &DecryptNandPartition, N_EMUNAND | P_TWLN },
            { "Dump TWLP Partition",          &DecryptNandPartition, N_EMUNAND | P_TWLP },
            { "Dump AGBSAVE Partition",       &DecryptNandPartition, N_EMUNAND | P_AGBSAVE },
            { "Dump FIRM0 Partition",         &DecryptNandPartition, N_EMUNAND | P_FIRM0 },
            { "Dump FIRM1 Partition",         &DecryptNandPartition, N_EMUNAND | P_FIRM1 },
            { "Dump CTRNAND Partition",       &DecryptNandPartition, N_EMUNAND | P_CTRNAND }
        }
    },
    {
        "Partition Inject... (SysNAND)", 6, // ID 2
        {
            { "Inject TWLN Partition",        &InjectNandPartition, N_NANDWRITE | P_TWLN },
            { "Inject TWLP Partition",        &InjectNandPartition, N_NANDWRITE | P_TWLP },
            { "Inject AGBSAVE Partition",     &InjectNandPartition, N_NANDWRITE | P_AGBSAVE },
            { "Inject FIRM0 Partition",       &InjectNandPartition, N_NANDWRITE | P_FIRM0 },
            { "Inject FIRM1 Partition",       &InjectNandPartition, N_NANDWRITE | P_FIRM1 },
            { "Inject CTRNAND Partition",     &InjectNandPartition, N_NANDWRITE | P_CTRNAND }
        }
    },
    {
        "Partition Inject... (EmuNAND)", 6, // ID 3
        {
            { "Inject TWLN Partition",        &InjectNandPartition, N_NANDWRITE | N_EMUNAND | P_TWLN },
            { "Inject TWLP Partition",        &InjectNandPartition, N_NANDWRITE | N_EMUNAND | P_TWLP },
            { "Inject AGBSAVE Partition",     &InjectNandPartition, N_NANDWRITE | N_EMUNAND | P_AGBSAVE },
            { "Inject FIRM0 Partition",       &InjectNandPartition, N_NANDWRITE | N_EMUNAND | P_FIRM0 },
            { "Inject FIRM1 Partition",       &InjectNandPartition, N_NANDWRITE | N_EMUNAND | P_FIRM1 },
            { "Inject CTRNAND Partition",     &InjectNandPartition, N_NANDWRITE | N_EMUNAND | P_CTRNAND }
        }
    },
    {
        "File Dump... (SysNAND)", 10, // ID 4
        {
            { "Dump ticket.db",               &DumpFile,             F_TICKET },
            { "Dump title.db",                &DumpFile,             F_TITLE },
            { "Dump import.db",               &DumpFile,             F_IMPORT },
            { "Dump certs.db",                &DumpFile,             F_CERTS },
            { "Dump SecureInfo_A",            &DumpFile,             F_SECUREINFO },
            { "Dump LocalFriendCodeSeed_B",   &DumpFile,             F_LOCALFRIEND },
            { "Dump rand_seed",               &DumpFile,             F_RANDSEED },
            { "Dump movable.sed",             &DumpFile,             F_MOVABLE },
            { "Dump nagsave.bin",             &DumpFile,             F_NAGSAVE },
            { "Dump nnidsave.bin",            &DumpFile,             F_NNIDSAVE }
        }
    },
    {
        "File Dump... (EmuNAND)", 11, // ID 5
        {
            { "Dump ticket.db",               &DumpFile,             N_EMUNAND | F_TICKET },
            { "Dump title.db",                &DumpFile,             N_EMUNAND | F_TITLE },
            { "Dump import.db",               &DumpFile,             N_EMUNAND | F_IMPORT },
            { "Dump certs.db",                &DumpFile,             N_EMUNAND | F_CERTS },
            { "Dump SecureInfo_A",            &DumpFile,             N_EMUNAND | F_SECUREINFO },
            { "Dump LocalFriendCodeSeed_B",   &DumpFile,             N_EMUNAND | F_LOCALFRIEND },
            { "Dump rand_seed",               &DumpFile,             N_EMUNAND | F_RANDSEED },
            { "Dump movable.sed",             &DumpFile,             N_EMUNAND | F_MOVABLE },
            { "Dump seedsave.bin",            &DumpFile,             N_EMUNAND | F_SEEDSAVE },
            { "Dump nagsave.bin",             &DumpFile,             N_EMUNAND | F_NAGSAVE },
            { "Dump nnidsave.bin",            &DumpFile,             N_EMUNAND | F_NNIDSAVE }
        }
    },
    {
        "File Inject... (SysNAND)", 10, // ID 6
        {
            { "Inject ticket.db",             &InjectFile,           N_NANDWRITE | F_TICKET },
            { "Inject title.db",              &InjectFile,           N_NANDWRITE | F_TITLE },
            { "Inject import.db",             &InjectFile,           N_NANDWRITE | F_IMPORT },
            { "Inject certs.db",              &InjectFile,           N_NANDWRITE | F_CERTS },
            { "Inject SecureInfo_A",          &InjectFile,           N_NANDWRITE | F_SECUREINFO },
            { "Inject LocalFriendCodeSeed_B", &InjectFile,           N_NANDWRITE | F_LOCALFRIEND },
            { "Inject rand_seed",             &InjectFile,           N_NANDWRITE | F_RANDSEED },
            { "Inject movable.sed",           &InjectFile,           N_NANDWRITE | F_MOVABLE },
            { "Inject nagsave.bin",           &InjectFile,           N_NANDWRITE | F_NAGSAVE },
            { "Inject nnidsave.bin",          &InjectFile,           N_NANDWRITE | F_NNIDSAVE }
        }
    },
    {
        "File Inject... (EmuNAND)", 11, // ID 7
        {
            { "Inject ticket.db",             &InjectFile,           N_NANDWRITE | N_EMUNAND | F_TICKET },
            { "Inject title.db",              &InjectFile,           N_NANDWRITE | N_EMUNAND | F_TITLE },
            { "Inject import.db",             &InjectFile,           N_NANDWRITE | N_EMUNAND | F_IMPORT },
            { "Inject certs.db",              &InjectFile,           N_NANDWRITE | N_EMUNAND | F_CERTS },
            { "Inject SecureInfo_A",          &InjectFile,           N_NANDWRITE | N_EMUNAND | F_SECUREINFO },
            { "Inject LocalFriendCodeSeed_B", &InjectFile,           N_NANDWRITE | N_EMUNAND | F_LOCALFRIEND },
            { "Inject rand_seed",             &InjectFile,           N_NANDWRITE | N_EMUNAND | F_RANDSEED },
            { "Inject movable.sed",           &InjectFile,           N_NANDWRITE | N_EMUNAND | F_MOVABLE },
            { "Inject seedsave.bin",          &InjectFile,           N_NANDWRITE | N_EMUNAND | F_SEEDSAVE },
            { "Inject nagsave.bin",           &InjectFile,           N_NANDWRITE | N_EMUNAND | F_NAGSAVE },
            { "Inject nnidsave.bin",          &InjectFile,           N_NANDWRITE | N_EMUNAND | F_NNIDSAVE }
        }
    },
    {
        NULL, 0, {}, // empty menu to signal end
    }
};


void Reboot()
{
    i2cWriteRegister(I2C_DEV_MCU, 0x20, 1 << 2);
    while(true);
}


void PowerOff()
{
    i2cWriteRegister(I2C_DEV_MCU, 0x20, 1 << 0);
    while (true);
}


int main()
{
    ClearScreenFull(true, true);
    InitFS();

    u32 menu_exit = ProcessMenu(menu, SUBMENU_START);
    
    DeinitFS();
    (menu_exit == MENU_EXIT_REBOOT) ? Reboot() : PowerOff();
    return 0;
}
