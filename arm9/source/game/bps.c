/*
 *   This file is part of GodMode9
 *   Copyright (C) 2019 Wolfvak
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <libgen.h>

#include "language.h"
#include "common.h"
#include "timer.h"
#include "crc32.h"
#include "fs.h"
#include "ui.h"

#include "hid.h"
#include "bps.h"

#define BEAT_VLIBUFSZ	(8)
#define BEAT_MAXPATH	(256)
#define BEAT_FILEBUFSZ	(256 * 1024)

#define BEAT_RANGE(c, i)	((c)->ranges[1][i] - (c)->ranges[0][i])
#define BEAT_UPDATEDELAYMS	(1000 / 4)

#define BEAT_ABSPOS(c, i)	((c)->foff[i] + (c)->ranges[0][i])

#define BEAT_READONLY	(FA_READ | FA_OPEN_EXISTING)
#define BEAT_RWCREATE	(FA_READ | FA_WRITE | FA_CREATE_ALWAYS)

static u32 progress_refcnt = 0;
static u64 progress_timer = 0;

static size_t fs_size(const char *path)
{
	FILINFO fno;
	FRESULT res = fvx_stat(path, &fno);
	if (res != FR_OK) return 0;
	return fno.fsize;
}

static const char *basepath(const char *path)
{
	const char *ret = path + strlen(path);
	while((--ret) > path) {
		if (*ret == '/') break;
	}
	return ret;
}

/* Possible error codes */
enum {
	BEAT_OK = 0,
	BEAT_EOAL,
	BEAT_ABORTED,
	BEAT_IO_ERROR,
	BEAT_OVERFLOW,
	BEAT_BADPATCH,
	BEAT_BADINPUT,
	BEAT_BADOUTPUT,
	BEAT_BADCHKSUM,
	BEAT_PATCH_EXPECT,
	BEAT_OUT_OF_MEMORY,
};

/* State machine actions */
enum {
	BPS_SOURCEREAD = 0,
	BPS_TARGETREAD = 1,
	BPS_SOURCECOPY = 2,
	BPS_TARGETCOPY = 3
};

enum {
	BPM_CREATEPATH = 0,
	BPM_CREATEFILE = 1,
	BPM_MODIFYFILE = 2,
	BPM_MIRRORFILE = 3
};

/* File handles used within the Beat state */
enum {
	BEAT_PF = 0, // patch file
	BEAT_IF, // input file
	BEAT_OF, // output file
	BEAT_FILENUM,
};

static const u8 bps_signature[] = { 'B', 'P', 'S', '1' };
static const u8 bps_chksumoffs[BEAT_FILENUM] = {
	[BEAT_PF] = 4, [BEAT_OF] = 8, [BEAT_IF] = 12,
};
static const u8 bpm_signature[] = { 'B', 'P', 'M', '1' };

/** BEAT STATE STORAGE */
typedef struct {
	u8 *copybuf;
	size_t foff[BEAT_FILENUM], eoal_offset;
	size_t ranges[2][BEAT_FILENUM];
	u32 ocrc; // Output crc

	union {
		struct { // BPS exclusive fields
			u32 xocrc; // Expected output crc
			size_t source_relative, target_relative;
		};
		struct { // BPM exclusive fields
			const char *bpm_path, *source_dir, *target_dir;
		};
	};
	char processing[BEAT_MAXPATH];
	FIL file[BEAT_FILENUM];
} BEAT_Context;

typedef int (*BEAT_Action)(BEAT_Context*, u32);

static bool BEAT_UpdateProgress(const BEAT_Context *ctx)
{ // only updates progress for the parent patch, so the embedded BPS wont be displayed
	u64 tmr;
	if (progress_refcnt > 2) bkpt; // nope, bug out

	tmr = timer_msec(progress_timer);
	if (CheckButton(BUTTON_B)) return false; // check for an abort situation
	if (progress_refcnt != 1) return true; // only show the first level progress
	if (tmr < BEAT_UPDATEDELAYMS) return true; // give it some time

	progress_timer = timer_start();
	ShowProgress(ctx->foff[BEAT_PF], BEAT_RANGE(ctx, BEAT_PF), ctx->processing);
	return true;
}

static const char *BEAT_ErrString(int error)
{ // Get an error description string
	switch(error) {
		case BEAT_OK: return STR_BEAT_NO_ERROR;
		case BEAT_EOAL: return STR_BEAT_END_OF_ACTION_LIST;
		case BEAT_ABORTED: return STR_BEAT_ABORTED_BY_USER;
		case BEAT_IO_ERROR: return STR_BEAT_FAILED_TO_READ_WRITE_FILE;
		case BEAT_OVERFLOW: return STR_BEAT_ATTEMPTED_TO_WRITE_BEYOND_EOF;
		case BEAT_BADPATCH: return STR_BEAT_INVALID_PATCH_FILE;
		case BEAT_BADINPUT: return STR_BEAT_INVALID_INPUT_FILE;
		case BEAT_BADOUTPUT: return STR_BEAT_OUTPUT_FILE_CHECKSUM_MISMATCH;
		case BEAT_BADCHKSUM: return STR_BEAT_FILE_CHECKSUM_FAILED;
		case BEAT_PATCH_EXPECT: return STR_BEAT_EXPECTED_MORE_PATCH_DATA;
		case BEAT_OUT_OF_MEMORY: return STR_BEAT_OUT_OF_MEMORY;
		default: return STR_BEAT_UNKNOWN_ERROR;
	}
}

static int BEAT_Read(BEAT_Context *ctx, int id, void *out, size_t len, int fwd)
{ // Read up to `len` bytes from the context file `id` to the `out` buffer
	UINT br;
	FRESULT res;
	if ((len + ctx->foff[id]) > BEAT_RANGE(ctx, id))
		return BEAT_OVERFLOW;

	fvx_lseek(&ctx->file[id], BEAT_ABSPOS(ctx, id)); // ALWAYS use the state offset + start range
	ctx->foff[id] += len * fwd;
	res = fvx_read(&ctx->file[id], out, len, &br);
	return (res == FR_OK && br == len) ? BEAT_OK : BEAT_IO_ERROR;
}

static int BEAT_WriteOut(BEAT_Context *ctx, const u8 *in, size_t len, int fwd)
{ // Write `len` bytes from `in` to BEAT_OF, updates the output CRC
	UINT bw;
	FRESULT res;
	if ((len + ctx->foff[BEAT_OF]) > BEAT_RANGE(ctx, BEAT_OF))
		return BEAT_OVERFLOW;

	// Blindly assume all writes will be done linearly
	ctx->ocrc = ~crc32_calculate(~ctx->ocrc, in, len);
	fvx_lseek(&ctx->file[BEAT_OF], BEAT_ABSPOS(ctx, BEAT_OF));
	ctx->foff[BEAT_OF] += len * fwd;
	res = fvx_write(&ctx->file[BEAT_OF], in, len, &bw);
	return (res == FR_OK && bw == len) ? BEAT_OK : BEAT_IO_ERROR;
}

static void BEAT_SeekOff(BEAT_Context *ctx, int id, ssize_t offset)
{ ctx->foff[id] += offset; } // Seek `offset` bytes forward
static void BEAT_SeekAbs(BEAT_Context *ctx, int id, size_t pos)
{ ctx->foff[id] = pos; } // Seek to absolute position `pos`

static int BEAT_NextVLI(BEAT_Context *ctx, u32 *vli)
{ // Read the next VLI in the file, update the seek position
	int res;
	u32 ret = 0;
	u32 iter = 0;
	u8 vli_rdbuf[BEAT_VLIBUFSZ], *scan = vli_rdbuf;

	res = BEAT_Read(ctx, BEAT_PF, vli_rdbuf, sizeof(vli_rdbuf), 0);
	if (res != BEAT_OK) return res;

	while(scan < &vli_rdbuf[sizeof(vli_rdbuf)]) {
		u32 val = *(scan++);
		ret += (val & 0x7F) << iter;
		if (val & 0x80) break;
		iter += 7;
		ret += (u32)(1ULL << iter);
	}

	// Seek forward only by the amount of used bytes
	BEAT_SeekOff(ctx, BEAT_PF, scan - vli_rdbuf);
	*vli = ret;
	return res;
}

static s32 BEAT_DecodeSigned(u32 val) // Extract the signed number
{ if (val&1) return -(val>>1); else return (val>>1); }

static int BEAT_RunActions(BEAT_Context *ctx, const BEAT_Action *acts)
{ // Parses an action list and runs commands specified in `acts`
	u32 vli, len;
	int cmd, res = BEAT_OK;

	while((res == BEAT_OK) &&
		(ctx->foff[BEAT_PF] < (BEAT_RANGE(ctx, BEAT_PF) - ctx->eoal_offset))) {
		res = BEAT_NextVLI(ctx, &vli); // get next action
		cmd = vli & 3;
		len = (vli >> 2) + 1;
		if (res != BEAT_OK) return res;

		if (!BEAT_UpdateProgress(ctx)) return BEAT_ABORTED;

		res = (acts[cmd])(ctx, len); // Execute next action
		if (res != BEAT_OK) return res; // Break on error or user abort
	}

	return res;
}

static void BEAT_ReleaseCTX(BEAT_Context *ctx)
{ // Release any resources associated to the context
	free(ctx->copybuf);
	for (int i = 0; i < BEAT_FILENUM; i++) {
		if (fvx_opened(&ctx->file[i])) fvx_close(&ctx->file[i]);
	}
	progress_refcnt--; // lol what even are atomics
}

// BPS Specific functions
/**
 Initialize the Beat File Structure
 - verifies checksums
 - performs further sanity checks
 - extracts initial info
 - leaves the file ready to begin state machine execution
 */
static int BPS_InitCTX_Advanced(BEAT_Context *ctx, const char *bps_path, const char *in_path, const char *out_path, size_t start, size_t end, bool do_chksum)
{
	int res;
	u8 read_magic[4];
	u32 vli, in_sz, metaend_off;
	u32 chksum[BEAT_FILENUM], expected_chksum[BEAT_FILENUM];

	// Clear stackbuf
	memset(ctx, 0, sizeof(*ctx));
	ctx->eoal_offset = 12;

	if (end == 0) {
		start = 0;
		end = fs_size(bps_path);
	}

	if (do_chksum) // get BPS checksum
		chksum[BEAT_PF] = crc32_calculate_from_file(bps_path, start, end - start - 4);

	strcpy(ctx->processing, basepath(bps_path));

	// open all files
	fvx_open(&ctx->file[BEAT_PF], bps_path, BEAT_READONLY);
	ctx->ranges[0][BEAT_PF] = start;
	ctx->ranges[1][BEAT_PF] = end;

	fvx_open(&ctx->file[BEAT_IF], in_path, BEAT_READONLY);
	ctx->ranges[0][BEAT_IF] = 0;
	ctx->ranges[1][BEAT_IF] = fs_size(in_path);

	res = fvx_open(&ctx->file[BEAT_OF], out_path, BEAT_RWCREATE);
	if (res != FR_OK) return BEAT_IO_ERROR;

	// Verify BPS1 header magic
	res = BEAT_Read(ctx, BEAT_PF, read_magic, sizeof(read_magic), 1);
	if (res != BEAT_OK) return BEAT_IO_ERROR;
	res = memcmp(read_magic, bps_signature, sizeof(bps_signature));
	if (res != 0) return BEAT_BADPATCH;

	// Check input size
	res = BEAT_NextVLI(ctx, &in_sz);
	if (res != BEAT_OK) return res;
	if (ctx->ranges[1][BEAT_IF] != in_sz) return BEAT_BADINPUT;

	// Get expected output size
	res = BEAT_NextVLI(ctx, &vli);
	if (res != BEAT_OK) return res;
	ctx->ranges[0][BEAT_OF] = 0;
	ctx->ranges[1][BEAT_OF] = vli;

	// Get end of metadata offset
	res = BEAT_NextVLI(ctx, &metaend_off);
	if (res != BEAT_OK) return res;
	metaend_off += ctx->foff[BEAT_PF];

	// Read checksums from BPS file
	for (int i = 0; i < BEAT_FILENUM; i++) {
		BEAT_SeekAbs(ctx, BEAT_PF, BEAT_RANGE(ctx, BEAT_PF) - bps_chksumoffs[i]);
		BEAT_Read(ctx, BEAT_PF, &expected_chksum[i], sizeof(u32), 0);
	}

	if (do_chksum) { // Verify patch checksum
		if (chksum[BEAT_PF] != expected_chksum[BEAT_PF]) return BEAT_BADCHKSUM;
	}

	// Initialize output checksums
	ctx->ocrc = 0;
	ctx->xocrc = expected_chksum[BEAT_OF];

	// Allocate temporary block copy buffer
	ctx->copybuf = malloc(BEAT_FILEBUFSZ);
	if (ctx->copybuf == NULL) return BEAT_OUT_OF_MEMORY;

	// Seek back to the start of action stream / end of metadata
	BEAT_SeekAbs(ctx, BEAT_PF, metaend_off);
	progress_refcnt++;
	return BEAT_OK;
}

static int BPS_InitCTX(BEAT_Context *ctx, const char *bps, const char *in, const char *out)
{ return BPS_InitCTX_Advanced(ctx, bps, in, out, 0, 0, true); }

/*
 Generic helper function to copy from `src_id` to BEAT_OF
 Used by SourceRead, TargetRead and CreateFile
*/
static int BEAT_BlkCopy(BEAT_Context *ctx, int src_id, u32 len)
{
	while(len > 0) {
		ssize_t blksz = min(len, BEAT_FILEBUFSZ);
		int res = BEAT_Read(ctx, src_id, ctx->copybuf, blksz, 1);
		if (res != BEAT_OK) return res;

		res = BEAT_WriteOut(ctx, ctx->copybuf, blksz, 1);
		if (res != BEAT_OK) return res;

		if (!BEAT_UpdateProgress(ctx)) return BEAT_ABORTED;

		len -= blksz;
	}
	return BEAT_OK;
}

static int BPS_SourceRead(BEAT_Context *ctx, u32 len)
{ // This command copies bytes from the source file to the target file
	BEAT_SeekAbs(ctx, BEAT_IF, ctx->foff[BEAT_OF]);
	return BEAT_BlkCopy(ctx, BEAT_IF, len);
}

/*
 [...] the actual data is not available to the patch applier,
 so it is stored directly inside the patch.
*/
static int BPS_TargetRead(BEAT_Context *ctx, u32 len)
{
	return BEAT_BlkCopy(ctx, BEAT_PF, len);
}

/*
 An offset is supplied to seek the sourceRelativeOffset to the desired
 location, and then data is copied from said offset to the target file
*/
static int BPS_SourceCopy(BEAT_Context *ctx, u32 len)
{
	int res;
	u32 vli;
	s32 offset;

	res = BEAT_NextVLI(ctx, &vli);
	if (res != BEAT_OK) return res;

	offset = BEAT_DecodeSigned(vli);
	BEAT_SeekAbs(ctx, BEAT_IF, ctx->source_relative + offset);
	ctx->source_relative += offset + len;

	return BEAT_BlkCopy(ctx, BEAT_IF, len);
}

/* This command treats all of the data that has already been written to the target file as a dictionary */
static int BPS_TargetCopy(BEAT_Context *ctx, u32 len)
{ // the black sheep of the family, needs special care
	int res;
	s32 offset;
	u32 out_off, rel_off, vli;

	res = BEAT_NextVLI(ctx, &vli);
	if (res != BEAT_OK) return res;

	offset = BEAT_DecodeSigned(vli);
	out_off = ctx->foff[BEAT_OF];
	rel_off = ctx->target_relative + offset;
	if (rel_off > out_off) return BEAT_BADPATCH; // Illegal

	while(len != 0) {
		u8 *remfill;
		ssize_t blksz, distance, remainder;

		blksz = min(len, BEAT_FILEBUFSZ);
		distance = min((ssize_t)(out_off - rel_off), blksz);

		BEAT_SeekAbs(ctx, BEAT_OF, rel_off);
		res = BEAT_Read(ctx, BEAT_OF, ctx->copybuf, distance, 0);
		if (res != BEAT_OK) return res;

		remfill = ctx->copybuf + distance;
		remainder = blksz - distance;
		while(remainder > 0) { // fill the buffer with repeats
			ssize_t remblk = min(distance, remainder);
			memcpy(remfill, ctx->copybuf, remblk);
			remfill += remblk;
			remainder -= remblk;
		}

		BEAT_SeekAbs(ctx, BEAT_OF, out_off);
		res = BEAT_WriteOut(ctx, ctx->copybuf, blksz, 0);
		if (res != BEAT_OK) return res;

		if (!BEAT_UpdateProgress(ctx)) return BEAT_ABORTED;
		rel_off += blksz;
		out_off += blksz;
		len -= blksz;
	}

	BEAT_SeekAbs(ctx, BEAT_OF, out_off);
	ctx->target_relative = rel_off;
	return BEAT_OK;
}

static int BPS_RunActions(BEAT_Context *ctx)
{
	static const BEAT_Action BPS_Actions[] = { // BPS action handlers
		[BPS_SOURCEREAD] = BPS_SourceRead,
		[BPS_TARGETREAD] = BPS_TargetRead,
		[BPS_SOURCECOPY] = BPS_SourceCopy,
		[BPS_TARGETCOPY] = BPS_TargetCopy
	};
	int res = BEAT_RunActions(ctx, BPS_Actions);
	if (res == BEAT_ABORTED) return BEAT_ABORTED;
	if (res == BEAT_EOAL) // Verify hashes
		return (ctx->ocrc == ctx->xocrc) ? BEAT_OK : BEAT_BADOUTPUT;
	return res; // some kind of error
}

/***********************
 BPM Specific functions
***********************/
static int BPM_OpenFile(BEAT_Context *ctx, int id, const char *path, size_t max_sz)
{
	FRESULT res;

	if (fvx_opened(&ctx->file[id])) fvx_close(&ctx->file[id]);
	res = fvx_open(&ctx->file[id], path, max_sz ? BEAT_RWCREATE : BEAT_READONLY);
	if (res != FR_OK) return BEAT_IO_ERROR;

	ctx->ranges[0][id] = 0;
	if (max_sz > 0) {
		ctx->ranges[1][id] = max_sz;
	} else {
		ctx->ranges[1][id] = f_size(&ctx->file[id]);
	}

	// if a new file is opened it makes no sense to keep the old CRC
	// a single outfile wont be created from more than one infile (& patch)
	ctx->ocrc = 0;
	ctx->foff[id] = 0;
	return BEAT_OK;
}

static int BPM_InitCTX(BEAT_Context *ctx, const char *bpm_path, const char *src_dir, const char *dst_dir)
{
	int res;
	u32 metaend_off;
	u8 read_magic[4];
	u32 chksum, expected_chksum;

	memset(ctx, 0, sizeof(*ctx));

	ctx->bpm_path = bpm_path;
	ctx->source_dir = src_dir;
	ctx->target_dir = dst_dir;
	ctx->eoal_offset = 4;

	chksum = crc32_calculate_from_file(bpm_path, 0, fs_size(bpm_path) - 4);
	res = BPM_OpenFile(ctx, BEAT_PF, bpm_path, 0);
	if (res != BEAT_OK) return res;
	res = BEAT_Read(ctx, BEAT_PF, read_magic, sizeof(read_magic), 1);
	if (res != BEAT_OK) return res;
	res = memcmp(read_magic, bpm_signature, sizeof(bpm_signature));
	if (res != 0) return BEAT_BADPATCH;

	// Get end of metadata offset
	res = BEAT_NextVLI(ctx, &metaend_off);
	if (res != BEAT_OK) return res;
	metaend_off += ctx->foff[BEAT_PF];

	// Read checksums from BPS file
	BEAT_SeekAbs(ctx, BEAT_PF, BEAT_RANGE(ctx, BEAT_PF) - 4);
	res = BEAT_Read(ctx, BEAT_PF, &expected_chksum, sizeof(u32), 0);
	if (res != BEAT_OK) return res;
	if (expected_chksum != chksum) return BEAT_BADCHKSUM;

	// Allocate temporary block copy buffer
	ctx->copybuf = malloc(BEAT_FILEBUFSZ);
	if (ctx->copybuf == NULL) return BEAT_OUT_OF_MEMORY;

	// Seek back to the start of action stream / end of metadata
	BEAT_SeekAbs(ctx, BEAT_PF, metaend_off);
	progress_refcnt++;
	return BEAT_OK;
}

static int BPM_NextPath(BEAT_Context *ctx, char *out, int name_len)
{
	if (name_len >= BEAT_MAXPATH) return BEAT_BADPATCH;
	int res = BEAT_Read(ctx, BEAT_PF, out, name_len, 1);
	out[name_len] = '\0';
	if (res == BEAT_OK) {
		out[name_len] = '\0'; // make sure the buffer ends with a zero char
		strcpy(ctx->processing, out);
	}
	return res;
}

static int BPM_MakeRelativePaths(BEAT_Context *ctx, char *src, char *dst, int name_len)
{
	char name[BEAT_MAXPATH];
	int res = BPM_NextPath(ctx, name, name_len);
	if (res != BEAT_OK) return res;
	if (src != NULL)
		if (snprintf(src, BEAT_MAXPATH, "%s/%s", ctx->source_dir, name) >= BEAT_MAXPATH) return BEAT_BADPATCH;
	if (dst != NULL)
		if (snprintf(dst, BEAT_MAXPATH, "%s/%s", ctx->target_dir, name) >= BEAT_MAXPATH) return BEAT_BADPATCH;
	return res;
}

static int BPM_CreatePath(BEAT_Context *ctx, u32 name_len)
{ // Create a directory
	char path[BEAT_MAXPATH];
	int res = BPM_MakeRelativePaths(ctx, NULL, path, name_len);
	if (res != BEAT_OK) return res;

	res = fvx_mkdir(path);
	if (res != FR_OK && res != FR_EXIST) return BEAT_IO_ERROR;
	return BEAT_OK;
}

static int BPM_CreateFile(BEAT_Context *ctx, u32 name_len)
{ // Create a file and fill it with data provided in the BPM
	u32 file_sz;
	u32 checksum;
	char path[BEAT_MAXPATH];

	int res = BPM_MakeRelativePaths(ctx, NULL, path, name_len);
	if (res != BEAT_OK) return res;

	res = BEAT_NextVLI(ctx, &file_sz); // get new file size
	if (res != BEAT_OK) return res;
	res = BPM_OpenFile(ctx, BEAT_OF, path, file_sz); // open file as RW
	if (res != BEAT_OK) return res;
	res = BEAT_BlkCopy(ctx, BEAT_PF, file_sz); // copy data to new file
	if (res != BEAT_OK) return res;

	res = BEAT_Read(ctx, BEAT_PF, &checksum, sizeof(u32), 1);
	if (res != BEAT_OK) return res;
	if (ctx->ocrc != checksum) return BEAT_BADOUTPUT; // get and check CRC32
	return BEAT_OK;
}

static int BPM_ModifyFile(BEAT_Context *ctx, u32 name_len)
{ // Apply a BPS patch
	u32 origin, bps_sz;
	BEAT_Context bps_context;
	char src[BEAT_MAXPATH], dst[BEAT_MAXPATH];

	int res = BPM_MakeRelativePaths(ctx, src, dst, name_len);
	if (res != BEAT_OK) return res;

	res = BEAT_NextVLI(ctx, &origin); // get dummy(?) origin value
	if (res != BEAT_OK) return res;
	res = BEAT_NextVLI(ctx, &bps_sz); // get embedded BPS size
	if (res != BEAT_OK) return res;

	res = BPS_InitCTX_Advanced(
		&bps_context, ctx->bpm_path, src, dst,
		ctx->foff[BEAT_PF], ctx->foff[BEAT_PF] + bps_sz,
		false
	); // create a BPS context using the current ranges
	if (res == BEAT_OK) res = BPS_RunActions(&bps_context); // run if OK
	BEAT_ReleaseCTX(&bps_context);
	if (res != BEAT_OK) return res; // break off if there was an error

	BEAT_SeekOff(ctx, BEAT_PF, bps_sz); // advance beyond the BPS
	return BEAT_OK;
}

static int BPM_MirrorFile(BEAT_Context *ctx, u32 name_len)
{ // Copy a file from source to target without any modifications
	u32 origin;
	u32 checksum;
	char src[BEAT_MAXPATH], dst[BEAT_MAXPATH];

	int res = BPM_MakeRelativePaths(ctx, src, dst, name_len);
	if (res != BEAT_OK) return res;

	// open source and destination files, read the origin dummy
	res = BPM_OpenFile(ctx, BEAT_IF, src, 0);
	if (res != BEAT_OK) return res;
	res = BPM_OpenFile(ctx, BEAT_OF, dst, ctx->ranges[1][BEAT_IF]);
	if (res != BEAT_OK) return res;
	res = BEAT_NextVLI(ctx, &origin);
	if (res != BEAT_OK) return res;

	// copy straight from source to destination
	res = BEAT_BlkCopy(ctx, BEAT_IF, ctx->ranges[1][BEAT_IF]);
	if (res != BEAT_OK) return res;

	res = BEAT_Read(ctx, BEAT_PF, &checksum, sizeof(u32), 1);
	if (res != BEAT_OK) return res;
	if (ctx->ocrc != checksum) return BEAT_BADOUTPUT; // verify checksum
	return BEAT_OK;
}

static int BPM_RunActions(BEAT_Context *ctx)
{
	static const BEAT_Action BPM_Actions[] = { // BPM Action handlers
		[BPM_CREATEPATH] = BPM_CreatePath,
		[BPM_CREATEFILE] = BPM_CreateFile,
		[BPM_MODIFYFILE] = BPM_ModifyFile,
		[BPM_MIRRORFILE] = BPM_MirrorFile
	};
	int res = BEAT_RunActions(ctx, BPM_Actions);
	if (res == BEAT_ABORTED) return BEAT_ABORTED;
	if (res == BEAT_EOAL) return BEAT_OK;
	return res;
}

static int BEAT_Run(const char *p, const char *s, const char *d, bool bpm)
{
	int res;
	BEAT_Context ctx;

	progress_timer = timer_start();
	res = (bpm ? BPM_InitCTX : BPS_InitCTX)(&ctx, p, s, d);
	if (res != BEAT_OK) {
		ShowPrompt(false, bpm ? STR_FAILED_TO_INITIALIZE_BPM_FILE : STR_FAILED_TO_INITIALIZE_BPS_FILE, BEAT_ErrString(res));
	} else {
		res = (bpm ? BPM_RunActions : BPS_RunActions)(&ctx);
		switch(res) {
			case BEAT_OK:
				ShowPrompt(false, "%s", STR_PATCH_SUCCESSFULLY_APPLIED);
				break;
			case BEAT_ABORTED:
				ShowPrompt(false, "%s", STR_PATCHING_ABORTED_BY_USER);
				break;
			default:
				ShowPrompt(false, STR_FAILED_TO_RUN_PATCH, BEAT_ErrString(res));
				break;
		}
	}
	BEAT_ReleaseCTX(&ctx);
	return (res == BEAT_OK) ? 0 : 1;
}

int ApplyBPSPatch(const char* modifyName, const char* sourceName, const char* targetName)
{ return BEAT_Run(modifyName, sourceName, targetName, false); }
int ApplyBPMPatch(const char* patchName, const char* sourcePath, const char* targetPath)
{ return BEAT_Run(patchName, sourcePath, targetPath, true); }
