/******************************************************************************
*
* CAEN SpA - Front End Division
* Via Vetraia, 11 - 55049 - Viareggio ITALY
* +390594388398 - www.caen.it
*
***************************************************************************//**
* \note TERMS OF USE:
* This program is free software; you can redistribute it and/or modify it under
* the terms of the GNU General Public License as published by the Free Software
* Foundation. This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. The user relies on the
* software, documentation and results solely at his own risk.
******************************************************************************/

#define DAWrunner_Release        "1.0"
#define DAWrunner_Release_Date   "May 2017"

#include "CAENDigitizer.h"
#include "CAENDigitizerType.h"
#include "DAWDemoFunc.h"
#include <time.h>
#include <sys/stat.h>

char path[128];

static long get_time()
{
	long time_ms;
#ifdef WIN32
	struct _timeb timebuffer;
	_ftime(&timebuffer);
	time_ms = (long)timebuffer.time * 1000 + (long)timebuffer.millitm;
#else
	struct timeval t1;
	struct timezone tz;
	gettimeofday(&t1, &tz);
	time_ms = (t1.tv_sec) * 1000 + t1.tv_usec / 1000;
#endif
	return time_ms;
}

int main(int argc, char *argv[])
{
	int ev, board, chan; // loop indices
	int *handle = NULL;// pointer to handles
	ERROR_CODES ErrCode = ERR_NONE;
	// Data and config structures
	CAEN_DGTZ_730_DAW_Event_t      **Event = NULL; // as many events as the maximum number obtainable with a BLT transfer access must be allocated 
	
	DAWPlot_t                       PlotVar; // struct containing the plot options	
	DAWConfig_t			            ConfigVar; // struct containing the config file options
	// File I/O and related variables
	FILE **RawFile;
	FILE ***WaveFile;
	FILE *f_ini = NULL;
	int *RawFileIndex;
	struct stat buf;
	int FileIndex = 0;
	uint64_t PrevCheckTime; // printout and plot time
	Counter_t *Counter, *CounterOld;
	int PrintFlag=0,PlotFlag=0;
	uint32_t AllocatedSize,BufferSize;
	char *buffer = NULL;
	char ConfigFileName[255];
	char tmpConfigFileName[255];
	int *NumEvents;
	int EventPlotted=-1;
	int BLTn,MaxBLTn=0,MaxBLTnHIndex;
	uint32_t MSize, MaxMSize=0;
	int MaxMSizeHIndex;
	struct stat info;
	uint32_t BoardInfo;
	printf("%d\n",argc);
	printf("**************************************************************\n");
	printf("                      X725/X730 DAW Demo %s\n", DAWrunner_Release);
	printf("**************************************************************\n");
	
#ifdef  WIN32
	sprintf(path, "%s\\DAW_DEMO\\", getenv("USERPROFILE"));
	_mkdir(path);
#else
	sprintf(path,"");
#endif

	// Open and parse configuration file, init parameters
	if (argc > 1) strcpy(ConfigFileName, argv[1]);
	else {
		strcpy(tmpConfigFileName, DEFAULT_CONFIG_FILE);
		sprintf(ConfigFileName, "%s%s", path, tmpConfigFileName);
	}
	ErrCode = OpenConfigFile(&f_ini, ConfigFileName);

	// new code added by Chengjie . 
	int design_time ; 
	int time_designed = 0 ; // 0 means disable 
	if (argc > 2)
	{
		design_time= atoi(argv[2]);
		printf("Designed run_time %d s \n",design_time);
		time_designed=1;
	}


	if (ErrCode == ERR_NONE) ErrCode = ParseConfigFile(f_ini, &ConfigVar);
	if (f_ini!=NULL) fclose(f_ini);
	
	// Allocate space for handles and counters according to the number of boards in the acquisition chain
	if (ErrCode == ERR_NONE) {
		if (((handle = (int*)calloc(ConfigVar.Nhandle,sizeof(int))) == NULL) ||
			((Counter = (Counter_t*)calloc(ConfigVar.Nhandle, sizeof(Counter_t))) == NULL) ||
			((CounterOld = (Counter_t*)calloc(ConfigVar.Nhandle, sizeof(Counter_t))) == NULL) ||
			((NumEvents = (int*)calloc(ConfigVar.Nhandle, sizeof(int))) == NULL) ||
			((Event = (CAEN_DGTZ_730_DAW_Event_t**)calloc(ConfigVar.Nhandle, sizeof(CAEN_DGTZ_730_DAW_Event_t*))) == NULL) ||
			((RawFileIndex = (int*)calloc(ConfigVar.Nhandle, sizeof(int))) == NULL) ||
			((RawFile = (FILE**)calloc(ConfigVar.Nhandle, sizeof(FILE*))) == NULL) ||
			((WaveFile = (FILE***)calloc(ConfigVar.Nhandle, sizeof(FILE**))) == NULL)
			) ErrCode = ERR_MALLOC;
	}

	// Open the digitizer and read the board information
	if (ErrCode == ERR_NONE) {
		printf("Open digitizers\n");
		if (OpenDigitizer(handle, &ConfigVar)) ErrCode = ERR_DGZ_OPEN;
	}
	
	// Print board info and set board-specific parameters
	if (ErrCode == ERR_NONE) {
		printf("Get board info and set board-specific parameters\n");
		for (board = 0; board < ConfigVar.Nhandle; board++) {
			if (CAEN_DGTZ_GetInfo(handle[board],&ConfigVar.BoardConfigVar[board]->BoardInfo)) ErrCode = ERR_BOARD_INFO_READ;
			else {
				printf("***************************************\n");
				printf("Connected to CAEN Digitizer Model %s\n", ConfigVar.BoardConfigVar[board]->BoardInfo.ModelName);
				printf("Board serial number %d\n", ConfigVar.BoardConfigVar[board]->BoardInfo.SerialNumber);
				printf("ROC FPGA Release is %s\n", ConfigVar.BoardConfigVar[board]->BoardInfo.ROC_FirmwareRel);
				printf("AMC FPGA Release is %s\n", ConfigVar.BoardConfigVar[board]->BoardInfo.AMC_FirmwareRel);
				CAEN_DGTZ_ReadRegister(handle[board], 0x108C, &BoardInfo);
				if (((BoardInfo >> 8) & 0xff) != DAW_FW_ID) { ErrCode = ERR_WRONG_FW; break; }
			}
		}
		printf("***************************************\n");
	}

    // Program the digitizer
	if (ErrCode == ERR_NONE) {
		printf("Program digitizers\n");
		if (ProgramDigitizers(handle, &ConfigVar)) ErrCode = ERR_DGZ_PROGRAM;
		for (board = 0; board < ConfigVar.Nhandle; board++) {
			// get the handles with the highest values of event size, events/block transfer
			CAEN_DGTZ_GetMaxNumEventsBLT(handle[board], &BLTn); if (BLTn > MaxBLTn) { BLTn = MaxBLTn; MaxBLTnHIndex = board; }
			if ((MSize = CheckMallocSize(handle[board])) > MaxMSize) { MaxMSize = MSize; MaxMSizeHIndex = board; }
		}
	}

	// Open the output files
	if (ErrCode == ERR_NONE) {
		if (stat(ConfigVar.OutFilePath, &info) != 0) {
			#ifdef  WIN32
			if (_mkdir(ConfigVar.OutFilePath) != 0) { printf("Output directory %s could not be created. Please verify that the path exists and is writable\n", ConfigVar.OutFilePath); ErrCode = ERR_OUTDIR_OPEN; }
			else printf("Output directory %s created\n", ConfigVar.OutFilePath);
			#else 
			if (mkdir(ConfigVar.OutFilePath, 0777) != 0) { printf("Output directory %s could not be created. Please verify that the path exists and is writable\n", ConfigVar.OutFilePath); ErrCode = ERR_OUTDIR_OPEN; }
			else printf("Output directory %s created\n", ConfigVar.OutFilePath);
			#endif
		}
		if (ErrCode == ERR_NONE) {
			for (board = 0; board < ConfigVar.Nhandle; board++) {
				RawFileIndex[board] = 0;
				if (ConfigVar.OFRawEnable) { if ((ErrCode = OpenRawFile(RawFile + board, board, RawFileIndex[board], ConfigVar.OutFilePath, ConfigVar.OutFileName)) != ERR_NONE) break; }
				if (ConfigVar.OFWaveEnable) {
					if ((*(WaveFile + board) = (FILE**)calloc(ConfigVar.BoardConfigVar[board]->BoardInfo.Channels, sizeof(FILE*))) == NULL) { ErrCode = ERR_MALLOC; break; }
					if ((ErrCode = OpenWaveFile(WaveFile + board, board, ConfigVar.BoardConfigVar[board], ConfigVar.OutFilePath, ConfigVar.OutFileName)) != ERR_NONE) break;
				}
			}
		}
	}
 
    // WARNING: The mallocs must be done after the digitizer programming
    // Allocate memory for the readout buffer
	if (ErrCode == ERR_NONE) {
		printf("Readout buffer malloc\n");
		if (CAEN_DGTZ_MallocReadoutBuffer(handle[MaxMSizeHIndex], &buffer, &AllocatedSize)) ErrCode = ERR_MALLOC;
	}
	// Allocate memory for the events 
	if (ErrCode == ERR_NONE) {
		printf("Event malloc\n");
		for (board = 0; board < ConfigVar.Nhandle; board++) {
			if (CAEN_DGTZ_MallocDPPEvents(handle[MaxBLTnHIndex], &Event[board], &AllocatedSize)) ErrCode = ERR_MALLOC;
		}
	}

	// reset counters
	if(ErrCode==ERR_NONE) {
		for (board = 0; board < ConfigVar.Nhandle; board++) {
			ResetCounter(Counter + board);
			ResetCounter(CounterOld + board);
		}
	}

	// No memory allocation for waveform required in this firmware
    // Open the plotter and configure its options
	if (ErrCode == ERR_NONE) {
		printf("Open plotter\n");
		ErrCode = OpenPlotter(&ConfigVar, &PlotVar);
	}   
    // Readout Loop 
	if (ErrCode == ERR_NONE) {
		printf("[s] start/stop the acquisition, [q] quit, [space key] help\n");

		// code added by Chengjie
		if (argc>2)
		{
			ConfigVar.Quit=0;
			ConfigVar.AcqRun=1;
		}


		PrevCheckTime = get_time();

		int start_time= time((time_t *)NULL);
		printf("%d\n",start_time);
		while (!ConfigVar.Quit) {
			CheckKeyboardCommands(handle, &ConfigVar);
			if (UpdateTime(ConfigVar.PlotRefreshTime, &PrevCheckTime) && ConfigVar.AcqRun) PrintFlag = 1;
			// if continuous trigger is enabled, send software triggers 
			if (ConfigVar.ContTrigger) for (board = 0; board < ConfigVar.Nhandle; board++) CAEN_DGTZ_SendSWtrigger(handle[board]);
			// Read data from the board
			for (board = 0; board < ConfigVar.Nhandle; board++) {
				if (CAEN_DGTZ_ReadData(handle[board], CAEN_DGTZ_SLAVE_TERMINATED_READOUT_MBLT, buffer, &BufferSize)) {
					ErrCode = ERR_READOUT;
					break;
				}
				if (board == ConfigVar.BoardPlotted) EventPlotted = -1;
				if (BufferSize > 0) { // The BLT returned data
					Counter[board].ByteCnt += BufferSize;
					// allocate the buffered data in the struct and return the number of events
					if (CAEN_DGTZ_GetDPPEvents(handle[board], buffer, BufferSize / 4, &Event[board], &NumEvents[board])) {
						ErrCode = ERR_EVENT_BUILD;
						break;
					}
					Counter[board].MB_TS = Event[board][NumEvents[board] - 1].timeStamp;
					//Counter[board].TrgCnt += NumEvents[board];
					if (ConfigVar.OFRawEnable) fwrite(buffer, BufferSize, 1, RawFile[board]);
				} else {
					NumEvents[board] = 0;
					continue; // the BLT returned no data, acquisition still on
				} 
				// check file size and open new file if the file size is larger than the value set in the config file
				if (RawFile[board] != NULL) {
					fstat(fileno(RawFile[board]), &buf);
					if ((int)(buf.st_size / MB_SIZE) > ConfigVar.MaxFileSize) { RawFileIndex[board]++; OpenRawFile(&RawFile[board], board, RawFileIndex[board], ConfigVar.OutFilePath, ConfigVar.OutFileName); }
				}
				// Analyze data for each event		
				for (ev = 0; ev < NumEvents[board]; ev++) {	
					for (chan = 0; chan < MAX_V1730_CHANNEL_SIZE; chan++) {
						if (Event[board][ev].chmask & (1 << chan)) {
							// add events
							Counter[board].TrgCnt[chan]++;
							// increase truncate counters if the related header flag is set
							if (Event[board][ev].Channel[chan]->truncate) Counter[board].OFCnt[chan]++;
							// remember the last event of the buffer where the plotted channel is present 
							if ((board == ConfigVar.BoardPlotted) && (chan == ConfigVar.EnableTrack)) EventPlotted = ev;
						}
					}
				}
			}
			// Print event info for the selected board and channel
			if ((PrintFlag || ConfigVar.SinglePlot) && ConfigVar.AcqRun) {
				// if plotflag is still 1 no event from the channel selected for plotting was found
				if(PlotFlag==1 && Counter[ConfigVar.BoardPlotted].ByteCnt!=CounterOld[ConfigVar.BoardPlotted].ByteCnt) {
					printf("The channel selected for plotting (board #%d, channel #%d) was not present in data\n", ConfigVar.BoardPlotted, ConfigVar.EnableTrack);
					PlotFlag = 0;
				}
				printf("==========================================\n");
				for (board = 0; board < ConfigVar.Nhandle; board++) {
					printf("Board %d", board); if (board == ConfigVar.BoardPlotted) printf("(plotted): \n"); else printf("         : \n");
					PrintData(Counter + board, CounterOld + board,&ConfigVar);
					for (chan = 0; chan < MAX_V1730_CHANNEL_SIZE; chan++) {
						if ((ConfigVar.BoardConfigVar[board]->EnableMask & (1 << chan)) && (Counter[board].OFCnt[chan] != CounterOld[board].OFCnt[chan])) {
							printf("fraction of truncated events in board %d, channel %d: %.2f%%\n", board, chan, (float)(100 * (Counter[board].OFCnt[chan] - CounterOld[board].OFCnt[chan])) / (float)NumEvents[board]);
						}
					}
					// save waves
					if (ConfigVar.OFWaveEnable) {
						OpenWaveFile(WaveFile + board, board, ConfigVar.BoardConfigVar[board], ConfigVar.OutFilePath, ConfigVar.OutFileName);
						WaveWrite(*(WaveFile + board), *(Event + board), ConfigVar.BoardConfigVar[board]);
					}
				}
				PrintFlag = 0;
				if(ConfigVar.PlotEnable || ConfigVar.SinglePlot) PlotFlag = 1;
				ConfigVar.SinglePlot = 0; // reset one-shot plot ("p" key during acquisition) 
			}

			if (PlotFlag==1) { // if no event from the selected channel has been found, PlotFlag remains active
				if ((~ConfigVar.BoardConfigVar[ConfigVar.BoardPlotted]->EnableMask) & (1 << ConfigVar.EnableTrack)) {
					printf("The channel selected for plotting (board #%d, channel #%d) is not enabled\n", ConfigVar.BoardPlotted, ConfigVar.EnableTrack);
					PlotFlag = 0;
				} else if (EventPlotted != -1) {
					PlotEvent(&ConfigVar, &PlotVar, &Event[ConfigVar.BoardPlotted][EventPlotted]);
					PlotFlag = 0;
				}
			}		

			// new code added by Chengjie 	
			if (argc>2)
			{
				int now_time=time((time_t *)NULL);
				// printf("%d\n",now_time);
				if ( now_time > start_time + design_time)
					{
						ConfigVar.Quit=1;	
					}
			}	
		}
	}

    if (ErrCode) {
        printf("\a%s\n", ErrMsg[ErrCode]);
        printf("Press a key to quit\n");
		getch();
    }

    // stop the acquisition
	if (ErrCode != ERR_DGZ_OPEN) for (board = 0; board < ConfigVar.Nhandle; board++) CAEN_DGTZ_SWStopAcquisition(handle[board]);
	// close the output files (if allocated)
	for (board = 0; board < ConfigVar.Nhandle; board++) {
		if (RawFile[board] != NULL) fclose(RawFile[board]);
	}
	// close the plotter
	if (PlotVar.plotpipe != NULL) ClosePlotter(&PlotVar.plotpipe);
	// free buffers (if allocated)
	if (buffer != NULL) CAEN_DGTZ_FreeReadoutBuffer(&buffer);
	for (board = 0; board < ConfigVar.Nhandle; board++) {
		if (Event[board] != NULL) {
			// free events and waveforms (if allocated)
			CAEN_DGTZ_FreeDPPEvents(handle[MaxBLTnHIndex],&Event[board]);
		}
	}
	if (ErrCode != ERR_DGZ_OPEN) for (board = 0; board < ConfigVar.Nhandle; board++) CAEN_DGTZ_CloseDigitizer(handle[board]);
	return 0;
}