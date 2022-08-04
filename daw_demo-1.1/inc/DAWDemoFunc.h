#ifndef _DAWDEMOFUNC_H_
#define _DAWDEMOFUNC_H_

#include <CAENDigitizer.h>
#include <CAENDigitizerType.h>
#include <time.h>
#include <sys/timeb.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "DAWDemoConst.h"

#ifdef WIN32
#include <time.h>
#include <sys/timeb.h>
#include <conio.h>
#include <process.h>
#include <windows.h>
#define popen  _popen    /* redefine POSIX 'deprecated' popen as _popen */
#define pclose _pclose  /* redefine POSIX 'deprecated' pclose as _pclose */
#define getch  _getch     /* redefine POSIX 'deprecated' */
#define kbhit  _kbhit     /* redefine POSIX 'deprecated' */
#else
#include <unistd.h>
#include <stdint.h>   /* C99 compliant compilers: uint64_t */
#include <ctype.h>    /* toupper() */
#include <sys/time.h>
#define Sleep(t) usleep((t)*1000);
#endif

#define CAL_PAGE_NUMBER					2		// page number containing the calibration values (referred to 32 Mbit memories)

static long get_time();
ERROR_CODES OpenConfigFile(FILE **f_ini, char *ConfigFileName);
ERROR_CODES ParseConfigFile(FILE *f_ini, DAWConfig_t *ConfigVar);
ERROR_CODES OpenPlotter(DAWConfig_t *ConfigVar, DAWPlot_t *PlotVar);
void SetPlotOptions(DAWPlot_t *PlotVar);
int OpenDigitizer(int *handle, DAWConfig_t  *ConfigVar);
int ProgramDigitizers(int *handle, DAWConfig_t *ConfigVar);
int PlotEvent(DAWConfig_t  *ConfigVar, DAWPlot_t *PlotVar, CAEN_DGTZ_730_DAW_Event_t *Event);
void CheckKeyboardCommands(int *handle, DAWConfig_t  *ConfigVar);
int ClosePlotter(FILE **gnuplot);
void ResetCounter(Counter_t *Counter);
int UpdateTime(int RefreshTime, uint64_t *PrevRateTime);
void PrintData(Counter_t *Counter, Counter_t *CounterOld,DAWConfig_t *ConfigVar);
ERROR_CODES OpenRawFile(FILE **outfile, int BoardIndex, int FileIndex, char *path, char *fname);
ERROR_CODES OpenWaveFile(FILE ***outfile, int BoardIndex, DAWBoardConfig_t *BoardConfigVar, char *path, char *fname);
void WaveWrite(FILE **WaveFile, CAEN_DGTZ_730_DAW_Event_t *Event, DAWBoardConfig_t *BoardConfigVar);
uint32_t CheckMallocSize(int handle);
int XX2530_DAW_SetPostSamples(int handle, uint32_t samples, int channel);
int XX2530_DAW_SetMaxTail(int handle, uint32_t tail, int channel);
int XX2530_DAW_SetTriggerThreshold(int handle, uint16_t threshold, int channel);
int XX2530_DAW_SetPreTrigger(int handle, uint32_t samples, int channel);
int XX2530_DAW_SetBLineMode(int handle, uint32_t mode, int channel);
int XX2530_DAW_SetBLineDefValue(int handle, uint32_t bl, int channel);
int XX2530_DAW_TestPulseEnable(int handle, int channel);
int XX2530_DAW_TestPulseDisable(int handle, int channel);
int XX2530_DAW_SelfTriggerEnable(int handle, int channel);
int XX2530_DAW_SelfTriggerDisable(int handle, int channel);
int XX2530_DAW_SetPulsePolarity(int handle, uint32_t polarity, int channel);
int XX2530_DAW_SetTestPulseType(int handle, uint32_t type);
int WriteSPIRegister(int handle, uint32_t ch, uint32_t address, uint8_t value);
int ReadSPIRegister(int handle, uint32_t ch, uint32_t address, uint8_t *value);
int LockTempCalibration(int handle, int ch);
#endif