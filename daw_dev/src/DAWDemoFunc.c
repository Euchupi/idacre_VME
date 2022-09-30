#include "DAWDemoFunc.h"
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>

extern char path[128];

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

ERROR_CODES OpenConfigFile(FILE **f_ini, char *ConfigFileName) {
	ERROR_CODES return_code = ERR_NONE;
	printf("Opening Configuration File %s\n", ConfigFileName);
	if ((*f_ini = fopen(ConfigFileName, "r")) == NULL) return_code = ERR_CONF_FILE_NOT_FOUND;
	return return_code;
}

ERROR_CODES ParseConfigFile(FILE *f_ini, DAWConfig_t *ConfigVar) {
	ERROR_CODES return_code = ERR_NONE;
	char *str, str1[1000];
	char strline[1000];
	uint32_t addr, data;
	int line = 0;
	int i, j, ch = -1, board = -1, val, Off = 0;
	int DefConfig = 0;
	/* Default settings */
	memset(ConfigVar, 0, sizeof(*ConfigVar));
	ConfigVar->Nhandle = 0;
	#ifdef WIN32
		sprintf(ConfigVar->OutFilePath, "%s%s", path, OUTFILE_PATH);
	#else
		sprintf(ConfigVar->OutFilePath, "%s%s", getenv("HOME"), OUTFILE_PATH);
	#endif
	sprintf(ConfigVar->OutFileName, "%s", OUTFILE_NAME);
	sprintf(ConfigVar->GnuPlotPath, "%s%s", path, PLOTTER_PATH);
	/* read config file and assign parameters */
	while (fgets(strline, 1000, f_ini) != NULL) { // get a line
		line++;
		if (!strline || strline[0] == '#' || strline[0] == ' ' || strline[0] == '\t' || strline[0] == '\n' || strline[0] == '\r') continue;
		str = strtok(strline, " \r\t\n");
		if (str[0] == '[') {
			fscanf(f_ini, "%d", &val);
			if (strstr(str, "COMMON")) { ch = -1; board = -1; }
			else if (strstr(str, "BOARD")) { ch = -1; board = (int)strtol(strtok(NULL, " \r\t\n"), NULL, 10); }
			else if (strstr(str, "CHANNEL")) {
				ch = (int)strtol(strtok(NULL, " \r\t\n"), NULL, 10);
				if (ch < 0 || ch >= MAX_CH) printf("%s: Invalid channel number\n", str);
			}
			continue;
		}

		// OPEN: malloc memory for the board config variable, init it to default and read the details of physical path to the digitizer
		if (!strcmp(strcpy(str1, str), "OPEN")) {
			// malloc board config variable
			ConfigVar->BoardConfigVar[ConfigVar->Nhandle] = (DAWBoardConfig_t*)malloc(sizeof(DAWBoardConfig_t));
			// initialize parameters
			ConfigVar->BoardConfigVar[ConfigVar->Nhandle]->ExtTriggerMode = CAEN_DGTZ_TRGMODE_ACQ_ONLY;
			ConfigVar->BoardConfigVar[ConfigVar->Nhandle]->StartMode = CAEN_DGTZ_SW_CONTROLLED;
			ConfigVar->BoardConfigVar[ConfigVar->Nhandle]->FPIOtype = CAEN_DGTZ_IOLevel_TTL;
			ConfigVar->BoardConfigVar[ConfigVar->Nhandle]->EnableMask = 0x0;
			ConfigVar->BoardConfigVar[ConfigVar->Nhandle]->GainFactor = 0;
			ConfigVar->BoardConfigVar[ConfigVar->Nhandle]->GWn = 0;
			for (j = 0; j < MAX_CH; j++) {
				ConfigVar->BoardConfigVar[ConfigVar->Nhandle]->RecordLength[j] = 256;
				ConfigVar->BoardConfigVar[ConfigVar->Nhandle]->PulsePolarity[j] = 1;
				ConfigVar->BoardConfigVar[ConfigVar->Nhandle]->preTrgg[j] = 12;
				ConfigVar->BoardConfigVar[ConfigVar->Nhandle]->NSampAhe[j] = 4;
				ConfigVar->BoardConfigVar[ConfigVar->Nhandle]->DAWTrigThr[j] = 1000;
				ConfigVar->BoardConfigVar[ConfigVar->Nhandle]->BLineMode[j] = 0;
				ConfigVar->BoardConfigVar[ConfigVar->Nhandle]->BLineDefValue[j] = 0x2000;
				ConfigVar->BoardConfigVar[ConfigVar->Nhandle]->DCoffset[j] = 0;
				ConfigVar->BoardConfigVar[ConfigVar->Nhandle]->TP_Enable[j] = 0;

			}
			// end of initalization
			if ((str = strtok(NULL, " \r\t\n")) == NULL) { printf("No 1st argument for %s. The command will be ignored\n", str1); continue; }
			if (strcmp(str, "USB") == 0) ConfigVar->BoardConfigVar[ConfigVar->Nhandle]->LinkType = CAEN_DGTZ_USB;
			else if (strcmp(str, "PCI") == 0) ConfigVar->BoardConfigVar[ConfigVar->Nhandle]->LinkType = CAEN_DGTZ_OpticalLink;
			else { printf("%s %s: Invalid connection type\n", str, str1); return -1; }

			if ((str = strtok(NULL, " \r\t\n")) == NULL) { printf("No 1st argument for %s. The command will be ignored\n", str1); continue; }
			ConfigVar->BoardConfigVar[ConfigVar->Nhandle]->LinkNum = (int)strtol(str, NULL, 10);
			if (ConfigVar->BoardConfigVar[ConfigVar->Nhandle]->LinkType == CAEN_DGTZ_USB) {
				ConfigVar->BoardConfigVar[ConfigVar->Nhandle]->ConetNode = 0;
				if ((str = strtok(NULL, " \r\t\n")) == NULL) ConfigVar->BoardConfigVar[ConfigVar->Nhandle]->BaseAddress = 0;
				else ConfigVar->BoardConfigVar[ConfigVar->Nhandle]->BaseAddress = (int)strtol(str, NULL, 0);
			}
			else {
				if ((str = strtok(NULL, " \r\t\n")) == NULL) { printf("No 3rd argument for %s. The command will be ignored\n", str1); continue; }
				ConfigVar->BoardConfigVar[ConfigVar->Nhandle]->ConetNode = (int)strtol(str, NULL, 10);
				if ((str = strtok(NULL, " \r\t\n")) == NULL) ConfigVar->BoardConfigVar[ConfigVar->Nhandle]->BaseAddress = 0;
				else ConfigVar->BoardConfigVar[ConfigVar->Nhandle]->BaseAddress = (int)strtol(str, NULL, 0);
			}
			ConfigVar->Nhandle++;
			if ((str = strtok(NULL, " \r\t\n")) != NULL) printf("WARNING: too many arguments in %s. the first exceeding argument is %s\n", str1, str);
			continue;
		}

		// Generic VME Write (address offset + data, both exadecimal)
		if (!strcmp(strcpy(str1, str), "WRITE_REGISTER")) {
			addr = (int)strtol(strtok(NULL, " \r\t\n"), NULL, 0);
			data = (int)strtol(strtok(NULL, " \r\t\n"), NULL, 0);
			for (i = 0; i < ConfigVar->Nhandle; i++) {
				if (ConfigVar->BoardConfigVar[i]->GWn < MAX_GW) {
					if (i == board || board == -1) {
						ConfigVar->BoardConfigVar[i]->GWaddr[ConfigVar->BoardConfigVar[i]->GWn] = addr;
						ConfigVar->BoardConfigVar[i]->GWdata[ConfigVar->BoardConfigVar[i]->GWn] = data;
						ConfigVar->BoardConfigVar[i]->GWn++;
					}
				}
				else {
					printf("Maximum number of register locations reached\n");
				}
			}
			if ((str = strtok(NULL, " \r\t\n")) != NULL) printf("WARNING: too many arguments in %s. the first exceeding argument is %s\n", str1, str);
			continue;
		}

		// Enable gnuplot (YES/NO)
		if (!strcmp(strcpy(str1, str), "PERIODIC_PLOT")) {
			if ((str = strtok(NULL, " \r\t\n")) == NULL) { printf("No argument for %s. The command will be ignored\n", str1); continue; }
			if (strcmp(str, "YES") == 0) ConfigVar->PlotEnable = 1;
			else if (strcmp(str, "NO") == 0) ConfigVar->PlotEnable = 0;
			else printf("%s: invalid option\n", str);
			if ((str = strtok(NULL, " \r\t\n")) != NULL) printf("WARNING: too many arguments in %s. the first exceeding argument is %s\n", str1, str);
			continue;
		}

		// Enable sync procedure (YES/NO)
		if (!strcmp(strcpy(str1, str), "SYNC_ENABLE")) {
			if ((str = strtok(NULL, " \r\t\n")) == NULL) { printf("No argument for %s. The command will be ignored\n", str1); continue; }
			if (strcmp(str, "YES") == 0) ConfigVar->SyncEnable = 1;
			else if (strcmp(str, "NO") == 0) ConfigVar->SyncEnable = 0;
			else printf("%s: invalid option\n", str);
			if ((str = strtok(NULL, " \r\t\n")) != NULL) printf("WARNING: too many arguments in %s. the first exceeding argument is %s\n", str1, str);
			continue;
		}

		// Enable raw output (YES/NO)
		if (!strcmp(strcpy(str1, str), "OUTFILE_RAW")) {
			if ((str = strtok(NULL, " \r\t\n")) == NULL) { printf("No argument for %s. The command will be ignored\n", str1); continue; }
			if (strcmp(str, "YES") == 0) ConfigVar->OFRawEnable = 1;
			else if (strcmp(str, "NO") == 0) ConfigVar->OFRawEnable = 0;
			else printf("%s: invalid option\n", str);
			if ((str = strtok(NULL, " \r\t\n")) != NULL) printf("WARNING: too many arguments in %s. the first exceeding argument is %s\n", str1, str);
			continue;
		}

		// Enable raw output (YES/NO)
		if (!strcmp(strcpy(str1, str), "OUTFILE_WAVE")) {
			if ((str = strtok(NULL, " \r\t\n")) == NULL) { printf("No argument for %s. The command will be ignored\n", str1); continue; }
			if (strcmp(str, "YES") == 0) ConfigVar->OFWaveEnable = 1;
			else if (strcmp(str, "NO") == 0) ConfigVar->OFWaveEnable = 0;
			else printf("%s: invalid option\n", str);
			if ((str = strtok(NULL, " \r\t\n")) != NULL) printf("WARNING: too many arguments in %s. the first exceeding argument is %s\n", str1, str);
			continue;
		}

		// Output file path
		if (!strcmp(strcpy(str1, str), "OUTFILE_PATH")) {
			if ((str = strtok(NULL, " \r\t\n")) == NULL) { printf("No argument for %s. The command will be ignored\n", str1); continue; }
			#ifdef WIN32
			sprintf(ConfigVar->OutFilePath, "%s%s", path, str);
			#else
			sprintf(ConfigVar->OutFilePath, "%s%s", getenv("HOME"), str);
			#endif
			strcpy(ConfigVar->OutFilePath, str);
			if ((str = strtok(NULL, " \r\t\n")) != NULL) printf("WARNING: too many arguments in %s. the first exceeding argument is %s\n", str1, str);
			continue;
		}

		// Output file name
		if (!strcmp(strcpy(str1, str), "OUTFILE_NAME")) {
			if ((str = strtok(NULL, " \r\t\n")) == NULL) { printf("No argument for %s. The command will be ignored\n", str1); continue; }
			strcpy(ConfigVar->OutFileName, str);
			if ((str = strtok(NULL, " \r\t\n")) != NULL) printf("WARNING: too many arguments in %s. the first exceeding argument is %s\n", str1, str);
			continue;
		}

		// Output file max size
		if (!strcmp(strcpy(str1, str), "OUTFILE_MAXSIZE")) {
			if ((str = strtok(NULL, " \r\t\n")) == NULL) { printf("No argument for %s. The command will be ignored\n", str1); continue; }
			ConfigVar->MaxFileSize = (int)strtol(str, NULL, 10);
			if ((str = strtok(NULL, " \r\t\n")) != NULL) printf("WARNING: too many arguments in %s. the first exceeding argument is %s\n", str1, str);
			continue;
		}
		// continuous software trigger (YES/NO)
		if (!strcmp(strcpy(str1, str), "CONT_SWTRIGGER")) {
			if ((str = strtok(NULL, " \r\t\n")) == NULL) { printf("No argument for %s. The command will be ignored\n", str1); continue; }
			if (strcmp(str, "YES") == 0) ConfigVar->ContTrigger = 1;
			else if (strcmp(str, "NO") == 0) ConfigVar->ContTrigger = 0;
			else printf("%s: invalid option\n", str);
			if ((str = strtok(NULL, " \r\t\n")) != NULL) printf("WARNING: too many arguments in %s. the first exceeding argument is %s\n", str1, str);
			continue;
		}

		if (!strcmp(strcpy(str1, str), "GNUPLOT_PATH")) {
			if ((str = strtok(NULL, " \r\t\n")) == NULL) { printf("No argument for %s. The command will be ignored\n", str1); continue; }
			strcpy(ConfigVar->GnuPlotPath, str);
			if ((str = strtok(NULL, " \r\t\n")) != NULL) printf("WARNING: too many arguments in %s. the first exceeding argument is %s\n", str1, str);
			continue;
		}


		if (!strcmp(strcpy(str1, str), "STAT_REFRESH")) {
			if ((str = strtok(NULL, " \r\t\n")) == NULL) { printf("No argument for %s. The command will be ignored\n", str1); continue; }
			ConfigVar->PlotRefreshTime = (int)strtol(str, NULL, 10);
			if ((str = strtok(NULL, " \r\t\n")) != NULL) printf("WARNING: too many arguments in %s. the first exceeding argument is %s\n", str1, str);
			continue;
		}

		// Front Panel LEMO I/O level (NIM, TTL)
		if (!strcmp(strcpy(str1, str), "FPIO_LEVEL")) {
			if ((str = strtok(NULL, " \r\t\n")) == NULL) { printf("No argument for %s. The command will be ignored\n", str1); continue; }
			for (i = 0; i < ConfigVar->Nhandle; i++) {
				if (i == board || board == -1) {
					if (strcmp(str, "TTL") == 0) ConfigVar->BoardConfigVar[i]->FPIOtype = 1;
					else if (strcmp(str, "NIM") == 0) ConfigVar->BoardConfigVar[i]->FPIOtype = 0;
					else { printf("%s: invalid option\n", str); break; }
				}
			}
			if ((str = strtok(NULL, " \r\t\n")) != NULL) printf("WARNING: too many arguments in %s. the first exceeding argument is %s\n", str1, str);
			continue;
		}

		// External Trigger (DISABLED, ACQUISITION_ONLY, ACQUISITION_AND_TRGOUT)
		if (!strcmp(strcpy(str1, str), "EXTERNAL_TRIGGER")) {
			if ((str = strtok(NULL, " \r\t\n")) == NULL) { printf("No argument for %s. The command will be ignored\n", str1); continue; }
			for (i = 0; i < ConfigVar->Nhandle; i++) {
				if (i == board || board == -1) {
					if (strcmp(str, "DISABLED") == 0)
						ConfigVar->BoardConfigVar[i]->ExtTriggerMode = CAEN_DGTZ_TRGMODE_DISABLED;
					else if (strcmp(str, "ACQUISITION_ONLY") == 0)
						ConfigVar->BoardConfigVar[i]->ExtTriggerMode = CAEN_DGTZ_TRGMODE_ACQ_ONLY;
					else if (strcmp(str, "ACQUISITION_AND_TRGOUT") == 0)
						ConfigVar->BoardConfigVar[i]->ExtTriggerMode = CAEN_DGTZ_TRGMODE_ACQ_AND_EXTOUT;
					else { printf("%s: Invalid Parameter\n", str); break; }
				}
			}
			if ((str = strtok(NULL, " \r\t\n")) != NULL) printf("WARNING: too many arguments in %s. the first exceeding argument is %s\n", str1, str);
			continue;
		}

		if (!strcmp(strcpy(str1, str), "START_ACQ")) {
			if ((str = strtok(NULL, " \r\t\n")) == NULL) { printf("No argument for %s. The command will be ignored\n", str1); continue; }
			for (i = 0; i < ConfigVar->Nhandle; i++) {
				if (i == board || board == -1) {
					if (strcmp(str, "SW") == 0) ConfigVar->BoardConfigVar[i]->StartMode = CAEN_DGTZ_SW_CONTROLLED;
					else if (strcmp(str, "S_IN") == 0) ConfigVar->BoardConfigVar[i]->StartMode = CAEN_DGTZ_S_IN_CONTROLLED;
					else if (strcmp(str, "FIRST_TRG") == 0) ConfigVar->BoardConfigVar[i]->StartMode = CAEN_DGTZ_FIRST_TRG_CONTROLLED;
					else if (strcmp(str, "LVDS") == 0) ConfigVar->BoardConfigVar[i]->StartMode = CAEN_DGTZ_LVDS_CONTROLLED;
					else { printf("%s: Invalid Parameter\n", str); break; }
				}
			}
			if ((str = strtok(NULL, " \r\t\n")) != NULL) printf("WARNING: too many arguments in %s. the first exceeding argument is %s\n", str1, str);
			continue;
		}

		// BASELINE DEFAULT ENABLED (YES/NO)
		if (!strcmp(strcpy(str1, str), "BLINE_DEFMODE")) {
			if ((str = strtok(NULL, " \r\t\n")) == NULL) { printf("No argument for %s. The command will be ignored\n", str1); continue; }
			for (j = 0; j < ConfigVar->Nhandle; j++) {
				if (j == board || board == -1) {
					for (i = 0; i < MAX_CH; i++) {
						if (i == ch || ch == -1) {
							if (strcmp(str, "YES") == 0) ConfigVar->BoardConfigVar[j]->BLineMode[i] = 1;
							else if (strcmp(str, "NO") == 0) ConfigVar->BoardConfigVar[j]->BLineMode[i] = 0;
							else { printf("%s: invalid option\n", str); break; }
						}
					}
				}
			}
			if ((str = strtok(NULL, " \r\t\n")) != NULL) printf("WARNING: too many arguments in %s. the first exceeding argument is %s\n", str1, str);
			continue;
		}

		// BASELINE DEFAULT VALUE
		if (!strcmp(strcpy(str1, str), "BLINE_DEFVALUE")) {
			if ((str = strtok(NULL, " \r\t\n")) == NULL) { printf("No argument for %s. The command will be ignored\n", str1); continue; }
			for (j = 0; j < ConfigVar->Nhandle; j++) {
				if (j == board || board == -1) {
					for (i = 0; i < MAX_CH; i++) if (i == ch || ch == -1) ConfigVar->BoardConfigVar[j]->BLineDefValue[i] = (int)strtol(str, NULL, 10);
				}
			}
			if ((str = strtok(NULL, " \r\t\n")) != NULL) printf("WARNING: too many arguments in %s. the first exceeding argument is %s\n", str1, str);
			continue;
		}

		// PULSE POLARITY (POSITIVE/NEGATIVE)
		if (!strcmp(strcpy(str1, str), "PULSE_POLARITY")) {
			if ((str = strtok(NULL, " \r\t\n")) == NULL) { printf("No argument for %s. The command will be ignored\n", str1); continue; }
			for (j = 0; j < ConfigVar->Nhandle; j++) {
				if (j == board || board == -1) {
					for (i = 0; i < MAX_CH; i++) {
						if (i == ch || ch == -1) {
							if (strcmp(str, "POSITIVE") == 0) ConfigVar->BoardConfigVar[j]->PulsePolarity[i] = 1;
							else if (strcmp(str, "NEGATIVE") == 0) ConfigVar->BoardConfigVar[j]->PulsePolarity[i] = 0;
							else printf("%s: invalid option\n", str); continue;
						}
					}
				}
			}
			if ((str = strtok(NULL, " \r\t\n")) != NULL) printf("WARNING: too many arguments in %s. the first exceeding argument is %s\n", str1, str);
			continue;
		}

		// SELF TRIGGER ENABLE (YES/NO)
		if (!strcmp(strcpy(str1, str), "LOCK_TEMP_CALIBRATION")) {
			if ((str = strtok(NULL, " \r\t\n")) == NULL) { printf("No argument for %s. The command will be ignored\n", str1); continue; }
			for (j = 0; j < ConfigVar->Nhandle; j++) {
				if (j == board || board == -1) {
					if (strcmp(str, "YES") == 0) ConfigVar->BoardConfigVar[j]->LockTempCalib = 1;
					else if (strcmp(str, "NO") == 0) ConfigVar->BoardConfigVar[j]->LockTempCalib = 0;
					else printf("%s: invalid option\n", str); continue;		
				}
			}
			if ((str = strtok(NULL, " \r\t\n")) != NULL) printf("WARNING: too many arguments in %s. the first exceeding argument is %s\n", str1, str);
			continue;
		}

		// SELF TRIGGER ENABLE (YES/NO)
		if (!strcmp(strcpy(str1, str), "SELF_TRIGGER")) {
			if ((str = strtok(NULL, " \r\t\n")) == NULL) { printf("No argument for %s. The command will be ignored\n", str1); continue; }
			for (j = 0; j < ConfigVar->Nhandle; j++) {
				if (j == board || board == -1) {
					for (i = 0; i < MAX_CH; i++) {
						if (i == ch || ch == -1) {
							if (strcmp(str, "YES") == 0) ConfigVar->BoardConfigVar[j]->ST_Enable[i] = 1;
							else if (strcmp(str, "NO") == 0) ConfigVar->BoardConfigVar[j]->ST_Enable[i] = 0;
							else printf("%s: invalid option\n", str); continue;
						}
					}
				}
			}
			if ((str = strtok(NULL, " \r\t\n")) != NULL) printf("WARNING: too many arguments in %s. the first exceeding argument is %s\n", str1, str);
			continue;
		}

		// TEST PULSE ENABLE (YES/NO)
		if (!strcmp(strcpy(str1, str), "TEST_PULSE")) {
			if ((str = strtok(NULL, " \r\t\n")) == NULL) { printf("No argument for %s. The command will be ignored\n", str1); continue; }
			for (j = 0; j < ConfigVar->Nhandle; j++) {
				if (j == board || board == -1) {
					for (i = 0; i < MAX_CH; i++) {
						if (i == ch || ch == -1) {
							if (strcmp(str, "YES") == 0) ConfigVar->BoardConfigVar[j]->TP_Enable[i] = 1;
							else if (strcmp(str, "NO") == 0) ConfigVar->BoardConfigVar[j]->TP_Enable[i] = 0;
							else printf("%s: invalid option\n", str); continue;
						}
					}
				}
			}
			if ((str = strtok(NULL, " \r\t\n")) != NULL) printf("WARNING: too many arguments in %s. the first exceeding argument is %s\n", str1, str);
			continue;
		}

		if (!strcmp(strcpy(str1, str), "TP_TYPE")) {
			if ((str = strtok(NULL, " \r\t\n")) == NULL) { printf("No argument for %s. The command will be ignored\n", str1); continue; }
			val = (int)strtol(str, NULL, 10);
			for (j = 0; j < ConfigVar->Nhandle; j++) if (j == board || board == -1) ConfigVar->BoardConfigVar[j]->TP_Type = val;
			if ((str = strtok(NULL, " \r\t\n")) != NULL) printf("WARNING: too many arguments in %s. the first exceeding argument is %s\n", str1, str);
			continue;
		}

		if (!strcmp(strcpy(str1, str), "RECORD_LENGTH")) {
			if ((str = strtok(NULL, " \r\t\n")) == NULL) { printf("No argument for %s. The command will be ignored\n", str1); continue; }
			val = (int)strtol(str, NULL, 10);
			for (j = 0; j < ConfigVar->Nhandle; j++) {
				if (j == board || board == -1) {
					for (i = 0; i < MAX_CH; i++) if (ch == -1 || i == ch) ConfigVar->BoardConfigVar[j]->RecordLength[i] = val;
				}
			}
			if ((str = strtok(NULL, " \r\t\n")) != NULL) printf("WARNING: too many arguments in %s. the first exceeding argument is %s\n", str1, str);
			continue;
		}

		if (!strcmp(strcpy(str1, str), "GAIN_FACTOR")) {
			if ((str = strtok(NULL, " \r\t\n")) == NULL) { printf("No argument for %s. The command will be ignored\n", str1); continue; }
			val = (int)strtol(str, NULL, 10);
			for (j = 0; j < ConfigVar->Nhandle; j++) if (j == board || board == -1) ConfigVar->BoardConfigVar[j]->GainFactor = val;
			if ((str = strtok(NULL, " \r\t\n")) != NULL) printf("WARNING: too many arguments in %s. the first exceeding argument is %s\n", str1, str);
			continue;
		}

		// Trigger threshold
		if (!strcmp(strcpy(str1, str), "TRG_THRESHOLD")) {
			if ((str = strtok(NULL, " \r\t\n")) == NULL) { printf("No argument for %s. The command will be ignored\n", str1); continue; }
			val = (int)strtol(str, NULL, 10);
			for (j = 0; j < ConfigVar->Nhandle; j++) {
				if (j == board || board == -1) {
					for (i = 0; i < MAX_CH; i++) if (ch == -1 || i == ch) ConfigVar->BoardConfigVar[j]->DAWTrigThr[i] = val;
				}
			}
			if ((str = strtok(NULL, " \r\t\n")) != NULL) printf("WARNING: too many arguments in %s. the first exceeding argument is %s\n", str1, str);
			continue;
		}

		//Nsamp ahead
		if (!strcmp(strcpy(str1, str), "N_LFW")) {
			if ((str = strtok(NULL, " \r\t\n")) == NULL) { printf("No argument for %s. The command will be ignored\n", str1); continue; }
			val = (int)strtol(str, NULL, 10);
			for (j = 0; j < ConfigVar->Nhandle; j++) {
				if (j == board || board == -1) {
					for (i = 0; i < MAX_CH; i++) if (ch == -1 || i == ch) ConfigVar->BoardConfigVar[j]->NSampAhe[i] = val;
				}
			}
			if ((str = strtok(NULL, " \r\t\n")) != NULL) printf("WARNING: too many arguments in %s. the first exceeding argument is %s\n", str1, str);
			continue;
		}

		//Max tail
		if (!strcmp(strcpy(str1, str), "MAX_TAIL")) {
			if ((str = strtok(NULL, " \r\t\n")) == NULL) { printf("No argument for %s. The command will be ignored\n", str1); continue; }
			val = (int)strtol(str, NULL, 10);
			for (j = 0; j < ConfigVar->Nhandle; j++) {
				if (j == board || board == -1) {
					for (i = 0; i < MAX_CH; i++) if (ch == -1 || i == ch) ConfigVar->BoardConfigVar[j]->MaxTail[i] = val;
				}
			}
			if ((str = strtok(NULL, " \r\t\n")) != NULL) printf("WARNING: too many arguments in %s. the first exceeding argument is %s\n", str1, str);
			continue;
		}

		// Pretrigger
		if (!strcmp(strcpy(str1, str), "PRE_TRIGGER")) {
			if ((str = strtok(NULL, " \r\t\n")) == NULL) { printf("No argument for %s. The command will be ignored\n", str1); continue; }
			val = (int)strtol(str, NULL, 10);
			for (j = 0; j < ConfigVar->Nhandle; j++) {
				if (j == board || board == -1) {
					for (i = 0; i < MAX_CH; i++) if (ch == -1 || i == ch) ConfigVar->BoardConfigVar[j]->preTrgg[i] = val;
				}
			}
			if ((str = strtok(NULL, " \r\t\n")) != NULL) printf("WARNING: too many arguments in %s. the first exceeding argument is %s\n", str1, str);
			continue;
		}

		if (!strcmp(strcpy(str1, str), "DC_OFFSET")) {
			if ((str = strtok(NULL, " \r\t\n")) == NULL) { printf("No argument for %s. The command will be ignored\n", str1); continue; }
			float dc = strtof(str, NULL);
			val = (int)((dc + 50) * 65535 / 100);
			for (j = 0; j < ConfigVar->Nhandle; j++) {
				if (j == board || board == -1) {
					for (i = 0; i < MAX_CH; i++) if (ch == -1 || i == ch) ConfigVar->BoardConfigVar[j]->DCoffset[i] = val;
				}
			}
			if ((str = strtok(NULL, " \r\t\n")) != NULL) printf("WARNING: too many arguments in %s. the first exceeding argument is %s\n", str1, str);
			continue;
		}

		if (!strcmp(strcpy(str1, str), "ENABLE_INPUT")) {
			if ((str = strtok(NULL, " \r\t\n")) == NULL) { printf("No argument for %s. The command will be ignored\n", str1); continue; }
			for (j = 0; j < ConfigVar->Nhandle; j++) {
				if (j == board || board == -1) {
					if (strcmp(str, "YES") == 0) {
						if (ch == -1) ConfigVar->BoardConfigVar[j]->EnableMask = 0xFFFF;
						else ConfigVar->BoardConfigVar[j]->EnableMask = ConfigVar->BoardConfigVar[j]->EnableMask | (1 << ch);
					} else if (strcmp(str, "NO") == 0) {
						if (ch == -1) ConfigVar->BoardConfigVar[j]->EnableMask = 0x00;
						else ConfigVar->BoardConfigVar[j]->EnableMask = ConfigVar->BoardConfigVar[j]->EnableMask & ~(1 << ch);
					} else {
						printf("%s: invalid option\n", str);
						break;
					}
				}
			}
			if ((str = strtok(NULL, " \r\t\n")) != NULL) printf("WARNING: too many arguments in %s. the first exceeding argument is %s\n", str1, str);
			continue;
		}

		if (!strcmp(strcpy(str1, str), "ENABLE_GRAPH")) {
			if ((str = strtok(NULL, " \r\t\n")) == NULL) { printf("No argument for %s. The command will be ignored\n", str1); continue; }
			val = (int)strtol(str, NULL, 10);
			for (j = 0; j < ConfigVar->Nhandle; j++) if (j == board || board == -1) ConfigVar->EnableTrack = val;	
			if ((str = strtok(NULL, " \r\t\n")) != NULL) printf("WARNING: too many arguments in %s. the first exceeding argument is %s\n", str1, str);
			continue;
		}

		printf("%s: invalid setting at line %d\n", str, line);
		return_code = ERR_PARSE_CONFIG;
		break;
	}
	return return_code;
}

ERROR_CODES OpenRawFile(FILE **outfile, int BoardIndex, int FileIndex, char *path, char *fname) {
	ERROR_CODES return_code = ERR_NONE;
	char filename[400];
	struct stat info;
	if (stat(path, &info) != 0) {
		printf("path %s cannot be accessed. Please verify that the selected directory exists and is writable\n", path); return ERR_OUTFILE_OPEN;
	}
	if (*outfile != NULL) fclose(*outfile);
	sprintf(filename, "%s%s_raw_b%d_seg%d.bin", path, fname, BoardIndex, FileIndex);
	if ((*outfile = fopen(filename, "w")) == NULL) {
		printf("output file %s could not be created.\n", filename);
		return_code = ERR_OUTFILE_OPEN;
	}
	return return_code;
}

ERROR_CODES OpenWaveFile(FILE ***outfile, int BoardIndex, DAWBoardConfig_t *BoardConfigVar, char *path, char *fname) {
	ERROR_CODES return_code = ERR_NONE;
	char filename[400];
	int channel;
	struct stat info;
	if (stat(path, &info) != 0) {
		printf("path %s cannot be accessed. Please verify that the selected directory exists and is writable\n", path); return ERR_OUTFILE_OPEN;
	}
	for (channel = 0; channel < BoardConfigVar->BoardInfo.Channels; channel++) {
		if ((BoardConfigVar->EnableMask >> channel) & 0x1) {
			sprintf(filename, "%s%s_wave_b%d_ch%d.txt", path, fname, BoardIndex, channel);
			if ((*(*(outfile)+channel)) != NULL) {
				fclose((*(*(outfile)+channel)));
			}
			if ((*(*(outfile)+channel) = fopen(filename, "w")) == NULL) {
				printf("output file %s could not be created.\n", filename);
				return ERR_OUTFILE_OPEN;
			}
		}
		else *(*(outfile)+channel) = NULL;
	}
	return return_code;
}

void WaveWrite(FILE **WaveFile, CAEN_DGTZ_730_DAW_Event_t *Event, DAWBoardConfig_t *BoardConfigVar) {
	int channel;
	int sample;
	int sampnum = 0;
	for (channel = 0; channel < MAX_NUM_TRACES; channel++) {
		if ((Event->chmask >> channel) & 0x1) {
			for (sample = 0; sample < 2*Event->Channel[channel]->size; sample++) {
				fprintf(*(WaveFile + channel), "%*d\t", 7, sample);
				fprintf(*(WaveFile + channel), "%*u\n", 12, *(Event->Channel[channel]->DataPtr + sample));
			}
		}
	}
}

int XX2530_DAW_SetPostSamples(int handle, uint32_t samples, int channel) {
	int ret;
	if (samples <= 0xffffff) {
		ret = CAEN_DGTZ_WriteRegister(handle, 0x1078 | (channel << 8),samples);
	} else {
		printf("invalid value for post-signal sample number of channel %d\n",channel);
	}
	return ret;
}

int XX2530_DAW_SetMaxTail(int handle, uint32_t tail, int channel) {
	int ret;
	if (tail < 0xffffff) {
		ret = CAEN_DGTZ_WriteRegister(handle, 0x107C | (channel << 8), tail);
	}
	else {
		printf("invalid value for maximum tail value of channel %d\n", channel);
	}
	return ret;
}

int XX2530_DAW_SetTriggerThreshold(int handle, uint16_t threshold, int channel) {
	int ret;
	if (threshold < 16384) {
		ret = CAEN_DGTZ_WriteRegister(handle, 0x1060 | (channel << 8), (uint32_t)(threshold & 0x3FFF));
	} else {
		printf("invalid value for trigger threshold of channel %d\n", channel);
	}
	return ret;
}

int XX2530_DAW_SetPreTrigger(int handle, uint32_t samples, int channel) {
	int ret;
	if (samples < 254) {
		ret = CAEN_DGTZ_WriteRegister(handle, 0x1038 | (channel << 8), samples);
	} else {
		printf("invalid value for pretrigger sample number of channel %d\n", channel);
	}
	return ret;
}

int XX2530_DAW_SetBLineMode(int handle,uint32_t mode,int channel) {
	int ret;
	uint32_t regvalue;
	ret = CAEN_DGTZ_ReadRegister(handle, 0x1080 | (channel << 8), &regvalue);
	if (mode) ret |= CAEN_DGTZ_WriteRegister(handle, 0x1080 | (channel << 8), (regvalue & (0xff8fffff)));
	else ret |= CAEN_DGTZ_WriteRegister(handle, 0x1080 | (channel << 8), regvalue | 0x00100000);
	return ret;
}

int XX2530_DAW_SetBLineDefValue(int handle, uint32_t bl, int channel) {
	int ret;
	uint32_t regvalue;
	if (bl < 16384) {
		ret = CAEN_DGTZ_ReadRegister(handle, 0x1064 | (channel << 8), &regvalue);
		regvalue = (regvalue & (uint32_t)(~(0x00003fff))) | (uint32_t)(bl & 0x3fff); // replace only the two bits affecting the selected couple's logic.
		ret |= CAEN_DGTZ_WriteRegister(handle, 0x1064 | (channel << 8), regvalue);
	} else {
		printf("invalid value for default baseline of channel %d\n", channel);
	}
	return ret;
}

int XX2530_DAW_SelfTriggerEnable(int handle, int channel) {
	int ret;
	uint32_t regvalue;
	ret = CAEN_DGTZ_ReadRegister(handle, 0x1080 | (channel << 8), &regvalue);
	ret = CAEN_DGTZ_WriteRegister(handle, 0x1080 | (channel << 8), regvalue & ~(uint32_t)(0x1000000));

	return ret;
}

int XX2530_DAW_SelfTriggerDisable(int handle, int channel) {
	int ret;
	uint32_t regvalue;
	ret = CAEN_DGTZ_ReadRegister(handle, 0x1080 | (channel << 8), &regvalue);
	ret = CAEN_DGTZ_WriteRegister(handle, 0x1080 | (channel << 8), regvalue | (uint32_t)(0x1000000));
	return ret;
}
int XX2530_DAW_TestPulseEnable(int handle, int channel) {
	int ret;
	uint32_t regvalue;
	ret = CAEN_DGTZ_ReadRegister(handle, 0x1080 | (channel << 8), &regvalue);
	ret = CAEN_DGTZ_WriteRegister(handle, 0x1080 | (channel << 8), regvalue | (uint32_t)(0x100));
	return ret;
}

int XX2530_DAW_TestPulseDisable(int handle, int channel) {
	int ret;
	uint32_t regvalue;
	ret = CAEN_DGTZ_ReadRegister(handle, 0x1080 | (channel << 8), &regvalue);
	ret = CAEN_DGTZ_WriteRegister(handle, 0x1080 | (channel << 8), regvalue & ~(uint32_t)(0x100));
	return ret;
}

int XX2530_DAW_SetTestPulseType(int handle, uint32_t type) {
	int ret;
	uint32_t regvalue;
	ret = CAEN_DGTZ_ReadRegister(handle, 0x8000, &regvalue);
	if (type < 2) {
		ret |= CAEN_DGTZ_WriteRegister(handle, 0x8000, (regvalue & ~(0x00000008)) | (uint32_t)(type << 3));
	}
	else {
		printf("invalid value for test pulse type\n");
	}
	return ret;
}

int XX2530_DAW_SetPulsePolarity(int handle, uint32_t polarity, int channel) {
	int ret;
	uint32_t regvalue;
	ret = CAEN_DGTZ_ReadRegister(handle, 0x1080 | (channel << 8), &regvalue);
	(polarity) ? (regvalue = regvalue & ~(0x00010000)) : (regvalue = regvalue | 0x00010000);
	ret = CAEN_DGTZ_WriteRegister(handle, 0x1080 | (channel << 8), regvalue);
	return ret;
}

int OpenDigitizer(int *handle, DAWConfig_t  *ConfigVar)
{
	int Nboard, ret = 0;
	for (Nboard = 0; Nboard < ConfigVar->Nhandle; Nboard++) {
		printf("Board %d: ", Nboard);
		printf("link ID: %d;", ConfigVar->BoardConfigVar[Nboard]->LinkNum);
		printf(" Node ID: %d;", ConfigVar->BoardConfigVar[Nboard]->ConetNode);
		printf(" Base Address: %x\n", ConfigVar->BoardConfigVar[Nboard]->BaseAddress);
		if (ret |= CAEN_DGTZ_OpenDigitizer(ConfigVar->BoardConfigVar[Nboard]->LinkType, ConfigVar->BoardConfigVar[Nboard]->LinkNum,
			ConfigVar->BoardConfigVar[Nboard]->ConetNode, ConfigVar->BoardConfigVar[Nboard]->BaseAddress, handle + Nboard)) return ret;
	}
	return ret;
}

int ProgramDigitizers(int *handle, DAWConfig_t *ConfigVar) {
	int board;
	int i, ret = 0;
	uint32_t regval;
	for (board = 0; board < ConfigVar->Nhandle; board++) {
		ConfigVar->EnableHalf = 0;
		if (strstr(ConfigVar->BoardConfigVar[board]->BoardInfo.ModelName, "25") != NULL) ConfigVar->BoardConfigVar[board]->tSampl = 4;
		else if (strstr(ConfigVar->BoardConfigVar[board]->BoardInfo.ModelName, "30") != NULL) ConfigVar->BoardConfigVar[board]->tSampl = 2;
		else return -1;
		// mask possibly-configured channels that are not present in this digitizer model
		ConfigVar->BoardConfigVar[board]->EnableMask = ConfigVar->BoardConfigVar[board]->EnableMask & (uint16_t)((1 << ConfigVar->BoardConfigVar[board]->BoardInfo.Channels) - 1);
		// reset
		ret |= CAEN_DGTZ_Reset(handle[board]);
		ret |= CAEN_DGTZ_SetIOLevel(handle[board], ConfigVar->BoardConfigVar[board]->FPIOtype);
		ret |= CAEN_DGTZ_SetSWTriggerMode(handle[board], CAEN_DGTZ_TRGMODE_ACQ_ONLY);
		ret |= CAEN_DGTZ_SetExtTriggerInputMode(handle[board], ConfigVar->BoardConfigVar[board]->ExtTriggerMode);
		// trigmask/trigoutputmask?
		// Active channels
		ret |= CAEN_DGTZ_SetChannelEnableMask(handle[board], (uint32_t)ConfigVar->BoardConfigVar[board]->EnableMask);
		// gain control
		ret |= CAEN_DGTZ_WriteRegister(handle[board], 0x8028, ConfigVar->BoardConfigVar[board]->GainFactor);
		// max BLT events
		ret |= CAEN_DGTZ_SetMaxNumEventsBLT(handle[board], 1023);
		// test pulse type (exp decay or sawtooth)
		ret |= XX2530_DAW_SetTestPulseType(handle[board], ConfigVar->BoardConfigVar[board]->TP_Type);
		
		if (ConfigVar->SyncEnable) {
			// sets whether the LVDS quartets are input or output (bits [5:2]): 1st quartet is input, other outputs here
			// sets the LVDS "new" mode (bit 8)
			// TRG OUT is used to propagate signals (bits [17:16])
			// signal propagated through the trgout (bits [19:18]) is the busy signal
			// enable extended time-stamp (bits [22:21] = "10")
			// the other two quartets (not used) are also set to output
			ret |= CAEN_DGTZ_ReadRegister(handle[board], 0x811C, &regval);
			ret |= CAEN_DGTZ_WriteRegister(handle[board], 0x811C, regval | 0x4d0138);
			// acquisition mode is sw-controlled for the first board, LVDS-controlled for the others
			ret |= CAEN_DGTZ_ReadRegister(handle[board], 0x8100, &regval);
			ret |= CAEN_DGTZ_WriteRegister(handle[board], 0x8100, regval | (board == 0 ? 0x00000100 : 0x00000107));
			// register 0x816C: reduces the threshold at which the BUSY is raised at 2^buffer organization-10 events
			ret |= CAEN_DGTZ_ReadRegister(handle[board], 0x800C, &regval);
			ret |= CAEN_DGTZ_WriteRegister(handle[board], 0x816C, (uint32_t)(pow(2., regval) - 10));
			// register 0x8170: timestamp offset
			ret |= CAEN_DGTZ_WriteRegister(handle[board], 0x8170, 3 * (ConfigVar->Nhandle - board - 1) + (board == 0 ? -1 : 0));
			// register 0x81A0: select the lowest two quartet as "nBUSY/nVETO" type. BEWARE: set ALL the quartet bits to 2
			ret |= CAEN_DGTZ_ReadRegister(handle[board], 0x81A0, &regval);
			ret |= CAEN_DGTZ_WriteRegister(handle[board], 0x81A0, regval | 0x00002222);
		} else {
			// enable extended timestamp (bits [22:21] = "10")
			ret |= CAEN_DGTZ_ReadRegister(handle[board], 0x811C, &regval);
			ret |= CAEN_DGTZ_WriteRegister(handle[board], 0x811C, regval | 0x400000);
			// set acquisition mode
			ret |= CAEN_DGTZ_SetAcquisitionMode(handle[board], ConfigVar->BoardConfigVar[board]->StartMode);
			// register 0x8100: set bit 2 to 1 if not in sw-controlled mode
			ret |= CAEN_DGTZ_ReadRegister(handle[board], 0x8100, &regval);
			if (ConfigVar->BoardConfigVar[board]->StartMode == CAEN_DGTZ_SW_CONTROLLED) ret |= CAEN_DGTZ_WriteRegister(handle[board], 0x00008100, regval | 0x000100);
			else ret |= CAEN_DGTZ_WriteRegister(handle[board], 0x8100, regval | 0x00000104);
		}
		
		// channel-specific settings	
		for (i = 0; i < ConfigVar->BoardConfigVar[board]->BoardInfo.Channels; i++) {
			if (ConfigVar->BoardConfigVar[board]->EnableMask & (1 << i)) {
				// record length
				ret |= CAEN_DGTZ_SetRecordLength(handle[board], ConfigVar->BoardConfigVar[board]->RecordLength[i]); // ATTENZIONE: ricontrollare la funzione nelle CAENDIGI
																						   // set DC offset
				ret |= CAEN_DGTZ_SetChannelDCOffset(handle[board], i, ConfigVar->BoardConfigVar[board]->DCoffset[i]);
				// pretrigger
				ret |= XX2530_DAW_SetPreTrigger(handle[board], ConfigVar->BoardConfigVar[board]->preTrgg[i], i);
				//DAW baseline register
				ret |= XX2530_DAW_SetBLineMode(handle[board], ConfigVar->BoardConfigVar[board]->BLineMode[i], i);
				ret |= XX2530_DAW_SetBLineDefValue(handle[board], ConfigVar->BoardConfigVar[board]->BLineDefValue[i], i);
				//NSampAhead
				ret |= XX2530_DAW_SetPostSamples(handle[board], ConfigVar->BoardConfigVar[board]->NSampAhe[i], i);
				ret |= XX2530_DAW_SetMaxTail(handle[board], ConfigVar->BoardConfigVar[board]->MaxTail[i], i);
				//DAW Trigger Threshold
				ret |= XX2530_DAW_SetTriggerThreshold(handle[board], ConfigVar->BoardConfigVar[board]->DAWTrigThr[i], i);
				//DAW signal logic register
				ret |= XX2530_DAW_SetPulsePolarity(handle[board], ConfigVar->BoardConfigVar[board]->PulsePolarity[i], i);
				// self trigger enable
				ConfigVar->BoardConfigVar[board]->ST_Enable[i] ? ret |= XX2530_DAW_SelfTriggerEnable(handle[board], i) : XX2530_DAW_SelfTriggerDisable(handle[board], i);
				// pulse emulator
				ConfigVar->BoardConfigVar[board]->TP_Enable[i] ? ret |= XX2530_DAW_TestPulseEnable(handle[board], i) : XX2530_DAW_TestPulseDisable(handle[board], i);
				// lock temperature calibration
				if (ConfigVar->BoardConfigVar[board]->LockTempCalib) ret |= LockTempCalibration(handle[board], i);
			}
		}
		
		// Write register commands in the config file (possibily overwriting previous settings)
		for (i = 0; i < ConfigVar->BoardConfigVar[board]->GWn; i++) ret |= CAEN_DGTZ_WriteRegister(handle[board], ConfigVar->BoardConfigVar[board]->GWaddr[i], ConfigVar->BoardConfigVar[board]->GWdata[i]);
		// ADC calibration
		ret |= CAEN_DGTZ_WriteRegister(handle[board], 0x809c, 0x1);
		if (ret) printf("Warning: errors found during the programming of the digitizer.\nSome settings may not be executed\n");
	}
	return ret;
}

int PlotEvent(DAWConfig_t  *ConfigVar, DAWPlot_t *PlotVar, CAEN_DGTZ_730_DAW_Event_t *Event) {
	int comma = 0, c, npts = 0, WaitTime = 0;
	char fname[100];
	uint32_t s;
	FILE *fplot;
	sprintf(PlotVar->Title, "%s%d", "Waveforms Ch ",ConfigVar->EnableTrack);
	sprintf(PlotVar->TraceName[ConfigVar->EnableTrack], "Ch %d %s",ConfigVar->EnableTrack, "Input Signal");
	
	sprintf(fname, "%s%s", path, PLOT_DATA_FILE);
	if((fplot = fopen(fname, "w"))==NULL) {printf("file not allocated\n");return -1;}

	for (s = 0; s < 2*Event->Channel[ConfigVar->EnableTrack]->size; s++) {
		fprintf(fplot, "%d,\t", s);
		if (s < 2*Event->Channel[ConfigVar->EnableTrack]->size) {
			fprintf(fplot, "%u,\t", (Event->Channel[ConfigVar->EnableTrack]->DataPtr[s]&0x3fff)* PlotVar->Gain[ConfigVar->EnableTrack] + PlotVar->Offset[ConfigVar->EnableTrack]);
		} else {
			fprintf(fplot, " ,\t");
		}
		fprintf(fplot, "\n");
	}
	printf("plotted event counter : %u\n", Event->tcounter);
	printf("plotted event time    : %lld ns\n", Event->Channel[ConfigVar->EnableTrack]->timeStamp * ConfigVar->BoardConfigVar[ConfigVar->BoardPlotted]->tSampl);
	fclose(fplot);
	fprintf(PlotVar->plotpipe, "plot ");
	c = 2; // first data column
	if (comma) fprintf(PlotVar->plotpipe, ", ");
	fprintf(PlotVar->plotpipe, "'%s' using ($1*%f):($%d*%f) title '%s' with step lc %d",fname, PlotVar->Xscale, 2, PlotVar->Yscale, PlotVar->TraceName[ConfigVar->EnableTrack], 1 + ConfigVar->EnableTrack);
	comma = 1;

	fprintf(PlotVar->plotpipe, "\n");
	fflush(PlotVar->plotpipe);
	return 0;
}

void CheckKeyboardCommands(int *handle, DAWConfig_t  *ConfigVar)
{
	int c = 0, d = 0, ch = 0, i = 0, run = 0;
	int h2dcnt = 0;

	if (!kbhit()) return;
	c = getch();
	switch (c) {
	case 'q':
		ConfigVar->Quit = 1;
		break;
	case '1':	
		ConfigVar->EnableTrack = (ConfigVar->EnableHalf==0?0:8);
		break;
	case '2':
		ConfigVar->EnableTrack = (ConfigVar->EnableHalf == 0 ?1:9);
		break;
	case '3':
		ConfigVar->EnableTrack = (ConfigVar->EnableHalf == 0 ?2:10);
		break;
	case '4':
		ConfigVar->EnableTrack = (ConfigVar->EnableHalf == 0 ?3:11);
		break;
	case '5':
		ConfigVar->EnableTrack = (ConfigVar->EnableHalf == 0 ?4:12);
		break;
	case '6':
		ConfigVar->EnableTrack = (ConfigVar->EnableHalf == 0 ?5:13);
		break;
	case '7':
		ConfigVar->EnableTrack = (ConfigVar->EnableHalf == 0 ?6:14);
		break;
	case '8':
		ConfigVar->EnableTrack = (ConfigVar->EnableHalf == 0 ?7:15);
		break;
	case 't':
		if (!ConfigVar->ContTrigger) {
			for (i = 0; i < ConfigVar->Nhandle; i++) CAEN_DGTZ_SendSWtrigger(handle[i]);
			printf("Single Software Trigger issued\n");
		}
		break;
	case 'g':
		ConfigVar->EnableHalf = (ConfigVar->EnableHalf == 0 ? 1 : 0);
		printf("Channels from octet %d can be selected for plotting through keys 1-8\n", ConfigVar->EnableHalf);
		break;
	case 'p':
		ConfigVar->SinglePlot = 1;
		break;
	case 's':
		if (ConfigVar->AcqRun == 0) {
			for (i = 0; i < ConfigVar->Nhandle; i++) if(ConfigVar->BoardConfigVar[i]->StartMode == CAEN_DGTZ_SW_CONTROLLED) CAEN_DGTZ_SWStartAcquisition(handle[i]);
			printf("Acquisition started\n");
			// board is in RUN mode
			ConfigVar->AcqRun = 1;
		}
		else {
			for (i = 0; i < ConfigVar->Nhandle; i++) if (ConfigVar->BoardConfigVar[i]->StartMode == CAEN_DGTZ_SW_CONTROLLED) CAEN_DGTZ_SWStopAcquisition(handle[i]);
			printf("Acquisition stopped\n");
			ConfigVar->AcqRun = 0;
		}
		break;
	case '+':
		if (ConfigVar->BoardPlotted < ConfigVar->Nhandle - 1) ConfigVar->BoardPlotted++; else ConfigVar->BoardPlotted = 0;
		break;
	case '-':
		if (ConfigVar->BoardPlotted > 0) ConfigVar->BoardPlotted--; else ConfigVar->BoardPlotted = ConfigVar->Nhandle - 1;
		break;
	case 32:
		printf("\nBindkey help\n");
		printf("[q]          Quit\n");
		printf("[s]          Start/Stop acquisition\n");
		printf("[t]          Send a software trigger (single shot)\n");
		printf("[p]          Plot one event\n");
		printf("[g]          Switch octet for channel plotting selection\n");
		printf("[1-8]        Select plotted channel(if enabled) in the selected octet\n");
		printf("[+]/[-]      Plot the next/previous board (if any)\n");
		printf("[space]      This help\n\n");
		printf("Press a key to continue\n");
		getch();
		break;
	default:   break;
	}
}

ERROR_CODES OpenPlotter(DAWConfig_t *ConfigVar, DAWPlot_t *PlotVar)
{
	char str[1000];
	int i;
	strcpy(str, ConfigVar->GnuPlotPath);
	strcat(str, GNUPLOT_COMMAND);
	if ((PlotVar->plotpipe = popen(str, "w")) == NULL) return ERR_HISTO_MALLOC;

	strcpy(PlotVar->Title, "XX725/XX730 DAW Firmware Demo");
	strcpy(PlotVar->Xlabel, "us");
	strcpy(PlotVar->Ylabel, "ADC counts");
	for (i = 0; i<ConfigVar->BoardConfigVar[ConfigVar->BoardPlotted]->BoardInfo.Channels; i++) {
		PlotVar->Gain[i] = 1;
		PlotVar->Offset[i] = 0;
	}
	PlotVar->Xscale = (float)(ConfigVar->BoardConfigVar[ConfigVar->BoardPlotted]->tSampl / 1000.);
	PlotVar->Yscale = 1.0;
	PlotVar->Xmin = 0;
	PlotVar->Xmax = (float)(4 * ConfigVar->BoardConfigVar[ConfigVar->BoardPlotted]->RecordLength[ConfigVar->EnableTrack] * 5 / 4)*ConfigVar->BoardConfigVar[ConfigVar->BoardPlotted]->tSampl / 1000;
	PlotVar->Ymin = 0;
	PlotVar->Ymax = 16384;
	PlotVar->Xautoscale = 1;
	PlotVar->Yautoscale = 0;
	SetPlotOptions(PlotVar);
	return ERR_NONE;
}

void SetPlotOptions(DAWPlot_t *PlotVar)
{
	fprintf(PlotVar->plotpipe, "set grid\n");
	fprintf(PlotVar->plotpipe, "set mouse\n");
	fprintf(PlotVar->plotpipe, "set xlabel '%s'\n", PlotVar->Xlabel);
	fprintf(PlotVar->plotpipe, "set ylabel '%s'\n", PlotVar->Ylabel);
	fprintf(PlotVar->plotpipe, "set title '%s'\n", PlotVar->Title);
	fprintf(PlotVar->plotpipe, "Xs = %f\n", PlotVar->Xscale);
	fprintf(PlotVar->plotpipe, "Ys = %f\n", PlotVar->Yscale);
	fprintf(PlotVar->plotpipe, "Xmax = %f\n", PlotVar->Xmax);
	fprintf(PlotVar->plotpipe, "Ymax = %f\n", PlotVar->Ymax);
	fprintf(PlotVar->plotpipe, "Xmin = %f\n", PlotVar->Xmin);
	fprintf(PlotVar->plotpipe, "Ymin = %f\n", PlotVar->Ymin);
	if (PlotVar->Xautoscale) {
		fprintf(PlotVar->plotpipe, "set autoscale x\n");
		fprintf(PlotVar->plotpipe, "bind x 'set autoscale x'\n");
	}
	else {
		fprintf(PlotVar->plotpipe, "set xrange [Xmin:Xmax]\n");
		fprintf(PlotVar->plotpipe, "bind x 'set xrange [Xmin:Xmax]'\n");
	}
	if (PlotVar->Yautoscale) {
		fprintf(PlotVar->plotpipe, "set autoscale y\n");
		fprintf(PlotVar->plotpipe, "bind y 'set autoscale y'\n");
	}
	else {
		fprintf(PlotVar->plotpipe, "set yrange [Ymin:Ymax]\n");
		fprintf(PlotVar->plotpipe, "bind y 'set yrange [Ymin:Ymax]'\n");
	}
	fflush(PlotVar->plotpipe);
}

int ClosePlotter(FILE **gnuplot)
{
	if (*gnuplot != NULL)
		pclose(*gnuplot);
	return 0;
}

int UpdateTime(int RefreshTime, uint64_t *PrevTime) {
	uint64_t CurrentTime = get_time();
	if ((CurrentTime - *PrevTime) > RefreshTime) {
		*PrevTime = CurrentTime;
		return 1;
	}
	else {
		return 0;
	}
}

void PrintData(Counter_t *Counter, Counter_t *CounterOld, DAWConfig_t *ConfigVar) {

	double RoRate;
	double TrigRate;
	if (Counter->ByteCnt == CounterOld->ByteCnt) printf("No data...\n");
	else {
		float ElapsedTime = 8 * (float)(Counter->MB_TS - CounterOld->MB_TS) / (float)1000000000.;
		RoRate=Counter->ByteCnt!=CounterOld->ByteCnt? (float)(Counter->ByteCnt - CounterOld->ByteCnt) / (ElapsedTime * 1048576):0.;
		TrigRate = Counter->TrgCnt[ConfigVar->EnableTrack]!=CounterOld->TrgCnt[ConfigVar->EnableTrack] ?(float)(Counter->TrgCnt[ConfigVar->EnableTrack] - CounterOld->TrgCnt[ConfigVar->EnableTrack]) / (ElapsedTime*1000.):0.;
		printf("Board data rate       : %.2f MB/s\n", RoRate);
		if(ConfigVar->PlotEnable) printf("Event rate of ch.%*d   : %.2f KHz\n",2,ConfigVar->EnableTrack,TrigRate);
		*CounterOld = *Counter;
	}
}

uint32_t CheckMallocSize(int handle) {
	uint32_t size,lsize, maxEvents, nChEnabled = 0, u32;
	lsize = 0xffffff; // max length of an event
	if ((CAEN_DGTZ_GetChannelEnableMask(handle, &u32) != CAEN_DGTZ_Success) || // era COMMON_
		(CAEN_DGTZ_GetMaxNumEventsBLT(handle, &maxEvents) != CAEN_DGTZ_Success)) return 0;
	for (; u32 > 0; u32 >>= 1) if ((u32 & 0x1) != 0) nChEnabled++;
	size = 4 * ((((lsize * 2) + 3)*nChEnabled) + 4)*maxEvents;
	return size;
}

void ResetCounter(Counter_t *Counter) {
	int i;
	Counter->MB_TS=0;
	Counter->ByteCnt=0;
	for (i = 0; i < 16; i++) {
		Counter->TrgCnt[i];
		Counter->OFCnt[i];
	}
}

//  Check if plot has finished
/*int IsPlotterBusy()
{
	if (get_time() > Tfinish)
		Busy = 0;
	return Busy;
}*/

#define MAIN_MEM_PAGE_READ_CMD 0xD2
#define MAIN_MEM_PAGE_PROG_TH_BUF1_CMD 0x82
#define STATUS_READ_CMD 0xD7
#define READ_SECURITY_REGISTER_OPCODE 0x77
#define AT45DB08_PAGE_SIZE 264 // Number of bytes per page in the 4 Mbit flash
#define AT45DB32_PAGE_SIZE 528 // Number of bytes per page in the 32 Mbit flash
#define FLASH_PAGE_SIZE AT45DB32_PAGE_SIZE // operations on flash refer to 32 Mbit model;
// in case of old boards with 8 Mbit memories, 2 pages will be involved
#define REG_RW_FLASH_ADDRESS 0xEF30
#define REG_SEL_FLASH_ADDRESS 0xEF2C

// ---------------------------------------------------------------------------------------------------------
// Description: Write to internal SPI bus
// Inputs: handle = board handle
// ch = channel
// address = SPI address
// value: value to write
// Return: 0=OK, negative number = error code
// ---------------------------------------------------------------------------------------------------------
int WriteSPIRegister(int handle, uint32_t ch, uint32_t address, uint8_t value) {
	uint32_t SPIBusy = 1;
	int32_t ret = CAEN_DGTZ_Success;
	uint32_t SPIBusyAddr = 0x1088 + (ch << 8);
	uint32_t addressingRegAddr = 0x80B4;
	uint32_t valueRegAddr = 0x10B8 + (ch << 8);
	while (SPIBusy) {
		if ((ret = CAEN_DGTZ_ReadRegister(handle, SPIBusyAddr, &SPIBusy)) != CAEN_DGTZ_Success)
			return CAEN_DGTZ_CommError;
		SPIBusy = (SPIBusy >> 2) & 0x1;
		if (!SPIBusy) {
			if ((ret = CAEN_DGTZ_WriteRegister(handle, addressingRegAddr, address)) != CAEN_DGTZ_Success)
				return CAEN_DGTZ_CommError;
			if ((ret = CAEN_DGTZ_WriteRegister(handle, valueRegAddr, (uint32_t)value)) != CAEN_DGTZ_Success)
				return CAEN_DGTZ_CommError;
		}
		Sleep(1);
	}
	return CAEN_DGTZ_Success;
}

// ---------------------------------------------------------------------------------------------------------
// Description: Read from internal SPI bus
// Inputs: handle = board handle
// ch = channel
// address = SPI address
// Output value: read value
// Return: 0=OK, negative number = error code
// ---------------------------------------------------------------------------------------------------------
int ReadSPIRegister(int handle, uint32_t ch, uint32_t address, uint8_t *value) {
	uint32_t SPIBusy = 1;
	int32_t ret = CAEN_DGTZ_Success;
	uint32_t SPIBusyAddr = 0x1088 + (ch << 8);
	uint32_t addressingRegAddr = 0x80B4;
	uint32_t valueRegAddr = 0x10B8 + (ch << 8);
	uint32_t val;
	while (SPIBusy) {
		if ((ret = CAEN_DGTZ_ReadRegister(handle, SPIBusyAddr, &SPIBusy)) != CAEN_DGTZ_Success)
			return CAEN_DGTZ_CommError;
		SPIBusy = (SPIBusy >> 2) & 0x1;
		if (!SPIBusy) {
			if ((ret = CAEN_DGTZ_WriteRegister(handle, addressingRegAddr, address)) != CAEN_DGTZ_Success)
				return CAEN_DGTZ_CommError;
			if ((ret = CAEN_DGTZ_ReadRegister(handle, valueRegAddr, &val)) != CAEN_DGTZ_Success)
				return CAEN_DGTZ_CommError;
		}
		*value = (uint8_t)val;
		Sleep(1);
	}
	return CAEN_DGTZ_Success;
}

int LockTempCalibration(int handle, int ch) {
	uint8_t lock, ctrl;
	int ret = 0;
	// enter engineering functions
	ret |= WriteSPIRegister(handle, ch, 0x7A, 0x59);
	ret |= WriteSPIRegister(handle, ch, 0x7A, 0x1A);
	ret |= WriteSPIRegister(handle, ch, 0x7A, 0x11);
	ret |= WriteSPIRegister(handle, ch, 0x7A, 0xAC);
	// read lock value
	ret |= ReadSPIRegister(handle, ch, 0xA7, &lock);
	// write lock value
	ret |= WriteSPIRegister(handle, ch, 0xA5, lock);
	// enable lock
	ret |= ReadSPIRegister(handle, ch, 0xA4, &ctrl);
	ctrl |= 0x4; // set bit 2
	ret |= WriteSPIRegister(handle, ch, 0xA4, ctrl);
	ret |= ReadSPIRegister(handle, ch, 0xA4, &ctrl);
	return ret;
}
