//*****************************************************************************************

/*
 * FILE: 		F2806x_90MHZ.c
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

#include "F2806x_90MHZBOOT.h"
#include "types.h"

//---------------------------------------------------------------------------
// Example: ServiceDog:
//---------------------------------------------------------------------------
// This function resets the watchdog timer.
// Enable this function for using ServiceDog in the application
void ServiceDog(void)
{
      EALLOW;
      SysCtrlRegs.WDKEY = 0x0055;
      SysCtrlRegs.WDKEY = 0x00AA;
      EDIS;
}

//---------------------------------------------------------------------------
// Example: DisableDog:
//---------------------------------------------------------------------------
// This function disables the watchdog timer.
void DisableDog(void)
{
      EALLOW;
      SysCtrlRegs.WDCR = 0x0068;
      EDIS;
}

//---------------------------------------------------------------------------
// Example: InitPll:
//---------------------------------------------------------------------------
// This function initializes the PLLCR register.
void InitPll(Uint16 val, Uint16 divsel)
{
      volatile Uint16 iVol;
      volatile int i;

      // Make sure the PLL is not running in limp mode
      if (SysCtrlRegs.PLLSTS.bit.MCLKSTS != 0)
      {
            EALLOW;
            // OSCCLKSRC1 failure detected. PLL running in limp mode.
            // Re-enable missing clock logic.
            SysCtrlRegs.PLLSTS.bit.MCLKCLR = 1;
            EDIS;
            // Replace this line with a call to an appropriate
            // SystemShutdown(); function.
            __asm("        ESTOP0"); // Uncomment for debugging purposes
      }

      // DIVSEL MUST be 0 before PLLCR can be changed from
      // 0x0000. It is set to 0 by an external reset XRSn
      // This puts us in 1/4
      if (SysCtrlRegs.PLLSTS.bit.DIVSEL != 0)
      {
            EALLOW;
            SysCtrlRegs.PLLSTS.bit.DIVSEL = 0;
            EDIS;
      }

      // Change the PLLCR
      if (SysCtrlRegs.PLLCR.bit.DIV != val)
      {

            EALLOW;
            // Before setting PLLCR turn off missing clock detect logic
            SysCtrlRegs.PLLSTS.bit.MCLKOFF = 1;
            SysCtrlRegs.PLLCR.bit.DIV = val;
            EDIS;

            // Optional: Wait for PLL to lock.
            // During this time the CPU will switch to OSCCLK/2 until
            // the PLL is stable.  Once the PLL is stable the CPU will
            // switch to the new PLL value.
            //
            // This time-to-lock is monitored by a PLL lock counter.
            //
            // Code is not required to sit and wait for the PLL to lock.
            // However, if the code does anything that is timing critical,
            // and requires the correct clock be locked, then it is best to
            // wait until this switching has completed.

            // Wait for the PLL lock bit to be set.

            // The watchdog should be disabled before this loop, or fed within
            // the loop via ServiceDog().

            // Uncomment to disable the watchdog
            DisableDog();

            while (SysCtrlRegs.PLLSTS.bit.PLLLOCKS != 1)
            {
                  // Uncomment to service the watchdog
                  // ServiceDog();
            }

            EALLOW;
            SysCtrlRegs.PLLSTS.bit.MCLKOFF = 0;
            EDIS;
      }

      // If switching to 1/2
      if ((divsel == 1) || (divsel == 2))
      {
            EALLOW;
            SysCtrlRegs.PLLSTS.bit.DIVSEL = divsel;
            EDIS;
      }

      // If switching to 1/1
      // * First go to 1/2 and let the power settle
      //   The time required will depend on the system, this is only an example
      // * Then switch to 1/1
      if (divsel == 3)
      {
            EALLOW;
            SysCtrlRegs.PLLSTS.bit.DIVSEL = 2;
            // DELAY_US (50L);
            //for (i=0;i<=51;i++) {__asm ("nop");}
            for (i = 0; i <= 51; i++)
            {
                  NOP;
            }
            SysCtrlRegs.PLLSTS.bit.DIVSEL = 3;
            EDIS;
      }
}

//---------------------------------------------------------------------------
// Example: InitPll2:
//---------------------------------------------------------------------------
// This function initializes the PLL2 registers.
void InitPll2(Uint16 clksrc, Uint16 pllmult, Uint16 clkdiv)
{
      EALLOW;

      // Check if SYSCLK2DIV2DIS is in /2 mode
      if (DevEmuRegs.DEVICECNF.bit.SYSCLK2DIV2DIS != 0)
      {
            DevEmuRegs.DEVICECNF.bit.SYSCLK2DIV2DIS = 0;
      }

      // Enable PLL2
      SysCtrlRegs.PLL2CTL.bit.PLL2EN = 1;
      // Select clock source for PLL2
      SysCtrlRegs.PLL2CTL.bit.PLL2CLKSRCSEL = clksrc;
      // Set PLL2 Multiplier
      SysCtrlRegs.PLL2MULT.bit.PLL2MULT = pllmult;

      // Wait for PLL to lock.
      // Uncomment to disable the watchdog
      DisableDog();
      while (SysCtrlRegs.PLL2STS.bit.PLL2LOCKS != 1)
      {
            // Uncomment to service the watchdog
            // ServiceDog();
      }

      // Set System Clock 2 divider
      DevEmuRegs.DEVICECNF.bit.SYSCLK2DIV2DIS = clkdiv;
      EDIS;
}

//--------------------------------------------------------------------------
// Example: InitPeripheralClocks:
//---------------------------------------------------------------------------
// This function initializes the clocks to the peripheral modules.
// First the high and low clock prescalers are set
// Second the clocks are enabled to each peripheral.
// To reduce power, leave clocks to unused peripherals disabled
//
// Note: If a peripherals clock is not enabled then you cannot
// read or write to the registers for that peripheral
void InitPeripheralClocks(void)
{
      EALLOW;

      // LOSPCP prescale register settings, normally it will be set to default values

      // GpioCtrlRegs.GPAMUX2.bit.GPIO18 = 3;  // GPIO18 = XCLKOUT
      SysCtrlRegs.LOSPCP.all = 0x0002;

      // XCLKOUT to SYSCLKOUT ratio.  By default XCLKOUT = 1/4 SYSCLKOUT
      SysCtrlRegs.XCLK.bit.XCLKOUTDIV = 2;

      // Peripheral clock enables set for the selected peripherals.
      // If you are not using a peripheral leave the clock off
      // to save on power.
      //
      // Note: not all peripherals are available on all F2806x derivates.
      // Refer to the datasheet for your particular device.
      //
      // This function is not written to be an example of efficient code.

      SysCtrlRegs.PCLKCR1.bit.EPWM1ENCLK = 1; // ePWM1
      SysCtrlRegs.PCLKCR1.bit.EPWM2ENCLK = 1; // ePWM2
      SysCtrlRegs.PCLKCR1.bit.EPWM3ENCLK = 1; // ePWM3
      SysCtrlRegs.PCLKCR1.bit.EPWM4ENCLK = 1; // ePWM4
      SysCtrlRegs.PCLKCR1.bit.EPWM5ENCLK = 1; // ePWM5
      SysCtrlRegs.PCLKCR1.bit.EPWM6ENCLK = 1; // ePWM6
      SysCtrlRegs.PCLKCR1.bit.EPWM7ENCLK = 1; // ePWM7
      SysCtrlRegs.PCLKCR1.bit.EPWM8ENCLK = 1; // ePWM8

      SysCtrlRegs.PCLKCR0.bit.HRPWMENCLK = 1; // HRPWM
      SysCtrlRegs.PCLKCR0.bit.TBCLKSYNC = 1;  // Enable TBCLK within the ePWM

      SysCtrlRegs.PCLKCR1.bit.EQEP1ENCLK = 1; // eQEP1
      SysCtrlRegs.PCLKCR1.bit.EQEP2ENCLK = 1; // eQEP2

      SysCtrlRegs.PCLKCR1.bit.ECAP1ENCLK = 1; // eCAP1
      SysCtrlRegs.PCLKCR1.bit.ECAP2ENCLK = 1; // eCAP2
      SysCtrlRegs.PCLKCR1.bit.ECAP3ENCLK = 1; // eCAP3

      SysCtrlRegs.PCLKCR2.bit.HRCAP1ENCLK = 1; // HRCAP1
      SysCtrlRegs.PCLKCR2.bit.HRCAP2ENCLK = 1; // HRCAP2
      SysCtrlRegs.PCLKCR2.bit.HRCAP3ENCLK = 1; // HRCAP3
      SysCtrlRegs.PCLKCR2.bit.HRCAP4ENCLK = 1; // HRCAP4

      SysCtrlRegs.PCLKCR0.bit.ADCENCLK = 1;   // ADC
      SysCtrlRegs.PCLKCR3.bit.COMP1ENCLK = 1; // COMP1
      SysCtrlRegs.PCLKCR3.bit.COMP2ENCLK = 1; // COMP2
      SysCtrlRegs.PCLKCR3.bit.COMP3ENCLK = 1; // COMP3

      SysCtrlRegs.PCLKCR3.bit.CPUTIMER0ENCLK = 1; // CPU Timer 0
      SysCtrlRegs.PCLKCR3.bit.CPUTIMER1ENCLK = 1; // CPU Timer 1
      SysCtrlRegs.PCLKCR3.bit.CPUTIMER2ENCLK = 1; // CPU Timer 2

      SysCtrlRegs.PCLKCR3.bit.DMAENCLK = 1; // DMA

      SysCtrlRegs.PCLKCR3.bit.CLA1ENCLK = 1; // CLA1

      SysCtrlRegs.PCLKCR3.bit.USB0ENCLK = 1; // USB0

      SysCtrlRegs.PCLKCR0.bit.I2CAENCLK = 1;   // I2C-A
      SysCtrlRegs.PCLKCR0.bit.SPIAENCLK = 1;   // SPI-A
      SysCtrlRegs.PCLKCR0.bit.SPIBENCLK = 1;   // SPI-B
      SysCtrlRegs.PCLKCR0.bit.SCIAENCLK = 1;   // SCI-A
      SysCtrlRegs.PCLKCR0.bit.SCIBENCLK = 1;   // SCI-B
      SysCtrlRegs.PCLKCR0.bit.MCBSPAENCLK = 1; // McBSP-A
      SysCtrlRegs.PCLKCR0.bit.ECANAENCLK = 1;  // eCAN-A

      SysCtrlRegs.PCLKCR0.bit.TBCLKSYNC = 1; // Enable TBCLK within the ePWM

      EDIS;
}

//---------------------------------------------------------------------------
// Example: IntOsc1Sel:
//---------------------------------------------------------------------------
// This function switches to Internal Oscillator 1 and turns off all other clock
// sources to minimize power consumption
void IntOsc1Sel(void)
{
      EALLOW;
      SysCtrlRegs.CLKCTL.bit.INTOSC1OFF = 0;
      SysCtrlRegs.CLKCTL.bit.OSCCLKSRCSEL = 0; // Clk Src = INTOSC1
      SysCtrlRegs.CLKCTL.bit.XCLKINOFF = 1;    // Turn off XCLKIN
      SysCtrlRegs.CLKCTL.bit.XTALOSCOFF = 1;   // Turn off XTALOSC
      SysCtrlRegs.CLKCTL.bit.INTOSC2OFF = 1;   // Turn off INTOSC2
      EDIS;
}

//---------------------------------------------------------------------------
// Example: IntOsc2Sel:
//---------------------------------------------------------------------------
// This function switches to Internal oscillator 2 from External Oscillator
// and turns off all other clock sources to minimize power consumption
// NOTE: If there is no external clock connection, when switching from
//       INTOSC1 to INTOSC2, EXTOSC and XLCKIN must be turned OFF prior
//       to switching to internal oscillator 1
void IntOsc2Sel(void)
{
      EALLOW;
      SysCtrlRegs.CLKCTL.bit.INTOSC2OFF = 0;    // Turn on INTOSC2
      SysCtrlRegs.CLKCTL.bit.OSCCLKSRC2SEL = 1; // Switch to INTOSC2
      SysCtrlRegs.CLKCTL.bit.XCLKINOFF = 1;     // Turn off XCLKIN
      SysCtrlRegs.CLKCTL.bit.XTALOSCOFF = 1;    // Turn off XTALOSC
      SysCtrlRegs.CLKCTL.bit.OSCCLKSRCSEL = 1;  // Switch to Internal Oscillator 2
      SysCtrlRegs.CLKCTL.bit.WDCLKSRCSEL = 0;   // Clock Watchdog off of INTOSC1 always
      SysCtrlRegs.CLKCTL.bit.INTOSC1OFF = 0;    // Leave INTOSC1 on
      EDIS;
}

//---------------------------------------------------------------------------
// Example: XtalOscSel:
//---------------------------------------------------------------------------
// This function switches to External CRYSTAL oscillator and turns off all other clock
// sources to minimize power consumption. This option may not be available on all
// device packages
void XtalOscSel(void)
{
      EALLOW;
      SysCtrlRegs.CLKCTL.bit.XTALOSCOFF = 0;    // Turn on XTALOSC
      SysCtrlRegs.CLKCTL.bit.XCLKINOFF = 1;     // Turn off XCLKIN
      SysCtrlRegs.CLKCTL.bit.OSCCLKSRC2SEL = 0; // Switch to external clock
      SysCtrlRegs.CLKCTL.bit.OSCCLKSRCSEL = 1;  // Switch from INTOSC1 to INTOSC2/ext clk
      SysCtrlRegs.CLKCTL.bit.WDCLKSRCSEL = 0;   // Clock Watchdog off of INTOSC1 always
      SysCtrlRegs.CLKCTL.bit.INTOSC2OFF = 1;    // Turn off INTOSC2
      SysCtrlRegs.CLKCTL.bit.INTOSC1OFF = 0;    // Leave INTOSC1 on
      EDIS;
}

//---------------------------------------------------------------------------
// Example: ExtOscSel:
//---------------------------------------------------------------------------
// This function switches to External oscillator and turns off all other clock
// sources to minimize power consumption.
void ExtOscSel(void)
{
      EALLOW;
      SysCtrlRegs.XCLK.bit.XCLKINSEL = 1;       // 1-GPIO19 = XCLKIN, 0-GPIO38 = XCLKIN
      SysCtrlRegs.CLKCTL.bit.XTALOSCOFF = 1;    // Turn on XTALOSC
      SysCtrlRegs.CLKCTL.bit.XCLKINOFF = 0;     // Turn on XCLKIN
      SysCtrlRegs.CLKCTL.bit.OSCCLKSRC2SEL = 0; // Switch to external clock
      SysCtrlRegs.CLKCTL.bit.OSCCLKSRCSEL = 1;  // Switch from INTOSC1 to INTOSC2/ext clk
      SysCtrlRegs.CLKCTL.bit.WDCLKSRCSEL = 0;   // Clock Watchdog off of INTOSC1 always
      SysCtrlRegs.CLKCTL.bit.INTOSC2OFF = 1;    // Turn off INTOSC2
      SysCtrlRegs.CLKCTL.bit.INTOSC1OFF = 0;    // Leave INTOSC1 on
      EDIS;
}

const struct PIE_VECT_TABLE PieVectTableInit = {

    PIE_RESERVED, // 1  Reserved space
    PIE_RESERVED, // 2  Reserved space
    PIE_RESERVED, // 3  Reserved space
    PIE_RESERVED, // 4  Reserved space
    PIE_RESERVED, // 5  Reserved space
    PIE_RESERVED, // 6  Reserved space
    PIE_RESERVED, // 7  Reserved space
    PIE_RESERVED, // 8  Reserved space
    PIE_RESERVED, // 9  Reserved space
    PIE_RESERVED, // 10 Reserved space
    PIE_RESERVED, // 11 Reserved space
    PIE_RESERVED, // 12 Reserved space
    PIE_RESERVED, // 13 Reserved space

    // Non-Peripheral Interrupts
    INT13_ISR,   // CPU-Timer 1
    INT14_ISR,   // CPU-Timer 2
    DATALOG_ISR, // Datalogging interrupt
    RTOSINT_ISR, // RTOS interrupt
    EMUINT_ISR,  // Emulation interrupt
    NMI_ISR,     // Non-maskable interrupt
    ILLEGAL_ISR, // Illegal operation TRAP
    USER1_ISR,   // User Defined trap 1
    USER2_ISR,   // User Defined trap 2
    USER3_ISR,   // User Defined trap 3
    USER4_ISR,   // User Defined trap 4
    USER5_ISR,   // User Defined trap 5
    USER6_ISR,   // User Defined trap 6
    USER7_ISR,   // User Defined trap 7
    USER8_ISR,   // User Defined trap 8
    USER9_ISR,   // User Defined trap 9
    USER10_ISR,  // User Defined trap 10
    USER11_ISR,  // User Defined trap 11
    USER12_ISR,  // User Defined trap 12

    // Group 1 PIE Vectors
    ADCINT1_ISR, // 1.1 ADC  ADC - make rsvd1_1 if ADCINT1 is wanted in Group 10 instead.
    ADCINT2_ISR, // 1.2 ADC  ADC - make rsvd1_2 if ADCINT2 is wanted in Group 10 instead.
    rsvd_ISR,    // 1.3
    XINT1_ISR,   // 1.4 External Interrupt
    XINT2_ISR,   // 1.5 External Interrupt
    ADCINT9_ISR, // 1.6 ADC Interrupt 9
    TINT0_ISR,   // 1.7 Timer 0
    WAKEINT_ISR, // 1.8 WD, Low Power

    // Group 2 PIE Vectors
    EPWM1_TZINT_ISR, // 2.1 EPWM-1 Trip Zone
    EPWM2_TZINT_ISR, // 2.2 EPWM-2 Trip Zone
    EPWM3_TZINT_ISR, // 2.3 EPWM-3 Trip Zone
    EPWM4_TZINT_ISR, // 2.4 EPWM-4 Trip Zone
    EPWM5_TZINT_ISR, // 2.5 EPWM-5 Trip Zone
    EPWM6_TZINT_ISR, // 2.6 EPWM-6 Trip Zone
    EPWM7_TZINT_ISR, // 2.7 EPWM-7 Trip Zone
    EPWM8_TZINT_ISR, // 2.8 EPWM-8 Trip Zone

    // Group 3 PIE Vectors
    EPWM1_INT_ISR, // 3.1 EPWM-1 Interrupt
    EPWM2_INT_ISR, // 3.2 EPWM-2 Interrupt
    EPWM3_INT_ISR, // 3.3 EPWM-3 Interrupt
    EPWM4_INT_ISR, // 3.4 EPWM-4 Interrupt
    EPWM5_INT_ISR, // 3.5 EPWM-5 Interrupt
    EPWM6_INT_ISR, // 3.6 EPWM-6 Interrupt
    EPWM7_INT_ISR, // 3.7 EPWM-7 Interrupt
    EPWM8_INT_ISR, // 3.8 EPWM-8 Interrupt

    // Group 4 PIE Vectors
    ECAP1_INT_ISR,  // 4.1 ECAP-1
    ECAP2_INT_ISR,  // 4.2 ECAP-2
    ECAP3_INT_ISR,  // 4.3 ECAP-3
    rsvd_ISR,       // 4.4
    rsvd_ISR,       // 4.5
    rsvd_ISR,       // 4.6
    HRCAP1_INT_ISR, // 4.7 HRCAP-1
    HRCAP2_INT_ISR, // 4.8 HRCAP-2

    // Group 5 PIE Vectors

    EQEP1_INT_ISR,  // 5.1 EQEP-1
    EQEP2_INT_ISR,  // 5.2 EQEP-2
    rsvd_ISR,       // 5.3
    HRCAP3_INT_ISR, // 5.4 HRCAP-3
    HRCAP4_INT_ISR, // 5.5 HRCAP-4
    rsvd_ISR,       // 5.6
    rsvd_ISR,       // 5.7
    USB0_INT_ISR,   // 5.8 USB-0

    // Group 6 PIE Vectors
    SPIRXINTA_ISR, // 6.1 SPI-A
    SPITXINTA_ISR, // 6.2 SPI-A
    SPIRXINTB_ISR, // 6.3 SPI-B
    SPITXINTA_ISR, // 6.4 SPI-B
    MRINTA_ISR,    // 6.5 McBSP-A
    MXINTA_ISR,    // 6.6 McBSP-A
    rsvd_ISR,      // 6.7
    rsvd_ISR,      // 6.8

    // Group 7 PIE Vectors
    DINTCH1_ISR, // 7.1 DMA Channel 1
    DINTCH2_ISR, // 7.2 DMA Channel 2
    DINTCH3_ISR, // 7.3 DMA Channel 3
    DINTCH4_ISR, // 7.4 DMA Channel 4
    DINTCH5_ISR, // 7.5 DMA Channel 5
    DINTCH6_ISR, // 7.6 DMA Channel 6
    rsvd_ISR,    // 7.7
    rsvd_ISR,    // 7.8

    // Group 8 PIE Vectors
    I2CINT1A_ISR, // 8.1 I2C-A
    I2CINT2A_ISR, // 8.2 I2C-A
    rsvd_ISR,     // 8.3
    rsvd_ISR,     // 8.4
    rsvd_ISR,     // 8.5
    rsvd_ISR,     // 8.6
    rsvd_ISR,     // 8.7
    rsvd_ISR,     // 8.8

    // Group 9 PIE Vectors
    SCIRXINTA_ISR, // 9.1 SCI-A
    SCITXINTA_ISR, // 9.2 SCI-A
    SCIRXINTB_ISR, // 9.3 SCI-B
    SCITXINTB_ISR, // 9.4 SCI-B
    ECAN0INTA_ISR, // 9.5 ECAN-A
    ECAN1INTA_ISR, // 9.6 ECAN-A
    rsvd_ISR,      // 9.7
    rsvd_ISR,      // 9.8

    // Group 10 PIE Vectors
    rsvd_ISR,    // 10.1 Can be ADCINT1, but must make ADCINT1 in Group 1 space "reserved".
    rsvd_ISR,    // 10.2 Can be ADCINT2, but must make ADCINT2 in Group 1 space "reserved".
    ADCINT3_ISR, // 10.3 ADC
    ADCINT4_ISR, // 10.4 ADC
    ADCINT5_ISR, // 10.5 ADC
    ADCINT6_ISR, // 10.6 ADC
    ADCINT7_ISR, // 10.7 ADC
    ADCINT8_ISR, // 10.8 ADC

    // Group 11 PIE Vectors
    CLA1_INT1_ISR, // 11.1 CLA1
    CLA1_INT2_ISR, // 11.2 CLA1
    CLA1_INT3_ISR, // 11.3 CLA1
    CLA1_INT4_ISR, // 11.4 CLA1
    CLA1_INT5_ISR, // 11.5 CLA1
    CLA1_INT6_ISR, // 11.6 CLA1
    CLA1_INT7_ISR, // 11.7 CLA1
    CLA1_INT8_ISR, // 11.8 CLA1

    // Group 12 PIE Vectors
    XINT3_ISR, // 12.1 External Interrupt
    rsvd_ISR,  // 12.2
    rsvd_ISR,  // 12.3
    rsvd_ISR,  // 12.4
    rsvd_ISR,  // 12.5
    rsvd_ISR,  // 12.6
    LVF_ISR,   // 12.7 Latched Overflow
    LUF_ISR    // 12.8 Latched Underflow
};

//---------------------------------------------------------------------------
// InitPieVectTable:
//---------------------------------------------------------------------------
// This function initializes the PIE vector table to a known state.
// This function must be executed after boot time.
//
void InitPieVectTable(void)
{
      int16 i;
      Uint32 *Source = (void *)&PieVectTableInit;
      Uint32 *Dest = (void *)&PieVectTable;

      // Do not write over first 3 32-bit locations (these locations are
      // initialized by Boot ROM with boot variables)

      Source = Source + 3;
      Dest = Dest + 3;

      EALLOW;
      for (i = 0; i < 125; i++)
            *Dest++ = *Source++;
      EDIS;

      // Enable the PIE Vector Table
      PieCtrlRegs.PIECTRL.bit.ENPIE = 1;
}

//---------------------------------------------------------------------------
// Example: CsmUnlock:
//---------------------------------------------------------------------------
// This function unlocks the CSM. User must replace 0xFFFF's with current
// password for the DSP. Returns 1 if unlock is successful.
#define STATUS_FAIL 0
#define STATUS_SUCCESS 1

Uint16 CsmUnlock()
{
      volatile Uint16 temp;

      // Load the key registers with the current password. The 0xFFFF's are dummy
      // passwords.  User should replace them with the correct password for the DSP.

      EALLOW;
      CsmRegs.KEY0 = 0xFFFF;
      CsmRegs.KEY1 = 0xFFFF;
      CsmRegs.KEY2 = 0xFFFF;
      CsmRegs.KEY3 = 0xFFFF;
      CsmRegs.KEY4 = 0xFFFF;
      CsmRegs.KEY5 = 0xFFFF;
      CsmRegs.KEY6 = 0xFFFF;
      CsmRegs.KEY7 = 0xFFFF;
      EDIS;

      // Perform a dummy read of the password locations
      // if they match the key values, the CSM will unlock

      temp = CsmPwl.PSWD0;
      temp = CsmPwl.PSWD1;
      temp = CsmPwl.PSWD2;
      temp = CsmPwl.PSWD3;
      temp = CsmPwl.PSWD4;
      temp = CsmPwl.PSWD5;
      temp = CsmPwl.PSWD6;
      temp = CsmPwl.PSWD7;

      // If the CSM unlocked, return succes, otherwise return
      // failure.
      if (CsmRegs.CSMSCR.bit.SECURE == 0)
            return STATUS_SUCCESS;
      else
            return STATUS_FAIL;
}

//---------------------------------------------------------------------------
// InitPieCtrl:
//---------------------------------------------------------------------------
// This function initializes the PIE control registers to a known state.
//
void InitPieCtrl(void)
{
      // Disable Interrupts at the CPU level:
      DINT;

      // Disable the PIE
      PieCtrlRegs.PIECTRL.bit.ENPIE = 0;

      // Clear all PIEIER registers:
      PieCtrlRegs.PIEIER1.all = 0;
      PieCtrlRegs.PIEIER2.all = 0;
      PieCtrlRegs.PIEIER3.all = 0;
      PieCtrlRegs.PIEIER4.all = 0;
      PieCtrlRegs.PIEIER5.all = 0;
      PieCtrlRegs.PIEIER6.all = 0;
      PieCtrlRegs.PIEIER7.all = 0;
      PieCtrlRegs.PIEIER8.all = 0;
      PieCtrlRegs.PIEIER9.all = 0;
      PieCtrlRegs.PIEIER10.all = 0;
      PieCtrlRegs.PIEIER11.all = 0;
      PieCtrlRegs.PIEIER12.all = 0;

      // Clear all PIEIFR registers:
      PieCtrlRegs.PIEIFR1.all = 0;
      PieCtrlRegs.PIEIFR2.all = 0;
      PieCtrlRegs.PIEIFR3.all = 0;
      PieCtrlRegs.PIEIFR4.all = 0;
      PieCtrlRegs.PIEIFR5.all = 0;
      PieCtrlRegs.PIEIFR6.all = 0;
      PieCtrlRegs.PIEIFR7.all = 0;
      PieCtrlRegs.PIEIFR8.all = 0;
      PieCtrlRegs.PIEIFR9.all = 0;
      PieCtrlRegs.PIEIFR10.all = 0;
      PieCtrlRegs.PIEIFR11.all = 0;
      PieCtrlRegs.PIEIFR12.all = 0;
}

void initDSPclocks(void)
{
      // Disable the watchdog
      DisableDog();

      // Select external crystal as Clock Source, and turn off all but internal oscillator 1
      // to save power and backup when PLL is locking.
      XtalOscSel();

      // Initialize the PLL control: PLLCR and CLKINDIV
      InitPll(DSP28_PLLCR, DSP28_DIVSEL);

// Initialize the second PLL control running off of internal oscillator 1
#if USB_HRCAP_USED
      InitPll2(PLL2_PLLSRC, PLL2_PLLMULT, PLL2_SYSCLK2DIV2DIS);
#endif

      // Initialize the peripheral clocks
      InitPeripheralClocks();
}

void initDSPhardware(void)
{
      // Step 1. Initialize System Control:
      // PLL, WatchDog, enable Peripheral Clocks
      initDSPclocks();

      // Step 2. Initialize GPIO:
      // This example function is found in the DSP2833x_Gpio.c file and
      // illustrates how to set the GPIO to it's default state.
      // InitGpio();	// Skipped for this example

      // For this case just initialize GPIO pins for ePWM1, ePWM2, ePWM3
      // These functions are in the DSP2833x_EPwm.c file
      InitEPwmGpio();
      //	InitEPwm2Gpio();
      //	InitEPwm3Gpio();

      // Step 3. Clear all interrupts and initialize PIE vector table:
      // Disable CPU interrupts
      DINT;

      // Initialize the PIE control registers to their default state.
      // The default state is all PIE interrupts disabled and flags
      // are cleared.
      InitPieCtrl();

      // Disable CPU interrupts and clear all CPU interrupt flags:
      IER = 0x0000;
      IFR = 0x0000;

      // Initialize the PIE vector table with pointers to the shell Interrupt
      // Service Routines (ISR).
      // This will populate the entire table, even if the interrupt
      // is not used in this example.	 This is useful for debug purposes.
      InitPieVectTable();
}
