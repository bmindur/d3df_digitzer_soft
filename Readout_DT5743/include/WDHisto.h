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

#ifndef _WDHISTO_H
#define _WDHISTO_H

#include "WaveDemo.h"

#define HISTO2D_NBINX		1024	// Num of bins in the x axes of the scatter plot
#define HISTO2D_NBINY		1024	// Num of bins in the y axes of the scatter plot

//****************************************************************************
// Function prototypes
//****************************************************************************
int CreateHistograms(uint32_t *AllocatedSize);
int DestroyHistograms();
int ResetHistograms();
int Histo1D_AddCount(Histogram1D_t *Histo, int Bin);
int Histo2D_AddCount(Histogram2D_t *Histo, int BinX, int BinY);

#endif