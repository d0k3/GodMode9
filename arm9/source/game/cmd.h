#pragma once

#include "common.h"


// from: http://3dbrew.org/wiki/Titles#Data_Structure
typedef struct {
	u32 cmd_id; 	// same as filename id, <cmd_id>.cmd
	u32 n_entries; 	// matches highest content index
	u32 n_cmacs; 	// number of cmacs in file (excluding the one @0x10)
	u32 unknown; 	// usually 1
	u8  cmac[0x10]; // calculated from first 0x10 byte of data, no hashing
	// followed by u32 list of content ids (sorted by index, 0xFFFFFFFF for unavailable)
	// followed by u32 list of content ids (sorted by id?)
	// followed by <n_entries> CMACs (may contain garbage)
} __attribute__((packed, aligned(4))) CmdHeader;

u32 CheckCmdSize(CmdHeader* cmd, u64 fsize);
