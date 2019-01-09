// timer.c

#include "timer.h"
#include "types.h"
#include "system_config.h" // includes file name for configuration
#include SYSTEM_CONFIG	 // configuration header file
#include "epwm.h"
#include "ups.h"
#include "utilities.h"
#include "event_controller.h"
#include "Enhanced_PLL.h"
#include "string.h"
#if ((defined VERBOSE_DEBUG) && (defined VERBOSE_DEBUG_SYNC)) // Debug variables
#include "logpll.h"
#include "uart.h"
#include "stdlib.h"
#include "stdio.h"
static char syncDebugMsg[200];
#endif // ((defined VERBOSE_DEBUG) && (defined VERBOSE_DEBUG_SYNC))

// External variables
//int delayedStartup /* = FALSE */;
extern timeT /*runTime,*/ timeStarted, /*delayedStartupTime,*/ timeUpsOff;
//struct timeT runTime, timeStarted ,delayedStartupTime, timeUpsOff;
timeT runTime;
extern int gSineIndex;
extern uint32_t overCurrentCount;
extern event_controller_data_t eventData, *pEventData;
volatile int adcDone;
extern uint16_t countEnhancedPLL;
extern real_T acIn;
extern real_T acOut;
uint8_t enhancedPLLInitialized = FALSE;
real_T zeroCrossDiffMsec = 0.0;

//volatile struct adcRawT adcRawNow, adcRawLast;
//int gSineIndex = 0;

// local variables

volatile const int latchDataPin[] = // assign the GPIO number to latch data pins
	{
		/* LATCH_DAT0 */ 33,
		/* LATCH_DAT1 */ 32,
		/* LATCH_DAT2 */ 22,
		/* LATCH_DAT3 */ 24,
		/* LATCH_DAT4 */ 13,
		/* LATCH_DAT5 */ 58,
		/* LATCH_DAT6 */ 57,
		/* LATCH_DAT7 */ 3};

volatile rtc_timeT rtcTime;

void initTime(void)
{
	runTime.days = 0;
	runTime.msec = 0;

	// set up octal latch GPIO
	LATCH_ENABLE_OFF;					  // latch_enable*, disable output to start
	GpioDataRegs.GPACLEAR.bit.GPIO20 = 1; // LATCH_CLK unset
	EALLOW;
	GpioCtrlRegs.GPADIR.bit.GPIO20 = 1; // LATCH_CLK set as output
	GpioCtrlRegs.GPBDIR.bit.GPIO41 = 1; // ADR0			"
	GpioCtrlRegs.GPBDIR.bit.GPIO34 = 1; // ADR1			"
	GpioCtrlRegs.GPBDIR.bit.GPIO53 = 1; // ADR2			"
	EDIS;

	InitCpuTimers(); // For this example, only initialize the Cpu Timers
	// Configure CPU-Timer 0 to interrupt every 500 milliseconds:
	// 150MHz CPU Freq, 50 millisecond Period (in uSeconds)
	ConfigCpuTimer(&CpuTimer0, DSPCLOCK_MHZ, 1000); // 1 millisecond period for main timer
	// set to time of period divided by updates per period
	ConfigCpuTimer(&CpuTimer1, DSPCLOCK_MHZ, (1 / (UPS_FREQINNOM * INVERTER_UPDATES_PER_CYCLE)) * 1e6);
	ConfigCpuTimer(&CpuTimer2, DSPCLOCK_MHZ, 130);
	// To ensure precise timing, use write-only instructions to write to the entire register. Therefore, if any
	// of the configuration bits are changed in ConfigCpuTimer and InitCpuTimers (in DSP2833x_CpuTimers.h), the
	// below settings must also be updated.
	// Interrupts that are used in this example are re-mapped to
	// ISR functions found within this file.
	EALLOW; // This is needed to write to EALLOW protected registers
	PieVectTable.TINT0 = &cpu_timer0_isr;
	PieVectTable.TINT1 = &cpu_timer1_isr;
	PieVectTable.TINT2 = &cpu_timer2_isr;
	EDIS; // This is needed to disable write to EALLOW protected registers

	CpuTimer0Regs.TCR.all = 0x4001; // Use write-only instruction to set TSS bit = 0
	CpuTimer1Regs.TCR.all = 0x4001; // Use write-only instruction to set TSS bit = 0
	CpuTimer2Regs.TCR.all = 0x4001; // Use write-only instruction to set TSS bit = 0
	// Enable CPU int1 which is connected to CPU-Timer 0, CPU int13
	// which is connected to CPU-Timer 1, and CPU int 14, which is connected
	// to CPU-Timer 2:
	IER |= M_INT1;  // CPU-Timer0
	IER |= M_INT13; // CPU-Timer1
	// CPU Timer 2 not used at this time
	IER |= M_INT14; // CPU-Timer2

	// Enable TINT0 in the PIE: Group 1 interrupt 7
	PieCtrlRegs.PIEIER1.bit.INTx7 = 1;

	EALLOW;
	SysCtrlRegs.PCLKCR0.bit.TBCLKSYNC = 1;
	EDIS;
}

// Millisecond Timer ISR
// This accumulates the runtime, senses/debounces the buttons and updates the octal latches
// Functions that will be run from RAM need to be assigned to
// a different section.  This section will then be mapped using
// the linker cmd file.
// look at CLA example files in reference folder, this project
// need to modify linker command file as well
//#pragma CODE_SECTION(cpu_timer0_isr, "ramfuncs");
interrupt void cpu_timer0_isr(void)
{
	static volatile unsigned int overCurrentDownCount = 0;
	//volatile unsigned int i, lastPinDirection=0;
	// debug
	volatile int scanflag = FALSE;
	volatile timeT timestart, timeEnd;
	int invOc;

// debug
#if ((defined VERBOSE_DEBUG) && (defined VERBOSE_DEBUG_SYNC)) // Debug variables
	static unsigned int overCurrentCountLast = 0;
	int latchAcEnable;
#endif // ((defined VERBOSE_DEBUG) && (defined VERBOSE_DEBUG_SYNC))
	   // debug end

	// debug
	timestart = getTime();
	// debug end

	runTime.msec++;
	if (runTime.msec >= (long)24 * 60 * 60 * 1000) // if we are at 24hours worth of msec 86,400,000
	{
		runTime.days++;			// bump days
		runTime.msec = (long)0; // reset milliseconds
	}

// Inverter Overcurrent countdown/limit
// if inverter is off or inverter disabled, don't count
// debug
#if ((defined VERBOSE_DEBUG) && (defined VERBOSE_DEBUG_SYNC)) // Debug variables
	if (pinLatchCheck(LATCH_AC_ENABLE_n) == 1)
	{
		latchAcEnable = FALSE;
	}
	else
	{
		latchAcEnable = TRUE;
	}
#endif

	if (INV_OC_NOT)
	{
		invOc = FALSE;
	}
	else
	{
		invOc = TRUE;
	}
	// debug end
	if ((upsBoss.invMode == AUTO_OFF) || (upsBoss.invMode == MANUAL_OFF) || (upsBoss.invMode == AUTO_RAMP) || (upsBoss.invMode == MANUAL_RAMP) || (pinLatchCheck(LATCH_AC_ENABLE_n) == 1))
	{
		overCurrentCount = 0;
	}
	else if (INV_OC_NOT == 0) // Inverter window comparator overcurrent
	{
		overCurrentCount++;
		if (overCurrentCount > 20000) // don't let int roll over, let adjustment > 500msec
		{
			overCurrentCount = 20000;
		}
		if (BYPASS_ON_OVERLOAD == TRUE)
		{
			upsBoss.bypassMode = WARN;
		}
		if (invOc >= INVERTER_OC_MSEC)
		{
			upsBoss.ampOutMode = ON_ALARM;
		}
		else
		{
			upsBoss.ampOutMode = OFF_ALARM;
		}
	}
	else // when inverter current less than limit
	{
		overCurrentDownCount++;
		if (overCurrentDownCount >= 16) // 16 msec
		{
			overCurrentDownCount = 0;
			/*
			overCurrentCount--;                 // reduce count
			if (overCurrentCount < 0) 
			{
				overCurrentCount = 0;
			}
			*/
			if (overCurrentCount != 0) // unsigned int, prevent roll over
			{
				overCurrentCount--; // reduce count
			}
		}
	}

// debug
#if ((defined VERBOSE_DEBUG) && (defined VERBOSE_DEBUG_SYNC)) // Debug variables
	if ((overCurrentCount == 100) || (overCurrentCount == 200) || (overCurrentCount == 300) || (overCurrentCount == 400) || (overCurrentCount == 499))
	{
		if (overCurrentCount != overCurrentCountLast)
		{
			overCurrentCountLast = overCurrentCount;

			log_trace("Inverter Enabled=%d, Inverter Over Current=%d, Over Current Count=%d Inverter Mode =%s", latchAcEnable, invOc, (int)overCurrentCount, getOpmodeStr(upsBoss.invMode));
		}
	}
#endif // ((defined VERBOSE_DEBUG) && (defined VERBOSE_DEBUG_SYNC))
	   // debug end

	// Acknowledge this interrupt to receive more interrupts from group 1
	PieCtrlRegs.PIEACK.all = PIEACK_GROUP1;
}

// This is the system Frame timer, it splits the Inverter update into slices per fundamental period
// calculates the PWM duty cycle for the frame by doing a sine table lookup and multiplying
// by the amplitude gain then calculating the PWM counts.  The PWM interrupt uses this value
// until the next frame update.
// It also gathers data for RMS calculation and at the end of the fundamental cycle transfers
// the accumulators, resets them and sets a flag for the background task.
interrupt void cpu_timer1_isr(void)
{
	static volatile int sineIndex = 0, overcurrentDownCount = 0;
	volatile int pwmCmd;
	volatile float gain;
	static volatile int ocCountdown = 0;
	// debug
	//static volatile struct timeT frameTime2;
	//static volatile float frameTimeFloat[150], frameTimeFloatDiff[150];
	static volatile int frameCount = 0;
	// debug end

	gain = invGetGain();
	if (gain > 100.0)
	{
		gain = 100.0;
	}
	else if (gain < 0.0)
	{
		gain = 0.0;
	}
// note: sineIndex is incremented and checked in epwm2_isr, just used here
//pwmCmd = (int) ((gain * (1/100.0) * sineLookup[sineIndex] * PWM_CENTER) + PWM_CENTER + 0.5);
#if OLD_MICRO_COMPATIBLE == 1 // 1=yes, old, non-direct PWM
	//GpioDataRegs.GPASET.bit.GPIO30 = 1;   // PWM ISR
	pwmCmd = (int)((gain * (1 / 100.0) * getSineValue(gSineIndex) * PWM_CENTER) + PWM_CENTER + 0.5);
	//GpioDataRegs.GPACLEAR.bit.GPIO30 = 1; // PWM ISR

	// Debug
	//fTemp = getSineValue(sineIndex);
	//iTemp = PWM_CENTER;
	// Debug end

	if (pwmCmd > PWM_PERIOD)
	{
		pwmCmd = PWM_PERIOD;
	}
	if (pwmCmd < 0)
	{
		pwmCmd = 0;
	}

	EPwm1Regs.CMPA.half.CMPA = pwmCmd; // update duty cycle for PWM

	// debug
	//frameTime2 = getTime();
	//frameTimeFloat[frameCount] = (float) frameTime2.msec + (frameTime2.usec/1000.0);
	//frameTimeFloatDiff[frameCount] = frameTimeFloat[frameCount]-frameTimeFloat[frameCount-1];
	frameCount++;
	if (frameCount >= 128)
	{
		frameCount = 0;
	}
	// debug end
	//AdcRegs.ADCSOCFRC1.all = BIT0;                // Software start of conversion for ADCINA0
	/*
		AdcRegs.ADCSOCFRC1.bit.SOC0 = 1;                // Software start of conversion for ADCINA0
		AdcRegs.ADCSOCFRC1.bit.SOC1 = 1;                // ADCINA1..etc.
		AdcRegs.ADCSOCFRC1.bit.SOC2 = 1;
		AdcRegs.ADCSOCFRC1.bit.SOC3 = 1;
		AdcRegs.ADCSOCFRC1.bit.SOC4 = 1;
		AdcRegs.ADCSOCFRC1.bit.SOC5 = 1;
		AdcRegs.ADCSOCFRC1.bit.SOC6 = 1;
		AdcRegs.ADCSOCFRC1.bit.SOC7 = 1;
		AdcRegs.ADCSOCFRC1.bit.SOC8 = 1;
		AdcRegs.ADCSOCFRC1.bit.SOC9 = 1;
		AdcRegs.ADCSOCFRC1.bit.SOC10 = 1;
		AdcRegs.ADCSOCFRC1.bit.SOC11 = 1;
		AdcRegs.ADCSOCFRC1.bit.SOC12 = 1;
		AdcRegs.ADCSOCFRC1.bit.SOC13 = 1;
		AdcRegs.ADCSOCFRC1.bit.SOC14 = 1;
		AdcRegs.ADCSOCFRC1.bit.SOC15 = 1;
		*/
#endif

	sineIndex++;
	// don't use global for test, wrong when rolling over
	if ((sineIndex >= INVERTER_UPDATES_PER_CYCLE) || (sineIndex < 0))
	{
		sineIndex = 0; // roll over
	}
	gSineIndex = sineIndex; // update global value

	// Inverter Overcurrent
	if (upsBoss.inverterOvercurrent == TRUE)
	{
		upsBoss.inverterOvercurrent = FALSE;
		upsBoss.inverterOvercurrentCount++;
		XIntruptRegs.XINT2CR.bit.ENABLE = 1; // re-enable overcurrent isr
	}
	else // count down if no overcurrent this msec
	{
		ocCountdown++;
		if (ocCountdown >= 16) // if no overcurrent count down 1 every 16msec
		{
			ocCountdown = 0; // reset counter
			upsBoss.inverterOvercurrentCount--;
			if (upsBoss.inverterOvercurrentCount < 0)
			{
				upsBoss.inverterOvercurrentCount = 0;
			}
		}
	}

	// this will update time that inverter cycle started, inverter function will use this
	// with vinZeroCross time to synchronize inverter to utility using frame period
	if (frameCount == 0)
	{
		upsBoss.invZeroCross = getTime();
		upsBoss.zeroCrossDiffMsec = timeDiffMsec(upsBoss.vinZeroCross, upsBoss.invZeroCross);
		if (upsBoss.zeroCrossDiffMsec > 0.0f)
        {
		if (upsBoss.zeroCrossDiffMsec >= (0.5 * 1000 * upsBoss.periodIn)) //put in same magnitude
		{
			upsBoss.zeroCrossDiffMsec -= 1000 * upsBoss.periodIn;
		}
        }
		else
		{
			if (upsBoss.zeroCrossDiffMsec <= (0.5 * 1000 * upsBoss.periodIn)) //put in same magnitude
			{
				upsBoss.zeroCrossDiffMsec += 1000 * upsBoss.periodIn;
			}
		}
		upsBoss.invZeroCrossFlag = TRUE; // notify inverter sync new inverter period

		zeroCrossDiffMsec = upsBoss.zeroCrossDiffMsec;
	}

	//debug.accIsrFrame++;

	// The CPU acknowledges the interrupt.
}

// not used at this time
interrupt void cpu_timer2_isr(void)
{
#if ((defined VERBOSE_DEBUG) && (defined VERBOSE_DEBUG_SYNC))		
	EnhancedPLL enhancedPLL;
#endif

	// extern real_T PLL_Vin;                 /* '<Root>/PLLVin' */
	// extern real_T DCFilter;                /* '<S11>/sum1' */
	// extern real_T Vin;                     /* '<S1>/Sum' */
	// extern real_T Theta_i;                 /* '<S1>/Delay' */
	// extern real_T Vsync;                   /* '<S1>/Fcn1' */
	// extern real_T Fsync;                   /* '<S1>/Sum1' */
	// extern real_T Fnow;                    /* '<S1>/Slew_Rate' */
	// extern real_T Ftarget;                 /* '<S1>/State_Machine' */
	// extern real_T Theta_t;                 /* '<S1>/Integrate1' */
	// extern real_T setFrq;                  /* '<S1>/Derivitive' */
	// extern real_T Theta_o;                 /* '<S1>/Angle' */
	// extern real_T Theta_Error;             /* '<S1>/Angle' */
	// extern boolean_T Sync;                 /* '<S1>/AND' */
	// extern boolean_T FInRng;               /* '<S1>/State_Machine' */
	// Process enhanced phase locked loop step temp1 is Input Voltage
	// 2048/110 Maximum AC Input

	if (pEventData->systemData.bankSwitches.bit.INV_SYNC_TO_LINE == 1)
	{
		
		if ( (upsBoss.invMode == AUTO_ON) || (upsBoss.invMode == MANUAL_ON) )
		{
		    PLL_Vin = acIn;
			Enhanced_PLL_step();
			SetPeriodCpuTimer(&CpuTimer1, DSPCLOCK_PER_USEC, (float)((1.0/(setFrq/*60.0f*/*(double)INVERTER_UPDATES_PER_CYCLE))*1e6));
		}

#if ((defined VERBOSE_DEBUG) && (defined VERBOSE_DEBUG_SYNC))
		enhancedPLL.PLL_Vin     = PLL_Vin;
		enhancedPLL.Vout        = acOut;
		enhancedPLL.Vin         = Vin;
		enhancedPLL.Theta_i     = Theta_i;
		enhancedPLL.Vsync       = Vsync;
		enhancedPLL.Fsync       = Fsync;
		enhancedPLL.Fnow        = Fnow;
		enhancedPLL.Ftarget     = Ftarget;
		enhancedPLL.Theta_t     = Theta_t;
		enhancedPLL.setFrq      = setFrq;
		enhancedPLL.Theta_o     = Theta_o;
		enhancedPLL.Theta_Error = Theta_Error;
		enhancedPLL.Sync        = Sync;
		enhancedPLL.FInRng      = FInRng;

		if (countEnhancedPLL < INVERTER_UPDATES_PER_CYCLE*CAPTURE_CYCLES)
		{
			storeEnhancePLL(enhancedPLL);
			countEnhancedPLL++;
		}
#endif
		
	}	
}

/**
 * 	Function call for setTime\n
 * 	Sets current time structure.
 * 	@param timeT	Current Time
 */
void setTime(timeT timeSet)
{
	volatile timeT timeNow;
	uint16_t temp_mS;
	PieCtrlRegs.PIEIER1.bit.INTx7 = 0;
	timeNow = runTime; // Grab the current time
	PieCtrlRegs.PIEIER1.bit.INTx7 = 1;
	timeNow.days = timeSet.days; // Reset the days
	timeNow.msec = timeSet.msec; // Reset the milliseconds
	if (timeNow.msec >= (long)86400000)
	{										   // If we are more than 24hours worth of milliseconds then
		timeNow.days++;						   // Bump days and
		timeNow.msec -= (long)86400000;		   // Remove a day's worth of msecs, leave overflow
	}										   // End of rollover check on milliseconds
	temp_mS = (uint16_t)(timeNow.msec % 1000); // Get just the millisecond count under one second total
	PieCtrlRegs.PIEIER1.bit.INTx7 = 0;
	runTime = timeNow;			// Set the new time
	rtcTime.mSeconds = temp_mS; // Update the milliseconds in the rtcTime structure
	PieCtrlRegs.PIEIER1.bit.INTx7 = 1;
	return; // Bye
} // End of setTime

void setRTCtime(rtc_timeT rtc_time)
{
	//__disable_interrupt ();						// Prevent corruption of data if interrupted
	PieCtrlRegs.PIEIER1.bit.INTx7 = 0;
	rtcTime = rtc_time; // Set the new time
	//__enable_interrupt ();						// Rock and roll
	PieCtrlRegs.PIEIER1.bit.INTx7 = 1;
	return;
}

char *rtcString(rtc_timeT rtcTime)
{
	static char buf[35]; // Character buffer to hold the conversion string and pass it back
						 // String Example -->	02/27/2015 11:19:23.178
	buf[0] = 0;		 // Prepare the buffer for use
	if (rtcTime.Month < 10)
	{								  // Check to see if we need to pad
		strcat(buf, "0");			  // Yes, so put in a leading zero
	}								  // End of pad check for the month
	strcat(buf, itoa(rtcTime.Month)); // Lead off with the month
	strcat(buf, "/");				  // Slash delimiter for the string
	if (rtcTime.Day < 10)
	{								// Check to see if we need to pad
		strcat(buf, "0");			// Yes, so put in a leading zero
	}								// End of pad check for the month
	strcat(buf, itoa(rtcTime.Day)); // Add in the day of the month
	strcat(buf, "/");				// Slash delimiter for the string
	if (rtcTime.Year < 10)
	{						// Check to see if we need to pad
		strcat(buf, "000"); // Yes, so put in a leading zero
	}						// End of pad check for the years
	if (rtcTime.Year < 100)
	{					   // Check to see if we need to pad
		strcat(buf, "00"); // Yes, so put in a leading zero
	}					   // End of pad check for the years
	if (rtcTime.Year < 1000)
	{								 // Check to see if we need to pad
		strcat(buf, "0");			 // Yes, so put in a leading zero
	}								 // End of pad check for the years
	strcat(buf, itoa(rtcTime.Year)); // Add in the year
	strcat(buf, " ");				 // Space delimiter for the string
	if (rtcTime.Hours < 10)
	{								  // Check to see if we need to pad
		strcat(buf, "0");			  // Yes, so put in a leading zero
	}								  // End of pad check for the hours
	strcat(buf, itoa(rtcTime.Hours)); // Add the hours to the string
	strcat(buf, ":");				  // Colon delimiter for the string
	if (rtcTime.Minutes < 10)
	{									// Check to see if we need to pad
		strcat(buf, "0");				// Yes, so put in a leading zero
	}									// End of pad check for the minutes
	strcat(buf, itoa(rtcTime.Minutes)); // Add the minutes to the string
	strcat(buf, ":");					// Colon delimiter for the string
	if (rtcTime.Seconds < 10)
	{									// Check to see if we need to pad
		strcat(buf, "0");				// Yes, so put in a leading zero
	}									// End of pad check for the seconds
	strcat(buf, itoa(rtcTime.Seconds)); // Add the seconds to the string
	strcat(buf, ".");					// Now for the milliseconds
	if (rtcTime.mSeconds < 100)
	{					  // Check to see if we need to pad
		strcat(buf, "0"); // Yes, so put in a leading zero
	}					  // End of pad check for the milliseconds
	if (rtcTime.mSeconds < 10)
	{									 // Check to see if we need to pad
		strcat(buf, "00");				 // Yes, so put in a leading zero
	}									 // End of pad check for the milliseconds
	strcat(buf, itoa(rtcTime.mSeconds)); // Finish off with the milliseconds on this RTC String
	return (&buf[0]);					 // All done, return the pointer to the string
} // End of *rtcString

// getTime method to get snapshot of time data, returns days, milliseconds and fractions of microseconds
timeT getTime(void)
{

	timeT timeNow;

	//	__disable_interrupt();	                        // prevent corruption of data if interrupted
	PieCtrlRegs.PIEIER1.bit.INTx7 = 0;
	timeNow = runTime; //
	timeNow.counts = ReadCpuTimer0Counter();
	// counter counts down from period to zero
	timeNow.usec = 1000 - (timeNow.counts * (1 / DSPCLOCK_PER_USEC));
	runTime.counts = timeNow.counts;
	runTime.usec = timeNow.usec; // so it shows up in runTime when debugging
	//	__enable_interrupt();
	PieCtrlRegs.PIEIER1.bit.INTx7 = 1;

	return (timeNow);
}

/**
 * 	Function call for getRTCtime\n
 * 	Returns current RTC time structure.
 * 	@return rtc_timeT	Current RTC Time
 */
rtc_timeT getRTCtime(void)
{
	volatile rtc_timeT timeNow;
	//__disable_interrupt ();		// Prevent corruption of data if interrupted
	PieCtrlRegs.PIEIER1.bit.INTx7 = 0;
	timeNow = rtcTime; // Grab the current time
	//__enable_interrupt ();		// Allow interrupts and return to normal operation
	PieCtrlRegs.PIEIER1.bit.INTx7 = 1;
	return (timeNow); // Return the rtc_timeT structure
} // End of getRTCtime

// timer
// get start time and delay and return true if expired, resolution milliseconds
int timer(timeT timestart, long delay)
{
	int expired;
	timeT timeNow, timeAlarm;

	expired = FALSE;

	timeNow = getTime();

	timeAlarm = timestart;
	timeAlarm.msec += delay;

	if (timeAlarm.msec >= (long)86400000) // if we are more than 24hours worth of msec
	{
		timeAlarm.days++;				  // bump days
		timeAlarm.msec -= (long)86400000; // remove a day's worth of msecs, leave overflow
	}

	// may make time delay more than a day, take that into account here
	if (timeNow.days > timeAlarm.days)
	{
		expired = TRUE;
	}
	else
	{
		if ((timeNow.msec >= timeAlarm.msec) && (timeNow.days == timeAlarm.days))
		{
			expired = TRUE;
		}
	}

	return (expired);
}

// Same as timer(), resolution in microseconds
int timerUsec(timeT timestart, float delayUsec)
{
	int expired;
	timeT timeNow, timeAlarm;

	expired = FALSE;

	timeNow = getTime();

	timeAlarm = timestart;
	timeAlarm.usec += delayUsec;

	if (timeAlarm.usec >= (float)1000.0) // if we are more than 1msec worth of usec
	{
		timeAlarm.msec++;				 // bump milliseconds
		timeAlarm.usec -= (float)1000.0; // remove a msec worth of usecs, leave overflow
	}

	// may make time delay more than a day, take that into account here
	if (timeNow.days > timeAlarm.days)
	{
		expired = TRUE;
	}
	else if ((timeNow.msec >= timeAlarm.msec) && (timeNow.days == timeAlarm.days) && (timeNow.usec >= timeAlarm.usec))
	{
		expired = TRUE;
	}

	return (expired);
}

void SetPeriodCpuTimer(struct CPUTIMER_VARS *Timer, float Freq, float Period)
{
	Uint32 PeriodInClocks;

	// Initialize timer period:
	Timer->CPUFreqInMHz = Freq;
	Timer->PeriodInUSec = Period;
	PeriodInClocks = (long)(Freq * Period);
	Timer->RegsAddr->PRD.all = PeriodInClocks - 1; // Counter decrements PRD+1 times each period
}

// return time difference in milliseconds
long timeExpired(timeT timeStart, timeT timeNow)
{
	const long msecPerDay = (long)24 * 60 * 60 * 1000;
	long msec;

	msec = (timeNow.days - timeStart.days) * msecPerDay;
	msec += timeNow.msec - timeStart.msec;
	return msec;
}

// return time difference in float fractions of milliseconds ex. 100.32232
// resolution to fractions of microseconds
float timeDiffMsec(timeT timeStart, timeT timeNow)
{
	static const long msecPerDay = (long)24 * 60 * 60 * 1000;
	long msec;
	float diff;

	msec = (timeNow.days - timeStart.days) * msecPerDay;
	msec += timeNow.msec - timeStart.msec;
	diff = (float)msec + ((1.0 / 1000.0) * (timeNow.usec - timeStart.usec));
	return diff;
}
