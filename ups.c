// ups.c

#include "ups.h"
#include "epwm.h"
#include "math.h"
#include "system_config.h" // includes file name for configuration
#include SYSTEM_CONFIG	 // configuration header file
#include "timer.h"
#include "display_led.h"
#include "types.h"
#include "ups_state_controller.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "utilities.h"
#include "adc.h"
#include "uart.h"
#include "event_controller.h"
#include "log.h"
#include "timer.h"
#include "Enhanced_PLL.h"
#include "logpll.h"

extern event_controller_data_t eventData, *pEventData;
extern uint32_t BRT;
extern uint32_t lastBRT;

uint8_t fakeButtonEnabled = FALSE;
extern char *opModeStr[OP_MODE_STR_MAX];
timeT overloadBypassSequenceOnTimer;
timeT overloadBypassSequenceOffTimer;
int debugCyclicChargingNextCycle = FALSE;

uint8_t overloadBypassTimerOnStarted = FALSE;
uint8_t overloadBypassTimerOffStarted = FALSE;
uint8_t overloadBypassOn = FALSE;

bool OverLoadEvent = FALSE;
bool OverTempEvent = FALSE;
bool HsAlmOn = FALSE;
bool BypassFaultState = FALSE;	// Set to TRUE if we have an OverLoad or OverTemp event

uint32_t overCurrentCount = 0;

// Data Structures

volatile float sineLookup[MAX_SINE_VALUES], sineLookupShifted[MAX_SINE_VALUES], gain = 0, gainTarget = 0;
volatile int pinLatchState[8][8]; // this array will be used to set latches in millisecond timer routine
//extern int sineIndex;
//int sineIndex;
volatile int sineEnd;
struct upsDataStrucT upsOne, upsTwo, upsBoss, *pUpsOne, *pUpsTwo, *pUpsBoss;
struct upsDataStrucT *pUpsMain, *pUps, *pUpsDisplay, *pUpsLcd;
// Battery
const float batteryLookupHighPower[] = {BAT_TABLE1_HIGHER_POWER BAT_TABLE2_HIGHER_POWER};
const float batteryLookupLowPower[] = {BAT_TABLE1_LOWER_POWER BAT_TABLE2_LOWER_POWER};
static unsigned int fastChargeCyclingOn = FALSE;
// Events
struct event_T events[NUM_OF_EVENTS];
long eventStart, eventEnd;

// tried to move table to types.h where enumerated events are defined
//#define EVENT_MSG_MAX 36
#define EVENT_MSG_LENGTH 30

const Uint32 bitMask[] = // mapping bits for Uint32
	{
		BIT0, BIT1, BIT2, BIT3, BIT4, BIT5, BIT6, BIT7,
		BIT8, BIT9, BIT10, BIT11, BIT12, BIT13, BIT14, BIT15,
		BIT16, BIT17, BIT18, BIT19, BIT20, BIT21, BIT22, BIT23,
		BIT24, BIT25, BIT26, BIT27, BIT28, BIT29, BIT30, BIT31};

void initUps(void)
{
	//long i;

	// Setup Utility zero crossing pin
	PieCtrlRegs.PIECTRL.bit.ENPIE = 1; // Enable the PIE block
	EALLOW;
	PieVectTable.XINT1 = &utilityZeroCross_isr; // Assign ISR function to External Interrupt 1
	GpioIntRegs.GPIOXINT1SEL.bit.GPIOSEL = 16;  // Assign Ext Int to GPIO 16
	EDIS;

	// enable
	//PieCtrlRegs.PIEIFR1.bit.INTx4 = 1;        // do not use, will mess up interrupt, never clear flag with software
	XIntruptRegs.XINT1CR.bit.ENABLE = 1;
	XIntruptRegs.XINT1CR.bit.POLARITY = 1;
	PieCtrlRegs.PIEIER1.bit.INTx4 = 1;

	// setup inverter overcurrent isr
	EALLOW;
	PieVectTable.XINT2 = &inverterOvercurrent_isr;
	GpioIntRegs.GPIOXINT2SEL.bit.GPIOSEL = 25;
	EDIS;
	XIntruptRegs.XINT2CR.bit.ENABLE = 1;
	XIntruptRegs.XINT2CR.bit.POLARITY = 1;
	PieCtrlRegs.PIEIER1.bit.INTx5 = 1;

	// Add digital filter to pin in QUALPRD2 Group GPIO16-23
	EALLOW;
	GpioCtrlRegs.GPAMUX2.bit.GPIO16 = 0; // Sync - zero crossing for interrupt, no mux
	GpioCtrlRegs.GPADIR.bit.GPIO16 = 0;  // set pin for input
	// Set sampling period (0=Tclk, 1=2xTclk, 2=4xTclk), frequency = 1/period
	//GpioCtrlRegs.GPACTRL.bit.QUALPRD2 = 3;      // QUALPRD2 Group GPIO16-23
	GpioCtrlRegs.GPACTRL.bit.QUALPRD2 = 5; // QUALPRD2 Group GPIO16-23
	// 0 = Synchronize to SYSCLKOUT only, 1 = Qualification with 3 samples, 2 = 6 samples, 3 = Async
	GpioCtrlRegs.GPAQSEL2.bit.GPIO16 = 2;
	//GpioCtrlRegs.GPAQSEL2.bit.GPIO16 = 3;
	EDIS;

	IER |= M_INT1; // Set
	EINT;
}

void initUpsData(void)
{
	upsBoss.batteryJoules = 0.0;
	upsBoss.updateLastBRT = FALSE;
}

interrupt void utilityZeroCross_isr(void)
{

	static timeT nowTime, lastTime;
	static float period;
#if ((defined VERBOSE_DEBUG) && (defined VERBOSE_DEBUG_SYNC))
#define timeCountMax 100
	static float timeHistory[timeCountMax], fDebug;
	static int /* timeCount = 0, */ count2 = 0;
#endif
#ifdef SYNC_OFFSET_USEC 
    // timeT offsetTime;                       // used for sync offset function
    long syncMsec;
#endif
	nowTime = getTime();

#if ((defined VERBOSE_DEBUG) && (defined VERBOSE_DEBUG_SYNC))
	//debug
	if (count2++ >= timeCountMax)
	{
		count2 = 0;
	}
	else
	{
		timeHistory[count2] = period;
		fDebug = timeHistory[count2]; // make compiler happy
		period = fDebug;			  // "
	}
	// debug end
#endif

	period = (float)(nowTime.msec - lastTime.msec);
	period = (float)((nowTime.usec - lastTime.usec) * (1 / 1000.0));
	period = (float)(1.0e-3 * (nowTime.msec - lastTime.msec)) + ((nowTime.usec - lastTime.usec) * 1e-6);
	if (nowTime.days == lastTime.days) // skip crossover to next day, only one cycle
	{
		//period = ((now.msec-lastUtilityZeroCross.msec)*1.0e-3) + ((now.msec-lastUtilityZeroCross.msec)*1.0e-6);
		//period = (nowUsec - lastUsec) * (1/1000);	// subtract and scale
		//period = (float) (1/1000.0) * ((nowTime.msec - lastTime.msec) + ((nowTime.usec - lastTime.usec) * (1/1.0e6)));
		period = (float)(1.0e-3 * (nowTime.msec - lastTime.msec)) + ((nowTime.usec - lastTime.usec) * 1e-6);
		upsBossUnfilt.freqIn = 1 / period;
	}
	/*
	if (timeCount < timeCountMax) 
	{
		timeHistory[timeCount++] = period;
	} 
	else 
	{
		NOP;
	}
	*/
	upsBoss.vinZeroCross = lastTime = nowTime;

// Added Code 
    // Put in system configuration file
    // #define SYNC_OFFSET_USEC (200.0f)    // shift input synchronization to compensate with filter phase shift

#ifdef SYNC_OFFSET_USEC 
    if (SYNC_OFFSET_USEC > 0.0f)
    {
        upsBoss.vinZeroCross.usec += SYNC_OFFSET_USEC;                      // Add offset to microseconds
        if (upsBoss.vinZeroCross.usec >= 1000.0f)                           // if greater than a millisecond then adjust msec and usec
        {
            syncMsec = (long)(upsBoss.vinZeroCross.usec/1000.0f);           // take number of milliseconds after adding microseconds
            upsBoss.vinZeroCross.usec -= SYNC_OFFSET_USEC;                  // subtract microseconds
            upsBoss.vinZeroCross.msec += syncMsec;                          // add msec
            if (upsBoss.vinZeroCross.msec >= 24l * 60l * 60l * 1000l)     // if we are at 24hours worth of msec 86,400,000
            {
                upsBoss.vinZeroCross.days++;			                    // bump days
                upsBoss.vinZeroCross.msec -= 24l * 60l * 60l * 1000l;     // remove day's worth of milliseconds
            }
        }
    }
    else                                                                    // negative usec
    {
        upsBoss.vinZeroCross.usec -= SYNC_OFFSET_USEC;                      // Subtract offset from microseconds
        if (upsBoss.vinZeroCross.usec < 0.0f)                               // if negative usec then adjust msec and usec
        {
            syncMsec = (long)(upsBoss.vinZeroCross.usec/1000.0f);           // take number of milliseconds after subtracting microseconds
            upsBoss.vinZeroCross.usec += SYNC_OFFSET_USEC;                  // add microseconds
            upsBoss.vinZeroCross.msec -= syncMsec;                          // subtract milliseconds
            if (upsBoss.vinZeroCross.msec <= 24l * 60l * 60l * 1000l)    // if we are at 24hours worth of msec 86,400,000
            {
                upsBoss.vinZeroCross.days--;			                    // decrement days
                upsBoss.vinZeroCross.msec += 24l * 60l * 60l * 1000l;     // add day's worth of milliseconds
            }
        }
    }
#endif // SYNC_OFFSET_USEC
// End of Added Code 

	// Acknowledge this interrupt to receive more interrupts from group 1
	PieCtrlRegs.PIEACK.all = PIEACK_GROUP1;
}

interrupt void inverterOvercurrent_isr(void)
{

	upsBoss.inverterOvercurrent = TRUE;
	XIntruptRegs.XINT2CR.bit.ENABLE = 0;

	// Acknowledge this interrupt to receive more interrupts from group 1
	PieCtrlRegs.PIEACK.all = PIEACK_GROUP1;
}

// the following control each function of the UPS, they are called from the background task
// and their function changes with the states of the UPS State Machine.

int Lucy(void) // Peanuts...Lucy is the bossy one
{
	int status;

	status = TRUE;

	inverter();
	batteries();
	pfc();
	bypass();
	fans();
	display();
	sonalert(UPDATE); // No mode change, just check
	adcProcess();	 // convert adc counts to engineering units, filter
	remote();
	eventStateController(); // Give the Event State Machine a turn at things
	// inputRelays (LAST);					// stay with last command (AUTO,ON,OFF,LAST)

	return status;
}

int inverter(void)
{
	int status /* , okay */;
	static int substate = 0;
	static operatingModesT /*inverterOnLast = LAST, */ invModeLast = LAST; // Normally ON or OFF, LAST will set timer
	static timeT inverterOnTime, inverterUpdateTime, overLoad0Time, overLoad1Time, overLoad2Time;
	static timeT inverterSyncTime, inverterRampTime;
	float fTemp1;
	static float vramp_f, rampStep_f;

#if (defined VERBOSE_DEBUG) // Debug variables
	static int lastSyncChange = FALSE;
#endif // (VERBOSE_DEBUG == TRUE)

	status = TRUE;

	switch (upsBoss.inverterCommand)
	{
	case AUTO_ON:
		//setFrequency(UPS_FREQOUTNOM, -1); // set frequency
		upsBoss.invMode = AUTO_RAMP;
		// 0=100, 1=110, 2=115, 3=120
		switch (pEventData->systemData.bankSwitches.bit.OUTPUT_VOLT_SET)
		{
		case 0:
			upsBoss.voltOutNom = 100;
			break;
		case 1:
			upsBoss.voltOutNom = 110;
			break;
		case 2:
			upsBoss.voltOutNom = 115;
			break;
		case 3:
		default:
			upsBoss.voltOutNom = 120;
			break;
		}
		turnOnInverter();
		addSystemEvent(EVENT_INVERTER_ON);
#if (defined VERBOSE_DEBUG)
        if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
		{
			if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				log_trace("inverter command Auto/On");
			}
		}
#endif
		break;
	case AUTO_OFF:
	    
		upsBoss.invMode = AUTO_OFF;
		turnOffInverter();
		addSystemEvent(EVENT_INVERTER_OFF);
#if (defined VERBOSE_DEBUG)
        if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
		{
			if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				log_trace("inverter command Auto/Off");
			}
		}
#endif
		break;
	case MANUAL_ON:
		//setFrequency(UPS_FREQOUTNOM, -1); // set frequency
		upsBoss.invMode = MANUAL_RAMP;
		turnOnInverter();
		addSystemEvent(EVENT_INVERTER_ON);
#if (defined VERBOSE_DEBUG)
        if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
		{
			if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				log_trace("inverter command Manual/On");
			}
		}
#endif
		break;
	case MANUAL_OFF:
		upsBoss.invMode = MANUAL_OFF;
		turnOffInverter();
		if (OverTempEvent)
		{
			addSystemEvent(EVENT_HEATSINK_TEMP_SD);
#if (defined VERBOSE_DEBUG)
			if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
			    if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			    {
			        log_trace("Heatsink Temperature Shutdown");
			    }
			}
#endif
		}
		else if (OverLoadEvent)
		{
		    addSystemEvent(EVENT_OVERLOAD_CURRENT_SD);
#if (defined VERBOSE_DEBUG)
            if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
            {
                if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
                {
                    log_trace("Inverter Overload Current Shutdown");
                }
            }
#endif
		}
		addSystemEvent(EVENT_INVERTER_OFF);
#if (defined VERBOSE_DEBUG)
        if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
		{
			if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				log_trace("inverter command Manual/Off");
			}
		}
#endif
		break;
	case FAULT:
		upsBoss.invMode = FAULT;
		turnOffInverter();
		addSystemEvent(EVENT_INVERTER_FAULT);
#if (defined VERBOSE_DEBUG)
        if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
		{
			if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				log_trace("inverter command Fault");
			}
		}
#endif
		break;
	default:
		status = FALSE; // failed command
		break;
	}
	upsBoss.inverterCommand = NONE; // command has been processed, clear

	switch (upsBoss.invMode)
	{
	case MANUAL_ON:
	case AUTO_ON:
		if (upsBoss.invMode != invModeLast)
		{
			invModeLast = upsBoss.invMode;
			inverterOnTime = inverterUpdateTime = inverterSyncTime = getTime();
			upsBoss.inverterFault = NORMAL;
		}
		if (timer(inverterUpdateTime, 100))
		{
			inverterUpdateTime = getTime();
			//setFrequency(-1.0, 0); // update frequency ramp
			// 0=100, 1=110, 2=115, 3=120
			switch (pEventData->systemData.bankSwitches.bit.OUTPUT_VOLT_SET)
			{
			case 0:
				upsBoss.voltOutNom = 100;
				break;
			case 1:
				upsBoss.voltOutNom = 110;
				break;
			case 2:
				upsBoss.voltOutNom = 115;
				break;
			case 3:
			default:
				upsBoss.voltOutNom = 120;
				break;
			}
			fTemp1 = upsBoss.voltOutNom - upsBoss.voltOut; // Voltage Error
			if (fRange(upsBoss.voltOut, upsBoss.voltOutNom - 0.1, upsBoss.voltOutNom + 0.1))
			{
				gain += fTemp1 * 0.002; // slowly correct error
			}
			else
			{
				gain += fTemp1 * 0.02; // slowly correct error
			}
			if (gain > 200.0)
			{
				gain = 200.0;
			}
		}
		if (upsBoss.ampOutMode == ON_ALARM) // Overcurrent trip in cpu_timer0_isr
		{
			upsBoss.inverterCommand = FAULT;
			addSystemEvent(EVENT_INVERTER_CURRENT_SD);
#if (defined VERBOSE_DEBUG)
            if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					log_trace("invMode=auto_on, ampOutMode==on_alarm");
				}
			}
#endif
		}
		if (timer(inverterOnTime, 10000)) // wait for stable inverter operation
		{
			if ((upsBoss.voltOut > (upsBoss.voltOutNom * 1.2f)) || (upsBoss.voltOut < (upsBoss.voltOutNom * 0.5f)))
			{
				upsBoss.inverterCommand = FAULT;
#if (defined VERBOSE_DEBUG)
                if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			    {
					if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						log_trace("invMode=auto_on, invVolt high or low (upsBoss.voltOut(%f) > (upsBoss.voltOutNom*1.2f)(%f)) || (upsBoss.voltOut(%f) < (upsBoss.voltOutNom*0.5f)(%f))", upsBoss.voltOut, upsBoss.voltOutNom*1.2f, upsBoss.voltOut, upsBoss.voltOutNom*0.5f);
					}
				}
#endif
			}
		}

		if (upsBoss.bypassState)
		{
			if (overloadBypassTimerOnStarted)
			{
				// WPB
				if (timer(overloadBypassSequenceOnTimer,1000))
				{
					upsBoss.inverterCommand = MANUAL_OFF;
					overloadBypassTimerOnStarted = FALSE;
				    sonalert(OFF);
				    sonalert(ON_SOLID);
					displayLedSet(LED_DISP_LOAD5, LED_DISP_FLASH);
					overloadBypassOn = TRUE;
#if (defined VERBOSE_DEBUG)
					if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
						{
							log_trace("upsBoss.inverterCommand = MANUAL_OFF");
							log_trace("overloadBypassTimerOnStarted = FALSE");
						}
					}
#endif
				}
			}

            if (overloadBypassTimerOffStarted)
			{
				if (timer(overloadBypassSequenceOffTimer, 1000))
				{
					upsBoss.bypassCommand = AUTO_OFF;
					overloadBypassTimerOffStarted = FALSE;
					upsBoss.bypassState = FALSE;
                    overloadBypassOn = FALSE;
#if (defined VERBOSE_DEBUG)
					if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
						{
							log_trace("upsBoss.bypassCommand = AUTO_OFF");
							log_trace("overloadBypassTimerOffStarted = FALSE");
						}
					}
#endif
				}
			}
		}

		// WPB Hard Overcurrent Limit or HEAT SYNC 2 
		// Timed overload, 2 timers defined in system_config.h

		if ( (upsBoss.tSink2 >= TEMP_HS_TRIP_ON) || (upsBoss.tSink1 >= TEMP_HS_TRIP_ON))
		{
			if ( OverTempEvent != TRUE )	// Prevent multiple verbose messages if we are already at trip point
			{
				OverTempEvent = TRUE;
#if (defined VERBOSE_DEBUG)
            	if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						if ( BYPASS_ACTIVE == TRUE )
						{
							log_trace( "Reached Temp Trip Level, going to Bypass mode" );
						}
						else if ( SHUTDOWN_ON_OVERLOAD == TRUE )
						{
							log_trace( "Reached Temp Trip Level, going to Shutdown mode" );
						}
					}
				}
			}
#endif
		}
		// allow for hysterisis
		else if ( (upsBoss.tSink2 < (TEMP_HS_TRIP_ON - 2.0f) ) && (upsBoss.tSink1 < ( TEMP_HS_TRIP_ON - 2.0f )) )
		{
			OverTempEvent = FALSE;
		}
		if ( ( ( upsBoss.tSink2 >= TEMP_HS_ALM_ON ) || ( upsBoss.tSink1 >= TEMP_HS_ALM_ON ) ) && ( OverTempEvent == FALSE ) &&
		   ( HsAlmOn == FALSE ) )   // Trip alarm once if we are below TEMP_HS_TRIP_ON (which goes to bypass and/or inverter shutdown) and not already at alarm level
		{
		    //Do alarm - talk to Hector, for now SONALERT beep...
			addSystemEvent(EVENT_HEATSINK_TEMP_ALARM);
		    sonalert( ON_BEEP );
		    HsAlmOn = TRUE;
#if (defined VERBOSE_DEBUG)
            if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					log_trace( "Reached Temp Alarm Level, >= TEMP_HS_ALM_ON" );
				}
			}
#endif
		}
		else if ( ( upsBoss.tSink2 < TEMP_HS_ALM_OFF ) && ( upsBoss.tSink1 < TEMP_HS_ALM_OFF ) && ( HsAlmOn == TRUE ) )
		{
		    sonalert(OFF);
		    HsAlmOn = FALSE;
		}
		if ( upsBoss.loadPctOut >= (UPS_LOAD_TRIP_ON * 100.0f) )
		{
			OverLoadEvent = TRUE;
		}
		if ( OverTempEvent || OverLoadEvent ) BypassFaultState = TRUE;		// Used to qualify BUTTON_ON to allow or suppress OFF_BYPASS events
		if ( 
			 ( OverLoadEvent && (timer(overLoad0Time, OVERLOAD0_MSEC)) )
#ifdef HEAT_SYNC_2_ENABLE
		     || ( OverTempEvent && (upsBoss.battleshort == FALSE) )
#endif
		   )
		{
			//upsBoss.inverterCommand = FAULT;
			// WPB
			
			if (( overloadBypassTimerOnStarted == FALSE ) && ( upsBoss.bypassMode != MANUAL_ON ) )
			{
				//addSystemEvent(EVENT_ON_BYPASS);				// Source of double ON_BYPASS events
				upsBoss.bypassCommand = MANUAL_ON;
				overloadBypassSequenceOnTimer = getTime();
				overloadBypassTimerOnStarted = TRUE;
				upsBoss.bypassState = TRUE;
#if (defined VERBOSE_DEBUG)
				if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						log_trace("overloadBypassTimerOnStarted = TRUE");
						log_trace("trip overload level %4.2f", upsBoss.loadPctOut);
					}
				}
#endif
			}
		}
		else if (upsBoss.loadPctOut < (UPS_LOAD_TRIP_OFF * 100.0)) // if not overloaded
		{
			overLoad0Time = getTime(); // reset overload timer
#if (defined VERBOSE_DEBUG)
            if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					//log_trace("invMode=auto_on, overload:upsBoss.loadPctOut(%f) >= (UPS_LOAD_TRIP_OFF * 100.0)(%f)", upsBoss.loadPctOut, (UPS_LOAD_TRIP_OFF * 100.0f));
				}
			}
#endif
		}
		if ((upsBoss.loadPctOut >= OVERLOAD1_PERCENT) && (timer(overLoad1Time, OVERLOAD1_MSEC)))
		{
			upsBoss.inverterCommand = FAULT;
			addSystemEvent(EVENT_INVERTER_POWER_SD);
#if (defined VERBOSE_DEBUG)
            if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					log_trace("invMode=auto_on, overload1");
				}
			}
#endif
		}
		else if (upsBoss.loadPctOut < OVERLOAD1_PERCENT) // if not overloaded
		{
			overLoad1Time = getTime(); // reset overload timer
		}
		if ((upsBoss.loadPctOut >= OVERLOAD2_PERCENT) && (timer(overLoad2Time, OVERLOAD2_MSEC)))
		{
			upsBoss.inverterCommand = FAULT;
			addSystemEvent(EVENT_INVERTER_POWER_SD);
#if (defined VERBOSE_DEBUG)
			if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					log_trace("invMode=auto_on, overload2");
				}
			}
#endif
		}
		else if (upsBoss.loadPctOut < OVERLOAD2_PERCENT) // if not overloaded
		{
			overLoad2Time = getTime(); // reset overload timer
		}
		break;
	case AUTO_OFF:
	case MANUAL_OFF:
		if (upsBoss.invMode != invModeLast)
		{
			invModeLast = upsBoss.invMode;
			upsBoss.inverterFault = NORMAL;
		}
		gain = 0;
		break;
	case AUTO_RAMP:
	case MANUAL_RAMP:
		if (upsBoss.invMode != invModeLast)
		{
			invModeLast = upsBoss.invMode;
			substate = 0; // reset substate
			upsBoss.inverterFault = NORMAL;
			inverterRampTime = getTime();
			gain = 0;
#if defined INVERTER_RAMP_TIME
			rampStep_f = 1 / (INVERTER_RAMP_TIME / 10e-3); // calculate step size for time
#else
			rampStep_f = 0.05;				// % step per time increment (10ms)
#endif
		}
		switch (substate)
		{
		case 0: // inverter enabled, let settle to 0V
			if (timer(inverterRampTime, 1000))
			{
				inverterRampTime = getTime(); // reset for ramp
				substate++;					  // go to ramp
			}
			break;
		case 1:
			if (timer(inverterRampTime, 10))
			{
				inverterRampTime = getTime();
				// calculate percent VAC peak divided by 1/2 bus voltage for ~ gain%
				vramp_f = (upsBoss.voltOutNom * SQRTOF2 / (upsBoss.voltBus * 0.5)) * 95.0; // gain in %
				if (vramp_f > gain)
				{
					gain += (vramp_f * rampStep_f); // add  calc % of target
				}
				else // exit ramp
				{
					// pre-stuff inverter voltage filter, otherwise lag will cause voltage to overshoot
					upsBoss.voltOut = upsBoss.voltOutNom;
					if (upsBoss.invMode == AUTO_RAMP)
					{
						upsBoss.invMode = AUTO_ON;
					}
					else
					{
						upsBoss.invMode = MANUAL_ON;
					}
				}
			}
			break;
		default:
			break;
		}
		break;
	case FAULT:
		if (upsBoss.invMode != invModeLast)
		{
			invModeLast = upsBoss.invMode;
			upsBoss.inverterFault = FAULT;
		}
		break;
	default:
		break;
	}

	// Synchronize Inverter to Utility, measure relative frequency and phase
	// match frequency then change phase to converge, later add function to
	// compensate for phase shift of inverter filter as a function of current
	switch (upsBoss.syncMode)
	{
	case AUTO_ON:
		if (upsBoss.invZeroCrossFlag == TRUE) // Inverter starting cycle
		{
			if (timer(inverterSyncTime, 1000)) // Synchronization routine to hold off for 1 second
			{
				if (pEventData->systemData.bankSwitches.bit.INV_SYNC_TO_LINE == 1)
				{
					// New Sync Implemented
					// freq_phase_sync(); // Do the synchronization
#ifdef VERBOSE_DEBUG_SYNC					
					logpll();
#endif
				}
			} // End of the 1 second hold off time check
			upsBoss.invZeroCrossFlag = FALSE;
		}
		break;
	case AUTO_OFF:
		if ((upsBoss.invMode == AUTO_ON) || (upsBoss.invMode == MANUAL_ON))
		{
			upsBoss.syncMode = AUTO_ON;
		}
		break;
	case MANUAL_ON:
		break;
	case MANUAL_OFF:
		//setFrequency(UPS_FREQOUTNOM, 2.0);
		break;
	default:
		break;
	}
// Indicate during debug when sync is enabled or disabled
#if (defined VERBOSE_DEBUG)
    if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
	{
		if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
		{
			if (pEventData->systemData.bankSwitches.bit.INV_SYNC_TO_LINE != lastSyncChange)
			{
				lastSyncChange = pEventData->systemData.bankSwitches.bit.INV_SYNC_TO_LINE;
				if (pEventData->systemData.bankSwitches.bit.INV_SYNC_TO_LINE == TRUE)
				{
					log_trace("Sync enabled");
				}
				else
				{
					log_trace("Sync disabled");
				}
			}
		}
	}
#endif // (defined VERBOSE_DEBUG)

	//test_frequency_slew ();										// A quick check to see if we need to slew to a test frequency setpoint
	return status;
}

int batteries(void)
{
	int status;
	static volatile float lowBatAlarm, lowBatShutdown, jouleTemp, fTemp;
	static timeT batteryCapTimer;
#if (defined BATTERY_CHARGE_CYCLING) || (defined BATTERY_CHARGE_CYCLING2)
	static timeT batteryChargeCyclingTimer;
	static timeT batteryChargeOnSlowTimer;
	static int debugMessageInhibit = FALSE;
#else
	static timeT batteryChargeTimeout;
#endif
#ifdef BATTERY_CHARGE_CYCLING2
	static cyclicChargeModesT cyclicChargeMode = CYCLIC_CHARGE_WAIT;
	static int lastCyclicChargeMode = CYCLIC_CHARGE_OFF, cyclicLastUpsState = UPS_ON_BAT;
#endif
#ifndef BATTERY_CHARGE_CYCLING2	
	static timeT fastChargeTimer;
#endif
	static timeT lowBatteryAlarmTimedTimer;
	static volatile ups_states_t lastUpsState = UPS_ON_BAT;
	static volatile operatingModesT chgModeLast, dcModeLast;
#if BAT_CAP_METHOD == 2 // Joule method of battery capacity calculation
	int iTemp;
	//float fTemp;
	static volatile operatingModesT chgModeLastJoule = AUTO_OFF;
#endif
	typedef enum
	{
		DC_MODE_ALM_OFF = 0,
		DC_MODE_ALM_ON_BAT,
		DC_MODE_ALM_LOW
	} dcModeAlmT;
	static dcModeAlmT dcModeAlarm = DC_MODE_ALM_OFF;
	static bool ChgrOVFault = FALSE, ChgrOVFaultLast = FALSE;
    static int ChgrState = 0;
    static timeT ChgrFaultTimer;
	static float UfiltVbatMax = 0;
#if ((defined VERBOSE_DEBUG) && (!defined BATTERY_CHARGE_CYCLING2))
	static uint32_t myCounter = 0;
#endif

	status = TRUE;

	if ( NUM_BAT > 0 )			// Make sure we 'normally' use batteries: line conditioners do not use batteries
	{
/*								// Commented this out temporarily to commit-pull-push and not effect Bill/Mark
#if (defined VERBOSE_DEBUG)
		if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
		{
		   if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
		   {
		        log_trace("Unfilt voltBat = %f", upsBossUnfilt.voltBat);
		   }
		}
#endif
*/
		if  ( UfiltVbatMax < upsBossUnfilt.voltBat )
  		{
  		    UfiltVbatMax = upsBossUnfilt.voltBat;
  		}
  		ChgrOVFault = (UfiltVbatMax >= BATTERY_V_MAX_0_PCT);

		switch ( ChgrState )
		{
		case 0:
			if ( ChgrOVFault != ChgrOVFaultLast )							// Ddo this only once if there was a change
			{
				addSystemEvent(EVENT_CHGR_OVERVOLT);
#if (defined VERBOSE_DEBUG)
            	if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
            	{
                	if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
                	{
                    	log_trace("ChgrOVFault != ChgrOVFaultLast: %d  %d", ChgrOVFault, ChgrOVFaultLast);
                	}
            	}
#endif
				ChgrOVFaultLast = ChgrOVFault;
				sonalert(OFF);
				sonalert(ON_SOLID);
				ChgrState = 1;
#if (defined VERBOSE_DEBUG)
				if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
			    	if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			    	{
			        	log_trace("upsBossUnfilt.voltBat = %f, too high", upsBossUnfilt.voltBat);
			    	}
				}
#endif
			}
			break;
		case 1:
			if ( upsBoss.displayMode == ON )				// If true, make sure fault indications are on
			{
				displayLedSet(LED_DISP_VERT_EXTDC, LED_DISP_SOLID);
				displayLedSet(LED_DISP_VERT_SERVICE_BAT, LED_DISP_SOLID);
				ChgrFaultTimer = getTime();								// Set timer
				ChgrState = 2;
			}
			break;						// Otherwise, wait until displayMode is 'ON'
		case 2:
			if ( timer(ChgrFaultTimer, ChgrFaultMsecs )  && ( upsBossUnfilt.voltBat < BATTERY_V_MAX_0_PCT ) )
			{
				displayLedSet(LED_DISP_VERT_EXTDC, LED_DISP_OFF);
				displayLedSet(LED_DISP_VERT_SERVICE_BAT, LED_DISP_OFF);
				sonalert(OFF);
				ChgrOVFault = ChgrOVFaultLast = FALSE;
				ChgrState = 0;
#if (defined VERBOSE_DEBUG)
                if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
                {
                    if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
                    {
                        log_trace("UnfiltVoltBat = %f, OK  UfiltVbatMax = %f", upsBossUnfilt.voltBat, UfiltVbatMax );
                    }
                }
#endif
				UfiltVbatMax = 0;
			}
			else
			{
				//  Make sure the LED's stay on -- other code was turning the SERVICE_BAT led off -- debugger won't run on system
			    //    to identify culprit code to fix it a different way
				displayLedSet(LED_DISP_VERT_EXTDC, LED_DISP_SOLID);
				displayLedSet(LED_DISP_VERT_SERVICE_BAT, LED_DISP_SOLID);	
			}
			break;
		}
	}

	if (upsBoss.powOut <= 150.0) // keep updated before switching to batteries
	{
		lowBatAlarm = LOW_BATTERY_ALARM_LOW_POWER;
		lowBatShutdown = LOW_BATTERY_SD_LOW_POWER;
	}
	else
	{
		lowBatAlarm = LOW_BATTERY_ALARM;
		lowBatShutdown = LOW_BATTERY_SD;
	}
	if (pEventData->systemData.bankSwitches.bit.LOW_BAT_ALM == 1) // Timed low battery alarm
	{
		switch (pEventData->systemData.bankSwitches.bit.LOW_BAT_ALM_SET) // lowBatteryAlarmTimedTimer set by ups state
		{
		case 0:
			if (timer(lowBatteryAlarmTimedTimer, (long)1800 * 1000)) // delay seconds * 1000ms
			{
				lowBatAlarm = 0.0; // force low battery alarm
			}
			break;
		case 1:
			if (timer(lowBatteryAlarmTimedTimer, (long)7200 * 1000)) // delay seconds * 1000ms
			{
				lowBatAlarm = 0.0; // force low battery alarm
			}
			break;
		case 2:
			if (timer(lowBatteryAlarmTimedTimer, (long)3600 * 1000)) // delay seconds * 1000ms
			{
				lowBatAlarm = 0.0; // force low battery alarm
			}
			break;
		case 3:
			if (timer(lowBatteryAlarmTimedTimer, (long)10800 * 1000)) // delay seconds * 1000ms
			{
				lowBatAlarm = 0.0; // force low battery alarm
			}
			break;
		}
	}
	upsBoss.lowBatAlarm = lowBatAlarm; // update UPS data structure
	upsBoss.lowBatShutdown = lowBatShutdown;

	switch (upsBoss.chgCommand)
	{
	case AUTO_FAST:
		pinLatch(LATCH_DISABLE_CHG_n, 1); // Charge on
		pinLatch(LATCH_FAST_CHG, 1);	  // Charge fast
		// MSJ not used for older version of cyclic charging
		//fastChargeTimer = getTime();
		upsBoss.chgMode = AUTO_FAST; // report mode change

		addSystemEvent(EVENT_FAST_CHARGE);
		upsBoss.batChgMode = SNMP_BAT_CHARGE_CHARGING; // SNMP battery status
#if (defined BATTERY_CHARGE_CYCLING)
		batteryChargeCyclingTimer = getTime();
		fastChargeCyclingOn = FALSE; // start with off cycle
#endif
#if (defined VERBOSE_DEBUG)
		if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
		{
			if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				log_trace("charge command: Auto Fast");
			}
		}
#endif
		break;
	case MANUAL_FAST:
		pinLatch(LATCH_DISABLE_CHG_n, 1); // Charge on
		pinLatch(LATCH_FAST_CHG, 1);	  // Charge fast
		// MSJ not used for older version of cyclic charging
		//fastChargeTimer = getTime();
		upsBoss.chgMode = MANUAL_FAST; // report mode change
		addSystemEvent(EVENT_FAST_CHARGE);
		upsBoss.batChgMode = SNMP_BAT_CHARGE_CHARGING; // SNMP battery status
#if (defined VERBOSE_DEBUG)
        if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
		{
			if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				log_trace("charge command: Manual Fast");
			}
		}
#endif
		break;
	case AUTO_SLOW:
		pinLatch(LATCH_DISABLE_CHG_n, 1); // Charge on
		pinLatch(LATCH_FAST_CHG, 0);	  // Charge slow
		upsBoss.chgMode = AUTO_SLOW;	  // report mode change
		addSystemEvent(EVENT_FLOAT_CHARGE);
		upsBoss.batChgMode = SNMP_BAT_CHARGE_FLOATING; // SNMP battery status
#if (defined VERBOSE_DEBUG)
        if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
		{
			if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				log_trace("charge command: Auto Slow");
			}
		}
#endif
		break;
	case MANUAL_SLOW:
		pinLatch(LATCH_DISABLE_CHG_n, 1); // Charge on
		pinLatch(LATCH_FAST_CHG, 0);	  // Charge slow
		upsBoss.chgMode = MANUAL_SLOW;	// report mode change
		addSystemEvent(EVENT_FLOAT_CHARGE);
		upsBoss.batChgMode = SNMP_BAT_CHARGE_FLOATING; // SNMP battery status
#if (defined VERBOSE_DEBUG)
        if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
		{
			if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				log_trace("charge command: Manual Slow");
			}
		}
#endif
		break;
	case AUTO_OFF:
		pinLatch(LATCH_DISABLE_CHG_n, 0);			  // Charge off
		pinLatch(LATCH_FAST_CHG, 0);				  // Charge slow
		upsBoss.chgMode = AUTO_OFF;					  // report mode change
		addSystemEvent(EVENT_NO_CHARGE);			  // add event
		//upsBoss.batChgMode = SNMP_BAT_CHARGE_RESTING; // SNMP battery status
#if (defined VERBOSE_DEBUG)
        if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
		{
			if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				log_trace("charge command: Auto Off");
			}
		}
#endif
		break;
	case MANUAL_OFF:
		pinLatch(LATCH_DISABLE_CHG_n, 0);			  // Charge off
		pinLatch(LATCH_FAST_CHG, 0);				  // Charge slow
		upsBoss.chgMode = MANUAL_OFF;				  // report mode change
		addSystemEvent(EVENT_NO_CHARGE);			  // add event
		//upsBoss.batChgMode = SNMP_BAT_CHARGE_RESTING; // SNMP battery status
#if (defined VERBOSE_DEBUG)
        if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
		{
			if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				log_trace("charge command: Manual Off");
			}
		}
#endif
		break;
	case AUTO_ON_ALM:
		pinLatch(LATCH_DISABLE_CHG_n, 0); // Charge off
		pinLatch(LATCH_FAST_CHG, 0);	  // Charge slow
		upsBoss.chgMode = AUTO_ON_ALM;	  // report mode change
		upsBoss.batMode = FAULT;		  // Fault and Service Battery LEDs
		sonalert(ON_SOLID);
		addSystemEvent(EVENT_BATTERY_FAIL);
#if (defined VERBOSE_DEBUG)
        if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
		{
			if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				log_trace("charge command: On Alarm Auto");
			}
		}
#endif
		break;
	default:
		break;
	}
	upsBoss.chgCommand = NONE; // clear command

	switch (upsBoss.upsState)
	{
	case UPS_INIT:
		if (upsBoss.upsState != lastUpsState) // state entry code
		{
			lastUpsState = upsBoss.upsState;
			batteryCapTimer = getTime();		 // intialize timer
			chgModeLast = dcModeLast = AUTO_OFF; // init last state to off
		}
		break;
	case UPS_ON_BAT:
		if (upsBoss.upsState != lastUpsState) // state entry code
		{
			lastUpsState = upsBoss.upsState;
			lowBatteryAlarmTimedTimer = getTime();			  // intialize timer
			chgModeLast = dcModeLast = AUTO_OFF;			  // init last state to off
			upsBoss.batChgMode = SNMP_BAT_CHARGE_DISCHARGING; // SNMP battery status
#if (defined BATTERY_CHARGE_CYCLING) // normal Fast charge
			upsBoss.numberOfChargeCycles = 0;
#if (defined VERBOSE_DEBUG)
			if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					log_trace("cyclic charging, went on batteries, reset number of charge cycles to 0");
				}
			}
#endif															// (defined VERBOSE_DEBUG)
#endif															// (defined BATTERY_CHARGE_CYCLING)
		}
		break;
	}

	static uint32_t mainCounter = 0;
	mainCounter++;
	if ((mainCounter % 100000) == 0)
	{
#if (defined VERBOSE_DEBUG)
				if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
//						log_trace("chgMode=%s cyclingTimer=%d", opModeStr[upsBoss.chgMode],batteryChargeCyclingTimer.msec);
					}
				}
#endif
	}
	
// simpifying charge cycling
#ifndef BATTERY_CHARGE_CYCLING2	
	// changing charge mode going on batteries and off batteries handled in ups state machine
	switch (upsBoss.chgMode) // battery state machine
	{
	case AUTO_OFF: // UPS running on batteries
		if (upsBoss.chgMode != chgModeLast)
		{
			chgModeLast = upsBoss.chgMode;
		}
		break;
	case AUTO_FAST: // fast charge
		if (upsBoss.chgMode != chgModeLast)
		{
			chgModeLast = upsBoss.chgMode;
#if (defined BATTERY_CHARGE_CYCLING) // normal Fast charge
			batteryChargeCyclingTimer = getTime();		  // reset timer for this cycle
			/*
				message using this flag can only be printed once after this state is entered.
			*/
			debugMessageInhibit = FALSE;
#else	// (!defined BATTERY_CHARGE_CYCLING)
			batteryChargeTimeout = getTime(); // track time in fast charge mode
#endif	// (defined BATTERY_CHARGE_CYCLING)
		}
#if (!defined BATTERY_CHARGE_CYCLING) // normal Fast charge
		if ((upsBoss.ampChg <= BAT_CHG_SLOW_THRESHOLD) && (timer(fastChargeTimer, 30000L)))
		{
			upsBoss.chgCommand = AUTO_SLOW;
		}
		// if charger in fast charge beyond limit (default 48 hours) show fault
		if (timer(batteryChargeTimeout, BAT_FAST_CHG_TIMEOUT))
		{
			addSystemEvent(EVENT_BATTERY_FAIL);
			upsBoss.chgCommand = AUTO_ON_ALM;
		}
#else  	// (!defined BATTERY_CHARGE_CYCLING) cycling Fast charge
		// start with 30 seconds of fast charge on first cycle, charger set to fast and timer set in chgCommand case
		if ((!timer(fastChargeTimer, 30000L)) && (upsBoss.numberOfChargeCycles == 0))
		{
			NOP; // debug
		}
		else if (upsBoss.numberOfChargeCycles == 0)		  // first time through after 30 second timer
		{
			batteryChargeCyclingTimer = getTime();		  // reset timer for next cycle
			fastChargeCyclingOn = FALSE;				  // set to off cycle
			upsBoss.numberOfChargeCycles++;				  // increment number of charge cycles
			if (upsBoss.numberOfChargeCycles > BATTERY_CYCLING_CHARGE_MAX_CYCLES)
			{
				upsBoss.chgCommand = AUTO_ON_ALM;		  // EVENT_BATTERY_FAIL set in chgCommand
			}
			upsBoss.chgCommand = MANUAL_OFF;			  // report mode change
			upsBoss.batChgMode = SNMP_BAT_CHARGE_RESTING; // SNMP battery status
#if (defined VERBOSE_DEBUG)
			if (upsBoss.verboseDebug == TRUE)
			{
				if (upsBoss.upsComMode == UPS_COM_USER)
				{
					log_trace("ups.c cyclic charger cycle 0, going to off after 30 seconds Fast Charge");
				}
			}
#endif	// (defined VERBOSE_DEBUG)
		}
		else if ((fastChargeCyclingOn == TRUE) && (upsBoss.numberOfChargeCycles > 0)) // On charge cycle
		{
			// When charge cycle ends, start next state of cycle
			if (upsBoss.numberOfChargeCycles > BATTERY_CYCLING_CHARGE_MAX_CYCLES)
			{
				upsBoss.chgCommand = AUTO_ON_ALM;		 // EVENT_BATTERY_FAIL set in chgCommand
			}
			uint8_t timesUp = timer(batteryChargeCyclingTimer, (BATTERY_CYCLING_CHARGE_ON_MINUTES * 60L * 1000L));// When charge cycle ends, start next state of cycle
			myCounter++;
			if (myCounter % 100)
			{
#if (defined VERBOSE_DEBUG)
				if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
//						log_trace("timesup=%d timer=%d", timesUp,batteryChargeCyclingTimer.msec);
					}
				}
#endif	// (defined VERBOSE_DEBUG)
			}
			if (debugCyclicChargingNextCycle == TRUE)		  // set in ups_com.c by command '_'
			{
				debugCyclicChargingNextCycle = FALSE;		  // reset flag
				timesUp = TRUE;								  // pretend timer expired to go to next off/on state
			}
			if (timesUp)
			{
				fastChargeCyclingOn = FALSE;				  // set to off cycle
				upsBoss.numberOfChargeCycles++;				  // increment number of charge cycles
				if (upsBoss.numberOfChargeCycles > BATTERY_CYCLING_CHARGE_MAX_CYCLES)
				{
					upsBoss.chgCommand = AUTO_ON_ALM;		 // EVENT_BATTERY_FAIL set in chgCommand
				}
				upsBoss.chgCommand = MANUAL_OFF;				  // report mode change
				upsBoss.batChgMode = SNMP_BAT_CHARGE_RESTING; // SNMP battery status
#if (defined VERBOSE_DEBUG)
				if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						log_trace("charge cycle ends:%lu, fastChargeCyclingOn = FALSE", upsBoss.numberOfChargeCycles);
					}
				}
#endif	// (defined VERBOSE_DEBUG)
			}	// if (timesUp)
			// only need to check during On cycle
			if (upsBoss.ampChg <= BAT_CHG_SLOW_THRESHOLD)
			{
				upsBoss.chgCommand = AUTO_SLOW;
#if (defined VERBOSE_DEBUG)
				if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						log_trace("Charger Cycling upsBoss.chgCommand = AUTO_SLOW");
					}
				}
#endif	// (defined VERBOSE_DEBUG)
			}
#if (!defined BATTERY_CHARGE_CYCLING) // Cycling charge uses charge cycle count for this purpose
			// if charger in fast charge beyond limit (default 48 hours) show fault
			if (timer(batteryChargeTimeout, BAT_FAST_CHG_TIMEOUT))
			{
				addSystemEvent(EVENT_BATTERY_FAIL);
				upsBoss.chgCommand = AUTO_ON_ALM;
			}
#endif
		}	// else if (fastChargeCyclingOn == TRUE) // On charge cycle
#endif // (!defined BATTERY_CHARGE_CYCLING)
		break;
	case AUTO_SLOW: // slow charge
		if (upsBoss.chgMode != chgModeLast)
		{
			chgModeLast = upsBoss.chgMode;
#if (defined BATTERY_CHARGE_CYCLING) // Slow charge
			/*
				Start timer used to see how long charger is in Slow mode to
				indicate a sucessful charge completed, used to reset upsBoss.numberOfChargeCycles to zero.
			*/
			batteryChargeOnSlowTimer = getTime();
#endif
		}
		if (upsBoss.ampChg >= BAT_CHG_FAST_THRESHOLD)
		{
			upsBoss.chgCommand = AUTO_FAST;
#if (defined BATTERY_CHARGE_CYCLING) // Slow charge
#if (defined VERBOSE_DEBUG)
			if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					log_trace("Charger Cycling upsBoss.chgCommand = AUTO_FAST");
				}
			}
#endif
#endif
		}
#if (defined BATTERY_CHARGE_CYCLING) // Slow charge
		if (fastChargeCyclingOn == TRUE) // On charge cycle
		{
			// When charge cycle ends, start next state of cycle
			uint8_t timesUp = timer(batteryChargeCyclingTimer, (BATTERY_CYCLING_CHARGE_ON_MINUTES * 60L * 1000L));// When charge cycle ends, start next state of cycle
			/*
				If charger is in Slow Mode for set time then batteries are charged
				so reset number of charge cycles.
			*/
			if(timer(batteryChargeOnSlowTimer, 5L * 60L * 1000L)) // 5 minutes
			{
				upsBoss.numberOfChargeCycles = 0;
#if (defined VERBOSE_DEBUG)
			if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if ( ((upsBoss.upsComMode == UPS_COM_USER) || ( VERBOSE_DEBUG_PORT == UART_PORT1 )) && (debugMessageInhibit == FALSE))
				{
					log_trace("Charger Cycling, Auto Slow Charge Timer expired, set number of Charge Cycles to 0, batteries charged");
					debugMessageInhibit = TRUE; // only print message once in this state
				}
			}
#endif
			}
#if (defined VERBOSE_DEBUG)
			static uint32_t myCounter = 0;
			myCounter++;
			if (myCounter % 100)
			{
				if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
//						log_trace("timesup=%d timer=%d", timesUp,batteryChargeCyclingTimer.msec);
					}
				}
#endif
			}
			if (debugCyclicChargingNextCycle == TRUE)		  // set in ups_com.c by command '_'
			{
				debugCyclicChargingNextCycle = FALSE;		  // reset flag
				timesUp = TRUE;								  // pretend timer expired to go to next off/on state
			}
			if (timesUp)
			{
				fastChargeCyclingOn = FALSE;				  // set to off cycle
				upsBoss.numberOfChargeCycles++;				  // increment number of charge cycles
				if (upsBoss.numberOfChargeCycles >= BATTERY_CYCLING_CHARGE_MAX_CYCLES)
				{
					upsBoss.chgCommand = AUTO_ON_ALM;		  // EVENT_BATTERY_FAIL set in chgCommand
				}
				upsBoss.chgCommand = MANUAL_OFF;				  // report mode change
				upsBoss.batChgMode = SNMP_BAT_CHARGE_RESTING; // SNMP battery status
#if (defined VERBOSE_DEBUG)
				if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						log_trace("charge cycle ends:%lu, slowChargeCyclingOn = FALSE", upsBoss.numberOfChargeCycles);
					}
				}
				if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						log_trace("charge cycle ends:%lu, slowChargeCyclingOn = FALSE", upsBoss.numberOfChargeCycles);
					}
				}
#endif
			}
		}
#endif // (!defined BATTERY_CHARGE_CYCLING)
		break;
	case MANUAL_SLOW: // put in manual mode, charger turned off
		if (upsBoss.chgMode != chgModeLast)
		{
			chgModeLast = upsBoss.chgMode;
		}
		break;
	case MANUAL_OFF: // put in manual mode, charger turned off
		if (upsBoss.chgMode != chgModeLast)
		{
			chgModeLast = upsBoss.chgMode;
#if (defined BATTERY_CHARGE_CYCLING) 		// charge off
			batteryChargeCyclingTimer = getTime();		  // reset timer for this cycle
			upsBoss.batChgMode = SNMP_BAT_CHARGE_RESTING; // SNMP battery status
#endif
		}
#if (defined BATTERY_CHARGE_CYCLING) 		// charge off
		if (fastChargeCyclingOn == FALSE) 	// Off charge cycle
		{
			uint8_t timesUp = timer(batteryChargeCyclingTimer, (BATTERY_CYCLING_CHARGE_OFF_MINUTES * 60L * 1000L));// When charge cycle ends, start next state of cycle
			if (debugCyclicChargingNextCycle == TRUE)		  // set in ups_com.c by command '_'
			{
				debugCyclicChargingNextCycle = FALSE;		  // reset flag
				timesUp = TRUE;								  // pretend timer expired to go to next off/on state
			}
			if (timesUp)
			{
				fastChargeCyclingOn = TRUE;					   // set to off cycle
				upsBoss.chgCommand = AUTO_FAST;				  // report mode change
				upsBoss.batChgMode = SNMP_BAT_CHARGE_CHARGING; // SNMP battery status
#if (defined VERBOSE_DEBUG)
				if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						log_trace("charge cycle ends:%lu, fastChargeCyclingOn = TRUE", upsBoss.numberOfChargeCycles);
					}
				}
#endif
			}
		}
#endif // (defined BATTERY_CHARGE_CYCLING)
		break;
	case AUTO_ON_ALM: // alarm, charger off
		if (upsBoss.chgMode != chgModeLast)
		{
			chgModeLast = upsBoss.chgMode;
		}
		pinLatch(LATCH_DISABLE_CHG_n, 0);			  // Charge off
		break;
	default:
		upsBoss.chgCommand = AUTO_FAST;
		break;
	}	// 	switch (upsBoss.chgMode) // battery state machine
#else	// #ifdef BATTERY_CHARGE_CYCLING2
	switch(cyclicChargeMode)
	{
	case CYCLIC_CHARGE_WAIT:
		if (cyclicChargeMode != lastCyclicChargeMode)	// state entry code
		{
			lastCyclicChargeMode = cyclicChargeMode;
		}
		if (upsBoss.upsState == UPS_ON_UTIL)			// wait until UPS state machine gets to On Utility on cold start
		{
			cyclicLastUpsState = UPS_ON_UTIL;
			cyclicChargeMode = CYCLIC_CHARGE_START;		// start cyclic charger
			break;
		}
		break;
	case CYCLIC_CHARGE_START:
		if (cyclicChargeMode != lastCyclicChargeMode)	// state entry code
		{
			lastCyclicChargeMode = cyclicChargeMode;
			upsBoss.numberOfChargeCycles = 0;			// reset cycle counter
			batteryChargeCyclingTimer = getTime();		// start timer
			upsBoss.chgCommand = AUTO_FAST;				// start 30 second Fast Charge
#if (defined VERBOSE_DEBUG)
				if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						log_trace("CYCLIC_CHARGE_START");
					}
				}
#endif
		}
		if (upsBoss.upsState == UPS_ON_BAT)				// if system on battery
		{
			cyclicChargeMode = CYCLIC_CHARGE_OFF;		// go to charger off mode
			upsBoss.batChgMode = SNMP_BAT_CHARGE_DISCHARGING;
			break;
		}
		if (timer(batteryChargeCyclingTimer, 30000L))	// when timer expires
		{
			cyclicChargeMode = CYCLIC_CHARGE_OFF;		// go to off mode
#if (defined VERBOSE_DEBUG)
			if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					log_trace("ups.c cyclic charger cycle 0, going to off after 30 seconds Fast Charge");
				}
			}
#endif
		}
		break;
	case CYCLIC_CHARGE_OFF:
		if (cyclicChargeMode != lastCyclicChargeMode)		// state entry code
		{
			lastCyclicChargeMode = cyclicChargeMode;
			// if on 0 cycle, continue CYCLIC_CHARGE_START timing, otherwise start timer
			if (upsBoss.numberOfChargeCycles > 0)			// otherwise start timer
			{
				batteryChargeCyclingTimer = getTime();		// start timer
			}
			upsBoss.numberOfChargeCycles++;					// increment number of charge cycles
			if (upsBoss.numberOfChargeCycles >= BATTERY_CYCLING_CHARGE_MAX_CYCLES)
			{
				//upsBoss.chgCommand = AUTO_ON_ALM;
				cyclicChargeMode = CYCLIC_CHARGE_ALARM;		// go to off mode
			// EVENT_BATTERY_FAIL set in chgCommand
#if (defined VERBOSE_DEBUG)
				if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						log_trace("charge cycle:%lu, AUTO_ON_ALM", upsBoss.numberOfChargeCycles);
					}
				}
#endif
			}
			upsBoss.chgCommand = MANUAL_OFF;				// report mode change
			if (upsBoss.upsState == UPS_ON_BAT)
			    upsBoss.batChgMode = SNMP_BAT_CHARGE_DISCHARGING;
			else
			    upsBoss.batChgMode = SNMP_BAT_CHARGE_RESTING;	// SNMP battery status
#if (defined VERBOSE_DEBUG)
			if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					log_trace("charge cycle:%lu, Off Cycle", upsBoss.numberOfChargeCycles);
				}
			}
			if ((upsBoss.upsState == UPS_ON_BAT) && (upsBoss.chgMode != MANUAL_OFF))
			{
				upsBoss.chgCommand = MANUAL_OFF;
			}
#endif
		}	// state entry code

		if (upsBoss.upsState != cyclicLastUpsState)		// check for UPS state change
		{
			cyclicLastUpsState = upsBoss.upsState;

			if (upsBoss.upsState == UPS_ON_UTIL)		// transitioning from Battery State when utility returns
			{
				cyclicChargeMode = CYCLIC_CHARGE_START;		// restart cyclic charge
				break;										// leave this state
			}
		}
		if (upsBoss.upsState == UPS_ON_BAT)					// if system on battery
		{
			batteryChargeCyclingTimer = getTime();			// keep resetting timer, stay in this state
		}
		// Timer expires or forced
		if ( (timer(batteryChargeCyclingTimer, (BATTERY_CYCLING_CHARGE_ON_MINUTES * 60L * 1000L)))
		|| (debugCyclicChargingNextCycle == TRUE))			// debug end time function
		{
			debugCyclicChargingNextCycle = FALSE;		  	// reset flag

			cyclicChargeMode = CYCLIC_CHARGE_ON;
#if (defined VERBOSE_DEBUG)
			if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					log_trace("ups.c cyclic charger end of off cycle ");
				}
			}
#endif
		}
		break;
	case CYCLIC_CHARGE_ON:
		if (cyclicChargeMode != lastCyclicChargeMode)		// state entry code
		{
			lastCyclicChargeMode = cyclicChargeMode;

			batteryChargeCyclingTimer = getTime();			// start timer
			upsBoss.chgCommand = AUTO_FAST;					// report mode change
			upsBoss.batChgMode = SNMP_BAT_CHARGE_CHARGING;	// SNMP battery status
			debugMessageInhibit = FALSE; 					// only print "batteries charged" debug message once in this state
#if (defined VERBOSE_DEBUG)
			if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if ( ((upsBoss.upsComMode == UPS_COM_USER) || ( VERBOSE_DEBUG_PORT == UART_PORT1 )) && (debugMessageInhibit == FALSE))
				{
					log_trace("CYCLIC_CHARGE_ON");
				}
			}
#endif
		}

		if (upsBoss.upsState == UPS_ON_BAT)					// if system on battery
		{
			cyclicChargeMode = CYCLIC_CHARGE_OFF;			// go to charger off mode
			upsBoss.batChgMode = SNMP_BAT_CHARGE_DISCHARGING;
			break;
		}

		// keep updating in fast mode, will then start checking the time in slow mode
		if (upsBoss.chgMode == AUTO_FAST)
		{
			batteryChargeOnSlowTimer = getTime();
		}

		// after 5 minutes on slow charge
		if ((timer(batteryChargeOnSlowTimer, 5L * 60L * 1000L) && (upsBoss.chgMode == AUTO_SLOW)))
		{
			upsBoss.numberOfChargeCycles = 0;				// reset charge cycle count, batteries fully charged
#if (defined VERBOSE_DEBUG)
			if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if ( ((upsBoss.upsComMode == UPS_COM_USER) || ( VERBOSE_DEBUG_PORT == UART_PORT1 )) && (debugMessageInhibit == FALSE))
				{
					log_trace("Charger Cycling, Auto Slow Charge Timer expired, set number of Charge Cycles to 0, batteries charged");
					debugMessageInhibit = TRUE; 			// only print message once in this state
				}
			}
#endif
		}
		if ((upsBoss.ampChg <= BAT_CHG_SLOW_THRESHOLD) && (upsBoss.chgMode == AUTO_FAST))
		{
			upsBoss.chgCommand = AUTO_SLOW;
#if (defined VERBOSE_DEBUG)
			if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					log_trace("Charger Cycling upsBoss.chgCommand = AUTO_SLOW");
				}
			}
		}
#endif
		if ((upsBoss.ampChg >= BAT_CHG_FAST_THRESHOLD) && (upsBoss.chgMode == AUTO_SLOW))
		{
			upsBoss.chgCommand = AUTO_FAST;
#if (defined VERBOSE_DEBUG)
			if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if ( (upsBoss.upsComMode == UPS_COM_USER) || (VERBOSE_DEBUG_PORT == UART_PORT1) )
				{
					log_trace("Charger Cycling upsBoss.chgCommand = AUTO_FAST");
				}
			}
#endif
		}

		// Timer expires or forced
		if ( (timer(batteryChargeCyclingTimer, (BATTERY_CYCLING_CHARGE_ON_MINUTES * 60L * 1000L)))
		|| (debugCyclicChargingNextCycle == TRUE))			// debug end time function
		{
			debugCyclicChargingNextCycle = FALSE;		  	// reset flag

			cyclicChargeMode = CYCLIC_CHARGE_OFF;
#if (defined VERBOSE_DEBUG)
			if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if ( (upsBoss.upsComMode == UPS_COM_USER) || (VERBOSE_DEBUG_PORT == UART_PORT1) )
				{
					log_trace("ups.c cyclic charger end of On Cycle ");
				}
			}
#endif
		}
		break;
	case CYCLIC_CHARGE_ALARM:
		if (cyclicChargeMode != lastCyclicChargeMode)	// state entry code
		{
			lastCyclicChargeMode = cyclicChargeMode;
			upsBoss.chgCommand = AUTO_ON_ALM;
		}
		break;
	}
#endif	// #ifndef BATTERY_CHARGE_CYCLING2

	if (timer(batteryCapTimer, 1000)) // check battery charge percent occasionally
	{
		unsigned int batteryCapacityMethod;

		batteryCapTimer = getTime();
		batteryCapacityMethod = BAT_CAP_METHOD;
		switch (batteryCapacityMethod) // Compiler didn't like using constant for switch()
		{
		case 0: // Table lookup
			fTemp = batteryChargePercent();
			if (upsBoss.chgMode == AUTO_FAST)
			{
				fTemp = fMin(fTemp, 99.0); // 99% ceiling
			}
			else
			{
				fTemp = fMin(fTemp, 100.0); // 100% ceiling
			}
			upsBoss.batChgPct = fTemp;
// TODO: add estimated remaining battery time to table lookup and Crude linear calcs
// Scaling = 100% load disch time * Charge % remaining * 100%/load %
#if (defined BATTERY_MAX_TIME) // battery time in seconds
			upsBoss.estSecBat = (long)((float)BATTERY_MAX_TIME * (upsBoss.batChgPct * 0.01) * (100.0 / upsBoss.loadPctOut));
#else
			upsBoss.estSecBat = (long)((float)600 * (upsBoss.batChgPct * 0.01) * (100.0 / upsBoss.loadPctOut));
#endif
			upsBoss.estSecBat = lMax(upsBoss.estSecBat, 0L);
			upsBoss.estSecBat = lMin(upsBoss.estSecBat, (50L * 3600L)); // 50 hours max
			break;
		case 1: // Crude Linear
			fTemp = batteryChargePercent();
			if (upsBoss.chgMode == AUTO_FAST)
			{
				fTemp = fMin(fTemp, 99.0); // 99% ceiling
			}
			else
			{
				fTemp = fMin(fTemp, 100.0); // 100% ceiling
			}
			upsBoss.batChgPct = fTemp;
// TODO: add estimated remaining battery time to table lookup and Crude linear calcs
// Scaling = 100% load disch time * Charge % remaining * 100%/load %
#if (defined BATTERY_MAX_TIME) // battery time in seconds
			upsBoss.estSecBat = (long)((float)BATTERY_MAX_TIME * (upsBoss.batChgPct * 0.01) * (100.0 / upsBoss.loadPctOut));
#else
			upsBoss.estSecBat = (long)((float)600 * (upsBoss.batChgPct * 0.01) * (100.0 / upsBoss.loadPctOut));
#endif
			upsBoss.estSecBat = lMax(upsBoss.estSecBat, 0L);
			upsBoss.estSecBat = lMin(upsBoss.estSecBat, (50L * 3600L)); // 50 hours max
			break;
#if BAT_CAP_METHOD == 2				  // Joule method of battery capacity calculation
		case 2:						  // Joule
			switch (upsBoss.upsState) // TODO: add battery joules to NV memory
			{
			case UPS_INIT:
				upsBoss.batteryJoulesIncrement = 0.0; // initialize
				break;
			case UPS_ON_BAT:
			    upsBoss.batChgMode = SNMP_BAT_CHARGE_DISCHARGING;
#ifdef EXTERNAL_BATTERY_PACK_JUMPER_SENSE
				if (externalDioInput(1))			// read Din1
				{
					// decrement base joule count (without external batteries) by a ratio of internal to total number of batteries
					upsBoss.batteryJoulesIncrement += (upsBoss.ampBat * upsBoss.voltBat) * (EXTERNAL_BATTERY_PACT_JOULE_RATIO);
				}
				else
				{
					// Single set of batteries
					upsBoss.batteryJoulesIncrement += upsBoss.ampBat * upsBoss.voltBat;
				}
#else	// not defined EXTERNAL_BATTERY_PACK_JUMPER_SENSE
				upsBoss.batteryJoulesIncrement += upsBoss.ampBat * upsBoss.voltBat;
#endif	// defined
				iTemp = (int)upsBoss.batteryJoulesIncrement; // get integer part
				upsBoss.batteryJoules -= (long)iTemp;		 // won't do anything if less than 1
				upsBoss.batteryJoulesIncrement -= (float)iTemp;
				break;
			default: // utility available output on/off/fault
				switch (upsBoss.chgMode)
				{
				case AUTO_SLOW: // Slow Charge
					if (upsBoss.chgMode != chgModeLastJoule)
					{
						if (chgModeLastJoule == AUTO_FAST) // went from fast to slow charge, should be 80% charged
						{
							if (upsBoss.batteryJoules < (long)(BATTERY_MAX_JOULES * 0.8))
							{
								upsBoss.batteryJoules = (long)BATTERY_MAX_JOULES * 0.8;
							}
						}
						// used chgModeLastJoule above to see prior state was AUTO_FAST, now set to current state
						chgModeLastJoule = upsBoss.chgMode;
					}
#ifdef EXTERNAL_BATTERY_PACK_JUMPER_SENSE
					if (externalDioInput(1))
					{
						// decrement base joule count (without external batteries) by a ratio of internal to total number of batteries
						upsBoss.batteryJoulesIncrement += (upsBoss.ampChg * upsBoss.voltBat) * (EXTERNAL_BATTERY_PACT_JOULE_RATIO);
					}
					else
					{
						// single pack of batteries
						upsBoss.batteryJoulesIncrement += upsBoss.ampChg * upsBoss.voltBat;
					}
#else	// not defined EXTERNAL_BATTERY_PACK_JUMPER_SENSE
					upsBoss.batteryJoulesIncrement += upsBoss.ampChg * upsBoss.voltBat;
#endif	// defined
					iTemp = (int)upsBoss.batteryJoulesIncrement; // get integer part
					upsBoss.batteryJoules += (long)iTemp;		 // won't do anything if less than 1
					upsBoss.batteryJoulesIncrement -= (float)iTemp;
					break;
				case AUTO_FAST: // Fast Charge
					if (upsBoss.chgMode != chgModeLastJoule)
					{
						chgModeLastJoule = upsBoss.chgMode;
					}
					upsBoss.batteryJoulesIncrement += upsBoss.ampChg * upsBoss.voltBat;
					iTemp = (int)upsBoss.batteryJoulesIncrement; // get integer part
					upsBoss.batteryJoules += (long)iTemp;		 // won't do anything if less than 1
					upsBoss.batteryJoulesIncrement -= (float)iTemp;
					break;
/*				// MSJ never used and decrements instead of incrementing joules
				case AUTO_ON: // On battery, charger off
 
					if (upsBoss.chgMode != chgModeLastJoule)
					{
						chgModeLastJoule = upsBoss.chgMode;
					}
					// now it's subtracting from total available joules
					upsBoss.batteryJoulesIncrement -= upsBoss.ampBat * upsBoss.voltBat;
					iTemp = (int)upsBoss.batteryJoulesIncrement; // get integer part
					upsBoss.batteryJoules -= (long)iTemp;		 // won't do anything if less than 1
					upsBoss.batteryJoulesIncrement += (float)iTemp;
					break;
 */	
				case MANUAL_OFF: // put in manual mode, charger turned off
					if (upsBoss.chgMode != chgModeLastJoule)
					{
						chgModeLastJoule = upsBoss.chgMode;
					}
					break;
				} // switch (upsBoss.chgMode) end
				break;
			} // switch (upsBoss.upsState) end
			// clamp joules between 0 and max
			upsBoss.batteryJoules = lMin(upsBoss.batteryJoules, BATTERY_MAX_JOULES);
			upsBoss.batteryJoules = lMax(upsBoss.batteryJoules, 0L);
			// calculate percent based on current number and full charge number
			fTemp = ((float)upsBoss.batteryJoules / (float)BATTERY_MAX_JOULES) * 100.0;
			if (upsBoss.chgMode == AUTO_FAST)
			{
				fTemp = fMin(fTemp, 99.0); // 99% ceiling
			}
			/*
				This is needed for Charger Cycling otherwise when charger is in MANUAL_OFF, when it was just and 'else'
				it will go to 100% even if batteries are still not charged.
			*/
			else if (upsBoss.chgMode == AUTO_SLOW)
			{
				fTemp = fMin(fTemp, 100.0); // 100% ceiling
			}
			upsBoss.batChgPct = fMax(fTemp, 0.0); // floor
#ifdef EXTERNAL_BATTERY_PACK_JUMPER_SENSE
			if (externalDioInput(1))
			{
				if (upsBoss.powOut >= 1.0)			  // make sure power more than 1 watt
				{
					upsBoss.estSecBat = (long)(((float)upsBoss.batteryJoules * (1.0f/EXTERNAL_BATTERY_PACT_JOULE_RATIO)) / upsBoss.powOut); // calculate time remaining
				}
				else // don't divide by zero
				{
					upsBoss.estSecBat = (long)(((float)upsBoss.batteryJoules * (1.0f/EXTERNAL_BATTERY_PACT_JOULE_RATIO)) / 1.0f); // calculate time remaining
				}
			}
			else
			{
				if (upsBoss.powOut >= 1.0)			  // make sure power more than 1 watt
				{
					upsBoss.estSecBat = (long)((float)upsBoss.batteryJoules / upsBoss.powOut); // calculate time remaining
				}
				else // don't divide by zero
				{
					upsBoss.estSecBat = (long)((float)upsBoss.batteryJoules / 1.0f); // calculate time remaining
				}
			}
#else	// not defined EXTERNAL_BATTERY_PACK_JUMPER_SENSE
			if (upsBoss.powOut >= 1.0)			  // make sure power more than 1 watt
			{
				upsBoss.estSecBat = (long)((float)upsBoss.batteryJoules / upsBoss.powOut); // calculate time remaining
			}
			else // don't divide by zero
			{
				upsBoss.estSecBat = (long)((float)upsBoss.batteryJoules / 1.0f); // calculate time remaining
			}
#endif	// defined EXTERNAL_BATTERY_PACK_JUMPER_SENSE
			upsBoss.estSecBat = lMax(upsBoss.estSecBat, 0L);		  // not negative
			upsBoss.estSecBat = lMin(upsBoss.estSecBat, 50L * 3600L); // 50 hours max
			break;
#endif	// BAT_CAP_METHOD==2 end
		} // switch(batteryCapacityMethod) end
	}	 // if (timer(batteryCapTimer,1000)) end
	switch (upsBoss.dcMode)
	{
	case AUTO_OFF:
		if (upsBoss.dcMode != dcModeLast)
		{
			if (dcModeLast == AUTO_ON)
			{
				// leaving state to online or shutdown, save battery run time
				upsBoss.lastBatteryRuntime = upsBoss.secOnBat;
				sonalert(OFF);
				if (dcModeAlarm != DC_MODE_ALM_OFF)
				{
					dcModeAlarm = DC_MODE_ALM_OFF;
				}
			}
			dcModeLast = upsBoss.dcMode;
		}
		break;
	case AUTO_ON:
	case MANUAL_ON:
		// at lower power there is more drop across internal battery resistance so internal cell
		// voltage is higher than at the terminals, this compensates so at low power there won't
		// be deep damaging discharge.
		if (upsBoss.dcMode != dcModeLast)
		{
			dcModeLast = upsBoss.dcMode;
			upsBoss.timeBatRunStarted = getTime();
			sonalert(ON_BEEP);
			if (dcModeAlarm != DC_MODE_ALM_ON_BAT)
			{
				dcModeAlarm = DC_MODE_ALM_ON_BAT;
			}
		}
		if (upsBoss.upsState == UPS_ON_BAT)
		{
			// Update the last battery run time
			BRT = timeExpired(upsBoss.timeBatRunStarted, getTime());

			if (upsBoss.powOut <= 150.0)
			{
				lowBatAlarm = LOW_BATTERY_ALARM_LOW_POWER;
				lowBatShutdown = LOW_BATTERY_SD_LOW_POWER;
			}
			else
			{
				lowBatAlarm = LOW_BATTERY_ALARM;
				lowBatShutdown = LOW_BATTERY_SD;
			}
			upsBoss.lowBatAlarm = lowBatAlarm; // update UPS data structure
			upsBoss.lowBatShutdown = lowBatShutdown;
			if ((upsBoss.voltBat <= lowBatAlarm) && (dcModeAlarm != DC_MODE_ALM_LOW))
			{
				dcModeAlarm = DC_MODE_ALM_LOW;
				sonalert(ON_SOLID);

				addSystemEvent(EVENT_BATTERY_LOW_ALARM);
			}
			// Low Battery Warning
			//			if (upsBoss.bankConfig.lowBatAlarmTimer == 1) {
			//				switch(upsBoss.bankConfig.lowBatTime) {	// configuration bits
			if (pEventData->systemData.bankSwitches.bit.LOW_BAT_ALM == 1)
			{
				switch (pEventData->systemData.bankSwitches.bit.LOW_BAT_ALM_SET) // configuration bits
				{
				case 0:
					upsBoss.secBatWarning = 120L;
					break;
				case 2:
					upsBoss.secBatWarning = 300L;
					break;
				case 1:
					upsBoss.secBatWarning = 900L;
					break;
				case 3:
					upsBoss.secBatWarning = 1800L;
					break;
				}
			}
		}
		break;
	case AUTO_ON_ALM:
		break;
	default:
		break;
	}

	return status;
}

int pfc(void)
{
	int status;

	status = TRUE;

	return status;
}

int bypass(void)
{
	int status;
	static volatile operatingModesT ModeLast = NORMAL, bypassCommandLast = AUTO_OFF;
	static volatile timeT bypassTime;
    static volatile timeT realDynamicBypassTimer; // Clean up Refactor
#ifdef DYNAMIC_BYPASS
	static bool realDynamicBypassTimeStarted= FALSE;
#endif

	status = TRUE;

#ifdef DYNAMIC_BYPASS
    if (pEventData->systemData.bankSwitches.bit.DYNAMIC_BYPASS == 0)
	{
		switch (upsBoss.upsState)
		{
			case UPS_ON_UTIL:
				if (overCurrentCount > 0)
				{
					realDynamicBypassTimer = getTime();
					realDynamicBypassTimeStarted = TRUE;
					upsBoss.bypassCommand = MANUAL_ON;
				}

				if (realDynamicBypassTimeStarted)
				{
					if (timer(realDynamicBypassTimer, 1000))
					{
						upsBoss.bypassCommand = AUTO_OFF;
						realDynamicBypassTimeStarted = FALSE;
					} 
				}
				break;
			default:
				break;
		}
	}
#endif
	
	switch (upsBoss.bypassMode) // bypass operational state machine
	{
	case AUTO_OFF:
		if (upsBoss.bypassMode != ModeLast) // Bypass state has changed
		{
			ModeLast = upsBoss.bypassMode;
		}
		break;
#if (BYPASS_ON_OVERLOAD == TRUE)			// dynamic bypass active compiler switch
	case WARN:								// set in msec isr
		if (upsBoss.bypassMode != ModeLast) // Bypass state has changed
		{
			ModeLast = upsBoss.bypassMode;
			upsBoss.bypassCommand = MANUAL_ON; // force bypass on
			addSystemEvent(EVENT_ON_BYPASS);
			upsBoss.dynamicBypassTimer = getTime();
		}
		if (timer(upsBoss.dynamicBypassTimer, BYPASS_OVERLOAD_TIME))
		{
			upsBoss.bypassCommand = AUTO_OFF; // force bypass off or on?
			addSystemEvent(EVENT_AUTO_BYPASS);
		}
		break;
#endif
	}
	if (upsBoss.bypassMode == WARN) // overcurrent bypass condition
	{
		if (timer(bypassTime, 1000)) // has it timed out?
		{
			upsBoss.bypassMode = AUTO_OFF;
			BYPASS_AUTO1; // Both signals should be set each time bypass => auto
			BYPASS_AUTO2;
			upsBoss.bypassMode = NORMAL;
			addSystemEvent(EVENT_OFF_BYPASS);
		}
	}
	// bypass commands
	if (upsBoss.bypassCommand != bypassCommandLast) // Bypass state has changed
	{
		bypassCommandLast = upsBoss.bypassCommand;
		switch (upsBoss.bypassCommand)
		{
		case AUTO_OFF: // Auto, normally off, turns on when inverter fails
			upsBoss.bypassMode = AUTO_OFF;
			BYPASS_AUTO1; // Both signals should be set each time bypass => auto
			BYPASS_AUTO2;
			addSystemEvent(EVENT_AUTO_BYPASS);
			if ( BypassFaultState == TRUE )
			{
				addSystemEvent (EVENT_OFF_BYPASS); 
				BypassFaultState = FALSE;
				OverLoadEvent = FALSE;
			}
#if (defined VERBOSE_DEBUG)
			if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					log_trace("bypass in AUTO_OFF mode");
				}
			}
#endif
			break;
		case MANUAL_OFF:
			if (upsBoss.bypassMode == AUTO_ON)
			{
				addSystemEvent(EVENT_OFF_BYPASS);
			}
			upsBoss.bypassMode = MANUAL_OFF;
			BYPASS_FORCE_OFF1; // Both signals should be set each time
			BYPASS_FORCE_OFF2;
#if (defined VERBOSE_DEBUG)
			if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					log_trace("bypass in MANUAL_OFF mode");
				}
			}
#endif
			break;
		case MANUAL_ON:
			upsBoss.bypassMode = MANUAL_ON;
			BYPASS_FORCE_ON;
			addSystemEvent(EVENT_ON_BYPASS);
#if (defined VERBOSE_DEBUG)
			if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					log_trace("bypass in MANUAL_ON mode");
				}
			}
#endif
			break;
		default:
			break;
		}

// debug
//WPB Put guards
#if (defined VERBOSE_DEBUG) // Debug variables
        if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
		{
			// log_trace("Bypass Command = %s", getOpmodeStr(upsBoss.bypassCommand));
		}
#endif // (defined VERBOSE_DEBUG)
// debug end

		upsBoss.bypassCommand = bypassCommandLast = OFF; // only process command once, then OFF
	}
	return status;
}

int fans(void)
{
	int status;

	status = TRUE;

	return status;
}

static volatile timeT modeTimer, substateTimer;
static volatile buttonT lastButton = BUTTON_NONE;
static volatile int configSwitch = 0, substate = 0, substateLast = -1, delay;
static volatile Uint32 configTemp;
int status = TRUE, i, led;
static volatile operatingModesT displayModeLast = NULL_MODE;
static volatile timeT momentaryBypass;
static bool momentaryTimerStarted = FALSE;

void displayOnMode ()
{
	if (upsBoss.displayMode != displayModeLast)
	{
		displayModeLast = upsBoss.displayMode;
	}
	// button response
	if (upsBoss.displayButton != lastButton) // look for a button change
	{
		sonalert(SINGLE_BEEP); // Beep sonalert when buttons are pressed
		lastButton = upsBoss.displayButton;
		if (upsBoss.displayButton != BUTTON_NONE) // button pressed
		{
			switch (upsBoss.displayButton)
			{
			// done in ups state machine
			case BUTTON_ON: // F1
			    if (upsBoss.bypassState)
				{
					//WPB
					upsBoss.inverterCommand = AUTO_ON;
					if ( BypassFaultState == TRUE )
					{
						overloadBypassSequenceOffTimer = getTime();
						overloadBypassTimerOffStarted = TRUE;
#if (defined VERBOSE_DEBUG)
						if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
						{
							if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
							{
								log_trace("button => On upsBoss.inverterCommand = AUTO_ON"); 
								log_trace("overloadBypassTimerOffStarted = TRUE");
							}
						}
#endif
				    }
				}
#ifdef MOMENTARY_BYPASS_ENABLED			
				if ((upsBoss.invMode == AUTO_ON) && vinInRange(&upsBoss) && (momentaryTimerStarted == FALSE))
				{
					momentaryBypass = getTime();
					momentaryTimerStarted = TRUE;
					upsBoss.bypassCommand = MANUAL_ON;
#if (defined VERBOSE_DEBUG)

					if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
						{
							log_trace("((upsBoss.invMode == AUTO_ON) && vinInRange(&upsBoss) && (momentaryTimerStarted == FALSE)) => upsBoss.bypassCommand = MANUAL_ON");
						}
					}
#endif
				}
				else
#endif // MOMENTARY_BYPASS_ENABLED
				{
					upsBoss.displayButtonCmd = BUTTON_ON;
#if (defined VERBOSE_DEBUG)

					if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
						{
							log_trace("button => On");
						}
					}
#endif
				}

				break;
			case BUTTON_OFF:
				upsBoss.displayButtonCmd = BUTTON_OFF;
				//WPB
				if (overloadBypassOn)
				{
					upsBoss.bypassCommand = AUTO_OFF;
					upsBoss.bypassState = FALSE;
					overloadBypassOn = FALSE;
				}

#if (defined VERBOSE_DEBUG)

				if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						log_trace("button => Off");
					}
				}
#endif
				break;
			case BUTTON_SILENCE: // F3
				sonalert(OFF);
				upsBoss.displayButton = BUTTON_NONE; // only clear buttons used in this routine
#if (defined VERBOSE_DEBUG)

				if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						log_trace("button => Silence");
					}
				}
#endif
				break;
			case BUTTON_TEST: // F4
				upsBoss.displayMode = LIGHT_SHOW;
				upsBoss.displayButton = BUTTON_NONE;
#if (defined VERBOSE_DEBUG)
				if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						log_trace("button => Test");
					}
				}
#endif
				break;
			case BUTTON_FUNCTION: // F2
#if (defined VERBOSE_DEBUG)
				if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						log_trace("button => Function");
					}
				}
#endif
				break;
			case BUTTON_BANKS: // F2 and F4
				upsBoss.displayMode = CONFIGURATION;
				upsBoss.displayButton = BUTTON_NONE;
#if (defined VERBOSE_DEBUG)
				if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						log_trace("button => Bank Config (F2&F4)");
					}
				}
#endif
				break;
			case BUTTON_BYPASS: // F2 and F1
#if (defined VERBOSE_DEBUG)
				if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						log_trace("button => Bypass Toggle (F2&F1)");
					}
				}
#endif
				if ((upsBoss.invMode == AUTO_ON) && vinInRange(&upsBoss))
				{
					switch (upsBoss.bypassMode)
					{
					case OFF:
					case AUTO_OFF:
						upsBoss.bypassCommand = MANUAL_ON;
#if (defined VERBOSE_DEBUG)
						if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
						{
							if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
							{
								log_trace("Manual On");
							}
						}
#endif
						break;
					case MANUAL_ON:
						upsBoss.bypassCommand = AUTO_OFF;
#if (defined VERBOSE_DEBUG)
						if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
						{
							if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
							{
								log_trace("Auto Off");
							}
						}
#endif
						break;
					default:
#if (defined VERBOSE_DEBUG)
						if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
						{
							if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
							{
								log_trace("upsBoss.bypassMode unknown: %d", upsBoss.bypassMode);
							}
						}
#endif
						break;
					}
				}
				break;
			case BUTTON_NONE:
#if (defined VERBOSE_DEBUG)
				if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						log_trace("button => none");
					}
				}
#endif
				break;
			}
		}
	}
	displayLed(); // LED Update

}

const char* getButtonString(buttonT button)
{
	switch (button)
	{
	case BUTTON_ON:
	  return "ON";
	case BUTTON_OFF:
	  return "OFF";
	case BUTTON_SILENCE:
	  return "SILENCE";
	case BUTTON_TEST:
	  return "TEST";
	case BUTTON_FUNCTION:
	  return "FUNCTION";
	case BUTTON_BANKS:
	  return "BANKS";
	case BUTTON_BYPASS:
	  return "BYPASS";
	case BUTTON_NONE:
	  return "NONE";
	default:		
	  return "DEFAULT";
	}
}
void displayConfigurationMode()
{
    int bar = 0;
	int bank = 0;

	if (upsBoss.displayMode != displayModeLast)
	{
		displayModeLast = upsBoss.displayMode;
		configSwitch = 0;
		configTemp = pEventData->systemData.bankSwitches.all; // get current config
		modeTimer = getTime();
#if (defined VERBOSE_DEBUG)
		if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
		{
			if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				log_trace("Banks Switch configuration");
			}
		}
#endif
	}
	if (timer(modeTimer, 200000)) // config timeout 2 minutes
	{
#if (defined VERBOSE_DEBUG)
		if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
		{
			if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				log_trace("Banks Switch configuration timeout");
			}
		}
#endif
		upsBoss.displayMode = ON;
	}
	if (upsBoss.displayButton != lastButton) // look for a button change
	{
#if (defined VERBOSE_DEBUG)
		if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
		{
			if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				log_trace("displayButton = %s", getButtonString(upsBoss.displayButton));
			}
		}
#endif
		
		if (upsBoss.displayButton != lastButton) // button pressed
		{
			lastButton = upsBoss.displayButton;
			modeTimer = getTime(); // reset timeout
									// button response
			sonalert(SINGLE_BEEP); // Beep sonalert when buttons are pressed

			switch (upsBoss.displayButton)
			{
			case BUTTON_ON: // F1
				// see if selected switch is set
				if (configTemp & bitMask[configSwitch])
				{
					configTemp &= ~bitMask[configSwitch]; // unset
				}
				else
				{
					configTemp |= bitMask[configSwitch]; // set
				}
				break;
			case BUTTON_OFF:
				configTemp |= 0x20000000UL; // Set the THOR bit. Next time shutdown factory/time reset
				break;
			case BUTTON_SILENCE: // F3
			    configSwitch -= 1;
#if (defined VERBOSE_DEBUG)
				if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						log_trace("Silence:configSwitch-=1 : %d", configSwitch);
					}
				}
#endif
				break;
			case BUTTON_TEST: // F4
			    configSwitch += 1;
#if (defined VERBOSE_DEBUG)
				if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						log_trace("Test:configSwitch+=1 : %d", configSwitch);
					}
				}
#endif
				break;
			case BUTTON_FUNCTION: // F2
				configSwitch += 10;
#if (defined VERBOSE_DEBUG)
				if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						log_trace("Function:configSwitch+=10 : %d", configSwitch);
					}
				}
#endif
				break;
			case BUTTON_BANKS:						   // F2 and F4
				pEventData->systemData.bankSwitches.all = configTemp; // save changes
				saveConfiguration();
				bankSwitchBaudSet();	  // update baud rate based on changes
				Enhanced_PLL_UpdateParameters(); // Update EPLL parameters
				upsBoss.displayMode = ON; // go back to normal display
				modeTimer = getTime();
#if (defined VERBOSE_DEBUG)
				if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						log_trace("Banks Switch configuration F2&F4 exit");
					}
				}
#endif
				break;
			case BUTTON_NONE:
			default:
				break;
			}

			upsBoss.displayButton = BUTTON_NONE;
			if (configSwitch >= 30) // wrap around
			{
				configSwitch = 0;
			}
			else if (configSwitch < 0)
			{
				configSwitch = 29; // wrap around
			}

			if (configSwitch < 10)
			{
				displayLedSet(LED_DISP_BYPASS, LED_DISP_FLASH);
				displayLedSet(LED_DISP_SERVICE_BAT, LED_DISP_OFF);
				displayLedSet(LED_DISP_FAULT, LED_DISP_OFF);
				bank = 0;
#if (defined VERBOSE_DEBUG)
				if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						log_trace("Banks Switch 1 : BYPASS");
					}
				}
#endif
			}
			else if ((configSwitch >= 10) && (configSwitch < 20))
			{
				displayLedSet(LED_DISP_BYPASS, LED_DISP_OFF);
				displayLedSet(LED_DISP_SERVICE_BAT, LED_DISP_FLASH);
				displayLedSet(LED_DISP_FAULT, LED_DISP_OFF);
				bank = 1;
#if (defined VERBOSE_DEBUG)
				if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						log_trace("Banks Switch 2 : SERVICE_BAT");
					}
				}
#endif
			}
			else
			{
				displayLedSet(LED_DISP_BYPASS, LED_DISP_OFF);
				displayLedSet(LED_DISP_SERVICE_BAT, LED_DISP_OFF);
				displayLedSet(LED_DISP_FAULT, LED_DISP_FLASH);
				bank = 2;
#if (defined VERBOSE_DEBUG)
				if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						log_trace("Banks Switch 3 : FAULT");
					}
				}
#endif
			}
            // Update bit index location
			for (i=0;i<10;i++)
			{
				if (i+(bank*10) == configSwitch) // selected switch
				{
					led = LED_DISP_FLASH;
				}
				// set On Led to reflect selected switch condition, on or off
				else if (configTemp & bitMask[i+(bank*10)]) 
				{
					led = LED_DISP_SOLID;
				}
				else
				{
					led = LED_DISP_OFF;
				}
				// Set On LED to reflect selected switch condition, on or off
				if (i+(bank*10) == configSwitch)
				{
					if (configTemp & bitMask[configSwitch])
					{
						displayLedSet(LED_DISP_ON, LED_DISP_SOLID);
					}
					else
					{
						displayLedSet(LED_DISP_ON, LED_DISP_OFF);
					}
				}
                bar = i % 10;
				// set both bars to indicate selected bit (flash) and bit status
				switch (bar)
				{
				case 0:
					displayLedSet(LED_DISP_BAT5, led);
					break;
				case 1:
					displayLedSet(LED_DISP_BAT4, led);
					break;
				case 2:
					displayLedSet(LED_DISP_BAT3, led);
					break;
				case 3:
					displayLedSet(LED_DISP_BAT2, led);
					break;
				case 4:
					displayLedSet(LED_DISP_BAT1, led);
					break;
				case 5:
					displayLedSet(LED_DISP_LOAD5, led);
					break;
				case 6:
					displayLedSet(LED_DISP_LOAD4, led);
					break;
				case 7:
					displayLedSet(LED_DISP_LOAD3, led);
					break;
				case 8:
					displayLedSet(LED_DISP_LOAD2, led);
					break;
				case 9:
					displayLedSet(LED_DISP_LOAD1, led);
					break;
				}
			}
		}
	}

}

void displayLightsOn()
{
	if (upsBoss.displayMode != displayModeLast)
	{
		displayModeLast = upsBoss.displayMode;
		substate = 0;
		delay = 10;
		substateTimer = getTime();
		initDisplay(); // clear display
	} 
	switch (substate)
	{
	case 0:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
		displayLedSet(LED_DISP_LOAD1, LED_DISP_SOLID);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 1:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
		displayLedSet(LED_DISP_LOAD2, LED_DISP_SOLID);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 2:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
		displayLedSet(LED_DISP_LOAD3, LED_DISP_SOLID);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 3:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
		displayLedSet(LED_DISP_LOAD4, LED_DISP_SOLID);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 4:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
		displayLedSet(LED_DISP_LOAD5, LED_DISP_SOLID);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 5:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
		displayLedSet(LED_DISP_EXTDC, LED_DISP_SOLID);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 6:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
		displayLedSet(LED_DISP_BAT1, LED_DISP_SOLID);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 7:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
		displayLedSet(LED_DISP_BAT2, LED_DISP_SOLID);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 8:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
		displayLedSet(LED_DISP_BAT3, LED_DISP_SOLID);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 9:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
		displayLedSet(LED_DISP_BAT4, LED_DISP_SOLID);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 10:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
        displayLedSet(LED_DISP_BAT5, LED_DISP_SOLID);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 11:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
        displayLedSet(LED_DISP_OPTION, LED_DISP_SOLID);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 12:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
        displayLedSet(LED_DISP_ON, LED_DISP_SOLID);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 13:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
        displayLedSet(LED_DISP_BYPASS, LED_DISP_SOLID);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 14:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
        displayLedSet(LED_DISP_SERVICE_BAT, LED_DISP_SOLID);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 15:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
  		displayLedSet(LED_DISP_FAULT, LED_DISP_SOLID);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	default:
		//initDisplay();			  // clear display
		//upsBoss.displayMode = ON; // go back to normal display mode
		break;
	}
}

void displayLightsOff()
{
	if (upsBoss.displayMode != displayModeLast)
	{
		displayModeLast = upsBoss.displayMode;
		substate = 16;
		delay = 10;
		substateTimer = getTime();
	} 
	switch (substate)
	{
	case 16:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
		displayLedSet(LED_DISP_LOAD1, LED_DISP_OFF);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 17:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
		displayLedSet(LED_DISP_LOAD2, LED_DISP_OFF);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 18:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
		displayLedSet(LED_DISP_LOAD3, LED_DISP_OFF);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 19:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
		displayLedSet(LED_DISP_LOAD4, LED_DISP_OFF);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 20:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
		displayLedSet(LED_DISP_LOAD5, LED_DISP_OFF);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 21:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
		displayLedSet(LED_DISP_EXTDC, LED_DISP_OFF);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 22:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
		displayLedSet(LED_DISP_BAT1, LED_DISP_OFF);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 23:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
		displayLedSet(LED_DISP_BAT2, LED_DISP_OFF);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 24:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
		displayLedSet(LED_DISP_BAT3, LED_DISP_OFF);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 25:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
		displayLedSet(LED_DISP_BAT4, LED_DISP_OFF);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 26:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
        displayLedSet(LED_DISP_BAT5, LED_DISP_OFF);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 27:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
        displayLedSet(LED_DISP_OPTION, LED_DISP_OFF);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 28:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
        displayLedSet(LED_DISP_ON, LED_DISP_OFF);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 29:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
        displayLedSet(LED_DISP_BYPASS, LED_DISP_OFF);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 30:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
        displayLedSet(LED_DISP_SERVICE_BAT, LED_DISP_OFF);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 31:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
  		displayLedSet(LED_DISP_FAULT, LED_DISP_OFF);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	default:
		initDisplay();			  // clear display
		upsBoss.displayMode = ON; // go back to normal display mode
		break;
	}
}


void newDisplayLightShow()
{
	if (upsBoss.displayMode != displayModeLast)
	{
		displayModeLast = upsBoss.displayMode;
		substate = 0;
		delay = 10;
		substateTimer = getTime();
		initDisplay(); // clear display
		sonalert(SINGLE_BEEP); // Beep sonalert when buttons are pressed
	} 
	switch (substate)
	{
	case 0:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
		displayLedSet(LED_DISP_LOAD1, LED_DISP_SOLID);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 1:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
		displayLedSet(LED_DISP_LOAD2, LED_DISP_SOLID);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 2:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
		displayLedSet(LED_DISP_LOAD3, LED_DISP_SOLID);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 3:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
		displayLedSet(LED_DISP_LOAD4, LED_DISP_SOLID);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 4:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
		displayLedSet(LED_DISP_LOAD5, LED_DISP_SOLID);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 5:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
		displayLedSet(LED_DISP_EXTDC, LED_DISP_SOLID);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 6:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
		displayLedSet(LED_DISP_BAT1, LED_DISP_SOLID);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 7:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
		displayLedSet(LED_DISP_BAT2, LED_DISP_SOLID);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 8:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
		displayLedSet(LED_DISP_BAT3, LED_DISP_SOLID);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 9:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
		displayLedSet(LED_DISP_BAT4, LED_DISP_SOLID);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 10:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
        displayLedSet(LED_DISP_BAT5, LED_DISP_SOLID);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 11:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
        displayLedSet(LED_DISP_OPTION, LED_DISP_SOLID);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 12:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
        displayLedSet(LED_DISP_ON, LED_DISP_SOLID);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 13:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
        displayLedSet(LED_DISP_BYPASS, LED_DISP_SOLID);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 14:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
        displayLedSet(LED_DISP_SERVICE_BAT, LED_DISP_SOLID);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 15:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
  		displayLedSet(LED_DISP_FAULT, LED_DISP_SOLID);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 16:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
		displayLedSet(LED_DISP_LOAD1, LED_DISP_OFF);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 17:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
		displayLedSet(LED_DISP_LOAD2, LED_DISP_OFF);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 18:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
		displayLedSet(LED_DISP_LOAD3, LED_DISP_OFF);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 19:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
		displayLedSet(LED_DISP_LOAD4, LED_DISP_OFF);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 20:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
		displayLedSet(LED_DISP_LOAD5, LED_DISP_OFF);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 21:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
		displayLedSet(LED_DISP_EXTDC, LED_DISP_OFF);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 22:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
		displayLedSet(LED_DISP_BAT1, LED_DISP_OFF);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 23:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
		displayLedSet(LED_DISP_BAT2, LED_DISP_OFF);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 24:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
		displayLedSet(LED_DISP_BAT3, LED_DISP_OFF);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 25:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
		displayLedSet(LED_DISP_BAT4, LED_DISP_OFF);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 26:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
        displayLedSet(LED_DISP_BAT5, LED_DISP_OFF);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 27:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
        displayLedSet(LED_DISP_OPTION, LED_DISP_OFF);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 28:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
        displayLedSet(LED_DISP_ON, LED_DISP_OFF);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 29:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
        displayLedSet(LED_DISP_BYPASS, LED_DISP_OFF);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 30:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
        displayLedSet(LED_DISP_SERVICE_BAT, LED_DISP_OFF);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 31:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
		}
  		displayLedSet(LED_DISP_FAULT, LED_DISP_OFF);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	default:
		initDisplay();			  // clear display
		upsBoss.displayMode = ON; // go back to normal display mode
		break;
	}
}

void oldDisplayLightShow()
{
	if (upsBoss.displayMode != displayModeLast)
	{
		displayModeLast = upsBoss.displayMode;
		substate = 0;
		delay = 100;
		substateTimer = getTime();
		sonalert(SINGLE_BEEP); // Beep sonalert when buttons are pressed
	}
	switch (substate)
	{
	case 0:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
			initDisplay(); // clear display
		}
		displayLedSet(LED_DISP_LOAD5, LED_DISP_SOLID);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 1:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
			initDisplay(); // clear display
		}
		displayLedSet(LED_DISP_LOAD4, LED_DISP_SOLID);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 2:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
			initDisplay(); // clear display
		}
		displayLedSet(LED_DISP_LOAD3, LED_DISP_SOLID);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 3:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
			initDisplay(); // clear display
		}
		displayLedSet(LED_DISP_LOAD2, LED_DISP_SOLID);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 4:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
			initDisplay(); // clear display
		}
		displayLedSet(LED_DISP_LOAD1, LED_DISP_SOLID);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 5:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
			initDisplay(); // clear display
		}
		displayLedSet(LED_DISP_BAT5, LED_DISP_SOLID);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 6:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
			initDisplay(); // clear display
		}
		displayLedSet(LED_DISP_BAT4, LED_DISP_SOLID);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 7:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
			initDisplay(); // clear display
		}
		displayLedSet(LED_DISP_BAT3, LED_DISP_SOLID);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 8:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
			initDisplay(); // clear display
		}
		displayLedSet(LED_DISP_BAT2, LED_DISP_SOLID);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 9:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
			initDisplay(); // clear display
		}
		displayLedSet(LED_DISP_BAT1, LED_DISP_SOLID);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 10:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
			initDisplay(); // clear display
		}
		displayLedSet(LED_DISP_ON, LED_DISP_SOLID);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 11:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
			initDisplay(); // clear display
		}
		displayLedSet(LED_DISP_FAULT, LED_DISP_SOLID);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 12:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
			initDisplay(); // clear display
		}
		displayLedSet(LED_DISP_SERVICE_BAT, LED_DISP_SOLID);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 13:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
			initDisplay(); // clear display
		}
		displayLedSet(LED_DISP_BYPASS, LED_DISP_SOLID);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 14:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
			initDisplay(); // clear display
		}
		displayLedSet(LED_DISP_OPTION, LED_DISP_SOLID);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 15:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			substateTimer = getTime();
			initDisplay(); // clear display
		}
		displayLedSet(LED_DISP_REMOTE, LED_DISP_SOLID);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	case 16:
		if (substate != substateLast) // substate entry code
		{
			substateLast = substate;
			delay = 3000;
			substateTimer = getTime();
			initDisplay(); // clear display
		}
		displayLedSet(LED_DISP_VERT_LOAD1, LED_DISP_FLASH);
		displayLedSet(LED_DISP_VERT_LOAD2, LED_DISP_FLASH_ALT);
		displayLedSet(LED_DISP_VERT_LOAD3, LED_DISP_FLASH);
		displayLedSet(LED_DISP_VERT_LOAD4, LED_DISP_FLASH_ALT);
		displayLedSet(LED_DISP_VERT_LOAD5, LED_DISP_FLASH);
		displayLedSet(LED_DISP_VERT_BAT1, LED_DISP_FLASH_ALT);
		displayLedSet(LED_DISP_VERT_BAT2, LED_DISP_FLASH);
		displayLedSet(LED_DISP_VERT_BAT3, LED_DISP_FLASH_ALT);
		displayLedSet(LED_DISP_VERT_BAT4, LED_DISP_FLASH);
		displayLedSet(LED_DISP_VERT_BAT5, LED_DISP_FLASH_ALT);
		displayLedSet(LED_DISP_VERT_ON, LED_DISP_FLASH);
		displayLedSet(LED_DISP_VERT_FAULT, LED_DISP_FLASH_ALT);
		displayLedSet(LED_DISP_VERT_SERVICE_BAT, LED_DISP_FLASH);
		displayLedSet(LED_DISP_VERT_BYPASS, LED_DISP_FLASH_ALT);
		displayLedSet(LED_DISP_VERT_OPTION, LED_DISP_FLASH);
		displayLedSet(LED_DISP_REMOTE, LED_DISP_FLASH_ALT);
		if (timer(substateTimer, delay))
		{
			substate++;
		}
		break;
	default:
		initDisplay();			  // clear display
		upsBoss.displayMode = ON; // go back to normal display mode
		break;
	}

}
int display(void)
{

	// set state, block commands on startup
	if (!timer(upsBoss.timeSystemStart, 2000))
	{
		upsBoss.displayButton = upsBoss.displayButtonCmd = BUTTON_NONE;
	}

	if (timer(upsBoss.displayUpdateTimer, 100)) // update on a regular interval
	{
		upsBoss.displayUpdateTimer = getTime();
		if (fakeButtonEnabled)
		{
			 upsBoss.displayButton = BUTTON_TEST;
			 fakeButtonEnabled = FALSE;
		}
		switch (upsBoss.displayMode)
		{
		case ON:
		    displayOnMode();
			break;
		case CONFIGURATION: // set configuration soft switches
		    displayConfigurationMode();
			break;
		case LIGHT_SHOW:
		    //oldDisplayLightShow();
			newDisplayLightShow();
			break;
		case LIGHTS_ON:
		    displayLightsOn();
		    break;
		case LIGHTS_OFF:
		    displayLightsOff();
		    break;
		case OFF:
			if (upsBoss.displayMode != displayModeLast)
			{
				displayModeLast = upsBoss.displayMode;
				initDisplay();		   // clear display
				sonalert(SINGLE_BEEP); // Beep sonalert when buttons are pressed
			}
			break;
		default:
			upsBoss.displayMode = ON;
			break;
		}
	}

	// BUTTON_ON Pressed while interter is on go into Bypass mode for 10 seconds; check in voltage rance and momentaryBypass timer timeout

	if (momentaryTimerStarted)
	{
		if (!vinInRange(&upsBoss))
		{
			momentaryTimerStarted = FALSE;
			upsBoss.bypassCommand = MANUAL_ON;
		}

		if (timer(momentaryBypass, 10000))
		{
			momentaryTimerStarted = FALSE;
			upsBoss.bypassCommand = AUTO_OFF;
		}
	}

	return status;
}

int remote(void)
{
	remoteModeT remoteMode;
	int remoteState, remoteStateLast = 0, status;

	status = TRUE;
	if (pinLatchCheck(LATCH_REMOTE_IN_n)) // inverted state, flip it
	{
		remoteState = 0;
	}
	else
	{
		remoteState = 1;
	}
	if (remoteState != remoteStateLast)
	{
		remoteStateLast = remoteState;
		if (remoteState == 1)
		{
			addSystemEvent(EVENT_REMOTE_HIGH);
		}
		else
		{
			addSystemEvent(EVENT_REMOTE_LOW);
		}
		remoteMode = (remoteModeT)REMOTE_MODE; // set in system_config.h
		switch ((int)remoteMode)
		{
		case 0: // no function
			break;
		case 1: // startup/shutdown
			//switch(upsBoss.bankConfig.remoteCurrentLoopTurnoffDelay) {
			switch (pEventData->systemData.bankSwitches.bit.REMOTE_DELAY) // Modified to use bank switches
			{
			case 0:
				upsBoss.secShutdownDelay = 0L;
				break;
			case 2:
				upsBoss.secShutdownDelay = 120L;
				break;
			case 1:
				upsBoss.secShutdownDelay = 300L;
				break;
			case 3:
				upsBoss.secShutdownDelay = 900L;
				break;
			}
			if (remoteState == 1) // startup
			{
				upsBoss.secShutdownDelay = -1;
				upsBoss.delayedShutdown = FALSE; // reset shut down delay, in case it was active
			}
			else // shutdown
			{
				upsBoss.secShutdownDelay = upsBoss.secShutdownDelay;
				upsBoss.delayedShutdown = TRUE;
			}
			break;
		case 2: // battleshort 1=on, 0=0ff
			if (remoteState == 1)
			{
				upsBoss.battleshort = TRUE;
			}
			else
			{
				upsBoss.battleshort = FALSE;
			}
			break;
		case 3:					  // input transformer tap changer tap selected
			if (remoteState == 1) // 1=120V tap, 0=240V tap
			{
				upsBoss.remoteTapSelected = 120;
			}
			else
			{
				upsBoss.remoteTapSelected = 240;
			}
			break;
		}
	}

	return status;
}

/* 	digital I/O - Functions associated with external digital signals compatable with
	Micro Controller Board (PC00840) J4. This is also on J4 on the DSP Board.
	After discussing the 840 use of these signals not all of the signals on the
	840 board had been needed. I have Din0-9 and Dout0-2, this allows us to use
	the same wiring for existing systems.
	Future systems can also use optically isolated channels ISO_IN1&2 and ISO_OUT1&2.
*/
int externalDioInput(int channel)
{
	int status, i;
	static int state[10], firstTime = TRUE;
	static int stateLastDebug[10];

	status = TRUE;

	if (firstTime == TRUE)
	{
		firstTime = FALSE;
		for (i = 0; i <= 9; i++)					// set all states to zero
		{
			state[i] = stateLastDebug[i] = 0;
/*
#if (defined VERBOSE_DEBUG)
			stateLastDebug[i] = 0;
#endif
*/
		}
	}

	switch(channel)
	{
	case 0:
		break;
	case 1:
#ifdef EXTERNAL_BATTERY_PACK_JUMPER_SENSE
		if (pinLatchCheck(LATCH_DIN1_n)) 		// Pull up resistor, 0V when batteries plugged in
		{
			state[channel] = status = 1;
		}
		else
		{
			state[channel] = status = 0;
		}
#endif	// EXTERNAL_BATTERY_PACK_JUMPER_SENSE
		break;
	case 8:
#ifdef DUAL_INPUT_VOLTAGE
#if (defined VERBOSE_DEBUG)
    if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
    {
        if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
        {
			log_trace ("pinLatchCheck(LATCH_DIN8_n): %d", pinLatchCheck(LATCH_DIN8_n));
        }
    }
#endif	// (defined VERBOSE_DEBUG)
		if (pinLatchCheck(LATCH_DIN8_n))				// Returns '1' if IVD in 240 volt range
		{
			state[channel] = status = 0;
		}
		else
		{
			state[channel] = status = 1;
		}
#endif	// DUAL_INPUT_VOLTAGE
		break;
	default:
		break;
	}	// switch(channel)

#if (defined VERBOSE_DEBUG)
#if ((defined EXTERNAL_BATTERY_PACK_JUMPER_SENSE) || (defined DUAL_INPUT_VOLTAGE))
    if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
    {
        if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
        {
			if (state[channel] != stateLastDebug[channel])
			{
				stateLastDebug[channel] = state[channel];
				log_trace ("DIO Input Channel %d, State %d", channel, state[channel]);
			}
        }
    }
#endif	// EXTERNAL_BATTERY_PACK_JUMPER_SENSE
#endif	// (defined VERBOSE_DEBUG)
	state[0] = state[0];	stateLastDebug[0] = stateLastDebug[0];		// make compiler happy
	return status;
}	// end externalDioInput(channel)

// this will close the input relays when voltage is within acceptable range
// and open relays when running on battery
// Note: at this point ups state controller is using pinLatch() to control relays directly
int inputRelays(operatingModesT commandNow)
{
	int status;
	static int relayOn = FALSE;
	static operatingModesT command = AUTO; // command ON or OFF overrides AUTO, null doesn't change command

	status = TRUE;

	if (commandNow != LAST) // new command
	{
		command = commandNow;
	}
	switch (command)
	{
	case AUTO:
		switch (upsBoss.dcMode)
		{
		case AUTO_ON:
		case MANUAL_ON: // Running on battery
			if (relayOn == TRUE)
			{
				pinLatch(LATCH_IN_RLY1_n, 0); // Turn off relays
				pinLatch(LATCH_IN_RLY2_n, 0);
				relayOn = FALSE;
			}
			break;
		case AUTO_OFF:
		case MANUAL_OFF:
			// if input relays off but input voltage within acceptable range
			if ((relayOn == FALSE) && (fRange(upsBoss.voltIn, INPUT_RELAYS_MIN_ON, INPUT_RELAYS_MAX_ON)))
			{
				pinLatch(LATCH_IN_RLY1_n, 1); // Turn on relays
				pinLatch(LATCH_IN_RLY2_n, 1);
				relayOn = TRUE;
			}
			else if ((relayOn == TRUE) && ((upsBoss.voltIn >= INPUT_RELAYS_MAX_OFF) || (upsBoss.voltIn <= INPUT_RELAYS_MIN_OFF)))
			{
				pinLatch(LATCH_IN_RLY1_n, 0); // Turn off relays
				pinLatch(LATCH_IN_RLY2_n, 0);
				relayOn = FALSE;
			}
			break;
		case TEST:
			break;
		}
		break;
	case ON:
		if (relayOn == FALSE)
		{
			pinLatch(LATCH_IN_RLY1_n, 1); // Turn on relays
			pinLatch(LATCH_IN_RLY2_n, 1);
			relayOn = TRUE;
		}
		break;
	case OFF:
		if (relayOn == TRUE)
		{
			pinLatch(LATCH_IN_RLY1_n, 0); // Turn off relays
			pinLatch(LATCH_IN_RLY2_n, 0);
			relayOn = FALSE;
		}
		break;
	default:			// no valid command, go auto
		status = FALSE; // indicate command fail
		command = AUTO;
		break;
	}
	return status;
}

// End of basic UPS function routines

// Used to change gain and/or ramp to commanded gain.  Full gain = 100.0%
float updateInverterGain(float gainCommand)
{
	//static float gainTarget = 0;

	if (gainCommand != -1.0) // -1.0 means keep last command
	{
		gainTarget = gainCommand;
	}
	// Ramps from gain now to target gain.  *TODO: add ramp speed function
	if (gain != gainTarget)
	{
		if (gain < gainTarget)
		{
			gain += 2.0;
		}
		else
		{
			gain -= 2.0;
		}
		if (fabs(gain - gainTarget) <= 4.0)
		{
			gain = gainTarget;
		}
	}
	return gain;
}

// sets Inverter frequency, calculates sine table the first time it's called.
// after that it will take target freq and ramp speed, if given a negative frequency it will
// just update the ramp.
// Setting hzPerSec to negative number will set frequency without ramp
// returns current frequency.
// float setFrequency(float freq, float hzPerSec)
// {
// 	static volatile timeT rampTimer;
// 	static float freqTarget = UPS_FREQOUTNOM, freqNow = 0, hzPerSecTarget = 0;
// 	const int rampTimeMsec = 100;				 // update time interval in millisec.
// 	const float rampStep = rampTimeMsec / 1.0e3; // used to scale hzPerSec to interval time
// 	static int startUpFlag = TRUE;				 // Boolean flag to kick this party off with a good time

// 	// Is it our first time? Yes-> Update the freqRampTimer to current time, and lock the gate
// 	if (startUpFlag)
// 	{
// 		startUpFlag = FALSE;
// 		rampTimer = getTime();
// 	}

// 	// check frequency range, make sure freq is not -1 so it is not a frequency ramp operation
// 	if (((freq <= 45.0) || (freq >= 405.0)) && (freq != -1))
// 	{
// 		freq = 60.0; // outside range, set to 60
// 	}
// 	if (hzPerSec < 0) // no ramp
// 	{
// 		freqNow = freq;   // target, just set to freq
// 		if (freqNow <= 0) // guard against divide by zero
// 		{
// 			freqNow = 0.1;
// 		}
// 		SetPeriodCpuTimer(&CpuTimer1, DSPCLOCK_PER_USEC, (1 / (freqNow * INVERTER_UPDATES_PER_CYCLE)) * 1e6);
// 	}
// 	else // ramp operation to target at rate (hzPerSec)
// 	{
// 		if (freq >= 0) // set freqTarget and rate
// 		{
// 			freqTarget = freq;
// 			if (hzPerSec > 2.0) // limit from 0 to 2 Hz/sec
// 			{
// 				hzPerSecTarget = 2.0; // set freq ramp
// 			}
// 			else
// 			{
// 				hzPerSecTarget = hzPerSec; // set freq ramp
// 			}
// 		}
// 		else if (freq == -1) // update frequency ramp
// 		{
// 			if (timer(rampTimer, rampTimeMsec)) // ramp based on fixed period
// 			{
// 				rampTimer = getTime();	// update timer
// 				if (freqTarget > freqNow) // frequency too low
// 				{
// 					freqNow += hzPerSecTarget * rampStep; // increase
// 				}
// 				else if (freqTarget < freqNow) // frequency too high
// 				{
// 					freqNow -= hzPerSecTarget * rampStep; // decrease
// 				}
// 				if (freqNow <= 0) // guard against divide by zero
// 				{
// 					freqNow = 0.1;
// 				}
// 				SetPeriodCpuTimer(&CpuTimer1, DSPCLOCK_PER_USEC, (1 / (freqNow * INVERTER_UPDATES_PER_CYCLE)) * 1e6);
// 			}
// 		}
// 	}
// 	return freqNow;
// }

// set gain on modulation percent
void invGainSet(float cmdPct) // convert volt desired to gain command 0-100%
{
	gainTarget = cmdPct;
}

// gain is global to this module, allow other modules to access it
float invGetGain(void)
{
	return gain;
}

// let other modules access Sine lookup table
float getSineValue(int tableIndex)
{
	if (tableIndex >= INVERTER_UPDATES_PER_CYCLE)
	{
		tableIndex = INVERTER_UPDATES_PER_CYCLE;
	}
	else if (tableIndex < 0)
	{
		tableIndex = 0;
	}
	return sineLookup[tableIndex];
}

// let other modules access interleaved Sine lookup table
float getSineValueShifted(int tableIndex)
{
	return sineLookupShifted[tableIndex];
}

void setupPins(void)
{
	int i, j;

	// setup pin latches
	for (i = 0; i <= 7; i++)
	{
		for (j = 0; j <= 7; j++)
		{
			pinLatchState[i][j] = 0;
		}
	}
	// setup direction and initial state of GPIO pins
	// *TODO: setup GPIO
}

// Set the state of a latch pin by passing the latch number, pin number and state (0 or 1)
int pinLatch(int latch, int pin, int state)
{
	int check;

	if ((latch > 7) || (latch < 0) || (pin > 7) || (pin < 0) || (state > 1) || (state < 0))
	{
		check = FALSE; // parameters outside of acceptable range
	}
	else
	{
		check = TRUE;
	}

	pinLatchState[latch][pin] = state; // update array used to set latches
	return check;
}

// Read the state of a latch pin by passing the latch number and pin number
int pinLatchCheck(int latch, int pin)
{
	int state;

	if ((latch > 7) || (latch < 0) || (pin > 7) || (pin < 0))
	{
		state = -1; // parameters outside of acceptable range
	}
	else
	{
		state = pinLatchState[latch][pin]; // get requested pin state
	}

	return state;
}

void init_ups_com(struct upsDataStrucT *upsData) // *TODO - Add virtual UPS Nominal values in initiation
{
	volatile int i;
	volatile float iFloat, temp1;

	// calculate sine table (-1.0 to +1.0) for Sine Reference, Direct Control and phase shifted second leg
	for (i = 0; i < INVERTER_UPDATES_PER_CYCLE; i++)
	{
		iFloat = (float)i;
		// Angle in Radians cycle = 2*pi = 360 degrees
		temp1 = sin((iFloat / INVERTER_UPDATES_PER_CYCLE) * 2 * PI);
		sineLookup[i] = temp1;
		iFloat = (float)i + 0.5; // shift by 180 degrees = 1/2 period
		temp1 = sin((iFloat / INVERTER_UPDATES_PER_CYCLE) * 2 * PI);
		sineLookupShifted[i] = temp1;
	}

	upsBoss.upsNumber = 4;
	strcpy(upsData->upsId, "Unknown"); // in snmp com it responds with "^D00200"
	strcpy(upsData->model, UPS_PART);
	strcpy(upsData->man, UPS_MANUFACTURER);
	strcpy(upsBoss.verSoftware, VERSION);  // String representing Router version, set at beginning of main.c
	strcpy(upsOne.verSoftware, "Unknown"); // String representing Master software version
	strcpy(upsTwo.verSoftware, "Unknown"); // String representing Slave software version
	upsData->freqInNom = UPS_FREQINNOM;
	upsData->voltInNom = UPS_VOLTINNOM;
	upsData->timeLoBatNom = UPS_TIMELOBATNOM;
	upsData->freqOutNom = UPS_FREQOUTNOM;
	upsData->vaOutNom = (float)UPS_VAOUTNOM;
	upsData->powOutNom = UPS_POWOUTNOM;
	upsData->voltOutNom = UPS_VOLTOUTNOM;
	upsData->voltBatNom = NUM_CELLS * 2.0;
	upsData->voltSupply = 12.666;
	upsData->ampSupply = 3.666;
	upsData->msgStr[0] = NULL;
	upsData->msgChr = ' ';
	upsData->nextChrPos = 0;
	upsData->timeOutStart = getTime();
	upsData->masterCmdBot = 0;
	upsData->masterCmdTop = 0;
	upsData->masterCmdTot = 0;
	upsData->notifyMsg = FALSE;
	upsData->timedCmd = 0; // start with first command
	upsData->upsState = UPS_OFF;
	upsData->timeCmdMade.msec = 0;
	upsData->timeCmdMade.days = 0;
	upsData->comErrors = 0;
	upsData->invFaultAlm = OFF_ALARM;
	upsData->batCond = 0;
	upsData->batSts = 0;
	upsData->batChgMode = SNMP_BAT_CHARGE_RESTING;
	upsData->numberOfChargeCycles = 0;
	upsData->ampBat = 0;
	upsData->ampChg = 0;
	upsData->voltBat = NUM_CELLS * 2.0;
	upsData->batChgPct = 100.0;
	upsData->freqIn = UPS_FREQINNOM;
	upsData->voltIn = UPS_VOLTINNOM;
	upsData->freqOut = UPS_FREQOUTNOM;
	upsData->loadPctOut = 0;
	upsData->powOut = 0;
	upsData->voltOut = UPS_VOLTOUTNOM;
	upsData->vaOut = 0;
	upsData->pfOut = 0;
	upsData->ampOut = 0;
	upsData->voltBus = 0;
	upsData->voltSupply = 0;
	upsData->ampSupply = 0;
	upsData->tAmb = upsData->tSink1 = upsData->tSink2 = 20.0;
	upsData->tPfc = upsData->tExt1 = upsData->tExt2 = 20.0;
	upsData->invMode = MANUAL_OFF;
	upsData->dcMode = AUTO_OFF;
	upsData->chgMode = AUTO_SLOW;
	upsData->bypassMode = OFF;
	if (pEventData->systemData.bankSwitches.bit.INV_SYNC_TO_LINE == 1)
	{
		upsData->syncMode = AUTO_ON;
		upsData->zeroCrossDiff = getTime();
	}
	else
	{
		upsData->syncMode = MANUAL_OFF;
	}

	upsData->shutdownType = 1;
	upsData->secStartupDelay = -1;
	upsData->secShutdownDelay = -1;
	upsData->delayedShutdown = FALSE;
	upsData->fanMode = AUTO_FAST;
	upsData->fanFail = FALSE;
	upsData->batMode = NORMAL;
	upsData->battleshort = FALSE;
	upsData->battleshortLocal = FALSE;
	upsData->battleshortSNMP = FALSE;
	strcpy(upsData->almMask, "^RALMNNNS");
	upsData->inverterCommand = MANUAL_OFF;
	upsData->inverterFault = NORMAL;
	upsData->inverterOvercurrentCount = 0;
	upsData->eventDump = OFF;
	upsData->displayMode = ON;
	upsData->audAlmMode = OFF;
// 1=SNMP, 2=UPSILON or 3=USER
#if COM_PARSE == 1
	upsBoss.upsComMode = UPS_COM_SNMP;
	upsBoss.upsComModeLast = UPS_COM_SNMP;
#elif COM_PARSE == 2
		upsBoss.upsComMode = UPS_COM_UPSILON;
		upsBoss.upsComModeLast = UPS_COM_UPSILON;
#elif COM_PARSE == 3
		upsBoss.upsComMode = UPS_COM_USER;
		upsBoss.upsComModeLast = UPS_COM_USER;
#endif
	// sends message indicating operational change including parameters changing more than 5%
	upsData->notifyCustomerMsg = OFF;

#if (defined VERBOSE_DEBUG)
	upsBoss.verboseDebug = DEFAULT_VERBOSE_DEBUG;
#endif

#if (defined VERBOSE_DEBUG_SYNC)
	upsBoss.verboseDebugSync = TRUE;
#else
		upsBoss.verboseDebugSync = FALSE;
#endif
}


// TODO: modified to make compiler and remarks happy
char *returnEventStr(int i)
{
	// make happy
	if (i)
	{
		return ("yes");
	}
	else
	{
		return ("no");
	}
	// make happy end

}



void sonalert(operatingModesT mode)
{

	static timeT beepTime, beepTime2;
	static int beep = 0; // 0 = off, 1 = solid, 2 = beep on/off, 3 = single beep
	static operatingModesT modeLast = ON_SOLID;

	switch (mode)
	{
	case OFF:
		if (mode != modeLast)
		{
			modeLast = mode;
			SONALERT_OFF;
			beep = 0;
			upsBoss.audAlmMode = OFF;
		}
		break;
	case ON_SOLID:
		if (mode != modeLast)
		{
			modeLast = mode;
			SONALERT_ON;
			upsBoss.audAlmMode = ON_SOLID;
		}
		break;
	case ON_BEEP:
		if (mode != modeLast)
		{
			modeLast = mode;
			beep = 1;
			beepTime = getTime();
			upsBoss.audAlmMode = ON_BEEP;
		}
		break;
	case SINGLE_BEEP:
		if (mode != modeLast)
		{
			modeLast = mode;
			beep = 3;
			beepTime2 = getTime();
		}
		break;
	case UPDATE:
		modeLast = mode;
		if (((beep == 1) || (beep == 2)) && (timer(beepTime, 500)))
		{
			beepTime = getTime();
			if (beep == 1)
			{
				beep = 2;
				SONALERT_ON;
			}
			else
			{
				beep = 1;
				SONALERT_OFF;
			}
		}
		// single beep, make sure sonalert isn't already on or beeping
		if ((beep == 3) && (upsBoss.audAlmMode != ON_BEEP) && (upsBoss.audAlmMode != ON_SOLID))
		{
			SONALERT_ON;			  // turn on sonalert
			if (timer(beepTime2, 50)) // wait
			{
				SONALERT_OFF; // turn off sonalert after delay
				beep = 0;	 // reset beep state
			}
		}
		break;
	default:
		if (mode != modeLast)
		{
			modeLast = mode;
			//SONALERT_OFF;
			beep = 0;
			upsBoss.audAlmMode = OFF;
		}
		break;
	}
}

float batteryChargePercent(void)
{
	float percent;

// BAT_CAP_METHOD==2 Joule calculation done in batteries() these other methods called if selected
#if BAT_CAP_METHOD == 0						   // table lookup method
	const float inverseNumBat = 1.0 / NUM_BAT; // faster to multiply
											   // int percent;
	int i;

	// Make sure tables are right size, no way to check the elements
	if ((sizeof(batteryLookupHighPower) == 40) && (sizeof(batteryLookupLowPower) == 40))
	{
		if (upsBoss.powOut < 1200.0) // Two tables one for < 1200 Watts, other for > 1200 Watts
		{
			for (i = 0; i < BAT_TABLE_SIZE; i++)
			{
				if (batteryLookupLowPower[i] < (upsBoss.voltBat * inverseNumBat))
				{
					percent = (float)(100 - (i * 5)); // start at 100%, 5% down each element
					break;							  // no need to look further
				}
			}
		}
		else // > 1200 Watts
		{
			for (i = 0; i < BAT_TABLE_SIZE; i++)
			{
				if (batteryLookupHighPower[i] < (upsBoss.voltBat * inverseNumBat))
				{
					percent = (float)(100 - (i * 5)); // start at 100%, 5% down each element
					break;							  // no need to look further
				}
			}
		}
	}
	else
	{
		percent = -1.0; // failed table size test
	}
	return percent;
#elif BAT_CAP_METHOD == 1 // BAT_CAP_METHOD == 1, Crude calculation
		//int percent;

		if ((upsBoss.dcMode == AUTO_ON) || (upsBoss.dcMode == MANUAL_ON))
		{
			percent = ((upsBoss.voltBat - (NUM_CELLS * 1.67)) / 8) * 100.0;
			percent = fMax(percent, 1.0);
			percent = fMin(percent, 99.0);
		}
		else // charging
		{
			if ((upsBoss.chgMode == AUTO_FAST) || (upsBoss.chgMode == MANUAL_FAST))
			{
				percent = ((upsBoss.voltBat - (NUM_BAT * 12.00)) / (NUM_CELLS / 3)) * 100.0;
				percent = fMax(percent, 1.0);
				percent = fMin(percent, 99.0);
			}
			else
			{
				percent = 100.0;
			}
		}
		return percent;
#else
		// int percent;
		percent = 100.0; // stop compiler from complaining...
		return percent;
#endif
}

float thermistorResistanceToTemp(float resistance, float ohmThermistor, float beta)
{

	// equation 1/T = (1/To)+(1/B)*ln(R/Ro)
	// where T = temperature in Kelvin, To Ro = Resistance at To = 25C => 273.15 + 25 = 298.15K
	// B = Beta supplied by Manufacturer = 4400 for this thermistor
	// solve for T = (To * B) / (To * ln(R / Ro) + B) for Kelvin, subtract 273.15 for Centigrade

	return ((298.15 * beta) / (298.15 * log(resistance / ohmThermistor) + beta)) - 273.15;
}

float thermistorConvertExternal(float volts)
{

	float Vrt, IR1, IRdiv, Irt, Rtherm;
	// equivalent circuit Reference going through R1 RTherm then divider R2 R3 to ADC
	const float R1 = 47.5e3, R2 = 10.0e3, R3 = 10.0e3, Vref = 5.0;

	Vrt = volts * ((R2 + R3) / R3); // calculate voltage across Thermistor based on divider voltage
	IR1 = (Vref - Vrt) / R1;		// calculate current through R1
	IRdiv = volts / R3;				// calculate current through R3, same as R2
	Irt = IR1 - IRdiv;				// calculate difference current that is going through Thermistor
	Rtherm = Vrt / Irt;				// E=I*R => R=E/I
	// Convert Thermistor resistance to Centigrade
	return thermistorResistanceToTemp(Rtherm, 50.0e3, 4400.0);
}

float thermistorConvertInternal(float volts)
{

	float Vrt, Irt, Rtherm;
	// equivalent circuit Reference going through R1 RTherm then divider R2 R3 to ADC
	const float R1 = 4.99e3, R2 = 1.00e3, Vref = 3.3, OpAmpGainInToOut = 0.75;
	const float OpAmpGainOutToIn = 1 / OpAmpGainInToOut; // 1/opamp gain to represent voltage across Thermistor

	Vrt = volts * OpAmpGainOutToIn;							  // calculate voltage across Thermistor based on opamp gain looking backwards
	Irt = (Vref - Vrt) / (R1 + R2);							  // calculate current through Thermistor
	Rtherm = Vrt / Irt;										  // calculate resistance based on Thermistor voltage and current, E=I*R => R=E/I
	return thermistorResistanceToTemp(Rtherm, 5.0e3, 3980.0); // Convert Thermistor resistance to Centigrade
}

// take settings from system configuration file and set internal Bank Switches
Uint32 initBankSwitches(void)
{
	BankSwitchConfigurationBits_t data;
	data.all = 0;
	// data.all = bitMask[29];
	data.bit.INV_SYNC_TO_LINE = BANK3_5_SYNC_INV_TO_LINE;                  // 0 = OFF, 1 = SYNC
	data.bit.VERTICAL_DISPLAY = BANK1_1_VERTICAL_DISPLAY;				   // 1 = vertical, 0 = horizontal (not implemented)
	data.bit.AUTOSTART = BANK1_2_AUTOSTART;								   // 1 = Energizes output on startup, 0 = not
	data.bit.LOW_BAT_ALM = BANK1_3_LOW_BATTERY_ALARM;					   // 1 = Timed, 0 = Calculated
	data.bit.LOW_BAT_ALM_SET = BANK1_4_5_LOW_BAT_ALM_SET;				   // 0 = 120T/120C, 1 = 360T/900C, 2 = 240T/300C, 3 = 600T/1800C
	data.bit.RESERVED1 = 0; // 1 = Inhibit start up if there is a wire fault
	data.bit.RESERVED2 = 0;  // 1 = Inhibit Wire Fault Alarm
	data.bit.RESERVED3 = 0;												   // Reserved for future use
	data.bit.RS232_BAUD_RATE = BANK1_9_10_RS232_BAUD_RATE;				   // 0 = 1200, 1 = 4800, 2 = 2400, 3 = 9600
	data.bit.REMOTE_ON_OFF = BANK2_1_2_REMOTE_ON_OFF;					   // 0=Turn Off Enabled, 1=Disabled, 2=Turn On/Off Enabled, 3=Turn On Enabled
	data.bit.REMOTE_DELAY = BANK2_3_4_REMOTE_DELAY;						   // Turn off delay (seconds), 0 = 0, 1 = 300, 2 = 120, 3 = 900
	data.bit.BATTERY_TEST_INTERVAL = BANK2_5_BATTERY_TEST_INTERVAL;		   // 1 = 1 Month, 0 = 3 Months
	data.bit.OUTPUT_VOLT_SET = BANK2_6_7_INVERTER_VOLT_SET;				   // 0 = 100, 1 = 110, 2 = 115, 3 = 120
	data.bit.ALLOW_REMOTE_OFF = BANK2_8_REMOTE_OFF_BAT;					   // Remote can turn off system 0 = When on Battery only, 1 = When on battery or on utility
	data.bit.NARROW_INPUT_FREQ_RANGE = BANK2_10_SYNC_FREQ_RANGE;		   // 0 = +/- 3Hz, 1 = +/- 1.5Hz
	data.bit.BYPASS_STARTUP = BANK3_1_BYPASS_STARTUP;					   // 0 = Bypass on, 1 = Bypass off
	data.bit.DYNAMIC_BYPASS = BANK3_2_BYPASS_DYNAMIC;					   // Dynamic Bypass 0 = On, 1 = Off
	data.bit.INPUT_BAT_SETPOINT = BANK3_3_4_BAT_TRANSFER_SET;			   // Utility Voltage on/off battery, 0 = 85/95, 1 = 85/90, 2 = 80/85, 3 = 100/107
	data.bit.LOOP_RESPONSE = BANK3_5_INV_REG_RATE;						   // Inverter Regulation Rate 0=Fast, 1=Slow, Not used on Model F
	data.bit.SMART_BAT_SETPOINT = BANK3_6_7_8_BAT_SMART_SET;			   // Smart Battery Setpoints ????
	data.bit.REMOTE_MANUAL_START = BANK_REMOTE_MANUAL_START;			   // 1 = Startup in Manual, 0 = Startup in Auto. New Bank Switch for DSP
	data.bit.THOR = BANK3_10_RESET_BANKS;								   // Reset all bank switches to defaults, now two bits for extended functionality
	data.bit.REMOTE_INPUT = BANK_REMOTE_INPUT_IO;						   // Which input signal used for Remote command, (0=Din0 (DIO0), 1=Din1 (DIO1), 2=Din2 (DIO2), 0=Din3 (DIO3))
	return data.all;													   // Return the default bank switch data for this setup
} // End of initBankSwitches

// Convert Seconds to Hours:Minutes:Seconds
char *secToHmsString(long sec)
{
	static char buf[35];
	volatile long itemp;

	buf[0] = NULL;
	itemp = sec / (60 * 60);  // hours
	strcat(buf, itoa(itemp)); // print hours
	strcat(buf, ":");
	sec -= itemp * (60 * 60); // subtract hours
	itemp = sec / 60;		  // minutes
	if (itemp < 10)			  // if single digit print leading 0
	{
		strcat(buf, "0");
	}
	strcat(buf, itoa(itemp)); // print minutes
	strcat(buf, ":");
	sec -= itemp * 60; // subtract minutes
	if (sec < 10)	  // if single digit print leading 0
	{
		strcat(buf, "0");
	}
	strcat(buf, itoa(sec)); // print seconds
	return (&buf[0]);		// return string
}

// Convert Seconds to Minutes:Seconds
char *secToMsString(long sec)
{
	static char buf[35];
	volatile long itemp;

	buf[0] = NULL;
/* 	itemp = sec / (60 * 60);  // hours
	strcat(buf, itoa(itemp)); // print hours
	strcat(buf, ":");
	sec -= itemp * (60 * 60); 	// subtract hours
*/
	itemp = sec / 60;		  	// minutes
/* 	if (itemp < 10)			  	// if single digit print leading 0
	{
		strcat(buf, "0");
	}
*/
	strcat(buf, itoa(itemp)); 	// print minutes
	strcat(buf, ":");
	sec -= itemp * 60;			// subtract minutes
	if (sec < 10)	  // if single digit print leading 0
	{
		strcat(buf, "0");
	}
	strcat(buf, itoa(sec)); // print seconds
	return (&buf[0]);		// return string
}

char *timeString(timeT time)
{
	static char buf[35];
	volatile int itemp;
	volatile long ltemp;

	buf[0] = NULL;
	strcat(buf, itoa(time.days)); // print days
	strcat(buf, " days ");
	ltemp = time.msec / ((long)60 * 60 * 1000); // slight of hand, denom too large for integer
	itemp = (int)ltemp;							// but can only print int
	strcat(buf, itoa(itemp));					// print hours
	strcat(buf, ":");
	time.msec -= ltemp * ((long)60 * 60 * 1000); // subtract hours
	ltemp = time.msec / ((long)60 * 1000);
	itemp = (int)ltemp;
	if (itemp < 10) // if single digit print leading 0
	{
		strcat(buf, "0");
	}
	strcat(buf, itoa(itemp)); // print minutes
	strcat(buf, ":");
	time.msec -= ltemp * ((long)60 * 1000); // subtract minutes
	ltemp = time.msec / (1000);
	itemp = (int)ltemp;
	if (itemp < 10) // if single digit print leading 0
	{
		strcat(buf, "0");
	}
	strcat(buf, itoa(itemp));	// print seconds
	time.msec -= ltemp * (1000); // subtract seconds
	strcat(buf, ".");
	if (time.msec < 100) // if double digit print leading 0
	{
		strcat(buf, "0");
	}
	if (time.msec < 10) // if single digit print leading 0
	{
		strcat(buf, "0");
	}
	strcat(buf, itoa(time.msec)); // remaining milliseconds
	return (&buf[0]);			  // return string
}

char *runtimeString(void)
{

	return timeString(getTime());
}

static volatile int buttonCount[4] = {0, 0, 0, 0};
const int buttonMax = 12;

// Reads input latches, updates output latches including LED Display row and columns.
// Also scans Display buttons by turning on one LED column at a time and reading SW_ROW1
// since the 4 buttons are multiplexed on single signal.
void updateOctalLatchState(void)
{
	static volatile int overCurrentCount = 0, overCurrentDownCount = 0, column = 0, row = 0, blink = 0;
	static volatile int buttonScanCount = 0, buttonScan = FALSE;
	static volatile int /* buttonCount[4] = {0,0,0,0}, */ latch = 0;
	const int buttonScanInterval = 20, buttonOn = 10, ButtonOff = 3, /* buttonMax = 12, */ latchDelay1 = 10, latchDelay2 = 50;
	static int displayLatchDelay = 450;
	volatile int i, lastPinDirection = 0;
	volatile buttonT octalLatchButton = BUTTON_NONE;
	static volatile long lastMsec = 0;
	timeT timeNow;

	timeNow = getTime();

	if (timeNow.msec != lastMsec)
	{
		lastMsec = timeNow.msec;

		buttonScanCount++;
		if (buttonScanCount >= buttonScanInterval) // time to scan buttons?
		{
			buttonScanCount = 0; // yeppers, reset counter
			column = 0;
			buttonScan = TRUE; // notify the troops
		}

		for (latch = 0; latch <= 6; latch++)
		{
			//gpio(LATCH_CLK,0,GPIO_OUT);                       // set clock to zero
			GpioDataRegs.GPACLEAR.bit.GPIO20 = 1; // LATCH_CLK off
			for (i = 0; i <= latchDelay1; i++)	// just wait a little
			{
				NOP;
			}
			switch (latch) // set address
			{
			case 0:
				GpioDataRegs.GPBCLEAR.bit.GPIO41 = 1; // ADR0
				GpioDataRegs.GPBCLEAR.bit.GPIO34 = 1; // ADR1
				GpioDataRegs.GPBCLEAR.bit.GPIO53 = 1; // ADR2
				break;
			case 1:
				GpioDataRegs.GPBSET.bit.GPIO41 = 1;   // ADR0
				GpioDataRegs.GPBCLEAR.bit.GPIO34 = 1; // ADR1
				GpioDataRegs.GPBCLEAR.bit.GPIO53 = 1; // ADR2
				break;
			case 2:
				GpioDataRegs.GPBCLEAR.bit.GPIO41 = 1; // ADR0
				GpioDataRegs.GPBSET.bit.GPIO34 = 1;   // ADR1
				GpioDataRegs.GPBCLEAR.bit.GPIO53 = 1; // ADR2
				break;
			case 3:
				GpioDataRegs.GPBSET.bit.GPIO41 = 1;   // ADR0
				GpioDataRegs.GPBSET.bit.GPIO34 = 1;   // ADR1
				GpioDataRegs.GPBCLEAR.bit.GPIO53 = 1; // ADR2
				break;
			case 4:
				GpioDataRegs.GPBCLEAR.bit.GPIO41 = 1; // ADR0
				GpioDataRegs.GPBCLEAR.bit.GPIO34 = 1; // ADR1
				GpioDataRegs.GPBSET.bit.GPIO53 = 1;   // ADR2
				break;
			case 5:
				GpioDataRegs.GPBSET.bit.GPIO41 = 1;   // ADR0
				GpioDataRegs.GPBCLEAR.bit.GPIO34 = 1; // ADR1
				GpioDataRegs.GPBSET.bit.GPIO53 = 1;   // ADR2
				break;
			case 6:
				GpioDataRegs.GPBCLEAR.bit.GPIO41 = 1; // ADR0
				GpioDataRegs.GPBSET.bit.GPIO34 = 1;   // ADR1
				GpioDataRegs.GPBSET.bit.GPIO53 = 1;   // ADR2
				break;
			case 7: // there is no latch 7
				GpioDataRegs.GPBSET.bit.GPIO41 = 1; // ADR0
				GpioDataRegs.GPBSET.bit.GPIO34 = 1; // ADR1
				GpioDataRegs.GPBSET.bit.GPIO53 = 1; // ADR2
				break;
			}
			switch (latch) // update latches
			{
			case 0:
				EALLOW;
				GpioCtrlRegs.GPBDIR.bit.GPIO33 = 1; //	Latch	Data 0, set all to outputs
				GpioCtrlRegs.GPBDIR.bit.GPIO32 = 1; //			Data 1
				GpioCtrlRegs.GPADIR.bit.GPIO22 = 1; //			Data 2
				GpioCtrlRegs.GPADIR.bit.GPIO24 = 1; //			Data 3
				GpioCtrlRegs.GPADIR.bit.GPIO13 = 1; //			Data 4
				GpioCtrlRegs.GPBDIR.bit.GPIO58 = 1; //			Data 5
				GpioCtrlRegs.GPBDIR.bit.GPIO57 = 1; //			Data 6
				GpioCtrlRegs.GPADIR.bit.GPIO3 = 1;  //			Data 7
				EDIS;								// no break, drop into next case...

				// TODO: Display LED Guard Code
				// Turn off all rows and columns
				GpioDataRegs.GPBCLEAR.bit.GPIO33 = 1;       // DAT0
				GpioDataRegs.GPBCLEAR.bit.GPIO32 = 1;       // DAT1
				GpioDataRegs.GPACLEAR.bit.GPIO22 = 1;       // DAT2
				GpioDataRegs.GPACLEAR.bit.GPIO24 = 1;       // DAT3
				GpioDataRegs.GPACLEAR.bit.GPIO13 = 1;       // DAT4
				GpioDataRegs.GPBCLEAR.bit.GPIO58 = 1;       // DAT5
				GpioDataRegs.GPBCLEAR.bit.GPIO57 = 1;       // DAT6
				GpioDataRegs.GPACLEAR.bit.GPIO3 = 1;        // DAT7
				for (i=0;i<=latchDelay1;i++)                // just wait a little
				{
					NOP;
				}
				//gpio(LATCH_CLK,1,GPIO_OUT);               // set clock to 1 latch in data
				GpioDataRegs.GPASET.bit.GPIO20 = 1;         // LATCH_CLK
				for (i=0;i<=latchDelay1;i++)                // just wait a little
				{
					NOP;
				}
				//gpio(LATCH_CLK,0,GPIO_OUT);               // set clock to zero
				GpioDataRegs.GPACLEAR.bit.GPIO20 = 1;       // LATCH_CLK off
				for (i=0;i<=displayLatchDelay;i++)        // Wait for 5uSec
				{
					NOP;
				}
			case 1:
			case 2:
			case 3:
			case 4:
				if (pinLatchState[latch][0] == 0)
				{
					GpioDataRegs.GPBCLEAR.bit.GPIO33 = 1; // DAT0
				}
				else if (pinLatchState[latch][0] == 1)
				{
					GpioDataRegs.GPBSET.bit.GPIO33 = 1; // DAT0
				}
				if (pinLatchState[latch][1] == 0)
				{
					GpioDataRegs.GPBCLEAR.bit.GPIO32 = 1; // DAT1
				}
				else if (pinLatchState[latch][1] == 1)
				{
					GpioDataRegs.GPBSET.bit.GPIO32 = 1; // DAT1
				}
				if (pinLatchState[latch][2] == 0)
				{
					GpioDataRegs.GPACLEAR.bit.GPIO22 = 1; // DAT2
				}
				else if (pinLatchState[latch][2] == 1)
				{
					GpioDataRegs.GPASET.bit.GPIO22 = 1; // DAT2
				}
				if (pinLatchState[latch][3] == 0)
				{
					GpioDataRegs.GPACLEAR.bit.GPIO24 = 1; // DAT3
				}
				else if (pinLatchState[latch][3] == 1)
				{
					GpioDataRegs.GPASET.bit.GPIO24 = 1; // DAT3
				}
				if (pinLatchState[latch][4] == 0)
				{
					GpioDataRegs.GPACLEAR.bit.GPIO13 = 1; // DAT4
				}
				else if (pinLatchState[latch][4] == 1)
				{
					GpioDataRegs.GPASET.bit.GPIO13 = 1; // DAT4
				}
				if (pinLatchState[latch][5] == 0)
				{
					GpioDataRegs.GPBCLEAR.bit.GPIO58 = 1; // DAT5
				}
				else if (pinLatchState[latch][5] == 1)
				{
					GpioDataRegs.GPBSET.bit.GPIO58 = 1; // DAT5
				}
				if (pinLatchState[latch][6] == 0)
				{
					GpioDataRegs.GPBCLEAR.bit.GPIO57 = 1; // DAT6
				}
				else if (pinLatchState[latch][6] == 1)
				{
					GpioDataRegs.GPBSET.bit.GPIO57 = 1; // DAT6
				}
				if (pinLatchState[latch][7] == 0)
				{
					GpioDataRegs.GPACLEAR.bit.GPIO3 = 1; // DAT7
				}
				else if (pinLatchState[latch][7] == 1)
				{
					GpioDataRegs.GPASET.bit.GPIO3 = 1; // DAT7
				}
				for (i = 0; i <= latchDelay1; i++) // just wait a little
				{
					NOP;
				}
				GpioDataRegs.GPASET.bit.GPIO20 = 1; // LATCH_CLK
				break;
			case 5:
				EALLOW;
				GpioCtrlRegs.GPBDIR.bit.GPIO33 = 0; //	Latch	Data 0, set all to inputs
				GpioCtrlRegs.GPBDIR.bit.GPIO32 = 0; //			Data 1
				GpioCtrlRegs.GPADIR.bit.GPIO22 = 0; //			Data 2
				GpioCtrlRegs.GPADIR.bit.GPIO24 = 0; //			Data 3
				GpioCtrlRegs.GPADIR.bit.GPIO13 = 0; //			Data 4
				GpioCtrlRegs.GPBDIR.bit.GPIO58 = 0; //			Data 5
				GpioCtrlRegs.GPBDIR.bit.GPIO57 = 0; //			Data 6
				GpioCtrlRegs.GPADIR.bit.GPIO3 = 0;  //			Data 7
				EDIS;
				// don't break, flow into next case for latch 5&6
			case 6: // input from latches
				GpioDataRegs.GPASET.bit.GPIO20 = 1; // LATCH_CLK
				for (i = 0; i <= latchDelay1; i++)  // just wait a little
				{
					NOP;
				}
				GpioDataRegs.GPACLEAR.bit.GPIO20 = 1; // OC*
				for (i = 0; i <= latchDelay1; i++)	// just wait a little
				{
					NOP;
				}
				if (GpioDataRegs.GPBDAT.bit.GPIO33 == 0) // DAT0
				{
					pinLatchState[latch][0] = 0;
				}
				else
				{
					pinLatchState[latch][0] = 1;
				}
				if (GpioDataRegs.GPBDAT.bit.GPIO32 == 0) // DAT1
				{
					pinLatchState[latch][1] = 0;
				}
				else
				{
					pinLatchState[latch][1] = 1;
				}
				if (GpioDataRegs.GPADAT.bit.GPIO22 == 0) // DAT2
				{
					pinLatchState[latch][2] = 0;
				}
				else
				{
					pinLatchState[latch][2] = 1;
				}
				if (GpioDataRegs.GPADAT.bit.GPIO24 == 0) // DAT3
				{
					pinLatchState[latch][3] = 0;
				}
				else
				{
					pinLatchState[latch][3] = 1;
				}
				if (GpioDataRegs.GPADAT.bit.GPIO13 == 0) // DAT4
				{
					pinLatchState[latch][4] = 0;
				}
				else
				{
					pinLatchState[latch][4] = 1;
				}
				if (GpioDataRegs.GPBDAT.bit.GPIO58 == 0) // DAT5
				{
					pinLatchState[latch][5] = 0;
				}
				else
				{
					pinLatchState[latch][5] = 1;
				}
				if (GpioDataRegs.GPBDAT.bit.GPIO57 == 0) // DAT6
				{
					pinLatchState[latch][6] = 0;
				}
				else
				{
					pinLatchState[latch][6] = 1;
				}
				if (GpioDataRegs.GPADAT.bit.GPIO3 == 0) // DAT7
				{
					pinLatchState[latch][7] = 0;
				}
				else
				{
					pinLatchState[latch][7] = 1;
				}
				if (latch == 6)
				{
					GpioDataRegs.GPBCLEAR.bit.GPIO41 = 1; // ADR0
					GpioDataRegs.GPBCLEAR.bit.GPIO34 = 1; // ADR1
					GpioDataRegs.GPBCLEAR.bit.GPIO53 = 1; // ADR2
				}
				break;
			default: // output to latches
				break;
			} // end select
		}	 // end for
			  // latch_enable*, disabled in initTime(), after latches set the first time enable it
		LATCH_ENABLE_ON;

		// LED update
		if (buttonScan == FALSE) // normal Display LED updates
		{
			// Normally I would scan by column then turn on the row associated with the LEDs
			// that should be on for that column, however the Display board was designed to
			// scan by rows then turn on columns associated with the LED meant to be on for
			// that row
			switch (row)
			{
			case 0:
				pinLatchState[0][7] = 0; // Last Row = off
				pinLatchState[0][4] = 1; // This Row = on
				break;
			case 1:
				pinLatchState[0][4] = 0; // Last Row = off
				pinLatchState[0][5] = 1; // This Row = on
				break;
			case 2:
				pinLatchState[0][5] = 0; // Last Row = off
				pinLatchState[0][6] = 1; // This Row = on
				break;
			case 3:
				pinLatchState[0][6] = 0; // Last Row = off
				pinLatchState[0][7] = 1; // This Row = on
				break;
			default: // should never happen, guard anyway
				row = 0;
				break;
			}
			if (blink < 250)
			{
				for (column = 0; column <= 3; column++)
				{
					if (upsBoss.ledState1[column][row] == 0)
					{
						pinLatchState[0][column] = 0;
					}
					else
					{
						pinLatchState[0][column] = 1;
					}
				}
			}
			else
			{
				for (column = 0; column <= 3; column++)
				{
					if (upsBoss.ledState2[column][row] == 0)
					{
						pinLatchState[0][column] = 0;
					}
					else
					{
						pinLatchState[0][column] = 1;
					}
				}
			}
			blink++;
			if (blink >= 500)
			{
				blink = 0;
			}
			row++;
			if (row >= 4)
			{
				row = 0;
			}

			//debug.accIsrTimer++;
		}
		else // buttonScan == TRUE
		{
			// do this just before changing column so signal has 1msec to settle

			buttonScan = FALSE; // wait until next interval
			// set octal latch address to 0, display rows and columns, should be at this address after latch update above
			GpioDataRegs.GPBCLEAR.bit.GPIO41 = 1; // ADR0
			GpioDataRegs.GPBCLEAR.bit.GPIO34 = 1; // ADR1
			GpioDataRegs.GPBCLEAR.bit.GPIO53 = 1; // ADR2
			// remember and put back when done, EALLOW not needed for reads
			lastPinDirection = GpioCtrlRegs.GPBDIR.bit.GPIO33;
			if (lastPinDirection == 0)
			{
				EALLOW;
				GpioCtrlRegs.GPBDIR.bit.GPIO33 = 1; //	Latch	Data 0, set all to outputs
				GpioCtrlRegs.GPBDIR.bit.GPIO32 = 1; //			Data 1
				GpioCtrlRegs.GPADIR.bit.GPIO22 = 1; //			Data 2
				GpioCtrlRegs.GPADIR.bit.GPIO24 = 1; //			Data 3
				GpioCtrlRegs.GPADIR.bit.GPIO13 = 1; //			Data 4
				GpioCtrlRegs.GPBDIR.bit.GPIO58 = 1; //			Data 5
				GpioCtrlRegs.GPBDIR.bit.GPIO57 = 1; //			Data 6
				GpioCtrlRegs.GPADIR.bit.GPIO3 = 1;  //			Data 7
				EDIS;
			}
			// debug
			column = GpioDataRegs.GPADAT.bit.GPIO6;
			column = GpioDataRegs.GPBDAT.bit.GPIO33;
			// debug end
			for (column = 0; column <= 3; column++) // check for button press and debounce
			{
				switch (column)
				{
				case 0:									  // Test button, F4
														  // Turn off all rows, set column (1=on,0=off)
					GpioDataRegs.GPBSET.bit.GPIO33 = 1;   // DAT0 Column 1
					GpioDataRegs.GPBCLEAR.bit.GPIO32 = 1; // DAT1			2
					GpioDataRegs.GPACLEAR.bit.GPIO22 = 1; // DAT2			3
					GpioDataRegs.GPACLEAR.bit.GPIO24 = 1; // DAT3			4
					GpioDataRegs.GPACLEAR.bit.GPIO13 = 1; // DAT4		Row	1
					GpioDataRegs.GPBCLEAR.bit.GPIO58 = 1; // DAT5			2
					GpioDataRegs.GPBCLEAR.bit.GPIO57 = 1; // DAT6			3
					GpioDataRegs.GPACLEAR.bit.GPIO3 = 1;  // DAT7			4
					for (i = 0; i <= latchDelay2; i++)	// data setup time
					{
						NOP;
					}
					GpioDataRegs.GPASET.bit.GPIO20 = 1; // LATCH_CLK
					for (i = 0; i <= latchDelay2; i++)  // wait for latch
					{
						NOP;
					}
					GpioDataRegs.GPACLEAR.bit.GPIO20 = 1; // LATCH_CLK off
					if (BUTTON_PRESSED == 0)
					{
						buttonCount[column]++;
						buttonCount[column] = iMin(buttonCount[column], buttonMax);
					}
					else
					{
						buttonCount[column]--;
						buttonCount[column] = iMax(buttonCount[column], 0);
					}
					// if Test button pressed and Function button isn't, then put out Test
					if ((buttonCount[column] >= buttonOn) && (buttonCount[2] < 3))
					{
						octalLatchButton = BUTTON_TEST;
					}
					else if ((buttonCount[column] <= ButtonOff) && (octalLatchButton == BUTTON_TEST))
					{
					    octalLatchButton = BUTTON_NONE;
					}
					else
					{

					}
					break;
				case 1:									  // Silence button, F3 - used with Function to turn UPS off
					GpioDataRegs.GPBCLEAR.bit.GPIO33 = 1; // DAT0 Column 1
					GpioDataRegs.GPBSET.bit.GPIO32 = 1;   // DAT1        2
					for (i = 0; i <= latchDelay2; i++)	// data setup time
					{
						NOP;
					}
					GpioDataRegs.GPASET.bit.GPIO20 = 1; // LATCH_CLK
					for (i = 0; i <= latchDelay2; i++)  // wait for latch
					{
						NOP;
					}
					GpioDataRegs.GPACLEAR.bit.GPIO20 = 1; // LATCH_CLK off
					if (BUTTON_PRESSED == 0)
					{
						buttonCount[column]++;
						buttonCount[column] = iMin(buttonCount[column], buttonMax);
					}
					else
					{
						buttonCount[column]--;
						buttonCount[column] = iMax(buttonCount[column], 0);
					}
					// if Silence button pressed and Function button isn't, then put out Silence
					if (buttonCount[column] >= buttonOn) // button pressed
					{
					    octalLatchButton = BUTTON_SILENCE;
					}
					else if ((buttonCount[column] <= ButtonOff) && (octalLatchButton == BUTTON_SILENCE))
					{
					    octalLatchButton = BUTTON_NONE;
					}
					break;
				case 2:									  // Function button, F2 - used with Silence button to turn UPS off
														  // also used with Test button to go into Bank Switch setup
					GpioDataRegs.GPBCLEAR.bit.GPIO32 = 1; // DAT1 Column 2
					GpioDataRegs.GPASET.bit.GPIO22 = 1;   // DAT2        3
					for (i = 0; i <= latchDelay2; i++)	// data setup time
					{
						NOP;
					}
					GpioDataRegs.GPASET.bit.GPIO20 = 1; // LATCH_CLK
					for (i = 0; i <= latchDelay2; i++)  // wait for latch
					{
						NOP;
					}
					GpioDataRegs.GPACLEAR.bit.GPIO20 = 1; // LATCH_CLK off
					if (BUTTON_PRESSED == 0)
					{
						buttonCount[column]++;
						buttonCount[column] = iMin(buttonCount[column], buttonMax);
					}
					else
					{
						buttonCount[column]--;
						buttonCount[column] = iMax(buttonCount[column], 0);
					}
					if (buttonCount[column] >= buttonOn) // button pressed
					{
						if (buttonCount[1] >= buttonOn) // also Silence button pressed
						{
							octalLatchButton = BUTTON_OFF; // so notify system to shut down
						}
						else if (buttonCount[0] >= buttonOn) // also Test button pressed
						{
							octalLatchButton = BUTTON_BANKS; // so notify system to shut down
						}
						else if (buttonCount[3] >= buttonOn) // also On button pressed
						{
							octalLatchButton = BUTTON_BYPASS; // so notify system toggle bypass
						}
						else
						{
                            octalLatchButton = BUTTON_FUNCTION;
						}
					}
					else if ((buttonCount[column] <= ButtonOff) && ((octalLatchButton == BUTTON_OFF) || (octalLatchButton == BUTTON_BANKS)))
					{
					    octalLatchButton = BUTTON_NONE;
					}
					break;
				case 3:									  // On button, F1
					GpioDataRegs.GPACLEAR.bit.GPIO22 = 1; // DAT2 Column 3
					GpioDataRegs.GPASET.bit.GPIO24 = 1;   // DAT3        4
					for (i = 0; i <= latchDelay2; i++)	// data setup time
					{
						NOP;
					}
					GpioDataRegs.GPASET.bit.GPIO20 = 1; // LATCH_CLK
					for (i = 0; i <= latchDelay2; i++)  // wait for latch
					{
						NOP;
					}
					GpioDataRegs.GPACLEAR.bit.GPIO20 = 1; // LATCH_CLK off
					if (BUTTON_PRESSED == 0)
					{
						buttonCount[column]++;
						buttonCount[column] = iMin(buttonCount[column], buttonMax);
					}
					else
					{
						buttonCount[column]--;
						buttonCount[column] = iMax(buttonCount[column], 0);
					}
					// if On button pressed and Function button isn't, then put out BUTTON_ON
					if ((buttonCount[column] >= buttonOn) && (buttonCount[2] < 3))
					{
					    octalLatchButton = BUTTON_ON;
					}
					else if ((buttonCount[column] <= ButtonOff) && (octalLatchButton == BUTTON_ON))
					{
					    octalLatchButton = BUTTON_NONE;
					}
					GpioDataRegs.GPACLEAR.bit.GPIO24 = 1; // DAT3        4
					break;
				default:
					buttonScan = FALSE;
					break;
				}
			}
			if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if (upsBoss.displayButton != octalLatchButton)
				{
					if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
// #if (defined VERBOSE_DEBUG)
//                         if (upsBoss.verboseDebug == TRUE)
// 			            {
// 							switch (octalLatchButton)
// 							{
// 							case BUTTON_ON:
// 								log_trace("displayButton => On");
// 								break;
// 							case BUTTON_OFF:
// 								log_trace("displayButton => Off");
// 								break;
// 							case BUTTON_SILENCE:
// 								log_trace("displayButton => Silence");
// 								break;
// 							case BUTTON_TEST:
// 								log_trace("displayButton => Test");
// 								break;
// 							case BUTTON_FUNCTION:
// 								log_trace("displayButton => Function");
// 								break;
// 							case BUTTON_BANKS:
// 								log_trace("displayButton => Bank Config F2&F4");
// 								break;
// 							case BUTTON_BYPASS:
// 								log_trace("displayButton => Bypass Toggle F2&F1");
// 								break;
// 							case BUTTON_NONE:
// 								log_trace("displayButton => None");
// 								break;
// 							}
// 						}
// #endif
					}
				}
			}
			upsBoss.displayButton = octalLatchButton;

			if (lastPinDirection == 1)
			{
				EALLOW;
				GpioCtrlRegs.GPBDIR.bit.GPIO33 = 1; //	Latch	Data 0, set all to outputs
				GpioCtrlRegs.GPBDIR.bit.GPIO32 = 1; //			Data 1
				GpioCtrlRegs.GPADIR.bit.GPIO22 = 1; //			Data 2
				GpioCtrlRegs.GPADIR.bit.GPIO24 = 1; //			Data 3
				GpioCtrlRegs.GPADIR.bit.GPIO13 = 1; //			Data 4
				GpioCtrlRegs.GPBDIR.bit.GPIO58 = 1; //			Data 5
				GpioCtrlRegs.GPBDIR.bit.GPIO57 = 1; //			Data 6
				GpioCtrlRegs.GPADIR.bit.GPIO3 = 1;  //			Data 7
				EDIS;
			}
			else
			{
				EALLOW;
				GpioCtrlRegs.GPBDIR.bit.GPIO33 = 0; //	Latch	Data 0, set all to inputs
				GpioCtrlRegs.GPBDIR.bit.GPIO32 = 0; //			Data 1
				GpioCtrlRegs.GPADIR.bit.GPIO22 = 0; //			Data 2
				GpioCtrlRegs.GPADIR.bit.GPIO24 = 0; //			Data 3
				GpioCtrlRegs.GPADIR.bit.GPIO13 = 0; //			Data 4
				GpioCtrlRegs.GPBDIR.bit.GPIO58 = 0; //			Data 5
				GpioCtrlRegs.GPBDIR.bit.GPIO57 = 0; //			Data 6
				GpioCtrlRegs.GPADIR.bit.GPIO3 = 0;  //			Data 7
				EDIS;
			}
		}
	}
}

// function to fake button push, will fill debounce filter for single button and multiple buttons
// single buttons based on columns: 0=Test, 1=Silence, 2=Function, 3=On
void fakeButton(buttonT button)
{
	switch (button)
	{
	case BUTTON_ON:
		buttonCount[3] = buttonMax;
		break;
	case BUTTON_OFF:
		buttonCount[1] = buttonMax;
		buttonCount[2] = buttonMax;
		break;
	case BUTTON_SILENCE:
		buttonCount[1] = buttonMax;
		break;
	case BUTTON_TEST:
		buttonCount[0] = buttonMax;
		fakeButtonEnabled = TRUE;
		break;
	case BUTTON_FUNCTION:
		buttonCount[2] = buttonMax;
		break;
	case BUTTON_BANKS:
		buttonCount[1] = buttonMax;
		buttonCount[2] = buttonMax;
		buttonCount[3] = buttonMax;
		break;
	case BUTTON_NONE:
	default:
		buttonCount[0] = buttonCount[1] = buttonCount[2] = buttonCount[3] = 0;
		break;
	}
	
}

int vinInRange(struct upsDataStrucT *upsData)
{
	int inputInRange;
	switch (upsData->upsState)
	{
	case UPS_ON_UTIL: // On Utility, inverter on.
	case UPS_OFF:	 // System powered but Inverter off
	case UPS_INIT:	// Initial state when router/system powers up
// outside input voltage range, force to battery
#if defined DUAL_INPUT_VOLTAGE
		inputInRange = TRUE;
		if (externalDioInput(8))				// Returns '1' if IVD in 240 volt range
		{
			// check range
			if ((upsData->voltIn > (upsData->voltInNom * 2.0 * UPS_VOLTIN_HIGH)) || (upsData->voltIn < (upsData->voltInNom * 2.0 * UPS_VOLTIN_LOW)))
			{
				inputInRange = FALSE;
			}
		}
		else // Input tap changer on 120V tap
		{
			if ((upsData->voltIn > (upsData->voltInNom * UPS_VOLTIN_HIGH)) || (upsData->voltIn < (upsData->voltInNom * UPS_VOLTIN_LOW))) // overvoltage
			{
				inputInRange = FALSE;
			}
		}
#else					   // !defined UPS_VOLTIN_LOW
#if defined UPS_VOLTIN_LOW // overides bankswitch setpoint and dual input
			inputInRange = TRUE;
			if (upsData->voltIn <= (UPS_VOLTIN_LOW * upsData->voltInNom))
			{
				inputInRange = FALSE;
			}
			if (upsData->voltIn >= (UPS_VOLTIN_HIGH * upsData->voltInNom))
			{
				inputInRange = FALSE;
			}
#else					   // !defined DUAL_INPUT_VOLTAGE
			inputInRange = TRUE;
			switch (pEventData->systemData.bankSwitches.bit.INPUT_BAT_SETPOINT)
			{
			case 0: // 85/95 on/off battery
			case 1: // 85/90 on/off battery
				if (upsData->voltIn <= 85.0)
				{
					inputInRange = FALSE;
				}
				break;
			case 2: // 80/85 on/off battery
				if (upsData->voltIn <= 80.0)
				{
					inputInRange = FALSE;
				}
				break;
			case 3: // 100/107 on/off battery
				if (upsData->voltIn <= 100.0)
				{
					inputInRange = FALSE;
				}
				break;
			}
#endif					   // defined DUAL_INPUT_VOLTAGE
#endif					   // defined UPS_VOLTIN_LOW
		break;
	case UPS_ON_BAT: // On battery, inverter on
// inside input voltage range, transfer to utility
#if defined DUAL_INPUT_VOLTAGE
		inputInRange = TRUE;
		if (externalDioInput(8))				// Returns '1' if IVD in 240 volt range
		{
			// check range
			if ((upsData->voltIn > (upsData->voltInNom * 2.0 * UPS_VOLTIN_HIGH * 0.95)) || (upsData->voltIn < (upsData->voltInNom * 2.0 * UPS_VOLTIN_LOW * 1.05)))
			{
				inputInRange = FALSE;
			}
		}
		else // Input tap changer on 120V tap
		{
			if ((upsData->voltIn > (upsData->voltInNom * UPS_VOLTIN_HIGH * 0.95)) || (upsData->voltIn < (upsData->voltInNom * UPS_VOLTIN_LOW * 1.05))) // overvoltage
			{
				inputInRange = FALSE;
			}
		}
#else					   // !defined UPS_VOLTIN_LOW
#if defined UPS_VOLTIN_LOW // overides bankswitch setpoint and dual input
			inputInRange = TRUE;
			if (upsData->voltIn <= (UPS_VOLTIN_LOW * upsData->voltInNom * 1.05)) // Hysterysis
			{
				inputInRange = FALSE;
			}
			if (upsData->voltIn >= (UPS_VOLTIN_HIGH * upsData->voltInNom * 0.95))
			{
				inputInRange = FALSE;
			}
#else					   // !defined DUAL_INPUT_VOLTAGE
			inputInRange = TRUE;
			switch (pEventData->systemData.bankSwitches.bit.INPUT_BAT_SETPOINT)
			{
			case 0: // 85/95 on/off battery
				if (upsData->voltIn <= 95.0)
				{
					inputInRange = FALSE;
				}
				break;
			case 1: // 85/90 on/off battery
				if (upsData->voltIn <= 90.0)
				{
					inputInRange = FALSE;
				}
				break;
			case 2: // 80/85 on/off battery
				if (upsData->voltIn <= 85.0)
				{
					inputInRange = FALSE;
				}
				break;
			case 3: // 100/107 on/off battery
				if (upsData->voltIn <= 107.0)
				{
					inputInRange = FALSE;
				}
				break;
			}
			// check for high voltage state
			if (upsData->voltIn >= (UPS_VOLTIN_HIGH * upsData->voltInNom * 0.95))
			{
				inputInRange = FALSE;
			}
#endif					   // defined DUAL_INPUT_VOLTAGE
#endif					   // defined UPS_VOLTIN_LOW
		break;
	}
	return inputInRange;
}

uint8_t getFastChargeCyclingOn()
{
	return fastChargeCyclingOn;
}
