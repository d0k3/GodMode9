#pragma once

#include <types.h>

/*
 how to run the SYS_Core(Zero){Init,Shutdown} functions:
 for init:
  - FIRST run CoreZeroInit ONCE
  - all cores must run CoreInit ONCE

 for shutdown:
  - all non-zero cores must call CoreShutdown
  - core zero must call CoreZeroShutdown, then CoreShutdown
*/

void SYS_CoreZeroInit(void);
void SYS_CoreInit(void);

void SYS_CoreZeroShutdown(void);
void SYS_CoreShutdown(void);
