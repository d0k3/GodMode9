#include "cmd.h"


u32 CheckCmdSize(CmdHeader* cmd, u64 fsize) {
	u64 cmdsize = sizeof(CmdHeader) +
		(cmd->n_entries * sizeof(u32)) +
		(cmd->n_cmacs   * sizeof(u32)) +
		(cmd->n_entries * 0x10);

	return (fsize == cmdsize) ? 0 : 1;
}

u32 BuildCmdData(CmdHeader* cmd, TitleMetaData* tmd) {
    u32 content_count = getbe16(tmd->content_count);

    // header basic info
    cmd->cmd_id = 0x1;
    cmd->n_entries = content_count;
    cmd->n_cmacs = content_count;
    cmd->unknown = 0x0; // this means no CMACs, only valid for NAND

    // copy content ids
    u32* cnt_id = (u32*) (cmd + 1);
    u32* cnt_id_cpy = cnt_id + content_count;
	TmdContentChunk* chunk = (TmdContentChunk*) (tmd + 1);
    for (u32 i = 0; (i < content_count) && (i < TMD_MAX_CONTENTS); i++, chunk++) {
    	cnt_id[i] = getbe32(chunk->id);
    	cnt_id_cpy[i] = cnt_id[i];
    }

    // bubble sort the second content id list
    u32 b = 0;
    while ((b < content_count) && (b < TMD_MAX_CONTENTS)) {
        for (b = 1; (b < content_count) && (b < TMD_MAX_CONTENTS); b++) {
            if (cnt_id_cpy[b] < cnt_id_cpy[b-1]) {
                u32 swp = cnt_id_cpy[b];
                cnt_id_cpy[b] = cnt_id_cpy[b-1];
                cnt_id_cpy[b-1] = swp;
            }
        }
    }

    // set CMACs to 0xFF
    u8* cnt_cmac = (u8*) (cnt_id + (2*cmd->n_entries));
    memset(cmd->cmac, 0xFF, 0x10);
    memset(cnt_cmac, 0xFF, 0x10 * content_count);

    // we still need to fix / set the CMACs inside the CMD file!
    return 0;
}
