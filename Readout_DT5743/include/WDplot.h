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


#ifndef __WDPLOT_H
#define __WDPLOT_H



#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CAENDigitizerType.h"

#ifdef WIN32
	#include <windows.h>
	#include <process.h>
	#define popen  _popen    /* redefine POSIX 'deprecated' popen as _popen */
	#define pclose  _pclose  /* redefine POSIX 'deprecated' pclose as _pclose */
#else

#endif

/* Defines */
#define PLOT_WAVES_DATA_FILE	"PlotWavesData.txt"
#define PLOT_HISTO_DATA_FILE	"PlotHistoData.txt"

#ifdef WIN32
/******************************************************************************
* Executable file for 'gnuplot'
* NOTE: use pgnuplot instead of wgnuplot under Windows, otherwise the pipe
*       will not work
******************************************************************************/
#define GNUPLOT_COMMAND  "pgnuplot"
#else
#define GNUPLOT_COMMAND  "gnuplot"
#endif


#define MAX_NUM_TRACES    16     /* Maximum number of traces in a plot */

typedef enum {
	PLOT_DATA_UINT8		= 0,
	PLOT_DATA_UINT16	= 1,
	PLOT_DATA_UINT32	= 2,
	PLOT_DATA_DOUBLE	= 3,
	PLOT_DATA_FLOAT		= 4, 
} PlotDataType_t;

typedef struct {
	char              Title[100];
	char              TraceName[MAX_NUM_TRACES][100];
	char              Xlabel[100];
	char              Ylabel[100];
	int               Xautoscale;
	int               Yautoscale;
	float             Xscale;
	float             Yscale;
	float             Xmax;
	float             Ymax;
	float             Xmin;
	float             Ymin;
	int               NumTraces;
	int               TraceSize[MAX_NUM_TRACES];
	void              *TraceData[MAX_NUM_TRACES];
	int               TraceXOffset[MAX_NUM_TRACES];
	PlotDataType_t    DataType;
	float vertical_line;
} WDPlot_t;


/* Functions */
WDPlot_t *OpenPlotter(char *Path, int NumTraces, int MaxTraceLenght);
int SetPlotOptions();
int IsPlotterBusy();
void ClearPlot();
int PlotWaveforms();

int OpenPlotter2();
int PlotHisto(uint32_t *Histo, int Nbin, float Xmin, float Xmax, char *title, char *xlabel);
int PlotSelectedHisto(int HistoPlotType, int Xunits);
void ClearHistoPlot();
int ClosePlotter();

#endif // __WDPLOT_H
