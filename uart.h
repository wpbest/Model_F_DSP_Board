// uart.h

//#ifndef _uart_h										// inclusion guard
//#define _uart_h
#ifndef UART_H
#define UART_H

#include "system_config.h" // includes file name for configuration
#include SYSTEM_CONFIG	 // configuration header file

#define CR 0x0D
#define LF 0x0A

#define USART_BUFSIZE (2000)   // default buffer size
#define USART0_RXBUFSIZE (256) // receive buffer size - must be a power of 2
#define USART0_TXBUFSIZE (256) // transmit buffer size - must be a power of 2
#define USART1_RXBUFSIZE (256) // receive buffer size - must be a power of 2
#define USART1_TXBUFSIZE (256) // transmit buffer size - must be a power of 2

//Interrupt Enable/Disable for C2000
#define USART0_RX_INT_DISABLE SciaRegs.SCICTL2.bit.RXBKINTENA = 0 // Receive interrupt enable, 1 = enabled, 0 = disabled
#define USART0_RX_INT_ENABLE SciaRegs.SCICTL2.bit.RXBKINTENA = 1  // Receive interrupt enable, 1 = enabled, 0 = disabled
#define USART0_TX_INT_DISABLE          \
	SciaRegs.SCICTL2.bit.TXINTENA = 0; \
	SciaRegs.SCIFFTX.bit.TXFFIENA = 0;
#define USART0_TX_INT_ENABLE           \
	SciaRegs.SCICTL2.bit.TXINTENA = 1; \
	SciaRegs.SCIFFTX.bit.TXFFIENA = 1;
#define USART1_RX_INT_DISABLE ScibRegs.SCICTL2.bit.RXBKINTENA = 0 // Receive interrupt enable, 1 = enabled, 0 = disabled
#define USART1_RX_INT_ENABLE ScibRegs.SCICTL2.bit.RXBKINTENA = 1  // Receive interrupt enable, 1 = enabled, 0 = disabled
#define USART1_TX_INT_DISABLE          \
	ScibRegs.SCICTL2.bit.TXINTENA = 0; \
	ScibRegs.SCIFFTX.bit.TXFFIENA = 0;
#define USART1_TX_INT_ENABLE           \
	ScibRegs.SCICTL2.bit.TXINTENA = 1; \
	ScibRegs.SCIFFTX.bit.TXFFIENA = 1;

#define MAX_PORT (2)

#define LSPCLK_FREQ (DSPCLOCK / 4)
// SCIA_PRD has an 8 bit high and 8 bit low register, calculate and break it down
// the next define had an 'integer out of range' problem making all of the following calculations having the same warning
#define SCIA_PRD (((long)LSPCLK_FREQ / ((long)SERIAL_A_BAUD_RATE * 8)) - 1)
#define SCIAH_PRD (SCIA_PRD / 256)
#define SCIAL_PRD (SCIA_PRD - (SCIAH_PRD * 256))
#define SCIB_PRD (((long)LSPCLK_FREQ / ((long)SERIAL_B_BAUD_RATE * 8)) - 1)
#define SCIBH_PRD (SCIB_PRD / 256)
#define SCIBL_PRD (SCIB_PRD - (SCIBH_PRD * 256))

// Defines for SCIB and RS485
#define RXD_ISO_SELECT_PIN GpioDataRegs.GPADAT.bit.GPIO21
#define RXD_ISO_SELECT_485 GpioDataRegs.GPADAT.bit.GPIO21 = 0;
#define RXD_ISO_SELECT_232 GpioDataRegs.GPADAT.bit.GPIO21 = 1;
#define RS485TXEN_ON GpioDataRegs.GPADAT.bit.GPIO8 = 1;
#define RS485TXEN_OFF GpioDataRegs.GPADAT.bit.GPIO8 = 0;
#define RS485_TE_ON GpioDataRegs.GPBDAT.bit.GPIO52 = 0;
#define RS485_TE_OFF GpioDataRegs.GPBDAT.bit.GPIO52 = 1;

typedef enum portType {
	RS485,
	RS232
} portType_t;

/*
typedef enum {
	UPS1_PORT,		// Port 0
	UPS2_PORT,		// Port 1
	SNMP_PORT,		// Port 2
	MSP_PORT		// Port 3
} portName_t;
*/

typedef enum
{
	UPS1_PORT,		// Port 0
	UPS2_PORT,		// Port 1
	UART_PORT0 = 0, // Port 0
	UART_PORT1 = 1  // Port 1
} portName_t;

struct uartDataStrucT
{
	int bufferSize, txReadIndex, txWriteIndex, rxReadIndex, rxWriteIndex;
	int txCharCount, rxCharCount;
	portType_t portType;
	char txBuffer[USART_BUFSIZE];
	char rxBuffer[USART_BUFSIZE];
};

void usart_init(void);
void SCIB_port_init(void);
interrupt void sciaTxFifoIsr(void);
interrupt void sciaRxFifoIsr(void);
interrupt void scibTxFifoIsr(void);
interrupt void scibRxFifoIsr(void);
void usart_tx_buffer_flush(int port);
char usart_putchar(char cByte, int port);
int usart_rx_buffer_count(int port);
void usart_rx_buffer_flush(int port);
char usart_getchar(int port);
char usart_peekchar(int port);
void usart_putstr(char *str, int port);
char usart_putbuffer(unsigned char *str, unsigned char length, int port);
int usart_tx_buffer_count(int port);
int usartBufferCheck(void);
void bankSwitchBaudSet(void);

#endif // _uart_h
