//###########################################################################
//
// FILE:    F2806x_ECan.c
//
// TITLE:   F2806x Enhanced CAN Initialization & Support Functions.
//
//###########################################################################
// $TI Release: F2806x C/C++ Header Files and Peripheral Examples V141 $
// $Release Date: January 19, 2015 $
// $Copyright: Copyright (C) 2011-2015 Texas Instruments Incorporated -
//             http://www.ti.com/ ALL RIGHTS RESERVED $
//###########################################################################

#include "F2806x_Device.h"   // F2806x Headerfile Include File
#include "F2806x_Examples.h" // F2806x Examples Include File

struct ECAN_REGS ECanaShadow;

//---------------------------------------------------------------------------
// InitECan:
//---------------------------------------------------------------------------
// This function initializes the eCAN module to a known state.
//
#if DSP28_ECANA
void InitECan(void)
{
	InitECana();
}
#endif

#if DSP28_ECANA
void InitECana(void) // Initialize eCAN-A module
{

	/* Create a shadow register structure for the CAN control registers. This is
	 needed, since only 32-bit access is allowed to these registers. 16-bit access
	 to these registers could potentially corrupt the register contents or return
	 false data. */

	//struct ECAN_REGS ECanaShadow;

	InitECanGpio();
	EALLOW; // EALLOW enables access to protected bits

	/* Configure eCAN RX and TX pins for CAN operation using eCAN regs*/

	ECanaShadow.CANTIOC.all = ECanaRegs.CANTIOC.all;
	ECanaShadow.CANTIOC.bit.TXFUNC = 1;
	ECanaRegs.CANTIOC.all = ECanaShadow.CANTIOC.all;

	ECanaShadow.CANRIOC.all = ECanaRegs.CANRIOC.all;
	ECanaShadow.CANRIOC.bit.RXFUNC = 1;
	ECanaRegs.CANRIOC.all = ECanaShadow.CANRIOC.all;

	/* Configure eCAN for HECC mode - (reqd to access mailboxes 16 thru 31) */
	// HECC mode also enables time-stamping feature

	ECanaShadow.CANMC.all = ECanaRegs.CANMC.all;
	ECanaShadow.CANMC.bit.SCB = 1;
	ECanaRegs.CANMC.all = ECanaShadow.CANMC.all;

	/* Initialize all bits of 'Message Control Register' to zero */
	// Some bits of MSGCTRL register come up in an unknown state. For proper operation,
	// all bits (including reserved bits) of MSGCTRL must be initialized to zero

	ECanaMboxes.MBOX0.MSGCTRL.all = 0x00000000;
	ECanaMboxes.MBOX1.MSGCTRL.all = 0x00000000;
	ECanaMboxes.MBOX2.MSGCTRL.all = 0x00000000;
	ECanaMboxes.MBOX3.MSGCTRL.all = 0x00000000;
	ECanaMboxes.MBOX4.MSGCTRL.all = 0x00000000;
	ECanaMboxes.MBOX5.MSGCTRL.all = 0x00000000;
	ECanaMboxes.MBOX6.MSGCTRL.all = 0x00000000;
	ECanaMboxes.MBOX7.MSGCTRL.all = 0x00000000;
	ECanaMboxes.MBOX8.MSGCTRL.all = 0x00000000;
	ECanaMboxes.MBOX9.MSGCTRL.all = 0x00000000;
	ECanaMboxes.MBOX10.MSGCTRL.all = 0x00000000;
	ECanaMboxes.MBOX11.MSGCTRL.all = 0x00000000;
	ECanaMboxes.MBOX12.MSGCTRL.all = 0x00000000;
	ECanaMboxes.MBOX13.MSGCTRL.all = 0x00000000;
	ECanaMboxes.MBOX14.MSGCTRL.all = 0x00000000;
	ECanaMboxes.MBOX15.MSGCTRL.all = 0x00000000;
	ECanaMboxes.MBOX16.MSGCTRL.all = 0x00000000;
	ECanaMboxes.MBOX17.MSGCTRL.all = 0x00000000;
	ECanaMboxes.MBOX18.MSGCTRL.all = 0x00000000;
	ECanaMboxes.MBOX19.MSGCTRL.all = 0x00000000;
	ECanaMboxes.MBOX20.MSGCTRL.all = 0x00000000;
	ECanaMboxes.MBOX21.MSGCTRL.all = 0x00000000;
	ECanaMboxes.MBOX22.MSGCTRL.all = 0x00000000;
	ECanaMboxes.MBOX23.MSGCTRL.all = 0x00000000;
	ECanaMboxes.MBOX24.MSGCTRL.all = 0x00000000;
	ECanaMboxes.MBOX25.MSGCTRL.all = 0x00000000;
	ECanaMboxes.MBOX26.MSGCTRL.all = 0x00000000;
	ECanaMboxes.MBOX27.MSGCTRL.all = 0x00000000;
	ECanaMboxes.MBOX28.MSGCTRL.all = 0x00000000;
	ECanaMboxes.MBOX29.MSGCTRL.all = 0x00000000;
	ECanaMboxes.MBOX30.MSGCTRL.all = 0x00000000;
	ECanaMboxes.MBOX31.MSGCTRL.all = 0x00000000;

	// TAn, RMPn, GIFn bits are all zero upon reset and are cleared again
	//  as a matter of precaution.

	ECanaRegs.CANTA.all = 0xFFFFFFFF; /* Clear all TAn bits, writing 1 clears bit */

	ECanaRegs.CANRMP.all = 0xFFFFFFFF; /* Clear all RMPn bits, writing 1 clears bit  */

	ECanaRegs.CANGIF0.all = 0xFFFFFFFF; /* Clear all interrupt flag bits, writing 1 clears bit  */
	ECanaRegs.CANGIF1.all = 0xFFFFFFFF;

	/* Configure bit timing parameters for eCANA*/

	ECanaShadow.CANMC.all = ECanaRegs.CANMC.all;
	ECanaShadow.CANMC.bit.CCR = 1; // Set CCR = 1 Allows configuration
	ECanaRegs.CANMC.all = ECanaShadow.CANMC.all;

	// Wait until the CPU has been granted permission to change the configuration registers
	do
	{
		ECanaShadow.CANES.all = ECanaRegs.CANES.all;
	} while (ECanaShadow.CANES.bit.CCE != 1); // Wait for CCE bit to be set.. Allow config ack

	ECanaShadow.CANBTC.all = 0;
	/* The following block is for 80 MHz SYSCLKOUT. (40 MHz CAN module clock Bit rate = 1 Mbps
	   See Note at end of file. */
	// CAN Clock is enabled in dsp_hardware_initialization.c SysCtrlRegs.PCLKCR0.bit.ECANAENCLK  = 1;

	ECanaShadow.CANBTC.bit.BRPREG = 1;
	ECanaShadow.CANBTC.bit.TSEG2REG = 4;
	ECanaShadow.CANBTC.bit.TSEG1REG = 13;

	ECanaShadow.CANBTC.bit.SAM = 1; // Samples receive 3 times
	ECanaRegs.CANBTC.all = ECanaShadow.CANBTC.all;

	ECanaShadow.CANMC.all = ECanaRegs.CANMC.all;
	ECanaShadow.CANMC.bit.CCR = 0; // Set CCR = 0, Note: check register, it's still 1
	ECanaRegs.CANMC.all = ECanaShadow.CANMC.all;

	// Wait until the CPU no longer has permission to change the configuration registers
	do
	{
		ECanaShadow.CANES.all = ECanaRegs.CANES.all;
	} while (ECanaShadow.CANES.bit.CCE != 0); // Wait for CCE bit to be  cleared.. Ack CCR

	/* Disable all Mailboxes  */
	ECanaRegs.CANME.all = 0; // Required before writing the MSGIDs

	EDIS;
}
#endif // endif DSP28_ECANA

//---------------------------------------------------------------------------
// Example: InitECanGpio:
//---------------------------------------------------------------------------
// This function initializes GPIO pins to function as eCAN pins
//
// Each GPIO pin can be configured as a GPIO pin or up to 3 different
// peripheral functional pins. By default all pins come up as GPIO
// inputs after reset.
//
// Caution:
// Only one GPIO pin should be enabled for CANTXA operation.
// Only one GPIO pin shoudl be enabled for CANRXA operation.
// Comment out other unwanted lines.

#if DSP28_ECANA
void InitECanGpio(void)
{
	InitECanaGpio();
}
#endif

#if DSP28_ECANA
void InitECanaGpio(void)
{
	EALLOW;

	/* Enable internal pull-up for the selected CAN pins */
	// Pull-ups can be enabled or disabled by the user.
	// This will enable the pullups for the specified pins.
	// Comment out other unwanted lines.
	GpioCtrlRegs.GPADIR.bit.GPIO30 = 0;
	GpioCtrlRegs.GPADIR.bit.GPIO31 = 1;

	GpioCtrlRegs.GPAPUD.bit.GPIO30 = 0; // Enable pull-up for GPIO30 (CANRXA)
	GpioCtrlRegs.GPAPUD.bit.GPIO31 = 0; // Enable pull-up for GPIO31 (CANTXA)

	/* Set qualification for selected CAN pins to asynch only */
	// Inputs are synchronized to SYSCLKOUT by default.
	// This will select asynch (no qualification) for the selected pins.

	GpioCtrlRegs.GPAQSEL2.bit.GPIO30 = 3; // Asynch qual for GPIO30 (CANRXA)

	/* Configure eCAN-A pins using GPIO regs*/
	// This specifies which of the possible GPIO pins will be eCAN functional pins.

	GpioCtrlRegs.GPAMUX2.bit.GPIO30 = 1; // Configure GPIO30 for CANRXA operation
	GpioCtrlRegs.GPAMUX2.bit.GPIO31 = 1; // Configure GPIO31 for CANTXA operation

	EDIS;
}
#endif // endif DSP28_ECANA

/* Note: Bit timing parameters must be chosen based on the network parameters such as
   the sampling point desired and the propagation delay of the network. The propagation
   delay is a function of length of the cable, delay introduced by the
   transceivers and opto/galvanic-isolators (if any).

   The parameters used in this file must be changed taking into account the above mentioned
   factors in order to arrive at the bit-timing parameters suitable for a network.
*/

//===========================================================================
// End of file.
//===========================================================================

//###########################################################################
// Description:
//! \addtogroup f2806x_example_list
//! <h1>eCAN back to back (ecan_back2back)</h1>
//!
//! This example tests eCAN by transmitting data back-to-back at high speed
//! without stopping. The received data is verified. Any error is flagged.
//! MBX0 transmits to MBX16, MBX1 transmits to MBX17 and so on....
//! This program illustrates the use of self-test mode
//!
//! \b Watch \b Variables \n
//! - PassCount
//! - ErrorCount
//! - MessageReceivedCount
//
//
//###########################################################################
// $TI Release: F2806x C/C++ Header Files and Peripheral Examples V141 $
// $Release Date: January 19, 2015 $
// $Copyright: Copyright (C) 2011-2015 Texas Instruments Incorporated -
//             http://www.ti.com/ ALL RIGHTS RESERVED $
//###########################################################################

//#include "DSP28x_Project.h"     // Device Headerfile and Examples Include File

// Prototype statements for functions found within this file.
void mailbox_check(int32 T1, int32 T2, int32 T3);
void mailbox_read(int16 i);

// Global variable for this example
Uint32 ErrorCount = 0;
Uint32 PassCount = 0;
Uint32 MessageReceivedCount = 0;

Uint32 TestMbox1 = 0;
Uint32 TestMbox2 = 0;
Uint32 TestMbox3 = 0;

void eCAN_Test_init(void)
{

	// eCAN control registers require read/write access using 32-bits.  Thus we
	// will create a set of shadow registers for this example.  These shadow
	// registers will be used to make sure the access is 32-bits and not 16.
	//   struct ECAN_REGS ECanaShadow;

	// Step 1. Initialize System Control:
	// PLL, WatchDog, enable Peripheral Clocks
	// This example function is found in the F2806x_SysCtrl.c file.
	//InitSysCtrl();

	// Step 2. Initalize GPIO:
	// This example function is found in the F2806x_Gpio.c file and
	// illustrates how to set the GPIO to it's default state.
	// InitGpio();  // Skipped for this example

	// For this example, configure CAN pins using GPIO regs here
	// This function is found in F2806x_ECan.c
	InitECanGpio();

	// Step 3. Clear all interrupts and initialize PIE vector table:
	// Disable CPU interrupts
	//   DINT;

	// Initialize PIE control registers to their default state.
	// The default state is all PIE interrupts disabled and flags
	// are cleared.
	// This function is found in the F2806x_PieCtrl.c file.
	//   InitPieCtrl();

	// Disable CPU interrupts and clear all CPU interrupt flags:
	//   IER = 0x0000;
	//   IFR = 0x0000;

	// Initialize the PIE vector table with pointers to the shell Interrupt
	// Service Routines (ISR).
	// This will populate the entire table, even if the interrupt
	// is not used in this example.  This is useful for debug purposes.
	// The shell ISR routines are found in F2806x_DefaultIsr.c.
	// This function is found in F2806x_PieVect.c.
	//   InitPieVectTable();

	// Step 4. Initialize all the Device Peripherals:
	// This function is found in F2806x_InitPeripherals.c
	// InitPeripherals(); // Not required for this example

	// Step 5. User specific code, enable interrupts:

	//InitECana(); // Initialize eCAN-A module

	// Mailboxs can be written to 16-bits or 32-bits at a time
	// Write to the MSGID field of TRANSMIT mailboxes MBOX0 - 15
	ECanaMboxes.MBOX0.MSGID.all = 0x9555AAA0;
	ECanaMboxes.MBOX1.MSGID.all = 0x9555AAA1;
	ECanaMboxes.MBOX2.MSGID.all = 0x9555AAA2;
	ECanaMboxes.MBOX3.MSGID.all = 0x9555AAA3;
	ECanaMboxes.MBOX4.MSGID.all = 0x9555AAA4;
	ECanaMboxes.MBOX5.MSGID.all = 0x9555AAA5;
	ECanaMboxes.MBOX6.MSGID.all = 0x9555AAA6;
	ECanaMboxes.MBOX7.MSGID.all = 0x9555AAA7;
	ECanaMboxes.MBOX8.MSGID.all = 0x9555AAA8;
	ECanaMboxes.MBOX9.MSGID.all = 0x9555AAA9;
	ECanaMboxes.MBOX10.MSGID.all = 0x9555AAAA;
	ECanaMboxes.MBOX11.MSGID.all = 0x9555AAAB;
	ECanaMboxes.MBOX12.MSGID.all = 0x9555AAAC;
	ECanaMboxes.MBOX13.MSGID.all = 0x9555AAAD;
	ECanaMboxes.MBOX14.MSGID.all = 0x9555AAAE;
	ECanaMboxes.MBOX15.MSGID.all = 0x9555AAAF;

	// Write to the MSGID field of RECEIVE mailboxes MBOX16 - 31
	ECanaMboxes.MBOX16.MSGID.all = 0x9555AAA0;
	ECanaMboxes.MBOX17.MSGID.all = 0x9555AAA1;
	ECanaMboxes.MBOX18.MSGID.all = 0x9555AAA2;
	ECanaMboxes.MBOX19.MSGID.all = 0x9555AAA3;
	ECanaMboxes.MBOX20.MSGID.all = 0x9555AAA4;
	ECanaMboxes.MBOX21.MSGID.all = 0x9555AAA5;
	ECanaMboxes.MBOX22.MSGID.all = 0x9555AAA6;
	ECanaMboxes.MBOX23.MSGID.all = 0x9555AAA7;
	ECanaMboxes.MBOX24.MSGID.all = 0x9555AAA8;
	ECanaMboxes.MBOX25.MSGID.all = 0x9555AAA9;
	ECanaMboxes.MBOX26.MSGID.all = 0x9555AAAA;
	ECanaMboxes.MBOX27.MSGID.all = 0x9555AAAB;
	ECanaMboxes.MBOX28.MSGID.all = 0x9555AAAC;
	ECanaMboxes.MBOX29.MSGID.all = 0x9555AAAD;
	ECanaMboxes.MBOX30.MSGID.all = 0x9555AAAE;
	ECanaMboxes.MBOX31.MSGID.all = 0x9555AAAF;

	// Configure Mailboxes 0-15 as Tx, 16-31 as Rx
	// Since this write is to the entire register (instead of a bit
	// field) a shadow register is not required.
	ECanaRegs.CANMD.all = 0xFFFF0000;

	// Enable all Mailboxes */
	// Since this write is to the entire register (instead of a bit
	// field) a shadow register is not required.
	ECanaRegs.CANME.all = 0xFFFFFFFF;

	// Specify that 8 bytes will be sent/received
	ECanaMboxes.MBOX0.MSGCTRL.bit.DLC = 8;
	ECanaMboxes.MBOX1.MSGCTRL.bit.DLC = 8;
	ECanaMboxes.MBOX2.MSGCTRL.bit.DLC = 8;
	ECanaMboxes.MBOX3.MSGCTRL.bit.DLC = 8;
	ECanaMboxes.MBOX4.MSGCTRL.bit.DLC = 8;
	ECanaMboxes.MBOX5.MSGCTRL.bit.DLC = 8;
	ECanaMboxes.MBOX6.MSGCTRL.bit.DLC = 8;
	ECanaMboxes.MBOX7.MSGCTRL.bit.DLC = 8;
	ECanaMboxes.MBOX8.MSGCTRL.bit.DLC = 8;
	ECanaMboxes.MBOX9.MSGCTRL.bit.DLC = 8;
	ECanaMboxes.MBOX10.MSGCTRL.bit.DLC = 8;
	ECanaMboxes.MBOX11.MSGCTRL.bit.DLC = 8;
	ECanaMboxes.MBOX12.MSGCTRL.bit.DLC = 8;
	ECanaMboxes.MBOX13.MSGCTRL.bit.DLC = 8;
	ECanaMboxes.MBOX14.MSGCTRL.bit.DLC = 8;
	ECanaMboxes.MBOX15.MSGCTRL.bit.DLC = 8;

	// Write to the mailbox RAM field of MBOX0 - 15
	ECanaMboxes.MBOX0.MDL.all = 0x9555AAA0;
	ECanaMboxes.MBOX0.MDH.all = 0x89ABCDEF;

	ECanaMboxes.MBOX1.MDL.all = 0x9555AAA1;
	ECanaMboxes.MBOX1.MDH.all = 0x89ABCDEF;

	ECanaMboxes.MBOX2.MDL.all = 0x9555AAA2;
	ECanaMboxes.MBOX2.MDH.all = 0x89ABCDEF;

	ECanaMboxes.MBOX3.MDL.all = 0x9555AAA3;
	ECanaMboxes.MBOX3.MDH.all = 0x89ABCDEF;

	ECanaMboxes.MBOX4.MDL.all = 0x9555AAA4;
	ECanaMboxes.MBOX4.MDH.all = 0x89ABCDEF;

	ECanaMboxes.MBOX5.MDL.all = 0x9555AAA5;
	ECanaMboxes.MBOX5.MDH.all = 0x89ABCDEF;

	ECanaMboxes.MBOX6.MDL.all = 0x9555AAA6;
	ECanaMboxes.MBOX6.MDH.all = 0x89ABCDEF;

	ECanaMboxes.MBOX7.MDL.all = 0x9555AAA7;
	ECanaMboxes.MBOX7.MDH.all = 0x89ABCDEF;

	ECanaMboxes.MBOX8.MDL.all = 0x9555AAA8;
	ECanaMboxes.MBOX8.MDH.all = 0x89ABCDEF;

	ECanaMboxes.MBOX9.MDL.all = 0x9555AAA9;
	ECanaMboxes.MBOX9.MDH.all = 0x89ABCDEF;

	ECanaMboxes.MBOX10.MDL.all = 0x9555AAAA;
	ECanaMboxes.MBOX10.MDH.all = 0x89ABCDEF;

	ECanaMboxes.MBOX11.MDL.all = 0x9555AAAB;
	ECanaMboxes.MBOX11.MDH.all = 0x89ABCDEF;

	ECanaMboxes.MBOX12.MDL.all = 0x9555AAAC;
	ECanaMboxes.MBOX12.MDH.all = 0x89ABCDEF;

	ECanaMboxes.MBOX13.MDL.all = 0x9555AAAD;
	ECanaMboxes.MBOX13.MDH.all = 0x89ABCDEF;

	ECanaMboxes.MBOX14.MDL.all = 0x9555AAAE;
	ECanaMboxes.MBOX14.MDH.all = 0x89ABCDEF;

	ECanaMboxes.MBOX15.MDL.all = 0x9555AAAF;
	ECanaMboxes.MBOX15.MDH.all = 0x89ABCDEF;

	// Since this write is to the entire register (instead of a bit
	// field) a shadow register is not required.
	EALLOW;
	//ECanaRegs.CANMIM.all = 0xFFFFFFFF;						// Mailbox Interrupt Mask

	// Configure the eCAN for self test mode
	// Enable the enhanced features of the eCAN.
	EALLOW;
	ECanaShadow.CANMC.all = ECanaRegs.CANMC.all;
	// debug
	//	ECanaShadow.CANMC.bit.STM = 1;    // Configure CAN for self-test mode
	ECanaShadow.CANMC.bit.STM = 0; // Configure CAN for self-test mode
	// debug end
	ECanaRegs.CANMC.all = ECanaShadow.CANMC.all;
	EDIS;
}

void eCAN_Test(void)
{
	Uint16 j;

	// Begin transmitting
	ECanaRegs.CANTRS.all = 0x0000FFFF; // Set TRS for all transmit mailboxes
	// debug
	//while(ECanaRegs.CANTA.all != 0x0000FFFF ) {}  // Wait for all TAn bits to be set..
	// debug end
	ECanaRegs.CANTA.all = 0x0000FFFF; // Clear all TAn
	MessageReceivedCount++;

	//Read from Receive mailboxes and begin checking for data */
	for (j = 16; j < 32; j++) // Read & check 16 mailboxes
	{
		mailbox_read(j);								// This func reads the indicated mailbox data
		mailbox_check(TestMbox1, TestMbox2, TestMbox3); // Checks the received data
	}
}

// This function reads out the contents of the indicated
// by the Mailbox number (MBXnbr).
void mailbox_read(int16 MBXnbr)
{
	volatile struct MBOX *Mailbox;
	Mailbox = &ECanaMboxes.MBOX0 + MBXnbr;
	TestMbox1 = Mailbox->MDL.all;   // = 0x9555AAAn (n is the MBX number)
	TestMbox2 = Mailbox->MDH.all;   // = 0x89ABCDEF (a constant)
	TestMbox3 = Mailbox->MSGID.all; // = 0x9555AAAn (n is the MBX number)

} // MSGID of a rcv MBX is transmitted as the MDL data.

void mailbox_check(int32 T1, int32 T2, int32 T3)
{
	if ((T1 != T3) || (T2 != 0x89ABCDEF))
	{
		ErrorCount++;
	}
	else
	{
		PassCount++;
	}
}

//===========================================================================
// No more.
//===========================================================================
