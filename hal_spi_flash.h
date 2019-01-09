/*!	@file		hal_spi_flash.h
 *	@author 	Argil Shaver
 *	@date		29 December 2014
 *	@version	1.04
 *	@copyright	IntelliPower, Inc. (c) 2014
 *
 *	@brief		Performs Hardware Access Level control of SPI interface to External Serial SPI Flash Device and MSP430.
 *
 * TARGET: 		MSP430x5419, MSP430x5419A, and TMS320x2806x devices
 *
 * HISTORY:
 * -			1.04	29 December 2014
 * 						Made modifications to the MSP430 Interface to handle the DSP changes.
 * -			1.03	08 December 2014
 *						Cleaned up and adjusted the SPI hardware code for the TMS320x2806x device.
 *						System now uses interrupts and the FIFO correctly.
 * -			1.02	19 November 2014
 * 						Fixed the code when an extra NULL character would overwrite in memory. Cleaned
 * 						up the read codes and made them match in syntax.
 * -			1.01	11 November 2014
 * 						Fixed the DSP Functions for Chip Enable for interfacing to the MSP430, and
 * 						adjusted the major function calls so that that return commandComplete as TRUE
 * 						only when the function is complete.
 * -			1.00	Initial Version	17 October 2014
 */
#ifndef HAL_SPI_FLASH_H_
#define HAL_SPI_FLASH_H_

#include "types.h"
#include "timer.h"
#include "system_config.h"

//! From define above this section selects the required interface header file
#if defined MSP430_INTERFACE
#include <msp430x54x.h>
#define BATTERY_MAX_JOULES 0L
#endif
#if defined DSP_INTERFACE
#include "F2806x_Device.h"
#endif

#if defined MSP430_INTERFACE
//! Defines the dividing of peripheral clock for interface
#define BITRATECONTROL (91)
//! Defines the setting of Interface Port Pin Low for Flash Chip Enable
#define FLASH_CE_LOW P10OUT &= ~BIT0
//! Defines the setting of Interface Port Pin Low for MSP430 Enable
#define MSP430_CE_LOW FLASH_CE_LOW
//! Defines the setting of Interface Port Pin High for Flash Chip Enable
#define FLASH_CE_HIGH P10OUT |= BIT0
//! Defines the setting of Interface Port Pin High for Flash Chip Enable
#define MSP430_CE_HIGH FLASH_CE_HIGH
//! Defines SPI Receive Interrupt Disable function
#define SPI_RX_INT_DISABLE UCB3IE &= ~UCRXIE
//! Defines SPI Receive Interrupt Enable function
#define SPI_RX_INT_ENABLE UCB3IE |= UCRXIE
//! Defines SPI Transmit Interrupt Disable function
#define SPI_TX_INT_DISABLE UCB3IE &= ~UCTXIE
//! Defines SPI Transmit Interrupt Enable function
#define SPI_TX_INT_ENABLE UCB3IE |= UCTXIE
#endif

#if defined DSP_INTERFACE
//! Defines the dividing of peripheral clock for interface
#define BITRATECONTROL (127)
//! Defines the setting of Interface Port Pin Low for Flash Chip Enable
#define FLASH_CE_LOW GpioDataRegs.GPBCLEAR.bit.GPIO50 = 1;
//! Defines the setting of Interface Port Pin High for Flash Chip Enable
#define FLASH_CE_HIGH GpioDataRegs.GPBSET.bit.GPIO50 = 1;
//! Defines the setting of Interface Port Pin Low for MSP430 Enable
#define MSP430_CE_LOW GpioDataRegs.GPACLEAR.bit.GPIO29 = 1;
//! Defines the setting of Interface Port Pin High for MSP430 Enable
#define MSP430_CE_HIGH GpioDataRegs.GPASET.bit.GPIO29 = 1;
//! Define for reading state of the MSP430_CE
#define MSP430_STATE GpioDataRegs.GPADAT.bit.GPIO29
//! Defines SPI Receive Interrupt Disable function
#define SPI_RX_INT_DISABLE SpiaRegs.SPIFFRX.bit.RXFFIENA = 0
//! Defines SPI Receive Interrupt Enable function
#define SPI_RX_INT_ENABLE SpiaRegs.SPIFFRX.bit.RXFFIENA = 1
//! Defines SPI Transmit Interrupt Disable function
#define SPI_TX_INT_DISABLE SpiaRegs.SPIFFTX.bit.TXFFIENA = 0
//! Defines SPI Transmit Interrupt Enable function
#define SPI_TX_INT_ENABLE SpiaRegs.SPIFFTX.bit.TXFFIENA = 1
#endif

//! Definition for Flash Busy Bit in Status Register
#define FLASH_STATUS_BUSY BIT0
//! Definition for Flash Write Enabled Bit in Status Register
#define FLASH_STATUS_WEL BIT1
//! Definition for Flash Block Write Protection Bit 0 in Status Register
#define FLASH_STATUS_BP0 BIT2
//! Definition for Flash Block Write Protection Bit 1 in Status Register
#define FLASH_STATUS_BP1 BIT3
//! Definition for Flash Block Write Protection Bit 2 in Status Register
#define FLASH_STATUS_BP2 BIT4
//! Definition for Flash Block Write Protection Bit 3 in Status Register
#define FLASH_STATUS_BP3 BIT5
//! Definition for Flash Auto Address Increment Programming Bit in Status Register
#define FLASH_STATUS_AAI BIT6
//! Definition for Flash Block Protection Lock Down Bit in Status Register
#define FLASH_STATUS_BPL BIT7
//! Start Address of Data Area in External SPI Flash
#define DATA_ADDRESS_START (0x00002000)
//! Start Address of Configuration Area in External SPI Flash
#define BANK_ADDRESS_START (0x00000000)
//! Number of Bytes per Sector in External SPI Flash (4k Bytes)
#define SECTOR_SIZE (0x00001000)
//! Mask for Sector Math
#define SECTOR_MASK (0x00FFF000)
//! Definition for a Blank Address Read
#define BLANK_MEMORY (0xFFFFFFFF)
//! Define Physical Memory End for the SST25VF016
#define SST25VF016 (0x00200000)
//! Define Physical Memory End for the SST25VF032
#define SST25VF032 (0x00400000)
//! Define Physical Memory End for the SST25VF064
#define SST25VF064 (0x00800000)

typedef enum flashCommand_t
{
	flashNoOperation = (0x00),				   //! Nothing to do but get out of state
	flashWriteStatusRegister = (0x01),		   //! Command to Write the Status Register of the Flash Device
	flashProgramByte = (0x02),				   //! Command to Write a single Byte of Data to the Flash Device
	flashReadByte = (0x03),					   //! Command to Read one Byte of Data from the Flash Device
	flashDisableWriteLatch = (0x04),		   //! Command to Disable the Write Latch of the Flash Device
	flashReadStatusRegister = (0x05),		   //! Command to Read Status Register of Flash Device
	flashEnableWriteLatch = (0x06),			   //! Command to Enable the Write Latch of the Flash Device
	flashHighSpeedRead = (0x0B),			   //! Command to High Speed Read
	flashSectorErase = (0x20),				   //! Command to Erase a 4k byte sector of the Flash Device
	flashEnableWritingStatusRegister = (0x50), //! Command to Enable the Write Status Register of the Flash Device
	flash32kBlockErase = (0x52),			   //! Command to Erase a 32k byte block of the Flash Device
	flashChipErase = (0x60),				   //! Command to Erase the entire Flash Device
	flashEnableBusySO = (0x70),				   //! Command to Enable  SO to indicate Ready/Busy Status during AAI programming of the Flash Device
	flashDisableBusySO = (0x80),			   //! Command to Disable SO to indicate Ready/Busy Status during AAI programming of the Flash Device
	flashReadID = (0x90),					   //! Command to Read Manufacturer or Device ID of the Flash Device (90h or ABh)
	flashReadJedecID = (0x9F),				   //! Command to Read JEDEC ID of the Flash Device
	flashReadIDalternate = (0xAB),			   //! Command to Read Manufacturer or Device ID of the Flash Device (90h or ABh)
	flashAutoAddIncA = (0xAD),				   //! Command to Write 2-bytes in address auto increment address mode of the Flash Device
	flash64kBlockErase = (0xD8),			   //! Command to Erase a 64k byte block of the Flash Device
	flashAutoAddIncB = (0xF0),				   //! Not a real command, but location in case switch statement but calls extension to flashAutoAddIncA
	flashReadContinuous = (0xF1),			   //! Not a real command, but location in case switch statement but calls extension to flashReadByte
	flashHighSpeedReadContinuous = (0xF2),	 //! Not a real command, but location in case switch statement but calls extension to flashHighSpeedRead
	flashEraseNextBlock = (0xF8),			   //! Complex function to Erase Next Block in Data Area of the SPI Flash
	msp430Write = (0xF9),					   //! Complex function to Set the RTC on the MSP430 on the DSP card
	msp430Read = (0xFA),					   //! Complex function to Retrieve data from the MSP430 on the DSP card
	flashEraseNextSector = (0xFB),			   //! Complex function to Erase the Next Sector with One Function Call
	flashReadRecord = (0xFC),				   //! Complex function to Read a Record from Flash with One Function Call
	flashWriteRecord = (0xFD),				   //! Complex function to Write a Record to Flash with One Function Call
	flashThorsHammer = (0xFE),				   //! Complex function to Erase the Entire Flash with One Function Call
	flashBadCommand = (0xFF)				   //! Bad Command place holder to finish the case switch statement
} flashCommand_t;

extern struct flashStructure_S
{
	flashCommand_t currentCommand; //! What function is being invoked with the SPI Flash Device
	ubyte subState;				   //! Character to access various levels within the command process
	ubyte spiStatusRegister;	   //! Character reflecting a copy of the Status Register of the SPI Flash Device
	ubyte eatEcho;				   //! Character to eat echoed characters from SPI transmission
	bool commandComplete;		   //! Boolean indicating process complete. TRUE = Done, FALSE = still processing
	bool devicePresent;			   //! Boolean indicating SPI Flash Device is present and usable in the system
	ubyte *data;				   //! Character pointer to data structure from calling program
	ubyte manufacturerID;		   //! Character representing the Manufacturer's Identification
	ubyte deviceID;				   //! Character representing the Device's Identification
	ubyte memoryType;			   //! Character representing the Memory Type
	ulong jedecID;				   //! Character representing the JEDEC Identification
	ulong address;				   //! Memory address we are accessing within the SPI Flash Device
	uint numberOfBytes;			   //! Integer value indicating the number of bytes to transfer
	ulong Data_Address_End;		   //! End of Data Address Space as we may have different size devices
} flashStructure_S;				   //! Structure definition for external SPI Flash Access

typedef struct flashStructure_S spiStrucT; //! Type define of structure
extern spiStrucT spiFlashData;			   //! Data Structure for Flash Access which is available externally

void init_hal_spi_flash(void);
void flashCommunications(volatile spiStrucT *parseType);

#endif /* HAL_SPI_FLASH_H_ */
