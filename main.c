/*!	@file		main.c
 *	@author		Mark James
 *	@date		16 April 2009
 *	@version	1.0
 *	@copyright	IntelliPower, Inc. (c) 1988-2018
 *
 *	@brief		main routine.
 *
 * TARGET: 		MSP430x5419, MSP430x5419A, and TMS320x2806x devices
 *
 * HISTORY:
 * -			1.00	Initial Version	16 April 2009
 */
#include "types.h"
#include "timer.h"
#include "epwm.h"
#include "uart.h" // serial routines
#include "adc.h"  // Analog to digital stuff
#include "ups_state_controller.h"
#include "ups_com.h"
#include "snmp.h"
#include "math.h"
#include "string.h"
#include "stdio.h"
#include <stdlib.h>
#include <ctype.h>
#include "system_config.h" // includes file name for configuration
#include SYSTEM_CONFIG	 // configuration header file
#include "display_led.h"
#include "ups.h"
#include "utilities.h"
#include "F2806x_DefaultISR.h"
#include "event_controller.h"
#include "event_display.h"
#include "log.h"
#include "Enhanced_PLL.h"

uint8_t startControllers = FALSE;
uint8_t testLEDs = FALSE;

timeT flashTimer;
timeT voltRampTimer;

// Neat forever loop define
#define Forever for (;;)

// To keep track of which way the Dead Band is moving
#define DB_UP 1
#define DB_DOWN 0

float voltCmd = 0, voltNow = 0;

timeT syncFreqModeTimer;

void initSync(void)
{
    syncFreqModeTimer = getTime();
}

void main(void)
{

	static struct snmpDataStruct snmp, upsilon, *pSnmp, *pUpsilon;
	static float virgin; // used to test if unit started up cold or reset
	static timeT mainTimer1, ledTimer, ledTimer2 /* , ledTimer3, ledTimer4 */;
	static timeT debugTimer1 /* , debugTimer2 */;
	static int iLed = 0, iLed2 = 0;

    // Set log level
    log_set_level(LOG_LEVEL); // Defined in system config header file 

	setupPins();

	// Initialize DSP System Control:
	initDSPhardware();

/*	This initialization occurs during usart_init() below -- candidate for deletion
	// Enable interrupts required for this example
	PieCtrlRegs.PIECTRL.bit.ENPIE = 1; // Enable the PIE block
	PieCtrlRegs.PIEIER9.bit.INTx1 = 1; // PIE Group 9, int1
	PieCtrlRegs.PIEIER9.bit.INTx2 = 1; // PIE Group 9, INT2
	PieCtrlRegs.PIEIER9.bit.INTx3 = 1; // PIE Group 9, INT3
	PieCtrlRegs.PIEIER9.bit.INTx4 = 1; // PIE Group 9, INT4
	IER |= M_INT9;					   // Core enable Int 9
*/

	// Step 4. Initialize all the Device Peripherals:
	// This function is found in DSP2833x_InitPeripherals.c
	// InitPeripherals();  // Not required for this example

	EALLOW;
	SysCtrlRegs.PCLKCR0.bit.TBCLKSYNC = 0;
	EDIS;

	// Initialize Flash
	EALLOW;
	/*
	//Enable Flash Pipeline mode to improve performance
	//of code executed from Flash.
	FlashRegs.FOPT.bit.ENPIPE = 1;

	//                CAUTION
	//Minimum waitstates required for the flash operating
	//at a given CPU rate must be characterized by TI.
	//Refer to the datasheet for the latest information.
	*/
	//Set the Paged Waitstate for the Flash
	FlashRegs.FBANKWAIT.bit.PAGEWAIT = 2;

	//Set the Random Waitstate for the Flash
	FlashRegs.FBANKWAIT.bit.RANDWAIT = 2;

	//Set the Waitstate for the OTP
	FlashRegs.FOTPWAIT.bit.OTPWAIT = 2;
	/*
	//                CAUTION
	//ONLY THE DEFAULT VALUE FOR THESE 2 REGISTERS SHOULD BE USED
	FlashRegs.FSTDBYWAIT.bit.STDBYWAIT = 0x01FF;
	FlashRegs.FACTIVEWAIT.bit.ACTIVEWAIT = 0x01FF;
	*/
	EDIS;

	gpio(LATCH_ENA, 1, GPIO_OUT); // latch_enable*, disable output to start

	EALLOW;
	GpioCtrlRegs.GPAMUX1.bit.GPIO3 = 0; // latch data 7
	GpioCtrlRegs.GPADIR.bit.GPIO3 = 1;
	GpioCtrlRegs.GPAMUX1.bit.GPIO13 = 0; // latch data 4
	GpioCtrlRegs.GPADIR.bit.GPIO13 = 1;
	GpioCtrlRegs.GPAMUX2.bit.GPIO20 = 0; // latch clock
	GpioCtrlRegs.GPADIR.bit.GPIO20 = 1;
	GpioCtrlRegs.GPAMUX2.bit.GPIO22 = 0; // latch data 2
	GpioCtrlRegs.GPADIR.bit.GPIO22 = 1;
	GpioCtrlRegs.GPAMUX2.bit.GPIO24 = 0; // latch data 3
	GpioCtrlRegs.GPADIR.bit.GPIO24 = 1;
	GpioCtrlRegs.GPAMUX2.bit.GPIO25 = 0; // Inverter Over Current
	GpioCtrlRegs.GPADIR.bit.GPIO25 = 0;
	GpioCtrlRegs.GPBMUX1.bit.GPIO32 = 0; // latch data 1
	GpioCtrlRegs.GPBDIR.bit.GPIO32 = 1;
	GpioCtrlRegs.GPBMUX1.bit.GPIO33 = 0; // latch data 0
	GpioCtrlRegs.GPBDIR.bit.GPIO33 = 1;
	GpioCtrlRegs.GPBMUX1.bit.GPIO34 = 0; // latch address 1
	GpioCtrlRegs.GPBDIR.bit.GPIO34 = 1;
	GpioCtrlRegs.GPBMUX1.bit.GPIO41 = 0; // latch address 0, direction is set in msec isr
	GpioCtrlRegs.GPBDIR.bit.GPIO41 = 1;
	GpioCtrlRegs.GPBMUX2.bit.GPIO53 = 0; // latch address 2
	GpioCtrlRegs.GPBDIR.bit.GPIO53 = 1;
	LATCH_ENABLE_OFF;					 // disable latch, it will be enabled in ups.c updateOctalLatchState()
	GpioCtrlRegs.GPBMUX2.bit.GPIO54 = 0; // latch enable*
	GpioCtrlRegs.GPBDIR.bit.GPIO54 = 1;
	GpioCtrlRegs.GPBMUX2.bit.GPIO57 = 0; // latch data 6
	GpioCtrlRegs.GPBDIR.bit.GPIO57 = 1;
	GpioCtrlRegs.GPBMUX2.bit.GPIO58 = 0; // latch data 5
	GpioCtrlRegs.GPBDIR.bit.GPIO58 = 1;
	EDIS;


	//setFrequency(UPS_FREQOUTNOM, 0); // start at 60Hz, parameters (frequency, hz/sec rate)

	initUpsData();
	initSync();
	initInverter();
	InitEPwm();
	turnOffInverter();

#if INVERTER_DIRECT_CONNECT == 1 // use EPWM2 to run upper and lower IGBTs directly
	setDeadtime(1);				 // underlap in microseconds
#else
	setDeadtime(0); // no underlap, distorts sine reference
#endif

	turnOnInverter();
	// Enable global Interrupts and higher priority real-time debug events:
	EINT; // Enable Global interrupt INTM
	ERTM; // Enable Global realtime interrupt DBGM

	initTime();

	initEventStateController(); // Fire up the Event State Controller

	checkFlashEraseAtStartup(BANK3_10_RESET_BANKS); // Check for Thor's Hammer

	initEventDisplayController(); // Fire up Event State Controller

	flashTimer = getTime();
	voltRampTimer = getTime();
	upsBoss.displayUpdateTimer = getTime(); // led display update timer, initialize

	InitializeADC();

    Enhanced_PLL_initialize();

	ledTimer = ledTimer2 = mainTimer1 /* = ledTimer3 = ledTimer4  = ledTimerAdcBusy */ = getTime();

	pUpsOne = &upsOne; // address of structure into pointer var
	pUpsTwo = &upsTwo;
	pUpsBoss = &upsBoss;
	pUpsMain = &upsBoss; // Main is virtual ups

	// Initialize Comm ports
	// usart_init(); // Setup in Event Controller
	SCIB_port_init();

	// Initialize UPS Datastructure
	init_ups_com(pUpsBoss);
	init_ups_state_controller(pUpsMain);

	// set up SNMP and Upsilon
	pSnmp = &snmp;			  // address of structure into pointer var
	pUpsilon = &upsilon;	  // address of structure into pointer var
	snmp.parser = SNMP;		  // set parser type in SNMP data structure
	upsilon.parser = UPSILON; // set parser type in UPSILON data structure
	init_snmp_com(pUpsilon);  // just to prevent compiler warning saying variable not used
	init_snmp_com(pSnmp);
	initDisplay();
	initUps();

	// check virgin value, the value of PI is put in RAM in UPS_INIT after that
	if (virgin == PI)
	{
		addSystemEvent(EVENT_SYSTEM_RESET);
	}
	else
	{
		addSystemEvent(EVENT_COLD_START);
		virgin = PI;
	}

	while (!timer(mainTimer1, 200)); // give hardware some time after setup

	if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
	{
		if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
		{
			log_trace("Program initialize Start of Forever loop");
		}
	}	

	// debug display LEDs end
	// debug ULN2003 (see below)
	// debugTimer1 = getTime();
	// debug ULN2003 (see below)
	// debug restart: add code to check virgin SPI flash and set default in system_config.h
	// upsBoss.bankSwitches.bit.AUTOSTART = 1;
	// debut autorestart end

	// Step 6. IDLE loop. Just sit and loop forever (optional):
	Forever
	{
		adcProcess(); // convert adc counts to engineering units, filter
		inverter();
		if (testLEDs == FALSE) display();
		batteries();
		sonalert(UPDATE); // No mode change, just check

		updateOctalLatchState();
		bypass();
		ups_state_controller(pUpsBoss);
		eventStateController();
		// use correct parser and parser mode
		// at this point main communication is on uart port0
		// port1 is to be used for UPS RS485 LAN, panel mount serial or "other"
		switch (upsBoss.upsComMode)
		{
		case UPS_COM_SNMP:
			snmp_com(pUpsBoss, pSnmp, UART_PORT0);
			break;
		case UPS_COM_UPSILON:
			snmp_com(pUpsBoss, pUpsilon, UART_PORT0);
			break;
		case UPS_COM_USER: // only used on startup from system_config
            CommandProcessor(UART_PORT0);
			//upsBoss.upsComMode = UPS_COM_USER;
			break;
		default:
			break;
		}
		// debug pwm
		// debug ULN2003, use Dout0 to see if that chip is the problem for Display Column drive
		if (timer(debugTimer1, 10))
		{
			debugTimer1 = getTime();
			if (pinLatchCheck(LATCH_DOUT0_n) == 0)
			{
				pinLatch(LATCH_DOUT0_n, 1);
			}
			else
			{
				pinLatch(LATCH_DOUT0_n, 0);
			}
		}
		// Use 4 fan LEDs to indicate background loop running, run LED's clockwise at increasing speed
		// Multiplex board status with fan status in release code.
		if (timer(ledTimer, 100))
		{
			ledTimer = getTime();
			iLed -= 1;
			if (iLed <= 0)
			{
				iLed = 1000;
			}
		}

		if (timer(ledTimer2, (long)iLed))
		{
			ledTimer2 = getTime();
			switch (iLed2)
			{
			case 0:
				pinLatch(LATCH_LED_COL0, 1);
				pinLatch(LATCH_LED_COL1, 0);
				pinLatch(LATCH_LED_ROW1, 0);
				pinLatch(LATCH_LED_ROW0, 1);
				iLed2++;
				break;
			case 1:
				pinLatch(LATCH_LED_ROW0, 0);
				pinLatch(LATCH_LED_ROW2, 1);
				iLed2++;
				break;
			case 2:
				pinLatch(LATCH_LED_COL0, 0);
				pinLatch(LATCH_LED_COL1, 1);
				pinLatch(LATCH_LED_ROW2, 0);
				pinLatch(LATCH_LED_ROW2, 1);
				iLed2++;
				break;
			case 3:
				pinLatch(LATCH_LED_ROW2, 0);
				pinLatch(LATCH_LED_ROW0, 1);
				iLed2++;
				break;
			case 4:
				pinLatch(LATCH_LED_COL0, 1);
				pinLatch(LATCH_LED_COL1, 0);
				pinLatch(LATCH_LED_ROW0, 0);
				pinLatch(LATCH_LED_ROW1, 1);
				iLed2++;
				break;
			case 5:
				pinLatch(LATCH_LED_ROW1, 0);
				pinLatch(LATCH_LED_ROW3, 1);
				iLed2++;
				break;
			case 6:
				pinLatch(LATCH_LED_COL0, 0);
				pinLatch(LATCH_LED_COL1, 1);
				pinLatch(LATCH_LED_ROW1, 0);
				pinLatch(LATCH_LED_ROW3, 1);
				iLed2++;
				break;
			case 7:
				pinLatch(LATCH_LED_ROW3, 0);
				pinLatch(LATCH_LED_ROW1, 1);
				iLed2 = 0;
				break;
			}
		}
	}
}
