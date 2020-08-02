#include "cmd.h"


CmdHeader* BuildAllocCmdData(TitleMetaData* tmd) {
    CmdHeader proto;
    CmdHeader* cmd = NULL;
    u32 content_count = getbe16(tmd->content_count);
    u16 max_cnt_idx = 0;

    // sanity check
    if (!content_count)
        return NULL;

    // find max content id
    TmdContentChunk* chunk = (TmdContentChunk*) (tmd + 1);
    for (u32 i = 0; (i < content_count) && (i < TMD_MAX_CONTENTS); i++, chunk++)
        if (getbe16(chunk->index) > max_cnt_idx) max_cnt_idx = getbe16(chunk->index);

    // allocate memory for CMD / basic setup
    proto.cmd_id = 1;
    proto.n_entries = max_cnt_idx + 1;
    proto.n_cmacs = content_count;
    proto.unknown = 1;
    memset(proto.cmac, 0x00, 0x10);
    cmd = (CmdHeader*) malloc(CMD_SIZE(&proto));
    if (!cmd) return NULL;
    memset(cmd, 0x00, CMD_SIZE(&proto));
    memcpy(cmd, &proto, sizeof(CmdHeader));
    cmd->unknown = 0x0; // this means no CMACs, only valid for NAND

    // copy content ids
    u32* cnt_id = (u32*) (cmd + 1);
    u32* cnt_id_2nd = cnt_id + cmd->n_entries;
    chunk = (TmdContentChunk*) (tmd + 1);
    memset(cnt_id, 0xFF, cmd->n_entries * sizeof(u32));
    for (u32 i = 0; (i < content_count) && (i < TMD_MAX_CONTENTS); i++, chunk++) {
        u32 chunk_id = getbe32(chunk->id);
        cnt_id[getbe16(chunk->index)] = chunk_id;
        *(cnt_id_2nd++) = chunk_id;
    }

    // bubble sort the second content id list
    bool bs_finished = false;
    cnt_id_2nd = cnt_id + cmd->n_entries;
    while (!bs_finished) {
        bs_finished = true;
        for (u32 b = 1; b < cmd->n_cmacs; b++) {
            if (cnt_id_2nd[b] < cnt_id_2nd[b-1]) {
                u32 swp = cnt_id_2nd[b];
                cnt_id_2nd[b] = cnt_id_2nd[b-1];
                cnt_id_2nd[b-1] = swp;
                bs_finished = false;
            }
        }
    }

    // set CMACs to 0x00
    u8* cnt_cmac = (u8*) (cnt_id_2nd + cmd->n_cmacs);
    memset(cnt_cmac, 0x00, 0x10 * cmd->n_entries);

    // we still need to fix / set the CMACs inside the CMD file!
    return cmd;
}
