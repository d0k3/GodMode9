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

#include "common.h"
#include "crc32.h"
#include "bps.h"
#include "fs.h"
#include "ui.h"

#define BEAT_VLIBUFSZ	(16)
#define BEAT_MAXPATH	(256)
#define BEAT_RANGESZ(c, i)	((c)->ranges[1][i] - (c)->ranges[0][i])

#define BEAT_READONLY	(FA_READ | FA_OPEN_EXISTING)
#define BEAT_RWCREATE	(FA_READ | FA_WRITE | FA_CREATE_NEW)

static size_t fs_size(const char *path)
{
	FILINFO fno;
	FRESULT res = fvx_stat(path, &fno);
	if (res != FR_OK) return 0;
	return fno.fsize;
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
	BEAT_PATCHFILE = 0,
	BEAT_SRCFILE,
	BEAT_DSTFILE,
	BEAT_FILECOUNT,
};

static const u8 bps_signature[] = { 'B', 'P', 'S', '1' };
static const u8 bps_chksumoffs[BEAT_FILECOUNT] = {
	[BEAT_PATCHFILE] = 4, [BEAT_DSTFILE] = 8, [BEAT_SRCFILE] = 12,
};
static const u8 bpm_signature[] = { 'B', 'P', 'M', '1' };

/** BEAT STATE STORAGE */
typedef struct {
	u8 *copybuf;
	ssize_t foffset[BEAT_FILECOUNT], eoal_offset;
	size_t ranges[2][BEAT_FILECOUNT];
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
	FIL file[BEAT_FILECOUNT];
} BEAT_Context;

typedef int (*BEAT_Action)(BEAT_Context*, u64);

static const char *BEAT_ErrString(int error)
{ // Get an error description string
	switch(error) {
		case BEAT_OK: return "No error";
		case BEAT_EOAL: return "End of action list";
		case BEAT_ABORTED: return "Aborted by user";
		case BEAT_IO_ERROR: return "Failed to read/write file";
		case BEAT_OVERFLOW: return "Attempted to write beyond end of file";
		case BEAT_BADPATCH: return "Invalid patch file";
		case BEAT_BADINPUT: return "Invalid input file";
		case BEAT_BADOUTPUT: return "Output file checksum mismatch";
		case BEAT_BADCHKSUM: return "File checksum failed";
		case BEAT_PATCH_EXPECT: return "Expected more patch data";
		case BEAT_OUT_OF_MEMORY: return "Out of memory";
		default: return "Unknown error";
	}
}

static int BEAT_Read(BEAT_Context *ctx, int id, void *out, size_t len, bool advance)
{ // Read up to `len` bytes from the context file `id` to the `out` buffer
	FRESULT res;
	UINT br;
	len = min(len, BEAT_RANGESZ(ctx, id) - ctx->foffset[id]);
	fvx_lseek(&ctx->file[id], ctx->ranges[0][id] + ctx->foffset[id]); // ALWAYS use the state offset + start range
	if (advance) ctx->foffset[id] += len;
	res = fvx_read(&ctx->file[id], out, len, &br);
	return (res == FR_OK && br == len) ? BEAT_OK : BEAT_IO_ERROR;
}

static int BEAT_WriteOut(BEAT_Context *ctx, const u8 *in, size_t len, bool advance)
{ // Write `len` bytes from `in` to BEAT_DSTFILE, updates the output CRC
	FRESULT res;
	UINT bw;
	if ((len + ctx->foffset[BEAT_DSTFILE]) > BEAT_RANGESZ(ctx, BEAT_DSTFILE))
		return BEAT_OVERFLOW;

	// Blindly assume all writes will be done linearly
	ctx->ocrc = ~crc32_calculate(~ctx->ocrc, in, len);
	fvx_lseek(&ctx->file[BEAT_DSTFILE], ctx->ranges[0][BEAT_DSTFILE] + ctx->foffset[BEAT_DSTFILE]);
	if (advance) ctx->foffset[BEAT_DSTFILE] += len;
	res = fvx_write(&ctx->file[BEAT_DSTFILE], in, len, &bw);
	return (res == FR_OK && bw == len) ? BEAT_OK : BEAT_IO_ERROR;
}

static void BEAT_SeekOff(BEAT_Context *ctx, int id, ssize_t offset)
{ ctx->foffset[id] += offset; } // Seek `offset` bytes forward
static void BEAT_SeekAbs(BEAT_Context *ctx, int id, size_t pos)
{ ctx->foffset[id] = pos; } // Seek to absolute position `pos`

static int BEAT_NextVLI(BEAT_Context *ctx, u64 *vli)
{ // Read the next VLI in the file, update the seek position
	u8 vli_rdbuf[BEAT_VLIBUFSZ], *scan = vli_rdbuf;
	u32 iter = 0;
	u64 ret = 0;
	int res;

	res = BEAT_Read(ctx, BEAT_PATCHFILE, vli_rdbuf, sizeof(vli_rdbuf), false);
	if (res != BEAT_OK) return res;

	while(scan < &vli_rdbuf[sizeof(vli_rdbuf)]) {
		u64 val = *(scan++);
		ret += (val & 0x7F) << iter;
		if (val & 0x80) break;
		iter += 7;
		ret += (u64)(1ULL << iter);
	}

	// Seek forward only by the amount of used bytes
	BEAT_SeekOff(ctx, BEAT_PATCHFILE, scan - vli_rdbuf);
	*vli = ret;
	return res;
}

static s64 BEAT_DecodeSigned(u64 val) // Extract the signed number
{ if (val&1) return -(val>>1); else return (val>>1); }

static int BEAT_NextAction(int *act, u64 *len, BEAT_Context *ctx)
{ // Decode next action word, retrieves state and length parameters
	int res;
	ssize_t end;
	u64 val;

	end = BEAT_RANGESZ(ctx, BEAT_PATCHFILE) - ctx->foffset[BEAT_PATCHFILE];

	if (end == ctx->eoal_offset) return BEAT_EOAL;
	if (end < ctx->eoal_offset) return BEAT_PATCH_EXPECT;

	res = BEAT_NextVLI(ctx, &val);
	*act = val & 3;
	*len = (val >> 2) + 1;
	return res;
}

static int BEAT_RunActions(BEAT_Context *ctx, const BEAT_Action *acts)
{ // Parses an action list and runs commands specified in `acts`
	int cmd;
	u64 len;

	while(1) {
		int res = BEAT_NextAction(&cmd, &len, ctx);
		if (res == BEAT_EOAL) return BEAT_EOAL; // End of patch
		if (res != BEAT_OK) return res; // Failed to get next action

		res = (acts[cmd])(ctx, len); // Execute next action
		if (res == BEAT_ABORTED) return BEAT_ABORTED; // Return on user abort
		if (res != BEAT_OK) return res; // Break on error
	}
}

static void BEAT_ReleaseCTX(BEAT_Context *ctx)
{ // Release any resources associated to the context
	free(ctx->copybuf);
	for (int i = 0; i < BEAT_FILECOUNT; i++) {
		if (fvx_opened(&ctx->file[i])) fvx_close(&ctx->file[i]);
	}
}

// BPS Specific functions
/**
 Initialize the Beat File Structure
 - verifies checksums
 - performs further sanity checks
 - extracts initial info
 - leaves the file ready to begin state machine execution
 */
static int BPS_InitCTX_Advanced(BEAT_Context *ctx, const char *bps_path, const char *in_path, const char *out_path, size_t start, size_t end)
{
	int res;
	u8 read_magic[4];
	u64 vli, in_sz, metaend_off;
	u32 chksum[BEAT_FILECOUNT], expected_chksum[BEAT_FILECOUNT];

	// Clear stackbuf
	memset(ctx, 0, sizeof(*ctx));
	ctx->eoal_offset = 12;

	if (end == 0) {
		start = 0;
		end = fs_size(bps_path);
	}

	// Get checksums of BPS and input files
	chksum[BEAT_PATCHFILE] = crc32_calculate_from_file(bps_path, start, end - start - 4);
	chksum[BEAT_SRCFILE] = crc32_calculate_from_file(in_path, 0, fs_size(in_path));

	// open all files
	fvx_open(&ctx->file[BEAT_PATCHFILE], bps_path, BEAT_READONLY);
	ctx->ranges[0][BEAT_PATCHFILE] = start;
	ctx->ranges[1][BEAT_PATCHFILE] = end;

	fvx_open(&ctx->file[BEAT_SRCFILE], in_path, BEAT_READONLY);
	ctx->ranges[0][BEAT_SRCFILE] = 0;
	ctx->ranges[1][BEAT_SRCFILE] = fs_size(in_path);

	res = fvx_open(&ctx->file[BEAT_DSTFILE], out_path, BEAT_RWCREATE);
	if (res != FR_OK) return BEAT_IO_ERROR;

	// Verify BPS1 header magic
	res = BEAT_Read(ctx, BEAT_PATCHFILE, read_magic, sizeof(read_magic), true);
	if (res != BEAT_OK) return BEAT_IO_ERROR;
	res = memcmp(read_magic, bps_signature, sizeof(bps_signature));
	if (res != 0) return BEAT_BADPATCH;

	// Check input size
	res = BEAT_NextVLI(ctx, &in_sz);
	if (res != BEAT_OK) return res;
	if (ctx->ranges[1][BEAT_SRCFILE] != in_sz) return BEAT_BADINPUT;

	// Get expected output size
	res = BEAT_NextVLI(ctx, &vli);
	if (res != BEAT_OK) return res;
	ctx->ranges[0][BEAT_DSTFILE] = 0;
	ctx->ranges[1][BEAT_DSTFILE] = vli;

	// Get end of metadata offset
	res = BEAT_NextVLI(ctx, &metaend_off);
	if (res != BEAT_OK) return res;
	metaend_off += ctx->foffset[BEAT_PATCHFILE];

	// Read checksums from BPS file
	for (int i = 0; i < BEAT_FILECOUNT; i++) {
		BEAT_SeekAbs(ctx, BEAT_PATCHFILE, ctx->ranges[1][BEAT_PATCHFILE] - ctx->ranges[0][BEAT_PATCHFILE] - bps_chksumoffs[i]);
		BEAT_Read(ctx, BEAT_PATCHFILE, &expected_chksum[i], sizeof(u32), false);
	}

	// Verify patch and input checksums
	if (chksum[BEAT_PATCHFILE] != expected_chksum[BEAT_PATCHFILE]) return BEAT_BADCHKSUM;
	if (chksum[BEAT_SRCFILE] != expected_chksum[BEAT_SRCFILE]) return BEAT_BADCHKSUM;

	// Initialize output checksums
	ctx->ocrc = 0;
	ctx->xocrc = expected_chksum[BEAT_DSTFILE];

	// Allocate temporary block copy buffer
	ctx->copybuf = malloc(STD_BUFFER_SIZE);
	if (ctx->copybuf == NULL) return BEAT_OUT_OF_MEMORY;

	// Seek back to the start of action stream / end of metadata
	BEAT_SeekAbs(ctx, BEAT_PATCHFILE, metaend_off);
	return BEAT_OK;
}

static int BPS_InitCTX(BEAT_Context *ctx, const char *bps_path, const char *in_path, const char *out_path)
{
	return BPS_InitCTX_Advanced(ctx, bps_path, in_path, out_path, 0, 0);
}

/*
 Generic helper function to copy from `src_id` to BEAT_DSTFILE
 Used by SourceRead, TargetRead and CreateFile
*/
static int BEAT_BlkCopy(BEAT_Context *ctx, int src_id, u64 len)
{
	while(len > 0) {
		ssize_t blksz = min(len, STD_BUFFER_SIZE);
		int res = BEAT_Read(ctx, src_id, ctx->copybuf, blksz, true);
		if (res != BEAT_OK) return res;

		res = BEAT_WriteOut(ctx, ctx->copybuf, blksz, true);
		if (res != BEAT_OK) return res;

		len -= blksz;
	}
	return BEAT_OK;
}

static int BPS_SourceRead(BEAT_Context *ctx, u64 len)
{ // This command copies bytes from the source file to the target file
	BEAT_SeekAbs(ctx, BEAT_SRCFILE, ctx->foffset[BEAT_DSTFILE]);
	return BEAT_BlkCopy(ctx, BEAT_SRCFILE, len);
}

/*
 [...] the actual data is not available to the patch applier,
 so it is stored directly inside the patch.
*/
static int BPS_TargetRead(BEAT_Context *ctx, u64 len)
{
	return BEAT_BlkCopy(ctx, BEAT_PATCHFILE, len);
}

/*
 An offset is supplied to seek the sourceRelativeOffset to the desired
 location, and then data is copied from said offset to the target file
*/
static int BPS_SourceCopy(BEAT_Context *ctx, u64 len)
{
	int res;
	u64 vli;
	s64 offset;

	res = BEAT_NextVLI(ctx, &vli);
	if (res != BEAT_OK) return res;

	offset = BEAT_DecodeSigned(vli);
	BEAT_SeekAbs(ctx, BEAT_SRCFILE, ctx->source_relative + offset);
	ctx->source_relative += offset + len;

	return BEAT_BlkCopy(ctx, BEAT_SRCFILE, len);
}

/* This command treats all of the data that has already been written to the target file as a dictionary */
static int BPS_TargetCopy(BEAT_Context *ctx, u64 len)
{ // the black sheep of the family, needs special care
	int res;
	s64 offset;
	u64 out_off, rel_off, vli;

	res = BEAT_NextVLI(ctx, &vli);
	if (res != BEAT_OK) return res;

	offset = BEAT_DecodeSigned(vli);
	out_off = ctx->foffset[BEAT_DSTFILE];
	rel_off = ctx->target_relative + offset;
	if (rel_off > out_off) return BEAT_BADPATCH; // Illegal

	while(len != 0) {
		u8 *remfill;
		ssize_t blksz, distance, remainder;

		blksz = min(len, STD_BUFFER_SIZE);
		distance = min((ssize_t)(out_off - rel_off), blksz);

		BEAT_SeekAbs(ctx, BEAT_DSTFILE, rel_off);
		res = BEAT_Read(ctx, BEAT_DSTFILE, ctx->copybuf, distance, false);
		if (res != BEAT_OK) return res;

		remfill = ctx->copybuf + distance;
		remainder = blksz - distance;
		while(remainder > 0) { // fill the buffer with repeats
			ssize_t remblk = min(distance, remainder);
			memcpy(remfill, ctx->copybuf, remblk);
			remfill += remblk;
			remainder -= remblk;
		}

		BEAT_SeekAbs(ctx, BEAT_DSTFILE, out_off);
		res = BEAT_WriteOut(ctx, ctx->copybuf, blksz, false);
		if (res != BEAT_OK) return res;

		rel_off += blksz;
		out_off += blksz;
		len -= blksz;
	}

	BEAT_SeekAbs(ctx, BEAT_DSTFILE, out_off);
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
	if (res == FR_OK) return BEAT_IO_ERROR;

	ctx->ranges[0][id] = 0;
	if (max_sz > 0) {
		ctx->ranges[1][id] = max_sz;
	} else {
		ctx->ranges[1][id] = f_size(&ctx->file[id]);
	}

	// if a new file is opened it makes no sense to keep the old CRC
	// a single outfile wont be created from more than one infile (& patch)
	ctx->ocrc = 0;
	ctx->foffset[id] = 0;
	return BEAT_OK;
}

static int BPM_InitCTX(BEAT_Context *ctx, const char *bpm_path, const char *src_dir, const char *dst_dir)
{
	int res;
	u64 metaend_off;
	u8 read_magic[4];
	u32 chksum, expected_chksum;

	memset(ctx, 0, sizeof(*ctx));

	ctx->bpm_path = bpm_path;
	ctx->source_dir = src_dir;
	ctx->target_dir = dst_dir;
	ctx->eoal_offset = 4;

	chksum = crc32_calculate_from_file(bpm_path, 0, fs_size(bpm_path) - 4);
	res = BPM_OpenFile(ctx, BEAT_PATCHFILE, bpm_path, 0);
	res = BEAT_Read(ctx, BEAT_PATCHFILE, read_magic, sizeof(read_magic), true);
	if (res != BEAT_OK) return res;
	res = memcmp(read_magic, bpm_signature, sizeof(bpm_signature));
	if (res != 0) return BEAT_BADPATCH;

	// Get end of metadata offset
	res = BEAT_NextVLI(ctx, &metaend_off);
	if (res != BEAT_OK) return res;
	metaend_off += ctx->foffset[BEAT_PATCHFILE];

	// Read checksums from BPS file
	BEAT_SeekAbs(ctx, BEAT_PATCHFILE, BEAT_RANGESZ(ctx, BEAT_PATCHFILE) - 4);
	res = BEAT_Read(ctx, BEAT_PATCHFILE, &expected_chksum, sizeof(u32), false);
	if (res != BEAT_OK) return res;
	if (expected_chksum != chksum) return BEAT_BADCHKSUM;

	// Allocate temporary block copy buffer
	ctx->copybuf = malloc(STD_BUFFER_SIZE);
	if (ctx->copybuf == NULL) return BEAT_OUT_OF_MEMORY;

	// Seek back to the start of action stream / end of metadata
	BEAT_SeekAbs(ctx, BEAT_PATCHFILE, metaend_off);
	return BEAT_OK;
}

static int BPM_NextPath(BEAT_Context *ctx, char *out, int name_len)
{
	if (name_len >= BEAT_MAXPATH) return BEAT_BADPATCH;
	int res = BEAT_Read(ctx, BEAT_PATCHFILE, out, name_len, true);
	out[name_len] = '\0'; // make sure the buffer ends with a zero char
	return res;
}

static int BPM_MakeRelativePaths(BEAT_Context *ctx, char *src, char *dst, int name_len)
{
	char name[BEAT_MAXPATH];
	int res = BPM_NextPath(ctx, name, name_len);
	if (res != BEAT_OK) return res;
	if (src != NULL)
		if (snprintf(src, BEAT_MAXPATH, "%s/%s", ctx->source_dir, name) < 0) return BEAT_BADPATCH;
	if (dst != NULL)
		if (snprintf(dst, BEAT_MAXPATH, "%s/%s", ctx->target_dir, name) < 0) return BEAT_BADPATCH;
	return res;
}

static int BPM_CreatePath(BEAT_Context *ctx, u64 name_len)
{ // Create a directory
	char path[BEAT_MAXPATH];
	int res = BPM_MakeRelativePaths(ctx, NULL, path, name_len);
	if (res != BEAT_OK) return res;

	res = fvx_mkdir(path);
	if (res != FR_OK && res != FR_EXIST) return BEAT_IO_ERROR;
	return BEAT_OK;
}

static int BPM_CreateFile(BEAT_Context *ctx, u64 name_len)
{ // Create a file and fill it with data provided in the BPM
	u64 file_sz;
	u32 checksum;
	char path[BEAT_MAXPATH];

	int res = BPM_MakeRelativePaths(ctx, NULL, path, name_len);
	if (res != BEAT_OK) return res;

	res = BEAT_NextVLI(ctx, &file_sz); // get new file size
	if (res != BEAT_OK) return res;
	res = BPM_OpenFile(ctx, BEAT_DSTFILE, path, file_sz); // open file as RW
	if (res != BEAT_OK) return res;
	res = BEAT_BlkCopy(ctx, BEAT_PATCHFILE, file_sz); // copy data to new file
	if (res != BEAT_OK) return res;

	res = BEAT_Read(ctx, BEAT_PATCHFILE, &checksum, sizeof(u32), true);
	if (res != BEAT_OK) return res;
	if (ctx->ocrc != checksum) return BEAT_BADOUTPUT; // get and check CRC32
	return BEAT_OK;
}

static int BPM_ModifyFile(BEAT_Context *ctx, u64 name_len)
{ // Apply a BPS patch
	u64 origin, bps_sz;
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
		ctx->foffset[BEAT_PATCHFILE], ctx->foffset[BEAT_PATCHFILE] + bps_sz
	); // create a BPS context using the current ranges
	if (res == BEAT_OK) res = BPS_RunActions(&bps_context); // run if OK
	BEAT_ReleaseCTX(&bps_context);
	if (res != BEAT_OK) return res; // break off if there was an error

	BEAT_SeekOff(ctx, BEAT_PATCHFILE, bps_sz); // advance beyond the BPS
	return BEAT_OK;
}

static int BPM_MirrorFile(BEAT_Context *ctx, u64 name_len)
{ // Copy a file from source to target without any modifications
	u64 origin;
	u32 checksum;
	char src[BEAT_MAXPATH], dst[BEAT_MAXPATH];

	int res = BPM_MakeRelativePaths(ctx, src, dst, name_len);
	if (res != BEAT_OK) return res;

	// open source and destination files, read the origin dummy
	res = BPM_OpenFile(ctx, BEAT_SRCFILE, src, 0);
	if (res != BEAT_OK) return res;
	res = BPM_OpenFile(ctx, BEAT_DSTFILE, dst, ctx->ranges[1][BEAT_SRCFILE]);
	if (res != BEAT_OK) return res;
	res = BEAT_NextVLI(ctx, &origin);
	if (res != BEAT_OK) return res;

	// copy straight from source to destination
	res = BEAT_BlkCopy(ctx, BEAT_SRCFILE, ctx->ranges[1][BEAT_SRCFILE]);
	if (res != BEAT_OK) return res;

	res = BEAT_Read(ctx, BEAT_PATCHFILE, &checksum, sizeof(u32), true);
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
	res = (bpm ? BPM_InitCTX : BPS_InitCTX)(&ctx, p, s, d);
	if (res != BEAT_OK) {
		ShowPrompt(false, "Failed to initialize %s file:\n%s",
			bpm ? "BPM" : "BPS", BEAT_ErrString(res));
	} else {
		res = (bpm ? BPM_RunActions : BPS_RunActions)(&ctx);
		switch(res) {
			case BEAT_OK:
				ShowPrompt(false, "Successfully patched");
				break;
			case BEAT_ABORTED:
				ShowPrompt(false, "Patching aborted by user");
				break;
			default:
				ShowPrompt(false, "Failed to run patch:\n%s", BEAT_ErrString(res));
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
