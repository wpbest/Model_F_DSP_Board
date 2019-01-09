//*****************************************************************************************

/*
 * FILE: 		F2806x_90MHZ.h
 *
 * TARGET: 		TMS320F2806x
 *
 * CO-AUTHOR: 	Argil Shaver
 * 				Used routines from TI for support functions and
 * 				definitions.
 *
 * DESCRIPTION:	Implements startup clocking initialization for
 * 				the DSP and ISR initialization. This also sets a
 * 				few variables used by other functions in the
 * 				overall system code.
 *
 * IMPORTANT:	To allow access to the DSPCLOCK definition used
 * 				in other sections of code, please add the following to
 * 				the file system_config.h :
 * 				#include "F2806x_90MHZBOOT.h"
 * 				Then in each of the individual system configuration
 * 				files please comment out the DSPCLOCK definition as it
 * 				is now in one single point instead of multiple files.
 * 				This should prevent timing problems from appearing
 * 				down the road when we re-build for the various unit
 * 				configurations.
 * 				There are other initializations that we could move here.
 *
 * DATE:		1 May 2014
 *
 * VERSION:		1.0.0
 *
 * HISTORY:		1.0.0	Initial Version
 *
*/

#ifndef F2806X_90MHZBOOT_H_
#define F2806X_90MHZBOOT_H_

#include "F2806x_Device.h"
#include "F2806x_DefaultISR.h"
#include "epwm.h"

/*----------------------------------------------------------------------------
      PLL2 Defines:	PLL2 output is the USB0 and HRCAP1-4 clock
------------------------------------------------------------------------------*/
// Specify input clock source to PLL2
//#define PLL2_PLLSRC		0x0		// PLL2 Input Osc1
//#define PLL2_PLLSRC		0x1 	// PLL2 Input Osc1
//#define PLL2_PLLSRC		0x2		// PLL2 Input X1
//#define PLL2_PLLSRC		0x3		// PLL2 Input XCLKIN

// Specify the PLL2 control register divide select (SYSCLK2DIV2DIS) and (PLL2MULT) values.
//#define PLL2_SYSCLK2DIV2DIS   0 	// PLL2 Output /2
//#define PLL2_SYSCLK2DIV2DIS  	1 	// PLL2 Output /1

//#define PLL2_PLLMULT   	15
//#define PLL2_PLLMULT   	14
//#define PLL2_PLLMULT   	13
//#define PLL2_PLLMULT   	12
//#define PLL2_PLLMULT   	11
//#define PLL2_PLLMULT   	10
//#define PLL2_PLLMULT    	9
//#define PLL2_PLLMULT    	8
//#define PLL2_PLLMULT    	7
//#define PLL2_PLLMULT    	6	// (CLKSOURCE*6) /2 = SYSCLKOUT2
//#define PLL2_PLLMULT    	5
//#define PLL2_PLLMULT    	4
//#define PLL2_PLLMULT    	3
//#define PLL2_PLLMULT    	2
//#define PLL2_PLLMULT    	1
//#define PLL2_PLLMULT    	0	// PLL is bypassed in this mode
//----------------------------------------------------------------------------
#define USB_HRCAP_USED FALSE  // Do we initialize and use the second PLL for these?
#define PLL2_PLLSRC 0x1       // PLL2 Input Osc1
#define PLL2_SYSCLK2DIV2DIS 0 // PLL2 Output /2
#define PLL2_PLLMULT 6        // (CLKSOURCE*6) /2 = SYSCLKOUT2

#define EXTERNAL_CRYSTAL 20 // External crystal in MHz (not used but informational)

/*-----------------------------------------------------------------------------
      Specify the clock rate of the CPU (SYSCLKOUT) in nS.

      Take into account the input clock frequency and the PLL multiplier
      selected in step 1.

      Use one of the values provided, or define your own.
      The trailing L is required tells the compiler to treat
      the number as a 64-bit value.

      Only one statement should be uncommented.

      Example:   90MHz devices:
                 CLKIN is a 10 MHz crystal or internal 10 MHz oscillator

                 In step 1 the user specified PLLCR = 0x18 for a
                 90 MHz CPU clock (SYSCLKOUT = 90 MHz).

                 In this case, the CPU_RATE will be 11.111L
                 Uncomment the line: #define CPU_RATE 11.111L

-----------------------------------------------------------------------------*/
//#define CPU_RATE   11.111L   // for a 90MHz CPU clock speed  (SYSCLKOUT)
//#define CPU_RATE   12.500L   // for a 80MHz CPU clock speed  (SYSCLKOUT)
//#define CPU_RATE   16.667L   // for a 60MHz CPU clock speed  (SYSCLKOUT)
//#define CPU_RATE   20.000L   // for a 50MHz CPU clock speed  (SYSCLKOUT)
//#define CPU_RATE   25.000L   // for a 40MHz CPU clock speed  (SYSCLKOUT)
//#define CPU_RATE   33.333L   // for a 30MHz CPU clock speed  (SYSCLKOUT)
//#define CPU_RATE   41.667L   // for a 24MHz CPU clock speed  (SYSCLKOUT)
//#define CPU_RATE   50.000L   // for a 20MHz CPU clock speed  (SYSCLKOUT)
//#define CPU_RATE   66.667L   // for a 15MHz CPU clock speed  (SYSCLKOUT)
//#define CPU_RATE  100.000L   // for a 10MHz CPU clock speed  (SYSCLKOUT)
//----------------------------------------------------------------------------
#define CPU_RATE 11.111L // for a 90MHz CPU clock speed (SYSCLKOUT)

/*-----------------------------------------------------------------------------
      Specify the PLL control register (PLLCR) and divide select (DIVSEL) value.
-----------------------------------------------------------------------------*/

//#define DSP28_DIVSEL   0 // Enable /4 for SYSCLKOUT
//#define DSP28_DIVSEL   1 // Disable /4 for SYSCKOUT
//#define DSP28_DIVSEL   2 // Enable /2 for SYSCLKOUT
//#define DSP28_DIVSEL   3 // Enable /1 for SYSCLKOUT

//#define DSP28_PLLCR   18
//#define DSP28_PLLCR   17
//#define DSP28_PLLCR   16
//#define DSP28_PLLCR   15
//#define DSP28_PLLCR   14
//#define DSP28_PLLCR   13
//#define DSP28_PLLCR   12
//#define DSP28_PLLCR   11
//#define DSP28_PLLCR   10
//#define DSP28_PLLCR    9
//#define DSP28_PLLCR    8
//#define DSP28_PLLCR    7
//#define DSP28_PLLCR    6
//#define DSP28_PLLCR    5
//#define DSP28_PLLCR    4
//#define DSP28_PLLCR    3
//#define DSP28_PLLCR    2
//#define DSP28_PLLCR    1
//#define DSP28_PLLCR    0  // PLL is bypassed in this mode
//----------------------------------------------------------------------------
#define DSP28_DIVSEL 2 // Enable /2 for SYSCLKOUT
#define DSP28_PLLCR 9  // For 90 MHz devices [90 MHz = (20MHz * 9)/2]

#define DSPCLOCK (90e6) // 90 MHz system clock, used to set various timed functions

// DO NOT MODIFY THIS LINE.
#define DELAY_US(A) DSP28x_usDelay(((((long double)A * 1000.0L) / (long double)CPU_RATE) - 9.0L) / 5.0L)

void ServiceDog(void);
void DisableDog(void);
void InitPeripheralClocks(void);
void InitPieVectTable(void);
void InitPieCtrl(void);
void initDSPclocks(void);
void initDSPhardware(void);

#endif /* F2806X_90MHZBOOT_H_ */
