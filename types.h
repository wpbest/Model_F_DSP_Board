// types.h

#ifndef __LOCAL_TYPES_H // inclusion guard
#define __LOCAL_TYPES_H

#include "F2806x_Device.h"
#define uint8_t unsigned char
#define uint8_t unsigned char
#define uint16_t unsigned int
#define uint unsigned int
#define uint32_t unsigned long
#define ulong unsigned long
#define ubyte unsigned char
#define bool unsigned char

// Octal latch definitions

#define LATCH_ADR0 41
#define LATCH_ADR1 34
#define LATCH_ADR2 53

#define LATCH_DAT0 33
#define LATCH_DAT1 32
#define LATCH_DAT2 22
#define LATCH_DAT3 24
#define LATCH_DAT4 13
#define LATCH_DAT5 58
#define LATCH_DAT6 57
#define LATCH_DAT7 3

#define LATCH_CLK 20
#define LATCH_ENA 54

// upsState definitions
#define TEMP_BUFFER_LENGTH 100
#define PARAM_NUM_MAX 25
#define PARAM_LEN_MAX 15
#define IDENT_LEN_MAX 50
#define ALM_MASK_LEN_MAX 15
#define CMD_STR_MAX 15
#define MAX_MASTER_CMDS 20

typedef struct _timeT
{
	int days;
	long msec;
	long counts;
	float usec;
} timeT;

typedef enum
{
	DIAGNOSTIC_TEST = 1001,
	DIAGNOSTIC_TYPE_1,
	DIAGNOSTIC_TYPE_2,
	DIAGNOSTIC_END
} diagnosticNum_T;
// Typedef for commands sent to a *_Control()
typedef struct
{
	Uint16 nCommand;		 // Command to execute
	void *pData;			 // Pointer to data (could be NULL, could be original pointer)
	Uint16 nDataLen;		 // Length of the data field
	Uint32 nInfo;			 // Extra info, such as NVRAM address, needed by command
	int16 nStatus;			 // Status of the operation
	Uint16 bCommandDone;	 // Flag indicating when the command is completed
	long nTimeout;			 // Timeout waiting for the command to complete (if non-zero)
	timeT nStartTime; // Used to hold the start time of the command for a timeout
} sCommandEvent;

typedef enum
{
	UPS_COM_SNMP,
	UPS_COM_UPSILON,
	UPS_COM_USER
} upsComModesT;

typedef enum
{
	OFF, // 0
	ON,
	AUTO,
	MANUAL,
	AUTO_OFF,
	AUTO_ON, // 5
	AUTO_SLOW,
	AUTO_MED,
	AUTO_FAST,
	AUTO_RAMP,
	MANUAL_OFF, // 10
	MANUAL_ON,
	MANUAL_SLOW,
	MANUAL_MED,
	MANUAL_FAST,
	MANUAL_RAMP, // 15
	ON_ALARM,	// from this point on there isn't Model F equivalent mode expressed in RDAT03
	OFF_ALARM,
	NORMAL,
	WARN,
	FAULT, // 20
	AUTO_ON_ALM,
	AUTO_ON_SHUTDOWN,
	ON_BEEP,
	ON_SOLID,
	SINGLE_BEEP, // 25
	UPDATE,
	LIGHT_SHOW,
	LIGHTS_ON,
	LIGHTS_OFF,
	TEST,
	CONFIGURATION,
	INV_120V,
	INV_240V, // 30
	NONE,
	LAST,
	NULL_MODE = -1 // mode not found or invalid
} operatingModesT;

typedef enum
{
	CYCLIC_CHARGE_WAIT,
	CYCLIC_CHARGE_START,
	CYCLIC_CHARGE_OFF,
	CYCLIC_CHARGE_ON,
	CYCLIC_CHARGE_ALARM
}	cyclicChargeModesT;
// Battery State reported to SNMP "Battery Charge"
typedef enum
{
	SNMP_BAT_CHARGE_FLOATING = 0,
	SNMP_BAT_CHARGE_CHARGING = 1,
	SNMP_BAT_CHARGE_RESTING = 2,
	SNMP_BAT_CHARGE_DISCHARGING = 3
} snmpBatChargeStateT;

typedef enum
{
	BUTTON_ON,
	BUTTON_OFF,
	BUTTON_SILENCE,
	BUTTON_TEST,
	BUTTON_FUNCTION,
	BUTTON_BANKS,
	BUTTON_BYPASS,
	BUTTON_NONE
} buttonT;

typedef enum
{
	UPS_INIT,		  // Initial state when router/system powers up
	UPS_OFF,		  // System powered but Inverter off
	UPS_STARTUP,	  // Sequence the inverter on, then the outlets on based on customer times
	UPS_ON_BAT,		  // On battery, inverter on
	UPS_ON_UTIL,	  // On Utility, inverter on
	UPS_BYPASS,		  // On bypass
	UPS_SHUTDOWN,	 // Shutdown, transition state
	UPS_FAULT,		  // System powered, inverter off, indicating fault
	UPS_COM_SHUTDOWN, // System cutthroat, inverter off, indicating fault
	UPS_DEBUG_START, // Debug Start mode
	UPS_NULL
} ups_states_t;

// Map Bank Switch Information into a Uint32 structure
typedef struct BankSwitchConfiguration // Bits     Description
{
	// BANK 1 =======================================================================================================================================
	uint32_t VERTICAL_DISPLAY : 1;				   // 00       B1-01       Front Panel Display Orientation 1 = vertical, 0 = horizontal 
	uint32_t AUTOSTART : 1;						   // 01       B1-02       Autostart 1 = Energizes output on startup, 0 = not
	uint32_t LOW_BAT_ALM : 1;					   // 02       B1-03       Low Battery Alarm 1 = Timed, 0 = Calculated. 
	uint32_t LOW_BAT_ALM_SET : 2;				   // 03,04    B1-04,05    Low Battery Alarm Set 00=1800 seconds, 01=7200, 10=3600, 11=10800
	uint32_t RESERVED1 : 1; 					   // 05       B1-06       Reserved
	uint32_t RESERVED2 : 1;   					   // 06       B1-07       Reserved
	uint32_t RESERVED3 : 1;						   // 07       B1-08       Reserved
	uint32_t RS232_BAUD_RATE : 2;				   // 08,09    B1-09,10    BrightCom BaudRate 00=1200, 01=4800, 10=2400, 11=9600
	// BANK 2 =======================================================================================================================================
	uint32_t REMOTE_ON_OFF : 2;					   // 10,11    B2-01,02    Remote On/Off function 0 = Turn Off Enabled, 1 = Disabled, 2 = Turn On/Off Enabled, 3 = Turn On Enabled
	uint32_t REMOTE_DELAY : 2;					   // 12,13    B2-03,04    Time Delay Remote On/Off Turn off delay (seconds), 0 = 0, 1 = 300, 2 = 120, 3 = 900
	uint32_t BATTERY_TEST_INTERVAL : 1;			   // 14       B2-05       Internal Battery Test 0 = 1 Month, 1 = 3 Months, not re-implemented until battery test function implemented
	uint32_t OUTPUT_VOLT_SET : 2;				   // 15,16    B2-06,07    Output Voltage Selection 00 = 100, 01 = 110, 10 = 115, 11 = 120
	uint32_t ALLOW_REMOTE_OFF : 1;				   // 17       B2-08       Allow Remote Off 0 = Battery only, 1 = Utility and Battery
	uint32_t INV_SYNC_TO_LINE : 1;                 // 18	   B2-09       Use to indicate inverter will synchronize with line frequency and phase, 0=Don't sync, 1=Sync
	uint32_t NARROW_INPUT_FREQ_RANGE : 1;		   // 19       B2-10       Frequency Sync Range 1 = Normal (narrow) [+/- 1.5Hz], 0 = Wide [+/- 3Hz]
	// BANK 3 =======================================================================================================================================
	uint32_t BYPASS_STARTUP : 1;				   // 20       B3-01       Bypass During Inverter Start-up 1 = Bypass off during startup, 0 = Bypass on
	uint32_t DYNAMIC_BYPASS : 1;				   // 21       B3-02       Dynamic Bypass 1 = Disabled, 0 = Enabled
	uint32_t INPUT_BAT_SETPOINT : 2;			   // 22,23	   B3-03,04    Utility Voltage on/off battery, 0 = 85/95, 1 = 85/90, 2 = 80/85, 3 = 100/107
	uint32_t LOOP_RESPONSE:1;                      // 34	   B3-05       Transfer to battery setpoints, 1 = Slow, 0 = Fast
	uint32_t SMART_BAT_SETPOINT : 3;               // 25,26,27 B3-06,07,08 Smart Battery Setpoints
	uint32_t REMOTE_MANUAL_START : 1;              // 28       B3-09       Remote Manual Start 1 = Startup in Manual, 0 = Startup in Auto
	uint32_t THOR : 1;				               // 29       B3-10       Factory Reset (THOR) 0 = Do nothing, 1 = Thor on startup
	uint32_t REMOTE_INPUT : 2;                     // 30,31	               Not in old bank switches
} BankSwitchConfiguration_t;

typedef union BankSwitchConfigurationBits {
	Uint32 all;
	BankSwitchConfiguration_t bit;
} BankSwitchConfigurationBits_t;

// Configuration saved and loaded from SPI Flash and changed when system is running
struct SERIAL_FLASH_CONFIG1 // Bits 	Description
{
	uint16_t SWITCH_120_240 : 1; // 0		0 = 120V switch, 1 = 240V
	uint16_t RESERVED : 15;		 // 1-15	Reserved
};

union SERIAL_FLASH_CONFIG_T // Bits 	Description
{
	uint16_t all;
	struct SERIAL_FLASH_CONFIG1 bit;
};

struct BANK_CONFIG_T // used in upsData structure
{
	int vertical, autoRestart, lowBatAlarmTimer, lowBatTime; // vertical doesn't change LED mapping for model F, only Model C
	int inputWireFaultStartupInhibit, inputWireFaultAlarmInhibit;
	int baudRate, remoteCurrentLoopInputMode;
	int remoteCurrentLoopTurnoffDelay, batteryTestInterval;
	int outputVoltSet, allowRemoteOff, narrowInputFrequencyRange;
	int bypassStartup, dynamicBypass, inputBatterySetpoint;
	int loopResponse, smartBatterySetpoint, manualStart, thor;
};

// events
enum eventStates_E // used by event state machine in ups.c
{
	EVENT_INIT = 0,
	EVENT_WRITE,
	EVENT_WRITE_WAIT,
	EVENT_READ,
	EVENT_READ_WAIT,
	EVENT_STATUS,
	EVENT_NONE
};

struct event_T
{
	timeT time;
	int eventType;
};

#define NUM_OF_EVENTS 100

extern struct event_T events[NUM_OF_EVENTS];
extern long eventStart, eventEnd, eventTotal;

// Map these messages to string array eventTable
typedef enum
{
	EVENT_INVERTER_CURRENT_SD,
	EVENT_OVERLOAD_CURRENT_SD,
	EVENT_INVERTER_CURRENT_ALARM,
	EVENT_INVERTER_POWER_SD,
	EVENT_INVERTER_POWER_ALARM,
	EVENT_INVERTER_FAULT,
	EVENT_INVERTER_ON,
	EVENT_INVERTER_OFF,
	EVENT_HEATSINK_TEMP_SD,         // Issued when heat sink temp is at or above TEMP_HS_TRIP_ON
	EVENT_HEATSINK_TEMP_ALARM,      // Issued when heat sink temp is at or above TEMP_HS_ALM_ON but below TEMP_HS_TRIP_ON
	EVENT_AMBIENT_TEMP_SD,
	EVENT_AMBIENT_TEMP_ALARM,
	EVENT_ON_BATTERY,
	EVENT_BATTERY_LOW_ALARM,
	EVENT_BATTERY_TEST,
	EVENT_BATTERY_WARN,
	EVENT_BATTERY_FAIL,
	EVENT_CHGR_OVERVOLT,	        // Charger over-volt on startup, batteries unplugged
	EVENT_FAST_CHARGE,
	EVENT_FLOAT_CHARGE,
	EVENT_NO_CHARGE,
	EVENT_ON_UTILITY,
	EVENT_ON_BYPASS,
	EVENT_OFF_BYPASS,
	EVENT_AUTO_BYPASS,
	EVENT_COLD_START,               // start up from de-energized state
	EVENT_BATTERY_COLD_START,       // start up on batteries
	EVENT_SYSTEM_RESET,
	EVENT_SYSTEM_SHUTDOWN,
	EVENT_BATTLESHORT_ON,
	EVENT_BATTLESHORT_OFF,
	EVENT_UTILITY_OV,
	EVENT_NON_VOL_MEMORY_ERROR,
	EVENT_REMOTE_HIGH,
	EVENT_REMOTE_LOW,
	EVENT_SIG_RELAY_AC_ON,
	EVENT_SIG_RELAY_AC_OFF,
	EVENT_RS232_TIMEOUT,
	EVENT_TEST_COMMAND,
	EVENT_END // End of defined events
} eventNum_T;


// This structure is the central data hub of the program, all communication is done through string
// variables in this structure, all transactions are tracked, all parameters, errors, etc.
extern struct upsDataStrucT
{
	int upsNumber;
	ups_states_t upsState, lastUpsState;
	upsComModesT upsComMode, upsComModeLast;
	//comStates_t upsComState, lastUpsComState;
	//comWaitStates_t ComWaitState, lastComWaitState;
	int SubState, SubStateLast; // sequential states within states, breaking states down into phases
	//upsParsePollEnum_t currentCmd;                    // either timedCmd or masterCmd
	//int rotatingCmdHold;                              // Active during startup if commanding another UPS
	operatingModesT invMode, invRangeMode, dcMode, chgMode, syncMode, fanMode, batMode;
	operatingModesT tAmbMode, tSinkMode, powOutMode, ampOutMode; // TODO get tAmbMode warn/fault, only have alarm
	float voltIn, pfcAmpIn, freqIn, periodIn, voltBus, voltOut, ampOut, powOut, vaOut, pfOut, freqOut, loadPctOut;
	float ampBat, ampChg, voltBat, batChgPct;
	long batteryJoules;
	long numberOfChargeCycles;
	float voltSupply, ampSupply;
	float tAmb, tSink1, tSink2, hiVoltTrans, lowVoltTrans, invVoltSet;
	float tPfc, voltExt1, voltExt2, tExt1, tExt2;
	int invZeroCrossFlag;
	float zeroCrossDiffMsec;
	operatingModesT inBadAlm, outBadAlm, invFaultAlm, outOffAlm;
	operatingModesT overloadAlmtempAlm, audAlmMode, autoStartMode;
	operatingModesT inverterFault, inverterCommand, eventDump;
	operatingModesT chgCommand;
	operatingModesT bypassCommand, bypassMode, displayMode;
	char msgStr[TEMP_BUFFER_LENGTH];
	char msgChr;									 // last character from buffer
	int nextChrPos;									 // position in string next character will take, empty = 0
	char StrTemp[TEMP_BUFFER_LENGTH];				 // use this to edit a string, ex. pulling comma delimited response apart
	int nextTempChrPos;								 // position in string next character will take, empty = 0
	char StrParams[PARAM_NUM_MAX][PARAM_LEN_MAX];	//	when parsed each parameter will be in a string form up to 20 params
	int lastParam;									 //	last parameter location, -1 if none, 0 if 1
	char masterCmdStr[MAX_MASTER_CMDS][CMD_STR_MAX]; //	This holds the commands sent to the UPS's in a circular buffer
	//upsParsePollEnum_t masterCmd[MAX_MASTER_CMDS];	// when masterCmdStr[x] gets sent, masterCmd[x] is used to process response
	int masterCmdBot, masterCmdTop, masterCmdTot; //	pointers and total of master commands in buffer
	long checksumMsg, checksumCalc;				  // checksum of parsed message, if active
	//checksum_t checksumMode;							// Indicates when we are receiving a checksum
	int timedCmd; // number of cmd pending from rotating list
	//int startupInvCmd;								// Flag indicating an inverter startup command has been issued
	int notifyMsg;					// in process of receiving notification from ups, button press, status change, etc.
	char almMask[ALM_MASK_LEN_MAX]; // alarm string sent to UPS, 9 char and null
	int comOkay, comErrors;
	timeT timeCmdMade, timeOutStart; // timeCmdMade used for next time, timeOut for waiting for response
	timeT timeSystemStart, timeBatRunStarted;
	timeT displayUpdateTimer, vinZeroCross, invZeroCross, zeroCrossDiff;
	struct event_T event;
	float batteryJoulesIncrement, lowBatAlarm, lowBatShutdown;
	float freqInNom, voltInNom, timeLoBatNom, freqOutNom, powOutNom, voltOutNom, vaOutNom, voltBatNom;
	long secStartupDelay, secShutdownDelay, baudrate; // secShutdownDelay 1-999 sec, 0=off
	// sends message indicating operational change including parameters changing more than 5%
	operatingModesT notifyCustomerMsg;
	int notifyEvent;
	buttonT displayButton, displayButtonCmd;
	long durSecReboot, secOnBat, estSecBat, secBatWarning, lastBatteryRuntime;
	char estTimeStr[PARAM_LEN_MAX];
	int batCond, batSts, linesNumIn, battleshort, battleshortLocal, battleshortSNMP, fanFail;
	snmpBatChargeStateT batChgMode;
	int linesNumOut, sourceOutMode, shutdownType, testType; // shutdownType 0=none, 1=type 1, 2=type2
	int remoteTapSelected, delayedShutdown, delayedStartup, inverterOvercurrent, inverterOvercurrentCount;
	char upsId[IDENT_LEN_MAX], man[IDENT_LEN_MAX], model[IDENT_LEN_MAX], verSoftware[IDENT_LEN_MAX];
	//struct BANK_CONFIG_T bankConfig;
	long /* bank1, bank2, bank3, */ ledStatus[4], ledState1[4][4], ledState2[4][4];
	// Configuration saved and loaded from SPI Flash and changed when system is running
	union SERIAL_FLASH_CONFIG_T serialFlashConfig1, serialFlashConfig1Saved;
	volatile timeT dynamicBypassTimer;
	int verboseDebug, verboseDebugSync;
	uint8_t bypassState; // need to refactor later
	uint8_t updateLastBRT;
} upsDataStrucT;

extern struct upsDataStrucT upsOne, upsTwo, upsBoss, *pUpsOne, *pUpsTwo, *pUpsBoss;
extern struct upsDataStrucT *pUpsMain, *pUps, *pUpsDisplay, *pUpsLcd;

extern struct upsDataStrucUnfilteredT
{
	float ampBat, ampChg, voltBat, batChgPct, freqIn, voltIn, pfcAmpIn;
	float freqOut, loadPctOut, powOut, voltOut, vaOut, pfOut, ampOut, tAmb, tSink1, tSink2;
	float tPfc, voltExt1, voltExt2, tExt1, tExt2;
	float voltBus, voltBusFast, voltSupply, ampSupply;
} upsDataStrucUnfilteredT;

extern struct upsDataStrucUnfilteredT upsBossUnfilt;

extern volatile struct adcRawT
{
	long voltOut, ampOut, powOut, pfcAmpIn, voltIn, ampChg, ampBat, voltBus, voltBusFast, voltBat;
	long tAmb, tSink1, tSink2, tPfc, voltExt1, voltExt2, tExt1, tExt2;
} adcRawT;

extern volatile struct adcRawT adcRawNow, adcRawLast;
//struct adcRawT adcRawNow, adcRawLast;

extern volatile int adcDone;

// common values
#define TRUE 0x01
#define FALSE 0x00

/************************************************************
* STANDARD BITS
************************************************************/
/* // defined in F2806x_Device.h
#define BIT0	(0x0001)
#define BIT1	(0x0002)
#define BIT2	(0x0004)
#define BIT3	(0x0008)
#define BIT4	(0x0010)
#define BIT5	(0x0020)
#define BIT6	(0x0040)
#define BIT7	(0x0080)
#define BIT8	(0x0100)
#define BIT9	(0x0200)
#define BIT10	(0x0400)
#define BIT11	(0x0800)
#define BIT12	(0x1000)
#define BIT13	(0x2000)
#define BIT14	(0x4000)
#define BIT15	(0x8000)
*/
#define BIT16 (0x10000)
#define BIT17 (0x20000)
#define BIT18 (0x40000)
#define BIT19 (0x80000)
#define BIT20 (0x100000)
#define BIT21 (0x200000)
#define BIT22 (0x400000)
#define BIT23 (0x800000)
#define BIT24 (0x1000000)
#define BIT25 (0x2000000)
#define BIT26 (0x4000000)
#define BIT27 (0x8000000)
#define BIT28 (0x10000000)
#define BIT29 (0x20000000)
#define BIT30 (0x40000000)
#define BIT31 (0x80000000)

#define BITA (0x0400)
#define BITB (0x0800)
#define BITC (0x1000)
#define BITD (0x2000)
#define BITE (0x4000)
#define BITF (0x8000)

// Some other stuff

#define PARAM_NUM_MAX 25
#define PARAM_LEN_MAX 15
#define IDENT_LEN_MAX 50
#define ALM_MASK_LEN_MAX 15
#define PI 3.14159265359
#define SQRTOF2 1.414214

// gpio
typedef enum
{
	GPIO_MUX,
	GPIO_NO_MUX,
	GPIO_OUT,
	GPIO_IN
} gpioT;

#define GET_DC_DC_ON () // *TODO: *** put in pin read for GPIO  GpioDataRegs.GPBTOGGLE.bit.GPIO34 = 1;
/* // changed to latch from GPIO23
#define SONALERT_ON GpioDataRegs.GPASET.bit.GPIO23 = 1
#define SONALERT_OFF GpioDataRegs.GPACLEAR.bit.GPIO23 = 1
*/
#define SONALERT_ON pinLatch(LATCH_SONALERT, 1) // latch high turns on sonalert
#define SONALERT_OFF pinLatch(LATCH_SONALERT, 0)

// Remote modes
typedef enum
{
	REMOTE_NONE,
	REMOTE_STARTUP,
	REMOTE_BATTLESHORT,
	REMOTE_TAP_SENSE
} remoteModeT;

// Common CPU Definitions
#define NOP __asm(" nop")
/*
#define  EINT   __asm(" clrc INTM")
#define  DINT   __asm(" setc INTM")
#define  ERTM   __asm(" clrc DBGM")
#define  DRTM   __asm(" setc DBGM")
#define  EALLOW __asm(" EALLOW")
#define  EDIS   __asm(" EDIS")
#define  ESTOP0 __asm(" ESTOP0")

#define M_INT1  0x0001
#define M_INT2  0x0002
#define M_INT3  0x0004
#define M_INT4  0x0008
#define M_INT5  0x0010
#define M_INT6  0x0020
#define M_INT7  0x0040
#define M_INT8  0x0080
#define M_INT9  0x0100
#define M_INT10 0x0200
#define M_INT11 0x0400
#define M_INT12 0x0800
#define M_INT13 0x1000
#define M_INT14 0x2000
#define M_DLOG  0x4000
#define M_RTOS  0x8000
*/

//#include "F2806x_Device.h"						// calls all "F2806x" peripheral include files
#include "F2806x_CpuTimers.h"
//#include "C28x_FPU_FastRTS.h"
//#pragma CODE_SECTION("FPUmathTables")
//#include "F2806x_GlobalPrototypes.h"
//#include "F2806x_EPwm_defines.h"
/*
#include "F2806x_EPwm.h"
#include "F2806x_Gpio.h"
#include "F2806x_PieCtrl.h"
#include "F2806x_Sci.h"
*/
#define OP_MODE_STR_MAX 30

#endif // __LOCAL_TYPES_H
