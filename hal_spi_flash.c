/*!	@file		hal_spi_flash.c
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
#include "hal_spi_flash.h"
#include "string.h"

//! Definition of the SPI Buffer Size. Sized to handle 16-bytes
#define SPI_BUFSIZE 16

struct spiDataStrucT
{
	int txReadIndex;			//! Integer value indicating the Transmit Read  Index of the data array
	int txWriteIndex;			//! Integer value indicating the Transmit Write Index of the data array
	int rxReadIndex;			//! Integer value indicating the Receive  Read  Index of the data array
	int rxWriteIndex;			//! Integer value indicating the Receive  Write Index of the data array
	int txCharCount;			//! Integer value indicating the number of characters in the transmit buffer
	int rxCharCount;			//! Integer value indicating the number of characters in the receive  buffer
	int checkCount;				//! Integer value indicating the number of expected characters in transfer
	char txBuffer[SPI_BUFSIZE]; //! Character array of size SPI_BUFSIZE for transmitting data
	char rxBuffer[SPI_BUFSIZE]; //! Character array of size SPI_BUFSIZE for receiving    data
};

struct spiDataStrucT spiPort; // Actual structure for the SPI Port Data
spiStrucT spiFlashData;		  // Actual structure for the Flash Data

//! Prototypes
ubyte spiTransmit(volatile char cByte);
ubyte spiReceive(void);
bool spiTxComplete(void);
bool spiSOready(void);

void flashCommunications(volatile spiStrucT *parseType)
{
	static unsigned int cnt = 0; // Static Integer variable used by some sub-functions
#if defined DSP_INTERFACE
	static int tempData = 0; // Static Integer variable used by some sub-functions on the DSP Board
#endif
	switch (parseType->currentCommand)
	{ // What is our command?
	case flashNoOperation:
		parseType->commandComplete = TRUE; // Finished with the command... setting flag for further processing
		break;
	case flashWriteStatusRegister: // Write byte to the Status Register
		switch (parseType->subState)
		{										   // Switch on subState to process this command
		case 1:									   // Start the command process
			FLASH_CE_LOW;						   // Select the SPI Serial Flash (Output is LOW)
			spiTransmit(flashWriteStatusRegister); // Select write to status register
			spiTransmit(parseType->data[0]);	   // Data that will change the status of BPx or BPL (only bits 2,3,4,5,7 can be written)
			parseType->subState++;				   // Ready for the next state...
			break;								   // Done with subState 1
		case 2:									   // Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{									   // Checking...
				FLASH_CE_HIGH;					   // Deselect the SPI Serial Flash (Output is HIGH)
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->commandComplete = TRUE; // Finished with the command... setting flag for further processing
			}									   // End of SPI Transmit Complete check
			break;								   // Done with subState 2
		}										   // End of subState switch
		break;									   // End of case: flashWriteStatusRegister
	case flashProgramByte:						   // Write a byte to Flash
		switch (parseType->subState)
		{														  // Switch on subState to process this command
		case 1:													  // Start the command process
			FLASH_CE_LOW;										  // Select the SPI Serial Flash (Output is LOW)
			spiTransmit(flashProgramByte);						  // Send the Write Byte command
			spiTransmit(((parseType->address & 0xFFFFFF) >> 16)); // Send the three address bytes for memory modification
			spiTransmit(((parseType->address & 0xFFFF) >> 8));
			spiTransmit((parseType->address & 0xFF));
			spiTransmit(parseType->data[0]); // Send byte to be programmed
			parseType->subState++;			 // Ready for the next state...
			break;							 // Done with subState 1
		case 2:								 // Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{									   // Checking...
				FLASH_CE_HIGH;					   // Deselect the SPI Serial Flash (Output is HIGH)
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->commandComplete = TRUE; // Finished with the command... setting flag for further processing
			}									   // End of SPI Transmit Complete check
			break;								   // Done with subState 2
		}										   // End of substate switch
		break;									   // End of case: flashProgramByte
	case flashReadByte:							   // Read a byte from Flash
		switch (parseType->subState)
		{														  // Switch on subState to process this command
		case 1:													  // Start the command process
			FLASH_CE_LOW;										  // Select the SPI Serial Flash (Output is LOW)
			spiTransmit(flashReadByte);							  // Send the Read command (03h)
			spiTransmit(((parseType->address & 0xFFFFFF) >> 16)); // Send the three address bytes for memory retrieval
			spiTransmit(((parseType->address & 0xFFFF) >> 8));
			spiTransmit((parseType->address & 0xFF));
			spiTransmit(flashBadCommand); // Send a dummy character to get the data back from the Flash Device
			parseType->subState++;		  // Ready for the next state...
			break;						  // Done with subState 1
		case 2:							  // Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{									   // Checking...
				FLASH_CE_HIGH;					   // Deselect the SPI Serial Flash (Output is HIGH)
				parseType->eatEcho = spiReceive(); // Eat the echoed command
				parseType->eatEcho = spiReceive(); // Eat the echoed address byte 1
				parseType->eatEcho = spiReceive(); // Eat the echoed address byte 2
				parseType->eatEcho = spiReceive(); // Eat the echoed address byte 3
				parseType->data[0] = spiReceive(); // Retrieve the echoed data
				parseType->commandComplete = TRUE; // Finished with the command... setting flag for further processing
			}									   // End of SPI Transmit Complete check
			break;								   // Done with subState 2
		}										   // End of substate switch
		break;									   // End of case: flashReadByte
	case flashDisableWriteLatch:				   // Disable the Flash Write Latch
		switch (parseType->subState)
		{										 // Switch on subState to process this command
		case 1:									 // Start the command process
			FLASH_CE_LOW;						 // Select the SPI Serial Flash (Output is LOW)
			spiTransmit(flashDisableWriteLatch); // Send the WRDI command
			parseType->subState++;				 // Ready for the next state...
			break;								 // Done with subState 1
		case 2:									 // Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{									   // Checking...
				FLASH_CE_HIGH;					   // Deselect the SPI Serial Flash (Output is HIGH)
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->commandComplete = TRUE; // Finished with the command... setting flag for further processing
			}									   // End of SPI Transmit Complete check
			break;								   // Done with subState 2
		}										   // End of substate switch
		break;									   // End of case: flashWriteDisableLatch
	case flashReadStatusRegister:				   // Read the Flash Status Register
		switch (parseType->subState)
		{										  // Switch on subState to process this command
		case 1:									  // Start the command process
			FLASH_CE_LOW;						  // Select the SPI Serial Flash (Output is LOW)
			spiTransmit(flashReadStatusRegister); // Send command to read the status register
			spiTransmit(flashBadCommand);		  // Send a dummy character to get the data back from the Flash Device
			parseType->subState++;				  // Ready for the next state...
			break;								  // Done with subState 1
		case 2:									  // Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{												 // Checking...
				FLASH_CE_HIGH;								 // Deselect the SPI Serial Flash (Output is HIGH)
				parseType->eatEcho = spiReceive();			 // Eat the echoed command
				parseType->spiStatusRegister = spiReceive(); // Receive the status byte
				parseType->commandComplete = TRUE;			 // Finished with the command... setting flag for further processing
			}												 // End of SPI Transmit Complete check
			break;											 // Done with subState 2
		}													 // End of substate switch
		break;												 // End of case: flashReadStatusRegister
	case flashEnableWriteLatch:								 // Enable the Flash Write Latch
		switch (parseType->subState)
		{										// Switch on subState to process this command
		case 1:									// Start the command process
			FLASH_CE_LOW;						// Select the SPI Serial Flash (Output is LOW)
			spiTransmit(flashEnableWriteLatch); // Send the WREN command
			parseType->subState++;				// Ready for the next state...
			break;								// Done with subState 1
		case 2:									// Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{									   // Checking...
				FLASH_CE_HIGH;					   // Deselect the SPI Serial Flash (Output is HIGH)
				parseType->eatEcho = spiReceive(); // Eat the echoed command
				parseType->commandComplete = TRUE; // Finished with the command... setting flag for further processing
			}									   // End of SPI Transmit Complete check
			break;								   // Done with subState 2
		}										   // End of substate switch
		break;									   // End of case: flashWriteDisableLatch
	case flashHighSpeedRead:					   // High Speed Read of the Flash
		switch (parseType->subState)
		{														  // Switch on subState to process this command
		case 1:													  // Start the command process
			FLASH_CE_LOW;										  // Select the SPI Serial Flash (Output is LOW)
			spiTransmit(flashHighSpeedRead);					  // Send the High Speed Read command
			spiTransmit(((parseType->address & 0xFFFFFF) >> 16)); // Send the three address bytes for memory retrieval
			spiTransmit(((parseType->address & 0xFFFF) >> 8));
			spiTransmit((parseType->address & 0xFF));
			spiTransmit(flashBadCommand); // Send a dummy character to the Flash Device
			spiTransmit(flashBadCommand); // Send a dummy character to get the data back from the Flash Device
			parseType->subState++;		  // Ready for the next state...
			break;						  // Done with subState 1
		case 2:							  // Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{									   // Checking...
				FLASH_CE_HIGH;					   // Deselect the SPI Serial Flash (Output is HIGH)
				parseType->eatEcho = spiReceive(); // Eat the echoed command
				parseType->eatEcho = spiReceive(); // Eat the echoed address byte 1
				parseType->eatEcho = spiReceive(); // Eat the echoed address byte 2
				parseType->eatEcho = spiReceive(); // Eat the echoed address byte 3
				parseType->eatEcho = spiReceive(); // Eat the echoed dummy
				parseType->data[0] = spiReceive(); // Receive requested byte
				parseType->commandComplete = TRUE; // Finished with the command... setting flag for further processing
			}									   // End of SPI Transmit Complete check
			break;								   // Done with subState 2
		}										   // End of substate switch
		break;									   // End of case: flashHighSpeedRead
	case flashSectorErase:						   // Erase a 4k Sector in the Flash Device
		switch (parseType->subState)
		{														  // Switch on subState to process this command
		case 1:													  // Start the command process
			FLASH_CE_LOW;										  // Select the SPI Serial Flash (Output is LOW)
			spiTransmit(flashSectorErase);						  // Send the Sector Erase command
			spiTransmit(((parseType->address & 0xFFFFFF) >> 16)); // Send the three address bytes for memory modification
			spiTransmit(((parseType->address & 0xFFFF) >> 8));
			spiTransmit((parseType->address & 0xFF));
			parseType->subState++; // Ready for the next state...
			break;				   // Done with subState 1
		case 2:					   // Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{									   // Checking...
				FLASH_CE_HIGH;					   // Deselect the SPI Serial Flash (Output is HIGH)
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->commandComplete = TRUE; // Finished with the command... setting flag for further processing
			}									   // End of SPI Transmit Complete check
			break;								   // Done with subState 2
		}										   // End of substate switch
		break;									   // End of case: flashSectorErase
	case flashEnableWritingStatusRegister:		   // Enable the Flash to allow writing to the Status Register
		switch (parseType->subState)
		{												   // Switch on subState to process this command
		case 1:											   // Start the command process
			FLASH_CE_LOW;								   // Select the SPI Serial Flash (Output is LOW)
			spiTransmit(flashEnableWritingStatusRegister); // Send command to enable writing to the status register
			parseType->subState++;						   // Ready for the next state...
			break;										   // Done with subState 1
		case 2:											   // Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{									   // Checking...
				FLASH_CE_HIGH;					   // Deselect the SPI Serial Flash (Output is HIGH)
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->commandComplete = TRUE; // Finished with the command... setting flag for further processing
			}									   // End of SPI Transmit Complete check
			break;								   // Done with subState 2
		}										   // End of substate switch
		break;									   // End of case: flashWriteDisableLatch
	case flash32kBlockErase:					   // Erase a 32k Block of memory in the Flash
		switch (parseType->subState)
		{														  // Switch on subState to process this command
		case 1:													  // Start the command process
			FLASH_CE_LOW;										  // Select the SPI Serial Flash (Output is LOW)
			spiTransmit(flash32kBlockErase);					  // Send the 32k Block Erase command
			spiTransmit(((parseType->address & 0xFFFFFF) >> 16)); // Send the three address bytes for memory modification
			spiTransmit(((parseType->address & 0xFFFF) >> 8));
			spiTransmit((parseType->address & 0xFF));
			parseType->subState++; // Ready for the next state...
			break;				   // Done with subState
		case 2:					   // Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{									   // Checking...
				FLASH_CE_HIGH;					   // Deselect the SPI Serial Flash (Output is HIGH)
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->commandComplete = TRUE; // Finished with the command... setting flag for further processing
			}									   // End of SPI Transmit Complete check
			break;								   // Done with subState 2
		}										   // End of substate switch
		break;									   // End of case: flash32kBlockErase
	case flashChipErase:						   // Erase the Entire Flash Device
		switch (parseType->subState)
		{								 // Switch on subState to process this command
		case 1:							 // Start the command process
			FLASH_CE_LOW;				 // Select the SPI Serial Flash (Output is LOW)
			spiTransmit(flashChipErase); // Send the Chip Erase command (60h or C7h)
			parseType->subState++;		 // Ready for the next state...
			break;						 // Done with subState 1
		case 2:							 // Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{									   // Checking...
				FLASH_CE_HIGH;					   // Deselect the SPI Serial Flash (Output is HIGH)
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->commandComplete = TRUE; // Finished with the command... setting flag for further processing
			}									   // End of SPI Transmit Complete check
			break;								   // Done with subState 2
		}										   // End of substate switch
		break;									   // End of case: flashWriteDisableLatch
	case flashEnableBusySO:						   // Enable output Busy Signal on the SO pin of the Flash Device
		switch (parseType->subState)
		{									// Switch on subState to process this command
		case 1:								// Start the command process
			FLASH_CE_LOW;					// Select the SPI Serial Flash (Output is LOW)
			spiTransmit(flashEnableBusySO); // Send the EBSY command
			parseType->subState++;			// Ready for the next state...
			break;							// Done with subState 1
		case 2:								// Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{									   // Checking...
				FLASH_CE_HIGH;					   // Deselect the SPI Serial Flash (Output is HIGH)
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->commandComplete = TRUE; // Finished with the command... setting flag for further processing
			}									   // End of SPI Transmit Complete check
			break;								   // Done with subState 2
		}										   // End of substate switch
		break;									   // End of case: flashEnableBusySO
	case flashDisableBusySO:					   // Disable output Busy Signal on the SO pin of the Flash Device
		switch (parseType->subState)
		{									 // Switch on subState to process this command
		case 1:								 // Start the command process
			FLASH_CE_LOW;					 // Select the SPI Serial Flash (Output is LOW)
			spiTransmit(flashDisableBusySO); // Send the DBSY command
			parseType->subState++;			 // Ready for the next state...
			break;							 // Done with subState 1
		case 2:								 // Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{									   // Checking...
				FLASH_CE_HIGH;					   // Deselect the SPI Serial Flash (Output is HIGH)
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->commandComplete = TRUE; // Finished with the command... setting flag for further processing
			}									   // End of SPI Transmit Complete check
			break;								   // Done with subState 2
		}										   // End of substate switch
		break;									   // End of case: flashDisableBusySO
	case flashReadID:							   // Read the Flash ID contents
	case flashReadIDalternate:
		switch (parseType->subState)
		{											// Switch on subState to process this command
		case 1:										// Start the command process
			FLASH_CE_LOW;							// Select the SPI Serial Flash (Output is LOW)
			spiTransmit(parseType->currentCommand); // Send the Read ID command (90h or ABh)
			spiTransmit(0x00);						// Send the address
			spiTransmit(0x00);						// Send the address
			spiTransmit(0x00);						// Send the address - either 00 Hex or 01 Hex
			spiTransmit(flashBadCommand);			// Send a dummy character to get the data back from the Flash Device
			parseType->subState++;					// Ready for the next state...
			break;									// Done with subState 1
		case 2:										// Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{											  // Checking...
				FLASH_CE_HIGH;							  // Deselect the SPI Serial Flash (Output is HIGH)
				parseType->eatEcho = spiReceive();		  // Eat the echoed command
				parseType->eatEcho = spiReceive();		  // Eat the echoed address byte 1
				parseType->eatEcho = spiReceive();		  // Eat the echoed address byte 2
				parseType->eatEcho = spiReceive();		  // Eat the echoed address byte 3
				parseType->manufacturerID = spiReceive(); // Get the data from the Flash
				parseType->subState++;					  // All clear... ready for the next state
			}											  // End of SPI Transmit Complete check
			break;										  // Done with subState 2
		case 3:											  // Eat the echos and end the command process
			FLASH_CE_LOW;								  // Select the SPI Serial Flash (Output is LOW)
			spiTransmit(flashReadID);					  // Send the Read ID command (90h or ABh)
			spiTransmit(0x00);							  // Send the address
			spiTransmit(0x00);							  // Send the address
			spiTransmit(0x01);							  // Send the address - either 00 Hex or 01 Hex
			spiTransmit(flashBadCommand);				  // Send a dummy character to get the data back from the Flash Device
			parseType->subState++;						  // Ready for the next state...
			break;										  // End of subState 3
		case 4:											  // Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{										// Checking...
				FLASH_CE_HIGH;						// Deselect the SPI Serial Flash (Output is HIGH)
				parseType->eatEcho = spiReceive();  // Eat the echoed command
				parseType->eatEcho = spiReceive();  // Eat the echoed address byte 1
				parseType->eatEcho = spiReceive();  // Eat the echoed address byte 2
				parseType->eatEcho = spiReceive();  // Eat the echoed address byte 3
				parseType->deviceID = spiReceive(); // Get the data from the Flash
				parseType->commandComplete = TRUE;  // Finished with the command... setting flag for further processing
			}										// End of SPI Transmit Complete check
			break;									// End of subState 4
		}											// End of substate switch
		break;										// End of case: flashReadID
	case flashReadJedecID:							// Read the Jedec ID contents
		switch (parseType->subState)
		{								   // Switch on subState to process this command
		case 1:							   // Start the command process
			FLASH_CE_LOW;				   // Select the SPI Serial Flash (Output is LOW)
			spiTransmit(flashReadJedecID); // Send the JEDEC ID command (9Fh)
			spiTransmit(flashBadCommand);  // Send a dummy character to get the data back from the Flash Device
			spiTransmit(flashBadCommand);  // Send a dummy character to get the data back from the Flash Device
			spiTransmit(flashBadCommand);  // Send a dummy character to get the data back from the Flash Device
			parseType->subState++;		   // Ready for the next state...
			break;						   // End of subState 1
		case 2:							   // Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{																				// Checking...
				FLASH_CE_HIGH;																// Deselect the SPI Serial Flash (Output is HIGH)
				parseType->eatEcho = spiReceive();											// Eat the echoed command
				parseType->manufacturerID = spiReceive();									// Receive byte for Manufacturer's ID
				parseType->jedecID = (parseType->jedecID | parseType->manufacturerID) << 8; // Build Jedec ID
				parseType->memoryType = spiReceive();										// Receive byte for Memory Type
				parseType->jedecID = (parseType->jedecID | parseType->memoryType) << 8;		// Build onto Jedec ID
				parseType->deviceID = spiReceive();											// Receive byte for Device ID
				parseType->jedecID = (parseType->jedecID | parseType->deviceID);			// Build onto Jedec, the value should be equal to 0xBF254A
				parseType->commandComplete = TRUE;											// Finished with the command... setting flag for further processing
			}																				// End of SPI Transmit Complete check
			break;																			// End of subState 2
		}																					// End of substate switch
		break;																				// End of case: flashReadJedecID
	case flashAutoAddIncA:																	// Start Auto Add Increment Word Write command
		switch (parseType->subState)
		{														  // Switch on subState to process this command
		case 1:													  // Start the command process
			FLASH_CE_LOW;										  // Select the SPI Serial Flash (Output is LOW)
			spiTransmit(flashAutoAddIncA);						  // Send the AAI command
			spiTransmit(((parseType->address & 0xFFFFFF) >> 16)); // Send the three address bytes for memory modification
			spiTransmit(((parseType->address & 0xFFFF) >> 8));
			spiTransmit((parseType->address & 0xFF));
			spiTransmit(parseType->data[0]); // Send first  byte to be programmed
			spiTransmit(parseType->data[1]); // Send second byte to be programmed
			parseType->subState++;			 // Ready for the next state...
			break;							 // Done with subState 1
		case 2:								 // Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{									   // Checking...
				FLASH_CE_HIGH;					   // Deselect the SPI Serial Flash (Output is HIGH)
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->commandComplete = TRUE; // Finished with the command... setting flag for further processing
			}									   // End of SPI Transmit Complete check
			break;								   // End of subState 2
		}										   // End of substate switch
		break;									   // End of case: flashAutoAddIncA
	case flash64kBlockErase:					   // Erase a 64k Block of memory in the Flash
		switch (parseType->subState)
		{														  // Switch on subState to process this command
		case 1:													  // Start the command process
			FLASH_CE_LOW;										  // Select the SPI Serial Flash (Output is LOW)
			spiTransmit(flash64kBlockErase);					  // Send the 64k Block Erase command
			spiTransmit(((parseType->address & 0xFFFFFF) >> 16)); // Send the three address bytes for memory modification
			spiTransmit(((parseType->address & 0xFFFF) >> 8));
			spiTransmit((parseType->address & 0xFF));
			parseType->subState++; // Ready for the next state...
			break;				   // Done with subState 1
		case 2:					   // Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{									   // Checking...
				FLASH_CE_HIGH;					   // Deselect the SPI Serial Flash (Output is HIGH)
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->commandComplete = TRUE; // Finished with the command... setting flag for further processing
			}									   // End of SPI Transmit Complete check
			break;								   // Done with subState 2
		}										   // End of substate switch
		break;									   // End of case: flash64kBlockErase
	case flashAutoAddIncB:						   // Continue with  Auto Add Increment Word Write command
		switch (parseType->subState)
		{									 // Switch on subState to process this command
		case 1:								 // Start the command process
			FLASH_CE_LOW;					 // Select the SPI Serial Flash (Output is LOW)
			spiTransmit(flashAutoAddIncA);   // Send the AAI command
			spiTransmit(parseType->data[0]); // Send first  byte to be programmed
			spiTransmit(parseType->data[1]); // Send second byte to be programmed
			parseType->subState++;			 // Ready for the next state...
			break;							 // Done with subState 1
		case 2:								 // Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{									   // Checking...
				FLASH_CE_HIGH;					   // Deselect the SPI Serial Flash (Output is HIGH)
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->commandComplete = TRUE; // Finished with the command... setting flag for further processing
			}									   // End of SPI Transmit Complete check
			break;								   // Done with subState 2
		}										   // End of substate switch
		break;									   // End of case: flashAutoAddIncB
	case flashReadContinuous:					   // Perform a continuous read from the Flash Device
		switch (parseType->subState)
		{														  // Switch on subState to process this command
		case 1:													  // Start the command process
			FLASH_CE_LOW;										  // Select the SPI Serial Flash (Output is LOW)
			spiTransmit(flashReadByte);							  // Send the Read command (03h)
			spiTransmit(((parseType->address & 0xFFFFFF) >> 16)); // Send the three address bytes for memory retrieval
			spiTransmit(((parseType->address & 0xFFFF) >> 8));
			spiTransmit((parseType->address & 0xFF));
			parseType->subState++; // Ready for the next state...
			break;				   // Done with subState 1
		case 2:					   // Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{									   // Checking...
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
				cnt = 0;						   // Preset the counter/pointer marker
				parseType->subState++;			   // All clear... ready for the next state
			}									   // End of SPI Transmit Complete check
			break;								   // Done with subState 2
		case 3:									   // Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{						   // Checking...
				parseType->subState++; // Go for it
			}						   // End of SPI Transmit Complete check
			break;					   // Done with subState 3
		case 4:						   // Transfer the Data
			if (cnt < parseType->numberOfBytes)
			{ // Is there more to do?
				if (spiPort.rxCharCount)
				{										   // Any more characters in the buffer?
					parseType->data[cnt++] = spiReceive(); // Receive byte and store it into the selected memory while incrementing counter/pointer
				}										   // End of receive buffer check
				spiTransmit(flashBadCommand);			   // Send a dummy character to get the data back from the Flash Device
				parseType->subState--;					   // Go back and wait for transmission to finish
			}
			else
			{									   // Else when have what we came for...
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->subState++;			   // Ready for the next state...
			}									   // End of number of bytes to transfer check
			break;								   // Done with subState 4
		case 5:									   // Clean up time
			if (spiTxComplete())
			{									   // Checking...
				FLASH_CE_HIGH;					   // Deselect the SPI Serial Flash (Output is HIGH)
				parseType->commandComplete = TRUE; // Finished with the command... setting flag for further processing
			}									   // End of SPI Transmit Complete check
			break;								   // Done with subState 5
		}										   // End of substate switch
		break;									   // End of case: flashReadContinious
	case flashHighSpeedReadContinuous:			   // Perform a continuous high speed read from the Flash Device
		switch (parseType->subState)
		{														  // Switch on subState to process this command
		case 1:													  // Start the command process
			FLASH_CE_LOW;										  // Select the SPI Serial Flash (Output is LOW)
			spiTransmit(flashHighSpeedRead);					  // Send the Read command (03h)
			spiTransmit(((parseType->address & 0xFFFFFF) >> 16)); // Send the three address bytes for memory retrieval
			spiTransmit(((parseType->address & 0xFFFF) >> 8));
			spiTransmit((parseType->address & 0xFF));
			spiTransmit(flashBadCommand); // Send dummy byte
			parseType->subState++;		  // Ready for the next state...
			break;						  // Done with subState 1
		case 2:							  // Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{									   // Checking...
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
				cnt = 0;						   // Preset the counter/pointer marker
				parseType->subState++;			   // All clear... ready for the next state
			}									   // End of SPI Transmit Complete check
			break;								   // Done with subState 2
		case 3:									   // Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{						   // Checking...
				parseType->subState++; // Go for it
			}						   // End of SPI Transmit Complete check
			break;					   // Done with subState 3
		case 4:						   // Transfer the Data
			if (cnt < parseType->numberOfBytes)
			{ // Is there more to do?
				if (spiPort.rxCharCount)
				{										   // Any more characters in the buffer?
					parseType->data[cnt++] = spiReceive(); // Receive byte and store it into the selected memory while incrementing counter/pointer
				}										   // End of receive buffer check
				spiTransmit(flashBadCommand);			   // Send a dummy character to get the data back from the Flash Device
				parseType->subState--;					   // Go back and wait for transmission to finish
			}
			else
			{									   // Else when have what we came for...
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->subState++;			   // Ready for the next state...
			}									   // End of number of bytes to transfer check
			break;								   // Done with subState 4
		case 5:									   // Clean up time
			if (spiTxComplete())
			{									   // Checking...
				FLASH_CE_HIGH;					   // Deselect the SPI Serial Flash (Output is HIGH)
				parseType->commandComplete = TRUE; // Finished with the command... setting flag for further processing
			}									   // End of SPI Transmit Complete check
			break;								   // Done with subState 5
		}										   // End of substate switch
		break;									   // End of case: flashHighSpeedReadContinuous
	case flashEraseNextSector:
		switch (parseType->subState)
		{												   // Switch on subState to process this command
		case 1:											   // Start the command process
			FLASH_CE_LOW;								   // Select the SPI Serial Flash (Output is LOW)
			spiTransmit(flashEnableWritingStatusRegister); // Send command to enable writing to the status register
			parseType->subState++;						   // Ready for the next state...
			break;										   // Done with subState 1
		case 2:											   // Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{									   // Checking...
				FLASH_CE_HIGH;					   // Deselect the SPI Serial Flash (Output is HIGH)
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->subState++;			   // Ready for the next state...
			}									   // End of SPI Transmit Complete check
			break;								   // Done with subState 2
		case 3:									   // Start the command process
			FLASH_CE_LOW;						   // Select the SPI Serial Flash (Output is LOW)
			spiTransmit(flashWriteStatusRegister); // Select write to status register
			spiTransmit(0x00);					   // This will unlock all write protects
			parseType->subState++;				   // Ready for the next state...
			break;								   // Done with subState 3
		case 4:									   // Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{									   // Checking...
				FLASH_CE_HIGH;					   // Deselect the SPI Serial Flash (Output is HIGH)
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->subState++;			   // Ready for the next state...
			}									   // End of SPI Transmit Complete check
			break;								   // Done with subState 4
		case 5:									   // Start the command process
			FLASH_CE_LOW;						   // Select the SPI Serial Flash (Output is LOW)
			spiTransmit(flashEnableWriteLatch);	// Send the WREN command
			parseType->subState++;				   // Ready for the next state...
			break;								   // Done with subState 5
		case 6:									   // Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{													  // Checking...
				FLASH_CE_HIGH;									  // Deselect the SPI Serial Flash (Output is HIGH)
				parseType->eatEcho = spiReceive();				  // Eat the echoes
				parseType->subState++;							  // Ready for the next state...
			}													  // End of SPI Transmit Complete check
			break;												  // Done with subState 6
		case 7:													  // Start the command process
			FLASH_CE_LOW;										  // Select the SPI Serial Flash (Output is LOW)
			spiTransmit(flashSectorErase);						  // Send the Sector Erase command
			spiTransmit(((parseType->address & 0xFFFFFF) >> 16)); // Send the three address bytes for memory modification
			spiTransmit(((parseType->address & 0xFFFF) >> 8));
			spiTransmit((parseType->address & 0xFF));
			parseType->subState++; // Ready for the next state...
			break;				   // Done with subState 7
		case 8:					   // Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{									   // Checking...
				FLASH_CE_HIGH;					   // Deselect the SPI Serial Flash (Output is HIGH)
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->subState++;			   // Ready for the next state...
			}									   // End of SPI Transmit Complete check
			break;								   // Done with subState 8
		case 9:									   // Start the command process
			FLASH_CE_LOW;						   // Select the SPI Serial Flash (Output is LOW)
			spiTransmit(flashReadStatusRegister);  // Send command to read the status register
			spiTransmit(flashBadCommand);		   // Send a dummy character to get the data back from the Flash Device
			parseType->subState++;				   // Ready for the next state...
			break;								   // Done with subState 9
		case 10:								   // Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{												 // Checking...
				FLASH_CE_HIGH;								 // Deselect the SPI Serial Flash (Output is HIGH)
				parseType->eatEcho = spiReceive();			 // Eat the echoed command
				parseType->spiStatusRegister = spiReceive(); // Receive the status byte
				if (parseType->spiStatusRegister & FLASH_STATUS_BUSY)
				{
					parseType->subState--;
					break;
				}
				parseType->commandComplete = TRUE; // Finished with the command... setting flag for further processing
			}									   // End of SPI Transmit Complete check
			break;								   // Done with subState 10
		}										   // End of substate switch
		break;									   // End of case: flashEraseNextSector
	case flashEraseNextBlock:
		switch (parseType->subState)
		{												   // Switch on subState to process this command
		case 1:											   // Start the command process
			FLASH_CE_LOW;								   // Select the SPI Serial Flash (Output is LOW)
			spiTransmit(flashEnableWritingStatusRegister); // Send command to enable writing to the status register
			parseType->subState++;						   // Ready for the next state...
			break;										   // Done with subState 1
		case 2:											   // Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{									   // Checking...
				FLASH_CE_HIGH;					   // Deselect the SPI Serial Flash (Output is HIGH)
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->subState++;			   // Ready for the next state...
			}									   // End of SPI Transmit Complete check
			break;								   // Done with subState 2
		case 3:									   // Start the command process
			FLASH_CE_LOW;						   // Select the SPI Serial Flash (Output is LOW)
			spiTransmit(flashWriteStatusRegister); // Select write to status register
			spiTransmit(0x00);					   // This will unlock all write protects
			parseType->subState++;				   // Ready for the next state...
			break;								   // Done with subState 3
		case 4:									   // Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{									   // Checking...
				FLASH_CE_HIGH;					   // Deselect the SPI Serial Flash (Output is HIGH)
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->subState++;			   // Ready for the next state...
			}									   // End of SPI Transmit Complete check
			break;								   // Done with subState 4
		case 5:									   // Start the command process
			FLASH_CE_LOW;						   // Select the SPI Serial Flash (Output is LOW)
			spiTransmit(flashEnableWriteLatch);	// Send the WREN command
			parseType->subState++;				   // Ready for the next state...
			break;								   // Done with subState 5
		case 6:									   // Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{													  // Checking...
				FLASH_CE_HIGH;									  // Deselect the SPI Serial Flash (Output is HIGH)
				parseType->eatEcho = spiReceive();				  // Eat the echoes
				parseType->subState++;							  // Ready for the next state...
			}													  // End of SPI Transmit Complete check
			break;												  // Done with subState 6
		case 7:													  // Start the command process
			FLASH_CE_LOW;										  // Select the SPI Serial Flash (Output is LOW)
			spiTransmit(flash32kBlockErase);					  // Send the 32K Block Erase command
			spiTransmit(((parseType->address & 0xFFFFFF) >> 16)); // Send the three address bytes for memory modification
			spiTransmit(((parseType->address & 0xFFFF) >> 8));
			spiTransmit((parseType->address & 0xFF));
			parseType->subState++; // Ready for the next state...
			break;				   // Done with subState 7
		case 8:					   // Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{									   // Checking...
				FLASH_CE_HIGH;					   // Deselect the SPI Serial Flash (Output is HIGH)
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->subState++;			   // Ready for the next state...
			}									   // End of SPI Transmit Complete check
			break;								   // Done with subState 8
		case 9:									   // Start the command process
			FLASH_CE_LOW;						   // Select the SPI Serial Flash (Output is LOW)
			spiTransmit(flashReadStatusRegister);  // Send command to read the status register
			spiTransmit(flashBadCommand);		   // Send a dummy character to get the data back from the Flash Device
			parseType->subState++;				   // Ready for the next state...
			break;								   // Done with subState 9
		case 10:								   // Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{												 // Checking...
				FLASH_CE_HIGH;								 // Deselect the SPI Serial Flash (Output is HIGH)
				parseType->eatEcho = spiReceive();			 // Eat the echoed command
				parseType->spiStatusRegister = spiReceive(); // Receive the status byte
				if (parseType->spiStatusRegister & FLASH_STATUS_BUSY)
				{
					parseType->subState--;
					break;
				}
				parseType->commandComplete = TRUE; // Finished with the command... setting flag for further processing
			}									   // End of SPI Transmit Complete check
			break;								   // Done with subState 10
		}										   // End of substate switch
		break;									   // End of case: flashEraseNextSector
	case flashReadRecord:
		switch (parseType->subState)
		{														  // Switch on subState to process this command
		case 1:													  // Start the command process
			FLASH_CE_LOW;										  // Select the SPI Serial Flash (Output is LOW)
			spiTransmit(flashReadByte);							  // Send the Read command (03h)
			spiTransmit(((parseType->address & 0xFFFFFF) >> 16)); // Send the three address bytes for memory retrieval
			spiTransmit(((parseType->address & 0xFFFF) >> 8));
			spiTransmit((parseType->address & 0xFF));
			parseType->subState++; // Ready for the next state...
			break;				   // Done with subState 1
		case 2:					   // Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{									   // Checking...
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
				cnt = 0;						   // Preset the counter/pointer marker
				parseType->subState++;			   // All clear... ready for the next state
				parseType->subState++;
			}	  // End of SPI Transmit Complete check
			break; // Done with subState 2
		case 3:	// Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{						   // Checking...
				parseType->subState++; // Go for it
			}						   // End of SPI Transmit Complete check
			break;					   // Done with subState 3
		case 4:						   // Transfer the Data
			if (cnt < parseType->numberOfBytes)
			{ // Is there more to do?
				if (spiPort.rxCharCount)
				{ // Any more characters in the buffer?
#if defined MSP430_INTERFACE
					*parseType->data++ = spiReceive(); // Receive byte and store it into the selected memory while incrementing pointer up one byte in memory
#endif
#if defined DSP_INTERFACE
					if (((cnt % 2) == 0) || (cnt == 0))
					{							 // If count is zero or evenly divisible by two then...
						tempData = spiReceive(); // Get the byte
						tempData <<= 8;			 // Shift it to the upper section
						tempData &= 0xFF00;		 // Clear out the garbage from the lower section
					}
					else
					{								   // Otherwise it is an odd one
						tempData += spiReceive();	  // Put the byte in the lower half of the integer
						*parseType->data++ = tempData; // Pass it back to the odd structure on the DSP
						tempData = 0;				   // Clear it out for the next go
					}
#endif
					cnt++;					  // Increment the counter for bytes received
				}							  // End of receive buffer check
				spiTransmit(flashBadCommand); // Send a dummy character to get the data back from the Flash Device
				parseType->subState--;		  // Go back and wait for transmission to finish
			}
			else
			{									   // Else when have what we came for...
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->subState++;			   // Ready for the next state...
			}									   // End of number of bytes to transfer check
			break;								   // Done with subState 4
		case 5:									   // Clean up time
			if (spiTxComplete())
			{									   // Checking...
				FLASH_CE_HIGH;					   // Deselect the SPI Serial Flash (Output is HIGH)
				parseType->commandComplete = TRUE; // Finished with the command... setting flag for further processing
			}									   // End of SPI Transmit Complete check
			break;								   // Done with subState 5
		}										   // End of substate switch
		break;									   // End of case: flashReadRecord
	case flashWriteRecord:
		switch (parseType->subState)
		{												   // Switch on subState to process this command
		case 1:											   // Start the command process
			FLASH_CE_LOW;								   // Select the SPI Serial Flash (Output is LOW)
			spiTransmit(flashEnableWritingStatusRegister); // Send command to enable writing to the status register
			parseType->subState++;						   // Ready for the next state...
			break;										   // Done with subState 1
		case 2:											   // Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{									   // Checking...
				FLASH_CE_HIGH;					   // Deselect the SPI Serial Flash (Output is HIGH)
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->subState++;			   // Ready for the next state...
			}									   // End of SPI Transmit Complete check
			break;								   // Done with subState 2
		case 3:									   // Start the command process
			FLASH_CE_LOW;						   // Select the SPI Serial Flash (Output is LOW)
			spiTransmit(flashWriteStatusRegister); // Select write to status register
			spiTransmit(0x00);					   // This will unlock all write protects
			parseType->subState++;				   // Ready for the next state...
			break;								   // Done with subState 3
		case 4:									   // Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{									   // Checking...
				FLASH_CE_HIGH;					   // Deselect the SPI Serial Flash (Output is HIGH)
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->subState++;			   // Ready for the next state...
			}									   // End of SPI Transmit Complete check
			break;								   // Done with subState 4
		case 5:									   // Start the command process
			FLASH_CE_LOW;						   // Select the SPI Serial Flash (Output is LOW)
			spiTransmit(flashEnableBusySO);		   // Send the EBSY command
			parseType->subState++;				   // Ready for the next state...
			break;								   // Done with subState 5
		case 6:									   // Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{									   // Checking...
				FLASH_CE_HIGH;					   // Deselect the SPI Serial Flash (Output is HIGH)
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->subState++;			   // Ready for the next state...
			}									   // End of SPI Transmit Complete check
			break;								   // Done with subState 6
		case 7:									   // Start the command process
			FLASH_CE_LOW;						   // Select the SPI Serial Flash (Output is LOW)
			spiTransmit(flashEnableWriteLatch);	// Send the WREN command
			parseType->subState++;				   // Ready for the next state...
			break;								   // Done with subState 7
		case 8:									   // Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{													  // Checking...
				FLASH_CE_HIGH;									  // Deselect the SPI Serial Flash (Output is HIGH)
				parseType->eatEcho = spiReceive();				  // Eat the echoes
				cnt = 0;										  // Preset the counter/pointer marker
				parseType->subState++;							  // Ready for the next state...
			}													  // End of SPI Transmit Complete check
			break;												  // Done with subState 8
		case 9:													  // Start the command process
			FLASH_CE_LOW;										  // Select the SPI Serial Flash (Output is LOW)
			spiTransmit(flashAutoAddIncA);						  // Send the AAI command
			spiTransmit(((parseType->address & 0xFFFFFF) >> 16)); // Send the three address bytes for memory modification
			spiTransmit(((parseType->address & 0xFFFF) >> 8));
			spiTransmit((parseType->address & 0xFF));
#if defined DSP_INTERFACE
			tempData = *parseType->data++;
			spiTransmit(((tempData & 0xFFFF) >> 8));
			spiTransmit((tempData & 0xFF));
			cnt++;
			cnt++;
#endif
#if defined MSP430_INTERFACE
			spiTransmit(parseType->data[cnt++]); // Send first  byte to be programmed and increment pointer
			spiTransmit(parseType->data[cnt++]); // Send second byte to be programmed and increment pointer
#endif
			parseType->subState++; // Ready for the next state...
			break;				   // Done with subState 9
		case 10:				   // Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{									   // Checking...
				FLASH_CE_HIGH;					   // Deselect the SPI Serial Flash (Output is HIGH)
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->subState++;			   // Ready for the next state...
				parseType->subState++;			   // Ready for the next state...
			}									   // End of SPI Transmit Complete check
			break;								   // Done with subState 10
		case 11:
			if (spiTxComplete())
			{									   // Checking...
				FLASH_CE_HIGH;					   // Deselect the SPI Serial Flash (Output is HIGH)
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->subState++;			   // SPI is ready, go to next state
			}									   // End of SPI Transmit Complete check
			break;								   // Done with subState 11
		case 12:								   // Start the command process
			if (spiSOready())
			{						   // Has the SPI finished the write?
				parseType->subState++; // SPI is ready, go to next state
			}						   // End of Ready/Busy check
			break;					   // Done with subState 12
		case 13:					   // Start the command process
			if (cnt < parseType->numberOfBytes)
			{								   // Have we sent all of the data?
				FLASH_CE_LOW;				   // Select the SPI Serial Flash (Output is LOW)
				spiTransmit(flashAutoAddIncA); // Send the AAI command
#if defined DSP_INTERFACE
				tempData = *parseType->data++;
				spiTransmit(((tempData & 0xFFFF) >> 8));
				spiTransmit((tempData & 0xFF));
				cnt++;
				cnt++;
#endif
#if defined MSP430_INTERFACE
				spiTransmit(parseType->data[cnt++]); // Send first  byte to be programmed and increment pointer
				spiTransmit(parseType->data[cnt++]); // Send second byte to be programmed and increment pointer
#endif
				parseType->subState--; // We need to go back two states for the loop
				parseType->subState--;
			}
			else
			{
				parseType->subState++; // Ready for the next state...
			}
			break; // Done with subState 13
		case 14:   // Start the command process
			if (spiSOready())
			{									 // Has the SPI finished the write?
				parseType->subState++;			 // Ready for the next state...
			}									 // End of Ready/Busy check
			break;								 // Done with subState 14
		case 15:								 // Start the command process
			FLASH_CE_LOW;						 // Select the SPI Serial Flash (Output is LOW)
			spiTransmit(flashDisableWriteLatch); // Send the WRDI command
			parseType->subState++;				 // Ready for the next state...
			break;								 // Done with subState 15
		case 16:								 // Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{									   // Checking...
				FLASH_CE_HIGH;					   // Deselect the SPI Serial Flash (Output is HIGH)
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->subState++;			   // Ready for the next state...
			}									   // End of SPI Transmit Complete check
			break;								   // Done with subState 16
		case 17:								   // Start the command process
			FLASH_CE_LOW;						   // Select the SPI Serial Flash (Output is LOW)
			spiTransmit(flashDisableBusySO);	   // Send the DBSY command
			parseType->subState++;				   // Ready for the next state...
			break;								   // Done with subState 17
		case 18:								   // Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{									   // Checking...
				FLASH_CE_HIGH;					   // Deselect the SPI Serial Flash (Output is HIGH)
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->commandComplete = TRUE; // Finished with the command... setting flag for further processing
			}									   // End of SPI Transmit Complete check
			break;								   // Done with subState 18
		}										   // End of substate switch
		break;									   // End of case: flashWriteRecord
	case flashThorsHammer:
		switch (parseType->subState)
		{												   // Switch on subState to process this command
		case 1:											   // Start the command process
			FLASH_CE_LOW;								   // Select the SPI Serial Flash (Output is LOW)
			spiTransmit(flashEnableWritingStatusRegister); // Send command to enable writing to the status register
			parseType->subState++;						   // Ready for the next state...
			break;										   // Done with subState 1
		case 2:											   // Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{									   // Checking...
				FLASH_CE_HIGH;					   // Deselect the SPI Serial Flash (Output is HIGH)
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->subState++;			   // Ready for the next state...
			}									   // End of SPI Transmit Complete check
			break;								   // Done with subState 2
		case 3:									   // Start the command process
			FLASH_CE_LOW;						   // Select the SPI Serial Flash (Output is LOW)
			spiTransmit(flashWriteStatusRegister); // Select write to status register
			spiTransmit(0x00);					   // This will unlock all write protects
			parseType->subState++;				   // Ready for the next state...
			break;								   // Done with subState 3
		case 4:									   // Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{									   // Checking...
				FLASH_CE_HIGH;					   // Deselect the SPI Serial Flash (Output is HIGH)
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->subState++;			   // Ready for the next state...
			}									   // End of SPI Transmit Complete check
			break;								   // Done with subState 4
		case 5:									   // Start the command process
			FLASH_CE_LOW;						   // Select the SPI Serial Flash (Output is LOW)
			spiTransmit(flashEnableWriteLatch);	// Send the WREN command
			parseType->subState++;				   // Ready for the next state...
			break;								   // Done with subState 5
		case 6:									   // Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{									   // Checking...
				FLASH_CE_HIGH;					   // Deselect the SPI Serial Flash (Output is HIGH)
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->subState++;			   // Ready for the next state...
			}									   // End of SPI Transmit Complete check
			break;								   // Done with subState 6
		case 7:									   // Start the command process
			FLASH_CE_LOW;						   // Select the SPI Serial Flash (Output is LOW)
			spiTransmit(flashChipErase);		   // Send the Chip Erase command (60h or C7h)
			parseType->subState++;				   // Ready for the next state...
			break;								   // Done with subState 7
		case 8:									   // Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{									   // Checking...
				FLASH_CE_HIGH;					   // Deselect the SPI Serial Flash (Output is HIGH)
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->subState++;			   // Ready for the next state...
			}									   // End of SPI Transmit Complete check
			break;								   // Done with subState 8
		case 9:									   // Start the command process
			FLASH_CE_LOW;						   // Select the SPI Serial Flash (Output is LOW)
			spiTransmit(flashReadStatusRegister);  // Send command to read the status register
			spiTransmit(flashBadCommand);		   // Send a dummy character to get the data back from the Flash Device
			parseType->subState++;				   // Ready for the next state...
			break;								   // Done with subState 9
		case 10:								   // Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{												 // Checking...
				FLASH_CE_HIGH;								 // Deselect the SPI Serial Flash (Output is HIGH)
				parseType->eatEcho = spiReceive();			 // Eat the echoed command
				parseType->spiStatusRegister = spiReceive(); // Receive the status byte
				if (parseType->spiStatusRegister & FLASH_STATUS_BUSY)
				{
					parseType->subState++;
					break;
				}
				parseType->commandComplete = TRUE; // Finished with the command... setting flag for further processing
			}									   // End of SPI Transmit Complete check
			break;								   // Done with subState 10
		case 11:
			parseType->subState--;
			parseType->subState--;
			break;
		}	  // End of substate switch
		break; // End of case: flashThorsHammer
	case msp430Write:
		switch (parseType->subState)
		{				   // Switch on subState to process this command
		case 1:			   // Start the command process
			MSP430_CE_LOW; // Select the SPI MSP430 (Output is LOW)
			cnt = 0;
			parseType->subState++; // Ready for the next state...
			parseType->subState++; // Ready for the next state...
			break;				   // Done with subState 1
		case 2:					   // Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->eatEcho = spiReceive(); // Eat the echoes
			}
			break;
		case 3:
			if (cnt < parseType->numberOfBytes)
			{ // Is there more to do?
#if defined DSP_INTERFACE
				tempData = *parseType->data++;
				spiTransmit(((tempData & 0xFFFF) >> 8));
				spiTransmit((tempData & 0xFF));
				cnt++;
				cnt++;
#endif
#if defined MSP430_INTERFACE
				spiTransmit(parseType->data[cnt++]); // Send first  byte to be programmed and increment pointer
				spiTransmit(parseType->data[cnt++]); // Send second byte to be programmed and increment pointer
#endif
				parseType->subState--;
			}
			else
			{						   // Else when have done what we came to do...
				parseType->subState++; // Ready for the next state...
			}						   // End of number of bytes to transfer check
			break;					   // Done with subState 3
		case 4:						   // Clean up time
			if (spiTxComplete())
			{									   // Checking...
				MSP430_CE_HIGH;					   // Deselect the SPI MSP430 (Output is HIGH)
				parseType->commandComplete = TRUE; // Finished with the command... setting flag for further processing
			}									   // End of SPI Transmit Complete check
			break;								   // Done with subState 4
		}										   // End of substate switch
		break;
	case msp430Read:
		switch (parseType->subState)
		{								  // Switch on subState to process this command
		case 1:							  // Start the command process
			MSP430_CE_LOW;				  // Select the SPI Serial Flash (Output is LOW)
			cnt = 0;					  // Preset the counter/pointer marker
			spiTransmit(flashBadCommand); // Send the Read command fake character
			parseType->subState++;		  // Ready for the next state...
			parseType->subState++;		  // Ready for the next state...
			break;						  // Done with subState 1
		case 2:							  // Hold here for SPI Transmit to complete
			if (spiTxComplete())
			{						   // Checking...
				parseType->subState++; // Go for it
			}						   // End of SPI Transmit Complete check
			break;					   // Done with subState 3
		case 3:						   // Transfer the Data
			if (cnt < parseType->numberOfBytes)
			{ // Is there more to do?
				if (spiPort.rxCharCount)
				{ // Any more characters in the buffer?
#if defined MSP430_INTERFACE
					*parseType->data++ = spiReceive(); // Receive byte and store it into the selected memory while incrementing pointer up one byte in memory
#endif
#if defined DSP_INTERFACE
					if (((cnt % 2) == 0) || (cnt == 0))
					{							 // If count is zero or evenly divisible by two then...
						tempData = spiReceive(); // Get the byte
						tempData <<= 8;			 // Shift it to the upper section
						tempData &= 0xFF00;		 // Clear out the garbage from the lower section
					}
					else
					{								   // Otherwise it is an odd one
						tempData += spiReceive();	  // Put the byte in the lower half of the integer
						*parseType->data++ = tempData; // Pass it back to the odd structure on the DSP
						tempData = 0;				   // Clear it out for the next go
					}
#endif
					cnt++;					  // Increment the counter for bytes received
				}							  // End of receive buffer check
				spiTransmit(flashBadCommand); // Send a dummy character to get the data back from the Flash Device
				parseType->subState--;		  // Go back and wait for transmission to finish
			}
			else
			{									   // Else when have what we came for...
				parseType->eatEcho = spiReceive(); // Eat the echoes
				parseType->subState++;			   // Ready for the next state...
			}									   // End of number of bytes to transfer check
			break;								   // Done with subState 4
		case 4:									   // Clean up time
			if (spiTxComplete())
			{									   // Checking...
				FLASH_CE_HIGH;					   // Deselect the SPI Serial Flash (Output is HIGH)
				parseType->commandComplete = TRUE; // Finished with the command... setting flag for further processing
			}									   // End of SPI Transmit Complete check
			break;								   // Done with subState 5
		}										   // End of substate switch
		break;									   // End of case: flashReadRecord
	case flashBadCommand:						   // Oops
	default:									   // Default catch
		parseType->commandComplete = TRUE;		   // Finished with the command... setting flag for further processing
		break;									   // End of case: flashBadCommand and default
	}											   // End of SWITCH on currentCommand
	if (parseType->commandComplete == TRUE)
	{																		  // Check to see if the command has finished processing
		parseType->subState = 1;											  // Yes, so reset subState to starting position for the next command
		spiPort.rxReadIndex = spiPort.rxWriteIndex = spiPort.rxCharCount = 0; // Flush the SPI Receive  Buffer by Reseting the Indexes and Counters
		spiPort.txReadIndex = spiPort.txWriteIndex = spiPort.txCharCount = 0; // Flush the SPI Transmit Buffer by Reseting the Indexes and Counters
	}																		  // End of commandComplete check
	return;																	  // Bye (for now)
} // End of flashCommunications

#if defined MSP430_INTERFACE
void init_hal_spi_flash(void)
{
	spiPort.rxWriteIndex = spiPort.rxReadIndex = spiPort.rxCharCount = 0;
	spiPort.txWriteIndex = spiPort.txReadIndex = spiPort.txCharCount = 0;
	spiPort.checkCount = 0;
	// Serial_Flash_CE* = P10.0, SPI_SIMO = P10.1, SPI_SOMI = P10.2, SPI_CLK = P10.3
	FLASH_CE_HIGH;								 // Deselect the SPI Serial Flash (Output is HIGH)
	P10SEL |= BIT1 + BIT2 + BIT3;				 // Select alternate function SCLK, SIMO, SOMI
	P10DIR |= BIT0 + BIT1 + BIT3;				 // Make pins Output for Chip Select, SIMO, and SCLK
	UCB3CTL1 |= UCSWRST;						 // Put UCSI machine in reset for configuration
	UCB3CTL0 |= UCMST + UCSYNC + UCMSB + UCCKPL; // Master Mode, Synchronous Mode, Most Significant bit first, Clock polarity
	UCB3CTL1 |= UCSSEL__SMCLK;					 // SMCLK source
	UCB3BR1 = 0x00;								 // Program the bit rate registers, 16 MHz / 91 = 175,824 bps or 21,978 bytes per second
	UCB3BR0 = BITRATECONTROL;					 // Designed so DSP and MSP430 interfaces are the same bit rate
	UCB3CTL1 &= ~UCSWRST;						 // Initialize USCI state machine for SPI functions
	UCB3IE |= UCRXIE;							 // Enable USCI_A3 RX interrupt
	spiFlashData.currentCommand = flashNoOperation;
	spiFlashData.subState = 1;
	spiFlashData.devicePresent = FALSE;
	spiFlashData.commandComplete = TRUE;
	return; // Bye
} // End of init_hal_spi_flash

bool spiTxComplete(void)
{
	bool status = FALSE;
	status = (UCB3STAT & UCBUSY);						   // Is the SPI done?
	status = !status;									   // Reverse the Polarity of the Neutron Flow ... AKA... Put into correct logic polarity
	status &= (spiPort.txCharCount == 0);				   // Have we emptied our buffer?
	status &= (spiPort.checkCount == spiPort.rxCharCount); // Does our in(s) and out(s) agree?
	if (status)
	{							// Check for yes to all...
		spiPort.checkCount = 0; // Reset the checkCount for the next go around
	}							// End of check
	return status;				// Return TRUE for good to go, FALSE for hold on a minute
} // End of spiTxComplete

bool spiSOready(void)
{
	bool testFlag = FALSE;	 // Test Variable for True or False
	FLASH_CE_LOW;			   // Select the SPI Serial Flash (Output is LOW)
	testFlag = (P10IN & BIT2); // Read and Mask for the information we require
	FLASH_CE_HIGH;			   // Deselect the SPI Serial Flash (Output is HIGH)
	return testFlag;		   // Return the Status of SOMI indicating Ready or Busy for AAI Programming
} // End of spiSOready

ubyte spiTransmit(volatile char cByte)
{
	if (spiPort.txCharCount >= SPI_BUFSIZE)
	{																		  // Are we full?
		spiPort.txReadIndex = spiPort.txWriteIndex = spiPort.txCharCount = 0; // Reset the Indexes and Counters
		return FALSE;														  // Indicate that the character was not put into the buffer
	}																		  // End of buffer check
	spiPort.txBuffer[spiPort.txWriteIndex++] = cByte;						  // Load the byte into the buffer and increment the index
	if (spiPort.txWriteIndex >= SPI_BUFSIZE)
	{							  // If the index is past the end of the circular buffer, roll over
		spiPort.txWriteIndex = 0; // Reset the index
	}							  // End of buffer roll over
	__disable_interrupt();		  // Make it safe
	spiPort.txCharCount++;		  // Added a new character, so increment the count
	spiPort.checkCount++;
	SPI_TX_INT_ENABLE;	// Enable the transmit interrupt
	__enable_interrupt(); // Exit safe mode
	return TRUE;		  // Indicate that that the character was added to the buffer
} // End of spi_putchar

ubyte spiReceive(void)
{
	char cByte = NULL;
	if (spiPort.rxCharCount)
	{													 // Is the character still available?
		cByte = spiPort.rxBuffer[spiPort.rxReadIndex++]; // Yes, get the byte from buffer
		if (spiPort.rxReadIndex >= SPI_BUFSIZE)
		{ // Handle buffer wrap around
			spiPort.rxReadIndex = 0;
		}
		SPI_RX_INT_DISABLE;	// Disable Receive Interrupt
		spiPort.rxCharCount--; // One character read, decrement count
		SPI_RX_INT_ENABLE;	 // Done, enable Receive Interrupt
	}
	return (cByte); // Return the data
} // End of spiReceive

#pragma vector = USCI_B3_VECTOR
__interrupt void USCI_B3(void)
{
	if (UCB3IFG & UCTXIFG)
	{ // If this is a TX interrupt...
		if (spiPort.txCharCount)
		{														 // If there are still characters in the buffer...
			UCB3TXBUF = spiPort.txBuffer[spiPort.txReadIndex++]; // Load the transmit register with the data and increment the index
			if (spiPort.txReadIndex >= SPI_BUFSIZE)
			{							 // Check if we are at the end of the circular buffer...
				spiPort.txReadIndex = 0; // Reset the index if so
			}							 // End of buffer check
			spiPort.txCharCount--;		 // Character was sent, so decrement the count
		}
		else
		{						// If no more characters to send...
			SPI_TX_INT_DISABLE; // Turn off TX interrupt
		}						// End of buffer checks
	}							// End of TX interrupt checks
	if (UCB3IFG & UCRXIFG)
	{														  // If this is a RX interrupt...
		spiPort.rxBuffer[spiPort.rxWriteIndex++] = UCB3RXBUF; // Store the received byte and increment the receive index
		if (spiPort.rxWriteIndex >= SPI_BUFSIZE)
		{							  // Check if we are at the end of the circular buffer...
			spiPort.rxWriteIndex = 0; // Reset the index if so
		}							  // End of buffer check
		spiPort.rxCharCount++;		  // Character was received, so increment the count
	}								  // End of RX interrupt checks
} // End of USCI_B3 ISR
#endif

#if defined DSP_INTERFACE
__interrupt void spiaFlashTransmitIsr(void)
{
	uint tempData; // Temporary data variable
	if (SpiaRegs.SPIFFTX.bit.TXFFST < 4)
	{ // Make sure there is room in the FIFO for our data (only four locations)
		if (spiPort.txCharCount)
		{														// OK, are there still characters in the buffer...
			tempData = spiPort.txBuffer[spiPort.txReadIndex++]; // Yes, Load the data that we need to make left justified and increment the index
			tempData <<= 8;										// Shift it to the left
			tempData &= 0xFF00;
			SpiaRegs.SPITXBUF = tempData; // Load the transmit register with the data
			if (spiPort.txReadIndex >= SPI_BUFSIZE)
			{							 // Check if we are at the end of the circular buffer...
				spiPort.txReadIndex = 0; // Reset the index if so
			}							 // End of buffer check
			spiPort.txCharCount--;		 // Character was sent, so decrement the count
		}
		else
		{								 // Else if no more characters to send...
			SPI_TX_INT_DISABLE;			 // Turn off TX interrupt
		}								 // End of buffer checks
	}									 // End of FIFO full check
	SpiaRegs.SPIFFTX.bit.TXFFINTCLR = 1; // Clear Interrupt flag
	PieCtrlRegs.PIEACK.bit.ACK6 = 1;	 // Issue PIE acknowledge
} // End of __interrupt void spiFlashTransmitIsr

__interrupt void spiaFlashReceiveIsr(void)
{
	Uint16 tempData; // Temporary data variable
	tempData = SpiaRegs.SPIRXBUF;
	if (MSP430_STATE == 0)
	{ // Are we talking to the MSP430?
		if (tempData >= 0x7F)
		{																// Is the return data greater than printable characters? "~" is upper bound
			tempData = 0x00;											// Stuff it with a NULL if both are true
		}																// End of printable check
	}																	// End of MSP check
	spiPort.rxBuffer[spiPort.rxWriteIndex++] = (char)(tempData & 0xFF); // Store the received "converted" byte and increment the receive index
	if (spiPort.rxWriteIndex >= SPI_BUFSIZE)
	{									 // Check if we are at the end of the circular buffer...
		spiPort.rxWriteIndex = 0;		 // Reset the index if so
	}									 // End of buffer check
	spiPort.rxCharCount++;				 // Character was received, so increment the count
	SpiaRegs.SPIFFRX.bit.RXFFOVFCLR = 1; // Clear Overflow flag
	SpiaRegs.SPIFFRX.bit.RXFFINTCLR = 1; // Clear Interrupt flag
	PieCtrlRegs.PIEACK.bit.ACK6 = 1;	 // Issue PIE acknowledge
} // End of __interrupt void spiFlashReceiveIsr

void init_hal_spi_flash(void)
{
	spiPort.rxWriteIndex = spiPort.rxReadIndex = spiPort.rxCharCount = 0; // Flush and prepare the SPI Receive  Buffer
	spiPort.txWriteIndex = spiPort.txReadIndex = spiPort.txCharCount = 0; // Flush and prepare the SPI Transmit Buffer
	spiPort.checkCount = 0;
	SpiaRegs.SPICCR.bit.SPISWRESET = 0;				// Reset the SPI Interface on the DSP
	EALLOW;											// Allow Protected Access to Registers
	PieVectTable.SPIRXINTA = &spiaFlashReceiveIsr;  // Set the Interrupt Routine for Receive
	PieVectTable.SPITXINTA = &spiaFlashTransmitIsr; // Set the Interrupt Routine for Transmit
	EDIS;											// Disallow Protected Access to Registers
	EALLOW;											// Allow Protected Access to Registers
	GpioCtrlRegs.GPBMUX2.bit.GPIO50 = 0;			// SPI Enable 0 - Serial Flash, configured as GPIO
	GpioCtrlRegs.GPBPUD.bit.GPIO50 = 0;				// *Enable pullups
	GpioCtrlRegs.GPBDIR.bit.GPIO50 = 1;				// Make this pin an output
	GpioCtrlRegs.GPAMUX2.bit.GPIO29 = 0;			// SPI Enable 1 - Local MSP430 Real Time Clock, configured as GPIO
	GpioCtrlRegs.GPAPUD.bit.GPIO29 = 0;				// *Enable pullups
	GpioCtrlRegs.GPADIR.bit.GPIO29 = 1;				// Make this pin an output
	FLASH_CE_HIGH;									// Deselect the SPI Serial Flash (Output is HIGH)
	MSP430_CE_HIGH;									// Deselect the MSP430 (Output is HIGH)
	GpioCtrlRegs.GPAPUD.bit.GPIO5 = 0;				// *Enable pull-up on  GPIO5 (SPISIMOA)
	GpioCtrlRegs.GPAQSEL1.bit.GPIO5 = 3;			// Asynchronous input GPIO5 (SPISIMOA)
	GpioCtrlRegs.GPADIR.bit.GPIO5 = 1;				// SPISIMOA is an output GPIO
	GpioCtrlRegs.GPAMUX1.bit.GPIO5 = 2;				// Configure GPIO5 as SPISIMOA
	GpioCtrlRegs.GPAPUD.bit.GPIO17 = 0;				// *Enable pull-up on  GPIO17 (SPISOMIA)
	GpioCtrlRegs.GPAQSEL2.bit.GPIO17 = 3;			// Asynchronous input GPIO17 (SPISOMIA)
	GpioCtrlRegs.GPADIR.bit.GPIO17 = 0;				// SPISOMIA is an input GPIO
	GpioCtrlRegs.GPAMUX2.bit.GPIO17 = 1;			// Configure GPIO17 as SPISOMIA
	GpioCtrlRegs.GPAPUD.bit.GPIO18 = 0;				// *Enable pull-up on  GPIO18 (SPICLKA)
	GpioCtrlRegs.GPAQSEL2.bit.GPIO18 = 3;			// Asynchronous input GPIO18 (SPICLKA)
	GpioCtrlRegs.GPADIR.bit.GPIO18 = 1;				// SPICLKA is an output GPIO
	GpioCtrlRegs.GPAMUX2.bit.GPIO18 = 1;			// Configure GPIO18 as SPICLKA
	EDIS;											// Disallow Protected Access to Registers
	SpiaRegs.SPICCR.bit.SPICHAR = 7;				// Select 8-bit character output (x + 1)
	SpiaRegs.SPICCR.bit.CLKPOLARITY = 1;			// ***Data output on rising clock edge with input data on falling edge
	SpiaRegs.SPICCR.bit.SPILBK = 0;					// SPI loop back mode is disabled
	SpiaRegs.SPICTL.bit.CLK_PHASE = 0;				// Normal SPI Clocking Scheme
	SpiaRegs.SPICTL.bit.MASTER_SLAVE = 1;			// SPI is configured as a Master
	SpiaRegs.SPICTL.bit.OVERRUNINTENA = 0;			// Disable RECEIVER OVERRUN Flag bit interrupts
	SpiaRegs.SPICTL.bit.TALK = 1;					// Let the SPI peripheral Talk
	SpiaRegs.SPIPRI.bit.FREE = 1;					// Emulator breakpoints will not disturb transmissions
	SpiaRegs.SPIBRR = BITRATECONTROL;				// Set baud rate to slowest speed, 90 MHz / 4 / 128 = 175,781 bps or 21,972 bytes per second
	SpiaRegs.SPIFFTX.bit.SPIRST = 0;				// The SPI Transmit and Receive Channels can resume
	SpiaRegs.SPIFFTX.bit.SPIFFENA = 1;				// SPI FIFO enhancements enabled
	SpiaRegs.SPIFFTX.bit.TXFIFO = 0;				// Clear out FIFO, reset pointer, and hold in Reset
	SpiaRegs.SPIFFTX.bit.TXFFINTCLR = 1;			// Clear the TXFIFINT flag
	SpiaRegs.SPIFFTX.bit.TXFFIL = 1;				// Set to zero
	SpiaRegs.SPIFFTX.bit.TXFFIENA = 0;				// **Enable FIFO Interrupt
	SpiaRegs.SPIFFRX.bit.RXFIFORESET = 0;			// Reset FIFO to zero and hold in reset
	SpiaRegs.SPIFFRX.bit.RXFFINTCLR = 1;			// Clear any pending interrupts
	SpiaRegs.SPIFFRX.bit.RXFFIENA = 1;				// Enable RX FIFO interrupt
	SpiaRegs.SPIFFRX.bit.RXFFIL = 1;				// FIFO generates interrupt on 1 or more characters received
	SpiaRegs.SPIFFCT.bit.TXDLY = 0;					// No delays in FIFO transmit of bits, but running in FIFO mode so it is one
	SpiaRegs.SPIFFTX.bit.SPIRST = 1;				// The SPI Transmit and Receive Channels can resume
	SpiaRegs.SPICTL.bit.SPIINTENA = 1;				// SPI Interrupts Enabled
	SpiaRegs.SPICCR.bit.SPISWRESET = 1;				// Relinquish SPI from Reset
	SpiaRegs.SPIFFTX.bit.TXFIFO = 1;				// Release FIFO for normal operations
	SpiaRegs.SPIFFRX.bit.RXFIFORESET = 1;			// Release FIFO for use
	PieCtrlRegs.PIECTRL.bit.ENPIE = 1;				// Enable PIE Block... just in case
	PieCtrlRegs.PIEIER6.bit.INTx1 = 1;				// Enable PIE Group 6, INT 1
	PieCtrlRegs.PIEIER6.bit.INTx2 = 1;				// Enable PIE Group 6, INT 2
	IER |= M_INT6;									// Core enable interrupt for SPI block+
	spiFlashData.currentCommand = flashNoOperation; // Idle the SPI Flash State Machine
	spiFlashData.subState = 1;						// Preset to subState 1
	spiFlashData.devicePresent = FALSE;				// Preset so that there is no device present
	spiFlashData.commandComplete = TRUE;			// Command is complete as there is none at this point
	return;											// Bye
} // End of init_hal_spi_flash

bool spiTxComplete(void)
{
	bool status = FALSE;								   // Test variable for the return
	status = (spiPort.txCharCount == 0);				   // Have we emptied our buffer?
	status &= (SpiaRegs.SPIFFTX.bit.TXFFST == 0);		   // Has the SPI peripheral sent everything?
	status &= (spiPort.checkCount == spiPort.rxCharCount); // Does our in(s) and out(s) agree?
	if (status)
	{							// Check for yes to all...
		spiPort.checkCount = 0; // Reset the checkCount for the next go around
	}							// End of check
	return status;				// Return TRUE for good to go, FALSE for hold on a minute
} // End of spiTxComplete

bool spiSOready(void)
{
	bool testFlag = FALSE;							  // Test Variable for True or False
	FLASH_CE_LOW;									  // Select the SPI Serial Flash (Output is LOW)
	testFlag = (GpioDataRegs.GPBDAT.bit.GPIO33 == 1); // Read and Mask for the information we require
	FLASH_CE_HIGH;									  // Deselect the SPI Serial Flash (Output is HIGH)
	return testFlag;								  // Return the Status of SOMI indicating Ready or Busy for AAI Programming
} // End of spiSOready

ubyte spiTransmit(volatile char cByte)
{
	if (spiPort.txCharCount >= SPI_BUFSIZE)
	{																		  // Are we full?
		spiPort.txReadIndex = spiPort.txWriteIndex = spiPort.txCharCount = 0; // Reset the Indexes and Counters
		return FALSE;														  // Indicate that the character was not put into the buffer
	}																		  // End of buffer check
	spiPort.txBuffer[spiPort.txWriteIndex++] = cByte;						  // Load the byte into the buffer and increment the index
	if (spiPort.txWriteIndex >= SPI_BUFSIZE)
	{									 // If the index is past the end of the circular buffer, roll over
		spiPort.txWriteIndex = 0;		 // Reset the index
	}									 // End of buffer roll over
	SpiaRegs.SPICTL.bit.SPIINTENA = 0;   // SPI Interrupts Disabled
	spiPort.txCharCount++;				 // Added a new character, so increment the count
	SpiaRegs.SPIFFTX.bit.TXFFINTCLR = 1; // Clear the TXFIFINT flag
	spiPort.checkCount++;
	SPI_TX_INT_ENABLE;				   // Enable the transmit interrupt
	SpiaRegs.SPICTL.bit.SPIINTENA = 1; // SPI Interrupts Enabled
	return TRUE;					   // Indicate that that the character was added to the buffer
} // End of spi_putchar

ubyte spiReceive(void)
{
	char cByte = NULL; // Temporary character value
	if (spiPort.rxCharCount)
	{													 // Is the character still available?
		cByte = spiPort.rxBuffer[spiPort.rxReadIndex++]; // Yes, get the byte from the buffer
		if (spiPort.rxReadIndex >= SPI_BUFSIZE)
		{ // Handle buffer wrap around
			spiPort.rxReadIndex = 0;
		}
		SPI_RX_INT_DISABLE;	// Don't Interrupt me right now...
		spiPort.rxCharCount--; // One character read, decrement count
		SPI_RX_INT_ENABLE;	 // OK, do what you want now...
	}
	return (cByte); // Return the data
} // End of spiReceive
#endif
