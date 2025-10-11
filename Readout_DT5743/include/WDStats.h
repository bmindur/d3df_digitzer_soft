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

#ifndef _WDSTATS_H
#define _WDSTATS_H

#include "WaveDemo.h"

/* ###########################################################################
*  Functions
*  ########################################################################### */

// --------------------------------------------------------------------------------------------------------- 
// Description: Reset all the counters, histograms, etc...
// Return: 0=OK, -1=error
// --------------------------------------------------------------------------------------------------------- 
int ResetStatistics();

// --------------------------------------------------------------------------------------------------------- 
// Description: Calculate some statistcs (rates, etc...) in the Stats struct
// Return: 0=OK, -1=error
// --------------------------------------------------------------------------------------------------------- 
int UpdateStatistics(uint64_t CurrentTime);

#endif