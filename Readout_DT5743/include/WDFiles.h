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

#ifndef _WDFILES_H
#define _WDFILES_H                    // Protect against multiple inclusion

#include "WaveDemo.h"

# define DATA_FILE_FORMAT_VERSION 0x1

//****************************************************************************
// Function prototypes
//****************************************************************************
int OpenOutputDataFiles();
int CheckOutputDataFilePresence();
int CloseOutputDataFiles();
int SaveAllHistograms();
int ReadRawData(FILE* inputFile, WaveDemoEvent_t *eventPtr[MAX_BD], int printFlag);
int SaveRawData(int bd, const char channelsEnabled[MAX_CH], WaveDemoEvent_t* event);
int SaveTDCList(int bd, int ch, WaveDemoEvent_t* event);
int SaveWaveform(int bd, int ch, WaveDemoEvent_t* event);
int SaveList(int bd, int ch, WaveDemoEvent_t* event);
int SaveRunInfo(char* ConfigFileName);
int SaveRegImage(int handle);
void PrintOutputFilesSummary();

#endif