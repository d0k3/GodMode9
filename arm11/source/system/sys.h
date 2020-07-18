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
void __attribute__((noreturn)) SYS_CoreShutdown(void);
