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
* WDconfig contains the functions for reading the configuration file and
* setting the parameters in the WDcfg structure
******************************************************************************/


#include "WDconfig.h"
#include "ini.h"

/*! \brief	
	'common_deny' is used to avoid [COMMON] section after [BOARD ...] sections 
	'board_exceeded' is used as flag in [CONNECTIONS] section
*/
int common_deny = 0;
int board_exceeded = 0;

/* Strip whitespace chars off end of given string, in place. Return s. */
static char* rstrip(char* s) {
	char* p = s + strlen(s);
	while (p > s && isspace((unsigned char)(*--p)))
		*p = '\0';
	return s;
}

/* Return pointer to first non-whitespace char in given string. */
static char* lskip(const char* s) {
	while (*s && isspace((unsigned char)(*s)))
		s++;
	return (char*)s;
}

/*! \brief	function to parse values when options are YES (ENABLED or 1) or NO (DISABLED or 0)
 *	\return false = disabled, true = enabled
 */
static bool getBoolValue(const char *name, const char *value) {
	char str[10] = { 0 };
	bool ret = false;
	if (sscanf(value, "%s", str) == 1) {
		if (strcmp(str, "YES") == 0 || strcmp(str, "ENABLED") == 0 || strcmp(str, "1") == 0)
			return true;
		if (strcmp(str, "NO") == 0 || strcmp(str, "DISABLED") == 0 || strcmp(str, "0") == 0)
			return false;
	}
	printf("Option '%s' for setting %s is invalid! NO will be used by default.\n", value, name);
	return ret;
}

/*! \brief	function to parse number or text to number of bins (channels) of an histogram
*	\return integer (-1 if parse error occours)
*/
int GetHnbin(const char* value) {
	char str[100] = { 0 };
	int ret = -1;

	if (sscanf(value, "%s", str) == 1) {
		// string to uppercase
		for (int i = 0; i < (int)strlen(str); i++) {
			str[i] = (char)toupper(str[i]);
		}
		if      (streq(str, "1K"))  ret = 1024;
		else if (streq(str, "2K"))  ret = 2048;
		else if (streq(str, "4K"))  ret = 4096;
		else if (streq(str, "8K"))  ret = 8192;
		else if (streq(str, "16K")) ret = 16384;
		else if (streq(str, "32K")) ret = 32768;
		else if (sscanf(value, "%d", &ret) == 1) {
			// check if is not a power of 2
			if (!((ret != 0) && ((ret & (ret - 1)) == 0)))
				return -1;
		}
	}
	return ret;
}

/*! \brief	function to parse number
*	\return integer
*/
int GetIntValueDefault(const char* name, const char *value, int default_value) {
	int ret;
	if (sscanf(value, "%d", &ret) == 1) {
		return ret;
	}
	printf("Option '%s' for setting %s is invalid! %d will be used by default.\n", value, name, default_value);
	return default_value;
}
bool GetIntOption(const char* name, const char* value, int *ret) {
	if (sscanf(value, "%d", ret) == 1) {
		return true;
	}
	printf("Option '%s' for setting %s is not a valid int value!\n", value, name);
	return false;
}
/*! \brief	function to parse number
*	\return float
*/
float GetFloatValueDefault(const char* name, const char *value, float default_value) {
	float ret;
	if (sscanf(value, "%f", &ret) == 1) {
		return ret;
	}
	printf("Option '%s' for setting %s is invalid! %f will be used by default.\n", value, name, default_value);
	return default_value;
}
bool GetFloatOption(const char* name, const char* value, float* ret) {
	if (sscanf(value, "%f", ret) == 1) {
		return true;
	}
	printf("Option '%s' for setting %s is not a valid float value!\n", value, name);
	return false;
}

void NormalizeDataFilePath(char* path) {
	size_t len = strlen(path);
	if (len == 0) {
		strcpy(path, "./");
		return;
	}
	
	// Check if path already ends with a separator
	char lastChar = path[len - 1];
	if (lastChar != '/' && lastChar != '\\') {
		// Add appropriate separator for the platform
#ifdef WIN32
		strcat(path, "\\");
#else
		strcat(path, "/");
#endif
	}
}

void GetString(const char* value, char* ret, const char* default_value) {
	if (sscanf(value, "%s", ret) == 1) {
		return;
	}
	strcpy(ret, default_value);
	return;
}

/*!
 * \fn	void SetDefaultConfiguration(WaveDemoConfig_t *WDcfg)
 *
 * \brief	Fill the Configuration Structure with Default Values.
 *
 * \param [in,out]	WDcfg	Pointer to the WaveDumpConfig data structure.
 */
void SetDefaultConfiguration(WaveDemoConfig_t *WDcfg) {
	// fill general settings
	strcpy(WDcfg->GnuPlotPath, GNUPLOT_DEFAULT_PATH);
	strcpy(WDcfg->DataFilePath, DATA_FILE_PATH);
	NormalizeDataFilePath(WDcfg->DataFilePath);
	WDcfg->isRunNumberTimestamp = true;
	WDcfg->NumBoards = 0;
	WDcfg->doReset = 1;
	WDcfg->enableStats = 1;
	WDcfg->enablePlot = 1;
	WDcfg->StatUpdateTime = 1000;
	WDcfg->SyncEnable = 0;
	WDcfg->HistoOutputFormat = HISTO_FILE_FORMAT_1COL;
	WDcfg->OutFileTimeStampUnit = 1;
	WDcfg->EHnbin = EMAXNBITS;
	WDcfg->THnbin = TMAXNBITS;
	WDcfg->THmin = -50;
	WDcfg->THmax = 50;
	WDcfg->TspectrumMode = TAC_SPECTRUM_COMMON_START;
	WDcfg->WaveformProcessor = 0xF;
	WDcfg->GlobalRecordLength = 1024;
	WDcfg->TOFstartBoard = 0;
	WDcfg->TOFstartChannel = 0;
	WDcfg->TriggerFix = 20;

	// Batch mode defaults
	WDcfg->BatchMode = 0;           // 0 = interactive mode (default)
	WDcfg->BatchMaxEvents = 0;      // 0 = unlimited
	WDcfg->BatchMaxTime = 0;        // 0 = unlimited

	for (int b = 0; b < MAX_BD; b++) {
		// get pointer to substructure
		WaveDemoBoard_t *WDb = &WDcfg->boards[b];
		// assign defaults
		WDb->Enable = 0;
		WDb->RefChannel = 0;
		WDb->RecordLength = 1024;
		WDb->SamplingFrequency = CAEN_DGTZ_SAM_3_2GHz;
		WDb->CorrectionLevel = CAEN_DGTZ_SAM_CORRECTION_ALL;
		WDb->TriggerType = SYSTEM_TRIGGER_NORMAL;
		WDb->SwTrigger = CAEN_DGTZ_TRGMODE_ACQ_ONLY;
		WDb->ChannelSelfTrigger = CAEN_DGTZ_TRGMODE_ACQ_ONLY;
		WDb->ExtTrigger = CAEN_DGTZ_TRGMODE_ACQ_ONLY;
		WDb->FPIOtype = CAEN_DGTZ_IOLevel_NIM;

		WDb->GWn = 0;

		for (int g = 0; g < MAX_GR; g++) {
			// get pointer to substructure
			WaveDemoGroup_t *WDg = &WDb->groups[g];
			// assign defaults
			WDg->TriggerDelay = 1;

		}

		for (int c = 0; c < MAX_CH; c++) {
			// get pointer to substructure
			WaveDemoChannel_t *WDc = &WDb->channels[c];
			// assign defaults
			WDc->ChannelEnable = 1;
			WDc->EnablePulseChannels = 0;
			WDc->PulsePattern = 1;
			WDc->DCOffset_V = 0;
			WDc->m = 1;
			WDc->q = 0;
			WDc->ChannelTriggerEnable = 0;
			WDc->TriggerPolarity = CAEN_DGTZ_TriggerOnFallingEdge;
			WDc->TriggerThreshold_V = 0;
			WDc->CFDThreshold = -1;

			WDc->DiscrMode = 1;
			WDc->NsBaseline = 10;
			WDc->GateWidth = 0;
			WDc->PreGate = 0;

			WDc->CFDatten = 1.0;
			WDc->TTFsmoothing = 0;

			WDc->EnergyCoarseGain = 1 * 1024;
			WDc->ECalibration_m = 1.0;
			WDc->ECalibration_q = 0;
		}
	}
}

/*!
 *  \brief	parser for the [CONNECTIONS] section
 */
static int parseConnections(const char* name, const char* value, WaveDemoConfig_t* WDcfg) {
	WaveDemoBoard_t *WDb;
	unsigned int bd;
	char str1[1000] = { 0 };

	if (board_exceeded)
		return 0;

	if (sscanf(name, "OPEN %u", &bd) == 1) {
		if ((int) bd > WDcfg->NumBoards) {
			printf("OPEN %u: The board numbers must be in ascending order. Expected OPEN %d!\n", bd, WDcfg->NumBoards);
			return 0;
		}
		if (WDcfg->NumBoards == MAX_BD) {
			board_exceeded = 1;
			printf("The maximum number of boards supported has been exceeded\n");
			return 0;
		}
		if (bd < MAX_BD && !WDcfg->boards[bd].Enable) {
			WDb = &WDcfg->boards[bd];
			WDb->Enable = 1;
			if (sscanf(value, "%s", &str1) == 1) {
				if (strcmp(str1, "USB") == 0)
					WDb->LinkType = CAEN_DGTZ_USB;
				else if (strcmp(str1, "PCI") == 0)
					WDb->LinkType = CAEN_DGTZ_OpticalLink;
				else if (strcmp(str1, "USB_A4818") == 0)
					WDb->LinkType = CAEN_DGTZ_USB_A4818;
				else if (strcmp(str1, "USB_A4818_V2718") == 0)
					WDb->LinkType = CAEN_DGTZ_USB_A4818_V2718;
				else if (strcmp(str1, "USB_A4818_V3718") == 0)
					WDb->LinkType = CAEN_DGTZ_USB_A4818_V3718;
				else if (strcmp(str1, "USB_A4818_V4718") == 0)
					WDb->LinkType = CAEN_DGTZ_USB_A4818_V4718;
				else if (strcmp(str1, "USB_V4718") == 0)
					WDb->LinkType = CAEN_DGTZ_USB_V4718;
				else if (strcmp(str1, "ETH_V4718") == 0)
					WDb->LinkType = CAEN_DGTZ_ETH_V4718;
				else {
					printf("%s: Invalid connection type\n", str1);
					return 0;
				}
			}
			if (WDb->LinkType == CAEN_DGTZ_ETH_V4718) {
				if (sscanf(value, "%*s %s", &WDb->ipAddress) != 1) return 0;
			}
			else
				if (sscanf(value, "%*s %d", &WDb->LinkNum) != 1) return 0;
			if (WDb->LinkType == CAEN_DGTZ_USB) {
				WDb->ConetNode = 0;
				if (sscanf(value, "%*s %*d %x", &WDb->BaseAddress) != 1) return 0;
			}
			else {
				if (sscanf(value, "%*s %*d %d %x", &WDb->ConetNode, &WDb->BaseAddress) != 2) return 0;
			}
			WDcfg->NumBoards++;
		}
		else {
			return 0;
		}
	}
	return 1;
}

static int parseOptions(const char* name, const char* value, WaveDemoConfig_t* WDcfg) {
	char str[128] = { 0 };
	int val;

	// GNUplot path
	if (strcmp(name, "GNUPLOT_PATH") == 0) 
		GetString(value, WDcfg->GnuPlotPath, "./");
	// Data File path
	if (strcmp(name, "DATAFILE_PATH") == 0) {
		GetString(value, WDcfg->DataFilePath, "./");
		NormalizeDataFilePath(WDcfg->DataFilePath);
	}

	// Types file save
	if (strcmp(name, "SAVE_RAW_DATA") == 0)
		WDcfg->SaveRawData = getBoolValue(name, value);
	if (strcmp(name, "SAVE_TDC_LIST") == 0)
		WDcfg->SaveTDCList = getBoolValue(name, value);
	if (strcmp(name, "SAVE_WAVEFORM") == 0)
		WDcfg->SaveWaveforms = getBoolValue(name, value);
	if (strcmp(name, "SAVE_ENERGY_HISTOGRAM") == 0)
		WDcfg->SaveHistograms = getBoolValue(name, value) ? WDcfg->SaveHistograms | (1 << 0) : WDcfg->SaveHistograms & ~(1 << 0);
	if (strcmp(name, "SAVE_TIME_HISTOGRAM") == 0)
		WDcfg->SaveHistograms = getBoolValue(name, value) ? WDcfg->SaveHistograms | (1 << 1) : WDcfg->SaveHistograms & ~(1 << 1);
	if (strcmp(name, "SAVE_LISTS") == 0)
		WDcfg->SaveLists = getBoolValue(name, value);
	if (strcmp(name, "SAVE_RUN_INFO") == 0)
		WDcfg->SaveRunInfo = getBoolValue(name, value);

	// Output file format (BINARY or ASCII)
	if (strcmp(name, "OUTPUT_FILE_FORMAT") == 0) {
		GetString(value, str, "");
		if (strcmp(str, "BINARY") == 0)
			WDcfg->OutFileFormat = OUTFILE_BINARY;
		else if (strcmp(str, "ASCII") == 0)
			WDcfg->OutFileFormat = OUTFILE_ASCII;
		else {
			printf("%s: invalid output file format\n", value);
			return 0;
		}
	}
	// Header into output file (YES or NO)
	if (strcmp(name, "OUTPUT_FILE_HEADER") == 0) 
		WDcfg->OutFileHeader = getBoolValue(name, value) ? 1 : 0;
	// Unit for the time stamps in the output list files
	if (strcmp(name, "OUTPUT_FILE_TIMESTAMP_UNIT") == 0)
		WDcfg->OutFileTimeStampUnit = GetIntValueDefault(name, value, 1);

	// updating and printing statistics while acquisition
	if (strcmp(name, "STATS_RUN_ENABLE") == 0)
		WDcfg->enableStats = getBoolValue(name, value);
	// waveform plotting when the run starts
	if (strcmp(name, "PLOT_RUN_ENABLE") == 0)
		WDcfg->enablePlot = getBoolValue(name, value);
	
	// Reset before programming the board
	if (strcmp(name, "DGTZ_RESET") == 0)
		WDcfg->doReset = getBoolValue(name, value);

	// Synchronization
	if (strcmp(name, "SYNC_ENABLE") == 0)
		WDcfg->SyncEnable = getBoolValue(name, value) ? 1 : 0;

	// Trigger fixed parameters for trigger jitter correction
	if (strcmp(name, "TRIGGER_FIXED") == 0) {
		val = GetIntValueDefault(name, value, 20);
		val = coerce(val, 10, 90);
		WDcfg->TriggerFix = val;
	}

	// Board to which the CHANNEL_REF belongs
	if (strcmp(name, "BOARD_REF") == 0)
		WDcfg->TOFstartBoard = GetIntValueDefault(name, value, 0);
	// Channel used as a start in the TOF measurements
	if (strcmp(name, "CHANNEL_REF") == 0)
		WDcfg->TOFstartChannel = GetIntValueDefault(name, value, 0);

	// Histogram options

	// Num of channels in energy spectrum (256, 512, 1K, 2K, 4K, 8K, 16K, 32K)
	if (strcmp(name, "ENERGY_H_NBIN") == 0) {
		const int val = GetHnbin(value);
		if (val < 1024 || val > EMAXNBITS) {
			printf("%s: invalid setting for %s\n", value, name);
			return 0;
		}
		WDcfg->EHnbin = val;
	}

	// Num of channels in time spectrum (256, 512, 1K, 2K, 4K, 8K, 16K, 32K)
	if (strcmp(name, "TIME_H_NBIN") == 0) {
		const int val = GetHnbin(value);
		if (val < 256 || val > TMAXNBITS) {
			printf("%s: invalid setting for %s\n", value, name);
			return 0;
		}
		WDcfg->THnbin = val;
	}
	if (strcmp(name, "TIME_H_MIN") == 0)
		WDcfg->THmin = GetFloatValueDefault(name, value, -50);
	if (strcmp(name, "TIME_H_MAX") == 0)
		WDcfg->THmax = GetFloatValueDefault(name, value, 50);
	if (strcmp(name, "TIME_H_MODE") == 0) {
		GetString(value, str, "");
		if (streq(str, "START_STOP"))
			WDcfg->TspectrumMode = TAC_SPECTRUM_COMMON_START;
		else if (streq(str, "INTERVALS"))
			WDcfg->TspectrumMode = TAC_SPECTRUM_INTERVALS;
		else {
			printf("%s: invalid setting for %s\n", value, name);
			return 0;
		}
	}

	// Batch mode settings
	if (strcmp(name, "BATCH_MODE") == 0) {
		val = GetIntValueDefault(name, value, 0);
		if (val < 0 || val > 2) {
			printf("%s: invalid setting for %s (valid values: 0=interactive, 1=batch with visualization, 2=batch without visualization)\n", value, name);
			return 0;
		}
		WDcfg->BatchMode = val;
	}
	if (strcmp(name, "BATCH_MAX_EVENTS") == 0) {
		WDcfg->BatchMaxEvents = (uint64_t)GetIntValueDefault(name, value, 0);
	}
	if (strcmp(name, "BATCH_MAX_TIME") == 0) {
		WDcfg->BatchMaxTime = (uint64_t)GetIntValueDefault(name, value, 0);
	}

	return 1;
}

static int parseBoardSettings(const char* name, const char* value, WaveDemoConfig_t* WDcfg, int bd) {
	int val;
	char str[1000] = { 0 };

	// Acquisition Record Length (number of samples)
	if (strcmp(name, "RECORD_LENGTH") == 0) {
		if (!GetIntOption(name, value, &val))
			return 0;
		if (val % 16 || val <= 0 || val > 1024) {
			printf("%d: invalid option for %s\n", val, name);
			return 0;
		}
		if (bd == -1) {
			WDcfg->GlobalRecordLength = val;
			for (int i = 0; i < MAX_BD; i++)
				WDcfg->boards[i].RecordLength = val;
		}
		else
			WDcfg->boards[bd].RecordLength = val;
	}
	// Acquisition Frequency
	if (strcmp(name, "SAMPLING_FREQUENCY") == 0) {
		if (!GetIntOption(name, value, &val))
			return 0;
		if (val < 0 || val > 3) {
			printf("%d: invalid option for %s\n", val, name);
			return 0;
		}
		if (bd == -1)
			for (int i = 0; i < MAX_BD; i++)
				for (int j = 0; j < MAX_GR; j++)
					WDcfg->boards[i].SamplingFrequency = val;
		else
			WDcfg->boards[bd].SamplingFrequency = val;
	}

	// INL Correction
	if (strcmp(name, "INL_CORRECTION_ENABLE") == 0) {
		if (getBoolValue(name, value))
			val = CAEN_DGTZ_SAM_CORRECTION_ALL;
		else
			val = CAEN_DGTZ_SAM_CORRECTION_PEDESTAL_ONLY;
		if (bd == -1)
			for (int i = 0; i < MAX_BD; i++)
				for (int j = 0; j < MAX_GR; j++)
					WDcfg->boards[i].CorrectionLevel = val;
		else
			WDcfg->boards[bd].CorrectionLevel = val;
	}

	// Front Panel LEMO I/O level (NIM, TTL)
	if (strcmp(name, "FPIO_LEVEL") == 0) {
		GetString(value, str, "");
		if (strcmp(str, "TTL") == 0)
			val = 1;
		else if (strcmp(str, "NIM") == 0)
			val = 0;
		else {
			printf("%s: invalid setting for %s\n", value, name);
			return 0;
		}
		if (bd == -1)
			for (int i = 0; i < MAX_BD; i++)
				for (int j = 0; j < MAX_GR; j++)
					WDcfg->boards[i].FPIOtype = val;
		else
			WDcfg->boards[bd].FPIOtype = val;
	}

	// Trigger Type
	if (strcmp(name, "TRIGGER_TYPE") == 0) {
		GetString(value, str, "");
		TriggerType_t trigg;
		if (strcmp(str, "SOFTWARE") == 0)
			trigg = SYSTEM_TRIGGER_SOFT;
		else if (strcmp(str, "NORMAL") == 0)
			trigg = SYSTEM_TRIGGER_NORMAL;
		else if (strcmp(str, "EXTERNAL") == 0)
			trigg = SYSTEM_TRIGGER_EXTERNAL;
		else if (strcmp(str, "ADVANCED") == 0)
			trigg = SYSTEM_TRIGGER_ADVANCED;
		else {
			printf("%s: invalid setting for %s\n", value, name);
			return 0;
		}
		if (bd == -1)
			for (int i = 0; i < MAX_BD; i++)
				for (int j = 0; j < MAX_GR; j++)
					WDcfg->boards[i].TriggerType = trigg;
		else
			WDcfg->boards[bd].TriggerType = trigg;
	}

	// External Trigger
	if (strcmp(name, "EXTERNAL_TRIGGER") == 0) {
		GetString(value, str, "");
		CAEN_DGTZ_TriggerMode_t mode;
		if (strcmp(str, "DISABLED") == 0)
			mode = CAEN_DGTZ_TRGMODE_DISABLED;
		else if (strcmp(str, "ACQUISITION_ONLY") == 0)
			mode = CAEN_DGTZ_TRGMODE_ACQ_ONLY;
		else if (strcmp(str, "ACQUISITION_AND_TRGOUT") == 0)
			mode = CAEN_DGTZ_TRGMODE_ACQ_AND_EXTOUT;
		else if (strcmp(str, "TRGOUT_ONLY") == 0)
			mode = CAEN_DGTZ_TRGMODE_EXTOUT_ONLY;
		else {
			printf("%s: invalid setting for %s\n", value, name);
			return 0;
		}
		if (bd == -1)
			for (int i = 0; i < MAX_BD; i++)
				for (int j = 0; j < MAX_GR; j++)
					WDcfg->boards[i].ExtTrigger = mode;
		else
			WDcfg->boards[bd].ExtTrigger = mode;
	}
	// Software Trigger
	if (strcmp(name, "SOFTWARE_TRIGGER") == 0) {
		GetString(value, str, "");
		CAEN_DGTZ_TriggerMode_t mode;
		if (strcmp(str, "DISABLED") == 0)
			mode = CAEN_DGTZ_TRGMODE_DISABLED;
		else if (strcmp(str, "ACQUISITION_ONLY") == 0)
			mode = CAEN_DGTZ_TRGMODE_ACQ_ONLY;
		else if (strcmp(str, "ACQUISITION_AND_TRGOUT") == 0)
			mode = CAEN_DGTZ_TRGMODE_ACQ_AND_EXTOUT;
		else if (strcmp(str, "TRGOUT_ONLY") == 0)
			mode = CAEN_DGTZ_TRGMODE_EXTOUT_ONLY;
		else {
			printf("%s: invalid setting for %s\n", value, name);
			return 0;
		}
		if (bd == -1)
			for (int i = 0; i < MAX_BD; i++)
				for (int j = 0; j < MAX_GR; j++)
					WDcfg->boards[i].SwTrigger = mode;
		else
			WDcfg->boards[bd].SwTrigger = mode;
	}
	// Channel Self Trigger
	if (strcmp(name, "CHANNEL_SELF_TRIGGER") == 0) {
		GetString(value, str, "");
		CAEN_DGTZ_TriggerMode_t mode;
		if (strcmp(str, "DISABLED") == 0)
			mode = CAEN_DGTZ_TRGMODE_DISABLED;
		else if (strcmp(str, "ACQUISITION_ONLY") == 0)
			mode = CAEN_DGTZ_TRGMODE_ACQ_ONLY;
		else if (strcmp(str, "ACQUISITION_AND_TRGOUT") == 0)
			mode = CAEN_DGTZ_TRGMODE_ACQ_AND_EXTOUT;
		else if (strcmp(str, "TRGOUT_ONLY") == 0)
			mode = CAEN_DGTZ_TRGMODE_EXTOUT_ONLY;
		else {
			printf("%s: invalid setting for %s\n", value, name);
			return 0;
		}
		if (bd == -1)
			for (int i = 0; i < MAX_BD; i++)
				for (int j = 0; j < MAX_GR; j++)
					WDcfg->boards[i].ChannelSelfTrigger = mode;
		else
			WDcfg->boards[bd].ChannelSelfTrigger = mode;
	}

	// Generic Register Write (address offset + data + mask, each exadecimal)
	if (strcmp(name, "WRITE_REGISTER") == 0) {
		uint32_t addroffset, data, mask;
		int nret = sscanf(value, "%x %x %x", &addroffset, &mask, &data);
		if (nret != 3) {
			printf("%s: invalid value for ADDRESS MASK DATA\n", value);
			return 0;
		}
		if (bd == -1) {
			for (int i = 0; i < MAX_BD; i++) {
				if (WDcfg->boards[i].GWn < MAX_GW) {
					WDcfg->boards[i].GWaddr[WDcfg->boards[i].GWn] = addroffset;
					WDcfg->boards[i].GWdata[WDcfg->boards[i].GWn] = data;
					WDcfg->boards[i].GWmask[WDcfg->boards[i].GWn] = mask;
					WDcfg->boards[i].GWn++;
				}
			}
		}
		else {
			if (WDcfg->boards[bd].GWn < MAX_GW) {
				WDcfg->boards[bd].GWaddr[WDcfg->boards[bd].GWn] = addroffset;
				WDcfg->boards[bd].GWdata[WDcfg->boards[bd].GWn] = data;
				WDcfg->boards[bd].GWmask[WDcfg->boards[bd].GWn] = mask;
				WDcfg->boards[bd].GWn++;
			}
		}
	}

	return 1;
}
static int parseGroupSettings(const char* name, const char* value, WaveDemoConfig_t* WDcfg, int bd, int gr) {
	int val;

	// Post Trigger (periods of the SAMLONG chips write clock)
	if (strcmp(name, "POST_TRIGGER") == 0) {
		if (!GetIntOption(name, value, &val))
			return 0;
		if (val < 1 || val > 255) {
			printf("%d: invalid option for %s\n", val, name);
			return 0;
		}
		if (bd == -1)
			for (int i = 0; i < MAX_BD; i++)
				for (int j = 0; j < MAX_GR; j++)
					WDcfg->boards[i].groups[j].TriggerDelay = val;
		else if (gr == -1)
			for (int i = 0; i < MAX_GR; i++)
				WDcfg->boards[bd].groups[i].TriggerDelay = val;
		else
			WDcfg->boards[bd].groups[gr].TriggerDelay = val;
	}
	return 1;
}

static int parseChannelSettings(const char* name, const char* value, WaveDemoConfig_t* WDcfg, int bd, int ch) {
	int val;
	float valF;
	char str[128] = { 0 };

	// Channel Enable input
	if (strcmp(name, "INPUT_ENABLE") == 0) {
		if (getBoolValue(name, value))
			val = 1;
		else
			val = 0;

		if (bd == -1)
			for (int i = 0; i < MAX_BD; i++)
				for (int j = 0; j < MAX_CH; j++)
					WDcfg->boards[i].channels[j].ChannelEnable = (char)val;
		else if (ch == -1)
			for (int i = 0; i < MAX_CH; i++)
				WDcfg->boards[bd].channels[i].ChannelEnable = (char)val;
		else
			WDcfg->boards[bd].channels[ch].ChannelEnable = (char)val;
	}

	// Pulse polarity
	if (strcmp(name, "PULSE_POLARITY") == 0) {
		GetString(value, str, "");
		if (strcmp(str, "POSITIVE") == 0)
			val = CAEN_DGTZ_PulsePolarityPositive;
		else if (strcmp(str, "NEGATIVE") == 0)
			val = CAEN_DGTZ_PulsePolarityNegative;
		else {
			printf("%s: invalid setting for %s\n", value, name);
			return 0;
		}

		if (bd == -1) {
			for (int i = 0; i < MAX_BD; i++)
				for (int j = 0; j < MAX_CH; j++)
					WDcfg->boards[i].channels[j].PulsePolarity = val;
		}
		else if (ch == -1)
			for (int i = 0; i < MAX_CH; i++)
				WDcfg->boards[bd].channels[i].PulsePolarity = val;
		else
			WDcfg->boards[bd].channels[ch].PulsePolarity = val;
	}

	// DC offset
	if (strcmp(name, "DC_OFFSET") == 0) {
		if (!GetFloatOption(name, value, &valF))
			return 0;
		if (valF < -1.25 || valF > +1.25) {
			printf("%.2f: invalid option for %s\n", valF, name);
			return 0;
		}

		//int val_reg = (int)((val + 50) * 65535 / 100);
		if (bd == -1) {
			for (int i = 0; i < MAX_BD; i++)
				for (int j = 0; j < MAX_CH; j++)
					WDcfg->boards[i].channels[j].DCOffset_V = valF;
		}
		else if (ch == -1)
			for (int i = 0; i < MAX_CH; i++)
				WDcfg->boards[bd].channels[i].DCOffset_V = valF;
		else
			WDcfg->boards[bd].channels[ch].DCOffset_V = valF;
	}
	// Channel Auto trigger
	if (strcmp(name, "CHANNEL_TRIGGER_ENABLE") == 0) {
		if (getBoolValue(name, value))
			val = 1;
		else
			val = 0;

		if (bd == -1) {
			for (int i = 0; i < MAX_BD; i++)
				for (int j = 0; j < MAX_CH; j++)
					WDcfg->boards[i].channels[j].ChannelTriggerEnable = val;
		}
		else if (ch == -1)
			for (int i = 0; i < MAX_CH; i++)
				WDcfg->boards[bd].channels[i].ChannelTriggerEnable = val;
		else
			WDcfg->boards[bd].channels[ch].ChannelTriggerEnable = val;
	}
	// Trigger Edge
	if (strcmp(name, "TRIGGER_EDGE") == 0) {
		GetString(value, str, "");
		if (strcmp(str, "FALLING") == 0)
			val = CAEN_DGTZ_TriggerOnFallingEdge;
		else if (strcmp(str, "RISING") == 0)
			val = CAEN_DGTZ_TriggerOnRisingEdge;
		else {
			printf("%s: invalid setting for %s\n", value, name);
			return 0;
		}

		if (bd == -1) {
			for (int i = 0; i < MAX_BD; i++)
				for (int j = 0; j < MAX_CH; j++)
					WDcfg->boards[i].channels[j].TriggerPolarity = val;
		}
		else if (ch == -1)
			for (int i = 0; i < MAX_CH; i++)
				WDcfg->boards[bd].channels[i].TriggerPolarity = val;
		else
			WDcfg->boards[bd].channels[ch].TriggerPolarity = val;
	}
	// Trigger Threshold
	if (strcmp(name, "TRIGGER_THRESHOLD") == 0) {
		if (!GetFloatOption(name, value, &valF))
			return 0;
		if (valF < -1.25 || valF > +1.25) {
			printf("%.2f: invalid option for %s\n", valF, name);
			return 0;
		}

		if (bd == -1)
			for (int i = 0; i < MAX_BD; i++)
				for (int j = 0; j < MAX_CH; j++)
					WDcfg->boards[i].channels[j].TriggerThreshold_V = valF;
		else if (ch == -1)
			for (int i = 0; i < MAX_CH; i++)
				WDcfg->boards[bd].channels[i].TriggerThreshold_V = valF;
		else {
			WDcfg->boards[bd].channels[ch].TriggerThreshold_V = valF;
		}
	}
	// Channel pulse enable
	if (strcmp(name, "PULSE_ENABLE") == 0) {
		if (getBoolValue(name, value))
			val = 1;
		else
			val = 0;

		if (bd == -1)
			for (int i = 0; i < MAX_BD; i++)
				for (int j = 0; j < MAX_CH; j++)
					WDcfg->boards[i].channels[j].EnablePulseChannels = (char)val;
		else if (ch == -1)
			for (int i = 0; i < MAX_CH; i++)
				WDcfg->boards[bd].channels[i].EnablePulseChannels = (char)val;
		else
			WDcfg->boards[bd].channels[ch].EnablePulseChannels = (char)val;
	}
	// Pulse Pattern
	if (strcmp(name, "PULSE_PATTERN") == 0) {
		if (sscanf(value, "%i", &val) != 1)
			return 0;
		if (bd == -1)
			for (int i = 0; i < MAX_BD; i++)
				for (int j = 0; j < MAX_CH; j++)
					WDcfg->boards[i].channels[j].PulsePattern = (unsigned short)val;
		else if (ch == -1)
			for (int i = 0; i < MAX_CH; i++)
				WDcfg->boards[bd].channels[i].PulsePattern = (unsigned short)val;
		else
			WDcfg->boards[bd].channels[ch].PulsePattern = (unsigned short)val;
	}
	// Channel Enable plot
	if (strcmp(name, "PLOT_ENABLE") == 0) {
		if (getBoolValue(name, value))
			val = 1;
		else
			val = 0;

		if (bd == -1) {
			printf("invalid setting in this section\n");
			return 0;
		}
		else if (ch == -1)
			for (int i = 0; i < MAX_CH; i++)
				WDcfg->boards[bd].channels[i].PlotEnable = (char)val;
		else
			WDcfg->boards[bd].channels[ch].PlotEnable = (char)val;
	}

	//DISCR_MODE
	if (strcmp(name, "DISCR_MODE") == 0) {
		GetString(value, str, "");
		if (strcmp(str, "LED") == 0)
			val = 0;
		else if (strcmp(str, "CFD") == 0)
			val = 1;
		else {
			printf("%s: invalid setting for %s\n", value, name);
			return 0;
		}
		if (bd == -1) {
			for (int i = 0; i < MAX_BD; i++)
				for (int j = 0; j < MAX_CH; j++)
					WDcfg->boards[i].channels[j].DiscrMode = val;
		}
		else if (ch == -1)
			for (int i = 0; i < MAX_CH; i++)
				WDcfg->boards[bd].channels[i].DiscrMode = val;
		else
			WDcfg->boards[bd].channels[ch].DiscrMode = val;
	}
	//GATE_WIDTH
	if (strcmp(name, "GATE_WIDTH") == 0) {
		if (!GetFloatOption(name, value, &valF))
			return 0;
		if (bd == -1)
			for (int i = 0; i < MAX_BD; i++)
				for (int j = 0; j < MAX_CH; j++)
					WDcfg->boards[i].channels[j].GateWidth = valF;
		else if (ch == -1)
			for (int i = 0; i < MAX_CH; i++)
				WDcfg->boards[bd].channels[i].GateWidth = valF;
		else
			WDcfg->boards[bd].channels[ch].GateWidth = valF;
	}
	//PRE_GATE
	if (strcmp(name, "PRE_GATE") == 0) {
		if (!GetFloatOption(name, value, &valF))
			return 0;
		if (bd == -1)
			for (int i = 0; i < MAX_BD; i++)
				for (int j = 0; j < MAX_CH; j++)
					WDcfg->boards[i].channels[j].PreGate = valF;
		else if (ch == -1)
			for (int i = 0; i < MAX_CH; i++)
				WDcfg->boards[bd].channels[i].PreGate = valF;
		else
			WDcfg->boards[bd].channels[ch].PreGate = valF;
	}

	//CFD_DELAY
	if (strcmp(name, "CFD_DELAY") == 0) {
		if (!GetFloatOption(name, value, &valF))
			return 0;
		if (bd == -1)
			for (int i = 0; i < MAX_BD; i++)
				for (int j = 0; j < MAX_CH; j++)
					WDcfg->boards[i].channels[j].CFDdelay = valF;
		else if (ch == -1)
			for (int i = 0; i < MAX_CH; i++)
				WDcfg->boards[bd].channels[i].CFDdelay = valF;
		else
			WDcfg->boards[bd].channels[ch].CFDdelay = valF;
	}
	//CFD_ATTEN
	if (strcmp(name, "CFD_ATTEN") == 0) {
		if (!GetFloatOption(name, value, &valF))
			return 0;
		if (valF < 0.0 || valF > 1.0) {
			printf("%f: invalid option for %s\n", valF, name);
			return 0;
		}
		if (bd == -1)
			for (int i = 0; i < MAX_BD; i++)
				for (int j = 0; j < MAX_CH; j++)
					WDcfg->boards[i].channels[j].CFDatten = valF;
		else if (ch == -1)
			for (int i = 0; i < MAX_CH; i++)
				WDcfg->boards[bd].channels[i].CFDatten = valF;
		else
			WDcfg->boards[bd].channels[ch].CFDatten = valF;
	}

	//NS_BASELINE
	if (strcmp(name, "NS_BASELINE") == 0) {
		if (!GetIntOption(name, value, &val))
			return 0;

		if (bd == -1)
			for (int i = 0; i < MAX_BD; i++)
				for (int j = 0; j < MAX_CH; j++)
					WDcfg->boards[i].channels[j].NsBaseline = val;
		else if (ch == -1)
			for (int i = 0; i < MAX_CH; i++)
				WDcfg->boards[bd].channels[i].NsBaseline = val;
		else
			WDcfg->boards[bd].channels[ch].NsBaseline = val;
	}
	//TTF_SMOOTHING
	if (strcmp(name, "TTF_SMOOTHING") == 0) {
		if (!GetIntOption(name, value, &val))
			return 0;
		if (val < 0 || val > 4) {
			printf("%d: invalid option for %s\n", val, name);
			return 0;
		}
		if (bd == -1)
			for (int i = 0; i < MAX_BD; i++)
				for (int j = 0; j < MAX_CH; j++)
					WDcfg->boards[i].channels[j].TTFsmoothing = val;
		else if (ch == -1)
			for (int i = 0; i < MAX_CH; i++)
				WDcfg->boards[bd].channels[i].TTFsmoothing = val;
		else
			WDcfg->boards[bd].channels[ch].TTFsmoothing = val;
	}

	return 1;
}

static int handler(void* user, const char* section, const char* name, const char* value) {
	WaveDemoConfig_t* WDcfg = (WaveDemoConfig_t*)user;
	unsigned int bd, gr, ch;
	int ret = 1;

	if (strcmp(section, "CONNECTIONS") == 0) {
		return parseConnections(name, value, WDcfg);
	}
	else if (strcmp(section, "OPTIONS") == 0) {
		return parseOptions(name, value, WDcfg);
	}
	else if (strcmp(section, "COMMON") == 0) {
		if (common_deny) {
			printf("The [COMMON] section must be before [BOARD...]\n");
			return 0;
		}
		ret &= parseBoardSettings(name, value, WDcfg, -1);
		ret &= parseGroupSettings(name, value, WDcfg, -1, -1);
		ret &= parseChannelSettings(name, value, WDcfg, -1, -1);
	}
	else if (sscanf(section, "BOARD %u", &bd) == 1) {
		common_deny = 1;
		if (sscanf(section, "BOARD %u - GROUP %u", &bd, &gr) == 2) {
			if (bd >= MAX_BD) {
				printf("%u: Invalid board number\n", bd);
				return 0;
			}
			if (gr >= MAX_GR) {
				printf("%u: Invalid group number\n", gr);
				return 0;
			}
			ret &= parseGroupSettings(name, value, WDcfg, bd, gr);
		}
		else if (sscanf(section, "BOARD %u - CHANNEL %u", &bd, &ch) == 2) {
			if (bd >= MAX_BD) {
				printf("%u: Invalid board number\n", bd);
				return 0;
			}
			if (ch >= MAX_CH) {
				printf("%u: Invalid channel number\n", ch);
				return 0;
			}
			ret &= parseChannelSettings(name, value, WDcfg, bd, ch);
		}
		else {
			if (bd >= MAX_BD) {
				printf("%u: Invalid board number\n", bd);
				return 0;
			}
			ret &= parseBoardSettings(name, value, WDcfg, bd);
			ret &= parseGroupSettings(name, value, WDcfg, bd, -1);
			ret &= parseChannelSettings(name, value, WDcfg, bd, -1);
		}
	}
	else {
		return 0;  /* unknown section/name, error */
	}
	return ret;
}

/*!
 * \fn	int ParseConfigFile(FILE *f_ini, WaveDemoConfig_t *WDcfg)
 *
 * \brief	Read the configuration file and set the WaveDump paremeters.
 *
 * \param [in,out]	f_ini	Pointer to the config file.
 * \param [in,out]	WDcfg	Pointer to the WaveDumpConfig data structure.
 *
 * \return	0 = Success; negative numbers are error codes.
 */

int ParseConfigFile(FILE *f_ini, WaveDemoConfig_t *WDcfg) {
	int ret = 0;

	/* Default settings */
	SetDefaultConfiguration(WDcfg);

	/* read config file and assign parameters temporarily */
	common_deny = 0;
	ret = ini_parse_file(f_ini, handler, WDcfg);
	if (ret != 0) {
		printf("Error at line %d\n", ret);
		return -1;
	}

	return ret;
}
