/******************************************************************************
*
* Copyright (C) 2017 CAEN SpA - www.caen.it - support.computing@caen.it
*
***************************************************************************//**
* \note TERMS OF USE:
* This file is subject to the terms and conditions defined in file
* 'CAEN_License_Agreement.txt', which is part of this source code package.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. The user relies on the
* software, documentation and results solely at his own risk.
******************************************************************************/

#ifndef _WDLOGS_H
#define _WDLOGS_H

#include "WaveDemo.h"

int InitConsole();
void ClearScreen();

int msg_printf(FILE *fLog, char *fmt, ...);

void addProgressIndicator(int *progress);
char getProgressIndicator(int *progress);

#endif