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

#include "WDFiles.h"
#include "WDLogs.h"

uint64_t OutFileSize = 0; // Size of the output data file (in bytes)

#define OUTPUTFILE_TYPE_RAW				0
#define OUTPUTFILE_TYPE_LIST			1
#define OUTPUTFILE_TYPE_LIST_MERGED		2
#define OUTPUTFILE_TYPE_WAVE			3
#define OUTPUTFILE_TYPE_EHISTO			4
#define OUTPUTFILE_TYPE_THISTO			5
#define OUTPUTFILE_TYPE_RUN_INFO		6
#define OUTPUTFILE_TYPE_TDCLIST			7


/* Return pointer to first non-whitespace char in given string. */
static char* lskip(const char* s)
{
	while (*s && isspace((unsigned char)(*s)))
		s++;
	return (char*)s;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Ensure path ends with a path separator (/ or \)
// Return:		none
// --------------------------------------------------------------------------------------------------------- 
static void NormalizeOutputPath(char* path) {
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

// --------------------------------------------------------------------------------------------------------- 
// Description: create the folder for output files
// Return:		0=OK, -1=error
// --------------------------------------------------------------------------------------------------------- 
static int CreateOutputFolder() {
	struct stat st = { 0 };
	
	// Normalize the path to ensure it ends with a separator
	NormalizeOutputPath(WDcfg.DataFilePath);

	if (stat(WDcfg.DataFilePath, &st) == -1) {
		//return mkdir(WDcfg.DataFilePath, 700);
		return mkdir(WDcfg.DataFilePath);
	}

	return -1;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: create the file name for an output file
// Return:		0=OK, -1=error
// --------------------------------------------------------------------------------------------------------- 
static int CreateOutputFileName(int FileType, int b, int ch, char *fname) {
	char prefix[256], hext[10], wlext[10];
	if (WDcfg.isRunNumberTimestamp) {
		sprintf(prefix, "%s%s_", WDcfg.DataFilePath, WDrun.DataTimeFilename);
	}
	else {
		sprintf(prefix, "%s%03d_", WDcfg.DataFilePath, WDcfg.RunNumber);
	}

	if (WDcfg.HistoOutputFormat == HISTO_FILE_FORMAT_ANSI42) sprintf(hext, "n42");
	else sprintf(hext, "txt");
	if (WDcfg.OutFileFormat == OUTFILE_ASCII) sprintf(wlext, "txt");
	else sprintf(wlext, "dat");

	if (FileType == OUTPUTFILE_TYPE_RAW) {
		sprintf(fname, "%sraw.dat", prefix);
	} else if (FileType == OUTPUTFILE_TYPE_TDCLIST) {
		sprintf(fname, "%sTDC_%d_%d.%s", prefix, b, ch, wlext);
	} else if (FileType == OUTPUTFILE_TYPE_LIST) {
		sprintf(fname, "%sList_%d_%d.%s", prefix, b, ch, wlext);
	} else if (FileType == OUTPUTFILE_TYPE_LIST_MERGED) {
		sprintf(fname, "%sList_Merged.%s", prefix, wlext);
	} else if (FileType == OUTPUTFILE_TYPE_WAVE) {
		sprintf(fname, "%sWave_%d_%d.%s", prefix, b, ch, wlext);
	} else if (FileType == OUTPUTFILE_TYPE_EHISTO) {
		sprintf(fname, "%sEhisto_%d_%d.%s", prefix, b, ch, hext);
	} else if (FileType == OUTPUTFILE_TYPE_THISTO) {
		sprintf(fname, "%sThisto_%d_%d.%s", prefix, b, ch, hext);
	} else if (FileType == OUTPUTFILE_TYPE_EHISTO) {
		sprintf(fname, "%sPSDhisto_%d_%d.%s", prefix, b, ch, hext);
	} else if (FileType == OUTPUTFILE_TYPE_RUN_INFO) {
		sprintf(fname, "%srun_info.txt", prefix);
	} else {
		fname[0] = '\0';
		return -1;
	}
	return 0;
}


// --------------------------------------------------------------------------------------------------------- 
// Description: check if the output data files are already present
// Return:		0=OK, -1=error
// --------------------------------------------------------------------------------------------------------- 
int CheckOutputDataFilePresence() {
	int b, ch;
	char fname[300];
	FILE *of = NULL;

	// Raw data
	if (WDcfg.SaveRunInfo) {
		CreateOutputFileName(OUTPUTFILE_TYPE_RUN_INFO, 0, 0, fname);
		if ((of = fopen(fname, "r")) != NULL) return -1;
		fclose(of);
	}

	// Raw data
	if (WDcfg.SaveRawData) {
		CreateOutputFileName(OUTPUTFILE_TYPE_RAW, 0, 0, fname);
		WDrun.OutputDataFile = fopen(fname, "rb");
		if ((of = fopen(fname, "r")) != NULL) return -1;
		fclose(of);
	}

	// Merged list
	if (WDcfg.SaveLists & 0x2) {
		CreateOutputFileName(OUTPUTFILE_TYPE_LIST_MERGED, 0, 0, fname);
		WDrun.flist_merged = fopen(fname, "rb");
		if ((of = fopen(fname, "r")) != NULL) return -1;
		fclose(of);
	}

	// Histograms, Lists and Waveforms
	for (b = 0; b < WDcfg.NumBoards; b++) {
		for (ch = 0; ch < WDcfg.handles[b].Nch; ch++) {
			if (WDcfg.boards[b].channels[ch].ChannelEnable) {
				if (WDcfg.SaveHistograms & 0x1) {
					CreateOutputFileName(OUTPUTFILE_TYPE_EHISTO, b, ch, fname);
					//WDrun.OutputDataFile = fopen(fname, "rb");
					if ((of = fopen(fname, "r")) != NULL) return -1;
					fclose(of);
				}
				if (WDcfg.SaveHistograms & 0x2) {
					CreateOutputFileName(OUTPUTFILE_TYPE_THISTO, b, ch, fname);
					//WDrun.OutputDataFile = fopen(fname, "rb");
					if ((of = fopen(fname, "r")) != NULL) return -1;
					fclose(of);
				}
				if (WDcfg.SaveLists & 0x1) {
					CreateOutputFileName(OUTPUTFILE_TYPE_LIST, b, ch, fname);
					//WDrun.OutputDataFile = fopen(fname, "rb");
					if ((of = fopen(fname, "r")) != NULL) return -1;
					fclose(of);
				}
				if (WDcfg.SaveWaveforms) {
					CreateOutputFileName(OUTPUTFILE_TYPE_WAVE, b, ch, fname);
					//WDrun.OutputDataFile = fopen(fname, "rb");
					if ((of = fopen(fname, "r")) != NULL) return -1;
					fclose(of);
				}
			}
		}
	}
	return 0;
}


// --------------------------------------------------------------------------------------------------------- 
// Description: prepare output data files (file are actually opened when used for the 1st time)
// Return:		0=OK, -1=error
// --------------------------------------------------------------------------------------------------------- 
int OpenOutputDataFiles() {
	char c;
	char fname[200];
	char FileFormat = DATA_FILE_FORMAT_VERSION;
	int b, ch;
	uint32_t header[8] = { 8 };

	CreateOutputFolder();

	if (!WDcfg.isRunNumberTimestamp) {
		if (WDcfg.ConfirmFileOverwrite && (CheckOutputDataFilePresence() < 0)) {
			msg_printf(MsgLog, "\n\nWARINING: Output files for run %d already present in %s\n", WDcfg.RunNumber, WDcfg.DataFilePath);
			printf("Set ConfirmFileOverwrite=0 to prevent asking again\n\n");
			printf("Press 'q' to quit, any other key to continue\n");
			c = getch();
			if (tolower(c) == 'q')
				return -1;
		}
	}

	WDrun.flist_merged = NULL;
	for (b = 0; b < MAX_BD; b++)
		for (ch = 0; ch < MAX_CH; ch++)
			WDcfg.runs[b].flist[ch] = NULL;

	if (WDcfg.SaveRawData) {
		CreateOutputFileName(OUTPUTFILE_TYPE_RAW, 0, 0, fname);
		WDrun.OutputDataFile = fopen(fname, "wb");
		if (WDrun.OutputDataFile == NULL) {
			msg_printf(MsgLog, "Can't open Output Data File %s\n", fname);
			return -1;
		}

		// Write data file format
		fprintf(WDrun.OutputDataFile, "WaveDemo Raw Output FileFormat 0x%X\r\n", FileFormat);
		fwrite(&FileFormat, 1, 1, WDrun.OutputDataFile);

		// write data file header (1st word = header size)
		header[0] = 8;
		header[1] = WDcfg.GlobalRecordLength; 
		header[2] = 0;
		header[3] = 0;
		header[4] = WDcfg.NumBoards;
		header[5] = 0;
		header[6] = 0;
		header[7] = 12;
		fwrite(header, sizeof(uint32_t), header[0], WDrun.OutputDataFile);
	}
	return 0;
}


// --------------------------------------------------------------------------------------------------------- 
// Description: close output data files 
// Return:		0=OK, -1=error
// --------------------------------------------------------------------------------------------------------- 
int CloseOutputDataFiles() {
	int b, ch;

	if (WDrun.OutputDataFile != NULL) {
		fclose(WDrun.OutputDataFile);
		WDrun.OutputDataFile = NULL;
	}
	if (WDrun.flist_merged != NULL) {
		fclose(WDrun.flist_merged);
		WDrun.flist_merged = NULL;
	}
	for (b = 0; b < WDcfg.NumBoards; b++) {
		for (ch = 0; ch < WDcfg.handles[b].Nch; ch++) {
			if (WDcfg.runs[b].flist[ch] != NULL) {
				fclose(WDcfg.runs[b].flist[ch]);
				WDcfg.runs[b].flist[ch] = NULL;
			}
			if (WDcfg.runs[b].ftdc[ch] != NULL) {
				fclose(WDcfg.runs[b].ftdc[ch]);
				WDcfg.runs[b].ftdc[ch] = NULL;
			}
			if (WDcfg.runs[b].fwave[ch] != NULL) {
				fclose(WDcfg.runs[b].fwave[ch]);
				WDcfg.runs[b].fwave[ch] = NULL;
			}
		}
	}
	return 0;

}

int ReadRawHeader(FILE* inputFile, char *txtHeader, char *FileFormat, uint32_t *header, size_t headerElements) {
	if (fgets(txtHeader, 80, inputFile) == NULL) {
		fprintf(stderr, "Error reading file text header\n");
		return -1;
	}

	// Read data file format
	if (fread(FileFormat, sizeof(char), 1, inputFile) != 1) {
		fprintf(stderr, "Error reading file format version\n");
		return -1;
	}
	// Read data file header
	if (fread(header, sizeof(uint32_t), headerElements, inputFile) != headerElements) {
		fprintf(stderr, "Error reading file header\n");
		return -1;
	}

	return 0;
}

void ReadEventInfo(FILE* inputFile, CAEN_DGTZ_EventInfo_t* eventInfo) {
	fread(&eventInfo->EventCounter, sizeof(eventInfo->EventCounter), 1, inputFile);
	fread(&eventInfo->TriggerTimeTag, sizeof(eventInfo->TriggerTimeTag), 1, inputFile);
}
void ReadEventX743(FILE* inputFile, CAEN_DGTZ_X743_EVENT_t* event, char channelsEnabled[MAX_CH]) {
	//read group present mask
	int groupPresent = 0;

	fread(&groupPresent, sizeof(groupPresent), 1, inputFile);
	//write channelsEnabled
	fread(channelsEnabled, sizeof(char), MAX_CH, inputFile);

	for (int g = 0; g < MAX_V1743_GROUP_SIZE; g++) {
		event->GrPresent[g] = (groupPresent >> g) & 1;
		if (event->GrPresent[g]) {
			fread(&event->DataGroup[g].EventId, sizeof(event->DataGroup[g].EventId), 1, inputFile);
			fread(&event->DataGroup[g].TDC, sizeof(event->DataGroup[g].TDC), 1, inputFile);
			fread(&event->DataGroup[g].StartIndexCell, sizeof(event->DataGroup[g].StartIndexCell), 1, inputFile);
			fread(&event->DataGroup[g].ChSize, sizeof(event->DataGroup[g].ChSize), 1, inputFile);

			for (int c = 0; c < MAX_X743_CHANNELS_X_GROUP; c++) {
				// channel number in the board
				int ch = g * MAX_X743_CHANNELS_X_GROUP + c;

				if (channelsEnabled[ch]) {
					fread(&event->DataGroup[g].TriggerCount[c], sizeof(event->DataGroup[g].TriggerCount[c]), 1, inputFile);
					fread(&event->DataGroup[g].TimeCount[c], sizeof(event->DataGroup[g].TimeCount[c]), 1, inputFile);

					if (event->DataGroup[g].ChSize > 0) {
						event->DataGroup[g].DataChannel[c] = malloc(event->DataGroup[g].ChSize * sizeof(float));
						if (event->DataGroup[g].DataChannel[c]) {
							for (unsigned int s = 0; s < event->DataGroup[g].ChSize; s++) {
								fread(&event->DataGroup[g].DataChannel[c][s], sizeof(event->DataGroup[g].DataChannel[c][s]), 1, inputFile);
							}
						}
					}
				}
			}
		}
	}
}

int ReadRawData(FILE* inputFile, WaveDemoEvent_t *eventPtr[MAX_BD], int printFlag) {
	if (inputFile == NULL) return -1;

	char FileFormat;
	uint32_t header[8] = { 0 };
	char txtHeader[80] = { 0 };
	if (ReadRawHeader(inputFile, txtHeader, &FileFormat, header, 8) != 0) {
		fclose(inputFile);
		return -1;
	}
	if (printFlag) {
		printf("***\n");
		printf("Text Header: %s", txtHeader);
		printf("File Format: %d\n", FileFormat);
		printf("Headers: %x %x %x %x %x %x %x %x\n", header[0], header[1], header[2], header[3], header[4], header[5], header[6], header[7]);
		printf("***\n");
	}

	int n_evnt = 0;
	while (!feof(inputFile)) {
		int bd;
		if (fread(&bd, sizeof(bd), 1, inputFile) != 1) {
			if (feof(inputFile)) {
				break;
			} else {
				perror("Error reading the file");
				return -1;
			}
		}

		if (bd >= MAX_BD) return -1;

		if (!eventPtr[bd]) {
			eventPtr[bd] = malloc(sizeof(WaveDemoEvent_t));
			if (!eventPtr[bd]) return -1;
			memset(eventPtr[bd], 0, sizeof(WaveDemoEvent_t));
			eventPtr[bd]->Event = calloc(1, sizeof(CAEN_DGTZ_X743_EVENT_t));
			if (!eventPtr[bd]->Event) return -1;
		}

		CAEN_DGTZ_EventInfo_t *eventInfo = &eventPtr[bd]->EventInfo;
		ReadEventInfo(inputFile, eventInfo);
		if (printFlag) {
			printf("EventCounter: %u\n", eventInfo->EventCounter);
			printf("TriggerTimeTag: %u\n", eventInfo->TriggerTimeTag);
			printf("*\n");
		}

		CAEN_DGTZ_X743_EVENT_t *eventX743 = eventPtr[bd]->Event;
		char channelsEnabled[MAX_CH] = { 0 };
		ReadEventX743(inputFile, eventX743, channelsEnabled);
		if (printFlag) {
			printf("groupPresent: ");
			for (int g = 0; g < MAX_V1743_GROUP_SIZE; g++) {
				printf("%d", eventX743->GrPresent[g]);
			}
			printf("b\n");

			for (int g = 0; g < MAX_V1743_GROUP_SIZE; g++) {
				if (eventX743->GrPresent[g]) {
					printf("Group: %d\n", g);
					printf("EventId: %d\n", eventX743->DataGroup[g].EventId);
					printf("TDC: %lld\n", eventX743->DataGroup[g].TDC);
					printf("StartIndexCell: %d\n", eventX743->DataGroup[g].StartIndexCell);

					for (int c = 0; c < MAX_X743_CHANNELS_X_GROUP; c++) {
						// channel number in the board
						int ch = g * MAX_X743_CHANNELS_X_GROUP + c;

						if (channelsEnabled[ch]) {
							printf("CH: %d\n", ch);
							printf("TriggerCount: %d\n", eventX743->DataGroup[g].TriggerCount[c]);
							printf("TimeCount: %d\n", eventX743->DataGroup[g].TimeCount[c]);
							printf("N. Samples: %d\n", eventX743->DataGroup[g].ChSize);
							for (unsigned int s = 0; s < eventX743->DataGroup[g].ChSize; s++) {
								printf("%f ", eventX743->DataGroup[g].DataChannel[c][s]);
								if (s > 5) {
									printf("...");
									break;
								}
							}
							printf("\n");
						}
					}
				}
			}
			printf("*\n");
		}

		n_evnt++;
	}

	if (printFlag) {
		printf("Number of events found: %d\n", n_evnt);
	}
	
	return 0;
}

size_t WriteEventInfo(FILE* outputFile, CAEN_DGTZ_EventInfo_t* eventInfo) {
	size_t size = 0;

	fwrite(&eventInfo->EventCounter, sizeof(eventInfo->EventCounter), 1, outputFile);
	size += sizeof(eventInfo->EventCounter);
	fwrite(&eventInfo->TriggerTimeTag, sizeof(eventInfo->TriggerTimeTag), 1, outputFile);
	size += sizeof(eventInfo->TriggerTimeTag);

	return size;
}

size_t WriteEventX743(FILE* outputFile, CAEN_DGTZ_X743_EVENT_t* event, const char channelsEnabled[MAX_CH]) {
	size_t size = 0;

	//write group present mask
	int groupPresent = 0;
	for (int g = 0; g < MAX_V1743_GROUP_SIZE; g++) {
		if (event->GrPresent[g])
			groupPresent |= 1 << g;
	}
	fwrite(&groupPresent, sizeof(groupPresent), 1, outputFile);
	size += sizeof(groupPresent);
	//write channelsEnabled
	fwrite(channelsEnabled, sizeof(char), MAX_CH, outputFile);
	size += sizeof(char) * MAX_CH;

	for (int g = 0; g < MAX_V1743_GROUP_SIZE; g++) {
		if (event->GrPresent[g]) {
			fwrite(&event->DataGroup[g].EventId, sizeof(event->DataGroup[g].EventId), 1, outputFile);
			fwrite(&event->DataGroup[g].TDC, sizeof(event->DataGroup[g].TDC), 1, outputFile);
			fwrite(&event->DataGroup[g].StartIndexCell, sizeof(event->DataGroup[g].StartIndexCell), 1, outputFile);
			fwrite(&event->DataGroup[g].ChSize, sizeof(event->DataGroup[g].ChSize), 1, outputFile);
			size += sizeof(event->DataGroup[g].EventId) + sizeof(event->DataGroup[g].TDC) + sizeof(event->DataGroup[g].StartIndexCell) + sizeof(event->DataGroup[g].ChSize);

			for (int c = 0; c < MAX_X743_CHANNELS_X_GROUP; c++) {
				// channel number in the board
				int ch = g * MAX_X743_CHANNELS_X_GROUP + c;

				if (channelsEnabled[ch]) {
					fwrite(&event->DataGroup[g].TriggerCount[c], sizeof(event->DataGroup[g].TriggerCount[c]), 1, outputFile);
					fwrite(&event->DataGroup[g].TimeCount[c], sizeof(event->DataGroup[g].TimeCount[c]), 1, outputFile);
					size += sizeof(event->DataGroup[g].TriggerCount[c]) + sizeof(event->DataGroup[g].TimeCount[c]);

					for (unsigned int s = 0; s < event->DataGroup[g].ChSize; s++) {
						fwrite(&event->DataGroup[g].DataChannel[c][s], sizeof(event->DataGroup[g].DataChannel[c][s]), 1, outputFile);
						size += sizeof(event->DataGroup[g].DataChannel[c][s]);
					}
				}
			}
		}
	}

	return size;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Save one event into the raw data file
// Inputs:		bd = board index
//				channelsEnabled = channels enables for the board
//				event = event data structure
// Return:		0=OK, -1=error
// --------------------------------------------------------------------------------------------------------- 
int SaveRawData(int bd, const char channelsEnabled[MAX_CH], WaveDemoEvent_t* event) {
	if (WDrun.OutputDataFile == NULL) return -1;

	//write board index
	fwrite(&bd, sizeof(bd), 1, WDrun.OutputDataFile);
	OutFileSize += sizeof(bd);

	OutFileSize += WriteEventInfo(WDrun.OutputDataFile, &event->EventInfo);
	OutFileSize += WriteEventX743(WDrun.OutputDataFile, event->Event, channelsEnabled);

	if (OutFileSize > MAX_OUTPUT_FILE_SIZE) {
		WDcfg.SaveRawData = 0;
		printf("Saving of raw data stopped\n");
	}

	return 0;
}

int SaveTDCList(int bd, int ch, WaveDemoEvent_t *event) {
	char fname[100];
	WaveDemoBoard_t *WDb = &WDcfg.boards[bd];
	WaveDemoBoardHandle_t *WDh = &WDcfg.handles[bd];
	WaveDemoBoardRun_t *WDr = &WDcfg.runs[bd];

	uint64_t TDC = event->Event->DataGroup[ch / 2].TDC;

	if (WDr->ftdc[ch] == NULL) {
		CreateOutputFileName(OUTPUTFILE_TYPE_TDCLIST, bd, ch, fname);
		if (WDcfg.OutFileFormat == OUTFILE_ASCII)
			WDr->ftdc[ch] = fopen(fname, "w");
		else
			WDr->flist[ch] = fopen(fname, "wb");
		if (WDr->ftdc[ch] == NULL)
			return -1;
	}
	if (ftell(WDr->ftdc[ch]) < MAX_OUTPUT_FILE_SIZE) {
		if (WDcfg.OutFileFormat == OUTFILE_ASCII)
			fprintf(WDr->ftdc[ch], "%llu\n", TDC);
		else
			fwrite(&TDC, 1, sizeof(TDC), WDr->ftdc[ch]);
		fflush(WDr->ftdc[ch]);
	}
	return 0;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Save an histogram to output file
// Inputs:		FileName = filename radix (ch and board index will be added)
//				Nbin = number of bins
//				Histo = histogram to save
// Return:		0=OK, -1=error
// --------------------------------------------------------------------------------------------------------- 
int SaveHistogram(char *FileName, Histogram1D_t Histo) {
    FILE *fh, *ansi42;
    uint16_t i;
    char str[200];

	fh = fopen(FileName, "w");
    if (fh == NULL)
		return -1;
	if (WDcfg.HistoOutputFormat == HISTO_FILE_FORMAT_ANSI42) {
		ansi42 = fopen("ansi42template.txt", "r");
		if (ansi42 != NULL) {
			while(!feof(ansi42)) {
				fgets(str, 200, ansi42);
				if (strstr(str, "*PutChannelDataHere*")) {
					for(i=0; i<Histo.Nbin; i++) 
						fprintf(fh, "%d\n", Histo.H_data[i]);
				} else {
					fprintf(fh, "%s", str);
				}
			}
			fclose(ansi42);
		}
	} else if (WDcfg.HistoOutputFormat == HISTO_FILE_FORMAT_1COL) {
		for(i=0; i<Histo.Nbin; i++) 
			fprintf(fh, "%d\n", Histo.H_data[i]);
	} else if (WDcfg.HistoOutputFormat == HISTO_FILE_FORMAT_2COL) {
		for(i=0; i<Histo.Nbin; i++) 
			fprintf(fh, "%d %d\n", i, Histo.H_data[i]);
	}
    fclose(fh);
    return 0;
}


// --------------------------------------------------------------------------------------------------------- 
// Description: Save all histograms to output file
// Return:		0=OK, -1=error
// --------------------------------------------------------------------------------------------------------- 
int SaveAllHistograms() {
	int b, ch, ret = 0;
	char fname[300];

	/* Save Histograms to file for each board/channel */
	for (b = 0; b < WDcfg.NumBoards; b++) {
		for (ch = 0; ch < WDcfg.handles[b].Nch; ch++) {
			if (WDcfg.boards[b].channels[ch].ChannelEnable) {
				if (WDcfg.SaveHistograms & 0x1) {
					CreateOutputFileName(OUTPUTFILE_TYPE_EHISTO, b, ch, fname);
					ret |= SaveHistogram(fname, WDhistos.EH[b][ch]);
				}
				if (WDcfg.SaveHistograms & 0x2) {
					CreateOutputFileName(OUTPUTFILE_TYPE_THISTO, b, ch, fname);
					ret |= SaveHistogram(fname, WDhistos.TH[b][ch]);
				}
			}
		}
	}
	return ret;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Save one event to the List file
// Inputs:		bd = board index
//				ch = channel
//				timestamp = timestamp of the event
//				energy = energy of the event
// Return:		0=OK, -1=error
// --------------------------------------------------------------------------------------------------------- 
int SaveList(int bd, int ch, WaveDemoEvent_t *event) {
	char fname[100], str[1000];
	bool new_file = false;
	WaveDemoBoard_t *WDb = &WDcfg.boards[bd];
	WaveDemoBoardHandle_t *WDh = &WDcfg.handles[bd];
	WaveDemoBoardRun_t *WDr = &WDcfg.runs[bd];
	CAEN_DGTZ_X743_EVENT_t *Event = event->Event;

	if (!Event->GrPresent[ch / 2] || !WDb->channels[ch].ChannelEnable)
		return -1;

	uint32_t Size = Event->DataGroup[ch / 2].ChSize;
	if (Size <= 0) {
		return -1;
	}

	if (WDr->flist[ch] == NULL) {
		CreateOutputFileName(OUTPUTFILE_TYPE_LIST, bd, ch, fname);
		if (WDcfg.OutFileFormat == OUTFILE_ASCII)
			WDr->flist[ch] = fopen(fname, "w");
		else
			WDr->flist[ch] = fopen(fname, "wb");
		if (WDr->flist[ch] == NULL)
			return -1;
		new_file = true;
	}
	if ((WDcfg.SaveLists & 0x2) && (WDrun.flist_merged == NULL)) {
		CreateOutputFileName(OUTPUTFILE_TYPE_LIST_MERGED, 0, 0, fname);
		if (WDcfg.OutFileFormat == OUTFILE_ASCII)
			WDrun.flist_merged = fopen(fname, "w");
		else
			WDrun.flist_merged = fopen(fname, "wb");
		if (WDrun.flist_merged == NULL)
			return -1;
	}

	WaveDemo_EVENT_plus_t *evnt = &event->EventPlus[ch / 2][ch % 2];
	uint64_t TDC = event->Event->DataGroup[ch / 2].TDC;
	float RealtiveFineTime = event->EventPlus[ch / 2][ch % 2].FineTimeStamp;
	float time = TDC * 5 + RealtiveFineTime; //nanoseconds

	if (WDcfg.OutFileTimeStampUnit == 0)      sprintf(str, "%20.0f\t%10.5f",  time * 1000, evnt->Energy);        //ps
	else if (WDcfg.OutFileTimeStampUnit == 1) sprintf(str, "%20.0f\t%10.5f",  time, evnt->Energy);               //ns
	else if (WDcfg.OutFileTimeStampUnit == 2) sprintf(str, "%20.6f\t%10.5f",  time * 0.001, evnt->Energy);       //us
	else if (WDcfg.OutFileTimeStampUnit == 3) sprintf(str, "%20.9f\t%10.5f",  time * 0.000001, evnt->Energy);    //ms
	else if (WDcfg.OutFileTimeStampUnit == 4) sprintf(str, "%20.12f\t%10.5f", time * 0.000000001, evnt->Energy); // s

	char header_str[2][32] = { "Time", "Energy" };
	if (WDcfg.OutFileTimeStampUnit == 0) strcat(header_str[0], " (ps)");
	else if (WDcfg.OutFileTimeStampUnit == 1) strcat(header_str[0], " (ns)");
	else if (WDcfg.OutFileTimeStampUnit == 2) strcat(header_str[0], " (us)");
	else if (WDcfg.OutFileTimeStampUnit == 3) strcat(header_str[0], " (ms)");
	else if (WDcfg.OutFileTimeStampUnit == 4) strcat(header_str[0], " (s)");

	if (ftell(WDr->flist[ch]) < MAX_OUTPUT_FILE_SIZE) {
		if (WDcfg.OutFileFormat == OUTFILE_ASCII) {
			if (new_file && WDcfg.OutFileHeader)
				fprintf(WDr->flist[ch], "%20s\t%10s\n", header_str[0], header_str[1]);
			fprintf(WDr->flist[ch], "%s\n", str);
		}
		else {
			fwrite(&time, 1, sizeof(time), WDr->flist[ch]);
			fwrite(&evnt->Energy, 1, sizeof(evnt->Energy), WDr->flist[ch]);
		}
	}
	if (WDcfg.SaveLists & 0x2) {
		if (ftell(WDrun.flist_merged) < MAX_OUTPUT_FILE_SIZE) {
			if (WDcfg.OutFileFormat == OUTFILE_ASCII) {
				fprintf(WDrun.flist_merged, "%s\n", str);
			}
			else {
				uint8_t b8 = bd, ch8 = ch;
				fwrite(&b8, 1, sizeof(b8), WDrun.flist_merged);
				fwrite(&ch8, 1, sizeof(ch8), WDrun.flist_merged);
				fwrite(&time, 1, sizeof(time), WDr->flist[ch]);
				fwrite(&evnt->Energy, 1, sizeof(evnt->Energy), WDr->flist[ch]);
			}
		}
	}
	return 0;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Save a waveform (analog trace 0) to the Waveform file
// Inputs:		b = board index
//				ch = channel
//				evnt = event data
//				wfm = waveform data
// Return:		0=OK, -1=error
// --------------------------------------------------------------------------------------------------------- 
int SaveWaveform(int bd, int ch, WaveDemoEvent_t *event) {
	int i;
	char fname[500];

	time_t timer;
	struct tm* tm_info;


	WaveDemoBoard_t *WDb = &WDcfg.boards[bd];
	WaveDemoBoardHandle_t *WDh = &WDcfg.handles[bd];
	WaveDemoBoardRun_t *WDr = &WDcfg.runs[bd];

	WaveDemo_EVENT_plus_t *evnt = &event->EventPlus[ch / 2][ch % 2];
	Waveform_t *wfm = event->EventPlus[ch / 2][ch % 2].Waveforms;

	if (WDr->fwave[ch] == NULL) {
		CreateOutputFileName(OUTPUTFILE_TYPE_WAVE, bd, ch, fname);
		if (WDcfg.OutFileFormat == OUTFILE_BINARY)
			WDr->fwave[ch] = fopen(fname, "wb");
		else
			WDr->fwave[ch] = fopen(fname, "w");
	}
	if (WDr->fwave[ch] == NULL)
		return -1;

	if (ftell(WDr->fwave[ch]) >= MAX_OUTPUT_FILE_SIZE) {
		fclose(WDr->fwave[ch]);

		time(&timer);
		tm_info = localtime(&timer);
		strftime(WDrun.DataTimeFilename, 32, "%Y-%m-%d_%H-%M-%S", tm_info);


		CreateOutputFileName(OUTPUTFILE_TYPE_WAVE, bd, ch, fname);
		if (WDcfg.OutFileFormat == OUTFILE_BINARY)
			WDr->fwave[ch] = fopen(fname, "wb");
		else
			WDr->fwave[ch] = fopen(fname, "w");

		if (WDr->fwave[ch] == NULL)
			return -1;
	}

	if (WDcfg.OutFileFormat == OUTFILE_BINARY) {
		fwrite(&evnt->FineTimeStamp, sizeof(evnt->FineTimeStamp), 1, WDr->fwave[ch]);
		fwrite(&evnt->Energy, sizeof(evnt->Energy), 1, WDr->fwave[ch]);
		fwrite(&wfm->Ns, sizeof(wfm->Ns), 1, WDr->fwave[ch]);
		fwrite(wfm->AnalogTrace[0], sizeof(uint16_t), wfm->Ns, WDr->fwave[ch]);
	} else {
		fprintf(WDr->fwave[ch], "%lld %.3f %.3f %d\t", event->Event->DataGroup[ch / 2].TDC, evnt->FineTimeStamp, evnt->Energy, wfm->Ns);
		for(i=0; i<wfm->Ns; i++)
			fprintf(WDr->fwave[ch], "%d ", (int16_t)(wfm->AnalogTrace[0][i]));
		fprintf(WDr->fwave[ch], "\n");
	}
	
	return 0;
}

int SaveRunInfo(char *ConfigFileName) {
	char fname[500], line[500];
	char *c;
	int b, ch;
	FILE *cfg;
	FILE *rinf;
	CAEN_DGTZ_BoardInfo_t BoardInfo;

	sprintf(fname, "run_info.txt");
	CreateOutputFileName(OUTPUTFILE_TYPE_RUN_INFO, 0, 0, fname);
	rinf = fopen(fname, "w");
	if (rinf == NULL)
		return -1;

	fprintf(rinf, "-----------------------------------------------------------------\n");
	fprintf(rinf, "Boards\n");
	fprintf(rinf, "-----------------------------------------------------------------\n");
	for (b = 0; b < WDcfg.NumBoards; b++) {
		if (CAEN_DGTZ_GetInfo(WDcfg.handles[b].handle, &BoardInfo) != CAEN_DGTZ_Success) continue;
		fprintf(rinf, "Board %d:\n", b);
		fprintf(rinf, " CAEN Digitizer Model %s (S/N %u)\n", BoardInfo.ModelName, BoardInfo.SerialNumber);
		fprintf(rinf, " Rel. FPGA: ROC %s, AMC %s\n", BoardInfo.ROC_FirmwareRel, BoardInfo.AMC_FirmwareRel);
	}
	fprintf(rinf, "\n\n");
	fprintf(rinf, "-----------------------------------------------------------------\n");
	fprintf(rinf, "Statistics\n");
	fprintf(rinf, "-----------------------------------------------------------------\n");
	fprintf(rinf, "Acquisition started at %s\n", WDstats.AcqStartTimeString);
	fprintf(rinf, "Acquisition stopped at %s\n", WDstats.AcqStopTimeString);
	fprintf(rinf, "Acquisition stopped after %.2f s (RealTime)\n", WDstats.AcqStopTime / 1000);
	fprintf(rinf, "Total processed events = %llu\n", WDstats.TotEvRead_cnt);
	fprintf(rinf, "Total bytes = %.4f MB\n", (float)WDstats.RxByte_cnt / (1024 * 1024));
	for (b = 0; b < WDcfg.NumBoards; b++) {
		fprintf(rinf, "Board %2d : LastTstamp(s)   NumEvents      Rate(Hz)\n", b);
		for (ch = 0; ch < WDcfg.handles[b].Nch; ch++) {
			if (WDcfg.boards[b].channels[ch].ChannelEnable) {
				float rate = (WDstats.LatestProcTstamp[b][ch] > 0) ? (float)WDstats.EvProcessed_cnt[b][ch] / ((float)WDstats.LatestProcTstamp[b][ch] / 1e9) : 0;
				// rate = 0;
				fprintf(rinf, "   Ch %2d:  %10.2f   %12llu  %12.2f\n", ch, (float)WDstats.LatestProcTstamp[b][ch] / 1e9, WDstats.EvRead_cnt[b][ch], rate);
			}
		}
	}
	fprintf(rinf, "\n\n");
	fprintf(rinf, "-----------------------------------------------------------------\n");
	fprintf(rinf, "Configuration File\n");
	fprintf(rinf, "-----------------------------------------------------------------\n");
	cfg = fopen(ConfigFileName, "r");
	if (cfg != NULL) {
		while (!feof(cfg)) {
			fgets(line, 500, cfg);
			c = lskip(line);
			if (*c == ';' || *c == '#' || *c == '\0')
				continue;
			fputs(line, rinf);
		}
	}
	fclose(rinf);
	return 0;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Print a summary of expected output files based on configuration
// Return:      none
// --------------------------------------------------------------------------------------------------------- 
void PrintOutputFilesSummary() {
	char fname[300];
	int b, ch;
	printf("Output files saved in: %s\n", WDcfg.DataFilePath);
	// Run info
	if (WDcfg.SaveRunInfo) {
		CreateOutputFileName(OUTPUTFILE_TYPE_RUN_INFO, 0, 0, fname);
		printf("  %s\n", fname);
	}
	// Raw data
	if (WDcfg.SaveRawData) {
		CreateOutputFileName(OUTPUTFILE_TYPE_RAW, 0, 0, fname);
		printf("  %s\n", fname);
	}
	// Merged list file
	if (WDcfg.SaveLists & 0x2) {
		CreateOutputFileName(OUTPUTFILE_TYPE_LIST_MERGED, 0, 0, fname);
		printf("  %s\n", fname);
	}
	// Per channel files
	for (b = 0; b < WDcfg.NumBoards; b++) {
		for (ch = 0; ch < WDcfg.handles[b].Nch; ch++) {
			if (!WDcfg.boards[b].channels[ch].ChannelEnable)
				continue;
			if (WDcfg.SaveTDCList) {
				CreateOutputFileName(OUTPUTFILE_TYPE_TDCLIST, b, ch, fname);
				printf("  %s\n", fname);
			}
			if (WDcfg.SaveLists & 0x1) {
				CreateOutputFileName(OUTPUTFILE_TYPE_LIST, b, ch, fname);
				printf("  %s\n", fname);
			}
			if (WDcfg.SaveWaveforms) {
				CreateOutputFileName(OUTPUTFILE_TYPE_WAVE, b, ch, fname);
				printf("  %s\n", fname);
			}
			if (WDcfg.SaveHistograms & 0x1) {
				CreateOutputFileName(OUTPUTFILE_TYPE_EHISTO, b, ch, fname);
				printf("  %s\n", fname);
			}
			if (WDcfg.SaveHistograms & 0x2) {
				CreateOutputFileName(OUTPUTFILE_TYPE_THISTO, b, ch, fname);
				printf("  %s\n", fname);
			}
		}
	}
}


// --------------------------------------------------------------------------------------------------------- 
// Description: Save all the regsiters of the borad to a file
// Inputs:		handle = handle of the board
// Return:		0=OK, -1=error
// --------------------------------------------------------------------------------------------------------- 
int SaveRegImage(int handle) {
	FILE *regs;
	char fname[100];
	int ret;
	uint32_t addr, reg, gr;
	CAEN_DGTZ_BoardInfo_t BoardInfo;

	if (CAEN_DGTZ_GetInfo(handle, &BoardInfo) != CAEN_DGTZ_Success)
		return -1;

	sprintf(fname, "reg_image_%d.txt", handle);
	regs = fopen(fname, "r");
	if (regs != NULL) {
		fclose(regs);
		return -1;
	}
	regs = fopen(fname, "w");
	if (regs == NULL)
		return -1;

	fprintf(regs, "[COMMON REGS]\n");
	for (addr = 0x8000; addr <= 0x8200; addr += 4) {
		ret = CAEN_DGTZ_ReadRegister(handle, addr, &reg);
		if (ret == 0) {
			fprintf(regs, "%04X : %08X\n", addr, reg);
		}
		else {
			fprintf(regs, "%04X : --------\n", addr);
			Sleep(1);
		}
	}
	for (addr = 0xEF00; addr <= 0xEF34; addr += 4) {
		ret = CAEN_DGTZ_ReadRegister(handle, addr, &reg);
		if (ret == 0) {
			fprintf(regs, "%04X : %08X\n", addr, reg);
		}
		else {
			fprintf(regs, "%04X : --------\n", addr);
			Sleep(1);
		}
	}
	for (gr = 0; gr < BoardInfo.Channels / 2; gr++) {
		fprintf(regs, "[GROUP %d]\n", gr);
		for (addr = 0x1000 + (gr << 8); addr <= (0x10FF + (gr << 8)); addr += 4) {
			if (addr != 0x1090 + (gr << 8)) {
				ret = CAEN_DGTZ_ReadRegister(handle, addr, &reg);
				if (ret == 0) {
					fprintf(regs, "%04X : %08X\n", addr, reg);
				}
				else {
					fprintf(regs, "%04X : --------\n", addr);
					Sleep(1);
				}
			}
		}
	}
	fprintf(regs, "[CONFIGURATION ROM]\n");
	for (addr = 0xF000; addr <= 0xF088; addr += 4) {
		ret = CAEN_DGTZ_ReadRegister(handle, addr, &reg);
		if (ret == 0) {
			fprintf(regs, "%04X : %08X\n", addr, reg);
		}
		else {
			fprintf(regs, "%04X : --------\n", addr);
			Sleep(1);
		}
	}

	fclose(regs);
	return 0;
}

