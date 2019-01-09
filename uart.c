/*
 * FILE: uscia0_UART.c
 * TARGET: TMS320F2806x
 * AUTHOR: Dan Rhodes, very modified by Mark James
 * DESCRIPTION:	 Implements an interrupt driven, full duplex,
*				 buffered UART using uscia USART

USART0 is UPS 1
USART1 is UPS 2
USART3 is MSP430 interchip comm
	UPS1_PORT,		// Port 0
	UPS2_PORT,		// Port 1
	SNMP_PORT,		// Port 0 too
*/

#include "system_config.h" // includes file name for configuration
#include SYSTEM_CONFIG	 // configuration header file
#include "uart.h"
#include "types.h"
#include "string.h"
#include "event_controller.h"

extern event_controller_data_t eventData, *pEventData;
struct uartDataStrucT usartPort[MAX_PORT]; // Actual structure for each UART

/*
Name: usart0_init
Description: init the RS232 driver system
Parameter: -
Returns: -
*/
void usart_init(void)
{
	int port;
	for (port = 0; port < MAX_PORT; port++)
	{
		if ( port != UART_PORT1 )					// UART_PORT1 done in SCIB_port_init() below
		{
			usartPort[port].txReadIndex = 0;
			usartPort[port].txWriteIndex = 0;
			usartPort[port].rxReadIndex = 0;
			usartPort[port].rxWriteIndex = 0;
			usartPort[port].bufferSize = USART_BUFSIZE;
			usartPort[port].txCharCount = 0;
			usartPort[port].rxCharCount = 0;
			usartPort [port].portType = RS232;
		}	
	}

	// SCIA

	// Interrupts used in this code are re-mapped to ISR functions found within this file.
	EALLOW;

	PieVectTable.SCIRXINTA = &sciaRxFifoIsr;
	PieVectTable.SCITXINTA = &sciaTxFifoIsr;
	
	EDIS;

	EALLOW;

    // SCIA GPIO Setup
	GpioCtrlRegs.GPAPUD.bit.GPIO28 = 0;   // Enable pull-up for GPIO28 (SCIRXDA)
	GpioCtrlRegs.GPAQSEL2.bit.GPIO28 = 3; // Asynch input GPIO28 (SCIRXDA)
	GpioCtrlRegs.GPAMUX2.bit.GPIO28 = 1;  // Configure GPIO28 for SCIRXDA operation
	GpioCtrlRegs.GPAMUX1.bit.GPIO12 = 2;  // RS232 TX
	GpioCtrlRegs.GPADIR.bit.GPIO12 = 1;   // Pin is an output

	EDIS;

	SciaRegs.SCICCR.bit.SCICHAR = 7;		// Number of bits x+1, 7=8bits
	SciaRegs.SCICCR.bit.ADDRIDLE_MODE = 0;	// Address or Idle mode, 0=Idle
	SciaRegs.SCICCR.bit.LOOPBKENA = 0;		// Loopback Enable, 0=no
	SciaRegs.SCICCR.bit.PARITYENA = 0;		// Parity Enable, 0=no
	SciaRegs.SCICCR.bit.PARITY = 0;			// Parity 0=odd 1=even, parity not enabled
	SciaRegs.SCICCR.bit.STOPBITS = 0;		// Stop bits 0=1 1=2
	SciaRegs.SCICTL1.bit.RXENA = 1;  		// enable RX, connects pin to receive circuitry
	SciaRegs.SCICTL1.bit.TXENA = 1;			// enable TX, connects pin to transmit circuitry
	SciaRegs.SCICTL1.bit.SLEEP = 0;			// sleep mode, 0=disabled
	SciaRegs.SCICTL1.bit.TXWAKE = 0;		// transmit wake up mode, 1=mode selected by ADDRIDLE, 0=no mode
	SciaRegs.SCICTL1.bit.RXERRINTENA = 0;	// enable receive error interrupt 0=disable
	SciaRegs.SCICTL2.bit.TXINTENA = 1;
	SciaRegs.SCICTL2.bit.RXBKINTENA = 1;
	SciaRegs.SCIHBAUD = SCIAH_PRD;			// set baud rate
	SciaRegs.SCILBAUD = SCIAL_PRD;
	SciaRegs.SCIFFTX.bit.TXFFIL = 0;		// FIFO depth interrupt level, 0=int every char
	SciaRegs.SCIFFTX.bit.TXFFIENA = 1;		// Transmit FIFO interrupt enable, 1=enabled
	SciaRegs.SCIFFTX.bit.TXFFINTCLR = 1;	// Clear interrupt flag, 1=clear, 0=don't clear
	SciaRegs.SCIFFTX.bit.TXFIFOXRESET = 1;	// 0=reset FIFO, 1=Re-enable FIFO operation
	SciaRegs.SCIFFTX.bit.SCIFFENA = 1;		// FIFO enhancements, 0=disabled, 1=enabled
	SciaRegs.SCIFFTX.bit.SCIRST = 1;		// SCI Reset, 0=hold in reset, 1=enable transmit/receive
	SciaRegs.SCIFFRX.bit.RXFFIL = 1;		// Receive FIFO interrupt level, must match RXFFST pattern, 1=1word
	SciaRegs.SCIFFRX.bit.RXFFIENA = 1;		// Receive FIFO interrupt enable, 1=enabled
	SciaRegs.SCIFFRX.bit.RXFFINTCLR = 1;	// Clear interrupt flag, 1=clear, 0=don't clear
	SciaRegs.SCIFFRX.bit.RXFIFORESET = 1;	// 0=reset FIFO, 1=Re-enable FIFO operation
	SciaRegs.SCIFFRX.bit.RXFFOVRCLR = 1;	// Clear FIFO overflow flag RXFFOVF, 1=clear
	SciaRegs.SCICTL1.bit.SWRESET = 1;		// 0=software reset also done on chip startup reset
									  		// 1=turns off reset state, do this to allow SCI to operate
									  		// doesn't effect register configuration bits

	// Note: SERIAL_B_PORT_TYPE_RS485 established in SCIB_port_init() below

	PieCtrlRegs.PIEIER9.bit.INTx1 = 1; 		// SCIRXAINT1 in PIE Int9.1
									   		//	PieCtrlRegs.PIEIFR9.bit.INTx1 = 0;		// SCIRXAINT1 in PIE Int9.1 Flag
	PieCtrlRegs.PIEIER9.bit.INTx2 = 1; 		// SCITXAINT1 in PIE Int9.2
									   		//	PieCtrlRegs.PIEIFR9.bit.INTx2 = 0;		// SCITXAINT1 in PIE Int9.2 Flag
	IER |= M_INT9;					   		// Make sure core group 9 interrupt is enabled
}
/*
Name: Debug Port B init - SCIB
Description: init the RS232 driver system
Parameter: -
Returns: -
*/
void SCIB_port_init(void)
{
	int port = UART_PORT1;

	usartPort[port].txReadIndex = 0;
	usartPort[port].txWriteIndex = 0;
	usartPort[port].rxReadIndex = 0;
	usartPort[port].rxWriteIndex = 0;
	usartPort[port].bufferSize = USART_BUFSIZE;
	usartPort[port].txCharCount = 0;
	usartPort[port].rxCharCount = 0;
	usartPort [port].portType = RS232;

	int i;
	for ( i = 0; i < USART_BUFSIZE; i++)
	{
	    usartPort[port].txBuffer[i] = usartPort[port].rxBuffer[i] = 0;  // Clear the buffers -- easier for debug
	}

	// SCIB setup

	// Interrupts used in this code are re-mapped to ISR functions found within this file.
	EALLOW;

	PieVectTable.SCIRXINTB = &scibRxFifoIsr;
	PieVectTable.SCITXINTB = &scibTxFifoIsr;
	
	EDIS;

	EALLOW;

	// SCIB GPIO Setup
	GpioCtrlRegs.GPAPUD.bit.GPIO15 = 0;   // Enable pull-up for GPIO15 (SCIRXDB)
	GpioCtrlRegs.GPAQSEL1.bit.GPIO15 = 3; // Asynchronous input GPIO15 (SCIRXDB)
	GpioCtrlRegs.GPAMUX1.bit.GPIO15 = 2;  // Configure GPIO15 for SCIRXDB operation -- was wrong, set to 1 not SCIRXDB
	GpioCtrlRegs.GPAMUX1.bit.GPIO14 = 2;  // RS232 TX (SCITXDB)
	GpioCtrlRegs.GPADIR.bit.GPIO14 = 1;   // Pin is an output

	// SCIB GPIO RS485 Setup
	GpioCtrlRegs.GPAMUX1.bit.GPIO8 = 0;  // RS485 Isolated Transmit enable
	GpioCtrlRegs.GPADIR.bit.GPIO8 = 1;   // Pin is an output
	GpioCtrlRegs.GPBMUX2.bit.GPIO52 = 0; // RS485 Isolated Terminating Resistor Enable
	GpioCtrlRegs.GPBDIR.bit.GPIO52 = 1;  // Pin is an output
	GpioCtrlRegs.GPAMUX2.bit.GPIO21 = 0; // RXD Isolated Select
	GpioCtrlRegs.GPADIR.bit.GPIO21 = 1;  // Pin is an output

	#ifdef SERIAL_B_PORT_TYPE_RS485
		GpioDataRegs.GPACLEAR.bit.GPIO8 = 0;
		usartPort[UART_PORT1].portType = RS485;
		RXD_ISO_SELECT_485;
		RS485_TE_ON;
		RS485TXEN_OFF;
	#else
		GpioDataRegs.GPASET.bit.GPIO8 = 1;
		usartPort[UART_PORT1].portType = RS232;
		RXD_ISO_SELECT_232;
	#endif

	EDIS;

	ScibRegs.SCICTL1.bit.SWRESET = 0;	   // Make sure port is ready for init -- state can be uncertain without this
	ScibRegs.SCICCR.bit.SCICHAR = 7;	   // Number of bits x+1, 7 = 8-bits
	ScibRegs.SCICCR.bit.ADDRIDLE_MODE = 0; // Address or Idle mode, 0 = Idle
	ScibRegs.SCICCR.bit.LOOPBKENA = 0;	   // Loopback Enable, 0 = no
	ScibRegs.SCICCR.bit.PARITYENA = 0;	   // Parity Enable, 0 = no
	ScibRegs.SCICCR.bit.PARITY = 0;		   // Parity 0 = odd 1 = even, parity not enabled
	ScibRegs.SCICCR.bit.STOPBITS = 0;	   // Stop bits 0 = 1, 1 = 2
	ScibRegs.SCICTL1.bit.RXENA = 1;		   // Enable RX, connects pin to receive circuitry
	ScibRegs.SCICTL1.bit.TXENA = 1;		   // Enable TX, connects pin to transmit circuitry
	ScibRegs.SCICTL1.bit.SLEEP = 0;		   // Sleep mode, 0 = disabled
	ScibRegs.SCICTL1.bit.TXWAKE = 0;	   // Transmit wake up mode, 1 = mode selected by ADDRIDLE, 0 = no mode
	ScibRegs.SCICTL1.bit.RXERRINTENA = 0;  // Enable receive error interrupt 0=disable
	ScibRegs.SCICTL2.bit.TXINTENA = 1;
	ScibRegs.SCICTL2.bit.RXBKINTENA = 1;
	ScibRegs.SCIHBAUD = SCIBH_PRD; 		   // Initial baud rate is set to 9600 baud. This may be changed later.
	ScibRegs.SCILBAUD = SCIBL_PRD;
	ScibRegs.SCIFFTX.bit.TXFFIL = 0;	   // FIFO depth interrupt level, 0 = interrupt every char
	ScibRegs.SCIFFTX.bit.TXFFIENA = 1;	   // Transmit FIFO interrupt enable, 1 = enabled
	ScibRegs.SCIFFTX.bit.TXFFINTCLR = 1;   // Clear interrupt flag, 1 = clear, 0 = don't clear
	ScibRegs.SCIFFTX.bit.TXFIFOXRESET = 1; // 0 = reset FIFO, 1 = Re-enable FIFO operation
	ScibRegs.SCIFFTX.bit.SCIFFENA = 1;	   // FIFO enhancements, 0 = disabled, 1 = enabled
	ScibRegs.SCIFFTX.bit.SCIRST = 1;	   // SCI Reset, 0 = hold in reset, 1 = enable transmit/receive
	ScibRegs.SCIFFRX.bit.RXFFIL = 1;	   // Receive FIFO interrupt level, must match RXFFST pattern, 1 = 1 word
	ScibRegs.SCIFFRX.bit.RXFFIENA = 1;	   // Receive FIFO interrupt enable, 1 = enabled
	ScibRegs.SCIFFRX.bit.RXFFINTCLR = 1;   // Clear interrupt flag, 1 = clear, 0 = don't clear
	ScibRegs.SCIFFRX.bit.RXFIFORESET = 1;  // 0=reset FIFO, 1 = Re-enable FIFO operation
	ScibRegs.SCIFFRX.bit.RXFFOVRCLR = 1;   // Clear FIFO overflow flag RXFFOVF, 1 = clear
	ScibRegs.SCICTL1.bit.SWRESET = 1;	   // 1 = Release the SCIB for operation

	PieCtrlRegs.PIEIER9.bit.INTx3 = 1;     // SCIRXBINT1 in PIE Int9.3
									       //	PieCtrlRegs.PIEIFR9.bit.INTx3 = 0;		// SCIRXBINT1 in PIE Int9.3 Flag
	PieCtrlRegs.PIEIER9.bit.INTx4 = 1;     // SCITXBINT1 in PIE Int9.4
									       //	PieCtrlRegs.PIEIFR9.bit.INTx4 = 0;		// SCITXBINT1 in PIE Int9.4 Flag
	IER |= M_INT9;					       // Make sure core group 9 interrupt is enabled
}

// TX SCIA UART_PORT0
interrupt void sciaTxFifoIsr(void)
{
	if (usartPort[UART_PORT0].txCharCount)
	{																		   // send if chars are in buffer
		SciaRegs.SCITXBUF = usartPort[UART_PORT0].txBuffer[usartPort[UART_PORT0].txReadIndex++]; // load tx register, inc index
		if (usartPort[UART_PORT0].txReadIndex >= usartPort[UART_PORT0].bufferSize)
		{								  // end of circular buffer?
			usartPort[UART_PORT0].txReadIndex = 0; // reset index
		}
		usartPort[UART_PORT0].txCharCount--; // char sent, dec count
	}
	if ((usartPort[UART_PORT0].txCharCount == 0) || (SciaRegs.SCIFFTX.bit.TXFFST == 0))
	{
		USART0_TX_INT_DISABLE;
	}

	SciaRegs.SCIFFTX.bit.TXFFINTCLR = 1; // Clear SCI Interrupt flag
	PieCtrlRegs.PIEACK.bit.ACK9 = 1; // Acknowledge interrupt to PIE
}

// TX SCIB UART_PORT1
interrupt void scibTxFifoIsr(void)
{
	int i;
	if (usartPort[UART_PORT1].txCharCount)
	{ // Transmit only if characters are in the transmit buffer
		if (RXD_ISO_SELECT_PIN == 0)
		{				  // Are we in RS485 Mode?
			RS485TXEN_ON; // Yes, turn on the Transmit Enable Line
			for (i = 0; i < 20; i++)
			{ // Delay a bit (carry over from Serial Router Board)
				NOP;
			}																					 // End of small delay
		}																						 // End of RS485 check
		ScibRegs.SCITXBUF = usartPort[UART_PORT1].txBuffer[usartPort[UART_PORT1].txReadIndex++]; // Load SCIB transmit register, increment the index
		if (usartPort[UART_PORT1].txReadIndex >= usartPort[UART_PORT1].bufferSize)
		{										   // Is it the end of the circular buffer?
			usartPort[UART_PORT1].txReadIndex = 0; // Yes, reset the index
		}										   // End of buffer rollover check
		usartPort[UART_PORT1].txCharCount--;	   // Decrement the transmit character count since we transmitted a character
	}											   // End of pending characters check
	if ((usartPort[UART_PORT1].txCharCount == 0) || (ScibRegs.SCIFFTX.bit.TXFFST == 0))
	{									 // If no more characters and SCIB FIFO is empty, then...
		USART1_TX_INT_DISABLE;			 // Turn off the various SCIB transmit interrupt
	}									 // End of check
	ScibRegs.SCIFFTX.bit.TXFFINTCLR = 1; // Clear SCIB Interrupt flag
	PieCtrlRegs.PIEACK.bit.ACK9 = 1;	 // Acknowledge interrupt to PIE
}

// RX SCIA UART_PORT0
interrupt void sciaRxFifoIsr(void)
{
	usartPort[0].rxBuffer[usartPort[UART_PORT0].rxWriteIndex++] = SciaRegs.SCIRXBUF.all; // store received byte and inc receive index
	if (usartPort[UART_PORT0].rxWriteIndex >= usartPort[UART_PORT0].bufferSize)
	{								   // end of circular buffer?
		usartPort[UART_PORT0].rxWriteIndex = 0; // reset index
	}
	usartPort[UART_PORT0].rxCharCount++; // received, inc count
	SciaRegs.SCIFFRX.bit.RXFFOVRCLR = 1; // Clear Overflow flag
	SciaRegs.SCIFFRX.bit.RXFFINTCLR = 1; // Clear Interrupt flag
	PieCtrlRegs.PIEACK.bit.ACK9 = 1; // Acknowledge interrupt to PIE
}

// RX SCIB UART_PORT1
interrupt void scibRxFifoIsr(void)
{
	usartPort[UART_PORT1].rxBuffer[usartPort[UART_PORT1].rxWriteIndex++] = ScibRegs.SCIRXBUF.all; // Store the received byte and increment the receive index
	if (usartPort[UART_PORT1].rxWriteIndex >= usartPort[UART_PORT1].bufferSize)
	{											// Is it the end of the circular buffer?
		usartPort[UART_PORT1].rxWriteIndex = 0; // Yes, reset the index
	}											// End of buffer rollover check
	usartPort[UART_PORT1].rxCharCount++;		// Increment the received character count
	ScibRegs.SCIFFRX.bit.RXFFOVRCLR = 1;		// Clear SCIB Overflow flag
	ScibRegs.SCIFFRX.bit.RXFFINTCLR = 1;		// Clear SCIB Interrupt flag
	PieCtrlRegs.PIEACK.bit.ACK9 = 1;			// Acknowledge interrupt to PIE
}



/*
Name: usart_putchar
Description: stores one char in TX buffer. If it's the first one,
send it immediately. Rest is sent by TXInterrupt automatically
Parameter: char cByte (to store in buffer)
Returns: -
*/
char usart_putchar(char cByte, int port)
{
	if ((port >= 0) && (port < MAX_PORT))
	{
		if (usartPort[port].txCharCount >= usartPort[port].bufferSize)
		{								 // Are we full up?
			usart_tx_buffer_flush(port); // Yes, reset buffers and counters (IE flush)
			return FALSE;				 // Indicate character not put in buffer
		}
		usartPort[port].txBuffer[usartPort[port].txWriteIndex++] = cByte; // Load byte to buffer and increment index
		if (usartPort[port].txWriteIndex >= usartPort[port].bufferSize)
		{ // If index past end of circular buffer, roll over
			usartPort[port].txWriteIndex = 0;
		}
		switch (port)
		{
		case UART_PORT0:
			USART0_TX_INT_DISABLE;
			usartPort[port].txCharCount++; // New character, increment count
			USART0_TX_INT_ENABLE;
			break;
		case UART_PORT1:
			USART1_TX_INT_DISABLE;		   // Disable SCIB transmit interrupt
			usartPort[port].txCharCount++; // New character, increment count
			USART1_TX_INT_ENABLE;		   // Enable SCIB transmit interrupt
			break;
		default:
			break;
		}
	}
	EINT;
	return cByte;
}

/*
Name: usart0_putstr
Description: stores a string in the TX buffer. 
Parameter: pointer to string
Returns: -
*/
void usart_putstr(char *str, int port)
{
	int length, i;
	length = strlen(str);
	if ((port >= 0) && (port < MAX_PORT) && (length < USART_BUFSIZE))
	{ // don't accept message bigger than buffer
		for (i = 0; i < length; i++)
		{
			if (usartPort[port].txCharCount < USART_BUFSIZE)
			{ // only add if there is room in buffer
				usart_putchar(str[i], port);
			}
		}
	}
}

/*
Name: usart0_putbuffer
Description: stores a buffer in the TX buffer. 
Parameter: pointer to buffer, number of chars
Returns: -
*/
char usart_putbuffer(unsigned char *str, unsigned char length, int port)
{
	unsigned int i;
	if ((port >= 0) && (port < MAX_PORT) && (length < USART_BUFSIZE))
	{ // don't accept message bigger than buffer
		for (i = 0; i < length; i++)
		{
			if (usartPort[port].txCharCount < USART_BUFSIZE)
			{ // only add if there is room in buffer
				usart_putchar(str[i], port);
			}
		}
	}
	return 0;
}

/*
Name: usart0_rx_buffer_count
Description: How many chars are stored in RX buffer ?
if main routine wants to read chars, it has
to check first if RXBufCnt1() returns !=0
Parameter: -
Returns: number of chars in receive buffer
*/
int usart_rx_buffer_count(int port)
{
	int count = 0;
	if ((port >= 0) && (port < MAX_PORT))
	{
		count = usartPort[port].rxCharCount;
	}
	return (count);
}

/*
Name: usart0_rx_buffer_flush
Description: Resets pointers and count to effectively "empty" buffer
Returns: nothing
*/
void usart_rx_buffer_flush(int port)
{
	if ((port >= 0) && (port < MAX_PORT))
	{
		usartPort[port].rxReadIndex = 0;
		usartPort[port].rxWriteIndex = 0;
		usartPort[port].rxCharCount = 0;
	}
}

/*
Name: getchar0
Description: Get one char from RX buffer. Multiple calls will
return all chars.
Parameter: -
Returns: next valid char in receive buffer
*/
char usart_getchar(int port)
{
	char cByte = 0;
	if ((port >= 0) && (port < MAX_PORT))
	{ // Is it a valid port?
		if (usartPort[port].rxCharCount)
		{																	 // Is the character still available?
			cByte = usartPort[port].rxBuffer[usartPort[port].rxReadIndex++]; // Yes, get the byte from the receive buffer
			if (usartPort[port].rxReadIndex >= usartPort[port].bufferSize)
			{									 // Check for buffer rollover
				usartPort[port].rxReadIndex = 0; // Reset index if it rolled
			}									 // End of rollover check
			switch (port)
			{								   // Select the port to update
			case UART_PORT0:				   // SCIA
				USART0_RX_INT_DISABLE;		   // Disable receive interrupt
				usartPort[port].rxCharCount--; // One char read, decrement count
				USART0_RX_INT_ENABLE;		   // Done, enable receive interrupt
				break;						   // Done with case
			case UART_PORT1:				   // SCIB
				USART1_RX_INT_DISABLE;		   // Disable receive interrupt
				usartPort[port].rxCharCount--; // One char read, decrement count
				USART1_RX_INT_ENABLE;		   // Done, enable receive interrupt
				break;						   // Done with case
			default:						   // Default... should never get here
				break;						   // Done with case
			}								   // End of port switch
		}									   // End of character still available check
	}										   // End of port check
	return (cByte);							   // Return the received data
} // End of usart_getchar

/*
Name: peekchar0
Description: non-destructively looks at next char from RX buffer.
Parameter: -
Returns: next valid char in receive buffer
*/
char usart_peekchar(int port)
{
	char cByte = 0;
	if ((port >= 0) && (port < MAX_PORT))
	{
		if (usartPort[port].rxCharCount)
		{																   // char still available
			cByte = usartPort[port].rxBuffer[usartPort[port].rxReadIndex]; // get byte from buffer
		}
	}
	return (cByte);
}

/*
Name: usart0_tx_buffer_count
Description: How many chars are stored in TX buffer ?
Parameter: -
Returns: number of chars in TX buffer
*/
int usart_tx_buffer_count(int port)
{
	int count = 0;
	if ((port >= 0) && (port < MAX_PORT))
	{
		count = (usartPort[port].txCharCount);
	}
	return count;
}

/*
Name: usart0_tx_buffer_flush
Description: Reset transmit pointers seeming to flush the buffer
Parameter: -
Returns: -
*/
void usart_tx_buffer_flush(int port)
{
	if ((port >= 0) && (port < MAX_PORT))
	{
		usartPort[port].txReadIndex = 0;
		usartPort[port].txWriteIndex = 0;
		usartPort[port].txCharCount = 0;
	}
}

int usartBufferCheck(void)
{
	int port, bad;
	bad = FALSE;
	for (port = 0; port < MAX_PORT; port++)
	{
		if (usartPort[port].rxCharCount >= usartPort[port].bufferSize)
		{								 // full up
			usart_rx_buffer_flush(port); // reset buffers and counters (IE flush)
			bad = TRUE;					 // there was a problem
		}
	}
	return bad; // if there was a problem
}

void bankSwitchBaudSet(void)
{
	long temp;
	Uint16 baudTemp;
	switch (pEventData->systemData.bankSwitches.bit.RS232_BAUD_RATE)
	{ // Get configuration switch setting for baud
	case 0:
		temp = 1200;
		break;
	case 2:
		temp = 2400;
		break;
	case 1:
		temp = 4800;
		break;
	case 3:
		temp = 9600;
		break;
	}
	temp = (((long)LSPCLK_FREQ / ((long)temp * 8)) - 1);
	baudTemp = (Uint16)temp / 256;
	SciaRegs.SCIHBAUD = baudTemp;
	baudTemp = (temp - (baudTemp * 256));
	SciaRegs.SCILBAUD = baudTemp;
}
