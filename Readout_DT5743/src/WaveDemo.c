/******************************************************************************
*
* Copyright (C) 2017-2018 CAEN SpA - www.caen.it - support.computing@caen.it
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
*
*  Description:
*  -----------------------------------------------------------------------------
*  This is a demo program that can be used with the digitizer x743 family.
*  The purpose of this program is to configure the digitizer,
*  start the acquisition, read the data and write them into output files
*  and/or plot the waveforms using 'gnuplot' as an external plotting tool.
*  This program uses the CAENDigitizer library which is then based on the
*  CAENComm library for the access to the devices through any type of physical
*  channel (VME, Optical Link, USB, etc...).
*
*  -----------------------------------------------------------------------------
*  Syntax: WaveDemo [ConfigFile]
*  Default config file is "WaveDemoConfig.ini"
******************************************************************************/

#define WaveDemo_Release        "1.2.2_BM"
#define WaveDemo_Release_Date   "20251011"

#include "WaveDemo.h"

#include "WDBuffers.h"
#include "WDFiles.h"
#include "WDHisto.h"
#include "WDLogs.h"
#include "WDStats.h"
#include "WDWaveformProcess.h"
#include "WDconfig.h"
#include "WDplot.h"

#include "keyb.h"

/* Error messages */
static char ErrMsg[ERR_DUMMY_LAST][100] = {
	"No Error",                                         /* ERR_NONE */
	"Configuration File not found",                     /* ERR_CONF_FILE_NOT_FOUND */
	"Configuration Error",                              /* ERR_CONF */
	"Can't open the digitizer",                         /* ERR_DGZ_OPEN */
	"Can't read the Board Info",                        /* ERR_BOARD_INFO_READ */
	"Can't run WaveDump for this digitizer",            /* ERR_INVALID_BOARD_TYPE */
	"Can't program the digitizer",                      /* ERR_DGZ_PROGRAM */
	"Can't allocate the memory",						/* ERR_MALLOC */
	"Can't allocate the memory for the readout buffer", /* ERR_BUFF_MALLOC */
	"Can't allocate the memory for the histograms",     /* ERR_HISTO_MALLOC */
	"Restarting Error",                                 /* ERR_RESTART */
	"Interrupt Error",                                  /* ERR_INTERRUPT */
	"Readout Error",                                    /* ERR_READOUT */
	"Event Build Error",                                /* ERR_EVENT_BUILD */
	"Unmanaged board type",                             /* ERR_UNHANDLED_BOARD */
	"Output file write error",                          /* ERR_OUTFILE_WRITE */
	"Buffers error",									/* ERR_BUFFERS */
	"Internal Communication Timeout"					/* ERR_BOARD_TIMEOUT */
	"To Be Defined",									/* ERR_TBD */
};


/* ###########################################################################
*  Global Variables
*  ########################################################################### */
WaveDemoConfig_t	WDcfg;		// acquisition parameters and user settings
WaveDemoRun_t		WDrun;		// variables for the operation of the program
WaveDemoStats_t		WDstats;	// variables for the statistics
WaveDemoBuffers_t	WDbuff;		// events buffers
WaveDemoHistos_t	WDhistos;	// histograms

FILE *MsgLog = NULL;
WDPlot_t *WDPlotVar = NULL;

float PrevChTimeStamp[MAX_BD][MAX_CH];

int WPprogress = 0;

#define USE_EVT_BUFFERING	1

/* ###########################################################################
*  Functions
*  ########################################################################### */
/*! \fn      static long get_time()
*   \brief   Get time in milliseconds
*
*   \return  time in msec
*/
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

static int hexToInt(char ch) {
	return ((ch | 432) * 239217992 & 0xffffffff) >> 28;
}

/*!
 * \fn	ERROR_CODES_t OpenDigitizers(WaveDemoConfig_t *WDcfg)
 *
 * \brief	opens digitizers connection in the configuration.
 *
 * \param [in,out]	WDcfg	pointer to the config. struct.
 *
 * \return	error code.
 */

ERROR_CODES_t OpenDigitizers(WaveDemoConfig_t *WDcfg) {
	ERROR_CODES_t ErrCode = ERR_NONE;
	WaveDemoBoard_t *WDb;
	WaveDemoBoardHandle_t *WDh;
	unsigned int c32;
	int i;

	for (i = 0; i < WDcfg->NumBoards; i++) {
		WDb = &WDcfg->boards[i];
		WDh = &WDcfg->handles[i];
		printf("Initialization board %d...\n", i);

		if (WDb->BaseAddress == 0 && WDb->LinkType == 0) //wait a bit before open desktop digitizers
			SLEEP(1500);

		if (WDb->BaseAddress != 0 && WDb->LinkType == 0)
			printf("Loading SAM Correction Data from board. Please wait a few seconds...\n");

		WDh->ret_open = CAEN_DGTZ_OpenDigitizer2(WDb->LinkType, (WDb->LinkType == CAEN_DGTZ_ETH_V4718) ? WDb->ipAddress : (void*)&(WDb->LinkNum), WDb->ConetNode, WDb->BaseAddress, &WDh->handle);

		if (WDh->ret_open != CAEN_DGTZ_Success) {
			ErrCode = ERR_DGZ_OPEN;
			break;
		}
		WDh->ret_last = CAEN_DGTZ_GetInfo(WDh->handle, &WDh->BoardInfo);
		if (WDh->ret_last != CAEN_DGTZ_Success) {
			ErrCode = ERR_BOARD_INFO_READ;
			break;
		}

		if (WDh->BoardInfo.FamilyCode != CAEN_DGTZ_XX743_FAMILY_CODE) {
			ErrCode = ERR_UNHANDLED_BOARD;
			break;
		}
		else {
			WDh->Nbit = 12;
			if (WDh->BoardInfo.FormFactor == CAEN_DGTZ_VME64_FORM_FACTOR || WDh->BoardInfo.FormFactor == CAEN_DGTZ_VME64X_FORM_FACTOR) {
				WDh->Ngroup = 8;
				WDh->Nch = 16;
			}
			else if (WDh->BoardInfo.FormFactor == CAEN_DGTZ_DESKTOP_FORM_FACTOR || WDh->BoardInfo.FormFactor == CAEN_DGTZ_NIM_FORM_FACTOR) {
				WDh->Ngroup = 4;
				WDh->Nch = 8;
			}
		}

		//Important for old versions of PCB!!!!
		if (WDh->BoardInfo.PCB_Revision <= 3 && WDh->BoardInfo.FormFactor == CAEN_DGTZ_DESKTOP_FORM_FACTOR) {
			CAEN_DGTZ_ReadRegister(WDh->handle, 0x8168 /* fan speed */, &c32 /* max speed */);
			c32 |= 0x08;
			CAEN_DGTZ_WriteRegister(WDh->handle, 0x8168 /* fan speed */, c32 /* max speed */);
			msg_printf(MsgLog, "VERBOSE: Change fan speed (PCB_Revision: %u)\n", WDh->BoardInfo.PCB_Revision);
		}
	}
	return ErrCode;
}

/*!
 * \fn	void PrintDigitizersInfo(const WaveDemoConfig_t *WDcfg)
 *
 * \brief	Print digitizers information.
 *
 * \param	WDcfg	If non-null, the dcfg.
 */

void PrintDigitizersInfo(FILE *fLog, const WaveDemoConfig_t *WDcfg) {
	const WaveDemoBoardHandle_t *WDh;
	msg_printf(fLog, "------------------------------------------------------------------------------\n");
	for (int i = 0; i < WDcfg->NumBoards; i++) {
		WDh = &WDcfg->handles[i];
		msg_printf(fLog, "# %d - ", i);
		if (WDh->ret_open == CAEN_DGTZ_Success) {
			msg_printf(fLog, "Model: %s (S/N %u) - ", WDh->BoardInfo.ModelName, WDh->BoardInfo.SerialNumber);
			msg_printf(fLog, "Rel.: ROC %s, AMC %s\n", WDh->BoardInfo.ROC_FirmwareRel, WDh->BoardInfo.AMC_FirmwareRel);
		}
		msg_printf(fLog, "------------------------------------------------------------------------------\n");
	}
}

void StartAcquisition(WaveDemoConfig_t *WDcfg) {
	time_t timer;
	struct tm* tm_info;
	int NbOfChannels;
	//int channelsMask;
	WaveDemoBoardHandle_t *WDh;
	WaveDemoBoard_t *WDb;
	if (!WDcfg->SyncEnable) {
		// no syncronization enabled
		for (int i = 0; i < WDcfg->NumBoards; i++) {
			WDh = &WDcfg->handles[i];
			NbOfChannels = WDh->Nch;
			WDb = &WDcfg->boards[i];
			CAEN_DGTZ_SWStartAcquisition(WDh->handle);
		}
	}
	else {
		int handle_master = WDcfg->handles[0].handle;

		for (int i = 1; i < WDcfg->NumBoards; i++) {
			WDh = &WDcfg->handles[i];
			NbOfChannels = WDh->Nch;
			WDb = &WDcfg->boards[i];
			//start slave
			CAEN_DGTZ_SWStartAcquisition(WDh->handle);
		}

		// sw start master
		CAEN_DGTZ_SWStartAcquisition(handle_master);
	}
	// string with start time and date
	time(&timer);
	tm_info = localtime(&timer);
	strftime(WDstats.AcqStartTimeString, 32, "%Y-%m-%d %H:%M:%S", tm_info);
	strftime(WDrun.DataTimeFilename, 32, "%Y-%m-%d_%H-%M-%S", tm_info);
}

void StopAcquisition(WaveDemoConfig_t *WDcfg) {
	time_t timer;
	struct tm* tm_info;
	WaveDemoBoardHandle_t *WDh;
	for (int i = 0; i < WDcfg->NumBoards; i++) {
		WDh = &WDcfg->handles[i];
		CAEN_DGTZ_SWStopAcquisition(WDh->handle);
		CAEN_DGTZ_ClearData(WDh->handle);
	}
	// string with stop time and date
	time(&timer);
	tm_info = localtime(&timer);
	strftime(WDstats.AcqStopTimeString, 32, "%Y-%m-%d %H:%M:%S", tm_info);
}

void SendSWtrigger(WaveDemoConfig_t *WDcfg) {
	WaveDemoBoardHandle_t *WDh;
	int i;
	for (i = 0; i < WDcfg->NumBoards; i++) {
		WDh = &WDcfg->handles[i];
		CAEN_DGTZ_SendSWtrigger(WDh->handle);
	}
}

void DownloadAll() {
	WaveDemoBoardHandle_t *WDh;
	for (int bd = 0; bd < WDcfg.NumBoards; bd++) {
		WDh = &WDcfg.handles[bd];
		do {
			CAEN_DGTZ_ReadData(WDh->handle, CAEN_DGTZ_SLAVE_TERMINATED_READOUT_MBLT, WDh->buffer, &WDh->BufferSize);
		} while (WDh->BufferSize > 0);
	}
}

int ReadData(WaveDemoConfig_t *WDcfg) {
	ERROR_CODES_t ErrCode = ERR_NONE;
	WaveDemoBoardHandle_t *WDh;
	for (int bd = 0; bd < WDcfg->NumBoards; bd++) {
		WDh = &WDcfg->handles[bd];
		WDh->ret_last = CAEN_DGTZ_ReadData(WDh->handle, CAEN_DGTZ_SLAVE_TERMINATED_READOUT_MBLT, WDh->buffer, &WDh->BufferSize);
		if (WDh->ret_last != CAEN_DGTZ_Success) {
			ErrCode = ERR_READOUT;
			break;
		}
		WDh->Nb += WDh->BufferSize;
		WDh->NumEvents = 0;
		if (WDh->BufferSize != 0) {
			WDh->ret_last = CAEN_DGTZ_GetNumEvents(WDh->handle, WDh->buffer, WDh->BufferSize, &WDh->NumEvents);
			if (WDh->ret_last != CAEN_DGTZ_Success) {
				ErrCode = ERR_READOUT;
				break;
			}
		}
		WDh->Ne += WDh->NumEvents;

		WDstats.BlockRead_cnt++;
		WDstats.RxByte_cnt += WDh->BufferSize;
	}
	return ErrCode;
}

/*!
 * \fn	void CalcolateThroughput(WaveDemoConfig_t *WDcfg, WaveDemoRun_t *WDrun, uint64_t ElapsedTime)
 *
 * \brief	Calcolate throughput and print to screen.
 *
 * \param [in,out]	WDcfg	   	If non-null, the dcfg.
 * \param [in,out]	WDrun	   	If non-null, the drun.
 * \param 		  	ElapsedTime	The elapsed time.
 */

void ComputeThroughput(WaveDemoConfig_t *WDcfg, WaveDemoRun_t *WDrun, uint64_t ElapsedTime) {
	WaveDemoBoardHandle_t *WDh;
	char boardstr[16] = "";
	int i;
	if (WDcfg->NumBoards > 1) {
		sprintf(boardstr, "[board %d] ", WDrun->BoardSelected);
	}

	for (i = 0; i < WDcfg->NumBoards; i++) {
		WDh = &WDcfg->handles[i];
		if (i == WDrun->BoardSelected) {
			if (WDh->Nb == 0)
				if (WDh->ret_last == CAEN_DGTZ_Timeout)
					printf("%sTimeout...\n", boardstr);
				else
					printf("%sNo data...\n", boardstr);
			else
				printf("%sReading at %.2f MB/s (Trg Rate: %.2f Hz)\n", boardstr, (float)WDh->Nb / ((float)ElapsedTime*1048.576f), (float)WDh->Ne*1000.0f / (float)ElapsedTime);
		}
		WDh->Nb = 0;
		WDh->Ne = 0;
	}
}

void memcpyEvent(CAEN_DGTZ_X743_EVENT_t *dst, const CAEN_DGTZ_X743_EVENT_t *src) {
	for (int i = 0; i < MAX_V1743_GROUP_SIZE; i++) {
		dst->GrPresent[i] = src->GrPresent[i];
		if (src->GrPresent[i] == 0)
			continue;
		dst->DataGroup[i].ChSize = src->DataGroup[i].ChSize;
		for (int j = 0; j < MAX_X743_CHANNELS_X_GROUP; j++) {
			uint32_t k;
			for (k = 0; k < src->DataGroup[i].ChSize; k++) {
				dst->DataGroup[i].DataChannel[j][k] = src->DataGroup[i].DataChannel[j][k];
			}
			dst->DataGroup[i].TriggerCount[j] = src->DataGroup[i].TriggerCount[j];
			dst->DataGroup[i].TimeCount[j] = src->DataGroup[i].TimeCount[j];
		}
		dst->DataGroup[i].EventId = src->DataGroup[i].EventId;
		dst->DataGroup[i].StartIndexCell = src->DataGroup[i].StartIndexCell;
		dst->DataGroup[i].TDC = src->DataGroup[i].TDC;
		dst->DataGroup[i].PosEdgeTimeStamp = src->DataGroup[i].PosEdgeTimeStamp;
		dst->DataGroup[i].NegEdgeTimeStamp = src->DataGroup[i].NegEdgeTimeStamp;
		dst->DataGroup[i].PeakIndex = src->DataGroup[i].PeakIndex;
		dst->DataGroup[i].Peak = src->DataGroup[i].Peak;
		dst->DataGroup[i].Baseline = src->DataGroup[i].Baseline;
		dst->DataGroup[i].Charge = src->DataGroup[i].Charge;
	}
}

int EventProcessing(int bd, int ch, WaveDemoEvent_t *event) {
	// Energy Spectra 
	int Ebin;
	Ebin = (int)((event->EventPlus[ch / 2][ch % 2].Energy) / (WDcfg.boards[bd].channels[ch].EnergyCoarseGain * 1024 / WDcfg.EHnbin));
	Histo1D_AddCount(&WDhistos.EH[bd][ch], Ebin);

	// Time Spectrum 
	float time;
	int BrdRef = WDcfg.TOFstartBoard;
	int ChRef = WDcfg.TOFstartChannel;
	uint32_t Tbin;
	uint64_t TDC = event->Event->DataGroup[ch / 2].TDC;
	float RealtiveFineTime = event->EventPlus[ch / 2][ch % 2].FineTimeStamp;
	uint64_t TDCRef = WDcfg.handles[BrdRef].RefEvent->Event->DataGroup[ChRef / 2].TDC;
	float RealtiveFineTimeRef = WDcfg.handles[BrdRef].RefEvent->EventPlus[ChRef / 2][ChRef % 2].FineTimeStamp;
	if (WDcfg.TspectrumMode == TAC_SPECTRUM_INTERVALS) {
		time = (TDC * 5 + RealtiveFineTime) - PrevChTimeStamp[bd][ch]; // delta T between pulses on the same channel (in ns)
		PrevChTimeStamp[bd][ch] = TDC * 5 + RealtiveFineTime;
	}
	else
		time = (TDC - TDCRef) * 5 + (RealtiveFineTime - RealtiveFineTimeRef);  // delta T from Ref Channel (in ns)

	Tbin = (uint32_t)((time - WDcfg.THmin) * WDcfg.THnbin / (WDcfg.THmax - WDcfg.THmin));
	Histo1D_AddCount(&WDhistos.TH[bd][ch], Tbin);

	// Event Saving into the enabled output files
	if (WDrun.ContinuousWrite || WDrun.SingleWrite) {
		if (WDcfg.SaveLists)
			SaveList(bd, ch, event);
		if (WDcfg.SaveWaveforms)
			SaveWaveform(bd, ch, event);
	}
	return 0;
}

void LoadPlotOptionsCommon() {
	if (WDrun.SetPlotOptions) {
		// title
		strcpy(WDPlotVar->Title, "Waveforms");
		// data type
		WDPlotVar->DataType = PLOT_DATA_FLOAT;

		// x scale
		sprintf(WDPlotVar->Xlabel, "Time [%s]", "ns");
		WDPlotVar->Xscale = WDcfg.handles[0].Ts;
		WDPlotVar->Xautoscale = 1;

		// y scale
		sprintf(WDPlotVar->Ylabel, "Amplitude [%s]", "Volt");
		WDPlotVar->Yautoscale = 0;
		WDPlotVar->Ymin = -2.5;
		WDPlotVar->Ymax = +2.5;

		WDPlotVar->vertical_line = (WDcfg.GlobalRecordLength * WDcfg.TriggerFix / 100) * WDcfg.handles[0].Ts;
	}
}

const bool TraceEnableDefault[MAX_NTRACES] = { true, true, false, false, true, true, false, false };
static char TraceNames[MAX_NTRACES][48] = {
	"Input",
	"Discriminator",
	"Smoothing",
	"TriggerThreshold",
	"Trigger",
	"Gate",
	"BaselineCalc",
	"Baseline",
};

void fillTraces(WaveDemo_EVENT_plus_t* eventPlus) {
	Waveform_t* Wfm = eventPlus->Waveforms;
	int dtg = (1 << 12) / 20;
	int dto = 50;

	for (int i = 0; i < Wfm->Ns; i++) {
		int t;
		for (t = 0; t < NUM_ATRACE; t++)
			if (WDrun.TraceEnable[t]) WDrun.Traces[t][i] = Wfm->AnalogTrace[t][i];  // analog trace
		t = NUM_ATRACE;
		if (WDrun.TraceEnable[t + 0]) WDrun.Traces[t + 0][i] = (float) ((Wfm->DigitalTraces[i] & DTRACE_TRIGGER)  >> 0) * dtg + dto;			 // digital trace 0
		if (WDrun.TraceEnable[t + 1]) WDrun.Traces[t + 1][i] = (float) ((Wfm->DigitalTraces[i] & DTRACE_ENERGY)   >> 1) * dtg + dto + 2 * dtg;   // digital trace 1
		if (WDrun.TraceEnable[t + 2]) WDrun.Traces[t + 2][i] = (float) ((Wfm->DigitalTraces[i] & DTRACE_BASELINE) >> 2) * dtg + dto + 4 * dtg;   // digital trace 2
		if (WDrun.TraceEnable[t + 3]) WDrun.Traces[t + 3][i] = eventPlus->Baseline;
	}
}

int PlotMultiWaveforms(WaveDemoEvent_t* events[], int array_size, int bdplot, int chplot) {
	int Tn = 0;

	if (WDPlotVar == NULL)
		return -1;

	if (WDrun.SetPlotOptions) {
		LoadPlotOptionsCommon();
		if (bdplot != -1 && chplot != -1)
			sprintf(WDPlotVar->Title, "Waveforms board %d channel %d", bdplot, chplot);
		SetPlotOptions();
		WDrun.SetPlotOptions = 0;
	}

	uint64_t TDCRef;
	if (events[0] != NULL) {
		TDCRef = events[0]->Event->DataGroup[WDcfg.boards[0].RefChannel / 2].TDC;
	}
	else {
		TDCRef = 0;
	}

	//count traces enabled
	int tracesEnabled = 0;
	for (int t = 0; t < MAX_NTRACES; t++) {
		if (WDrun.TraceEnable[t])
			tracesEnabled++;
	}
	if (tracesEnabled == 0) {
		ClearPlot();
		return 0;
	}

	for (int i = 0; i < array_size; i++) {
		if (events[i] == NULL)
			continue;
		if (Tn >= MAX_NUM_TRACES)
			break;
		int bd = (bdplot == -1) ? i : bdplot;
		WaveDemoBoard_t* WDb = &WDcfg.boards[bd];
		WaveDemoBoardHandle_t* WDh = &WDcfg.handles[bd];
		WaveDemoBoardRun_t* WDr = &WDcfg.runs[bd];
		for (int ch = 0; ch < WDh->Nch; ch++) {
			if ((Tn + tracesEnabled) >= MAX_NUM_TRACES)
				break;
			if (chplot != -1 && chplot != ch)
				continue;
			WaveDemoChannel_t* WDc = &WDb->channels[ch];

			if (!WDr->ChannelPlotEnable[ch] || !WDc->ChannelEnable)
				continue;

			uint64_t TDC = events[bd]->Event->DataGroup[ch / 2].TDC;
			WaveDemo_EVENT_plus_t* eventPlus = &events[bd]->EventPlus[ch / 2][ch % 2];
			int TraceLength = events[bd]->Event->DataGroup[ch / 2].ChSize;
			fillTraces(eventPlus);
			for (int t = 0; t < MAX_NTRACES; t++) {
				if (WDrun.TraceEnable[t]) {
					sprintf(WDPlotVar->TraceName[Tn], "%s B %d CH %d", TraceNames[t], bd, ch);
					WDPlotVar->TraceSize[Tn] = TraceLength;

					//memcpy(WDPlotVar->TraceData[Tn], WDrun.Traces[t], TraceLength * sizeof(float));

					float* pTraceData = WDPlotVar->TraceData[Tn];
					for (int s = 0; s < TraceLength; s++) {
						pTraceData[s] = WDrun.Traces[t][s] * (1.25f / 2048.0f);
					}

					WDPlotVar->TraceXOffset[Tn] = (int) (TDC - TDCRef) * 5;
					Tn++;
				}
			}
		}
	}
	WDPlotVar->NumTraces = Tn;
	if (PlotWaveforms() < 0) {
		WDrun.ContinuousPlot = 0;
		printf("Plot Error\n");
		return -1;
	}
	else {
		addProgressIndicator(&WPprogress);
	}
	return Tn;
}

void PlotSingleEventOfBoard(int bd, WaveDemoEvent_t* event) {
	WaveDemoBoard_t* WDb = &WDcfg.boards[bd];
	WaveDemoBoardHandle_t* WDh = &WDcfg.handles[bd];
	WaveDemoBoardRun_t* WDr = &WDcfg.runs[bd];

	if (WDPlotVar != NULL) {
		int Tn = 0;
		if (WDrun.SetPlotOptions) {
			LoadPlotOptionsCommon();
			sprintf(WDPlotVar->Title, "Waveforms of board %d (only output data)", bd);
			WDPlotVar->vertical_line = 0;
			SetPlotOptions();
			WDrun.SetPlotOptions = 0;
		}
		for (int ch = 0; ch < WDh->Nch; ch++) {
			WaveDemoChannel_t* WDc = &WDb->channels[ch];
			int groupIndex = ch / 2;
			int channelIndex = ch % 2;

			if (!WDr->ChannelPlotEnable[ch] || !WDc->ChannelEnable)
				continue;

			sprintf(WDPlotVar->TraceName[Tn], "B %d CH %d", bd, ch);
			if (event->Event->GrPresent[groupIndex]) {
				int size = WDPlotVar->TraceSize[Tn] = event->Event->DataGroup[groupIndex].ChSize;

				//memcpy(WDPlotVar->TraceData[Tn], event->Event->DataGroup[groupIndex].DataChannel[channelIndex], event->Event->DataGroup[groupIndex].ChSize * sizeof(float));

				float* pTraceData = WDPlotVar->TraceData[Tn];
				for (int s = 0; s < size; s++) {
					pTraceData[s] = event->Event->DataGroup[groupIndex].DataChannel[channelIndex][s] * (1.25f / 2048.0f);
				}

				WDPlotVar->TraceXOffset[Tn] = 0;
			}
			Tn++;
			if (Tn >= MAX_NUM_TRACES)
				break;
		}
		WDPlotVar->NumTraces = Tn;
		if (PlotWaveforms() < 0) {
			WDrun.ContinuousPlot = 0;
			printf("Plot Error\n");
		}
		else {
			addProgressIndicator(&WPprogress);
		}
	}
}

int ProcessesSynchronizedEvents() {
	WaveDemoEvent_t *events[MAX_BD] = { NULL };
	uint64_t TDC_min = 0;
	int groupIndex;

	// look for the buffer with the least number of events 
	int min_buff_len = WDBuff_used_space(&WDbuff, 0);
	for (int bd = 1; bd < WDcfg.NumBoards; bd++) {
		if (WDBuff_used_space(&WDbuff, bd) < min_buff_len)
			min_buff_len = WDBuff_used_space(&WDbuff, bd);
	}
	// can not check synchronization if at least one buffer is empty
	if (min_buff_len == 0)
		return 0;

	for (int i = 0; i < min_buff_len; i++) {
		int event_good[MAX_BD] = { 0 };
		int count_sync_evt = 0;
		// get the oldest event for each board
		for (int bd = 0; bd < WDcfg.NumBoards; bd++) {
			if (WDBuff_empty(&WDbuff, bd))
				return 0;
			WDBuff_peak(&WDbuff, bd, &(events[bd]));
			event_good[bd] = 0;
		}

		groupIndex = WDcfg.boards[0].RefChannel / 2;
		// calculates the minimum timestamp
		if (events[0]) {
			TDC_min = events[0]->Event->DataGroup[groupIndex].TDC;
		}
		for (int bd = 1; bd < WDcfg.NumBoards; bd++) {
			groupIndex = WDcfg.boards[bd].RefChannel / 2;
			if (events[bd]->Event->DataGroup[groupIndex].TDC < TDC_min)
				TDC_min = events[bd]->Event->DataGroup[groupIndex].TDC;
		}
		// search for the event between the boards with the minumun timestamp
		for (int bd = 0; bd < WDcfg.NumBoards; bd++) {
			groupIndex = WDcfg.boards[bd].RefChannel / 2;
			uint64_t TDCdiff_ns = SYNC_WIN + 1; //init to invalid value
			if (events[bd]) {
				TDCdiff_ns = (events[bd]->Event->DataGroup[groupIndex].TDC - TDC_min) * 5; //this difference is positive
			}
			if (TDCdiff_ns <= SYNC_WIN) {
				// marks and counts the synchronized events
				event_good[bd] = 1;
				count_sync_evt++;
			}
			if (bd == WDcfg.TOFstartBoard)
				WDcfg.handles[bd].RefEvent = events[bd];
			else
				WDcfg.handles[bd].RefEvent = NULL;
		}

		if (count_sync_evt == WDcfg.NumBoards) {
			// Process waveform. (Set timestamp, fine time, energy fields in EventPlus data structure)
			MultiWaveformProcess(events, WDcfg.NumBoards);
			// Waveform Plotting
			if ((WDrun.ContinuousPlot || WDrun.SinglePlot) && WDrun.WavePlotMode == WPLOT_MODE_STD && !IsPlotterBusy()) {
				PlotMultiWaveforms(events, WDcfg.NumBoards, -1, -1);
				WDrun.SinglePlot = 0;
			}
		}
		else {
			WDstats.UnSyncEv_cnt += (uint64_t)WDcfg.NumBoards - count_sync_evt;
			if (WDstats.UnSyncEv_cnt == 0) {
				msg_printf(MsgLog, "WARN: events unsynchronized found!\n");
				SLEEP(120);
			}
		}

		for (int bd = 0; bd < WDcfg.NumBoards; bd++) {
			if (event_good[bd]) {
				if (count_sync_evt == WDcfg.NumBoards) {
					// update statistics
					for (int ch = 0; ch < WDcfg.handles[bd].Nch; ch++) {
						if (WDcfg.boards[bd].channels[ch].ChannelEnable) {
							bool toProcess = true;

							// eliminates event with has no fine timestamp
							if (events[bd]->EventPlus[ch / 2][ch % 2].FineTimeStamp == 0)
								toProcess = false;

							if (toProcess) {
								EventProcessing(bd, ch, events[bd]);
								WDstats.EvFilt_cnt[bd][ch]++;
							}
							// Waveform Plotting
							bool toPlot = (WDrun.BrdToPlot == bd && WDrun.ChToPlot == ch);
							if ((WDrun.ContinuousPlot || WDrun.SinglePlot) && toPlot && WDrun.WavePlotMode == WPLOT_MODE_1CH && !IsPlotterBusy()) {
								PlotMultiWaveforms(&events[bd], 1, bd, ch);
								WDrun.SinglePlot = 0;
							}
						}
					}
				}
				else {
					// update statistics
					for (int ch = 0; ch < WDcfg.handles[bd].Nch; ch++) {
						if (WDcfg.boards[bd].channels[ch].ChannelEnable)
							WDstats.EvLost_cnt[bd][ch]++;
					}
				}
				// remove event from buffer
				WDBuff_remove(&WDbuff, bd, 1);
				// update statistics
				for (int ch = 0; ch < WDcfg.handles[bd].Nch; ch++) {
					if (WDcfg.boards[bd].channels[ch].ChannelEnable)
						WDstats.EvProcessed_cnt[bd][ch]++;
				}
			}
		}
	}
	return 1;
}

int ProcessesUnsynchronizedEvents() {
	WaveDemoEvent_t* events[MAX_BD] = { NULL };

	// gets number of events that has each board
	unsigned int num_events[MAX_BD] = { 0 };
	int max_num_events = 0;
	for (int bd = 0; bd < WDcfg.NumBoards; bd++) {
		num_events[bd] = WDcfg.handles[bd].NumEvents;
		if ((int) num_events[bd] > max_num_events) {
			max_num_events = (int) num_events[bd];
		}
	}

	for (int i = 0; i < max_num_events; i++) {
		for (int bd = 0; bd < WDcfg.NumBoards; bd++) {
			events[bd] = NULL;

			if (WDBuff_empty(&WDbuff, bd))
				continue;
			// get event from buffer
			WaveDemoEvent_t* event;
			WDBuff_peak(&WDbuff, bd, &event);

			if (bd == WDcfg.TOFstartBoard)
				WDcfg.handles[bd].RefEvent = event;
			else
				WDcfg.handles[bd].RefEvent = NULL;

			for (int ch = 0; ch < WDcfg.handles[bd].Nch; ch++) {
				if (WDcfg.boards[bd].channels[ch].ChannelEnable) {
					bool toProcess = true;
					// Process waveform. (Set timestamp, fine time, energy fields in EventPlus data structure)
					WaveformProcess(bd, ch, event);

					// eliminates event with has no fine timestamp
					if (event->EventPlus[ch / 2][ch % 2].FineTimeStamp == 0)
						toProcess = false;

					if (toProcess) {
						EventProcessing(bd, ch, event);
						// update statistics
						WDstats.EvFilt_cnt[bd][ch]++;
					}
					// Waveform Plotting
					bool toPlot = (WDrun.BrdToPlot == bd && WDrun.ChToPlot == ch);
					if ((WDrun.ContinuousPlot || WDrun.SinglePlot) && toPlot && WDrun.WavePlotMode == WPLOT_MODE_1CH && !IsPlotterBusy()) {
						PlotMultiWaveforms(&event, 1, bd, ch);
						WDrun.SinglePlot = 0;
					}
				}
			}
			// remove event from buffer
			WDBuff_remove(&WDbuff, bd, 1);

			// update statistics
			for (int ch = 0; ch < WDcfg.handles[bd].Nch; ch++) {
				if (WDcfg.boards[bd].channels[ch].ChannelEnable)
					WDstats.EvProcessed_cnt[bd][ch]++;
			}

			events[bd] = event;
		}
		// Waveform Plotting
		if ((WDrun.ContinuousPlot || WDrun.SinglePlot) && WDrun.WavePlotMode == WPLOT_MODE_STD && !IsPlotterBusy()) {
			PlotMultiWaveforms(events, WDcfg.NumBoards, -1, -1);
			WDrun.SinglePlot = 0;
		}
	}
	return 1;
}

int MakeSpaceBuffers() {
	int num_new_evt;
	int evt_removed;
	for (int bd = 0; bd < WDcfg.NumBoards; bd++) {
		num_new_evt = WDcfg.handles[bd].NumEvents;
		// check if buffer have space for adding the new events
		if (WDBuff_free_space(&WDbuff, bd) < num_new_evt) {
			// remove old events to make space
			evt_removed = WDBuff_remove(&WDbuff, bd, num_new_evt);
			if (evt_removed < 0) {
				return -1;
			}
			// update statistics
			for (int ch = 0; ch < WDcfg.handles[bd].Nch; ch++) {
				WDstats.EvLost_cnt[bd][ch] += evt_removed;
				WDstats.EvProcessed_cnt[bd][ch] += evt_removed;
			}
		}
	}
	return 1;
}

void UpdateTimes4Stats(int bd, int ch, WaveDemoEvent_t *event) {
	int groupIndex = ch / 2;
	int channelIndex = ch % 2;

	uint64_t time = event->Event->DataGroup[groupIndex].TDC * 5;

	// Get the newest time stamp (used to calculate the real acquisition time)
	if (time > WDstats.LatestProcTstampAll)
		WDstats.LatestProcTstampAll = time;
	WDstats.AcqStopTime = (float)(WDstats.LatestProcTstampAll / 1e6);
	WDstats.LatestProcTstamp[bd][ch] = time;
}

int EventsDecoding(WaveDemoConfig_t *WDcfg) {
	ERROR_CODES_t ErrCode = ERR_NONE;
	WaveDemoBoard_t *WDb;
	WaveDemoBoardHandle_t *WDh;
	WaveDemoBoardRun_t *WDr;
	int ret;
	unsigned int eventIndex;
	WaveDemoEvent_t *event = NULL;
	char *EventPtr;

	//int BrdRef = WDcfg->TOFstartBoard;
	//int ChRef = WDcfg->TOFstartChannel;

	// Free space to add the new events 
	if (USE_EVT_BUFFERING)
		MakeSpaceBuffers();

	for (int bd = 0; bd < WDcfg->NumBoards; bd++) {
		WDb = &WDcfg->boards[bd];
		WDh = &WDcfg->handles[bd];
		WDr = &WDcfg->runs[bd];

		if (WDh->NumEvents == 0)
			continue;

		for (eventIndex = 0; eventIndex < WDh->NumEvents; eventIndex++) {

			/* get pointer to write event in the buffer */
			ret = WDBuff_get_write_pointer(&WDbuff, bd, &event);
			if (ret < 0 || event == NULL) {
				ErrCode = ERR_BUFFERS;
				return ErrCode;
			}

			/* Get one event from the readout buffer */
			ret = CAEN_DGTZ_GetEventInfo(WDh->handle, WDh->buffer, WDh->BufferSize, eventIndex, &event->EventInfo, &EventPtr);
			if (event->EventInfo.ChannelMask == 0) {
				continue;
			}
			if (ret != CAEN_DGTZ_Success) {
				ErrCode = ERR_EVENT_BUILD;
				return ErrCode;
			}

			/* Decode the event */
			ret = CAEN_DGTZ_DecodeEvent(WDh->handle, EventPtr, (void**)&event->Event);
			if (ret != CAEN_DGTZ_Success) {
				ErrCode = ERR_EVENT_BUILD;
				return ErrCode;
			}
#if USE_EVT_BUFFERING
			/* register event added in the buffer */
			ret = WDBuff_added(&WDbuff, bd, 1);
			if (ret != 1) {
				ErrCode = ERR_BUFFERS;
				return ErrCode;
			}
#endif // USE_EVT_BUFFERING

			char channelsEnabled[MAX_CH] = { 0 };

			/* runs operations on channels */
			for (int ch = 0; ch < WDh->Nch; ch++) {
				int groupIndex = ch / 2;
				int channelIndex = ch % 2;

				channelsEnabled[ch] = WDb->channels[ch].ChannelEnable;

				if (!event->Event->GrPresent[groupIndex] || !WDb->channels[ch].ChannelEnable)
					continue;

				// Updates times for statistic
				UpdateTimes4Stats(bd, ch, event);

				// Save raw data
				if (WDrun.ContinuousWrite || (WDrun.SingleWrite && eventIndex == 0)) {
					if (WDcfg->SaveTDCList)
						SaveTDCList(bd, ch, event);
				}
			}

			/* save raw data */
			if (WDrun.ContinuousWrite || (WDrun.SingleWrite && eventIndex == 0))
				if (WDcfg->SaveRawData)
					SaveRawData(bd, channelsEnabled, event);

			/* Plot Waveforms */
			if ((WDrun.ContinuousPlot || WDrun.SinglePlot) && WDrun.BrdToPlot == bd && WDrun.WavePlotMode == WPLOT_MODE_1BD && !IsPlotterBusy()) {
				PlotSingleEventOfBoard(bd, event);
				WDrun.SinglePlot = 0;
			}
		}

		WDstats.TotEvRead_cnt += WDh->NumEvents;
		for (int ch = 0; ch < WDh->Nch; ch++) {
			int groupIndex = ch / 2;
			int channelIndex = ch % 2;

			if (!event->Event->GrPresent[groupIndex] || !WDb->channels[ch].ChannelEnable)
				continue;

			uint32_t Size = event->Event->DataGroup[groupIndex].ChSize;
			if (Size <= 0) {
				continue;
			}
			WDstats.EvRead_cnt[bd][ch] += WDh->NumEvents;
			WDstats.LatestReadTstamp[bd][ch] = event->Event->DataGroup[groupIndex].TDC * 5;
		}
	}
	return ErrCode;
}

ERROR_CODES_t AllocateReadoutBuffer(WaveDemoConfig_t *WDcfg) {
	ERROR_CODES_t ErrCode = ERR_NONE;
	WaveDemoBoardHandle_t *WDh;
	int i;
	for (i = 0; i < WDcfg->NumBoards; i++) {
		WDh = &WDcfg->handles[i];
		WDh->ret_last = CAEN_DGTZ_MallocReadoutBuffer(WDh->handle, &WDh->buffer, &WDh->AllocatedSize); /* WARNING: This malloc must be done after the digitizer programming */
		if (WDh->ret_last != CAEN_DGTZ_Success) {
			ErrCode = ERR_BUFF_MALLOC;
			break;
		}
	}
	return ErrCode;
}
void FreeReadoutBuffer(WaveDemoConfig_t *WDcfg) {
	WaveDemoBoardHandle_t *WDh;
	int i;
	for (i = 0; i < WDcfg->NumBoards; i++) {
		WDh = &WDcfg->handles[i];
		if (WDh->buffer)
			CAEN_DGTZ_FreeReadoutBuffer(&WDh->buffer);
	}
}

int _AllocateWaveform(Waveform_t **Waveform, int ns) {
	int allocsize = 0;
	Waveform_t *wfm;
	
	wfm = malloc(sizeof(Waveform_t));
	if (wfm == NULL) {
		*Waveform = NULL;
		return -1;
	}
	allocsize += sizeof(Waveform_t);

	//memory initializing
	memset(wfm, 0, sizeof(Waveform_t));

	wfm->Ns = ns;
	for (int i = 0; i < NUM_ATRACE; i++) {
		wfm->AnalogTrace[i] = malloc(ns * sizeof(float));
		if (wfm->AnalogTrace[i] == NULL)
			return -1;
	}
	wfm->DigitalTraces = malloc(ns * sizeof(uint8_t));
	if (wfm->DigitalTraces == NULL)
		return -1;

	*Waveform = wfm;

	return allocsize;
}
void _FreeWaveform(Waveform_t* Waveform) {
	for (int i = 0; i < NUM_ATRACE; i++) {
		free(Waveform->AnalogTrace[i]);
		Waveform->AnalogTrace[i] = NULL;
	}

	free(Waveform->DigitalTraces);
	Waveform->DigitalTraces = NULL;

	free(Waveform);
	Waveform = NULL;
}

ERROR_CODES_t AllocateEventBuffer(WaveDemoBuffers_t *buff) {
	ERROR_CODES_t ErrCode = ERR_NONE;
	int size = EVT_BUF_SIZE * sizeof(WaveDemoEvent_t);
	for (int bd = 0; bd < WDcfg.NumBoards; bd++) {
		buff->buffer[bd] = (WaveDemoEvent_t *)malloc(size);
		if (buff->buffer[bd] == NULL) {
			ErrCode = ERR_MALLOC;
			break;
		}
		//memory initializing
		memset(buff->buffer[bd], 0, size);
		WDBuff_reset(buff, bd);
		for (int j = 0; j < EVT_BUF_SIZE; j++) {
			// allocate CAEN_DGTZ_X743_EVENT_t *Event
			if (CAEN_DGTZ_AllocateEvent(WDcfg.handles[bd].handle, (void**)&buff->buffer[bd][j].Event) != CAEN_DGTZ_Success)
				return ERR_MALLOC;

			// allocate Waveform_t *Waveforms
			for (int ch = 0; ch < MAX_CH; ch++) {
				if (!WDcfg.boards[bd].channels[ch].ChannelEnable)
					continue;
				WaveDemo_EVENT_plus_t *evt_plus = &buff->buffer[bd][j].EventPlus[ch / 2][ch % 2];
				_AllocateWaveform(&evt_plus->Waveforms, WDcfg.GlobalRecordLength);
				//Waveform_t *wfm = evt_plus->Waveforms;
				//wfm->AnalogTrace[0] = buff->buffer[bd][j].Event->DataGroup[ch / 2].DataChannel[ch % 2];
			}
		}
	}
	return ErrCode;
}

void FreeEventBuffer(WaveDemoBuffers_t *buff) {
	for (int bd = 0; bd < WDcfg.NumBoards; bd++) {
		if (buff->buffer[bd]) {
			for (int i = 0; i < EVT_BUF_SIZE; i++) {
				for (int ch = 0; ch < MAX_CH; ch++) {
					if (!WDcfg.boards[bd].channels[ch].ChannelEnable)
						continue;
					if (buff->buffer[bd][i].EventPlus[ch / 2][ch % 2].Waveforms)
						_FreeWaveform(buff->buffer[bd][i].EventPlus[ch / 2][ch % 2].Waveforms);
					buff->buffer[bd][i].EventPlus[ch / 2][ch % 2].Waveforms = NULL;
				}
				if (buff->buffer[bd][i].Event)
					CAEN_DGTZ_FreeEvent(WDcfg.handles[bd].handle, (void**)&buff->buffer[bd][i].Event);
				buff->buffer[bd][i].Event = NULL;
			}
			free(buff->buffer[bd]);
			buff->buffer[bd] = NULL;
		}
	}
}

ERROR_CODES_t ResetEventBuffer() {
	ERROR_CODES_t ErrCode = ERR_NONE;
	for (int bd = 0; bd < WDcfg.NumBoards; bd++) {
		WDBuff_reset(&WDbuff, bd);
	}
	return ErrCode;
}

ERROR_CODES_t AllocateTraces(WaveDemoConfig_t *WDcfg) {
	ERROR_CODES_t ErrCode = ERR_NONE;

	for (int t = 0; t < MAX_NTRACES; t++) {
		WDrun.Traces[t] = (float*)malloc(WDcfg->GlobalRecordLength * sizeof(float));
		if (WDrun.Traces[t] == NULL) {
			ErrCode = ERR_MALLOC;
			break;
		}
		//memory initializing
		memset(WDrun.Traces[t], 0, WDcfg->GlobalRecordLength * sizeof(float));
		WDrun.TraceEnable[t] = TraceEnableDefault[t];
	}

	return ErrCode;
}

void FreeTraces() {
	for (int t = 0; t < MAX_NTRACES; t++) {
		free(WDrun.Traces[t]);
		WDrun.Traces[t] = NULL;
		WDrun.TraceEnable[t] = false;
	}
}

/*!
 * \fn	void CloseDigitizers(WaveDemoConfig_t *WDcfg)
 *
 * \brief	Closes the digitizers.
 *
 * \param [in,out]	WDcfg	If non-null, the dcfg.
 */

void CloseDigitizers(WaveDemoConfig_t *WDcfg) {
	WaveDemoBoardHandle_t *WDh;
	int i;
	for (i = 0; i < WDcfg->NumBoards; i++) {
		WDh = &WDcfg->handles[i];
		CAEN_DGTZ_CloseDigitizer(WDh->handle);
	}
}

/*!
 * \fn	int WriteRegisterBitmask(int32_t handle, uint32_t address, uint32_t data, uint32_t mask)
 *
 * \brief	writes 'data' on register at 'address' using 'mask' as bitmask.
 *
 * \param	handle 	:   Digitizer handle.
 * \param	address	Address of the Register to write.
 * \param	data   	:   Data to Write on the Register.
 * \param	mask   	:   Bitmask to use for data masking.
 *
 * \return	0 = Success; negative numbers are error codes.
 */

int WriteRegisterBitmask(int32_t handle, uint32_t address, uint32_t data, uint32_t mask) {
	int32_t ret = CAEN_DGTZ_Success;
	uint32_t d32 = 0xFFFFFFFF;

	ret = CAEN_DGTZ_ReadRegister(handle, address, &d32);
	if (ret != CAEN_DGTZ_Success)
		return ret;

	data &= mask;
	d32 &= ~mask;
	d32 |= data;
	ret = CAEN_DGTZ_WriteRegister(handle, address, d32);
	return ret;
}

/*!
 * \fn	int ProgramBoard(WaveDemoBoard_t *WDb, WaveDemoBoardHandle_t *WDh, char doReset)
 *
 * \brief	configure the digitizer according to the parameters read from the cofiguration file
 * 			and saved in the WDcfg data structure.
 *
 * \param [in,out]	WDb	   	WaveDumpBoard data structure.
 * \param [in,out]	WDh	   	WaveDumpBoardRun data structure.
 * \param 		  	doReset	1 = call reset function.
 *
 * \return	0 = Success; negative numbers are error codes.
 */

int ProgramBoard(WaveDemoBoard_t *WDb, WaveDemoBoardHandle_t *WDh, char doReset) {
	int i, ret = 0;
	int samIndex, channel, channelsMask, groupsMask;
	int handle = WDh->handle;
	int NbOfSamBlocks = WDh->Ngroup;
	int NbOfChannels = WDh->Nch;

	/* reset the digitizer */
	if (doReset) {
		ret |= CAEN_DGTZ_Reset(handle);
		if (ret != 0) {
			msg_printf(MsgLog, "Error: Unable to reset digitizer.\n");
			printf("Please reset digitizer manually then restart the program\n");
			return -1;
		}
	}

	//Board Fail Status
	uint32_t d32 = 0;
	ret |= CAEN_DGTZ_ReadRegister(handle, 0x8178, &d32);
	if ((d32 & 0xF) != 0) {
		msg_printf(MsgLog, "Error: Internal Communication Timeout occurred.\n");
		printf("Please reset digitizer manually then restart the program\n");
		return -1;
	}

	/* Set Group Enable Mask*/
	groupsMask = 0;
	for (channel = 0; channel < NbOfChannels; channel++) {
		if (WDb->channels[channel].ChannelEnable)
			groupsMask |= (1 << (channel / 2));
	}
	ret |= CAEN_DGTZ_SetGroupEnableMask(handle, groupsMask);

	/* Set Post Trigger Delay */
	for (samIndex = 0; samIndex < NbOfSamBlocks; samIndex++) {
		ret |= CAEN_DGTZ_SetSAMPostTriggerSize(handle, samIndex, WDb->groups[samIndex].TriggerDelay & 0xFF);
	}

	/* Set Sampling Frequency */
	ret |= CAEN_DGTZ_SetSAMSamplingFrequency(handle, WDb->SamplingFrequency);
	switch (WDb->SamplingFrequency) {
	case CAEN_DGTZ_SAM_3_2GHz:
		WDh->Ts = 0.3125f;
		break;
	case CAEN_DGTZ_SAM_1_6GHz:
		WDh->Ts = 0.625f;
		break;
	case CAEN_DGTZ_SAM_800MHz:
		WDh->Ts = 1.25f;
		break;
	case CAEN_DGTZ_SAM_400MHz:
		WDh->Ts = 2.5f;
		break;
	}

	/* Set Pulser Parameters */
	for (channel = 0; channel < NbOfChannels; channel++) {
		if (WDb->channels[channel].EnablePulseChannels == 1)
			ret |= CAEN_DGTZ_EnableSAMPulseGen(handle, channel, WDb->channels[channel].PulsePattern, CAEN_DGTZ_SAMPulseCont);
		else
			ret |= CAEN_DGTZ_DisableSAMPulseGen(handle, channel);
	}

	/* Set Trigger Threshold */
	for (channel = 0; channel < NbOfChannels; channel++) {
		float valF = WDb->channels[channel].TriggerThreshold_V + WDb->channels[channel].DCOffset_V;
		int reg_val = (int)((MAX_DAC_RAW_VALUE - valF) / (MAX_DAC_RAW_VALUE - MIN_DAC_RAW_VALUE) * 65535);  // Inverted Range
		ret |= CAEN_DGTZ_SetChannelTriggerThreshold(handle, channel, reg_val);
	}

	/* Set Trigger Source */
	// reset channel trigger
	channelsMask = (1 << NbOfChannels) - 1; //all channels of the board
	ret |= CAEN_DGTZ_SetChannelSelfTrigger(handle, CAEN_DGTZ_TRGMODE_DISABLED, channelsMask); //disable self trigger on all channels
	// calculate the channel mask
	channelsMask = 0;
	for (i = 0; i < NbOfChannels / 2; i++)
		channelsMask += (WDb->channels[i * 2].ChannelTriggerEnable + ((WDb->channels[i * 2 + 1].ChannelTriggerEnable) << 1)) << (2 * i);
	switch (WDb->TriggerType) {
	case SYSTEM_TRIGGER_SOFT:
		ret |= CAEN_DGTZ_SetSWTriggerMode(handle, CAEN_DGTZ_TRGMODE_ACQ_ONLY);
		ret |= CAEN_DGTZ_SetExtTriggerInputMode(handle, CAEN_DGTZ_TRGMODE_DISABLED);
		WDrun.ContinuousTrigger = 1;
		break;
	case SYSTEM_TRIGGER_NORMAL:
		ret |= CAEN_DGTZ_SetSWTriggerMode(handle, CAEN_DGTZ_TRGMODE_ACQ_ONLY);
		ret |= CAEN_DGTZ_SetChannelSelfTrigger(handle, CAEN_DGTZ_TRGMODE_ACQ_ONLY, channelsMask);
		ret |= CAEN_DGTZ_SetExtTriggerInputMode(handle, CAEN_DGTZ_TRGMODE_DISABLED);
		break;
	case SYSTEM_TRIGGER_EXTERNAL:
		ret |= CAEN_DGTZ_SetSWTriggerMode(handle, CAEN_DGTZ_TRGMODE_ACQ_ONLY);
		ret |= CAEN_DGTZ_SetChannelSelfTrigger(handle, CAEN_DGTZ_TRGMODE_EXTOUT_ONLY, channelsMask);
		ret |= CAEN_DGTZ_SetExtTriggerInputMode(handle, CAEN_DGTZ_TRGMODE_ACQ_ONLY);
		break;
	case SYSTEM_TRIGGER_ADVANCED:
		ret |= CAEN_DGTZ_SetSWTriggerMode(handle, WDb->SwTrigger);
		ret |= CAEN_DGTZ_SetChannelSelfTrigger(handle, WDb->ChannelSelfTrigger, channelsMask);
		ret |= CAEN_DGTZ_SetExtTriggerInputMode(handle, WDb->ExtTrigger);
		break;
	}

	/* Set Trigger Polarity */
	for (channel = 0; channel < NbOfChannels; channel++) {
		ret |= CAEN_DGTZ_SetTriggerPolarity(handle, channel, WDb->channels[channel].TriggerPolarity);
	}

	/* Set Channel DC Offset */
	for (channel = 0; channel < NbOfChannels; channel++) {
		float valF = WDb->channels[channel].DCOffset_V;
		int reg_val = (int)((MAX_DAC_RAW_VALUE + valF) / (MAX_DAC_RAW_VALUE - MIN_DAC_RAW_VALUE) * 65535);  // Inverted Range
		ret |= CAEN_DGTZ_SetChannelDCOffset(handle, channel, reg_val);
	}

	/* Set Correction Level */
	ret |= CAEN_DGTZ_SetSAMCorrectionLevel(handle, WDb->CorrectionLevel);

	/* Set MAX NUM EVENTS */
	ret |= CAEN_DGTZ_SetMaxNumEventsBLT(handle, MAX_NUM_EVENTS_BLT);

	/* Set Recording Depth */
	ret |= CAEN_DGTZ_SetRecordLength(handle, WDb->RecordLength);

	/* Set Front Panel I/O Control */
	ret |= CAEN_DGTZ_SetIOLevel(handle, WDb->FPIOtype);

	ret |= CAEN_DGTZ_SetAcquisitionMode(handle, CAEN_DGTZ_SW_CONTROLLED);

	/* execute generic write commands */
	for (i = 0; i < WDb->GWn; i++)
		ret |= WriteRegisterBitmask(handle, WDb->GWaddr[i], WDb->GWdata[i], WDb->GWmask[i]);

	if (ret) {
		printf("\n");
		msg_printf(MsgLog, "WARN: there were errors when configuring the digitizer.\n");
		printf("\tSome settings may not be executed\n\n");
	}

	return 0;
}

int ProgramSynchronization(WaveDemoConfig_t *WDcfg) {
	int ret = 0;
	ERROR_CODES_t ErrCode = ERR_NONE;
	WaveDemoBoard_t *WDb;
	WaveDemoBoardHandle_t *WDh;
	uint32_t d32;

	for (int i = 0; i < WDcfg->NumBoards; i++) {
		WDb = &WDcfg->boards[i];
		WDh = &WDcfg->handles[i];
		int handle = WDh->handle;
		int NbOfChannels = WDh->Nch;

		if (i == 0) {
			// master
			// set start mode to sw controlled
			ret |= CAEN_DGTZ_ReadRegister(handle, CAEN_DGTZ_ACQ_CONTROL_ADD, &d32);
			ret |= CAEN_DGTZ_WriteRegister(handle, CAEN_DGTZ_ACQ_CONTROL_ADD, (d32 & 0xFFFFFFF8) | RUN_START_ON_SOFTWARE_COMMAND);

			//enable veto in
			ret |= CAEN_DGTZ_ReadRegister(handle, CAEN_DGTZ_ACQ_CONTROL_ADD, &d32);
			ret |= CAEN_DGTZ_WriteRegister(handle, CAEN_DGTZ_ACQ_CONTROL_ADD, (d32 & 0xFFFFEDFF) | 0x1200);

		}
		else {
			// slave
			// set start mode to lvds controlled
			ret |= CAEN_DGTZ_ReadRegister(handle, CAEN_DGTZ_ACQ_CONTROL_ADD, &d32);
			ret |= CAEN_DGTZ_WriteRegister(handle, CAEN_DGTZ_ACQ_CONTROL_ADD, (d32 & 0xFFFFFFF8) | RUN_START_ON_LVDS_IO);

			//enable veto in and busy in
			ret |= CAEN_DGTZ_ReadRegister(handle, CAEN_DGTZ_ACQ_CONTROL_ADD, &d32);
			ret |= CAEN_DGTZ_WriteRegister(handle, CAEN_DGTZ_ACQ_CONTROL_ADD, (d32 & 0xFFFFECFF) | 0x1300);
		}

		// option to use VetoIn LVDS to disable TRGOUT generation (set bit 12 of register 0x8100)
		ret |= CAEN_DGTZ_ReadRegister(handle, CAEN_DGTZ_ACQ_CONTROL_ADD, &d32);
		ret |= CAEN_DGTZ_WriteRegister(handle, CAEN_DGTZ_ACQ_CONTROL_ADD, (d32 & 0xFFFFEFFF) | 0x1000);

		// TRGOUT_VETO delay (default is 0x14, value should be units of 10 ns)
		ret |= CAEN_DGTZ_WriteRegister(handle, 0x81C4, 0x14);

		// set lvds
		ret |= CAEN_DGTZ_ReadRegister(handle, CAEN_DGTZ_FRONT_PANEL_IO_CTRL_ADD, &d32);
		ret |= CAEN_DGTZ_WriteRegister(handle, CAEN_DGTZ_FRONT_PANEL_IO_CTRL_ADD, (d32 & 0xFFFFFEC1) | 0x120);

		ret |= CAEN_DGTZ_WriteRegister(handle, 0x81A0, 0x2200);
		
		// set run delay (it is 2 between boards)
		// so, set 0 for the last, 2 for the penultimate, 4 for the third last and so on
		ret |= CAEN_DGTZ_WriteRegister(handle, 0x8170, 0x2 * (WDcfg->NumBoards - 1 - i));
	}
	return ErrCode;
}

void initializer(WaveDemoConfig_t *WDcfg) {
	// some initializations
	WDrun.Xunits = 1;

	for (int b = 0; b < WDcfg->NumBoards; b++) {
		// get pointer to substructure
		WaveDemoBoard_t *WDb = &WDcfg->boards[b];
		WaveDemoBoardHandle_t *WDh = &WDcfg->handles[b];

		for (int g = 0; g < MAX_GR; g++) {
			// get pointer to substructure
			WaveDemoGroup_t *WDg = &WDb->groups[g];
		}

		for (int c = 0; c < MAX_CH; c++) {
			// get pointer to substructure
			WaveDemoChannel_t *WDc = &WDb->channels[c];

			if (WDc->ChannelEnable == 0) {
				WDc->ChannelTriggerEnable = 0;
				WDc->EnablePulseChannels = 0;
				WDc->PlotEnable = 0;
			}
		}
	}
}

/*!
 * \fn	int ProgramDigitizers(WaveDemoConfig_t *WDcfg)
 *
 * \brief	configure the digitizer according to the parameters read from the cofiguration file
 * 			and saved in the WDcfg data structure.
 *
 * \param [in,out]	WDcfg	WaveDumpConfig data structure.
 *
 * \return	0 = Success; negative numbers are error codes.
 */

int ProgramDigitizers(WaveDemoConfig_t *WDcfg) {
	int ret = 0;
	ERROR_CODES_t ErrCode = ERR_NONE;
	WaveDemoBoard_t *WDb;
	WaveDemoBoardHandle_t *WDh;
	int i;
	for (i = 0; i < WDcfg->NumBoards; i++) {
		WDb = &WDcfg->boards[i];
		WDh = &WDcfg->handles[i];
		printf("Configuring board # %d...\n", i);
		ret = ProgramBoard(WDb, WDh, WDcfg->doReset);
		if (ret) {
			ErrCode = ERR_DGZ_PROGRAM;
			break;
		}
		msg_printf(MsgLog, "INFO: Board # %d configured.\n", i);
	}
#ifdef _DEBUG
	for (i = 0; i < WDcfg->NumBoards; i++) {
		WDh = &WDcfg->handles[i];
		printf("DEBUG: Saving Register Images to file reg_image_%d.txt...", i);
		if (SaveRegImage(WDh->handle) < 0)
			printf(" Failed!\n");
		else
			printf(" Done.\n");
	}
#endif
	return ErrCode;
}

int CheckTOFStartCh(WaveDemoConfig_t *WDcfg) {
	ERROR_CODES_t ErrCode = ERR_NONE;
	int ch;
	while (WDcfg->boards[WDcfg->TOFstartBoard].channels[WDcfg->TOFstartChannel].ChannelEnable == 0) {
		msg_printf(MsgLog, "ERROR: The board %d channel %d is disabled, it can't be the TOF start.\n", WDcfg->TOFstartBoard, WDcfg->TOFstartChannel);
		printf("Please, enter another channel (enter 99 to abort the program): ");
		if (scanf("%d", &ch) != 1)
			return ERR_CONF;
		if (ch == 99)
			return ERR_CONF;
		if (ch >= 0 && ch < WDcfg->handles[0].Nch) {
			WDcfg->TOFstartChannel = ch;
		}
		else {
			printf("%d is an invalid value\n", ch);
		}
	}
	return ErrCode;
}
int CheckRefCh(WaveDemoConfig_t *WDcfg) {
	ERROR_CODES_t ErrCode = ERR_NONE;
	int count = 0;
	int ch;
	int *ref_ch = &WDcfg->boards[0].RefChannel;
	while (WDcfg->boards[0].channels[*ref_ch].ChannelEnable == 0) {
		if (!count)
			printf("In synchronized mode, there must be a reference channel of the board 0.\n");
		msg_printf(MsgLog, "ERROR: Channel %d is disabled, it can't be the reference.\n", WDcfg->boards[0].RefChannel);
		printf("Please, enter another channel (enter 99 to abort the program): ");
		if (scanf("%d", &ch) != 1)
			return ERR_CONF;
		if (ch == 99)
			return ERR_CONF;
		if (ch >= 0 && ch < WDcfg->handles[0].Nch) {
			WDcfg->boards[0].RefChannel = ch;
		}
		else {
			printf("%d is an invalid value\n", ch);
		}
		count++;
	}
	return ErrCode;
}
void SetRefCh(WaveDemoConfig_t *WDcfg) {
	for (int b = 0; b < WDcfg->NumBoards; b++) {
		for (int ch = 0; ch < WDcfg->handles[b].Nch; ch++) {
			if (WDcfg->boards[b].channels[ch].ChannelEnable) {
				WDcfg->boards[b].RefChannel = ch;
				break;
			}
		}
	}
}

void SetFirstChannelEnableToPlot(WaveDemoRun_t *WDrun, WaveDemoConfig_t *WDcfg, int bd) {
	if (bd < 0 || bd > WDcfg->NumBoards)
		return;
	for (int ch = 0; ch < WDcfg->handles[bd].Nch; ch++) {
		if (WDcfg->boards[bd].channels[ch].ChannelEnable) {
			WDrun->BrdToPlot = bd;
			WDrun->ChToPlot = ch;
			break;
		}
	}
}

/*!
* \fn	void ConfigureChannelsPlot(WaveDemoRun_t *WDrun, WaveDemoConfig_t *WDcfg)
*/

void ConfigureChannelsPlot(WaveDemoRun_t *WDrun, WaveDemoConfig_t *WDcfg) {
	WaveDemoBoard_t *WDb;
	WaveDemoBoardHandle_t *WDh;
	WaveDemoBoardRun_t *WDr;

	WDrun->NumPlotEnable = 0;
	WDrun->ChannelEnabled.num = 0;
	WDrun->ChannelEnabled.index = 0;

	for (int b = 0; b < WDcfg->NumBoards; b++) {
		WDb = &WDcfg->boards[b];
		WDh = &WDcfg->handles[b];
		WDr = &WDcfg->runs[b];

		for (int c = 0; c < WDh->Nch; c++) {
			WaveDemoChannel_t *WDc = &WDb->channels[c];
			WDr->ChannelPlotEnable[c] = 0;

			//if (WDrun->MultiBoardPlot == 2)
			//	continue;
			if (WDc->ChannelEnable) {
				WDrun->ChannelEnabled.ch[WDrun->ChannelEnabled.index].board = b;
				WDrun->ChannelEnabled.ch[WDrun->ChannelEnabled.index].channel = c;
				WDrun->ChannelEnabled.index++;
				WDrun->ChannelEnabled.num++;
			}

			if (WDb->channels[c].ChannelEnable /*&& (!WDrun->MultiBoardPlot || WDb->channels[c].PlotEnable)*/) {
				WDr->ChannelPlotEnable[c] = 1;
				WDrun->NumPlotEnable++;
			}
		}
	}

	WaveDemoChannel_t *WDChToPlot = &WDcfg->boards[WDrun->BrdToPlot].channels[WDrun->ChToPlot];

	if (!WDChToPlot->ChannelEnable || (WDrun->BrdToPlot >= WDcfg->NumBoards)) {
		int ch, b;
		for (b = 0; b < WDcfg->NumBoards; b++) {
			for (ch = 0; ch < MAX_CH; ch++) {
				if (WDcfg->boards[b].channels[ch].ChannelEnable) {
					WDrun->BrdToPlot = b;
					WDrun->ChToPlot = ch;
					printf("WARNING: the selected channel for plot is disabled; now plotting BD %d - CH %d\n", WDrun->BrdToPlot, WDrun->ChToPlot);
					b = WDcfg->NumBoards;  //to exit the outer loop
					break;
				}
			}
		}
	}
}

/*!
 * \fn	void SetChannelsToPlot(WaveDemoRun_t *WDrun, WaveDemoConfig_t *WDcfg, int board, int ch, int enable)
 *
 * \brief	Enables/Disables channels for plotting.
 *
 * \param [in]	WDrun 	If non-null, the drun.
 * \param [in]	WDcfg 	If non-null, the dcfg.
 * \param [in]	board	board number (-1 for all boards)
 * \param [in]	ch	    channel number (-1 for all channels)
 * \param [in]	enable	0 = disable, 1 = enable
 */
void SetChannelsToPlot(WaveDemoRun_t *WDrun, WaveDemoConfig_t *WDcfg, int board, int ch, int enable) {
	 WaveDemoBoard_t *WDb;
	 WaveDemoBoardHandle_t *WDh;
	 WaveDemoBoardRun_t *WDr;

	 int plot_enabled = 0;
	 for (int bd = 0; bd < WDcfg->NumBoards; bd++) {
		 if (board != -1 && board != bd)
			 continue;

		 WDb = &WDcfg->boards[bd];
		 WDh = &WDcfg->handles[bd];
		 WDr = &WDcfg->runs[bd];

		 for (int i = 0; i < WDh->Nch; i++) {
			 if (ch != -1 && ch != i)
				 continue;
			 WDr->ChannelPlotEnable[i] = 0;
			 if (WDb->channels[i].ChannelEnable) {
				 WDr->ChannelPlotEnable[i] = enable ? 1 : 0;
				 if (enable)
					 plot_enabled++;
			 }
		 }
	 }
	 WDrun->NumPlotEnable = plot_enabled;
 }

/*!
* \fn	void SetTracesToPlot(WaveDemoRun_t *WDrun, int trace, int enable)
*
* \brief	Enables/Disables traces to plot (for WPLOT_MODE_1CH and WPLOT_MODE_STD).
*
* \param [in]	WDrun 	If non-null, the drun.
* \param [in]	trace	trace number (-1 for all traces)
* \param [in]	enable	0 = disable, 1 = enable
*/
void SetTracesToPlot(WaveDemoRun_t *WDrun, int trace, int enable) {
	WDrun->TraceEnable[0] = true; //this trace is always enabled
	for (int i = 1; i < MAX_NTRACES; i++) {
		if (trace == -1) {
			WDrun->TraceEnable[i] = enable ? TraceEnableDefault[i] : false;
		}
		else {
			if (trace != i)
				continue;
			WDrun->TraceEnable[i] = enable ? true : false;
		}
	}
}

void ChannelPlotController(WaveDemoRun_t *WDrun, WaveDemoConfig_t *WDcfg, int enable) {
	int board, channel;
	WaveDemoBoard_t *WDb;
	WaveDemoBoardRun_t *WDr;
	char current_enable;

	if (enable)
		printf("Enter channel to be added to the plot (#board-#channel, e.g. 0-2): ");
	else
		printf("Enter channel to be removed from the plot (#board-#channel, e.g. 0-2): ");
	if (scanf("%d-%d", &board, &channel) != 2) {
		printf("Invalid input\n");
		return;
	}

	//checks bounds
	if (board < 0 || board >= WDcfg->NumBoards) {
		printf("Invalid board entered\n");
		return;
	}
	if (channel < 0 || channel >= WDcfg->handles[board].Nch) {
		printf("Invalid channel entered\n");
		return;
	}
	WDb = &WDcfg->boards[board];
	WDr = &WDcfg->runs[board];
	if (WDb->channels[channel].ChannelEnable) {
		current_enable = WDr->ChannelPlotEnable[channel];
		if (current_enable && !enable) {
			WDrun->NumPlotEnable--;
		}
		else if (!current_enable && enable) {
			WDrun->NumPlotEnable++;
		}
		WDr->ChannelPlotEnable[channel] = enable ? 1 : 0;
		if (WDr->ChannelPlotEnable[channel]) {
			printf("Channel %d of board %d added to plot\n", channel, board);
		}
		else {
			printf("Channel %d of board %d removed from plot\n", channel, board);
		}
	}
	else
		printf("Channel %d of board %d is disabled\n", channel, board);
	return;
}

void RegisterModeController(WaveDemoConfig_t *WDcfg) {
	int c = 0;
	int board = 0;
	unsigned int addr, val;

	if (WDcfg->NumBoards > 1) {
		printf("Enter board index: ");
		if (scanf("%d", &board) != 1)
			return;
		if (board < 0 || board >= WDcfg->NumBoards) {
			printf("Invalid board entered\n");
			return;
		}
	}

	printf("Enter register address: 0x");
	if (scanf("%x", &addr) != 1)
		return;
	CAEN_DGTZ_ReadRegister(WDcfg->handles[board].handle, addr, &val);
	printf("%04X =  %08X\n", addr, val);

	while (c != 'x') {
		printf("[b] change board index, [c] change register address, [r] read, [w] write, [x] go back\n");
		c = getch();
		switch (c) {
		case 'b':
			printf("Enter board index: ");
			if (scanf("%d", &board) != 1)
				return;
			if (board < 0 || board >= WDcfg->NumBoards) {
				printf("Invalid board entered\n");
				return;
			}
			break;
		case 'c':
			printf("Enter register address: 0x");
			if (scanf("%x", &addr) != 1)
				return;
			/* Deliberately fall through */
		case 'r':
			CAEN_DGTZ_ReadRegister(WDcfg->handles[board].handle, addr, &val);
			printf("%04X =  %08X\n", addr, val);
			break;
		case 'w':
			printf("Enter new value: 0x");
			if (scanf("%x", &val) != 1)
				return;
			CAEN_DGTZ_WriteRegister(WDcfg->handles[board].handle, addr, val);
			break;
		default:
			break;
		}
	}
	printf("\n");
}

/*!
 * \fn	int CheckKeyboardCommands(WaveDemoRun_t *WDrun, WaveDemoConfig_t *WDcfg)
 *
 * \brief	check if there is a key pressed and execute the relevant command.
 *
 * \param [in,out]	WDrun	Pointer to the WaveDemoRun_t data structure.
 * \param [in,out]	WDcfg	Pointer to the WaveDemoConfig_t data structure.
 */

int CheckKeyboardCommands(WaveDemoRun_t *WDrun, WaveDemoConfig_t *WDcfg) {
	int ret = 0;
	int c = 0;
	int n;
	WaveDemoBoard_t *WDb;
	WaveDemoBoardHandle_t *WDh;
	WaveDemoBoardRun_t *WDr;

	char boardstr[16] = "";
	if (WDcfg->NumBoards > 1) {
		sprintf(boardstr, "[board %d] ", WDrun->BoardSelected);
	}

	if (!kbhit())
		return -1;

	c = getch();

	if ((c <= '9') && (c >= '0')) {
		n = c - '0' + WDrun->KeyDigitAdd;
		switch (WDrun->KeySelector)	{
		case KEYSEL_TRACES:
			if (n > 0 && n < MAX_NTRACES) {
				if (WDrun->TraceEnable[n]) {
					WDrun->TraceEnable[n] = false;
					printf("Trace \"%s\" removed from the plot\n", TraceNames[n]);
				}
				else {
					WDrun->TraceEnable[n] = true;
					printf("Trace \"%s\" added to the plot\n", TraceNames[n]);
				}
			}
			else
				printf("Trace %d unavailable\n", n);
			break;
		case KEYSEL_BOARD:
			if (n < WDcfg->NumBoards) {
				WDrun->BoardSelected = n;
				printf("Board %d is selected\n", n);
				WDrun->KeySelector = KEYSEL_CH;
			}
			else if (n < MAX_BD)
				printf("Board %d unavailable\n", n);
			else
				printf("Board %d unavailable (Max boards supported are %d)\n", n, MAX_BD);
			break;
		case KEYSEL_CH:
		default:
			WDb = &WDcfg->boards[WDrun->BoardSelected];
			WDh = &WDcfg->handles[WDrun->BoardSelected];
			WDr = &WDcfg->runs[WDrun->BoardSelected];

			if (n < WDh->Nch) {
				if (WDb->channels[n].ChannelEnable) {
					bool chToPlotIsChanged = WDrun->ChToPlot != n;
					// set board and channel to plot (used for waveform single channel and histogram plots)
					WDrun->BrdToPlot = WDrun->BoardSelected;
					WDrun->ChToPlot = n;

					if (WDrun->WavePlotMode == WPLOT_MODE_1CH && chToPlotIsChanged) 
						WDrun->SetPlotOptions = 1;
					
					if (WDrun->WavePlotMode == WPLOT_MODE_1BD) {
						// enable/disable channel for waveform multichannel plot
						WDr->ChannelPlotEnable[n] = WDr->ChannelPlotEnable[n] ? 0 : 1;
						if (WDr->ChannelPlotEnable[n]) {
							WDrun->NumPlotEnable++;
							printf("%sChannel %d added to the plot\n", boardstr, n);
						}
						else {
							WDrun->NumPlotEnable--;
							if (WDrun->NumPlotEnable == 0)
								ClearPlot();
							printf("%sChannel %d removed from the plot\n", boardstr, n);
						}
						WDrun->SetPlotOptions = 1;
					}
				}
				else {
					printf("%sChannel %d is disable\n", boardstr, n);
				}
			}
			else
				printf("%sChannel %d unavailable for this board\n", boardstr, n);
			break;
		}
	}
#ifdef _DEBUG
	else if (c == 0 || c == 224) {
		int dir = 0;
		switch (getch()) {
		case 72:
			/* Code for up arrow handling */
			dir = 2;
			break;
		case 75:
			/* Code for left arrow handling */
			dir = -1;
			break;
		case 77:
			/* Code for right arrow handling */
			dir = 1;
			break;
		case 80:
			/* Code for down arrow handling */
			dir = -2;
			break;
		}
		switch (WDrun->LastKeyOptSel) {
		case 'z':
			for (int i = 1; i < MAX_NTRACES; i++) {
				if (dir == -2)
					WDrun->TraceEnable[i] = false;
				else if (dir == 2)
					WDrun->TraceEnable[i] = TraceEnableDefault[i];
			}
			break;
		case 'c': //it is not executed when 'c' was pressed
			WDrun->ChannelEnabled.index += dir;
			if (WDrun->ChannelEnabled.index < 0)
				WDrun->ChannelEnabled.index = WDrun->ChannelEnabled.num;
			else
				WDrun->ChannelEnabled.index %= WDrun->ChannelEnabled.num;
			WDrun->BrdToPlot = WDrun->ChannelEnabled.ch[WDrun->ChannelEnabled.index].board;
			WDrun->ChToPlot = WDrun->ChannelEnabled.ch[WDrun->ChannelEnabled.index].channel;
			break;
		case 'f': //it is not executed when 'f' was pressed
			WDrun->StatsMode++;
			if (WDrun->StatsMode == 3) {
				ClearScreen();
				WDrun->StatsMode = -1;
			}
			break;
		case 'h': //it is not executed when 'h' was pressed
			WDrun->HistoPlotType = ((WDrun->HistoPlotType + dir) % HPLOT_TYPE_DUMMY_LAST);
			if (WDrun->HistoPlotType == 0)
				WDrun->HistoPlotType = (dir > 0) ? 1: HPLOT_TYPE_DUMMY_LAST - 1;
			break;
		case 'g': //it is not executed when 'g' was pressed
			WDrun->WavePlotMode = ((WDrun->WavePlotMode + dir) % WPLOT_MODE_DUMMY_LAST);
			if (WDrun->WavePlotMode < 0 && dir < 0)
				WDrun->WavePlotMode = WPLOT_MODE_DUMMY_LAST - 1;
			WDrun->SetPlotOptions = 1;
			break;
		default:
			break;
		}
	}
#endif // _DEBUG
	else {
		// cases in which the last selected option must be remembered
		switch (c) {
		case 'z':
		case 'c':
		case 'f':
		case 'g':
		case 'h':
			WDrun->LastKeyOptSel = c;
			break;
		default:
			break;
		}
		
		// keys manager
		switch (c) {
		case 'a':
			ChannelPlotController(WDrun, WDcfg, 1);
			break;
		case 'd':
			ChannelPlotController(WDrun, WDcfg, 0);
			break;
		case '+':
			WDrun->KeyDigitAdd += 10;
			if (WDrun->KeyDigitAdd > 10 * KEYDIGITADD_MAX)
				WDrun->KeyDigitAdd = 10 * KEYDIGITADD_MAX;
			printf("Digits [0-9] acts as [%d-%d]\n", WDrun->KeyDigitAdd, WDrun->KeyDigitAdd + 9);
			break;
		case '-':
			WDrun->KeyDigitAdd -= 10;
			if (WDrun->KeyDigitAdd < 0)
				WDrun->KeyDigitAdd = 0;
			printf("Digits [0-9] acts as [%d-%d]\n", WDrun->KeyDigitAdd, WDrun->KeyDigitAdd + 9);
			break;
		case '*':
			if (WDrun->LastKeyOptSel == 'c') {
				SetChannelsToPlot(WDrun, WDcfg, -1, -1, 1);
				printf("All enabled channels are added to the plot\n");
			}
			else if (WDrun->LastKeyOptSel == 'z') {
				SetTracesToPlot(WDrun, -1, 1);
				printf("Default traces are shown on the plot\n");
			}	
			break;
		case '/':
			if (WDrun->LastKeyOptSel == 'c') {
				SetChannelsToPlot(WDrun, WDcfg, WDrun->BrdToPlot, -1, 0);
				SetFirstChannelEnableToPlot(WDrun, WDcfg, WDrun->BoardSelected);
				SetChannelsToPlot(WDrun, WDcfg, WDrun->BrdToPlot, WDrun->ChToPlot, 1);
				printf("Only one channel is plotted\n");
			}
			else if (WDrun->LastKeyOptSel == 'z') {
				SetTracesToPlot(WDrun, -1, 0);
				printf("Only %s trace is shown on the plot\n", TraceNames[0]);
			}
			break;
		case 'z':
			WDrun->KeySelector = KEYSEL_TRACES;
			printf("Traces selector\n");
			break;
		case 'c':
			// Update plot to the next channel index
			if (WDrun->KeySelector == KEYSEL_CH) {
				WDrun->ChannelEnabled.index++;
				WDrun->ChannelEnabled.index %= WDrun->ChannelEnabled.num;					
				WDrun->BrdToPlot = WDrun->ChannelEnabled.ch[WDrun->ChannelEnabled.index].board;
				WDrun->ChToPlot = WDrun->ChannelEnabled.ch[WDrun->ChannelEnabled.index].channel;
				printf("Change plot to board %d - channel %d.\n", WDrun->BrdToPlot, WDrun->ChToPlot);
				if (WDrun->WavePlotMode == WPLOT_MODE_1CH) {
					ClearPlot();
					WDrun->SetPlotOptions = 1;
				}
			}
			else {
				// Switch key selector for channels
				WDrun->KeySelector = KEYSEL_CH;
				printf("Channels selector\n");
			}
			break;
		case 'b':
			if (WDcfg->NumBoards == 1) {
				printf("Only one board is connected, now it is useless!\n");
				break;
			}
			// Change plot to the next board index if multiple channels is active
			if (WDrun->KeySelector == KEYSEL_BOARD && WDrun->WavePlotMode == WPLOT_MODE_1BD) {
				WDrun->BoardSelected = (WDrun->BoardSelected + 1) % WDcfg->NumBoards;
				SetChannelsToPlot(WDrun, WDcfg, WDrun->BoardSelected, -1, 1);
				SetFirstChannelEnableToPlot(WDrun, WDcfg, WDrun->BoardSelected);
				printf("Change to board %d. All enabled channels are added to the plot\n", WDrun->BoardSelected);
				ClearPlot();
			}
			else {
				// Switch key selector for boards
				WDrun->KeySelector = KEYSEL_BOARD;
				printf("Boards selector\n");
			}
			break;
		case 'g':
			WDrun->WavePlotMode = ((WDrun->WavePlotMode + 1) % WPLOT_MODE_DUMMY_LAST);
			ClearPlot();
			WDrun->SetPlotOptions = 1;
			break;
		case 'q':
			if (WDrun->AcqRun) {
				printf("\nAre you sure to quit from the program? (press 'y' for yes) ");
				c = getch();
				if (c == 'y' || c == 'Y') {
					WDrun->Quit = 1;
				}
			}
			else
				WDrun->Quit = 1;
			printf("\n");
			break;
		case '\r': //[enter]
			break;
		case '\t': //[tab]
			break;
		case 'R':
			printf("Restart.\nAre you really sure? (press 'y' for yes) ");
			c = getch();
			if (c == 'y' || c == 'Y') {
				WDrun->Restart = 1;
			}
			printf("\n");
			break;
		case 't':
			if (!WDrun->ContinuousTrigger) {
				SendSWtrigger(WDcfg);
				printf("Single Software Trigger issued\n");
			}
			break;
		case 'T':
			WDrun->ContinuousTrigger ^= 1;
			if (WDrun->ContinuousTrigger)
				printf("Continuous trigger is enabled\n");
			else
				printf("Continuous trigger is disabled\n");
			break;
		case 'P':
			if (WDrun->NumPlotEnable == 0)
				printf("No channel enabled for plotting\n");
			else
				WDrun->ContinuousPlot ^= 1;
			break;
		case 'p':
			if (WDrun->NumPlotEnable == 0)
				printf("No channel enabled for plotting\n");
			else {
				WDrun->ContinuousPlot = 0;
				WDrun->SinglePlot = 1;
			}
			break;
		case 'i':
			PrintDigitizersInfo(NULL, WDcfg);
			break;
		case 'm':
			WDrun->IntegratedRates ^= 1;
			if (WDrun->IntegratedRates)
				printf("Statistics mode: integral\n");
			else
				printf("Statistics mode: instantaneous\n");
			break;
		case 'H':
			WDrun->HistoPlotType = HPLOT_DISABLED;
			ClearHistoPlot();
			break;
		case 'h':
			WDrun->HistoPlotType = ((WDrun->HistoPlotType + 1) % HPLOT_TYPE_DUMMY_LAST);
			if (WDrun->HistoPlotType == HPLOT_DISABLED)
				WDrun->HistoPlotType = HPLOT_DISABLED + 1;
			break;
		case 'x':
			WDrun->Xunits ^= 1;
			break;
		case 'w':
			if (!WDrun->ContinuousWrite)
				WDrun->SingleWrite = 1;
			break;
		case 'W':
			WDrun->ContinuousWrite ^= 1;
			if (WDrun->ContinuousWrite)
				printf("Continuous writing is enabled\n");
			else
				printf("Continuous writing is disabled\n");
			break;
		case 'F':
			WDrun->DoRefresh = 0;
			break;
		case 'f':
			WDrun->DoRefresh = 1;
			break;
		case 'o':
			if (WDrun->DoRefresh == 0)
				WDrun->DoRefreshSingle = 1;
			break;
		case 's':
			if (WDrun->AcqRun == 0) {
				if (WDcfg->SyncEnable) {
					ProgramSynchronization(WDcfg);
				}
				StartAcquisition(WDcfg);
				printf("Acquisition started\n");
				WDrun->AcqRun = 1;
				WDrun->DoRefresh = 1;
			}
			else {
				StopAcquisition(WDcfg);
				/* close the output files */
				CloseOutputDataFiles();
				printf("Acquisition stopped\n");
				WDrun->AcqRun = 0;
			}
			break;
		case 'e':
			printf("Reset Histograms and Statistics.\nAre you really sure? (press 'y' for yes) ");
			c = getch();
			if (c == 'y' || c == 'Y') {
				ResetHistograms();
				ResetStatistics();
				printf("Reset done.\n");
			}
			else
				printf("Canceled.\n");
			break;
		case 'r':
			if (WDrun->AcqRun == 0) {
				RegisterModeController(WDcfg);
				printf("[s] start/stop the acquisition, [q] quit, [?] help\n");
			}
			else
				printf("Operation not allowed during the acquisition\n");
			break;
#ifdef _DEBUG
		case 'v':
			printf("Change Waveform Processor setting.\nInsert new value or press enter to cancel: ");
			c = getch();
			if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
				WDcfg->WaveformProcessor = hexToInt((char)c);
				printf("0x%X has been set.\n", WDcfg->WaveformProcessor);
			}
			else 
				printf("Canceled.\n");
			break;
#endif // _DEBUG
		case '?':
		case ' ':
			printf("\n                         Keyboard shortcut help                         \n");
			printf("--------------------------------------------------------------------------\n");
			printf("   [?]   This help\n");
			printf("   [q]   Quit\n");
			printf("   [i]   Get info on the digitizers\n");
			printf("   [r]   Enter in Read/Write register mode\n");
			printf("   [R]   Reload configuration file and restart\n");
			printf("   [s]   Toggle Start/Stop acquisition\n");
			printf("   [T]   Toggle Enable/Disable continuous software trigger\n");
			printf("   [t]   Force software trigger (single shot)\n");
			printf("   [W]   Toggle Enable/Disable continuous writing to output file\n");
			printf("   [w]   Write one event to output file\n");
			printf("   [e]   Reset Histograms and Statistics\n");
			printf(" [f]/[F] Enable/Disable automatic statistics refresh\n");
			printf("   [o]   One shot statistics refresh\n");
			printf("   [m]   Toggle statistics mode (integral/istantaneous)\n");
			printf(" [a]/[d] Add/Delete channel to the plot\n");
			printf("   [c]   Switch to channel selector mode\n");
			printf("   [b]   Switch to board selector mode or change the board to plot (only one board plot mode)\n");
			printf("   [z]   Switch to traces selector mode\n");
			printf("   [*]   Enable all channels on the plot\n");
			printf("   [/]   Enable only one channel on the plot\n");
			printf(" [+]/[-] Add/Subtract 10 on the digits entered\n");
			printf("  [0-9]  Enable/Disable selected channel on the plot\n");
			printf("   [P]   Toggle Enable/Disable continuous Waveform plot\n");
			printf("   [p]   Plot one event at a time (stops if plot is continuous)\n");
			printf("   [g]   Toggle between Waveform plot modes\n");
			printf(" [h]/[H] Enable/Disable Histogram plot\n");
			printf("   [h]   Toggle between Histogram plot types\n");
			printf("   [x]   Toggle between Channels and Units in the Histogram plot\n");
#ifdef _DEBUG
			printf("   [v]   Change waveform processing\n");
			printf(" --- Below keys work depending on the selected one before ---\n");
			printf(" [RIGHT]/[LEFT] Go next/back (channel, plot, mode, ...)\n");
			printf("  [UP]/[DOWN]   Enable/Disable all traces\n");
#endif // _DEBUG
			printf("--------------------------------------------------------------------------\n");
			printf("\tPress a key to continue\n");
			getch();
			printf("[s] start/stop the acquisition, [q] quit, [?] help\n");
			break;
		default:
			if (c >= 65 && c <= 90)
				printf("Please be careful if the caps lock is active\n");
			else
				ret = 1;
			break;
		}
	}
	return ret;
}

// make a string with size value and units
void BytesUnits(uint64_t size, char str[1000]) {
	if (size >= 1099511628000)
		sprintf(str, "%.4f TB", size / 1099511628000.0);
	else if (size >= 1073741824)
		sprintf(str, "%.4f GB", size / 1073741824.0);
	else if (size >= 1048576)
		sprintf(str, "%.4f MB", size / 1048576.0);
	else if (size >= 1024)
		sprintf(str, "%.4f KB", size / 1024.0);
	else
		sprintf(str, "%.0f B ", size / 1.0);
}
// make a string with frequency value and units
void FreqUnits(float freq, char str[1000]) {
	if (freq >= 1000000)
		sprintf(str, "%6.2f MHz", freq / 1000000);
	else if (freq >= 1000)
		sprintf(str, "%6.2f KHz", freq / 1000);
	else
		sprintf(str, "%6.2f Hz ", freq);
}
// Header string
void HeaderLogString(int StatsMode, char *str) {
	switch (StatsMode) {
	case 0:
		//                |   |             |        |        |          |          |
		sprintf(str, "Brd  Ch |   Throughput   Match%%  Queue%%    TotCnt   DeltaCnt");
		break;
	case 1:
		sprintf(str, "Brd  Ch |   Throughput    ICR        OCR      Match%%  DeadT%%     TotCnt");
		break;
	case 2:
		sprintf(str, "Brd  Ch |    Satur%%     Ovf%%  UnCorr%%    Busy%%   Queue%%        DeltaCnt");
		break;
	default:
		sprintf(str, "This mode is not implemented yet! Press [Tab] to change log mode.");
		break;
	}
}
// channel string reporting the statistics
void ChannelLogString(int b, int ch, int StatsMode, char *str) {
	char ecrs[100], ocrs[100], icrs[100];
	uint64_t nev, totnev;

	totnev = WDstats.EvRead_cnt[b][ch];
	nev = WDstats.EvRead_dcnt[b][ch];

	FreqUnits(WDstats.EvRead_rate[b][ch], ecrs);
	if (WDstats.EvInput_rate[b][ch] < 0)
		sprintf(icrs, "   N.A.   ");
	else
		FreqUnits(WDstats.EvInput_rate[b][ch], icrs);
	FreqUnits(WDstats.EvOutput_rate[b][ch], ocrs);

	sprintf(str, "%3d %2d  | ", b, ch);
	if (!WDcfg.boards[b].channels[ch].ChannelEnable) {
		sprintf(str, "%s   Disabled", str);
	}
	else if (StatsMode == 0) {
		//                                                      ECR       Match%%                         QueueOccup%%                DeltaCnt EcCnt");
		sprintf(str, "%s %s %6.2f%% %6.2f%% %10llu %10llu", str, ecrs, 100.0 * WDstats.MatchingRatio[b][ch], WDBuff_occupancy(&WDbuff, b), totnev, nev);
	}
	else if (StatsMode == 1) {
		//                                                    ECR   ICR   OCR   Match%%                         DeadT%%                  DeltaCnt");
		sprintf(str, "%s %s %s %s %6.2f%% %6.2f%% %10llu", str, ecrs, icrs, ocrs, 100.0 * WDstats.MatchingRatio[b][ch], 100.0 * WDstats.DeadTime[b][ch], totnev);
	}
}

void PrintStatistics() {
	char str[100];
	ClearScreen();
	printf("\t--- WaveDemo for x743 Digitizer Family  (version: %s) ---\n", WaveDemo_Release);
#ifdef _DEBUG
	printf("\t\tDEBUG VERSION IS RUNNING\n");
#endif // _DEBUG
	printf("Press [?] for help\n");
	printf("\n");
	printf("Acquisition started at %s\n", WDstats.AcqStartTimeString);
	switch (WDcfg.boards[0].CorrectionLevel) {
	case CAEN_DGTZ_SAM_CORRECTION_DISABLED:
		printf("Data Correction is disabled!\n");
		break;
	case CAEN_DGTZ_SAM_CORRECTION_PEDESTAL_ONLY:
		printf("Only Pedestral data correction is enabled\n");
		break;
	case CAEN_DGTZ_SAM_CORRECTION_INL:
		printf("Only Time INL data correction is enabled\n");
		break;
	case CAEN_DGTZ_SAM_CORRECTION_ALL:
		printf("All Data Corrections are enabled\n");
		break;
	default:
		break;
	}
	if (WDrun.ContinuousTrigger)
		printf("Continuous SOFTWARE TRIGGER is enabled!\n");

	if (WDrun.ContinuousWrite || WDcfg.SaveHistograms || WDcfg.SaveRunInfo) {
		printf("Enabled Output Files: ");
		if (WDcfg.SaveRawData)    printf("Raw ");
		if (WDcfg.SaveTDCList)    printf("TDCList ");
		if (WDcfg.SaveLists)      printf("Lists ");
		if (WDcfg.SaveWaveforms)  printf("Waveforms ");
		if (WDcfg.SaveHistograms) {
			printf("Histograms (");
			if (WDcfg.SaveHistograms & 1) printf("E");
			if (WDcfg.SaveHistograms & 2) printf("T");
			printf(") ");
		}
		if (WDcfg.SaveRunInfo)    printf("Info ");
		printf("\n");
	}
	else
		printf("Output Files disabled.\n");

	if (WDrun.NumPlotEnable) {
		printf("Enabled Waveform plot: ");
		if (WDrun.WavePlotMode == WPLOT_MODE_1BD)       printf("only output data of board %d ", WDrun.BrdToPlot);
		else if (WDrun.WavePlotMode == WPLOT_MODE_1CH)	printf("board %d - channel %02d ", WDrun.BrdToPlot, WDrun.ChToPlot);
		else if (WDrun.WavePlotMode == WPLOT_MODE_STD && WDcfg.SyncEnable)  printf("synchronous events ");
		else if (WDrun.WavePlotMode == WPLOT_MODE_STD)  printf("NO synchronous events ");
		if (WDrun.ContinuousPlot) printf("[continuous plot  ");
		else printf("[one shot plot ");
		printf("<< %c >>]\n", getProgressIndicator(&WPprogress));
	}
	else
		printf("Waveform plot disabled.\n");

	if (WDrun.HistoPlotType != HPLOT_DISABLED) {
		printf("Enabled Histogram plot: ");
		if (WDrun.HistoPlotType == HPLOT_ENERGY)    printf("ENERGY ");
		else if (WDrun.HistoPlotType == HPLOT_TIME) printf("TAC ");
		printf("board %d - channel %02d\n", WDrun.BrdToPlot, WDrun.ChToPlot);
	}
	else
		printf("Histogram plot disabled.\n");

	if (WDrun.IntegratedRates)
		printf("Statistics Mode: Integral\n");
	else
		printf("Statistics Mode: Istantaneous\n");

	printf("Total processed events = %llu\n", WDstats.TotEvRead_cnt);
	BytesUnits(WDstats.RxByte_cnt, str);
	printf("Total bytes = %s\n", str);

	if (WDstats.RealTimeSource == REALTIME_FROM_BOARDS)
		printf("RealTime (from boards) = %.2f s", WDstats.AcqRealTime / 1000);
	else
		printf("RealTime (from computer) = %.2f s", WDstats.AcqRealTime / 1000);

	printf("\n");
	printf("Readout Rate = %.2f MB/s\n", WDstats.RxByte_rate);

	if (WDstats.UnSyncEv_cnt) {
		printf("\n");
		printf("--------------------------------------------------\n");
		printf("/!\\ Unsynchronized events found = %llu\n", WDstats.UnSyncEv_cnt);
		printf("--------------------------------------------------\n");
		printf("\n");
	}

	for (int b = 0; b < WDcfg.NumBoards; b++) {
		char str[1000];
		HeaderLogString(WDrun.StatsMode, str);
		if (b == 0) {
			printf("\n%s\n", str);
			printf("-----------------------------------------------------------------------\n");
		}
		for (int ch = 0; ch < WDcfg.handles[b].Nch; ch++) {
			if (WDcfg.boards[b].channels[ch].ChannelEnable) {
				ChannelLogString(b, ch, WDrun.StatsMode, str);
				printf("%s\n", str);
			}
		}
	}
	printf("-----------------------------------------------------------------------\n");
	printf("\n\n");
}


/*!
 * \fn	int CheckBatchModeConditions(WaveDemoRun_t *WDrun, WaveDemoConfig_t *WDcfg)
 *
 * \brief	Check if batch mode termination conditions are met (time limit or event count limit).
 *          Returns 0 to stop acquisition, -1 to continue.
 *
 * \param [in,out]	WDrun	Pointer to the WaveDemoRun_t data structure.
 * \param [in,out]	WDcfg	Pointer to the WaveDemoConfig_t data structure.
 *
 * \return	0 to stop acquisition, -1 to continue.
 */

int CheckBatchModeConditions(WaveDemoRun_t *WDrun, WaveDemoConfig_t *WDcfg) {
	// If not in batch mode, continue normally
	if (WDcfg->BatchMode == 0)
		return -1;

	// Calculate total events across all boards and channels
	uint64_t totalEvents = 0;
	for (int b = 0; b < WDcfg->NumBoards; b++) {
		for (int ch = 0; ch < WDcfg->handles[b].Nch; ch++) {
			totalEvents += WDstats.EvProcessed_cnt[b][ch];
		}
	}
	WDrun->BatchEventsTotal = totalEvents;

	// Check event count condition
	if (WDcfg->BatchMaxEvents > 0 && totalEvents >= WDcfg->BatchMaxEvents) {
		printf("\nBatch mode: Maximum event count reached (%llu events)\n", 
			(unsigned long long)WDcfg->BatchMaxEvents);
		msg_printf(MsgLog, "INFO: Batch mode stopped - Maximum event count reached (%llu events)\n", 
			(unsigned long long)WDcfg->BatchMaxEvents);
		WDrun->AcqRun = 0;
		return 0;
	}

	// Check time condition
	if (WDcfg->BatchMaxTime > 0) {
		uint64_t currentTime = get_time();
		uint64_t elapsedSeconds = (currentTime - WDrun->BatchStartTime) / 1000;
		
		if (elapsedSeconds >= WDcfg->BatchMaxTime) {
			printf("\nBatch mode: Maximum time reached (%llu seconds)\n", 
				(unsigned long long)WDcfg->BatchMaxTime);
			msg_printf(MsgLog, "INFO: Batch mode stopped - Maximum time reached (%llu seconds)\n", 
				(unsigned long long)WDcfg->BatchMaxTime);
			WDrun->AcqRun = 0;
			return 0;
		}

		// Print progress every 10 seconds
		if (elapsedSeconds % 10 == 0 && elapsedSeconds > 0) {
			static uint64_t lastPrintTime = 0;
			if (elapsedSeconds != lastPrintTime) {
				printf("Batch mode progress: %llu/%llu seconds, %llu events\n",
					(unsigned long long)elapsedSeconds,
					(unsigned long long)WDcfg->BatchMaxTime,
					(unsigned long long)totalEvents);
				lastPrintTime = elapsedSeconds;
			}
		}
	}

	return -1; // Continue acquisition
}

/* ########################################################################### */
/* MAIN                                                                        */
/* ########################################################################### */

/*!
 * \fn	int main(int argc, char *argv[])
 *
 * \brief	Main entry-point for this application.
 *
 * \param	argc	The number of command-line arguments provided.
 * \param	argv	An array of command-line argument strings.
 *
 * \return	Exit-code for the process - 0 for success, else an error code.
 */

int main(int argc, char *argv[]) {
	ERROR_CODES_t ErrCode = ERR_NONE;

	char ConfigFileName[100];
	char MsgLogFileName[500];

	uint64_t CurrentTime, ElapsedTime;
	uint64_t PrevStatTime = 0, PrevLogTime = 0;
	int ForceStatUpdate = 1;
	int AcqRunGoFlag = 0;
	int AcqRunStopFlag = 0;
	int print_warn_stats_off = 1;

	FILE *f_ini;  // config file

	// print program version and exit
	if (argc == 2 && (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0)) {
		printf("%s\n", WaveDemo_Release);
		return 0;
	}

	// read raw binary file
	if (argc == 3 && strcmp(argv[1], "--read-raw") == 0) {
		const char* filePath = argv[2];
		FILE* inputFile = fopen(filePath, "rb");
		if (inputFile == NULL) {
			fprintf(stderr, "Unable to open file: %s\n", filePath);
			return -1;
		}

		int ret = 0;
		WaveDemoEvent_t *events[MAX_BD] = { NULL };
		if (ReadRawData(inputFile, &events, 1) != 0) {
			fprintf(stderr, "Unable to read or parse the file: %s\n", filePath);
			ret = -1;
		}

		for (int i = 0; i < MAX_BD; i++) {
			WaveDemoEvent_t* e = events[i];
			if (e) {
				for (int j = 0; j < MAX_V1743_GROUP_SIZE * MAX_X743_CHANNELS_X_GROUP; j++) {
					int g = j / MAX_X743_CHANNELS_X_GROUP;
					int c = j % MAX_X743_CHANNELS_X_GROUP;
					float* w = e->Event->DataGroup[g].DataChannel[c];
					if (w)
						free(w);
				}
				free(e);
			}
		}

		fclose(inputFile);
		return ret;
	}

	printf("\n");

	/* *************************************************************************************** */
	/* Software Initialize                                                                     */
	/* *************************************************************************************** */
	// init the console window (I/O terminal)
	InitConsole();
	// open message log file
	sprintf(MsgLogFileName, "%sMsgLog.txt", "");
	MsgLog = fopen(MsgLogFileName, "w");
	if (MsgLog == NULL) {
		msg_printf(NULL, "WARN: Can't open message log file %s.\n", MsgLogFileName);
		printf("\n");
	}
	msg_printf(MsgLog, "**************************************************************\n");
	msg_printf(MsgLog, "\tWaveDemo for x743 Digitizer Family  (version: %s)\n", WaveDemo_Release);
	msg_printf(MsgLog, "**************************************************************\n");

	memset(&WDrun, 0, sizeof(WDrun));
	memset(&WDcfg, 0, sizeof(WDcfg));
	memset(&WDstats, 0, sizeof(WDstats));
	memset(PrevChTimeStamp, 0, sizeof(float) * MAX_CH * MAX_BD);

	/* *************************************************************************************** */
	/* Get command line options                                                                */
	/* *************************************************************************************** */
	strcpy(ConfigFileName, DEFAULT_CONFIG_FILE);
	
	// Command-line overrides (will be applied after config file is parsed)
	int cmdline_batch_mode = -1;  // -1 means not set
	uint64_t cmdline_max_events = 0;
	uint64_t cmdline_max_time = 0;
	char cmdline_datapath[200] = "";
	int has_cmdline_overrides = 0;
	
	for (int i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			if (argv[i][1] == 'h' || strcmp(argv[i], "--help") == 0) {
				printf("Syntax: %s [options] [ConfigFileName]\n", argv[0]);
				printf("\n");
				printf("Options:\n");
				printf("  ConfigFileName              : configuration file (default is %s)\n", DEFAULT_CONFIG_FILE);
				printf("  --version                   : Print program version\n");
				printf("  -h, --help                  : Show this help message\n");
				printf("\n");
				printf("Batch Mode Options:\n");
				printf("  --batch                     : Enable batch mode 2 (no visualization)\n");
				printf("  --batch-mode <0|1|2>        : Set batch mode (0=interactive, 1=with vis, 2=no vis)\n");
				printf("  --max-events <N>            : Maximum events to record (overrides config)\n");
				printf("  --max-time <seconds>        : Maximum time in seconds (overrides config)\n");
				printf("  --output-path <path>        : Output data path (overrides config)\n");
				printf("\n");
				printf("Examples:\n");
				printf("  %s --batch --max-events 10000 --output-path ./my_data/\n", argv[0]);
				printf("  %s myconfig.ini --batch-mode 1 --max-time 300\n", argv[0]);
				printf("\n");
				goto QuitProgram;
			}
			else if (strcmp(argv[i], "--batch") == 0) {
				cmdline_batch_mode = 2;  // Batch mode without visualization
				has_cmdline_overrides = 1;
			}
			else if (strcmp(argv[i], "--batch-mode") == 0) {
				if (i + 1 < argc) {
					int mode = atoi(argv[++i]);
					if (mode >= 0 && mode <= 2) {
						cmdline_batch_mode = mode;
						has_cmdline_overrides = 1;
					}
					else {
						printf("ERROR: Invalid batch mode '%d'. Must be 0, 1, or 2.\n", mode);
						return -1;
					}
				}
				else {
					printf("ERROR: --batch-mode requires a value (0, 1, or 2)\n");
					return -1;
				}
			}
			else if (strcmp(argv[i], "--max-events") == 0) {
				if (i + 1 < argc) {
					cmdline_max_events = (uint64_t)atoll(argv[++i]);
					has_cmdline_overrides = 1;
				}
				else {
					printf("ERROR: --max-events requires a value\n");
					return -1;
				}
			}
			else if (strcmp(argv[i], "--max-time") == 0) {
				if (i + 1 < argc) {
					cmdline_max_time = (uint64_t)atoll(argv[++i]);
					has_cmdline_overrides = 1;
				}
				else {
					printf("ERROR: --max-time requires a value\n");
					return -1;
				}
			}
			else if (strcmp(argv[i], "--output-path") == 0) {
				if (i + 1 < argc) {
					strcpy(cmdline_datapath, argv[++i]);
					has_cmdline_overrides = 1;
				}
				else {
					printf("ERROR: --output-path requires a path\n");
					return -1;
				}
			}
			else {
				printf("WARNING: Unknown option '%s' (use --help for usage)\n", argv[i]);
			}
		}
		else {
			strcpy(ConfigFileName, argv[i]);
		}
	}

	/* *************************************************************************************** */
	/* Open and parse configuration file                                                       */
	/* *************************************************************************************** */
	msg_printf(MsgLog, "INFO: Opening Configuration File -> %s\n", ConfigFileName);
	printf("*** Loading...\n");
	f_ini = fopen(ConfigFileName, "r");
	if (f_ini == NULL) {
		ErrCode = ERR_CONF_FILE_NOT_FOUND;
		goto QuitProgram;
	}

	if (ParseConfigFile(f_ini, &WDcfg)) {
		ErrCode = ERR_CONF;
		goto QuitProgram;
	}
	fclose(f_ini);
	msg_printf(MsgLog, "INFO: Configuration file parsed\n");

	// Apply command-line overrides
	if (has_cmdline_overrides) {
		msg_printf(MsgLog, "INFO: Applying command-line overrides\n");
		
		if (cmdline_batch_mode >= 0) {
			WDcfg.BatchMode = cmdline_batch_mode;
			msg_printf(MsgLog, "  BatchMode = %d (from command line)\n", cmdline_batch_mode);
		}
		
		if (cmdline_max_events > 0) {
			WDcfg.BatchMaxEvents = cmdline_max_events;
			msg_printf(MsgLog, "  BatchMaxEvents = %llu (from command line)\n", (unsigned long long)cmdline_max_events);
		}
		
		if (cmdline_max_time > 0) {
			WDcfg.BatchMaxTime = cmdline_max_time;
			msg_printf(MsgLog, "  BatchMaxTime = %llu (from command line)\n", (unsigned long long)cmdline_max_time);
		}
		
		if (strlen(cmdline_datapath) > 0) {
			strcpy(WDcfg.DataFilePath, cmdline_datapath);
			NormalizeDataFilePath(WDcfg.DataFilePath);
			msg_printf(MsgLog, "  DataFilePath = %s (from command line)\n", WDcfg.DataFilePath);
		}
		
		printf("*** Command-line overrides applied\n");
	}

	initializer(&WDcfg);

	/* *************************************************************************************** */
	/* Open the digitizer and read the board information                                       */
	/* *************************************************************************************** */
	ErrCode = OpenDigitizers(&WDcfg);
	if (ErrCode != ERR_NONE) {
		goto QuitProgram;
	}

	PrintDigitizersInfo(MsgLog, &WDcfg);

Restart:
	/* *************************************************************************************** */
	/* Program the digitizer                                                                   */
	/* *************************************************************************************** */
	printf("*** Digitizers configuring...\n");
	ErrCode = ProgramDigitizers(&WDcfg);
	if (ErrCode != ERR_NONE) {
		goto QuitProgram;
	}

	WDrun.WavePlotMode = WDcfg.SyncEnable ? WPLOT_MODE_STD : WPLOT_MODE_1BD;
	if (WDcfg.enablePlot)
		WDrun.ContinuousPlot = 1;
	if (WDcfg.SaveLists || WDcfg.SaveRawData || WDcfg.SaveWaveforms)
		WDrun.ContinuousWrite = 1;

	// Set reference channel for each borad, it is the first channel enabled
	SetRefCh(&WDcfg);
	if (WDcfg.SyncEnable) {
		ErrCode = CheckRefCh(&WDcfg);
		if (ErrCode != ERR_NONE) {
			goto QuitProgram;
		}
	}
	ErrCode = CheckTOFStartCh(&WDcfg);
	if (ErrCode != ERR_NONE)
		goto QuitProgram;

	// Set plot mask
	ConfigureChannelsPlot(&WDrun, &WDcfg);

	printf("*** Allocating buffers...\n");
	// Allocate memory for the event data and readout buffer
	/* WARNING: This function must be called after the digitizer programming */
	ErrCode = AllocateReadoutBuffer(&WDcfg);
	if (ErrCode != ERR_NONE) {
		goto QuitProgram;
	}
	// Allocate memory for buffers to contain the events
	ErrCode = AllocateEventBuffer(&WDbuff);
	if (ErrCode != ERR_NONE) {
		goto QuitProgram;
	}

	// Allocate Traces for the plotter;
	ErrCode = AllocateTraces(&WDcfg);
	if (ErrCode != ERR_NONE) {
		goto QuitProgram;
	}

	// Allocate and initialize Histograms
	uint32_t AllocatedSize, TotAllocSize = 0;
	ErrCode = ERR_MALLOC; // change error code
	if (CreateHistograms(&AllocatedSize) < 0) goto QuitProgram;
	TotAllocSize += AllocatedSize;
	if (InitWaveProcess() < 0) goto QuitProgram;
	ResetHistograms();
	ErrCode = ERR_NONE; // restore error code

	msg_printf(MsgLog, "INFO: Ready.\n");
	printf("\n");
	
	// Batch mode: start acquisition automatically
	if (WDcfg.BatchMode > 0) {
		// Force SaveRunInfo in batch mode
		WDcfg.SaveRunInfo = 1;
		
		printf("========================================\n");
		printf("BATCH MODE ENABLED (Mode %d)\n", WDcfg.BatchMode);
		printf("========================================\n");
		if (WDcfg.BatchMaxEvents > 0)
			printf("  Maximum events: %llu\n", (unsigned long long)WDcfg.BatchMaxEvents);
		else
			printf("  Maximum events: UNLIMITED\n");
		if (WDcfg.BatchMaxTime > 0)
			printf("  Maximum time: %llu seconds\n", (unsigned long long)WDcfg.BatchMaxTime);
		else
			printf("  Maximum time: UNLIMITED\n");
		if (WDcfg.BatchMode == 2)
			printf("  Visualization: DISABLED\n");
		else
			printf("  Visualization: ENABLED\n");
		printf("  Output path: %s\n", WDcfg.DataFilePath);
		printf("========================================\n");
		printf("\n");
		
		WDrun.BatchStartTime = get_time();
		WDrun.BatchEventsTotal = 0;
		StartAcquisition(&WDcfg);
		WDrun.AcqRun = 1;
		printf("Acquisition started automatically (batch mode)\n");
		if (WDcfg.BatchMode == 2) {
			printf("Press 'q' or 's' to stop acquisition early\n");
		}
	}
	else if (WDrun.Restart && WDrun.AcqRun) {
		StartAcquisition(&WDcfg);
	}
	else {
		printf("[s] start/stop the acquisition, [q] quit, [?] help\n");
	}
	
	WDrun.Quit = 0;
	WDrun.Restart = 0;
	//PrevRateTime = get_time();
	/* *************************************************************************************** */
	/* Readout Loop                                                                            */
	/* *************************************************************************************** */
	while (!WDrun.Quit) {
		// Check for keyboard commands (key pressed)
		if (WDcfg.BatchMode == 2) {
			// In batch mode 2 (no visualization), only check for soft stop keys (q or s)
			if (kbhit()) {
				int c = getch();
				if (c == 'q' || c == 's') {
					printf("\n");
					printf("========================================\n");
					printf("BATCH MODE STOPPED BY USER\n");
					printf("========================================\n");
					WDrun.AcqRun = 0;
					StopAcquisition(&WDcfg);
					
					// Print final statistics and file information
					if (WDcfg.enableStats) {
						UpdateStatistics(get_time());
						PrintStatistics();
					}
					if (WDcfg.SaveRunInfo)
						SaveRunInfo(ConfigFileName);
					if (WDcfg.SaveHistograms)
						SaveAllHistograms();
					CloseOutputDataFiles();
					
					printf("\n");
					printf("Output files saved in: %s\n", WDcfg.DataFilePath);
					printf("========================================\n");
					
					WDrun.Quit = 1; // Exit the loop
					continue;
				}
			}
		}
		else if (WDcfg.BatchMode != 2) {
			// Normal keyboard command processing for mode 0 and 1
			if (CheckKeyboardCommands(&WDrun, &WDcfg) == 0) {
				SLEEP(40); //pause to see messages displayed
			}
		}
		
		// Check batch mode conditions (time and event limits)
		if (WDcfg.BatchMode > 0 && WDrun.AcqRun) {
			if (CheckBatchModeConditions(&WDrun, &WDcfg) == 0) {
				// Batch mode termination condition met - stop acquisition
				StopAcquisition(&WDcfg);
				
				// Print final statistics and file information
				printf("\n");
				printf("========================================\n");
				printf("BATCH MODE COMPLETED\n");
				printf("========================================\n");
				if (WDcfg.enableStats) {
					UpdateStatistics(get_time());
					PrintStatistics();
				}
				if (WDcfg.SaveRunInfo)
					SaveRunInfo(ConfigFileName);
				if (WDcfg.SaveHistograms)
					SaveAllHistograms();
				CloseOutputDataFiles();
				
				printf("\n");
				printf("Output files saved in: %s\n", WDcfg.DataFilePath);
				printf("========================================\n");
				
				WDrun.Quit = 1; // Exit the loop
				continue;
			}
		}
		
		if (WDrun.Restart) {
			// reload configurations from config file
			f_ini = fopen(ConfigFileName, "r");
			ParseConfigFile(f_ini, &WDcfg);
			fclose(f_ini);
			// exit from loop
			WDrun.Quit = 1;
			continue;
		}
		// get current time
		CurrentTime = get_time();
		if (WDrun.AcqRun == 0) {
			if (AcqRunStopFlag) {
				if (WDcfg.enableStats) {
					UpdateStatistics(CurrentTime);
					if (WDrun.StatsMode >= 0)
						PrintStatistics();
				}
				if (WDcfg.SaveRunInfo)
					SaveRunInfo(ConfigFileName);
				if (WDcfg.SaveHistograms)
					SaveAllHistograms();
				CloseOutputDataFiles();

				//download and throw away events from all digitizers
				DownloadAll();

				msg_printf(MsgLog, "INFO: Stop Acquisition at %s\n", WDstats.AcqStopTimeString);
				printf("\n");
				printf("[s] start/stop the acquisition, [q] quit, [?] help\n");
				AcqRunStopFlag = 0;
			}
			if (CurrentTime % 1000 == 0) {
				//Board Fail Status
				uint32_t d32 = 0;
				for (int b = 0; b < WDcfg.NumBoards; b++) {
					CAEN_DGTZ_ReadRegister(WDcfg.handles[b].handle, 0x8178, &d32);
					if ((d32 & 0xF) != 0) {
						printf("Error: Internal Communication Timeout occurred.\nPlease reset digitizer manually then restart the program\n");
						ErrCode = ERR_BOARD_TIMEOUT;
						goto QuitProgram;
					}
					if ((d32 & 0x10) != 0) {
						printf("Warning: A PLL lock loss occurred on board %d.\n", b);
					}
				}
			}
			AcqRunGoFlag = 0;
			continue;
		}
		AcqRunStopFlag = 1;
		if (!AcqRunGoFlag) {
			// Prepare output data files
			if (OpenOutputDataFiles() < 0) {
				ErrCode = ERR_OUTFILE_WRITE;
				goto QuitProgram;
			}
			
			// Print output file information in batch mode
			if (WDcfg.BatchMode > 0) {
				char fname[200];
				printf("\n");
				printf("Output files being created:\n");
				if (WDcfg.SaveRawData) {
					printf("  - Raw data file\n");
				}
				if (WDcfg.SaveTDCList) {
					printf("  - TDC list files\n");
				}
				if (WDcfg.SaveWaveforms) {
					printf("  - Waveform files\n");
				}
				if (WDcfg.SaveLists) {
					printf("  - List files\n");
				}
				if (WDcfg.SaveHistograms) {
					printf("  - Histogram files\n");
				}
				printf("  - Run info file\n");
				printf("All files in: %s\n", WDcfg.DataFilePath);
				printf("\n");
			}
			
			// Open the plotters (skip if batch mode without visualization)
			if (WDPlotVar == NULL && WDcfg.BatchMode != 2) {
				printf("*** Plotters initializing...\n");
				//waveforms plot
				WDPlotVar = OpenPlotter(WDcfg.GnuPlotPath, MAX_NUM_TRACES, WDcfg.GlobalRecordLength);
				WDrun.SetPlotOptions = 1;
				//histogram plot
				OpenPlotter2();
			}

			WDstats.StartTime = get_time();
			PrevLogTime = WDstats.StartTime;
			PrevStatTime = WDstats.StartTime;

			ResetEventBuffer();
			ResetHistograms();
			memset(PrevChTimeStamp, 0, sizeof(float) * MAX_CH * MAX_BD);

			if (WDcfg.BatchMode == 0)
				printf("Press [?] for help\n");
			msg_printf(MsgLog, "INFO: Starting Acquisition at %s\n", WDstats.AcqStartTimeString);
			AcqRunGoFlag = 1;
		}

		/* Send a software trigger to each board */
		if (WDrun.ContinuousTrigger) {
			SendSWtrigger(&WDcfg);
		}

		/* Read data from all boards */
		ErrCode = ReadData(&WDcfg);
		if (ErrCode != ERR_NONE) {
			goto QuitProgram;
		}

		/* Decode and add events into the buffer */
		/* Plot and save raw data for all unfiltered events */
		ErrCode = EventsDecoding(&WDcfg);
		if (ErrCode != ERR_NONE) {
			goto QuitProgram;
		}

		/* Processes events */
		/* Plot and save data of the filtered events */
		if (WDcfg.SyncEnable) {
			ProcessesSynchronizedEvents();
		}
		else {
			ProcessesUnsynchronizedEvents();
		}

		/* Update statistics and print them onto the screen (once every second) */
		ElapsedTime = CurrentTime - PrevLogTime; // in ms
		if (WDcfg.enableStats || WDcfg.BatchMode > 0) {
			if (ElapsedTime > 1000 && (WDrun.DoRefresh || WDrun.DoRefreshSingle || WDcfg.BatchMode > 0)) {
				if (ForceStatUpdate || ((CurrentTime - PrevStatTime) > WDcfg.StatUpdateTime)) {
					UpdateStatistics(CurrentTime);
					PrevStatTime = CurrentTime;
					ForceStatUpdate = 0;
				}
				if (WDrun.StatsMode < 0) {
					if (WDcfg.BatchMode != 2) { // Print throughput except in batch mode 2
						ComputeThroughput(&WDcfg, &WDrun, ElapsedTime);
					}
				}
				else if (WDcfg.BatchMode == 1 || WDcfg.BatchMode == 0) { // Print statistics in batch mode 1 and interactive mode 0
					PrintStatistics();
				}
				PrevLogTime = CurrentTime;
				WDrun.DoRefreshSingle = 0;
				print_warn_stats_off = 1;
			}
			else if (!WDrun.DoRefresh && print_warn_stats_off && WDcfg.BatchMode == 0) {
				printf("Statistics refresh is disabled; press 'f' to enable or 'o' for single shots!\n");
				print_warn_stats_off = 0;
			}
		}

		/* Plot histogram (skip in batch mode without visualization) */
		if (ElapsedTime > 1000 && WDrun.HistoPlotType != HPLOT_DISABLED && WDcfg.BatchMode != 2) {
			PlotSelectedHisto(WDrun.HistoPlotType, WDrun.Xunits);
		}

		if (WDrun.SingleWrite) {
			printf("Single Event saved to output files\n");
			WDrun.SingleWrite = 0;
		}
	}
	ErrCode = ERR_NONE;

QuitProgram:
	if (!WDrun.Restart) {
		printf("Closing...\n");
		SLEEP(500);
	}

	/* stop the acquisition */
	StopAcquisition(&WDcfg);

	/* close the plotter */
	ClosePlotter();
	WDPlotVar = NULL;

	/* close the output files */
	CloseOutputDataFiles();

	/* free the buffers and some cleanup */
	FreeEventBuffer(&WDbuff);
	FreeReadoutBuffer(&WDcfg);
	FreeTraces();
	DestroyHistograms();
	CloseWaveProcess();

	if (WDrun.Restart) {
		msg_printf(MsgLog, "INFO: Restart.\n");
		goto Restart;
	}

	/* close the devices */
	CloseDigitizers(&WDcfg);

	/* print a possible error */
	if (ErrCode) {
		printf("\n");
		msg_printf(MsgLog, "ERROR %d: %s\n", ErrCode, ErrMsg[ErrCode]);
#ifdef WIN32
		printf("\n");
		// printf("Press a key to quit\n");
		// _getch();
#endif
	}

	/* close log file */
	msg_printf(MsgLog, "INFO: End.\n");
	if (MsgLog != NULL)
		fclose(MsgLog);

	return 0;
}
