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
* -----------------------------------------------------------------------------
* WDplot is a library that allows WaveDemo to plot the waveforms and histograms.
* It saves the data read from the digitizer into a text file and sends the
* plotting commands to gnuplot through a pipe. Thus, WDplot is just a simple
* server that transfers the data and the commands from WaveDemo to gnuplot,
* which is the actual plotting tool.
******************************************************************************/
#ifdef WIN32
#include <sys/timeb.h>
#include <time.h>
#else
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h> 
#include <pthread.h>
#endif

#include "WDplot.h"
#include "WaveDemo.h"

/* Global Variables */
WDPlot_t PlotVar;
int Busy = 0;
int SetOption = 1;
long Tfinish;
FILE *wplot = NULL, *hplot = NULL; // gnuplot pipes
int LastHPlotType = -1;
float GnuplotVersion = 0;

#ifdef WIN32
#else
#define Sleep(t) usleep((t)*1000);
#endif

static long get_time() {
	long time_ms;
#ifdef WIN32
	struct _timeb timebuffer;
	_ftime(&timebuffer);
	time_ms = (long)timebuffer.time * 1000 + (long)timebuffer.millitm;
#else
	struct timeval t1;
	gettimeofday(&t1, NULL);
	time_ms = (t1.tv_sec) * 1000 + t1.tv_usec / 1000;
#endif
	return time_ms;
}

static int OpenGnuplot(FILE **gplot) {
	FILE *vars = NULL;
	char str[1000] = { 0 };
	int i;

	if (*gplot != NULL) return 0;
	*gplot = popen(GNUPLOT_COMMAND, "w");
	if (*gplot == NULL) return -1;

	if (GnuplotVersion == 0) {
		vars = fopen("gpvars.txt", "r");
		if (vars != NULL) {
			fclose(vars);
			system("del gpvars.txt");
		}
		fprintf(*gplot, "save var 'gpvars.txt'\n");
		fflush(*gplot);
		for (i = 0; i < 200; i++) {
			vars = fopen("gpvars.txt", "r");
			if (vars != NULL) break;
			Sleep(10);
		}
		if (vars == NULL) return -1;
		Sleep(50);
		while (!feof(vars)) {
			if (fscanf(vars, "%s", str) == 1) {
				if (strcmp(str, "Version") == 0) {
					if (fscanf(vars, "%f", &GnuplotVersion) == 1) {
						break;
					}
				}
			}
		}
		fclose(vars);
		printf("INFO: using gunplot Ver. %.1f\n", GnuplotVersion);
		//if (GnuplotVersion == 0) return -1;
	}
	return 0;
}



void ClearPlot() {
	if (wplot != NULL) {
		fprintf(wplot, "clear\n");
		fflush(wplot);
	}
}

int SetPlotOptions() {
	fprintf(wplot, "reset\n");
	fprintf(wplot, "set grid\n");
	fprintf(wplot, "set mouse\n");
	fprintf(wplot, "bind y 'set yrange [Ymin:Ymax]'\n");
	fprintf(wplot, "bind x 'set xrange [Xmin:Xmax]'\n");
	fprintf(wplot, "set xlabel '%s'\n", PlotVar.Xlabel);
	fprintf(wplot, "set ylabel '%s'\n", PlotVar.Ylabel);
	fprintf(wplot, "set title '%s'\n", PlotVar.Title);
	fprintf(wplot, "Xs = %f\n", PlotVar.Xscale);
	fprintf(wplot, "Ys = %f\n", PlotVar.Yscale);
	fprintf(wplot, "Xmax = %f\n", PlotVar.Xmax);
	fprintf(wplot, "Ymax = %f\n", PlotVar.Ymax);
	fprintf(wplot, "Xmin = %f\n", PlotVar.Xmin);
	fprintf(wplot, "Ymin = %f\n", PlotVar.Ymin);
	if (PlotVar.Xautoscale)
		fprintf(wplot, "set autoscale x\n");
	else
		fprintf(wplot, "set xrange [Xmin:Xmax]\n");
	if (PlotVar.Yautoscale)
		fprintf(wplot, "set autoscale y\n");
	else
		fprintf(wplot, "set yrange [Ymin:Ymax]\n");
	if (PlotVar.vertical_line != 0)
		fprintf(wplot, "set arrow from %.2f, graph 0 to %.2f, graph 1 nohead\n", PlotVar.vertical_line, PlotVar.vertical_line);
	fflush(wplot);
	SetOption = 0;
	return 0;
}

int PlotWaveforms() {
	int i, s = 0, ntr, comma = 0, c, npts = 0, WaitTime;
	FILE *fplot;

	Busy = 1;
	if (SetOption)
		SetPlotOptions();
	fplot = fopen(PLOT_WAVES_DATA_FILE, "w");
	if (fplot == NULL) {
		Busy = 0;
		return -1;
	}

	ntr = PlotVar.NumTraces;
	while (ntr > 0) {
		fprintf(fplot, "%d\t", s);
		for (i = 0; i < PlotVar.NumTraces; i++) {
			if (s < PlotVar.TraceSize[i]) {
				if (PlotVar.DataType == PLOT_DATA_UINT8) {
					uint8_t *data = (uint8_t *)PlotVar.TraceData[i];
					fprintf(fplot, "%d\t", data[s]);
				}
				else if (PlotVar.DataType == PLOT_DATA_UINT16) {
					uint16_t *data = (uint16_t *)PlotVar.TraceData[i];
					fprintf(fplot, "%d\t", data[s]);
				}
				else if (PlotVar.DataType == PLOT_DATA_UINT32) {
					uint32_t *data = (uint32_t *)PlotVar.TraceData[i];
					fprintf(fplot, "%d\t", data[s]);
				}
				else if (PlotVar.DataType == PLOT_DATA_DOUBLE) {
					double *data = (double *)PlotVar.TraceData[i];
					fprintf(fplot, "%f\t", data[s]);
				}
				else if (PlotVar.DataType == PLOT_DATA_FLOAT) {
					float *data = (float *)PlotVar.TraceData[i];
					fprintf(fplot, "%f\t", data[s]);
				}
				npts++;
			}
			if (PlotVar.TraceSize[i] == (s - 1))
				ntr--;
		}
		s++;
		fprintf(fplot, "\n");
	}
	fclose(fplot);

	/* str contains the plot command for gnuplot */
	fprintf(wplot, "plot ");
	c = 2; /* first column of data */
	for (i = 0; i < PlotVar.NumTraces; i++) {
		if (comma)
			fprintf(wplot, ", ");
		fprintf(wplot, "'%s' using ($1*%f+%d):($%d*%f) title '%s' with step linecolor %d ", PLOT_WAVES_DATA_FILE, PlotVar.Xscale, PlotVar.TraceXOffset[i], c++, PlotVar.Yscale, PlotVar.TraceName[i], i + 1);
		comma = 1;
	}
	fprintf(wplot, "\n");
	fflush(wplot);

	/* set the time gnuplot will have finished */
	WaitTime = npts / 20;
	if (WaitTime < 100)
		WaitTime = 100;
	Tfinish = get_time() + WaitTime;
	return 0;
}

/**************************************************************************//**
* \fn      int OpenPlotter(void)
* \brief   Open the plotter (i.e. open gnuplot with a pipe)
* \return  0: Success; -1: Error
******************************************************************************/
WDPlot_t *OpenPlotter(char *Path, int NumTraces, int MaxTraceLenght)
{
	char str[1000];
	int i;

	if (NumTraces > MAX_NUM_TRACES)
		return NULL;

	strcpy(str, Path);
	strcat(str, GNUPLOT_COMMAND);
	if ((wplot = popen(str, "w")) == NULL) // open the pipe
		return NULL;

	/* send some plot settings */
	fprintf(wplot, "set grid\n");
	fprintf(wplot, "set mouse\n");
	fprintf(wplot, "bind y 'set yrange [Ymin:Ymax]'\n");
	fprintf(wplot, "bind x 'set xrange [Xmin:Xmax]'\n");
	fflush(wplot);

	/* set default parameters */
	strcpy(PlotVar.Title, "");
	strcpy(PlotVar.Xlabel, "");
	strcpy(PlotVar.Ylabel, "");
	for (i = 0; i < NumTraces; i++)
		strcpy(PlotVar.TraceName[i], "");
	PlotVar.Xscale = 1.0;
	PlotVar.Yscale = 1.0;
	PlotVar.Xmax = 16384;
	PlotVar.Ymax = 16384;
	PlotVar.Xmin = 0;
	PlotVar.Ymin = 0;
	PlotVar.NumTraces = 0;
	for (i = 0; i < NumTraces; i++)
		PlotVar.TraceXOffset[i] = 0;

	/* allocate data buffers (use 4 byte per point) */
	for (i = 0; i < NumTraces; i++) {
		PlotVar.TraceData[i] = malloc(MaxTraceLenght * sizeof(double));  // double is the maximum size
		if (PlotVar.TraceData[i] == NULL) {
			int j;
			for (j = 0; j < i; i++) {
				free(PlotVar.TraceData[j]);
				PlotVar.TraceData[j] = NULL;
			}
			break;
		}
	}
	if (i < NumTraces)
		return NULL;

	return &PlotVar;
}

/**************************************************************************//**
* \fn      void ClosePlotter(void)
* \brief   Close the plotter for waveforms and histograms (gnuplot pipe)
* \return  0: Success; -1: Error
******************************************************************************/
int ClosePlotter() {
	if (wplot != NULL) {
		pclose(wplot);
	}
	if (hplot != NULL) {
		fprintf(hplot, "quit\n");
		fflush(hplot);
		SLEEP(100);
		//fclose(hplot);
		pclose(hplot);
	}
	wplot = NULL;
	hplot = NULL;

	for (int i = 0; i < MAX_NUM_TRACES; i++) {
		free(PlotVar.TraceData[i]);
		PlotVar.TraceData[i] = NULL;
	}

	return 0;
}

/**************************************************************************//**
* \brief   Check if plot has finished
******************************************************************************/
int IsPlotterBusy() {
	if (get_time() > Tfinish)
		Busy = 0;
	return Busy;
}



// --------------------------------------------------------------------------------------------------------- 
// Description: Open gnuplot for histograms
// Return:		0=OK, -1=error
// --------------------------------------------------------------------------------------------------------- 
int OpenPlotter2() {
	/* open gnuplot in a pipe and the data file */
	if (OpenGnuplot(&hplot) < 0) {
		printf("WARNING: Can't open gnuplot for histograms\n");
	}
	if (hplot == NULL)
		return -1;
	if (GnuplotVersion >= 5.0)
		fprintf(hplot, "set terminal wxt noraise title 'Spectra' size 1200,800 position 680,30\n");
	fprintf(hplot, "set grid\n");
	fprintf(hplot, "set title 'Board %d - Channel %d'\n", 0, 0);
	fprintf(hplot, "set mouse\n");
	fprintf(hplot, "bind y 'set autoscale y'\n");
	fprintf(hplot, "bind x 'set autoscale x'\n");
	fprintf(hplot, "xc = 0\n");
	fprintf(hplot, "yc = 0\n");
	fprintf(hplot, "bind \"Button1\" 'unset arrow; xc = MOUSE_X; yc = MOUSE_Y; set arrow from xc, graph 0 to xc, graph 1 nohead; replot'\n");
	fprintf(hplot, "bind + 'set xrange [xc - (GPVAL_X_MAX-GPVAL_X_MIN)/4: xc + (GPVAL_X_MAX-GPVAL_X_MIN)/4]; replot'\n");
	fprintf(hplot, "bind - 'set xrange [xc - (GPVAL_X_MAX-GPVAL_X_MIN): xc + (GPVAL_X_MAX-GPVAL_X_MIN)]; replot'\n");
	fprintf(hplot, "bind \"Up\" 'set yrange [GPVAL_Y_MIN: GPVAL_Y_MAX/2]; replot'\n");
	fprintf(hplot, "bind \"Down\" 'set yrange [GPVAL_Y_MIN: GPVAL_Y_MAX*2]; replot'\n");
	fflush(hplot);

	return 0;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Plot one histogram
// Inputs:		hplot = gnuplot pipe
//				Histo = histogram to plot
//				Nbin = number of bins of the histogram
//				title = title of the plot
// Return:		0=OK, -1=error
// --------------------------------------------------------------------------------------------------------- 
int PlotHisto(uint32_t *Histo, int Nbin, float Xmin, float Xmax, char *title, char *xlabel) {
	int i;
	static int WasBelow10 = 0;
	uint32_t hmax = 0;
	FILE *phdata;
	float xa, xb;

	xb = Xmin;
	xa = (Xmax - Xmin) / Nbin;
	phdata = fopen(PLOT_HISTO_DATA_FILE, "w");
	if (phdata == NULL) {
		printf("Can't open plot data file\n");
		return -1;
	}
	for (i = 0; i < Nbin; i++) {
		fprintf(phdata, "%d  \n", Histo[i]);
		hmax = max(Histo[i], hmax);
	}
	fclose(phdata);
	fprintf(hplot, "set title '%s'\n", title);
	fprintf(hplot, "set xlabel '%s'\n", xlabel);
	fprintf(hplot, "set ylabel 'Counts'\n");
	if (hmax < 10) {
		fprintf(hplot, "set yrange [0:10]\n");
		WasBelow10 = 1;
	}
	else if (WasBelow10) {
		fprintf(hplot, "set autoscale y\n");
		WasBelow10 = 0;
	}
	fprintf(hplot, "plot '%s' using ($0*%f+%f):($1) title 'BinSize = %f' with step\n", PLOT_HISTO_DATA_FILE, xa, xb, xa);
	fflush(hplot);
	return 0;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Plot the selected histogram (energy or time)
// Inputs:		HistoPlotType: gnuplot pipe
//				Xunits: 0=channels; 1=physic units (KeV for energy, ns for time)
// Return:		0=OK, -1=error
// --------------------------------------------------------------------------------------------------------- 
int PlotSelectedHisto(int HistoPlotType, int Xunits) {
	char title[500], xlabel[500];
	double mean, rms, oup = 0;
	uint32_t ovf, cnt;

	WaveDemoChannel_t *WDChToPlot = &WDcfg.boards[WDrun.BrdToPlot].channels[WDrun.ChToPlot];

	if (!WDChToPlot->ChannelEnable || (WDrun.BrdToPlot >= WDcfg.NumBoards)) {
		int ch, b;
		for (b = 0; b < WDcfg.NumBoards; b++) {
			for (ch = 0; ch < MAX_CH; ch++) {
				if (WDcfg.boards[b].channels[ch].ChannelEnable) {
					WDrun.BrdToPlot = b;
					WDrun.ChToPlot = ch;
					printf("WARNING: the selected channel for plot is disabled; now plotting BD %d - CH %d\n", WDrun.BrdToPlot, WDrun.ChToPlot);
					b = WDcfg.NumBoards; //to exit the outer loop
					break;
				}
			}
		}
	}

	const int BrdToPlot = WDrun.BrdToPlot;
	const int ChToPlot = WDrun.ChToPlot;

	if (HistoPlotType == HPLOT_ENERGY) {
		float m = WDChToPlot->ECalibration_m;
		float q = WDChToPlot->ECalibration_q;
		if (LastHPlotType != HPLOT_ENERGY) {
			fprintf(hplot, "set xrange [0:%d]\n", WDcfg.EHnbin);
			fprintf(hplot, "set autoscale y\n");
			LastHPlotType = HPLOT_ENERGY;
		}
		mean = WDhistos.EH[BrdToPlot][ChToPlot].mean / WDhistos.EH[BrdToPlot][ChToPlot].H_cnt;
		rms = sqrt(WDhistos.EH[BrdToPlot][ChToPlot].rms / WDhistos.EH[BrdToPlot][ChToPlot].H_cnt - mean*mean);
		ovf = WDhistos.EH[BrdToPlot][ChToPlot].Ovf_cnt + WDhistos.EH[BrdToPlot][ChToPlot].Unf_cnt;
		cnt = WDhistos.EH[BrdToPlot][ChToPlot].H_cnt;
		if ((ovf + cnt) > 0)	oup = ovf * 100.0 / (ovf + cnt);
		sprintf(title, "ENERGY Brd-%d Ch-%d: Cnt=%d Ovf=%.1f%% - M=%.3f S=%.2f", BrdToPlot, ChToPlot, WDhistos.EH[BrdToPlot][ChToPlot].H_cnt, oup, mean, rms);
		if ((Xunits) && !((m == 1) && (q == 0))) {
			sprintf(xlabel, "keV");
			PlotHisto(WDhistos.EH[BrdToPlot][ChToPlot].H_data, WDcfg.EHnbin, q, WDcfg.EHnbin * m + q, title, xlabel);
		}
		else {
			sprintf(xlabel, "Channels");
			PlotHisto(WDhistos.EH[BrdToPlot][ChToPlot].H_data, WDcfg.EHnbin, 0, (float)WDcfg.EHnbin, title, xlabel);
		}

	}
	else if (HistoPlotType == HPLOT_TIME) {
		mean = WDhistos.TH[BrdToPlot][ChToPlot].mean / WDhistos.TH[BrdToPlot][ChToPlot].H_cnt;
		rms = sqrt(WDhistos.TH[BrdToPlot][ChToPlot].rms / WDhistos.TH[BrdToPlot][ChToPlot].H_cnt - mean*mean);
		ovf = WDhistos.TH[BrdToPlot][ChToPlot].Ovf_cnt + WDhistos.TH[BrdToPlot][ChToPlot].Unf_cnt;
		cnt = WDhistos.TH[BrdToPlot][ChToPlot].H_cnt;
		if (LastHPlotType != HPLOT_TIME) {
			fprintf(hplot, "set autoscale x\n");
			fprintf(hplot, "set autoscale y\n");
			LastHPlotType = HPLOT_TIME;
		}
		if ((ovf + cnt) > 0)	oup = ovf * 100.0 / (ovf + cnt);
		if (Xunits) {
			float tbin = (WDcfg.THmax - WDcfg.THmin) / WDcfg.THnbin;
			sprintf(title, "TAC Brd-%d Ch-%d: Cnt=%d Ovf=%.1f%% - M=%.3f ns, S=%.2f ps", BrdToPlot, ChToPlot, WDhistos.TH[BrdToPlot][ChToPlot].H_cnt, oup, WDcfg.THmin + tbin*mean, tbin*rms * 1000);
			sprintf(xlabel, "ns");
			PlotHisto(WDhistos.TH[BrdToPlot][ChToPlot].H_data, WDcfg.THnbin, WDcfg.THmin, WDcfg.THmax, title, xlabel);
		}
		else {
			sprintf(title, "TAC Brd-%d Ch-%d: Cnt=%d Ovf=%.1f%% - M=%.3f, S=%.2f", BrdToPlot, ChToPlot, WDhistos.TH[BrdToPlot][ChToPlot].H_cnt, oup, mean, rms);
			sprintf(xlabel, "Channels");
			PlotHisto(WDhistos.TH[BrdToPlot][ChToPlot].H_data, WDcfg.THnbin, 0, (float)WDcfg.THnbin, title, xlabel);
		}

	}
	return 0;
}

void ClearHistoPlot() {
	if (hplot != NULL) {
		fprintf(hplot, "clear\n");
		fflush(hplot);
	}
}