#ifndef _DAWDEMOCONST
#define _DAWDEMOCONST

#define MAX_CH                  16   /* max. number of channels */
#define MAX_GW                1000   /* max. number of generic write commads */
#define GNUPLOT_DEFAULT_PATH    ""
#define MAX_NUM_TRACES          16   /* Maximum number of traces in a plot */
#define BASE_HEADER_SIZE         4   /* Base header size in 32-bit words*/
#define CHANNEL_HEADER_SIZE      1   /* Channel header size in 32-bit words*/
#define MB_SIZE                1048576

#define PLOT_DATA_FILE   "PlotData.txt"
#define GNUPLOT_COMMAND  "gnuplot"
#define OUTFILE_NAME     "run0"

#define DAW_FW_ID        0x8D

#ifdef LINUX
#define OUTFILE_PATH         "/DAW_output/"
#define DEFAULT_CONFIG_FILE  "/etc/DAW_Demo/DAW_Config.txt"
#define PLOTTER_PATH         ""
#else
#define OUTFILE_PATH         "DAW_output\\"
#define DEFAULT_CONFIG_FILE  "DAW_Config.txt"
#define PLOTTER_PATH         "\\"
#endif

/* Error messages */
typedef enum {
	ERR_NONE = 0,
	ERR_CONF_FILE_NOT_FOUND,
	ERR_PARSE_CONFIG,
	ERR_DGZ_OPEN,
	ERR_WRONG_FW,
	ERR_BOARD_INFO_READ,
	ERR_INVALID_BOARD_TYPE,
	ERR_DGZ_PROGRAM,
	ERR_MALLOC,
	ERR_RESTART,
	ERR_INTERRUPT,
	ERR_READOUT,
	ERR_EVENT_BUILD,
	ERR_HISTO_MALLOC,
	ERR_UNHANDLED_BOARD,
	ERR_OUTFILE_OPEN,
	ERR_OUTDIR_OPEN,
	ERR_OUTFILE_WRITE,
	ERR_DUMMY_LAST,
} ERROR_CODES;

static char ErrMsg[ERR_DUMMY_LAST][100] = {
	"No Error",                                                       /* ERR_NONE */
	"Configuration File not found",                                   /* ERR_CONF_FILE_NOT_FOUND */
	"One or more parameters in the config file were not recognised",  /* ERR_PARSE_CONFIG */
	"Can't open the digitizer",                                       /* ERR_DGZ_OPEN */
	"No DAW firmware on the board",                                   /* ERR_WRONG_FW */
	"Can't read the Board Info",                                      /* ERR_BOARD_INFO_READ */
	"Can't run DPPrunner for this digitizer",                         /* ERR_INVALID_BOARD_TYPE */
	"Can't program the digitizer",                                    /* ERR_DGZ_PROGRAM */
	"Can't allocate the memory for the readout buffer",               /* ERR_MALLOC */
	"Restarting Error",                                               /* ERR_RESTART */
	"Interrupt Error",                                                /* ERR_INTERRUPT */
	"Readout Error",                                                  /* ERR_READOUT */
	"Event Build Error",                                              /* ERR_EVENT_BUILD */
	"Can't allocate the memory for the histograms",                   /* ERR_HISTO_MALLOC */
	"Unhandled board type",                                           /* ERR_UNHANDLED_BOARD */
	"Output directory open error",									  /* ERR_OUTDIR_OPEN*/
	"Output file open error",                                         /* ERR_OUTFILE_OPEN */
	"Output file write error",                                        /* ERR_OUTFILE_WRITE */
};

typedef struct {

	// Parameters for the communication
	int LinkType;
	int LinkNum;
	int ConetNode;
	uint32_t BaseAddress;
	// Parameters for the plot
	int tSampl;
	uint16_t EnableMask;
	int LockTempCalib;
	CAEN_DGTZ_BoardInfo_t BoardInfo;
	// Parameters for the digitizer
	CAEN_DGTZ_IOLevel_t FPIOtype;
	CAEN_DGTZ_TriggerMode_t ExtTriggerMode;
	CAEN_DGTZ_AcqMode_t StartMode;
	uint32_t GainFactor;
	uint32_t RecordLength[MAX_CH];
	int TP_Type;
	int NSampAhe[MAX_CH];
	int MaxTail[MAX_CH];
	int DAWTrigThr[MAX_CH];
	int BLineDefValue[MAX_CH];
	int BLineMode[MAX_CH];
	int SigLogicConfig[MAX_CH];
	int DCoffset[MAX_CH];
	int preTrgg[MAX_CH];
	int PulsePolarity[MAX_CH];
	int TP_Enable[MAX_CH];
	int ST_Enable[MAX_CH];
	// Generic write accesses to the registers
	int GWn;
	uint32_t GWaddr[MAX_GW];
	uint32_t GWdata[MAX_GW];
	// Variables for the acquisition and run control
	
} DAWBoardConfig_t;

typedef struct {
	int Nhandle;
	int AcqRun;
	int Quit;
	int ContTrigger;
	// Parameters for the plot and the output file
	int OFRawEnable; // Save raw data
	int OFWaveEnable; // Save histograms
	char OutFilePath[200]; // Output file path
	char OutFileName[200]; // Output file name
	char GnuPlotPath[200]; // gnuplot exec path
	int MaxFileSize;
	int PlotEnable;
	int PlotRefreshTime;
	int SinglePlot;
	int SyncEnable;
	uint16_t EnableTrack;
	uint16_t EnableHalf;
	uint16_t BoardPlotted;
	DAWBoardConfig_t *BoardConfigVar[1000];
} DAWConfig_t;



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
	int               TraceEnable[MAX_NUM_TRACES];
	int               Gain[MAX_NUM_TRACES];
	int               Offset[MAX_NUM_TRACES];
	FILE              *plotpipe;
} DAWPlot_t;

typedef struct {
	uint64_t MB_TS;
	int ByteCnt;
	int TrgCnt[16];
	int OFCnt[16];
} Counter_t;

#endif