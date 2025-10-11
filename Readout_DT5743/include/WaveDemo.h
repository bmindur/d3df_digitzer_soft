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
******************************************************************************/

#ifndef _WAVEDEMO_H_
#define _WAVEDEMO_H_

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <CAENDigitizer.h>

#ifdef WIN32
	#include <time.h>
	#include <sys/timeb.h>
	#include <conio.h>
	#include <process.h>
	#include <windows.h>
	#include <tchar.h>
	#include <strsafe.h>
	#include <direct.h>

	/* redefine POSIX 'deprecated' */
	#define getch _getch
	#define kbhit _kbhit
	#define stat _stat
	#define mkdir _mkdir

	#define		_PACKED_
	#define		_INLINE_

	#define SLEEP(x) Sleep(x)

#else // linux
	#include <sys/time.h>
	#include <sys/stat.h>
	#include <unistd.h>
	#include <stdint.h>   /* C99 compliant compilers: uint64_t */
	#include <ctype.h>    /* toupper() */
	#include <termios.h>
	
	#define scanf _scanf  // before calling the scanf function it is necessart to change termios settings

	#define		_PACKED_		__attribute__ ((packed, aligned(1)))
	#define		_INLINE_		__inline__ 

	#define SLEEP(x) usleep(x*1000)
#endif

#ifndef max
#define max(a,b) ((a) > (b) ? (a) : (b))
#endif
#ifndef min 
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifndef coerce
#define coerce(x,low,up) ((x < low) ? (low) : ((x > up) ? (up) : (x)))
#endif

#ifdef LINUX
#define DEFAULT_CONFIG_FILE  "/usr/local/etc/WaveDemoConfig.ini"
#define GNUPLOT_DEFAULT_PATH ""
#define DATA_FILE_PATH ""
#else
#define DEFAULT_CONFIG_FILE  "WaveDemoConfig.ini"  /* local directory */
#define GNUPLOT_DEFAULT_PATH ""
#define DATA_FILE_PATH ""
#endif

#define MAX_BD  4          /* max. number of boards */
#define MAX_GR  8          /* max. number of groups for board */
#define MAX_CH  16         /* max. number of channels for board */

#define MAX_NUM_EVENTS_BLT 1000 /* maximum number of events to read out in one Block Transfer (range from 1 to 1023) */

#define MIN_DAC_RAW_VALUE	-1.25
#define MAX_DAC_RAW_VALUE	+1.25

#define MAX_GW  1000       /* max. number of generic write commads */

#define DTRACE_TRIGGER	0x1
#define DTRACE_ENERGY	0x2
#define DTRACE_BASELINE	0x4
#define NUM_DTRACE	    3	// number of digital traces in the waveform plot 
#define NUM_ATRACE		4	// number of analog traces in the waveform plot 
#define MAX_NTRACES	(NUM_ATRACE + NUM_DTRACE + 1)	// max. number of traces in the waveform plot 

#define KEYSEL_CH      0
#define KEYSEL_BOARD   1
#define KEYSEL_TRACES  2

#define KEYDIGITADD_MAX 1

#define REALTIME_FROM_BOARDS		0
#define REALTIME_FROM_COMPUTER		1

#define EVT_BUF_SIZE		2000   // max num event in the circular buffer

#define SYNC_WIN		     100   // ns

#define EMAXNBITS		(1<<14)		// Max num of bits for the Charge histograms
#define TMAXNBITS		(1<<14)		// Max num of bits for the Time histograms 

//#define MAX_OUTPUT_FILE_SIZE (1099511627776) // 1 TB
#define MAX_OUTPUT_FILE_SIZE   (2147483648) // 2 GB

#define HISTO_FILE_FORMAT_1COL		0  // ascii 1 coloumn
#define HISTO_FILE_FORMAT_2COL		1  // ascii 1 coloumn
#define HISTO_FILE_FORMAT_ANSI42	2  // xml ANSI42

#define TAC_SPECTRUM_COMMON_START	0
#define TAC_SPECTRUM_INTERVALS		1

#define OUTFILE_BINARY				0
#define OUTFILE_ASCII				1

//****************************************************************************
// Run Modes
//****************************************************************************
// start on software command 
#define RUN_START_ON_SOFTWARE_COMMAND     0x0 
// start on S-IN level (logical high = run; logical low = stop)
#define RUN_START_ON_SIN_LEVEL            0x5
// start on first TRG-IN or Software Trigger 
#define RUN_START_ON_TRGIN_RISING_EDGE    0x6
// start on LVDS I/O level
#define RUN_START_ON_LVDS_IO              0x7


enum {
	WPLOT_MODE_1BD,		//plots only output waveforms of one board at a time without any processing
	WPLOT_MODE_1CH,		//plots analogue and digital waveforms of one channel at a time
	WPLOT_MODE_STD,		//plots analogue and digital waveforms of events

	WPLOT_MODE_DUMMY_LAST
} WPLOT_MODE;

enum {
	HPLOT_DISABLED,
	HPLOT_TIME,
	HPLOT_ENERGY,

	HPLOT_TYPE_DUMMY_LAST
} HPLOT_TYPE;

/* 
###########################################################################
 Typedefs
###########################################################################
*/

/*!
* \enum	ERROR_CODES
*
* \brief	Values that represent error codes.
*/

typedef enum ERROR_CODES {
	ERR_NONE = 0,
	ERR_CONF_FILE_NOT_FOUND,
	ERR_CONF,
	ERR_DGZ_OPEN,
	ERR_BOARD_INFO_READ,
	ERR_INVALID_BOARD_TYPE,
	ERR_DGZ_PROGRAM,
	ERR_MALLOC,
	ERR_BUFF_MALLOC,
	ERR_HISTO_MALLOC,
	ERR_RESTART,
	ERR_INTERRUPT,
	ERR_READOUT,
	ERR_EVENT_BUILD,
	ERR_UNHANDLED_BOARD,
	ERR_OUTFILE_WRITE,
	ERR_BUFFERS,
	ERR_BOARD_TIMEOUT,
	ERR_TBD,

	ERR_DUMMY_LAST
} ERROR_CODES_t;

typedef enum {
	SYSTEM_TRIGGER_SOFT,
	SYSTEM_TRIGGER_NORMAL,
	SYSTEM_TRIGGER_EXTERNAL,
	SYSTEM_TRIGGER_ADVANCED
} TriggerType_t;

typedef enum {
	START_SW_CONTROLLED,
	START_HW_CONTROLLED
} StartMode_t;

typedef enum {
	COMMONT_EXTERNAL_TRIGGER_TRGIN_TRGOUT,
	INDIVIDUAL_TRIGGER_SIN_TRGOUT,
	TRIGGER_ONE2ALL_EXTOR,
} SyncMode_t;

typedef struct {
	int board;
	int channel;
} ChannelUID_t;

//****************************************************************************
// Struct containing variables for the statistics
//****************************************************************************
typedef struct {
	// ----------------------------------
	// Counters and Rates
	// ----------------------------------
	// Each counter has two instances: total counter and previous value used to calculate the rate: Rate = (Counter - PrevCounter)/elapsed_time
	// after the rate calculation, PrevCounter is updated to the value of Counter
	uint64_t RxByte_cnt;						// Received Byte counter (data from board)
	uint64_t RxByte_pcnt;						// Previous value
	float    RxByte_rate;						// Data Throughput in KB/s (from boards or from files)
	uint64_t BlockRead_cnt;						// BlockRead counter

	uint64_t EvRead_cnt[MAX_BD][MAX_CH];		// Event Counter at board output (ECR); number of event read from the channel
	uint64_t EvRead_pcnt[MAX_BD][MAX_CH];
	uint64_t EvRead_dcnt[MAX_BD][MAX_CH];		// cnt - pcnt 
	float    EvRead_rate[MAX_BD][MAX_CH];

	uint64_t EvProcessed_cnt[MAX_BD][MAX_CH];	// Event Processed (extracted from the queues). It is almost the same as EvRead_cnt but delayed (it doesn't count the events still in the queues)
	uint64_t EvProcessed_pcnt[MAX_BD][MAX_CH];

	uint64_t EvInput_cnt[MAX_BD][MAX_CH];		// Event Counter at board input (ICR): number of event detected by the input discriminator
	uint64_t EvInput_pcnt[MAX_BD][MAX_CH];		// Note: ICR may differ from ICRB because of the inability of the discriminator to distinguish two close pulses
	float EvInput_rate[MAX_BD][MAX_CH];

	uint64_t EvFilt_cnt[MAX_BD][MAX_CH];		// Event Counter after the software filters (cuts & correlation; PUR is counted separately)
	uint64_t EvFilt_pcnt[MAX_BD][MAX_CH];
	float EvFilt_rate[MAX_BD][MAX_CH];

	uint64_t EvLost_cnt[MAX_BD][MAX_CH];		// Counter of the events lost (either in the HW or in the SW queues)
	uint64_t EvLost_pcnt[MAX_BD][MAX_CH];
	float EvLost_rate[MAX_BD][MAX_CH];

	float DeadTime[MAX_BD][MAX_CH];				// Dead Time (float number in the range 0 to 1)
	float MatchingRatio[MAX_BD][MAX_CH];		// Matching Ratio after the filters (cuts & correlation)
	float EvOutput_rate[MAX_BD][MAX_CH];		// OCR

	uint64_t BusyTimeGap[MAX_BD][MAX_CH];		// Sum of the DeadTime Gaps (saturation or busy); this is a real dead time (loss of triggers); it doesn't include dead time for pile-ups
	float BusyTime[MAX_BD][MAX_CH];				// Percent of BusyTimeGap 

	uint64_t TotEvRead_cnt;						// Total Event read from the boards (sum of all channels)
	uint64_t UnSyncEv_cnt;

	// Times
	uint64_t StartTime;							// Computer time at the start of the acquisition in ms
	uint64_t LastUpdateTime;					// Computer time at the last statistics update
	float AcqRealTime;							// Acquisition time (from the start) in ms
	float AcqStopTime;							// Acquisition Stop time (from the start) in ms
	int RealTimeSource;							// 0: real time from the time stamps; 1: real time from the computer
	char AcqStartTimeString[32];				// Start Time in the format %Y-%m-%d %H:%M:%S
	char AcqStopTimeString[32];					// Start Time in the format %Y-%m-%d %H:%M:%S

	uint64_t LatestProcTstampAll;					// Latest event time stamp (= acquisition time taken from the boards) in ns
	uint64_t PrevProcTstampAll;						// Previous event time stamp (used to calculate the elapsed time from the last update) in ns
	uint64_t LatestReadTstamp[MAX_BD][MAX_CH];		// Newest time stamp in ns at queue input
	uint64_t PrevReadTstamp[MAX_BD][MAX_CH];		// Previous value of LatestReadTstamp (used to calculate elapsed time)
	uint64_t LatestProcTstamp[MAX_BD][MAX_CH];		// Newest time stamp in ns at queue output
	uint64_t PrevProcTstamp[MAX_BD][MAX_CH];		// Previous value of LatestProcTstamp (used to calculate elapsed time)
	uint64_t ICRUpdateTime[MAX_BD][MAX_CH];			// Time stamp of the event containg ICR information (1K flag)
	uint64_t PrevICRUpdateTime[MAX_BD][MAX_CH];		// Previous value of ICRUpdateTime (used to calculate elapsed time for ICR)
	uint64_t LostTrgUpdateTime[MAX_BD][MAX_CH];		// Time stamp of the event containg Lost Trigger information (1K flag)
	uint64_t PrevLostTrgUpdateTime[MAX_BD][MAX_CH];	// Previous value of TrgLostUpdateTime (used to calculate elapsed time for LostTriggers)
} WaveDemoStats_t;

//****************************************************************************
// Waveform Data Structure
//****************************************************************************
typedef struct {
	int32_t Ns;							// Num of samples
	float *AnalogTrace[NUM_ATRACE];		// Analog traces (samples) 
	uint8_t *DigitalTraces; 			// Digital traces (1 bit = 1 trace)
} Waveform_t;

//****************************************************************************
// Histogram Data Structure (1 dimension)
//****************************************************************************
typedef struct {
	uint32_t *H_data;			// pointer to the histogram data
	uint32_t Nbin;				// Number of bins (channels) in the histogram
	uint32_t H_cnt;				// Number of entries (sum of the counts in each bin)
	uint32_t Ovf_cnt;			// Overflow counter
	uint32_t Unf_cnt;			// Underflow counter
	double rms;					// rms 
	double mean;				// mean
} Histogram1D_t;

//****************************************************************************
// Histogram Data Structure (2 dimensions)
//****************************************************************************
typedef struct {
	uint32_t *H_data;			// pointer to the histogram data
	uint32_t NbinX;				// Number of bins (channels) in the X axis
	uint32_t NbinY;				// Number of bins (channels) in the Y axis
	uint32_t H_cnt;				// Number of entries (sum of the counts in each bin)
	uint32_t Ovf_cnt;			// Overflow counter
	uint32_t Unf_cnt;			// Underflow counter
} Histogram2D_t;

//****************************************************************************
// Struct containing the histograms
//****************************************************************************
typedef struct {
	Histogram1D_t EH[MAX_BD][MAX_CH];		// Energy Histograms
	Histogram1D_t TH[MAX_BD][MAX_CH];	    // Time Histograms 
}  WaveDemoHistos_t;

typedef struct {
	float Baseline;				// Baseline (ADC counts)
	float FineTimeStamp;		// Fine time stamp (in ns)
	float Energy;				// Energy
	Waveform_t *Waveforms;		// Pointer to waveform data
} WaveDemo_EVENT_plus_t;

typedef struct {
	CAEN_DGTZ_EventInfo_t EventInfo;
	CAEN_DGTZ_X743_EVENT_t *Event;
	WaveDemo_EVENT_plus_t EventPlus[MAX_V1743_GROUP_SIZE][MAX_X743_CHANNELS_X_GROUP];
} WaveDemoEvent_t;

typedef struct {
	WaveDemoEvent_t *buffer[MAX_BD];
	int head[MAX_BD];
	int tail[MAX_BD];
	int tmp_pos[MAX_BD];
} WaveDemoBuffers_t;

typedef struct {
	int TriggerDelay;
} WaveDemoGroup_t;

typedef struct {
	char ChannelEnable;
	int EnablePulseChannels;
	unsigned short PulsePattern;
	float DCOffset_V;
	float m;
	float q;
	int ChannelTriggerEnable;
	float TriggerThreshold_V;
	float TriggerThreshold_adc;
	CAEN_DGTZ_TriggerPolarity_t TriggerPolarity;
	CAEN_DGTZ_PulsePolarity_t PulsePolarity;
	char PlotEnable;

	int DiscrMode;			// Discriminator Mode: 0=LED, 1=CFD
	int NsBaseline;			// Num of Samples for the input baseline calculation
	float GateWidth;		// Gate Width (in ns)
	float PreGate;			// Pre Gate (in ns)
	float CFDdelay;			// CFD delay (in ns)
	float CFDatten;			// CFD attenuation (between 0.0 and 1.0)
	int CFDThreshold;
	int TTFsmoothing;		// Smoothing factor in the trigger and timing filter (0 = disable)

	float EnergyCoarseGain;	// Energy Coarse Gain (requested by the user); can be a power of two (1, 2, 4, 8...) or a fraction (0.5, 0.25, 0.125...)
	float ECalibration_m;	// Energy Calibration slope (y=mx+q)
	float ECalibration_q;	// Energy Calibration offset (y=mx+q)
} WaveDemoChannel_t;

typedef struct {
	char Enable;
	int LinkType;
	int LinkNum;
	int ConetNode;
	uint32_t BaseAddress;
	char ipAddress[25];
	uint32_t RecordLength;
	TriggerType_t TriggerType;
	CAEN_DGTZ_TriggerMode_t ExtTrigger;
	CAEN_DGTZ_TriggerMode_t SwTrigger;
	CAEN_DGTZ_TriggerMode_t ChannelSelfTrigger;
	CAEN_DGTZ_SAMFrequency_t SamplingFrequency;
	CAEN_DGTZ_SAM_CORRECTION_LEVEL_t CorrectionLevel;
	CAEN_DGTZ_IOLevel_t FPIOtype;
	int GWn;
	uint32_t GWaddr[MAX_GW];
	uint32_t GWdata[MAX_GW];
	uint32_t GWmask[MAX_GW];
	WaveDemoGroup_t groups[MAX_GR];
	WaveDemoChannel_t channels[MAX_CH];
	int RefChannel; // reference channel
} WaveDemoBoard_t;

typedef struct {
	int handle;
	int Ngroup;
	int Nch;
	int Nbit;
	float Ts;
	CAEN_DGTZ_ErrorCode ret_open;
	CAEN_DGTZ_ErrorCode ret_last;
	CAEN_DGTZ_BoardInfo_t BoardInfo;
	char *buffer;
	uint32_t AllocatedSize, BufferSize, NumEvents;
	int Nb, Ne;
	WaveDemoEvent_t* RefEvent;
} WaveDemoBoardHandle_t;

typedef struct {
	char ChannelPlotEnable[MAX_CH];
	FILE *fwave[MAX_CH];
	FILE *flist[MAX_CH];
	FILE *ftdc[MAX_CH];
} WaveDemoBoardRun_t;

typedef struct {
	int NumBoards;  // Number of boards used in the data struct
	bool doReset;
	bool enableStats;
	bool enablePlot;
	char GnuPlotPath[1000];
	int StatUpdateTime;	// Update time in ms for the statistics (= averaging time) 
	int SyncEnable;
	SyncMode_t SyncMode;
	StartMode_t StartMode;

	WaveDemoBoard_t boards[MAX_BD];
	WaveDemoBoardHandle_t handles[MAX_BD];
	WaveDemoBoardRun_t runs[MAX_BD];

	int TOFstartChannel;		// Channel used as a start in the TOF measurements
	int TOFstartBoard;			// Board to which the TOFstartChannel belongs

								// Enable Output file saving
	int SaveRawData;		// Save raw data (events before selection)
	int SaveTDCList;		// Save TDC list (events before selection)
	int SaveHistograms;		// Save Histograms (Enabling Mask: bit 0 = Energy, bit 1 = Time)
	int SaveWaveforms;		// Save Waveforms (events after selection)
	int SaveLists;			// Save 3 column lists with timestamp, charge, psd. (events after selection)
	int SaveRunInfo;		// Save Run Info file with Run Description and a copy of the config file
							// Data and List Files information
	char DataFilePath[200];			// path to the folder where output data files are written
	int OutFileFormat;				// 0=BINARY or 1=ASCII (only for list and waveforms files; raw data files are always binary)
	int OutFileHeader;				// 0=NO or 1=YES
	int OutFileTimeStampUnit;		// 0=ps, 1=ns, 2=us, 3=ms, 4=s
	int HistoOutputFormat;			// 0=ASCII 1 column, 1= ASCII 2 column, 2=ANSI42
	int ConfirmFileOverwrite;		// ask before overwriting output data file
									// Run Number (used in output file names)
	int RunNumber;					// Run Number (can be a number or timestamp)
	bool isRunNumberTimestamp;

	int EHnbin;		// Number of bins in the E histograms
	int THnbin;		// Number of bins in the T histograms
	float THmin;	// lower time value used to make time histograms 
	float THmax;	// upper time value used to make time histograms 

	int WaveformProcessor; // 0=disabled, bit0 = timing interpolation, bit1 = energy, bit 2 = trigger jitter correction
	int GlobalRecordLength;

	// Time calibration
	int TspectrumMode;  // Timing spectrum (TAC): 0=start-stop, 1=intervals (time difference between consecutive events)

	int TriggerFix;
} WaveDemoConfig_t;

typedef struct {
	ChannelUID_t ch[MAX_BD * MAX_CH];
	int num;
	int index;
} ChannelEnabledList_t;

typedef struct {
	int Quit;
	int AcqRun; // Acquisition running
	char DataTimeFilename[32]; // String for files prefix
	int DoRefresh;
	int DoRefreshSingle;
	int StatsMode;
	int IntegratedRates; // 0=istantaneous rates; 1=integrated rates
	int Coincidences2Enable;
	int NumPlotEnable;
	ChannelEnabledList_t ChannelEnabled;
	int WavePlotMode;
	float *Traces[MAX_NTRACES];
	bool TraceEnable[MAX_NTRACES];
	int HistoPlotType;
	float PlotXscale;
	float PlotYmax;
	int Xunits;
	int ContinuousTrigger;
	int ContinuousWrite;
	int SingleWrite;
	int BrdToPlot;
	int ChToPlot;
	int ContinuousPlot;
	int SinglePlot;
	int SetPlotOptions;
	int LastKeyOptSel;
	int KeySelector;
	int KeyDigitAdd;
	int BoardSelected;
	int Restart;
	FILE *flist_merged;
	FILE *OutputDataFile;
} WaveDemoRun_t;

//****************************************************************************
// Global Variables
//****************************************************************************
extern WaveDemoConfig_t	 WDcfg;		// struct containing acquisition parameters and user settings
extern WaveDemoRun_t	 WDrun;		// struct containing variables for the operation of the program
extern WaveDemoStats_t	 WDstats;	// struct containing variables for the statistics
extern WaveDemoBuffers_t WDbuff;	// struct containing the buffers
extern WaveDemoHistos_t	 WDhistos;	// struct containing the histograms

extern FILE *MsgLog;

#endif /* _WAVEDEMO_H_ */
