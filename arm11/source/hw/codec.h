#pragma once

#include <types.h>

typedef struct {
	s16 cpad_x, cpad_y;
	s16 ts_x, ts_y;
} CODEC_Input;

void CODEC_Init(void);

void CODEC_GetRawData(u32 *buffer);
void CODEC_Get(CODEC_Input *input);
