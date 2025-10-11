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

#include <stdio.h>
#include <CAENDigitizer.h>

#include "WaveDemo.h"

/* ###########################################################################
*  Functions
*  ########################################################################### */

static long get_time()
{

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

// --------------------------------------------------------------------------------------------------------- 
// Description: Reset all the counters, histograms, etc...
// Return: 0=OK, -1=error
// --------------------------------------------------------------------------------------------------------- 
int ResetStatistics()
{
	//if (WDrun.AcqRun)	StopAcquisition();
	memset(&WDstats, 0, sizeof(WDstats));
	if (WDrun.AcqRun) {
		//StartAcquisition();
		WDstats.StartTime = get_time();
	}
	return 0;
}


// --------------------------------------------------------------------------------------------------------- 
// Description: Calculate some statistcs (rates, etc...) in the WDstats struct
// Return: 0=OK, -1=error
// --------------------------------------------------------------------------------------------------------- 
int UpdateStatistics(uint64_t CurrentTime)
{
	int b, ch;
	// Calculate Real Time (i.e. total acquisition time from the start of run).
	// If possible, the real time is taken form the most recent time stamp (coming from any channel),
	// otherwise it is taken from the computer time (much less precise)
	if (WDstats.LatestProcTstampAll > WDstats.PrevProcTstampAll) {
		WDstats.AcqRealTime = (float)(WDstats.LatestProcTstampAll / 1e6f);	// Acquisition Real time from board in ms
		WDstats.RealTimeSource = REALTIME_FROM_BOARDS;
	}
	else {
		WDstats.AcqRealTime = (float)(CurrentTime - WDstats.StartTime);	// time from computer in ms
		WDstats.RealTimeSource = REALTIME_FROM_COMPUTER;
	}
	// Calculate the data throughput rate from the boards to the computer
	if (WDrun.IntegratedRates)
		WDstats.RxByte_rate = (float)(WDstats.RxByte_cnt) / (WDstats.AcqRealTime*1048.576f);
	else
		WDstats.RxByte_rate = (float)(WDstats.RxByte_cnt - WDstats.RxByte_pcnt) / ((float)(CurrentTime - WDstats.LastUpdateTime)*1048.576f);
	WDstats.RxByte_pcnt = WDstats.RxByte_cnt;
	WDstats.BlockRead_cnt = 0;
	WDstats.LastUpdateTime = CurrentTime;

	// calculate counts and rater for each channel
	for (b = 0; b < WDcfg.NumBoards; b++) {
		for (ch = 0; ch < WDcfg.handles[b].Nch; ch++) {
			if (WDcfg.boards[b].channels[ch].ChannelEnable) {
				WDstats.EvRead_rate[b][ch] = 0;
				WDstats.EvFilt_rate[b][ch] = 0;
				WDstats.EvOutput_rate[b][ch] = 0;
				if (WDrun.IntegratedRates && (WDstats.LatestReadTstamp[b][ch] > 0)) {
					float elapsed = (float)(WDstats.LatestReadTstamp[b][ch]) / 1e9f;   // elapsed time (in seconds) of read events
					WDstats.EvRead_rate[b][ch] = WDstats.EvRead_cnt[b][ch] / elapsed;  // events read from the board
					WDstats.EvFilt_rate[b][ch] = WDstats.EvFilt_cnt[b][ch] / elapsed;  // events that passed the SW filters (cuts and correlation)
				}
				else if (WDstats.LatestReadTstamp[b][ch] > WDstats.PrevReadTstamp[b][ch]) {
					float elapsed = (float)(WDstats.LatestReadTstamp[b][ch] - WDstats.PrevReadTstamp[b][ch]) / 1e9f;  // elapsed time (in seconds) of read events
					WDstats.EvRead_rate[b][ch] = (WDstats.EvRead_cnt[b][ch] - WDstats.EvRead_pcnt[b][ch]) / elapsed;  // events read from the board
					WDstats.EvFilt_rate[b][ch] = (WDstats.EvFilt_cnt[b][ch] - WDstats.EvFilt_pcnt[b][ch]) / elapsed;  // events that passed the SW filters (cuts and correlation)
				}

				WDstats.EvOutput_rate[b][ch] = WDstats.EvFilt_rate[b][ch] /*- WDstats.EvOvf_rate[b][ch] - WDstats.EvUncorrel_rate[b][ch]*/;

				// correct for temporary errors due to asynchronous readings
				if (WDstats.EvFilt_rate[b][ch] > WDstats.EvRead_rate[b][ch])   WDstats.EvFilt_rate[b][ch] = WDstats.EvRead_rate[b][ch];
				if (WDstats.EvOutput_rate[b][ch] < 0) WDstats.EvOutput_rate[b][ch] = 0;

				// Total number of pulses (triggers) at the board input. If WDstats.EvInput_cnt<0, then the board doesn't provide this trigger counter
				if (!WDrun.AcqRun) {
					WDstats.EvInput_rate[b][ch] = 0;
				}
				else if (WDstats.EvInput_cnt[b][ch] == 0xFFFFFFFFFFFFFFFF) {
					WDstats.EvInput_rate[b][ch] = -1;  // Not available
				}
				else {
					if ((WDstats.EvRead_rate[b][ch] == 0) || (WDstats.ICRUpdateTime[b][ch] == 0)) {
						WDstats.EvInput_rate[b][ch] = 0;
					}
					else if (WDrun.IntegratedRates) {
						WDstats.EvInput_rate[b][ch] = WDstats.EvInput_cnt[b][ch] / ((float)WDstats.ICRUpdateTime[b][ch] / 1e9f);
						WDstats.EvInput_pcnt[b][ch] = WDstats.EvInput_cnt[b][ch];
						WDstats.PrevICRUpdateTime[b][ch] = WDstats.ICRUpdateTime[b][ch];
					}
					else if (WDstats.ICRUpdateTime[b][ch] > WDstats.PrevICRUpdateTime[b][ch]) {
						WDstats.EvInput_rate[b][ch] = (WDstats.EvInput_cnt[b][ch] - WDstats.EvInput_pcnt[b][ch]) / ((float)(WDstats.ICRUpdateTime[b][ch] - WDstats.PrevICRUpdateTime[b][ch]) / 1000000000);
						WDstats.EvInput_pcnt[b][ch] = WDstats.EvInput_cnt[b][ch];
						WDstats.PrevICRUpdateTime[b][ch] = WDstats.ICRUpdateTime[b][ch];
					}
					else if (((float)WDstats.PrevICRUpdateTime[b][ch] / 1e6) < (WDstats.AcqRealTime - 5000)) {  // if there is no ICR update after 5 sec, assume ICR=Read Rate
						WDstats.EvInput_rate[b][ch] = WDstats.EvRead_rate[b][ch];
					}
				}

				//
				if ((WDstats.EvInput_rate[b][ch] != -1) && (WDstats.EvInput_rate[b][ch] < WDstats.EvRead_rate[b][ch]))  // the ICR is updated every N triggers (typ N=1024) so it may result affected by an error that makes it
					WDstats.EvInput_rate[b][ch] = WDstats.EvRead_rate[b][ch];  // smaller than the event trhoughput; in this case, it is forced equal to event throughput to avoid negative dead-time

				// Number of lost pulses (triggers). If WDstats.EvLost_cnt<0, then the board doesn't provide this trigger counter
				if (WDstats.EvLost_cnt[b][ch] < 0) {
					WDstats.EvLost_rate[b][ch] = -1;  // Not available
				}
				else {
					if (WDrun.IntegratedRates && (WDstats.LostTrgUpdateTime[b][ch] > 0)) {
						WDstats.EvLost_rate[b][ch] = WDstats.EvLost_cnt[b][ch] / ((float)WDstats.LostTrgUpdateTime[b][ch] / 1e9f);
						WDstats.EvLost_pcnt[b][ch] = WDstats.EvLost_cnt[b][ch];
						WDstats.PrevLostTrgUpdateTime[b][ch] = WDstats.LostTrgUpdateTime[b][ch];
					}
					else if (WDstats.LostTrgUpdateTime[b][ch] > WDstats.PrevLostTrgUpdateTime[b][ch]) {
						WDstats.EvLost_rate[b][ch] = (WDstats.EvLost_cnt[b][ch] - WDstats.EvLost_pcnt[b][ch]) / ((float)(WDstats.LostTrgUpdateTime[b][ch] - WDstats.PrevLostTrgUpdateTime[b][ch]) / 1e9f);
						WDstats.EvLost_pcnt[b][ch] = WDstats.EvLost_cnt[b][ch];
						WDstats.PrevLostTrgUpdateTime[b][ch] = WDstats.LostTrgUpdateTime[b][ch];
					}
					else {
						WDstats.EvLost_rate[b][ch] = 0;	// HACK: what to do here ???
					}
				}
				if (WDstats.EvLost_rate[b][ch] > WDstats.EvInput_rate[b][ch])
					WDstats.EvLost_rate[b][ch] = WDstats.EvInput_rate[b][ch];

				// Dead Time
				if ((WDstats.EvInput_rate[b][ch] > 0) && (WDstats.EvLost_rate[b][ch] >= 0)) {
					WDstats.DeadTime[b][ch] = 1 - (WDstats.EvInput_rate[b][ch] - WDstats.EvLost_rate[b][ch]) / WDstats.EvInput_rate[b][ch];
				}
				else {
					WDstats.DeadTime[b][ch] = 0;
				}
				if (WDstats.DeadTime[b][ch] < 0)	WDstats.DeadTime[b][ch] = 0;
				if (WDstats.DeadTime[b][ch] > 1) 	WDstats.DeadTime[b][ch] = 1;

				// Percent of Busy time in the board (memory full; during this time the board is not able to accept triggers)
				WDstats.BusyTime[b][ch] = 0;
				if (WDstats.LatestReadTstamp[b][ch] > WDstats.PrevReadTstamp[b][ch]) {
					float period = 0;
					if (WDstats.EvInput_rate[b][ch] > 0)
						period = 1e9f / WDstats.EvInput_rate[b][ch];  // in ns
					WDstats.BusyTime[b][ch] = ((float)WDstats.BusyTimeGap[b][ch] - period) / (WDstats.LatestReadTstamp[b][ch] - WDstats.PrevReadTstamp[b][ch]);
				}
				if (WDstats.BusyTime[b][ch] < 0) WDstats.BusyTime[b][ch] = 0;
				if (WDstats.BusyTime[b][ch] > 1) WDstats.BusyTime[b][ch] = 1;


				// matching ratio (out_rate / in_rate) of the filters applied by the software
				if (WDstats.EvProcessed_cnt[b][ch] > WDstats.EvProcessed_pcnt[b][ch])
					WDstats.MatchingRatio[b][ch] = (float)(WDstats.EvFilt_cnt[b][ch] - WDstats.EvFilt_pcnt[b][ch]) / (WDstats.EvProcessed_cnt[b][ch] - WDstats.EvProcessed_pcnt[b][ch]);
				else
					WDstats.MatchingRatio[b][ch] = 0;


				// save current counters to prev_counters
				WDstats.EvRead_dcnt[b][ch] = WDstats.EvRead_cnt[b][ch] - WDstats.EvRead_pcnt[b][ch];
				WDstats.EvRead_pcnt[b][ch] = WDstats.EvRead_cnt[b][ch];
				WDstats.EvFilt_pcnt[b][ch] = WDstats.EvFilt_cnt[b][ch];
				WDstats.EvProcessed_pcnt[b][ch] = WDstats.EvProcessed_cnt[b][ch];
				WDstats.PrevReadTstamp[b][ch] = WDstats.LatestReadTstamp[b][ch];
				WDstats.PrevProcTstamp[b][ch] = WDstats.LatestProcTstamp[b][ch];
				WDstats.BusyTimeGap[b][ch] = 0;
			}
		}
	}
	WDstats.PrevProcTstampAll = WDstats.LatestProcTstampAll;
	return 0;
}