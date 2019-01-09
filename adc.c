// TI File $Revision: /main/2 $
// Checkin $Date: July 30, 2009	  18:44:13 $
//###########################################################################
//
// FILE:   Example_2833xAdc.c
//
// TITLE:  DSP2833x ADC Example Program.
//
// ASSUMPTIONS:
//
//	 This program requires the DSP2833x header files.
//
//	 Make sure the CPU clock speed is properly defined in
//	 DSP2833x_Examples.h before compiling this example.
//
//	 Connect signals to be converted to A2 and A3.
//
//	  As supplied, this project is configured for "boot to SARAM"
//	  operation.  The 2833x Boot Mode table is shown below.
//	  For information on configuring the boot mode of an eZdsp,
//	  please refer to the documentation included with the eZdsp,
//
//		 $Boot_Table:
//
//		   GPIO87	GPIO86	   GPIO85	GPIO84
//			XA15	 XA14		XA13	 XA12
//			 PU		  PU		 PU		  PU
//		  ==========================================
//			  1		   1		  1		   1	Jump to Flash
//			  1		   1		  1		   0	SCI-A boot
//			  1		   1		  0		   1	SPI-A boot
//			  1		   1		  0		   0	I2C-A boot
//			  1		   0		  1		   1	eCAN-A boot
//			  1		   0		  1		   0	McBSP-A boot
//			  1		   0		  0		   1	Jump to XINTF x16
//			  1		   0		  0		   0	Jump to XINTF x32
//			  0		   1		  1		   1	Jump to OTP
//			  0		   1		  1		   0	Parallel GPIO I/O boot
//			  0		   1		  0		   1	Parallel XINTF boot
//			  0		   1		  0		   0	Jump to SARAM		<- "boot to SARAM"
//			  0		   0		  1		   1	Branch to check boot mode
//			  0		   0		  1		   0	Boot to flash, bypass ADC cal
//			  0		   0		  0		   1	Boot to SARAM, bypass ADC cal
//			  0		   0		  0		   0	Boot to SCI-A, bypass ADC cal
//												Boot_Table_End$
//
// DESCRIPTION:
//
//	 This example sets up the PLL in x10/2 mode.
//
//	 For 150 MHz devices (default)
//	 divides SYSCLKOUT by six to reach a 25.0Mhz HSPCLK
//	 (assuming a 30Mhz XCLKIN).
//
//	 For 100 MHz devices:
//	 divides SYSCLKOUT by four to reach a 25.0Mhz HSPCLK
//	 (assuming a 20Mhz XCLKIN).
//
//	 Interrupts are enabled and the ePWM1 is setup to generate a periodic
//	 ADC SOC on SEQ1. Two channels are converted, ADCINA3 and ADCINA2.
//
//###########################################################################
//
// Original Author: D.F.
//
// $TI Release: 2833x/2823x Header Files V1.32 $
// $Release Date: June 28, 2010 $
//###########################################################################

#include "types.h"
#include "adc.h"
#include "timer.h"
#include "math.h"
#include "system_config.h" // includes file name for configuration
#include SYSTEM_CONFIG	 // configuration header file
#include "ups.h"
#include "utilities.h"
#include "event_controller.h"
#include "Enhanced_PLL.h"
#include "log.h"

extern event_controller_data_t eventData, *pEventData;

extern real_T PLL_Vin; /* '<Root>/PLLVin' */
extern real_T setFrq;  /* '<S1>/Derivitive' */

real_T acIn;
real_T acOut;

//#include "debug.h"

// External variables
//int adcPwmDelay;
extern int adcPwmDelay;

volatile struct adcRawT adcRawNow  = 
{
	0L, // voltOut
    0L, // ampOut
	0L, // powOut
	0L, // pfcAmpIn
	0L, // voltIn
	0L, // ampChg
	0L, // ampBat
	0L, // voltBus
	3558L, // voltBusFast
	440000L, // voltBat
	0L, // tAmb
	0L, // tSink1
	0L, // tSink2
	0L, // tPfc
	0L, // voltExt1
	0L, // voltExt2
	0L, // tExt1
	0L  // tExt2
};
volatile struct adcRawT adcRawLast = 
{
	0L, // voltOut
    0L, // ampOut
	0L, // powOut
	0L, // pfcAmpIn
	0L, // voltIn
	0L, // ampChg
	0L, // ampBat
	0L, // voltBus
	3558L, // voltBusFast
	440000L, // voltBat
	0L, // tAmb
	0L, // tSink1
	0L, // tSink2
	0L, // tPfc
	0L, // voltExt1
	0L, // voltExt2
	0L, // tExt1
	0L  // tExt2
};

struct upsDataStrucUnfilteredT upsBossUnfilt = 
{
	0.0f, // ampBat
	0.0f, // ampChg
	66.0f, // voltBat
	0.0f, // batChgPct
	0.0f, // freqIn
	0.0f, // voltIn
	0.0f, // pfcAmpIn
	0.0f, // freqOut
	0.0f, // loadPctOut
	0.0f, // powOut
	0.0f, // voltOut
	0.0f, // vaOut
	0.0f, // pfOut
	0.0f, // ampOut
	0.0f, // tAmb
	0.0f, // tSink1
	0.0f, // tSink2
	0.0f, // tPfc
	0.0f, // voltExt1
	0.0f, // voltExt2
	0.0f, // tExt1
	0.0f, // tExt2
	394.0f, // voltBus
	394.0f, // voltBusFast
	0.0f, // voltSupply
	0.0f  // ampSupply
};
//struct debugStrucT debug /*, *debug */;
/*
unsigned int Voltage1[10] = {0,0,0,0,0,0,0,0,0,0};
unsigned int Voltage2[10] = {0,0,0,0,0,0,0,0,0,0};
//extern unsigned int Voltage1[10];
//extern unsigned int Voltage2[10];

unsigned int LoopCount = 0;
unsigned int ConversionCount = 0;
float temp1;
*/
#define R_FILTER_1 (5.0e3)   // RN1C and RN1D paralleled, 10k values, so half
#define R_FILTER_2 (4990.0)  // R260 value on the Model F Board
#define C_FILTER_1 (0.1e-6)  // C185 value on the Model F Board
#define C_FILTER_2 (47.0e-9) // C183 value on the Model F Board

unsigned long adcirscounter = 0;

interrupt void adc_isr(void)
{
	static volatile long temp1 = 0, temp2 = 0, tempCount = 0;
	static volatile int channel = 0, frameCount = 0, lastFrameCount, samplesVout[150] /*, samplesIout[150] */;
	// debug
	// static volatile int samplesVin[150], samplesCount = 0, debugCount = 0;
	static volatile float voltBusFastFilt = 3558L; // used to filter fast bus volts used to detect loss of AC

    // MSJ following group of variable declarations do no appear in this function, clearing compiler warnings
	//const float rcFilterOne = (2.0 * PI * R_FILTER_1 * C_FILTER_1);     // Part of Phase shift calculations based on Model F PWM RC circuit
    //const float rcFilterTwo = (2.0 * PI * R_FILTER_2 * C_FILTER_2);     // Part of Phase shift calculations based on Model F PWM RC circuit
    //const float rad2degTime = (1000.0 * 180.0 / PI / 360.0);            // Convert radians to frequency based in degrees value and scale to milliseconds
    //float zeroCrossDiffMsecCorrected;
    //float DUAL_INPUT_VOLTAGE = 0, lastPhaseDiff = 0;
    //static float freqAdjust = 0.0f;

	// set zero AC volt ADC count accordingly, ADC_VREF set in system_config.h
	// TODO: AC Input has series 1K resistor in series on Mother board AC Output doesn't
	volatile const int zeroAcCount = (int)(4096 / 2), zeroAcCount2_1div = (int)(4096 / 2.1);

    adcirscounter++;

	lastFrameCount = frameCount;
	if ((frameCount >= INVERTER_UPDATES_PER_CYCLE) || (frameCount < 0))
	{
		// debug
		//if (debugCount++ > (60 * 60))
		//{
		//	debugCount = 0;
		//}
		// debug end
		frameCount = 0;			// roll over
		adcRawLast = adcRawNow; // take completed captured data and put into variable for conversion
		// zero all accumulators for next cycle
		adcRawNow.voltOut = adcRawNow.ampOut = adcRawNow.powOut = adcRawNow.pfcAmpIn = 0;
		adcRawNow.voltIn = adcRawNow.ampChg = adcRawNow.ampBat = adcRawNow.voltBus = adcRawNow.voltBat = 0;
		adcRawNow.tSink1 = adcRawNow.tSink2 = adcRawNow.tPfc = adcRawNow.voltExt1 = 0;
		adcRawNow.voltExt2 = adcRawNow.tExt1 = adcRawNow.tExt2 = adcRawNow.tAmb = 0;
		adcDone = TRUE;
	}

	temp1 = AdcResult.ADCRESULT0; // AC out voltage
	temp1 -= zeroAcCount;		  // center around zero
	acOut = (real_T)temp1/(2048.0/110.0); 
	// test, this doesn't work? Nope.  It looks like first access clears register
	//temp2 = AdcResult.ADCRESULT0 - 2048;

	// debug
	//samplesVout[0] = frameCount;
	//samplesVout[frameCount] = temp1;
	//frameTime2 = getTime();
	//frameTime[frameCount+1] = getTime();
	tempCount = frameCount + 1;
	//frameTime[frameCount] = frameTime2;

	//frameTimeFloat[frameCount] = (float) frameTime2.msec + (frameTime2.usec/1000.0);
	//frameTimeFloatDiff[frameCount] = frameTimeFloat[frameCount]-frameTimeFloat[frameCount-1];
	// debug end

	adcRawNow.voltOut += temp1 * temp1; // square and accumulate
	temp2 = AdcResult.ADCRESULT1;		// get conversion
	// WPB Mark's fix
	//temp2 -= zeroAcCount;				// center around zero
	temp2 -= zeroAcCount2_1div;
	adcRawNow.ampOut += temp2 * temp2;  // square and accumulate
	adcRawNow.powOut += temp1 * temp2;  // V*I and accumulate
	temp1 = AdcResult.ADCRESULT2;		// get conversion
	adcRawNow.pfcAmpIn += temp1;		// just accumulate
	temp1 = AdcResult.ADCRESULT3;		// get conversion

#if (!defined INVERTER_400HZ)
	temp1 -= zeroAcCount2_1div; // center around zero using zero calculated for 1K series resistor
#else							// defined INVERTER_400HZ
	temp1 -= zeroAcCount; // center around zero without 1K series resistor
#endif							// !defined INVERTER_400HZ
    acIn = (real_T)temp1/(2048.0/110.0);
	
	// debug
	//samplesVout[frameCount] = temp1;
	//samplesIout[frameCount] = temp2;
	// debug end
	adcRawNow.voltIn += temp1 * temp1;					 // square and accumulate
	temp1 = AdcResult.ADCRESULT4;						 // get conversion
	adcRawNow.ampChg += temp1;							 // just accumulate
	temp1 = AdcResult.ADCRESULT5;						 // get conversion
	adcRawNow.ampBat += temp1;							 // just accumulate
	temp1 = AdcResult.ADCRESULT6;						 // get conversion
	adcRawNow.voltBus += temp1;							 // just accumulate
	voltBusFastFilt += (temp1 - voltBusFastFilt) * 0.01; // filter a little to ignore load steps
	adcRawNow.voltBusFast = voltBusFastFilt;			 // use for fast update, loss of utility
	temp1 = AdcResult.ADCRESULT7;						 // get conversion
	adcRawNow.voltBat += temp1;							 // just accumulate
	temp1 = AdcResult.ADCRESULT8;						 // get conversion
	adcRawNow.tSink1 += temp1;							 // just accumulate
	temp1 = AdcResult.ADCRESULT9;						 // get conversion
	adcRawNow.tSink2 += temp1;							 // just accumulate
	temp1 = AdcResult.ADCRESULT10;						 // get conversion
	adcRawNow.tPfc += temp1;							 // just accumulate
	temp1 = AdcResult.ADCRESULT11;						 // get conversion
	temp1 -= zeroAcCount;								 // center around zero
	adcRawNow.voltExt1 += temp1 * temp1;				 // square and accumulate
	temp1 = AdcResult.ADCRESULT12;						 // get conversion
	temp1 -= zeroAcCount;								 // center around zero
	adcRawNow.voltExt2 += temp1 * temp1;				 // square and accumulate
	temp1 = AdcResult.ADCRESULT13;						 // get conversion
	adcRawNow.tExt1 += temp1;							 // just accumulate
	temp1 = AdcResult.ADCRESULT14;						 // get conversion
	adcRawNow.tExt2 += temp1;							 // just accumulate
	temp1 = AdcResult.ADCRESULT15;						 // get conversion
	adcRawNow.tAmb += temp1;							 // just accumulate

	frameCount++;
	//debug.accIsrAdc++;

	// Reinitialize for next ADC sequence
	AdcRegs.ADCINTFLGCLR.bit.ADCINT1 = 1;   // Clear ADCINT1 flag reinitialize for next SOC
	PieCtrlRegs.PIEACK.all = PIEACK_GROUP1; // Acknowledge interrupt to PIE
}

void InitializeADC(void)
{
	timeT delay;

	// *IMPORTANT*
	// The Device_cal function, which copies the ADC & oscillator calibration values
	// from TI reserved OTP into the appropriate trim registers, occurs automatically
	// in the Boot ROM. If the boot ROM code is bypassed during the debug process, the
	// following function MUST be called for the ADC and oscillators to function according
	// to specification. The clocks to the ADC MUST be enabled before calling this
	// function.
	// See the device data manual and/or the ADC Reference
	// Manual for more information.
	EALLOW;

	/* Configure ADC pins using AIO regs*/
	// This specifies which of the possible AIO pins will be Analog input pins.
	// NOTE: AIO1,3,5,7-9,11,13,15 are analog inputs in all AIOMUX1 configurations.
	// Comment out other unwanted lines.

	GpioCtrlRegs.AIOMUX1.bit.AIO2 = 2;  // Configure AIO2 for A2 (analog input) operation
	GpioCtrlRegs.AIOMUX1.bit.AIO4 = 2;  // Configure AIO4 for A4 (analog input) operation
	GpioCtrlRegs.AIOMUX1.bit.AIO6 = 2;  // Configure AIO6 for A6 (analog input) operation
	GpioCtrlRegs.AIOMUX1.bit.AIO10 = 2; // Configure AIO10 for B2 (analog input) operation
	GpioCtrlRegs.AIOMUX1.bit.AIO12 = 2; // Configure AIO12 for B4 (analog input) operation
	GpioCtrlRegs.AIOMUX1.bit.AIO14 = 2; // Configure AIO14 for B6 (analog input) operation

	EDIS;

	EALLOW;
	AdcRegs.ADCCTL1.bit.RESET = 1; // Reset ADC Module
	NOP;						   // delay 2 cycles
	NOP;
	SysCtrlRegs.PCLKCR0.bit.ADCENCLK = 1; // Enable ADC peripheral clock
	(*Device_cal)();					  // Use OTP calibration data through function
	//SysCtrlRegs.PCLKCR0.bit.ADCENCLK = 0; // Return ADC clock to original state
	EDIS;

	// Configure ADC
	EALLOW;
	PieVectTable.ADCINT1 = &adc_isr;
	PieVectTable.ADCINT2 = &adc_isr;
	AdcRegs.ADCCTL2.bit.ADCNONOVERLAP = 1; // Enable non-overlap mode
	// when this was set to 1, it unset later in program, errata?
	AdcRegs.ADCCTL1.bit.INTPULSEPOS = 0; // ADCINT1 trips after AdcResults latch
	/*
	AdcRegs.ADCCTL1.bit.ADCPWDN         = 1;    // 0=ADC analog circuitry powered down
	AdcRegs.ADCCTL1.bit.ADCBGPWD        = 1;    // 0=ADC Bandgap circuitry powered down
	AdcRegs.ADCCTL1.bit.ADCREFPWD       = 1;    // 0=ADC vref circuitry powered down
	*/
	// Power up ADC, Bandgap and vref circuitry at the same time
	AdcRegs.ADCCTL1.all = BIT5 + BIT6 + BIT7;

	AdcRegs.ADCCTL1.bit.ADCREFSEL = 1;	// 0=ADC vref select internal, 1 = external
	AdcRegs.ADCCTL1.bit.ADCENABLE = 1;	// Must set before an ADC conversion
	AdcRegs.ADCINTFLGCLR.bit.ADCINT1 = 1; // Clear ADCINT1 flag reinitialize for next SOC
	AdcRegs.INTSEL1N2.bit.INT1E = 1;	  // Enabled ADCINT1
	AdcRegs.INTSEL1N2.bit.INT1CONT = 0;   // Disable ADCINT1 Continuous mode
	//AdcRegs.INTSEL1N2.bit.INT1SEL     = 0;    // setup EOC0 to trigger ADCINT1 to fire, changed in ADC ISR for other channels
	AdcRegs.INTSEL1N2.bit.INT1SEL = 15; // setup EOC15 to trigger ADCINT1 to fire
	//AdcRegs.SOCPRICTL.bit.SOCPRIORITY = 0x10; // All SOCs are in high priority mode, arbitrated by SOC number
	//AdcRegs.SOCPRICTL.bit.SOCPRIORITY = 1;    // set SOC0 as high priority, all others in round-robin
	AdcRegs.ADCSOC0CTL.bit.CHSEL = 0;   // set SOC0 channel select to ADCINA0
	AdcRegs.ADCSOC1CTL.bit.CHSEL = 1;   // set SOC1 channel select to ADCINA1
	AdcRegs.ADCSOC2CTL.bit.CHSEL = 2;   // set SOC2 channel select to ADCINA2
	AdcRegs.ADCSOC3CTL.bit.CHSEL = 3;   // set SOC3 channel select to ADCINA3
	AdcRegs.ADCSOC4CTL.bit.CHSEL = 4;   // set SOC4 channel select to ADCINA4
	AdcRegs.ADCSOC5CTL.bit.CHSEL = 5;   // set SOC5 channel select to ADCINA5
	AdcRegs.ADCSOC6CTL.bit.CHSEL = 6;   // set SOC6 channel select to ADCINA6
	AdcRegs.ADCSOC7CTL.bit.CHSEL = 7;   // set SOC7 channel select to ADCINA7
	AdcRegs.ADCSOC8CTL.bit.CHSEL = 8;   // set SOC8 channel select to ADCINB0
	AdcRegs.ADCSOC9CTL.bit.CHSEL = 9;   // set SOC9 channel select to ADCINB1
	AdcRegs.ADCSOC10CTL.bit.CHSEL = 10; // set SOC10 channel select to ADCINB2
	AdcRegs.ADCSOC11CTL.bit.CHSEL = 11; // set SOC11 channel select to ADCINB3
	AdcRegs.ADCSOC12CTL.bit.CHSEL = 12; // set SOC12 channel select to ADCINB4
	AdcRegs.ADCSOC13CTL.bit.CHSEL = 13; // set SOC13 channel select to ADCINB5
	AdcRegs.ADCSOC14CTL.bit.CHSEL = 14; // set SOC14 channel select to ADCINB6
	AdcRegs.ADCSOC15CTL.bit.CHSEL = 15; // set SOC15 channel select to ADCINB7
	//AdcRegs.ADCSOC0CTL.bit.TRIGSEL    = 5;    // set SOC0 start trigger on EPWM1A, due to round-robin SOC0 converts first then SOC1
	//AdcRegs.ADCSOC1CTL.bit.TRIGSEL    = 5;    // set SOC1 start trigger on EPWM1A, due to round-robin SOC0 converts first then SOC1
	//AdcRegs.ADCSOC0CTL.bit.TRIGSEL    = 0;    // set SOC0 start trigger on Software only, due to round-robin SOC0 converts first then SOC1
	//AdcRegs.ADCSOC1CTL.bit.TRIGSEL    = 0;    // set SOC1 start trigger on Software only, due to round-robin SOC0 converts first then SOC1
	AdcRegs.ADCSOC0CTL.bit.TRIGSEL = 2;  // set SOCx start trigger on CPU Timer 1, due to round-robin SOC0 converts first then SOC1
	AdcRegs.ADCSOC1CTL.bit.TRIGSEL = 2;  // ""
	AdcRegs.ADCSOC2CTL.bit.TRIGSEL = 2;  // ""
	AdcRegs.ADCSOC3CTL.bit.TRIGSEL = 2;  // ""
	AdcRegs.ADCSOC4CTL.bit.TRIGSEL = 2;  // ""
	AdcRegs.ADCSOC5CTL.bit.TRIGSEL = 2;  // ""
	AdcRegs.ADCSOC6CTL.bit.TRIGSEL = 2;  // ""
	AdcRegs.ADCSOC7CTL.bit.TRIGSEL = 2;  // ""
	AdcRegs.ADCSOC8CTL.bit.TRIGSEL = 2;  // ""
	AdcRegs.ADCSOC9CTL.bit.TRIGSEL = 2;  // ""
	AdcRegs.ADCSOC10CTL.bit.TRIGSEL = 2; // ""
	AdcRegs.ADCSOC11CTL.bit.TRIGSEL = 2; // ""
	AdcRegs.ADCSOC12CTL.bit.TRIGSEL = 2; // ""
	AdcRegs.ADCSOC13CTL.bit.TRIGSEL = 2; // ""
	AdcRegs.ADCSOC14CTL.bit.TRIGSEL = 2; // ""
	AdcRegs.ADCSOC15CTL.bit.TRIGSEL = 2; // ""
	// Silicon Errata sheet says the when ACQPS is set to 6 or 7 there may be a problem with the
	// conversion results due to a subsequent sample terminating on the 14th cycle instead of the
	// 15th cycle making the result possibly not valid when latched into the the result register.
	// This can be avoided by using other ACQPS values.
	AdcRegs.ADCSOC0CTL.bit.ACQPS = 8;  // set SOCx S/H Window to 9 ADC Clock Cycles, (8 ACQPS plus 1)
	AdcRegs.ADCSOC1CTL.bit.ACQPS = 8;  // ""
	AdcRegs.ADCSOC2CTL.bit.ACQPS = 8;  // ""
	AdcRegs.ADCSOC3CTL.bit.ACQPS = 8;  // ""
	AdcRegs.ADCSOC4CTL.bit.ACQPS = 8;  // ""
	AdcRegs.ADCSOC5CTL.bit.ACQPS = 8;  // ""
	AdcRegs.ADCSOC6CTL.bit.ACQPS = 8;  // ""
	AdcRegs.ADCSOC7CTL.bit.ACQPS = 8;  // ""
	AdcRegs.ADCSOC8CTL.bit.ACQPS = 8;  // ""
	AdcRegs.ADCSOC9CTL.bit.ACQPS = 8;  // ""
	AdcRegs.ADCSOC10CTL.bit.ACQPS = 8; // ""
	AdcRegs.ADCSOC11CTL.bit.ACQPS = 8; // ""
	AdcRegs.ADCSOC12CTL.bit.ACQPS = 8; // ""
	AdcRegs.ADCSOC13CTL.bit.ACQPS = 8; // ""
	AdcRegs.ADCSOC14CTL.bit.ACQPS = 8; // ""
	AdcRegs.ADCSOC15CTL.bit.ACQPS = 8; // ""
	AdcRegs.ADCINTSOCSEL1.all = 0;	 // No ADCINT will trigger SOC0-7
	AdcRegs.ADCINTSOCSEL2.all = 0;	 // No ADCINT will trigger SOC8-15
	EDIS;

	delay = getTime();
	while (!(timer(delay, 5)))
		; // 5msec delay before converting ADC channels

	// Enable ADCINT in PIE
	//PieCtrlRegs.PIEIER1.bit.INTx6 = 1;
	PieCtrlRegs.PIEIER1.bit.INTx1 = 1; // ADCINT1 in PIE Int1.1

	// Assumes ePWM1 clock is already enabled in InitSysCtrl();
	//EPwm1Regs.ETSEL.bit.SOCAEN	= 1;            // Enable SOC on A group
	//EPwm1Regs.ETSEL.bit.SOCASEL	= 4;            // Select SOC from CMPA on upcount
	//EPwm1Regs.ETPS.bit.SOCAPRD 	= 1;            // Generate pulse on 1st event
	//EPwm1Regs.CMPA.half.CMPA 	= 0x0080;           // Set compare A value
	//EPwm1Regs.TBPRD 				= 0xFFFF;       // Set period for ePWM1
	//EPwm1Regs.TBCTL.bit.CTRMODE 	= 0;            // count up and start

	/* // 28335 code
		__asm(" EALLOW");
	   #if (CPU_FRQ_150MHZ)		// Default - 150 MHz SYSCLKOUT
		 #define ADC_MODCLK 0x3 // HSPCLK = SYSCLKOUT/2*ADC_MODCLK2 = 150/(2*3)	  = 25.0 MHz
	   #endif
	   #if (CPU_FRQ_100MHZ)
		 #define ADC_MODCLK 0x2 // HSPCLK = SYSCLKOUT/2*ADC_MODCLK2 = 100/(2*2)	  = 25.0 MHz
	   #endif
		__asm(" EDIS");

		__asm(" EALLOW");	// This is needed to write to EALLOW protected register
		PieVectTable.ADCINT = &adc_isr;
		PieVectTable.SEQ1INT = &adc_isr;	// was going to the default SEQ1 interrupt trap in file DSP2833x_DefaultIsr.c
		__asm(" EDIS");	// This is needed to disable write to EALLOW protected registers

		// Enable ADCINT in PIE
		PieCtrlRegs.PIEIER1.bit.INTx6 = 1;

		InitAdc();  // init the ADC

		// Configure ADC
		AdcRegs.ADCMAXCONV.all = 0x0003;        // Setup 4 conv's on SEQ1
		AdcRegs.ADCCHSELSEQ1.bit.CONV00 = 0x0;  // Setup ADCINA0 as 1st SEQ1 conv.
		AdcRegs.ADCCHSELSEQ1.bit.CONV01 = 0x1;  // Setup ADCINA1 as 2nd SEQ1 conv.
		AdcRegs.ADCCHSELSEQ1.bit.CONV02 = 0x2;  // Setup ADCINA2 as 3rd SEQ1 conv.
		AdcRegs.ADCCHSELSEQ1.bit.CONV03 = 0x3;  // Setup ADCINA3 as 4th SEQ1 conv.
		AdcRegs.ADCTRL2.bit.EPWM_SOCA_SEQ1 = 1; // Enable SOCA from ePWM to start SEQ1
		AdcRegs.ADCTRL2.bit.INT_ENA_SEQ1 = 1;   // Enable SEQ1 interrupt (every EOS)

		// Assumes ePWM1 clock is already enabled in InitSysCtrl();
		EPwm1Regs.ETSEL.bit.SOCAEN = 1;         // Enable SOC on A group PWM1
		EPwm2Regs.ETSEL.bit.SOCAEN = 1;         // Enable SOC on A group PWM2
		EPwm1Regs.ETSEL.bit.SOCASEL = 4;        // Select SOC from from CPMA on upcount
		EPwm2Regs.ETSEL.bit.SOCASEL = 4;        // Select SOC from from CPMB on upcount
		EPwm1Regs.ETPS.bit.SOCAPRD = 1;         // Generate pulse on 1st event
		EPwm1Regs.CMPB = adcPwmDelay;           // Set compare B value PWM1
		EPwm2Regs.CMPB = adcPwmDelay;           // Set compare B value PWM2
	//	EPwm1Regs.CMPA.half.CMPA = 0x0080;      // Set compare A value
	//	EPwm1Regs.TBPRD = 0xFFFF;               // Set period for ePWM1
	//	EPwm1Regs.TBCTL.bit.CTRMODE = 0;        // count up and start
	*/
}

void adcProcess(void)
{

	static volatile int firstTime = TRUE;
	static volatile timeT filterTimer;
	volatile float ftemp;
	float ChgAmpCorrectionFactor;

#define ADC_VREF (2.5)

#if (!defined INVERTER_400HZ)
	// Motherboard 78.4:1 centered at 2.5 divided by 2 (on DSP board), ADC_VREF divided by 4096
	volatile const float acVoltInConvertScale = (82.46268657 * ADC_VREF * 2.1) / (4096.0);
#else  // defined INVERTER_400HZ
	// Motherboard rectified filtered DC 130:1 centered at 2.5 divided by 2 (on DSP board), ADC_VREF divided by 4096
	// Added correction factor (130/178)
	volatile const float acVoltInConvertScale = (130.0 * (130.0 / 178.0) * ADC_VREF * 2.0) / (4096.0);
#endif // !defined INVERTER_400HZ
	//volatile const float acVoltOutConvertScale = (78.4*ADC_VREF*2.0)/(4096.0);
	// On main board first opamp has a gain of 149.751244:1, then in 120V mode it is multiplied
	// by 2, on DSP board it is divided by 2.  In 240 mode the signal is not multiplied by
	// 2 so maximum signal will be the same in either output mode.
	volatile const float acVoltOutConvertScale120 = (149.751244 * ADC_VREF * 1.0) / (4096.0);
	volatile const float acVoltOutConvertScale240 = (149.751244 * ADC_VREF * 2.0) / (4096.0);
	// Hall effect sensor                   (scale bits) * (ADC Ref) * (divider) * (opamp gain) * (volts/amp)
	volatile const float acAmpConvertScale = ((1 / 4096.0) * ADC_VREF * 2.1 * INVERTER_OUTPUT_CURRENT_GAIN) / 25.0e-3;
	// Motherboard acVolt convert times acAmp convert,
	// centered at 2.5 divided by 2 (on DSP board), ADC_VREF divided by 4096
	//volatile const float acWattConvertScale = (78.4*R67*R77*ADC_VREF*2.0)/(R63*4096.0);
	//volatile const float acWattConvertScale = (78.4*R63*ADC_VREF*2.1)/(R77*R67*4096.0);
	volatile const float acWattConvertScale120 = (acVoltOutConvertScale120 * acAmpConvertScale);
	volatile const float acWattConvertScale240 = (acVoltOutConvertScale240 * acAmpConvertScale);
	// divided by 2 (on DSP board), ADC_VREF divided by 4096
	volatile const float pfcAmpConvertScale = (R9 * R7 * ADC_VREF * 2.0) / (R8 * 4096.0);
	// Motherboard CT 100:1 Burden resistor 100 Ohm for 1A=1V,
	// divided by 2 (on DSP board), ADC_VREF divided by 4096
	// Note: last *2.0, CT only captures FET current, diode current not seen
	volatile const float chargeAmpConvertScale = (1 / 4096.0) * ADC_VREF * 2.1 * 2.0;
	// Motherboard CT 100:1 Burden resistor R163 3.3 Ohm for 1A=33mV => 3.3/100,
	// divided by 2 (on DSP board), ADC_VREF divided by 4096
	//volatile const float dcAmpConvertScale = (3.3*ADC_VREF*2.0)/(100*4096.0);
	// New: 100/3.3 is 100:1 CT with 3.3ohm burden (R163), 1/2.91 is 1:2.91 opamp gain,
	// 2.1 is 2:1 10K divider with 1K on model F board
	//volatile const float dcAmpConvertScale = ((100/R163)*(1/2.91)*ADC_VREF*2.1)/(4096.0);
	// Note: last *2.0, CT only captures FET current, diode current not seen
	volatile const float dcAmpConvertScale = (1 / 4096.0) * ADC_VREF * 2.1 * (1 / 2.91) * (100.0 / R163) * 2.0;
	// Motherboard Analog gain (set in system_config.h) centered at 2.5 divided by 2 (on DSP board),
	// btw the divide by 2 is affected by a series 1K resistor on the Model F board so that changes
	// it to a 2.1.  ADC_VREF divided by 4096
	volatile const float dcVoltConvertScale = (DCV_GAIN * ADC_VREF * 2.1) / (4096.0);
	// Motherboard divider total R divided by bottom R times next divider take total (top = 787ohm,bottom = 1.5K) bottom divided by total
	// divided by 2 (on DSP board), ADC_VREF divided by 4096
	volatile const float equivR129 = 1 / ((1 / R129) + (1 / 20.0e3));
	//volatile const float batVoltConvertScale = (((R109_R111_R113+R120)/R120)*((equivR129+787.0)/equivR129)*ADC_VREF*2.0)/(4096.0);
	// 1.42798 fudge factor
	//volatile const float batVoltConvertScale = (((R109_R111_R113+R120)/R120)*((equivR129+787.0)/equivR129)*ADC_VREF*2.0*1.42798)/(4096.0);
	// New:                                         voltage divider                 opamp gain            DSP divider   Vref      ADC
	volatile const float batVoltConvertScale = (((R109_R111_R113 + R120) / R120) * ((equivR129 + R125) / equivR129) * (20e3 / 10e3) * ADC_VREF * (1 / 4096.0));
	// Just convert to volts, ADC_VREF divided by 4096, divided by intervals
	volatile const float thermistorConvertVolt = ADC_VREF / (4096.0 * (float)INVERTER_UPDATES_PER_CYCLE);
	//float ftemp1;
	// Onboard 320:1 centered at 1.25V, ADC_VREF divided by 4096
	volatile const float externalAcVoltConvertScale = (320 * ADC_VREF) / (4096.0);
	// multiply by inverse (1/INVERTER_UPDATES_PER_CYCLE) faster than divide
	volatile const float inverseSamples = 1.0 / INVERTER_UPDATES_PER_CYCLE;

	// debug
	/*
	ftemp = acVoltConvertScale;
	ftemp = acAmpConvertScale;
	*/
	if (upsBoss.invRangeMode == INV_120V)
	{
		ftemp = acWattConvertScale120;
	}
	else
	{
		ftemp = acWattConvertScale240;
	}
	/*
	ftemp = pfcAmpConvertScale;
	ftemp = chargeAmpConvertScale;
	ftemp = dcAmpConvertScale;
	ftemp = dcVoltConvertScale;
	ftemp = batVoltConvertScale;
	ftemp = thermistorConvertVolt;
	ftemp = externalAcVoltConvertScale;
	*/
	// end debug

	// use in ups_state_controller() to switch to batteries quickly
	// updated and filtered every frame in adc isr
	upsBossUnfilt.voltBusFast = (adcRawLast.voltBusFast * dcVoltConvertScale);

	if (adcDone == TRUE) // one complete cycle of data available
	{
		adcDone = FALSE; // reset flag

		// when sampling for RMS take counts-x to set zero
		// then multiply by itself to square, then accumulate
		// to finish conversion now, take the average, then square root then counts to engineering
		// unit conversion.
		// inverseSamples = 1/INVERTER_UPDATES_PER_CYCLE, multiply faster than divide.
		if (upsBoss.invRangeMode == INV_120V)
		{
			upsBossUnfilt.voltOut = sqrt(adcRawLast.voltOut * inverseSamples) * acVoltOutConvertScale120;
			upsBossUnfilt.powOut = abs(adcRawLast.powOut * inverseSamples * acWattConvertScale120);
		}
		else
		{
			upsBossUnfilt.voltOut = sqrt(adcRawLast.voltOut * inverseSamples) * acVoltOutConvertScale240;
			upsBossUnfilt.powOut = abs(adcRawLast.powOut * inverseSamples * acWattConvertScale240);
		}
		upsBossUnfilt.ampOut = sqrt(adcRawLast.ampOut * inverseSamples) * acAmpConvertScale;
		upsBossUnfilt.pfcAmpIn = sqrt(adcRawLast.pfcAmpIn * inverseSamples) * pfcAmpConvertScale;
#ifdef INPUT_TRANSFORMER_DROOP
		upsBossUnfilt.voltIn = sqrt(adcRawLast.voltIn * inverseSamples) * acVoltInConvertScale;
		upsBossUnfilt.voltIn = upsBossUnfilt.voltIn - INPUT_TRANSFORMER_DROOP_OFFSET + (INPUT_TRANSFORMER_DROOP * upsBoss.loadPctOut * 0.01);
#else
		upsBossUnfilt.voltIn = sqrt(adcRawLast.voltIn * inverseSamples) * acVoltInConvertScale;
#endif
		if (upsBossUnfilt.voltIn <= (UPS_VOLTINNOM * 0.25))
		{
			upsBossUnfilt.voltIn = 0;
		}
		upsBossUnfilt.ampChg = (adcRawLast.ampChg * inverseSamples * chargeAmpConvertScale);
		upsBossUnfilt.ampBat = (adcRawLast.ampBat * inverseSamples * dcAmpConvertScale);
		upsBossUnfilt.voltBus = (adcRawLast.voltBus * inverseSamples * dcVoltConvertScale);
		upsBossUnfilt.voltBat = (adcRawLast.voltBat * inverseSamples * batVoltConvertScale);
//
//		We need to correct the ampChg value based on the charger circuit topology -- the current read by a meter into
//      the battery complex is a mulitple of the ratio between the 1/2 the Bus Voltage divided by the battery voltage.
//		In effect -- power in and power out must match...so we will compute a correction vactor and adjust ampChg
//	    ahead of computing the filtered version of the ampChg.
		ChgAmpCorrectionFactor = 0.48f*(upsBossUnfilt.voltBus/2.0f)/(upsBossUnfilt.voltBat);   // The efficiency of charger circuit is less than 60%
		upsBossUnfilt.ampChg = upsBossUnfilt.ampChg*ChgAmpCorrectionFactor;

		upsBossUnfilt.tSink1 = thermistorConvertExternal(adcRawLast.tSink1 * thermistorConvertVolt);
		upsBossUnfilt.tSink2 = thermistorConvertExternal(adcRawLast.tSink2 * thermistorConvertVolt);
		upsBossUnfilt.tPfc = thermistorConvertExternal(adcRawLast.tPfc * thermistorConvertVolt);
		upsBossUnfilt.tAmb = thermistorConvertInternal(adcRawLast.tAmb * thermistorConvertVolt);
		upsBossUnfilt.tExt1 = thermistorConvertInternal(adcRawLast.tExt1 * thermistorConvertVolt);
		upsBossUnfilt.tExt2 = thermistorConvertInternal(adcRawLast.tExt2 * thermistorConvertVolt);
		upsBossUnfilt.voltExt1 = sqrt(adcRawLast.voltExt1 * inverseSamples) * externalAcVoltConvertScale;
		upsBossUnfilt.voltExt2 = sqrt(adcRawLast.voltExt2 * inverseSamples) * externalAcVoltConvertScale;
	}
	if (timer(filterTimer, 100)) // filter every 100ms
	{
		filterTimer = getTime(); // update timer
		// adds 1/10 the difference (raw to filtered) each time to the filtered value
		if (timer(upsBoss.timeSystemStart, 500))
		{
			upsBoss.voltOut += (upsBossUnfilt.voltOut - upsBoss.voltOut) * 0.1;
			/*  
			    On startup, if filtered not close to input frequency and sync off then
			    stuff nominal value into filtered variable, if sync on then stuff input
			    frequency into output frequency filtered variable
			*/
        	upsBossUnfilt.freqOut = 1 / ((CpuTimer1Regs.PRD.all / DSPCLOCK) * INVERTER_UPDATES_PER_CYCLE); // Update frequency
			//if (fRange(upsBossUnfilt.freqOut, 59.8, 60.2) && (!fRange(upsBoss.freqOut, 59.8, 60.2)))
			// if (!fRange(upsBoss.freqOut, upsBoss.freqIn - 0.2, upsBoss.freqIn + 0.2))
			// {
 			// 	if (pEventData->systemData.bankSwitches.bit.INV_SYNC_TO_LINE == 1)
			// 	{
			// 		upsBoss.freqOut = upsBoss.freqIn;
			// 	}
			// 	else
			// 	{
			// 		upsBoss.freqOut = UPS_FREQOUTNOM;
			// 	}
 			// }
			// else
			// {
				upsBoss.freqOut += (upsBossUnfilt.freqOut - upsBoss.freqOut) * 0.01;
			//}
			upsBoss.ampOut += (upsBossUnfilt.ampOut - upsBoss.ampOut) * 0.1;
			upsBoss.powOut += (upsBossUnfilt.powOut - upsBoss.powOut) * 0.1;
			upsBoss.pfcAmpIn += (upsBossUnfilt.pfcAmpIn - upsBoss.pfcAmpIn) * 0.1;
			upsBoss.voltIn += (upsBossUnfilt.voltIn - upsBoss.voltIn) * 0.1;
			upsBoss.ampChg += (upsBossUnfilt.ampChg - upsBoss.ampChg) * 0.01;
			upsBoss.ampBat += (upsBossUnfilt.ampBat - upsBoss.ampBat) * 0.01;
			upsBoss.voltBus += (upsBossUnfilt.voltBus - upsBoss.voltBus) * 0.1;
			upsBoss.voltBat += (upsBossUnfilt.voltBat - upsBoss.voltBat) * 0.01;
			upsBoss.voltExt1 += (upsBossUnfilt.voltExt1 - upsBoss.voltExt1) * 0.1;
			upsBoss.voltExt2 += (upsBossUnfilt.voltExt2 - upsBoss.voltExt2) * 0.1;
			upsBoss.vaOut = upsBoss.voltOut * upsBoss.ampOut;
#if (defined ZERO_POWER_REPORTING_PERCENT_LOAD)
			// Report power percent < ZERO_POWER_REPORTING_PERCENT_LOAD as Watts, higher as highest VA or Watts
			if (upsBoss.loadPctOut < ZERO_POWER_REPORTING_PERCENT_LOAD)
			{
				upsBoss.loadPctOut = (upsBoss.powOut / (float)UPS_POWOUTNOM) * 100.0;
			}
			else
			{
				upsBoss.loadPctOut = fMax(upsBoss.vaOut / (float)UPS_VAOUTNOM, upsBoss.powOut / (float)UPS_POWOUTNOM) * 100.0;
			}
#else
			upsBoss.loadPctOut = fMax(upsBoss.vaOut / (float)UPS_VAOUTNOM, upsBoss.powOut / (float)UPS_POWOUTNOM) * 100.0;
#endif
		}
		else // prestuff filter value
		{
			upsBoss.voltOut = upsBossUnfilt.voltOut;
			upsBoss.freqOut = UPS_FREQOUTNOM; // takes a few seconds to start up
			upsBoss.ampOut = upsBossUnfilt.ampOut;
			upsBoss.powOut = upsBossUnfilt.powOut;
			upsBoss.pfcAmpIn = upsBossUnfilt.pfcAmpIn;
			upsBoss.voltIn = upsBossUnfilt.voltIn;
			upsBoss.ampChg = upsBossUnfilt.ampChg;
			upsBoss.ampBat = upsBossUnfilt.ampBat;
			upsBoss.voltBus = upsBossUnfilt.voltBus;
			upsBoss.voltBat = upsBossUnfilt.voltBat;
			upsBoss.voltExt1 = upsBossUnfilt.voltExt1;
			upsBoss.voltExt2 = upsBossUnfilt.voltExt2;
			upsBoss.vaOut = upsBoss.voltOut * upsBoss.ampOut;
			upsBoss.loadPctOut = fMax(upsBoss.vaOut / (float)UPS_VAOUTNOM, upsBoss.powOut / (float)UPS_POWOUTNOM) * 100.0;
		}
		if (timer(upsBoss.timeSystemStart, 5000)) // wait after cold start before temperature
		{
			upsBoss.tSink1 += (upsBossUnfilt.tSink1 - upsBoss.tSink1) * 0.05;
			upsBoss.tSink2 += (upsBossUnfilt.tSink2 - upsBoss.tSink2) * 0.05;
			upsBoss.tPfc += (upsBossUnfilt.tPfc - upsBoss.tPfc) * 0.05;
			upsBoss.tAmb += (upsBossUnfilt.tAmb - upsBoss.tAmb) * 0.05;
			upsBoss.tExt1 += (upsBossUnfilt.tExt1 - upsBoss.tExt1) * 0.05;
			upsBoss.tExt2 += (upsBossUnfilt.tExt2 - upsBoss.tExt2) * 0.05;
		}
		else
		{
			upsBoss.tSink1 = 25.0;
			upsBoss.tSink2 = 25.0;
			upsBoss.tPfc = 25.0;
			upsBoss.tAmb = 25.0;
			upsBoss.tExt1 = 25.0;
			upsBoss.tExt2 = 25.0;
		}
		if (upsBoss.vaOut <= 0)
		{
			upsBoss.vaOut = 0.1; // don't divide by zero
		}
		ftemp = upsBoss.powOut / upsBoss.vaOut;
		ftemp = fMin(ftemp, 1.0); // can't have power factor greater than 1
		ftemp = fMax(ftemp, 0);   // nor less than 0
		upsBoss.pfOut = ftemp;

        // Filter not working?

 		if (upsBoss.voltIn <= (UPS_VOLTINNOM * 0.25)) // no input voltage, no input frequency
		 {
		 	upsBoss.freqIn = UPS_FREQINNOM;
		 }
		 else
		 {
		 	if (abs(upsBossUnfilt.freqIn - upsBoss.freqIn) < 10.0f)		// ignore instanteous change in frequency, don't add to filtered value
		 	{
		 	   	upsBoss.freqIn += (upsBossUnfilt.freqIn - upsBoss.freqIn) * 0.01f;
		       //upsBoss.freqIn = upsBossUnfilt.freqIn;
		 	}
		 }
		 if (upsBoss.freqIn > 0.1) // divide by zero possibility
		 {
		 	upsBoss.periodIn = 1 / upsBoss.freqIn;
		 }
		 else
		 {
		 	upsBoss.periodIn = 1 / UPS_FREQINNOM;
		 }
 
		

	}
	if (firstTime == TRUE) // initial call, do setup
	{
		firstTime = FALSE;		 // only the first time
		adcDone = FALSE;		 // reset flag
		filterTimer = getTime(); // initialize filter timer
	}

#if ((defined VERBOSE_DEBUG) && (defined VERBOSE_DEBUG_ADC))
    static timeT logTimer;
    static int logStart = TRUE;

    if (logStart == TRUE) // first pass
    {
        logStart = FALSE;     // reset
        logTimer = getTime(); // set timer
    }

    if (timer(logTimer, 500))
    {
        logTimer = getTime();
        if ( (upsBoss.verboseDebug == TRUE) || (VERBOSE_DEBUG_PORT == UART_PORT1) )
        {
            if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
            {

//                log_trace("adcRawLast.voltBat=%ld upsBossUnfilt.voltBat=%f upsBoss.voltBat=%f",
//                           adcRawLast.voltBat,    upsBossUnfilt.voltBat,   upsBoss.voltBat);            
                log_trace("adcRawNow.voltBusFast=%ld adcRawNow.voltBat=%ld adcRawLast.voltBat=%ld upsBossUnfilt.voltBat=%f upsBoss.voltBat=%f upsBossUnfilt.voltBusFast=%f adcirscounter=%ld",
                           adcRawNow.voltBusFast,   adcRawNow.voltBat,    adcRawLast.voltBat,    upsBossUnfilt.voltBat,   upsBoss.voltBat,   upsBossUnfilt.voltBusFast,    adcirscounter);            
            }
        }
    }    	
#endif // (defined VERBOSE_DEBUG)
}
