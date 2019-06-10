#include "cmd.h"


u32 CheckCmdSize(CmdHeader* cmd, u64 fsize) {
	u64 cmdsize = sizeof(CmdHeader) +
		(cmd->n_entries * sizeof(u32)) +
		(cmd->n_cmacs   * sizeof(u32)) +
		(cmd->n_entries * 0x10);

	return (fsize == cmdsize) ? 0 : 1;
}
