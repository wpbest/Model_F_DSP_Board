// epwm.c

#include "F2806x_EPwm_defines.h"
#include "epwm.h"
#include "types.h"
#include "ups.h"
#include "utilities.h"
#include "system_config.h" // includes file name for configuration
#include SYSTEM_CONFIG	 // configuration header file

// External Variables
int sineIndex, gSineIndex = 0;
extern int sineEnd;
int adcPwmDelay;
/*
struct upsDataStrucT upsOne, upsTwo, upsBoss, *pUpsOne, *pUpsTwo, *pUpsBoss;
struct upsDataStrucT *pUpsMain, *pUps, *pUpsDisplay, *pUpsLcd;
*/

void initInverter(void)
{
	sineIndex = 0;
	upsBoss.invMode = AUTO_OFF;
	EALLOW;
	GpioCtrlRegs.GPADIR.bit.GPIO0 = 1; // Set direction to output
	EDIS;
}

void turnOnInverter(void)
{
	EALLOW;
	gpio(0, 0, GPIO_OUT);				// set gpio 0 low as output
	GpioCtrlRegs.GPAMUX1.bit.GPIO0 = 1; // Configure GPIO0 as EPWM1A
//GpioCtrlRegs.GPADIR.bit.GPIO0 = 1;		// Set direction to output
#if INVERTER_DIRECT_CONNECT == 1		// use EPWM2 to run upper and lower IGBTs directly
	GpioCtrlRegs.GPAMUX1.bit.GPIO1 = 1; // Configure GPIO1 as EPWM1B
	gpio(1, 0, GPIO_OUT);				// set gpio 1 low as output
										//GpioCtrlRegs.GPADIR.bit.GPIO1 = 1;	// Set direction to output
										//GpioCtrlRegs.GPAMUX1.bit.GPIO2 = 1;	// Configure GPIO2 as EPWM2A
										//GpioCtrlRegs.GPAMUX1.bit.GPIO3 = 1;	// Configure GPIO3 as EPWM2B
#endif
	EDIS;
	pinLatch(LATCH_AC_ENABLE_n, 0); // Enable Inverter output on motherboard
}

void turnOffInverter(void)
{
	pinLatch(LATCH_AC_ENABLE_n, 1); // Disable Inverter output on motherboard
	EALLOW;
	GpioCtrlRegs.GPAMUX1.bit.GPIO0 = 0; // Configure GPIO0 as GPIO
	gpio(0, 0, GPIO_OUT);				// set gpio 0 low as output
#if INVERTER_DIRECT_CONNECT == 1		// use EPWM2 to run upper and lower IGBTs directly
	gpio(1, 0, GPIO_OUT);				// set gpio 1 low as output
	GpioCtrlRegs.GPAMUX1.bit.GPIO1 = 0; // Configure GPIO1 as GPIO
										//GpioCtrlRegs.GPAMUX1.bit.GPIO2 = 0;	// Configure GPIO2 as Input, tristate
										//GpioCtrlRegs.GPAMUX1.bit.GPIO3 = 0;	// Configure GPIO3 as Input, tristate
										//GpioCtrlRegs.GPADIR.bit.GPIO2 = 0;	// Set direction to input, tristate
										//GpioCtrlRegs.GPADIR.bit.GPIO3 = 0;	// Set direction to input, tristate
#endif
	//GpioCtrlRegs.GPADIR.bit.GPIO0 = 0;		// Set direction to input, tristate
	//GpioCtrlRegs.GPADIR.bit.GPIO1 = 0;		// Set direction to input, tristate
	EDIS;
}

void setDeadtime(float uSec)
{
	int deadTime;

#if INVERTER_DIRECT_CONNECT == 1 // use EPWM2 to run upper and lower IGBTs directly
	if (uSec < 1)
	{ // set minimum deadtime to prevent shoot through
		uSec = 1;
	}
#else
	uSec = 0; // normal Model F, set to zero
#endif

	deadTime = (int)(uSec * DSPCLOCK_PER_USEC);

	EPwm1Regs.DBRED = deadTime;
	EPwm1Regs.DBFED = deadTime;
	EPwm2Regs.DBRED = deadTime;
	EPwm2Regs.DBFED = deadTime;
}

interrupt void epwm1_isr(void)
{
	int pwmCmd;
	//int iTemp;
	float /* fTemp , */ gain;

	gain = invGetGain();
// note: sineIndex is incremented and checked in epwm2_isr, just used here
//pwmCmd = (int) ((gain * (1/100.0) * sineLookup[sineIndex] * PWM_CENTER) + PWM_CENTER + 0.5);
#if OLD_MICRO_COMPATIBLE == 1 // 1=yes, old, non-direct PWM
	pwmCmd = (int)((gain * (1 / 100.0) * getSineValue(gSineIndex) * PWM_CENTER) + PWM_CENTER + 0.5);
#if INVERTER_PULSE_BY_PULSE == 1
	pwmCmd = (int)((gain * (1 / 100.0) * getSineValue(sineIndex++) * PWM_CENTER) + PWM_CENTER + 0.5);
	if (sineIndex >= MAX_SINE_VALUES)
	{
		sineIndex = 0;
	}
#endif
#else
	pwmCmd = (int)((gain * (1 / 100.0) * getSineValue(sineIndex) * PWM_CENTER) + PWM_CENTER + 0.5);
#endif

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

	EPwm1Regs.CMPA.half.CMPA = pwmCmd;

	// trigger this in cpu_timer1_isr once each pwm duty cycle update
	//EPwm1Regs.CMPB = 200;					// Trigger SOC at this point in PWM timer

	// Clear INT flag for this timer
	EPwm1Regs.ETCLR.bit.INT = 1;

	// Acknowledge this interrupt to receive more interrupts from group 3
	PieCtrlRegs.PIEACK.all = PIEACK_GROUP3;
}

interrupt void epwm2_isr(void)
{
	int pwmCmd;
	//int iTemp;
	float /* fTemp, */ gain;

	gain = invGetGain();
	//pwmCmd = (int) ((gain * (1/100.0) * sineLookupShifted[sineIndex++] * PWM_CENTER) + PWM_CENTER + 0.5);
	pwmCmd = (int)((gain * (1 / 100.0) * getSineValueShifted(sineIndex++) * PWM_CENTER) + PWM_CENTER + 0.5);

	if (pwmCmd > PWM_PERIOD)
	{
		pwmCmd = PWM_PERIOD;
	}
	if (pwmCmd < 0)
	{
		pwmCmd = 0;
	}

	// Debug
	//fTemp = getSineValueShifted(sineIndex);
	//iTemp = PWM_CENTER;
	// Debug end

	EPwm2Regs.CMPA.half.CMPA = pwmCmd;
	if (sineIndex >= MAX_SINE_VALUES)
	{
		sineIndex = 0;
	}

	EPwm2Regs.CMPB = 200; // Trigger SOC at this point in PWM timer

	// Clear INT flag for this timer
	EPwm2Regs.ETCLR.bit.INT = 1;

	// Acknowledge this interrupt to receive more interrupts from group 3
	PieCtrlRegs.PIEACK.all = PIEACK_GROUP3;
}

interrupt void epwm3_isr(void)
{
	/*
	if(EPwm3_DB_Direction == DB_UP)
	{
		if(EPwm3Regs.DBFED < EPWM3_MAX_DB)
		{
			EPwm3Regs.DBFED++;
			EPwm3Regs.DBRED++;
		}
		else
		{
			EPwm3_DB_Direction = DB_DOWN;
			EPwm3Regs.DBFED--;
			EPwm3Regs.DBRED--;
		}
	}
	else
	{
		if(EPwm3Regs.DBFED == EPWM3_MIN_DB)
		{
			EPwm3_DB_Direction = DB_UP;
			EPwm3Regs.DBFED++;
			EPwm3Regs.DBRED++;
		}
		else
		{
			EPwm3Regs.DBFED--;
			EPwm3Regs.DBRED--;
		}
	}


	*/
	// Clear INT flag for this timer
	EPwm3Regs.ETCLR.bit.INT = 1;

	// Acknowledge this interrupt to receive more interrupts from group 3
	PieCtrlRegs.PIEACK.all = PIEACK_GROUP3;
}

void InitEPwmGpio(void)
{
	EALLOW;

	/* Enable internal pull-up for the selected pins */
	// Pull-ups can be enabled or disabled by the user.
	// This will enable the pullups for the specified pins.
	// Comment out other unwanted lines.

	//	GpioCtrlRegs.GPAPUD.bit.GPIO0 = 0;	  // Enable pull-up on GPIO0 (EPWM1A)
	//	GpioCtrlRegs.GPAPUD.bit.GPIO1 = 0;	  // Enable pull-up on GPIO1 (EPWM1B)

	/* Configure ePWM-1 pins using GPIO regs*/
	// This specifies which of the possible GPIO pins will be ePWM1 functional pins.
	// Comment out other unwanted lines.

	//	GpioCtrlRegs.GPAMUX1.bit.GPIO0 = 1;	  // Configure GPIO0 as EPWM1A
	//	GpioCtrlRegs.GPAMUX1.bit.GPIO1 = 1;	  // Configure GPIO1 as EPWM1B

	/* Enable internal pull-up for the selected pins */
	// Pull-ups can be enabled or disabled by the user.
	// This will enable the pullups for the specified pins.
	// Comment out other unwanted lines.

	//	GpioCtrlRegs.GPAPUD.bit.GPIO2 = 0;	  // Enable pull-up on GPIO2 (EPWM2A)
	//	GpioCtrlRegs.GPAPUD.bit.GPIO3 = 0;	  // Enable pull-up on GPIO3 (EPWM3B)

	/* Configure ePWM-2 pins using GPIO regs*/
	// This specifies which of the possible GPIO pins will be ePWM2 functional pins.
	// Comment out other unwanted lines.

	//	GpioCtrlRegs.GPAMUX1.bit.GPIO2 = 1;	  // Configure GPIO2 as EPWM2A
	//	GpioCtrlRegs.GPAMUX1.bit.GPIO3 = 1;	  // Configure GPIO3 as EPWM2B

	EDIS;
}

void InitEPwm2Gpio(void)
{
	EALLOW;

	/* Enable internal pull-up for the selected pins */
	// Pull-ups can be enabled or disabled by the user.
	// This will enable the pullups for the specified pins.
	// Comment out other unwanted lines.

	GpioCtrlRegs.GPAPUD.bit.GPIO2 = 0; // Enable pull-up on GPIO2 (EPWM2A)
	//GpioCtrlRegs.GPAPUD.bit.GPIO3 = 0;	  // Enable pull-up on GPIO3 (EPWM3B) Note: used for octal latch data7

	/* Configure ePWM-2 pins using GPIO regs*/
	// This specifies which of the possible GPIO pins will be ePWM2 functional pins.
	// Comment out other unwanted lines.

	GpioCtrlRegs.GPAMUX1.bit.GPIO2 = 1; // Configure GPIO2 as EPWM2A
	//GpioCtrlRegs.GPAMUX1.bit.GPIO3 = 1;	  // Configure GPIO3 as EPWM2B Note: used for octal latch data7

	EDIS;
}

// Charger direct digital control
// with modifications to the 780 board we can remove the analog control chip
// and run the charger FET with DSP PWM signal.
void InitEPwm3Gpio(void)
{
	EALLOW;

#if (defined CHARGER_DIGITAL_CONTROL)
	/* Enable internal pull-up for the selected pins */
	// Pull-ups can be enabled or disabled by the user.
	// This will enable the pullups for the specified pins.
	// Comment out other unwanted lines.

	GpioCtrlRegs.GPAPUD.bit.GPIO4 = 0; // Enable pull-up on GPIO4 (EPWM3A)
	GpioCtrlRegs.GPAPUD.bit.GPIO5 = 0; // Enable pull-up on GPIO5 (EPWM3B)

	/* Configure ePWM-3 pins using GPIO regs*/
	// This specifies which of the possible GPIO pins will be ePWM3 functional pins.
	// Comment out other unwanted lines.

	GpioCtrlRegs.GPAMUX1.bit.GPIO4 = 1; // Configure GPIO4 as EPWM3A
	GpioCtrlRegs.GPAMUX1.bit.GPIO5 = 1; // Configure GPIO5 as EPWM3B
#else									// !defined CHARGER_DIGITAL_CONTROL
	GpioDataRegs.GPACLEAR.bit.GPIO4 = 1; // Pin 9, set to zero
	GpioCtrlRegs.GPADIR.bit.GPIO4 = 1;
#endif									// defined CHARGER_DIGITAL_CONTROL

	EDIS;
}

void InitEPwm()
{

	//	EPwm1Regs.TBPRD = 1500;                         // Set timer period 50kHz
	EPwm1Regs.TBPRD = PWM_PERIOD;				// Set timer period 27kHz
	EPwm1Regs.TBPHS.half.TBPHS = 0;				// Phase is 0
	EPwm1Regs.TBCTL.bit.SYNCOSEL = TB_CTR_ZERO; // Sync down-stream module
	EPwm1Regs.TBCTR = 0;						// Clear counter

	// Setup TBCLK
	EPwm1Regs.TBCTL.bit.CTRMODE = TB_COUNT_UPDOWN; // Count up/down, center aligned
	EPwm1Regs.TBCTL.bit.PHSEN = TB_DISABLE;		   // Disable phase loading
	EPwm1Regs.TBCTL.bit.HSPCLKDIV = TB_DIV1;	   // Clock ratio to SYSCLKOUT
	EPwm1Regs.TBCTL.bit.CLKDIV = TB_DIV1;

	EPwm1Regs.CMPCTL.bit.SHDWAMODE = CC_SHADOW; // Load registers every ZERO
	EPwm1Regs.CMPCTL.bit.SHDWBMODE = CC_SHADOW;
	EPwm1Regs.CMPCTL.bit.LOADAMODE = CC_CTR_ZERO;
	EPwm1Regs.CMPCTL.bit.LOADBMODE = CC_CTR_ZERO;

	// Setup compare
	EPwm1Regs.CMPA.half.CMPA = PWM_CENTER; // set to 0 volts, 50% Duty

	// Set actions
	EPwm1Regs.AQCTLA.bit.CAU = AQ_SET; // Set PWM1A on Zero
	EPwm1Regs.AQCTLA.bit.CAD = AQ_CLEAR;

	EPwm1Regs.AQCTLB.bit.CAU = AQ_CLEAR; // Set PWM1A on Zero
	EPwm1Regs.AQCTLB.bit.CAD = AQ_SET;

	// Active Low PWMs - Setup Deadband
	EPwm1Regs.DBCTL.bit.OUT_MODE = DB_FULL_ENABLE;
	EPwm1Regs.DBCTL.bit.POLSEL = DB_ACTV_LOC;
	EPwm1Regs.DBCTL.bit.IN_MODE = DBA_ALL;
	EPwm1Regs.DBRED = EPWM1_MIN_DB;
	EPwm1Regs.DBFED = EPWM1_MIN_DB;

#if INVERTER_PULSE_BY_PULSE == 1
	// Interrupt where we will change the Deadband
	EPwm1Regs.ETSEL.bit.INTSEL = ET_CTR_ZERO; // Select INT on Zero event
	EPwm1Regs.ETSEL.bit.INTEN = 1;			  // Enable INT
	EPwm1Regs.ETPS.bit.INTPRD = ET_1ST;		  // Generate INT on 1st event
#endif

	//	EPwm2Regs.TBPRD = 6000;                     // Set timer period
	EPwm2Regs.TBPRD = PWM_PERIOD;			   // Set timer period
	EPwm2Regs.TBPHS.half.TBPHS = PWM_PERIOD;   // This is 1/2 total period for up/down, makes it 180 degrees from master
	EPwm2Regs.TBCTL.bit.SYNCOSEL = TB_SYNC_IN; // Sync flow through
	EPwm2Regs.TBCTR = 0;					   // Clear counter

	// Setup TBCLK
	EPwm2Regs.TBCTL.bit.CTRMODE = TB_COUNT_UPDOWN; // Count up
	EPwm2Regs.TBCTL.bit.PHSEN = TB_ENABLE;		   // Slave module
	EPwm2Regs.TBCTL.bit.HSPCLKDIV = TB_DIV1;	   // Clock ratio to SYSCLKOUT
	EPwm2Regs.TBCTL.bit.CLKDIV = TB_DIV1;		   // Slow just to observe on the scope

	EPwm2Regs.CMPCTL.bit.SHDWAMODE = CC_SHADOW; // Load registers every ZERO
	EPwm2Regs.CMPCTL.bit.SHDWBMODE = CC_SHADOW;
	EPwm2Regs.CMPCTL.bit.LOADAMODE = CC_CTR_ZERO;
	EPwm2Regs.CMPCTL.bit.LOADBMODE = CC_CTR_ZERO;

	// Setup compare
	EPwm2Regs.CMPA.half.CMPA = PWM_CENTER; // set to 0 volts, 50% Duty

	// Set actions
	EPwm2Regs.AQCTLA.bit.CAU = AQ_CLEAR; // Set PWM2A on Zero
	EPwm2Regs.AQCTLA.bit.CAD = AQ_SET;

	EPwm2Regs.AQCTLB.bit.CAU = AQ_SET; // Set PWM2A on Zero
	EPwm2Regs.AQCTLB.bit.CAD = AQ_CLEAR;

	// Active Low complementary PWMs - setup the deadband
	EPwm2Regs.DBCTL.bit.OUT_MODE = DB_FULL_ENABLE;
	EPwm2Regs.DBCTL.bit.POLSEL = DB_ACTV_LOC;
	EPwm2Regs.DBCTL.bit.IN_MODE = DBA_ALL;
	EPwm2Regs.DBRED = EPWM1_MIN_DB;
	EPwm2Regs.DBFED = EPWM1_MIN_DB;

#if INVERTER_PULSE_BY_PULSE == 1
	// Interrupt where we will modify the deadband
	EPwm2Regs.ETSEL.bit.INTSEL = ET_CTR_ZERO; // Select INT on Zero event
	EPwm2Regs.ETSEL.bit.INTEN = 1;			  // Enable INT
	EPwm2Regs.ETPS.bit.INTPRD = ET_1ST;		  // Generate INT on 1st event
#endif

	// Interrupts that are used in this example are re-mapped to
	// ISR functions found within this file.
	EALLOW; // This is needed to write to EALLOW protected registers
	PieVectTable.EPWM1_INT = &epwm1_isr;
	PieVectTable.EPWM2_INT = &epwm2_isr;
	PieVectTable.EPWM3_INT = &epwm3_isr;
	EDIS; // This is needed to disable write to EALLOW protected registers

#if INVERTER_PULSE_BY_PULSE == 1
		// Enable CPU INT3 which is connected to EPWM1-3 INT:
	IER |= M_INT3;

	// Enable EPWM INTn in the PIE: Group 3 interrupt 1-3
	PieCtrlRegs.PIEIER3.bit.INTx1 = 1;
	//PieCtrlRegs.PIEIER3.bit.INTx2 = 1;
	//PieCtrlRegs.PIEIER3.bit.INTx3 = 1;
#endif
    EALLOW;                                         // Allow Protected Access to the Registers
    GpioDataRegs.GPACLEAR.bit.GPIO10 = 1;
    GpioCtrlRegs.GPAMUX1.bit.GPIO10 = 1;            // Isolated Power Supply PWM1 EPWM6A
    GpioCtrlRegs.GPADIR.bit.GPIO10  = 1;
    GpioDataRegs.GPACLEAR.bit.GPIO11 = 1;
    GpioCtrlRegs.GPAMUX1.bit.GPIO11 = 1;            // Isolated Power Supply PWM2 EPWM6B
    GpioCtrlRegs.GPADIR.bit.GPIO11  = 1;
    EPwm6Regs.TBPRD = ISO_PS_PWM_PERIOD;            // Set the Time-Based Period register with the fan PWM period (DSPCLOCK / FAN_CARRIER)
    EPwm6Regs.CMPA.half.CMPA = ISO_PS_DUTY;         // Initialize with a 50% duty cycle
    EPwm6Regs.CMPB = ISO_PS_DUTY;                   // Initialize with a 50% duty cycle
    EPwm6Regs.TBPHS.all = 0;                        // Set phase shift register to zero
    EPwm6Regs.TBCTR = 0;                            // Clear the Time-Base Counter register
    EPwm6Regs.TBCTL.bit.CTRMODE = TB_COUNT_UP;      // Set mode to perform count up to value
    EPwm6Regs.TBCTL.bit.PHSEN = TB_DISABLE;         // Disable Time-Based Phase loading
    EPwm6Regs.TBCTL.bit.PRDLD = TB_SHADOW;          // Load the TBPRD register via the shadow registers
    EPwm6Regs.TBCTL.bit.SYNCOSEL = TB_SYNC_DISABLE; // Disable Time-Based Synchronization
    EPwm6Regs.TBCTL.bit.HSPCLKDIV = TB_DIV1;        // Set the extended high speed clock to the system clock divided by one
    EPwm6Regs.TBCTL.bit.CLKDIV = TB_DIV1;           // Set the PWM clock to the system clock divided by one
    EPwm6Regs.CMPCTL.bit.SHDWAMODE = CC_SHADOW;     // Reload the active registers from the shadow registers on module events
    EPwm6Regs.CMPCTL.bit.SHDWBMODE = CC_SHADOW;     // Reload the active registers from the shadow registers on module events
    EPwm6Regs.CMPCTL.bit.LOADAMODE = CC_CTR_ZERO;   // Load on CTR = Zero channel A
    EPwm6Regs.CMPCTL.bit.LOADBMODE = CC_CTR_ZERO;   // Load on CTR = Zero channel B
    EPwm6Regs.AQCTLA.bit.ZRO = AQ_SET;              // On zero set the EPWM output HIGH
    EPwm6Regs.AQCTLA.bit.CAU = AQ_CLEAR;            // On Compare is equal set the EPWM output LOW
    EPwm6Regs.AQCTLB.bit.ZRO = AQ_CLEAR;            // On zero set the EPWM output LOW
    EPwm6Regs.AQCTLB.bit.CBU = AQ_SET;              // On Compare is equal set the EPWM output HIGH
    EPwm6Regs.DBCTL.bit.OUT_MODE = DB_FULL_ENABLE;  // Active High complementary PWMs - setup the deadband
    EPwm6Regs.DBCTL.bit.POLSEL = DB_ACTV_HIC;
    EPwm6Regs.DBCTL.bit.IN_MODE = DBA_ALL;
    EPwm6Regs.DBRED = 180;                          // One microsecond deadband time on both ends = 2 * (1.0e-6 / (1/90.0e6))
    EPwm6Regs.DBFED = 180;                          // One microsecond deadband time on both ends = 2 * (1.0e-6 / (1/90.0e6))
    EDIS;                                           // Disallow Access (lock down) 
}

/*
void InitEPwm2Example()
{

	EPwm2Regs.TBPRD = 6000;						  // Set timer period
	EPwm2Regs.TBPHS.half.TBPHS = 0x0000;			  // Phase is 0
	EPwm2Regs.TBCTR = 0x0000;					  // Clear counter

	// Setup TBCLK
	EPwm2Regs.TBCTL.bit.CTRMODE = TB_COUNT_UPDOWN; // Count up
	EPwm2Regs.TBCTL.bit.PHSEN = TB_DISABLE;		  // Disable phase loading
	EPwm2Regs.TBCTL.bit.HSPCLKDIV = TB_DIV4;		  // Clock ratio to SYSCLKOUT
	EPwm2Regs.TBCTL.bit.CLKDIV = TB_DIV4;		  // Slow just to observe on the scope

	// Setup compare
	EPwm2Regs.CMPA.half.CMPA = 3000;

	// Set actions
	EPwm2Regs.AQCTLA.bit.CAU = AQ_SET;			  // Set PWM2A on Zero
	EPwm2Regs.AQCTLA.bit.CAD = AQ_CLEAR;


	EPwm2Regs.AQCTLB.bit.CAU = AQ_CLEAR;			  // Set PWM2A on Zero
	EPwm2Regs.AQCTLB.bit.CAD = AQ_SET;

	// Active Low complementary PWMs - setup the deadband
	EPwm2Regs.DBCTL.bit.OUT_MODE = DB_FULL_ENABLE;
	EPwm2Regs.DBCTL.bit.POLSEL = DB_ACTV_LOC;
	EPwm2Regs.DBCTL.bit.IN_MODE = DBA_ALL;
	EPwm2Regs.DBRED = EPWM2_MIN_DB;
	EPwm2Regs.DBFED = EPWM2_MIN_DB;
//	EPwm2_DB_Direction = DB_UP;

	// Interrupt where we will modify the deadband
	EPwm2Regs.ETSEL.bit.INTSEL = ET_CTR_ZERO;	  // Select INT on Zero event
	EPwm2Regs.ETSEL.bit.INTEN = 1;				  // Enable INT
	EPwm2Regs.ETPS.bit.INTPRD = ET_3RD;			  // Generate INT on 3rd event

}

void InitEPwm3Example()
{

	EPwm3Regs.TBPRD = 6000;						   // Set timer period
	EPwm3Regs.TBPHS.half.TBPHS = 0x0000;			   // Phase is 0
	EPwm3Regs.TBCTR = 0x0000;					   // Clear counter


	// Setup TBCLK
	EPwm3Regs.TBCTL.bit.CTRMODE = TB_COUNT_UPDOWN; // Count up
	EPwm3Regs.TBCTL.bit.PHSEN = TB_DISABLE;		  // Disable phase loading
	EPwm3Regs.TBCTL.bit.HSPCLKDIV = TB_DIV4;		  // Clock ratio to SYSCLKOUT
	EPwm3Regs.TBCTL.bit.CLKDIV = TB_DIV4;		  // Slow so we can observe on the scope

	// Setup compare
	EPwm3Regs.CMPA.half.CMPA = 3000;

	// Set actions
	EPwm3Regs.AQCTLA.bit.CAU = AQ_SET;			   // Set PWM3A on Zero
	EPwm3Regs.AQCTLA.bit.CAD = AQ_CLEAR;


	EPwm3Regs.AQCTLB.bit.CAU = AQ_CLEAR;			   // Set PWM3A on Zero
	EPwm3Regs.AQCTLB.bit.CAD = AQ_SET;

	// Active high complementary PWMs - Setup the deadband
	EPwm3Regs.DBCTL.bit.OUT_MODE = DB_FULL_ENABLE;
	EPwm3Regs.DBCTL.bit.POLSEL = DB_ACTV_HIC;
	EPwm3Regs.DBCTL.bit.IN_MODE = DBA_ALL;
	EPwm3Regs.DBRED = EPWM3_MIN_DB;
	EPwm3Regs.DBFED = EPWM3_MIN_DB;
//	EPwm3_DB_Direction = DB_UP;

	// Interrupt where we will change the deadband
	EPwm3Regs.ETSEL.bit.INTSEL = ET_CTR_ZERO;	   // Select INT on Zero event
	EPwm3Regs.ETSEL.bit.INTEN = 1;				   // Enable INT
	EPwm3Regs.ETPS.bit.INTPRD = ET_3RD;			   // Generate INT on 3rd event

}

*/
