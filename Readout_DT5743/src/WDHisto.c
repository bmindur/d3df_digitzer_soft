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

#include "WDHisto.h"

static int CreateHistogram1D(int Nbin, char *Title, char *Xlabel, char *Ylabel, Histogram1D_t *Histo) {
	Histo->H_data = (uint32_t *)malloc(Nbin * sizeof(uint32_t));
	Histo->Nbin = Nbin;
	Histo->H_cnt = 0;
	Histo->Ovf_cnt = 0;
	Histo->Unf_cnt = 0;
	Histo->mean = 0;
	Histo->rms = 0;
	return 0;
}

static int CreateHistogram2D(int NbinX, int NbinY, char *Title, char *Xlabel, char *Ylabel, Histogram2D_t *Histo) {
	Histo->H_data = (uint32_t *)malloc(NbinX * NbinY * sizeof(uint32_t));
	Histo->NbinX = NbinX;
	Histo->NbinY = NbinY;
	Histo->H_cnt = 0;
	Histo->Ovf_cnt = 0;
	Histo->Unf_cnt = 0;
	return 0;
}

static int DestroyHistogram2D(Histogram2D_t Histo) {
	free(Histo.H_data);
	Histo.H_data = NULL;
	return 0;
}

static int DestroyHistogram1D(Histogram1D_t Histo) {
	free(Histo.H_data);
	Histo.H_data = NULL;
	return 0;
}

static int ResetHistogram1D(Histogram1D_t *Histo) {
	memset(Histo->H_data, 0, Histo->Nbin * sizeof(uint32_t));
	Histo->H_cnt = 0;
	Histo->Ovf_cnt = 0;
	Histo->Unf_cnt = 0;
	Histo->rms = 0;
	Histo->mean = 0;
	return 0;
}

static int ResetHistogram2D(Histogram2D_t *Histo) {
	memset(Histo->H_data, 0, Histo->NbinX * Histo->NbinY * sizeof(uint32_t));
	Histo->H_cnt = 0;
	Histo->Ovf_cnt = 0;
	Histo->Unf_cnt = 0;
	return 0;
}


// --------------------------------------------------------------------------------------------------------- 
// Description: Create histograms and allocate the memory
// Return:		0=OK, -1=error
// --------------------------------------------------------------------------------------------------------- 
int CreateHistograms(uint32_t *AllocatedSize) {
	int b, ch;

	*AllocatedSize = 0;
	for (b = 0; b < WDcfg.NumBoards; b++) {
		for (ch = 0; ch < WDcfg.handles[b].Nch; ch++) {
			if (WDcfg.boards[b].channels[ch].ChannelEnable || b >= WDcfg.NumBoards) {
				CreateHistogram1D(WDcfg.EHnbin, "Charge", "Channels", "Cnt", &WDhistos.EH[b][ch]);
				*AllocatedSize += WDcfg.EHnbin * sizeof(uint32_t);
				CreateHistogram1D(WDcfg.THnbin, "TAC", "ns", "Cnt", &WDhistos.TH[b][ch]);
				*AllocatedSize += WDcfg.THnbin * sizeof(uint32_t);
			}
		}
	}
	ResetHistograms();
	return 0;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Destroy (free memory) the histograms
// Return:		0=OK, -1=error
// --------------------------------------------------------------------------------------------------------- 
int DestroyHistograms() {
	int b, ch;
	for (b = 0; b < WDcfg.NumBoards; b++) {
		for (ch = 0; ch < WDcfg.handles[b].Nch; ch++) {
			if (WDcfg.boards[b].channels[ch].ChannelEnable || b >= WDcfg.NumBoards) {
				DestroyHistogram1D(WDhistos.EH[b][ch]);
				DestroyHistogram1D(WDhistos.TH[b][ch]);
			}
		}
	}
	return 0;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Reset the histograms
// Return:		0=OK, -1=error
// --------------------------------------------------------------------------------------------------------- 
int ResetHistograms()
{
	int b, ch;
	for (b = 0; b < WDcfg.NumBoards; b++) {
		for (ch = 0; ch < WDcfg.handles[b].Nch; ch++) {
			if (WDcfg.boards[b].channels[ch].ChannelEnable || b >= WDcfg.NumBoards) {
				ResetHistogram1D(&WDhistos.EH[b][ch]);
				ResetHistogram1D(&WDhistos.TH[b][ch]);
			}
		}
	}
	return 0;
}


// --------------------------------------------------------------------------------------------------------- 
// Description: Add one count to the histogram 1D
// Return:		0=OK, -1=under/over flow
// --------------------------------------------------------------------------------------------------------- 
int Histo1D_AddCount(Histogram1D_t *Histo, int Bin) {
	if (Bin < 0) {
		Histo->Unf_cnt++;
		return -1;
	}
	else if (Bin >= (int)(Histo->Nbin - 1)) {
		Histo->Ovf_cnt++;
		return -1;
	}
	Histo->H_data[Bin]++;
	Histo->H_cnt++;
	Histo->mean += (double)Bin;
	Histo->rms += (double)(Bin*Bin);
	return 0;
}


// --------------------------------------------------------------------------------------------------------- 
// Description: Add one count to the histogram 1D
// Return:		0=OK, -1=error
// --------------------------------------------------------------------------------------------------------- 
int Histo2D_AddCount(Histogram2D_t *Histo, int BinX, int BinY) {
	if ((BinX >= (int)(Histo->NbinX - 1)) || (BinY >= (int)(Histo->NbinY - 1))) {
		Histo->Ovf_cnt++;
		return -1;
	}
	else if ((BinX < 0) || (BinY < 0)) {
		Histo->Unf_cnt++;
		return -1;
	}
	Histo->H_data[BinX + HISTO2D_NBINY * BinY]++;
	Histo->H_cnt++;
	return 0;
}