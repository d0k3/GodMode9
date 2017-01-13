
#pragma once

void cardWriteCommand(const u8 *command);
void cardPolledTransfer(u32 flags, u32 *destination, u32 length, const u8 *command);
void cardStartTransfer(const u8 *command, u32 *destination, int channel, u32 flags);
u32 cardWriteAndRead(const u8 *command, u32 flags);
void cardParamCommand (u8 command, u32 parameter, u32 flags, u32 *destination, u32 length);
void cardReadHeader(u8 *header);
u32 cardReadID(u32 flags);
void cardReset();
