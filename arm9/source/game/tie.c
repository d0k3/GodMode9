#include "tie.h"
#include "cmd.h"

#define CMD_SIZE_ALIGN(sd) (sd ? 0x8000 : 0x4000)


u32 BuildTitleInfoEntryTmd(TitleInfoEntry* tie, TitleMetaData* tmd, bool sd) {
	u64 title_id = getbe64(tmd->title_id);
	u32 has_id1 = false;
	
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
	TmdContentChunk* chunk = (TmdContentChunk*) (tmd + 1);
	tie->title_size =
		(align_size * 3) + // base folder + 'content' + 'cmd'
		align(TMD_SIZE_N(content_count), align_size) + // TMD
		align_size; // CMD, placeholder
	for (u32 i = 0; (i < content_count) && (i < TMD_MAX_CONTENTS); i++, chunk++) {
    	if (getbe32(chunk->id) == 1) has_id1 = true; // will be useful later
    	tie->title_size += align(getbe64(chunk->size), align_size);
    }

    // manual? (we need to properly check this later)
    if (has_id1 && (((title_id >> 32) == 0x00040000) || ((title_id >> 32) == 0x00040010))) {
    	tie->flags_0[0] = 0x1; // this may have a manual
    }

	return 0;
}

u32 BuildTitleInfoEntryTwl(TitleInfoEntry* tie, TitleMetaData* tmd, TwlHeader* twl) {
	u64 title_id = getbe64(tmd->title_id);

	if (ValidateTwlHeader(twl) != 0) return 1;
	if (BuildTitleInfoEntryTmd(tie, tmd, false) != 0) return 1;

	// product code
	memcpy(tie->product_code, twl->game_title, 0x0A);

	// specific flags
	// see: http://3dbrew.org/wiki/Titles
	if ((title_id >> 32) == 0x00048004) { // TWL app / game
		tie->flags_2[0] = 0x01;
		tie->flags_2[4] = 0x01;
		tie->flags_2[5] = 0x01;
	}

	return 0;
}

u32 BuildTitleInfoEntryNcch(TitleInfoEntry* tie, TitleMetaData* tmd, NcchHeader* ncch, NcchExtHeader* exthdr, bool sd) {
	u64 title_id = getbe64(tmd->title_id);

	if (ValidateNcchHeader(ncch) != 0) return 1;
	if (BuildTitleInfoEntryTmd(tie, tmd, sd) != 0) return 1;

	// product code, extended title version
	memcpy(tie->product_code, ncch->productcode, 0x10);
	tie->title_version |= (ncch->version << 16);
	
	// specific flags
	// see: http://3dbrew.org/wiki/Titles
	if (!((title_id >> 32) & 0x10)) // not a system title
		tie->flags_2[4] = 0x01;

	// stuff from extheader
	if (exthdr) {
		// add save data size to title size
		if (exthdr->savedata_size) {
			u32 align_size = CMD_SIZE_ALIGN(sd);
			tie->title_size +=
				align_size + // 'data' folder
				align(exthdr->savedata_size, align_size); // savegame
			tie->flags_1[0] = 0x01; // has SD save
		};
		// extdata ID low (hacky)
		tie->extdata_id_low = getle32(exthdr->aci_data + 0x30 - 0x0C + 0x04);
	} else tie->flags_0[0] = 0x00; // no manual

	return 0;
}
