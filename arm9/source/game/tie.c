#include "tie.h"
#include "cmd.h"

#define CMD_SIZE_ALIGN(sd) (sd ? 0x8000 : 0x4000)


u32 BuildTitleInfoEntryTmd(TitleInfoEntry* tie, TitleMetaData* tmd, bool sd) {
    u64 title_id = getbe64(tmd->title_id);
    bool has_idx1 = false;
    bool has_idx2 = false;

    // set basic values
    memset(tie, 0x00, sizeof(TitleInfoEntry));
    tie->title_type = 0x40;

    // title version, product code, cmd id
    tie->title_version = getbe16(tmd->title_version);
    tie->cmd_content_id = 0x01;
    memcpy(tie->unknown, "GM9", 4); // GM9 install magic number

    // calculate base title size
    // align size: 0x4000 for TWL and CTRNAND, 0x8000 for SD
    u32 align_size = CMD_SIZE_ALIGN(sd);
    u32 content_count = getbe16(tmd->content_count);
    tie->title_size =
        (align_size * 3) + // base folder + 'content' + 'cmd'
        align(TMD_SIZE_N(content_count), align_size) + // TMD
        align_size; // CMD, placeholder (!!!)
    if (getle32(tmd->save_size) || getle32(tmd->twl_privsave_size) || (tmd->twl_flag & 0x2)) {
        tie->title_size +=
            align_size + // data folder
            align(getle32(tmd->save_size), align_size) +
            align(getle32(tmd->twl_privsave_size), align_size) +
            ((tmd->twl_flag & 0x2) ? align(sizeof(TwlIconData), align_size) : 0);
    }

    // contents title size + some additional stuff
    TmdContentChunk* chunk = (TmdContentChunk*) (tmd + 1);
    tie->content0_id = getbe32(chunk->id);
    for (u32 i = 0; (i < content_count) && (i < TMD_MAX_CONTENTS); i++, chunk++) {
        if (getbe16(chunk->index) == 1) has_idx1 = true; // will be useful later
        else if (getbe16(chunk->index) == 2) has_idx2 = true; // will be useful later
        tie->title_size += align(getbe64(chunk->size), align_size);
    }

    // manual? dlp? save? (we need to properly check this later)
    if (((title_id >> 32) == 0x00040000) || ((title_id >> 32) == 0x00040010)) {
        if (has_idx1) tie->flags_0[0] = 0x1; // this may have a manual
        if (has_idx2) tie->title_version |= (0xFFFF << 16); // this may have a dlp
        if (getle32(tmd->save_size)) tie->flags_1[0] = 0x01; // this may have an sd save
    }

    return 0;
}

u32 BuildTitleInfoEntryTwl(TitleInfoEntry* tie, TitleMetaData* tmd, TwlHeader* twl) {
    u64 title_id = getbe64(tmd->title_id);

    // build the basic titledb entry
    if (BuildTitleInfoEntryTmd(tie, tmd, false) != 0) return 1;

    // proper handling of system data archives - thanks @aspargas!
    // see: http://3dbrew.org/wiki/Title_list#0004800F_-_System_Data_Archives
    if ((title_id >> 32) != 0x0004800F) {
        if (ValidateTwlHeader(twl) != 0) return 1;
        memcpy(tie->product_code, twl->game_title, 12);
    }

    // specific flags for DSiWare ports
    // see: http://3dbrew.org/wiki/Titles
    // see: http://3dbrew.org/wiki/Title_list#00048004_-_DSiWare_Ports
    if ((title_id >> 32) == 0x00048004) { // TWL app / game
        tie->flags_2[0] = 0x01;
        tie->flags_2[4] = 0x01;
        tie->flags_2[5] = 0x01;
    } else tie->content0_id = 0;

    return 0;
}

u32 BuildTitleInfoEntryNcch(TitleInfoEntry* tie, TitleMetaData* tmd, NcchHeader* ncch, NcchExtHeader* exthdr, bool sd) {
    u64 title_id = getbe64(tmd->title_id);

    if (ValidateNcchHeader(ncch) != 0) return 1;
    if (BuildTitleInfoEntryTmd(tie, tmd, sd) != 0) return 1;

    // product code, extended title version
    memcpy(tie->product_code, ncch->productcode, 0x10);
    tie->title_version &= ((ncch->version << 16) | 0xFFFF);

    // NCCH titles need no content0 ID
    tie->content0_id = 0;

    // specific flags
    // see: http://3dbrew.org/wiki/Titles
    if (!((title_id >> 32) & 0x10)) // not a system title
        tie->flags_2[4] = 0x01;

    // stuff from extheader
    if (exthdr) {
        // extdata ID low (hacky, we navigate to storage info)
        tie->extdata_id_low = getle32(exthdr->aci_data + (0x30 - 0x0C));
    } else {
        tie->flags_0[0] = 0x00; // no manual
        tie->flags_1[0] = 0x00; // no sd save
        tie->title_version &= 0xFFFF; // no dlp
    }

    return 0;
}
