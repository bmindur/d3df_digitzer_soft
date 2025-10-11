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

#include "keyb.h"
#include "WDLogs.h"

#ifdef LINUX
// --------------------------------------------------------------------------------------------------------- 
//  Init console window (terminal)
// --------------------------------------------------------------------------------------------------------- 
int InitConsole()
{
	return 0;
}
// --------------------------------------------------------------------------------------------------------- 
// Description: clear the console
// --------------------------------------------------------------------------------------------------------- 
void ClearScreen()
{
	system("clear");
}
#else  // Windows
// --------------------------------------------------------------------------------------------------------- 
//  Init console window (terminal)
// --------------------------------------------------------------------------------------------------------- 
int InitConsole()
{
	// Set console window size
	system("mode con: cols=100 lines=50");
	return 0;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: clear the console
// --------------------------------------------------------------------------------------------------------- 
void ClearScreen()
{
	system("cls");
}
#endif


/*! \fn      int msg_printf(FILE *MsgLog, char *fmt, ...)
*   \brief   Printf to screen and to log file if not null
*
*   \return  0: OK
*/
int msg_printf(FILE *fLog, char *fmt, ...) {
	va_list args;
	if (fLog) {
		va_start(args, fmt);
		vfprintf(fLog, fmt, args);
		fflush(fLog);
		va_end(args);
	}
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
	return 0;
}

void addProgressIndicator(int *progress) {
	if (*progress | 0x8) {
		int val = *progress & 0x7;
		*progress = (val + 1) % 4;
	}
}
char getProgressIndicator(int *progress) {
	int val = *progress & 0x7;
	*progress |= 0x8;
	switch (val)
	{
	case 0:
		return '-';
	case 1:
		return '\\';
	case 2:
		return '|';
	case 3:
		return '/';
	default:
		*progress = 0x7;
		break;
	}
	return 'X';
}
