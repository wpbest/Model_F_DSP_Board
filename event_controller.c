/*!	@file		event_controller.c
 *	@author 	Argil Shaver
 *	@author		Mark James
 *	@date		16 April 2015
 *	@version	1.06
 *	@copyright	IntelliPower, Inc. (c) 2015
 *
 *	@brief		Performs Event Logging and Retrieval Functions to an External SPI Flash Device and MSP430.
 *
 * TARGET: 		MSP430x5419, MSP430x5419A, and TMS320x2806x devices
 *
 * HISTORY:
 * -			1.06	16 April 2015
 * 						Moved Event/Diagnostic display to separate file. Changed the access to record based instead of memory.
 * 						Added a function to convert record to flash memory addressing with rollover and safety checks.
 * 						Implemented RTC access and cleaned up the code and added comments. Addressed a few reported issues.
 * 						The system can now handle a change in log size without much trouble.
 * -			1.05	21 February 2015
 * 						Made the padding fields available for data storage based on event or diagnostic. Only displayed in
 * 						the extended display parser at present.
 * -			1.04	20 February 2015
 * 						Added Diagnostic events to the structure along with RTC data. Broke the structure down
 * 						to avoid packing issues between the two compilers for Piccolo and MSP processors. Cleaned
 * 						up the code to avoid a potential lock on event dump. Added code to allow to block diagnostic
 * 						dumps from the User parser and Bright UPS Communications interface. Documentation updated
 * 						and some of the field names modified to be clearer in their functions.
 * -			1.03	07 January 2015
 * 						Added in a field for the Real Time Clock. Fixed the bank sectors for erasing so we
 * 						do not get invalid data retrieval on rollover. A little more cleanup and reality checks.
 * 						Updated time stamping of events to handle system start up events generated before time validated.
 * -			1.02	06 January 2015
 * 						Added recordKeyVersion field to both configuration and event fields. Made adjustment
 * 						to the stored event field to make the data align on the DSP board. Fixed a few small
 * 						errors and adjusted some comments.
 * -			1.01	29 December 2014
 * 						Added Bank Switch Configuration and Event Display functions
 * 						Now compatible with Bright-UPS Interface, old micro test mode, and still supports
 * 						the new features added.
 * -			1.00	Initial Version	24 October 2014
 */

#include "event_controller.h" // Our own header for important stuff
#include "timer.h"			  // Time functions
#include "utilities.h"		  // Common Utilities that we use
#include <string.h>
#include <time.h>

#if defined RTC_AVAILABLE
#if defined MSP430_INTERFACE
#include "hal_rtc.h"
#endif
#endif

#if defined DSP_INTERFACE
#include "ups.h"		   // UPS functions
#include "system_config.h" // Includes file name for configuration
#include SYSTEM_CONFIG	 // Configuration header file
#endif

#ifdef VERBOSE_DEBUG_EVENTS
#ifdef VERBOSE_DEBUG
#include "uart.h"
#include "log.h"
#endif
#endif

uint8_t THOR = FALSE;
static timeT recoveredTime;

// Debug controllers declared in main
extern uint8_t startControllers;

event_controller_data_t eventData, *pEventData;

uint16_t packRTCstructure(uint16_t highData, uint16_t lowData)
{
	uint16_t tempData = 0;	 // Always have the temporary variable cleared when entering
	tempData |= highData << 8; // OR and Shift left to place the low byte of high into upper of temp
	tempData |= lowData;	   // OR in the low byte of low into temp
	return tempData;		   // Return the packed value
} // End of packRTCstructure

void unPackRTCstructure(uint16_t data, uint8_t *highData, uint8_t *lowData)
{
	*highData = (data & 0xFFFF) >> 8; // Unpack high order data to pointer area
	*lowData = (data & 0xFF);		  // Unpack low  order data to pointer area
	return;							  // Bye
} // End of unPackRTCstructure

void defaultConfiguration(void)
{
	timeT currentTime;
	int i;
	pEventData->systemData.recordNumber = 0L;	// No configuration data at this point
	pEventData->systemData.lastSpiRecord = 1L;   // There are no events
	pEventData->systemData.firstSpiRecord = 1L;  // There are no events
	pEventData->systemData.numberOfRecords = 0L; // No Records
	pEventData->systemData.numberOfEvents = 0L;  // No Events
	for (i = 0; i < (int)EVENT_END; i++)
	{																 // From beginning to end of structure eventNum_T
		pEventData->systemData.eventCounts[i] = 0L;					 // Zero out the counts for our starting position
	}																 // End of loop clearing out counts
	currentTime = getTime();										 // New system so get the current up time
	pEventData->systemData.lifeTime_days = currentTime.days;		 // Set the days, should be zero
	pEventData->systemData.lifeTime_milliSeconds = currentTime.msec; // Set the milliseconds... non-zero value
	pEventData->nextWriteRecord = 1L;								 // No records so the first write at the start of data
	pEventData->viewingRecord = 1L;									 // No records so default viewing at the start of data
	pEventData->nextRecordNumber = 0L;								 // No records at this point
	pEventData->systemAddress = BANK_ADDRESS_START;					 // No configuration yet so put it at the start
	pEventData->systemData.snmpUid = 0;								 // Default is SNMP UPS ID of zero
	pEventData->systemData.jouleCount = BATTERY_MAX_JOULES;			 // Default is Maximum Joules as defined in configuration file
#if defined DSP_INTERFACE
	pEventData->systemData.bankSwitches.all = initBankSwitches(); // Use the defined defaults in the configuration file
#else
	pEventData->systemData.bankSwitches = 0L;   // Default for a non-UPS system
#endif
	pEventData->systemData.recordKeyVersion = NONEVENT_KEY_VERSION; // Preset with the current Key/Version for the configuration data
	pEventData->systemData.rtc_Minute_Second = 0;					// Default RTC time is January 01 1900 midnight on a Monday...
	pEventData->systemData.rtc_Hour_DOW = 0x0001;
	pEventData->systemData.rtc_Day_Month = 0x0101;
	pEventData->systemData.rtc_Year = 1900;
	pEventData->systemData.rtc_mSeconds = 0;

	pEventData->systemReadyForUse = FALSE; // Say we are not ready for use yet just in case...
	return;								   // Bye
} // End of defaultBankConfiguration


bool timedOutDebugOutputDisplayEventTimer()
{
	bool retVal = TRUE;
    static volatile timeT debugOutputDisplayEventTimer;
	static uint8_t startTimer = TRUE;

	if (startTimer)
	{
		startTimer = FALSE;
		debugOutputDisplayEventTimer = getTime();
		retVal = TRUE;
	}
    else
	{
	    if (timer(debugOutputDisplayEventTimer, 100))
	    {
		    retVal = TRUE;
		    debugOutputDisplayEventTimer = getTime();
	    }
	    else
	    {
            retVal = FALSE;
	    }
	}

	return retVal;
}

void initEventStateController(void)
{
	pEventData = &eventData;																			   // Initialize Event Data Pointer and memory structure
	init_hal_spi_flash();																				   // Initialize the hardware access level to the SPI Flash Device
#ifdef VERBOSE_DEBUG_START
    pEventData->currentState = EVENT_DEBUG_START;
#else
	pEventData->currentState = EVENT_INITIALIZE;														   // Start off with Initialization of the state machine
#endif
	pEventData->lastState = EVENT_NO_FLASH;																   // Make sure it is forced
	pEventData->subState = 1;																			   // Start off with a subState of one (aka first step)
	pEventData->systemValid = FALSE;																	   // No valid Configuration Data
	pEventData->eventWriteIndex = pEventData->eventReadIndex = pEventData->numberEventsBuffered = 0;	   // Initialize the pointers for reading/writing events in the event buffer
	pEventData->commandWriteIndex = pEventData->commandReadIndex = pEventData->numberCommandsBuffered = 0; // Initialize the pointers for reading/writing Commands in the command buffer
	pEventData->findRecord = 1L;
	pEventData->fullRecordCount = 0L;
	defaultConfiguration(); // Preset data values for default start
	recoveredTime = getTime();
	return;					// Bye
} // End of init_Event_Controller

uint16_t readEvent(eventControllerCommand_t command)
{
	uint16_t returnStatus = FALSE; // Boolean to indicate if addition is successful, preset to FALSE
	if (pEventData->numberCommandsBuffered >= EVENT_BUFFER_LENGTH)
	{						  // Check to see if the buffer is full
		returnStatus = FALSE; // Return with Sorry Charlie
	}
	else if (command < COMMAND_FIND_RECORD)
	{																   // Else is the Command within legal range?
		pEventData->readEvent.dataReady = FALSE;					   // Quickly indicate the event record is NOT ready
		pEventData->commands[pEventData->commandWriteIndex] = command; // Yes so add it to the buffer
		pEventData->commandWriteIndex++;							   // Increment the pointer
		if (pEventData->commandWriteIndex >= EVENT_BUFFER_LENGTH)
		{									   // Check to see if we are at the end of the buffer
			pEventData->commandWriteIndex = 0; // Buffer rollover, go back to the beginning
		}									   // End of buffer wrap check
		pEventData->numberCommandsBuffered++;  // Increment the counter
		returnStatus = TRUE;				   // Addition of the eventCommand was successful
	}										   // End of event bounds check
	return returnStatus;					   // Bye, return the status of the operation
} // End of readEvent

uint16_t findEvent(Uint32 recordNumber)
{
	uint16_t returnStatus = FALSE; // Boolean to indicate if addition is successful, preset to FALSE
	if (pEventData->numberCommandsBuffered >= EVENT_BUFFER_LENGTH)
	{						  // Check to see if the buffer is full
		returnStatus = FALSE; // Return with Sorry Charlie
	}
	else
	{																			   // Else process...
		pEventData->readEvent.dataReady = FALSE;								   // Quickly indicate the event record is NOT ready
		pEventData->findRecord = recordNumber;									   // Pre-load the record number for later processing
		pEventData->commands[pEventData->commandWriteIndex] = COMMAND_FIND_RECORD; // Add it to the commands buffer
		pEventData->commandWriteIndex++;										   // Increment the pointer
		if (pEventData->commandWriteIndex >= EVENT_BUFFER_LENGTH)
		{									   // Check to see if we are at the end of the buffer
			pEventData->commandWriteIndex = 0; // Buffer rollover, go back to the beginning
		}									   // End of buffer wrap check
		pEventData->numberCommandsBuffered++;  // Increment the counter
		returnStatus = TRUE;				   // Addition of the eventCommand was successful
	}										   // End of event bounds check
	return returnStatus;					   // Bye, return the status of the operation
} // End of findEvent

uint16_t readWriteMsp430(eventControllerCommand_t command, uint8_t *dataString)
{
	uint16_t returnStatus = FALSE; // Boolean to indicate if addition is successful, preset to FALSE
	if (pEventData->numberCommandsBuffered >= EVENT_BUFFER_LENGTH)
	{															   // Check to see if the buffer is full
		return returnStatus;									   // It is, so return Sorry Charlie
	}															   // End of buffer limit check
	pEventData->msp430Data.dataReady = FALSE;					   // Set flag for external check to not ready aka False
	pEventData->commands[pEventData->commandWriteIndex] = command; // Put the command into the buffer for state machine processing
	pEventData->msp430Data.dataAddressPointer = dataString;		   // Make sure everyone knows where the data is located (both read and write)
	if (command == COMMAND_READ_MSP430)
	{															 // Are we reading?
		pEventData->msp430Data.lengthOfData = MAX_MSP430_STRING; // Yes, so we get the full buffer because messages can vary in length
	}
	else if (command == COMMAND_WRITE_MSP430)
	{																			// No but are we writing?
		pEventData->msp430Data.lengthOfData = strlen((const char *)dataString); // Yes, so set the length to the length of the string we are sending
	}
	else
	{											 // Otherwise...
		pEventData->msp430Data.lengthOfData = 0; // Set it to zero because I am paranoid
	}											 // End of the length field setup
	pEventData->commandWriteIndex++;			 // Increment the command write index pointer
	if (pEventData->commandWriteIndex >= EVENT_BUFFER_LENGTH)
	{									   // Check to see if we are at the end of the buffer
		pEventData->commandWriteIndex = 0; // Buffer rollover, go back to the beginning
	}									   // End of buffer wrap check
	pEventData->numberCommandsBuffered++;  // Increment the counter of buffered commands
	returnStatus = TRUE;				   // Addition of the eventCommand was successful
	return returnStatus;				   // Bye, return the status of the operation
} // End of readWriteMsp430

uint16_t saveConfiguration(void)
{
	uint16_t returnStatus = FALSE; // Boolean to indicate if addition is successful, preset to FALSE
	timeT currentTime;
	rtc_timeT localRTCtime;
	
	if (THOR)
	{
		 pEventData->systemData.bankSwitches.all |= 0x20000000UL; // Set the THOR bit.
		 pEventData->systemData.jouleCount = BATTERY_MAX_JOULES;
		 upsBoss.batteryJoules = BATTERY_MAX_JOULES;
	}

	if (pEventData->numberCommandsBuffered >= EVENT_BUFFER_LENGTH)
	{						  // Check to see if the buffer is full
		returnStatus = FALSE; // Return with Sorry Charlie
	}
	else
	{										   // Else is the Command within legal range?
		pEventData->systemData.recordNumber++; // Bump the record number of the configuration save
		currentTime = getTime();			   // Get the current time stamp
		localRTCtime = getRTCtime();
		pEventData->systemData.lifeTime_days = currentTime.days;		 // Update the days
		pEventData->systemData.lifeTime_milliSeconds = currentTime.msec; // Update the milliseconds
		pEventData->systemData.rtc_mSeconds = localRTCtime.mSeconds;
		pEventData->systemData.rtc_Minute_Second = packRTCstructure(localRTCtime.Minutes, localRTCtime.Seconds);
		pEventData->systemData.rtc_Hour_DOW = packRTCstructure(localRTCtime.Hours, localRTCtime.DayOfWeek);
		pEventData->systemData.rtc_Day_Month = packRTCstructure(localRTCtime.Day, localRTCtime.Month);
		pEventData->systemData.rtc_Year = localRTCtime.Year;
#if defined DSP_INTERFACE
		pEventData->systemData.jouleCount = (Uint32)upsBoss.batteryJoules; // Save the battery joule count
#else
		pEventData->systemData.jouleCount = 0L; // Give it a value for non-UPS applications
#endif
		pEventData->commands[pEventData->commandWriteIndex] = COMMAND_SAVE_BANK_SWITCH_DATA; // Add the command to the buffer
		pEventData->commandWriteIndex++;													 // Increment the pointer
		if (pEventData->commandWriteIndex >= EVENT_BUFFER_LENGTH)
		{									   // Check to see if we are at the end of the buffer
			pEventData->commandWriteIndex = 0; // Buffer rollover, go back to the beginning
		}									   // End of buffer wrap check
		pEventData->numberCommandsBuffered++;  // Increment the counter
		returnStatus = TRUE;				   // Addition of the eventCommand was successful
	}										   // End of event bounds check
	return returnStatus;					   // Bye, return the status of the operation
} // End of saveConfiguration

uint16_t nonDataEventCommand(eventControllerCommand_t command)
{
	uint16_t returnStatus = FALSE; // Boolean to indicate if addition is successful, preset to FALSE
	if (pEventData->numberCommandsBuffered >= EVENT_BUFFER_LENGTH)
	{						  // Check to see if the buffer is full
		returnStatus = FALSE; // Return with Sorry Charlie
	}
	else if ((command == COMMAND_THORS_HAMMER))
	{																   // Else is the Command within legal range?
		pEventData->commands[pEventData->commandWriteIndex] = command; // Yes so add it to the buffer
		pEventData->commandWriteIndex++;							   // Increment the pointer
		if (pEventData->commandWriteIndex >= EVENT_BUFFER_LENGTH)
		{									   // Check to see if we are at the end of the buffer
			pEventData->commandWriteIndex = 0; // Buffer rollover, go back to the beginning
		}									   // End of buffer wrap check
		pEventData->numberCommandsBuffered++;  // Increment the counter
		returnStatus = TRUE;				   // Addition of the eventCommand was successful
	}										   // End of event bounds check
	return returnStatus;					   // Bye, return the status of the operation
} // End of nonDataEventCommand

uint16_t addSystemEvent(eventNum_T eventCause)
{
	uint16_t returnStatus = FALSE; // Boolean to indicate if addition is successful
	timeT localTime;
	rtc_timeT localRTCtime;

	if (THOR == FALSE)
	{
		if (pEventData->numberEventsBuffered > EVENT_BUFFER_LENGTH)
		{				  // Check to see if the buffer is full
			return FALSE; // Return with Sorry Charlie
		}				  // End of write event buffer full check
		if (eventCause <= EVENT_END)
		{																				 // Is the eventCause within legal range?
			pEventData->writeEvents[pEventData->eventWriteIndex].logCode = eventCause;   // Yes so add it to the buffer
			pEventData->writeEvents[pEventData->eventWriteIndex].recordNumber = 0L;		 // This will be updated later when saved out
			pEventData->writeEvents[pEventData->eventWriteIndex].eventRecordNumber = 0L; // Updated when saved to flash
			localTime = getTime();														 // Set the time for eventCause to the eventData buffer
			localRTCtime = getRTCtime();
			pEventData->writeEvents[pEventData->eventWriteIndex].lifeTime_days = localTime.days;		 // Update the days
			pEventData->writeEvents[pEventData->eventWriteIndex].lifeTime_milliSeconds = localTime.msec; // Update the milliseconds
			pEventData->writeEvents[pEventData->eventWriteIndex].rtc_mSeconds = localRTCtime.mSeconds;
			pEventData->writeEvents[pEventData->eventWriteIndex].rtc_Minute_Second = packRTCstructure(localRTCtime.Minutes, localRTCtime.Seconds);
			pEventData->writeEvents[pEventData->eventWriteIndex].rtc_Hour_DOW = packRTCstructure(localRTCtime.Hours, localRTCtime.DayOfWeek);
			pEventData->writeEvents[pEventData->eventWriteIndex].rtc_Day_Month = packRTCstructure(localRTCtime.Day, localRTCtime.Month);
			pEventData->writeEvents[pEventData->eventWriteIndex].rtc_Year = localRTCtime.Year;
			pEventData->writeEvents[pEventData->eventWriteIndex].event_diagnostic_data_1 = 0;
			pEventData->writeEvents[pEventData->eventWriteIndex].event_diagnostic_data_2 = 0;
			pEventData->writeEvents[pEventData->eventWriteIndex].event_diagnostic_data_3 = 0;
			pEventData->writeEvents[pEventData->eventWriteIndex].event_diagnostic_data_4 = 0;
			pEventData->writeEvents[pEventData->eventWriteIndex].recordKeyVersion = EVENT_KEY_VERSION;		// Tag it as one of our own
			pEventData->writeEvents[pEventData->eventWriteIndex].dataReady = pEventData->systemReadyForUse; // Indicate if configuration is ready or not for time stamp adjustment later
			pEventData->eventWriteIndex++;																	// Increment the pointer
			if (pEventData->eventWriteIndex >= EVENT_BUFFER_LENGTH)
			{									 // Check to see if we are at the end of the buffer
				pEventData->eventWriteIndex = 0; // Buffer rollover, go back to the beginning
			}									 // End of buffer wrap check
			pEventData->numberEventsBuffered++;  // Increment the counter
			returnStatus = TRUE;				 // Addition of the eventCause was successful
		}										 // End of event bounds check
	}
	return returnStatus;					 // Bye, return the status of the operation
} // End of addSystemEvent

uint16_t addDiagnostic(diagnosticNum_T diagnostic, uint16_t *data)
{
	uint16_t returnStatus = FALSE; // Boolean to indicate if addition is successful
	timeT localTime;
	rtc_timeT localRTCtime;
	if (pEventData->numberEventsBuffered > EVENT_BUFFER_LENGTH)
	{				  // Check to see if the buffer is full
		return FALSE; // Return with Sorry Charlie
	}				  // End of write event buffer full check
	if (diagnostic <= DIAGNOSTIC_END)
	{																						 // Is the eventCause within legal range?
		pEventData->writeEvents[pEventData->eventWriteIndex].logCode = (uint16_t)diagnostic; // Yes so add it to the buffer
		pEventData->writeEvents[pEventData->eventWriteIndex].recordNumber = 0L;				 // This will be updated later when saved out
		pEventData->writeEvents[pEventData->eventWriteIndex].eventRecordNumber = 0L;		 // Not used in diagnostics
		localTime = getTime();																 // Set the time for eventCause to the eventData buffer
		localRTCtime = getRTCtime();
		pEventData->writeEvents[pEventData->eventWriteIndex].lifeTime_days = localTime.days;		 // Update the days
		pEventData->writeEvents[pEventData->eventWriteIndex].lifeTime_milliSeconds = localTime.msec; // Update the milliseconds
		pEventData->writeEvents[pEventData->eventWriteIndex].rtc_mSeconds = localRTCtime.mSeconds;
		pEventData->writeEvents[pEventData->eventWriteIndex].rtc_Minute_Second = packRTCstructure(localRTCtime.Minutes, localRTCtime.Seconds);
		pEventData->writeEvents[pEventData->eventWriteIndex].rtc_Hour_DOW = packRTCstructure(localRTCtime.Hours, localRTCtime.DayOfWeek);
		pEventData->writeEvents[pEventData->eventWriteIndex].rtc_Day_Month = packRTCstructure(localRTCtime.Day, localRTCtime.Month);
		pEventData->writeEvents[pEventData->eventWriteIndex].rtc_Year = localRTCtime.Year;
		pEventData->writeEvents[pEventData->eventWriteIndex].event_diagnostic_data_1 = *data++;
		pEventData->writeEvents[pEventData->eventWriteIndex].event_diagnostic_data_2 = *data++;
		pEventData->writeEvents[pEventData->eventWriteIndex].event_diagnostic_data_3 = *data++;
		pEventData->writeEvents[pEventData->eventWriteIndex].event_diagnostic_data_4 = *data;
		pEventData->writeEvents[pEventData->eventWriteIndex].recordKeyVersion = EVENT_KEY_VERSION;		// Tag it as one of our own
		pEventData->writeEvents[pEventData->eventWriteIndex].dataReady = pEventData->systemReadyForUse; // Indicate if configuration is ready or not for time stamp adjustment later
		pEventData->eventWriteIndex++;																	// Increment the pointer
		if (pEventData->eventWriteIndex >= EVENT_BUFFER_LENGTH)
		{									 // Check to see if we are at the end of the buffer
			pEventData->eventWriteIndex = 0; // Buffer rollover, go back to the beginning
		}									 // End of buffer wrap check
		pEventData->numberEventsBuffered++;  // Increment the counter
		returnStatus = TRUE;				 // Addition of the eventCause was successful
	}										 // End of event bounds check
	return returnStatus;					 // Bye, return the status of the operation
} // End of addDiagnostic

Uint32 convertRecordToAddress(Uint32 record, uint16_t check)
{
	Uint32 test;
	if (check)
	{ // Do we do a bounds check?
		if (record < pEventData->systemData.firstSpiRecord)
		{
			record = pEventData->systemData.firstSpiRecord;
		} // Cannot go below the first retrievable record
		if (record > pEventData->systemData.lastSpiRecord)
		{
			record = pEventData->systemData.lastSpiRecord;
		}													// Cannot go above the last retrievable record
		pEventData->viewingRecord = record;					// If checking that means read, so update viewing just in case
	}														// End of bounds check for this function
	record -= 1L;											// Subtract one to normalize for zero base address counting
	record %= pEventData->fullRecordCount;					// Get the remainder of anything divided by the maximum stored records
	test = record;											// Just because I am paranoid
	record *= (Uint32)SIZE_LOG_DATA;						// Multiply by the record size
	record += ((test / RECORDS_PER_SECTOR) * ORPHAN_BYTES); // Account for orphaned bytes per storage sector
	record += DATA_ADDRESS_START;							// Add in the address for the beginning of data storage
	return record;											// Bye, return the address we are seeking
} // End of convertRecordToAddress

void eventStateController(void)
{
	
	static rtc_timeT localRTCtime;
	static volatile int i = 0;

	switch (pEventData->currentState)
	{					   // Switch on our current state requirements
	case EVENT_DEBUG_START:
        if (startControllers) pEventData->currentState = EVENT_INITIALIZE;
	    break;
	case EVENT_INITIALIZE: // Initialize the event controller, so let's fire this puppy up
		if (pEventData->currentState != pEventData->lastState)
		{													  // State Entry Code
			pEventData->lastState = pEventData->currentState; // lastState is now same as currentState... our entry point
			pEventData->systemValid = FALSE;				  // No valid Configuration Data
			spiFlashData.commandComplete = FALSE;			  // Preset commandComplete to FALSE so we can execute a SPI interface command
			spiFlashData.currentCommand = flashReadJedecID;   // Get important information about the External SPI Flash Device
			pEventData->subState = 1;						  // Start off with a subState of one (aka first step)
			flashCommunications(&spiFlashData);				  // Initiate the get Jedec ID command in flashCommunications
#ifdef VERBOSE_DEBUG_EVENTS
#ifdef VERBOSE_DEBUG)
            if ((upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					if (timedOutDebugOutputDisplayEventTimer()) 
					log_trace("EVENT_INITIALIZE:Initiate the get Jedec ID command in flashCommunications");
				}
		    }
#endif
#endif

		}													  // End of State Entry Code
		switch (pEventData->subState)
		{		// Switch base on our subState condition
		case 1: // Step 1: Wait for the Jedec ID command to complete
			if (spiFlashData.commandComplete)
			{ // Is the command finished?
				switch (spiFlashData.deviceID)
				{												// Which memory chip do we have installed?
				case 0x41:										// SST25VF016	->	2M Byte device
					spiFlashData.Data_Address_End = SST25VF016; // Note: 512 4k Sectors
					spiFlashData.devicePresent = TRUE;			// Device present and usable by this software
					break;										// Run away
				case 0x4A:										// SST25VF032	-> 4M Byte device
					spiFlashData.Data_Address_End = SST25VF032; // Note: 1024 4k Sectors
					spiFlashData.devicePresent = TRUE;			// Device present and usable by this software
					break;										// Run away
				case 0x4B:										// SST25VF064	-> 8M byte device
					spiFlashData.Data_Address_End = SST25VF064; // Note: 2048 4k Sectors
					spiFlashData.devicePresent = TRUE;			// Device present and usable by this software
					break;										// Run away
				default:										// DEFAULT		-> Nothing there or not talking
					spiFlashData.Data_Address_End = 0x00000000; // Default value, just to have something
					spiFlashData.devicePresent = FALSE;			// No device or not recognized by this software
					break;										// Run away, but it is only a bunny rabbit...
				}												// Done with switch on deviceID
				if (!spiFlashData.devicePresent)
				{																					// Do we have a valid recognized device?
					pEventData->currentState = EVENT_NO_FLASH;										// No, bad device or not supported, get out while you still can
#ifdef VERBOSE_DEBUG_EVENTS
#if (defined VERBOSE_DEBUG)
					if ((upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
						{
							if (timedOutDebugOutputDisplayEventTimer()) 
							log_trace("EVENT_INITIALIZE:EVENT_NO_FLASH:Bad device or not supported");
						}
					}
#endif
#endif
					break;																			// Run you clever boy...
				}																					// End of valid recognized device check
				pEventData->fullRecordCount = (spiFlashData.Data_Address_End - DATA_ADDRESS_START); // Get the number of data bytes that we can save
				pEventData->fullRecordCount /= SECTOR_SIZE;											// Bring back into a sector count for fullRecordCount calculation
				pEventData->fullRecordCount *= RECORDS_PER_SECTOR;									// Multiple by records per sector to give us our fullRecordCount for the system
				pEventData->systemAddress = DATA_ADDRESS_START - (Uint32)(SIZE_SYSTEM_DATA);		// Start by looking at the last configuration record
				pEventData->systemValid = FALSE;													// Set so we do a quick scan
				pEventData->subState++;																// Let's go to work on event record scanning now
			}
			else
			{											   // Otherwise
				flashCommunications(&spiFlashData);		   // Let it process
			}											   // End of commandComplete check
			break;										   // Done with subState 1
		case 2:											   // Step 2:
			spiFlashData.commandComplete = FALSE;		   // Start with command not complete
			spiFlashData.currentCommand = flashReadRecord; // We want to do a continuous read for the length selected
			if (pEventData->systemValid)
			{												   // Have we found the last record?
				spiFlashData.numberOfBytes = SIZE_SYSTEM_DATA; // Yes, get the entire Record now
			}
			else
			{													  // Otherwise
				spiFlashData.numberOfBytes = 4;					  // No, just get Record Number for now
			}													  // End of valid record size set check
			spiFlashData.address = pEventData->systemAddress;	 // Set the address for flashCommunications
			spiFlashData.data = (ubyte *)&pEventData->systemData; // Where to put the data, not initialized yet so we steal the structure
			pEventData->subState++;								  // Go to the next operation
			break;												  // Done with subState 2
		case 3:													  // Step 3: Wait for scan read to finish and check/modify memory access data
			if (spiFlashData.commandComplete)
			{ // Is the command finished?
#ifdef VERBOSE_DEBUG_EVENTS
#ifdef VERBOSE_DEBUG)
				if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						if (timedOutDebugOutputDisplayEventTimer())
						log_trace("spiFlashData.commandComplete");
					}
				}
#endif
#endif
				if (pEventData->systemData.recordNumber == BLANK_MEMORY)
				{ // Is the memory read blank?
					if (pEventData->systemAddress == BANK_ADDRESS_START)
					{														 // Are we at the beginning?
						pEventData->subState++;								 // Virgin System Alert !!!!!!!!!!!!!
						break;												 // Do the virgin
					}														 // End of Virgin System Check
					pEventData->systemAddress -= (Uint32)(SIZE_SYSTEM_DATA); // Still blank so go back one more record
					pEventData->subState--;									 // Go back and check the record number again
				}
				else
				{ // Otherwise...
					if (pEventData->systemValid)
					{ // Have we read in the valid record?
						if (pEventData->systemData.recordKeyVersion != NONEVENT_KEY_VERSION)
						{																	 // Check to see if it is a compatible format
							pEventData->subState = 5;										 // No it is not... so we must fix the problem
							break;															 // Leave this place
						}																	 // End of compatibility check
						pEventData->subState = 1;											 // Ready the subState for the next system state
						pEventData->nextRecordNumber = pEventData->systemData.lastSpiRecord; // Get the last saved event address
						pEventData->nextWriteRecord = pEventData->nextRecordNumber + 1L;	 // Point one record above the last
						pEventData->systemAddress += (Uint32)(SIZE_SYSTEM_DATA);			 // Point to the next write of bank configuration data as the initial pointer is ourself
						pEventData->viewingRecord = pEventData->systemData.lastSpiRecord;	// Set viewing to the last Event Address
						// THOR Bit Set: Reset Time and clear Events
						if (pEventData->systemData.bankSwitches.bit.THOR)
						{
#ifdef VERBOSE_DEBUG_EVENTS
#ifdef VERBOSE_DEBUG
							if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
							{
								if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
								{
									if (timedOutDebugOutputDisplayEventTimer())
									log_trace("THOR Bit Set: Reset Time and clear Events");
								}
							}
#endif
#endif
							pEventData->systemData.bankSwitches.bit.THOR = 0;
							pEventData->systemData.lifeTime_days = 0;
							pEventData->systemData.lifeTime_milliSeconds = 0;
							defaultConfiguration();
							pEventData->systemReadyForUse = TRUE;
							pEventData->systemData.jouleCount = BATTERY_MAX_JOULES;
							upsBoss.batteryJoules = BATTERY_MAX_JOULES;
						}
						recoveredTime.days = pEventData->systemData.lifeTime_days;			 // Recover the days
						recoveredTime.msec = pEventData->systemData.lifeTime_milliSeconds;   // Recover the milliseconds
						pEventData->currentState = EVENT_TEST_LOG_DATA;						 // Ready for data compatibility check						
					}
					else
					{									// Otherwise...
						pEventData->systemValid = TRUE; // Ready to read it in since we found it
						pEventData->subState--;			// Go back and get the record now
					}									// End of valid record check
				}										// End of blank memory check
			}
			else
			{												// Otherwise...
				flashCommunications(&spiFlashData);			// Command not done, so let it process
			}												// End of commandComplete check
			break;											// Done with subState 3
		case 4:												// Step 4: Handle a virgin system
			defaultConfiguration();							// Set it up as a clean system
			pEventData->subState = 1;						// Ready the subState for the next system state usage
			pEventData->currentState = EVENT_TEST_LOG_DATA; // Ready for execution in next state
			break;											// Done with subState 4
		case 5:
			spiFlashData.currentCommand = flashEraseNextSector; // Clear out the last sector of the BANK area
			spiFlashData.commandComplete = FALSE;				// Start with command not complete
			spiFlashData.address = SECTOR_SIZE;					// Point into start of second sector
			pEventData->subState++;								// Go to next subState to process
			break;												// End of subState 5
		case 6:													// Clear out the first sector of the BANK area when done
			if (spiFlashData.commandComplete)
			{														// Is it done?
				spiFlashData.currentCommand = flashEraseNextSector; // Yes, last write to first sector, so clear out the last sector
				spiFlashData.commandComplete = FALSE;				// Start with command not complete
				spiFlashData.address = BANK_ADDRESS_START;			// Point into start of second sector
				pEventData->subState++;								// Go to next subState to process
			}
			else
			{										// Otherwise...
				flashCommunications(&spiFlashData); // Continue to process the issued command
			}										// End of commandComplete check
			break;									// End of subState 6
		case 7:										// Step 7: Put everything at default when ready
			if (spiFlashData.commandComplete)
			{													// Has the previous erase finished?
				defaultConfiguration();							// Set it up as a clean system
				pEventData->subState = 1;						// Rest the subState for the next function
				pEventData->currentState = EVENT_TEST_LOG_DATA; // Go forth and recover any valid event records
			}
			else
			{										// Otherwise...
				flashCommunications(&spiFlashData); // Continue to process the issued command
			}										// End of commandComplete check
			break;									// Done with subState 7
		default:									// Default fall through
			break;									// Done with default
		}											// End of switch on subState
		break;										// End of case eventInitialize:
	case EVENT_TEST_LOG_DATA:
		if (pEventData->currentState != pEventData->lastState)
		{													  // State Entry Code
			pEventData->lastState = pEventData->currentState; // lastState is now same as currentState... our entry point
			pEventData->subState = 1;						  // Get read to rock and roll
#ifdef VERBOSE_DEBUG_EVENTS			
#if (defined VERBOSE_DEBUG)
            if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					if (timedOutDebugOutputDisplayEventTimer()) 
					log_trace("EVENT_TEST_LOG_DATA");
				}
		    }
#endif
#endif
			
		}													  // End of State Entry Code
		switch (pEventData->subState)
		{																								// Switch on subState
		case 1:																							// Step 1: Prepare to check the last record for compatibility
			spiFlashData.commandComplete = FALSE;														// Start with command not complete
			spiFlashData.currentCommand = flashReadRecord;												// We want to do a continuous read for the length selected
			spiFlashData.address = convertRecordToAddress(pEventData->systemData.lastSpiRecord, FALSE); // Set the address for flashCommunications
			spiFlashData.data = (ubyte *)&pEventData->readEvent;										// Where flashCommunications is to put the data
			spiFlashData.numberOfBytes = SIZE_LOG_DATA;													// Get the entire Record
			pEventData->subState++;																		// Wait for the request to finish
#ifdef VERBOSE_DEBUG_EVENTS			
#ifdef VERBOSE_DEBUG
            if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					if (timedOutDebugOutputDisplayEventTimer()) 
					log_trace("EVENT_TEST_LOG_DATAsubState:1");
				}
		    }
#endif
#endif
			break;																						// Done with subState 1
		case 2:
			if (spiFlashData.commandComplete)
			{ // Is the command finished?
				if (pEventData->readEvent.recordKeyVersion != EVENT_KEY_VERSION)
				{							// Test the event record key/version field for compatibility
					pEventData->subState++; // Sorry Charlie we are worlds apart, time to cleanse the system
#ifdef VERBOSE_DEBUG_EVENTS
#ifdef VERBOSE_DEBUG
					if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
						{
							if (timedOutDebugOutputDisplayEventTimer())
							log_trace("EVENT_TEST_LOG_DATA != EVENT_KEY_VERSION");
						}
					}
#endif
#endif
				}
				else
				{													 // Otherwise...
					pEventData->subState = 1;						 // Reset the subState for the next section
					pEventData->currentState = EVENT_RECOVERY_CHECK; // Proceed with a clear conscience to the eventRecorveryCheck section
#ifdef VERBOSE_DEBUG_EVENTS
#ifdef VERBOSE_DEBUG
					if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
						{
						    if (timedOutDebugOutputDisplayEventTimer()) 
							log_trace("EVENT_TEST_LOG_DATA:EVENT_RECOVERY_CHECK1");
						}
					}
#endif
#endif
				}													 // End of compatibility check
			}
			else
			{										// Otherwise...
				flashCommunications(&spiFlashData); // Command not done, so let it process
			}										// End of commandComplete check
			break;									// Done with subState 2
		case 3:
			spiFlashData.currentCommand = flashThorsHammer; // Bring the Hammer down with all the wrath of Thor
			spiFlashData.commandComplete = FALSE;			// Preset commandComplete to FALSE so we can execute a SPI interface command
			pEventData->subState++;							// Ready for the next subState operation
#ifdef VERBOSE_DEBUG_EVENTS			
#ifdef VERBOSE_DEBUG
			if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					if (timedOutDebugOutputDisplayEventTimer())
					log_trace("EVENT_TEST_LOG_DATA:flashThorsHammer");
				}
			}
#endif
#endif
			break;											// Done with subState 3
		case 4:
			if (spiFlashData.commandComplete)
			{													// Is the SPI Interface done?
				pEventData->nextRecordNumber = 0L;				// No event data at this point
				pEventData->systemData.lastSpiRecord = 1L;		// Rest to default values
				pEventData->systemData.firstSpiRecord = 1L;		// Rest to default values
				pEventData->nextWriteRecord = 1L;				// No records so the first write at the start of data
				pEventData->viewingRecord = 1L;					// No records so default viewing at the start of data
				pEventData->subState = 1;						// Reset the subState for the next operation
				pEventData->currentState = EVENT_TIME_RECOVERY; // Ready to Idle as there is nothing out there
#ifdef VERBOSE_DEBUG_EVENTS				
#ifdef VERBOSE_DEBUG
				if ((upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						if (timedOutDebugOutputDisplayEventTimer()) 
						log_trace("EVENT_TEST_LOG_DATA:EVENT_TIME_RECOVERY2");
					}
				}
#endif
#endif
			}
			else
			{										// Otherwise command not complete
				flashCommunications(&spiFlashData); // Let it process
			}										// End of commandComplete check
			break;									// End of subState 4
		}											// End of subState switch
		break;										// End of case eventTestEventData
	case EVENT_RECOVERY_CHECK:
		if (pEventData->currentState != pEventData->lastState)
		{													  // State Entry Code
			pEventData->lastState = pEventData->currentState; // lastState is now same as currentState... our entry point
			pEventData->subState = 1;						  // Get read to rock and roll
#ifdef VERBOSE_DEBUG_EVENTS			
#ifdef VERBOSE_DEBUG
            if ((upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					if (timedOutDebugOutputDisplayEventTimer())
					log_trace("EVENT_RECOVERY_CHECK");
				}
		    }
#endif
#endif
#if defined DSP_INTERFACE
			upsBoss.batteryJoules = pEventData->systemData.jouleCount; // Update upsBoss with our recovered Joule Count
#endif
		} // End of State Entry Code
		switch (pEventData->subState)
		{																					 // Switch on subState
		case 1:																				 // Step 1: Prepare to check for records past last saved location
			pEventData->viewingRecord++;													 // Bump viewingAddress up by one record size
			spiFlashData.commandComplete = FALSE;											 // Start with command not complete
			spiFlashData.currentCommand = flashReadRecord;									 // We want to do a continuous read for the length selected
			spiFlashData.address = convertRecordToAddress(pEventData->viewingRecord, FALSE); // Set the address for flashCommunications
			spiFlashData.data = (ubyte *)&pEventData->readEvent;							 // Where flashCommunications is to put the data
			spiFlashData.numberOfBytes = SIZE_LOG_DATA;										 // Get the entire Record
			pEventData->subState++;															 // Wait for the request to finish
#ifdef VERBOSE_DEBUG_EVENTS			
#ifdef VERBOSE_DEBUG
            if ((upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					if (timedOutDebugOutputDisplayEventTimer()) 
					log_trace(": esc: EVENT_RECOVERY_CHECK:subState:1");
				}
		    }
#endif
#endif
			break;																			 // Done with subState 1
		case 2:																				 // Step 2: Test the record and make adjustments
			if (spiFlashData.commandComplete)
			{ // Is the command finished?
				if (pEventData->readEvent.recordNumber == BLANK_MEMORY)
				{																				   // Is the memory read blank?
					pEventData->viewingRecord--;												   // Back viewingAddress down by one record size
					pEventData->subState = 1;													   // We are good
					pEventData->systemData.lastSpiRecord = pEventData->systemData.numberOfRecords; // Time to recover
					pEventData->nextRecordNumber = pEventData->systemData.lastSpiRecord;		   // Get the last saved event address
					pEventData->nextWriteRecord = pEventData->nextRecordNumber + 1L;			   // Base the next Write Address on the last Event Address, but...
					pEventData->systemData.lifeTime_days = recoveredTime.days;
					pEventData->systemData.lifeTime_milliSeconds = recoveredTime.msec;
					setTime(recoveredTime);							                               // Update the system runtime
					pEventData->spiRecoveryTimer = getTime();		                               // Update the configuration timer as we adjusted the time from recovered
					pEventData->currentState = EVENT_TIME_RECOVERY;                                // Time to rest
			        usart_init();                                                                  // Initialize UARTs
			        bankSwitchBaudSet();                                                           // Set Baud Rate

#ifdef VERBOSE_DEBUG_EVENTS
#ifdef VERBOSE_DEBUG
					if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
						{
							if (timedOutDebugOutputDisplayEventTimer())
							log_trace("EVENT_RECOVERY_CHECK:BLANK_MEMORY");
						}
					}
#endif
#endif		
				}
				else
				{ // Otherwise...
					if (pEventData->readEvent.recordKeyVersion != EVENT_KEY_VERSION)
					{							  // Check for compatibility
						pEventData->subState = 3; // Sorry Charlie, must run and cleanse myself now
#ifdef VERBOSE_DEBUG_EVENTS						
#ifdef VERBOSE_DEBUG
						if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
						{
							if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
							{
								if (timedOutDebugOutputDisplayEventTimer()) 
								log_trace("EVENT_RECOVERY_CHECK!= EVENT_KEY_VERSION");
							}
						}
#endif
#endif		
						break;					  // Run
					}							  // End of compatibility check
					if (pEventData->readEvent.logCode < (int)(EVENT_END))
					{																		 // Are we in the Event Range?
						pEventData->systemData.eventCounts[pEventData->readEvent.logCode]++; // It is a record of an Event, so bump that counter
						pEventData->systemData.numberOfEvents = pEventData->readEvent.eventRecordNumber;
					} // End of Event Range Check
					pEventData->systemData.numberOfRecords = pEventData->readEvent.recordNumber;
					recoveredTime.days = pEventData->readEvent.lifeTime_days;		  // Recover the days
					recoveredTime.msec = pEventData->readEvent.lifeTime_milliSeconds; // Recover the milliseconds
					pEventData->subState--;											  // No, so look at the next memory record
#ifdef VERBOSE_DEBUG_EVENTS					
#ifdef VERBOSE_DEBUG
					if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
						{
							if (timedOutDebugOutputDisplayEventTimer())
							log_trace("EVENT_RECOVERY_CHECK:subState-=1");
						}
					}
#endif
#endif		
				}																	  // End of blank checks
			}
			else
			{												// Otherwise...
				flashCommunications(&spiFlashData);			// Command not done, so let it process
			}												// End of commandComplete check
			break;											// Done with subState 2
		case 3:												// Step 3: The system is unclean
			spiFlashData.currentCommand = flashThorsHammer; // Bring the Hammer down with all the wrath of Thor
			spiFlashData.commandComplete = FALSE;			// Preset commandComplete to FALSE so we can execute a SPI interface command
			pEventData->subState++;							// Ready for the next subState operation
#ifdef VERBOSE_DEBUG_EVENTS			
#ifdef VERBOSE_DEBUG
			if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					if (timedOutDebugOutputDisplayEventTimer()) 
					log_trace("EVENT_RECOVERY_CHECK:subState:3:flashThorsHammer");
				}
			}
#endif
#endif		
			break;											// Done with subState 3
		case 4:												// Step 4: Wait for it to finish
			if (spiFlashData.commandComplete)
			{													// Is the SPI Interface done?
				pEventData->nextRecordNumber = 0L;				// No event data at this point
				pEventData->systemData.lastSpiRecord = 1L;		// Rest to default values
				pEventData->systemData.firstSpiRecord = 1L;		// Rest to default values
				pEventData->nextWriteRecord = 1L;				// No records so the first write at the start of data
				pEventData->viewingRecord = 1L;					// No records so default viewing at the start of data
				pEventData->subState = 1;						// Reset the subState for the next operation
				pEventData->currentState = EVENT_TIME_RECOVERY; // Ready to Idle as there is nothing out there
#ifdef VERBOSE_DEBUG_EVENTS
#ifdef VERBOSE_DEBUG
				if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						if (timedOutDebugOutputDisplayEventTimer())
						log_trace(": esc: EVENT_RECOVERY_CHECK:subState:4");
					}
				}
#endif		
#endif				
			}
			else
			{										// Otherwise command not complete
				flashCommunications(&spiFlashData); // Let it process
			}										// End of commandComplete check
			break;									// End of subState 4
		}											// End of switch on subState
		break;										// End of case eventRecoveryCheck:
	case EVENT_TIME_RECOVERY:
		if (pEventData->currentState != pEventData->lastState)
		{																											  // State Entry Code
			pEventData->lastState = pEventData->currentState;														  // lastState is now same as currentState... our entry point
			pEventData->subState = 1;																				  // Get read to rock and roll
			pEventData->systemData.lifeTime_days = recoveredTime.days;												  // Recover the days
			pEventData->systemData.lifeTime_milliSeconds = recoveredTime.msec;										  // Recover the milliseconds
			pEventData->spiRecoveryTimer = recoveredTime;															  // Update the recovery timer so we do not jump into the future
			setTime(recoveredTime);																					  // Update the system runtime
			pEventData->triggerDataSectorErase = convertRecordToAddress(pEventData->systemData.lastSpiRecord, FALSE); // Get the last SPI record from systemData and convert it to an address
			pEventData->triggerDataSectorErase &= SECTOR_MASK;														  // Mask it back down to the beginning of the sector
			pEventData->triggerDataSectorErase += (RECORDS_PER_SECTOR * (Uint32)SIZE_LOG_DATA);						  // Set the trigger to the last writable area in the sector
			usart_init();                                                                                             // Initialize UARTs
			bankSwitchBaudSet();                                                                                      // Set Baud Rate
#ifdef VERBOSE_DEBUG_EVENTS
#ifdef VERBOSE_DEBUG
			if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					if (timedOutDebugOutputDisplayEventTimer())
					{ 
					    log_trace("Time: %s", timeString(recoveredTime));
					    log_trace("Baud Rate: %d", pEventData->systemData.bankSwitches.bit.RS232_BAUD_RATE);
					    log_trace("Joule Count: %ld", pEventData->systemData.jouleCount);
						log_trace("Frequency Narrow Range: %d", pEventData->systemData.bankSwitches.bit.NARROW_INPUT_FREQ_RANGE);
						log_trace("THOR: %d", pEventData->systemData.bankSwitches.bit.THOR);
					}
				}
			}
#endif
#endif			
		}																											  // End of State Entry Code
		switch (pEventData->subState)
		{		// Switch our function based on subState
		case 1: // Step 1: Determine what type of RTC we are dealing with
#if defined RTC_AVAILABLE
#if defined DSP_INTERFACE
			pEventData->subState++; // Do the DSP call
#else
			pEventData->rtcTime = readRTCperipheral(); // Do the RTC Peripheral Read
			setRTCtime(pEventData->rtcTime);
			pEventData->subState = 3;
#endif
#else
			pEventData->rtcTime.Year = pEventData->systemData.rtc_Year;
			pEventData->rtcTime.mSeconds = pEventData->systemData.rtc_mSeconds;
			unPackRTCstructure(pEventData->systemData.rtc_Minute_Second, &pEventData->rtcTime.Minutes, &pEventData->rtcTime.Seconds);
			unPackRTCstructure(pEventData->systemData.rtc_Hour_DOW, &pEventData->rtcTime.Hours, &pEventData->rtcTime.DayOfWeek);
			unPackRTCstructure(pEventData->systemData.rtc_Day_Month, &pEventData->rtcTime.Day, &pEventData->rtcTime.Month);
			setRTCtime(pEventData->rtcTime);
			pEventData->subState = 3;
#endif
#ifdef VERBOSE_DEBUG_EVENTS
#ifdef VERBOSE_DEBUG
			if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					if (timedOutDebugOutputDisplayEventTimer())
					log_trace("EVENT_TIME_RECOVERY");
				}
			}
#endif
#endif
			break;
		case 2:
			pEventData->subState = 3;
			// TODO:Add DSP request for RTC data from the on board MSP430
			break;
		case 3:																  // Step 3 and default: Tidy up and move on from this place
			pEventData->viewingRecord = pEventData->systemData.lastSpiRecord; // Jump to the end of time for viewing history
		default:
			pEventData->subState = 1;			   // Reset subState for the next function
			pEventData->systemReadyForUse = TRUE;  // System is ready to go, so flag it
			pEventData->currentState = EVENT_IDLE; // Time to Idle
			break;								   // End of case 3 and default
		}										   // End of switch on subState
		break;									   // End of case EVENT_TIME_RECOVERY:
	case EVENT_IDLE:
		if (pEventData->currentState != pEventData->lastState)
		{													  // State Entry Code
			pEventData->lastState = pEventData->currentState; // lastState is now same as currentState... our entry point
			spiFlashData.currentCommand = flashNoOperation;   // Minimize time and processing in the SPI State Machine
#ifdef VERBOSE_DEBUG_EVENTS			
#ifdef VERBOSE_DEBUG
            if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					if (timedOutDebugOutputDisplayEventTimer()) 
					log_trace("EVENT_IDLE");
				}
		    }
#endif
#endif
		}													  // End of State Entry Code
		if (pEventData->numberEventsBuffered > 0)
		{												  // Do we have any pending Events to process?
			pEventData->currentState = EVENT_WRITE_FLASH; // Yes, so do it next round
		}
		else if (pEventData->numberCommandsBuffered > 0)
		{													  // Or do we have any pending Commands to process?
			pEventData->currentState = EVENT_PROCESS_COMMAND; // Yes, so do it next round
		}													  // End of action check
		break;												  // End of case eventIdle:
	case EVENT_PROCESS_COMMAND:
		if (pEventData->currentState != pEventData->lastState)
		{													  // State Entry Code
			pEventData->lastState = pEventData->currentState; // lastState is now same as currentState... our entry point
			pEventData->subState = 1;						  // Get read to rock and roll
#ifdef VERBOSE_DEBUG_EVENTS
#ifdef VERBOSE_DEBUG
			if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					if (timedOutDebugOutputDisplayEventTimer()) 
					log_trace("EVENT_PROCESS_COMMAND:substate:1");
				}
			}
#endif			
#endif
		}													  // End of State Entry Code
		switch (pEventData->subState)
		{		// Switch on subState
		case 1: // Step 1: Set SPI Interface parameters based on buffered Command
			switch (pEventData->commands[pEventData->commandReadIndex])
			{ // Switch on buffered Command
			case COMMAND_READ_FIRST:
				pEventData->viewingRecord = pEventData->systemData.firstSpiRecord;				// Update the viewingAddress location
				spiFlashData.currentCommand = flashReadRecord;									// We want to do a continuous read for the length selected
				spiFlashData.address = convertRecordToAddress(pEventData->viewingRecord, TRUE); // Set the address for flashCommunications
				spiFlashData.data = (ubyte *)&pEventData->readEvent;							// Where flashCommunications is to put the data
				spiFlashData.numberOfBytes = SIZE_LOG_DATA;										// Get the entire Event Record
				break;																			// End of case COMMAND_READ_FIRST:
			case COMMAND_READ_LAST:
				pEventData->viewingRecord = pEventData->systemData.lastSpiRecord;				// Update the viewingAddress location
				spiFlashData.currentCommand = flashReadRecord;									// We want to do a continuous read for the length selected
				spiFlashData.address = convertRecordToAddress(pEventData->viewingRecord, TRUE); // Set the address for flashCommunications
				spiFlashData.data = (ubyte *)&pEventData->readEvent;							// Where flashCommunications is to put the data
				spiFlashData.numberOfBytes = SIZE_LOG_DATA;										// Get the entire Event Record
				break;																			// End of case COMMAND_READ_LAST:
			case COMMAND_READ_CURRENT:
				spiFlashData.currentCommand = flashReadRecord;									// We want to do a continuous read for the length selected
				spiFlashData.address = convertRecordToAddress(pEventData->viewingRecord, TRUE); // Set the address for flashCommunications
				spiFlashData.data = (ubyte *)&pEventData->readEvent;							// Where flashCommunications is to put the data
				spiFlashData.numberOfBytes = SIZE_LOG_DATA;										// Get the entire Event Record
				break;																			// End of case COMMAND_READ_CURRENT:
			case COMMAND_READ_NEXT:
				pEventData->viewingRecord++;													// Bump viewingAddress up by one record size
				spiFlashData.currentCommand = flashReadRecord;									// We want to do a continuous read for the length selected
				spiFlashData.address = convertRecordToAddress(pEventData->viewingRecord, TRUE); // Set the address for flashCommunications
				spiFlashData.data = (ubyte *)&pEventData->readEvent;							// Where flashCommunications is to put the data
				spiFlashData.numberOfBytes = SIZE_LOG_DATA;										// Get the entire Event Record
				break;																			// End of case COMMAND_READ_NEXT:
			case COMMAND_READ_PREVIOUS:
				pEventData->viewingRecord--;													// Bump viewingAddress down by one record size
				spiFlashData.currentCommand = flashReadRecord;									// We want to do a continuous read for the length selected
				spiFlashData.address = convertRecordToAddress(pEventData->viewingRecord, TRUE); // Set the address for flashCommunications
				spiFlashData.data = (ubyte *)&pEventData->readEvent;							// Where flashCommunications is to put the data
				spiFlashData.numberOfBytes = SIZE_LOG_DATA;										// Get the entire Entire Record
				break;																			// End of case COMMAND_READ_PREVIOUS:
			case COMMAND_FIND_RECORD:
				pEventData->viewingRecord = pEventData->findRecord;								// Get the Record Number that we are looking for
				spiFlashData.currentCommand = flashReadRecord;									// We want to do a continuous read for the length selected
				spiFlashData.address = convertRecordToAddress(pEventData->viewingRecord, TRUE); // Set the address for flashCommunications
				spiFlashData.data = (ubyte *)&pEventData->readEvent;							// Where flashCommunications is to put the data
				spiFlashData.numberOfBytes = SIZE_LOG_DATA;										// Get the entire Entire Record
				break;																			// End of case COMMAND_FIND_RECORD:
			case COMMAND_SAVE_BANK_SWITCH_DATA:
				spiFlashData.currentCommand = flashWriteRecord;		  // We want to write the Bank Configuration Record
				spiFlashData.address = pEventData->systemAddress;	 // Set the address for flashCommunications
				spiFlashData.data = (ubyte *)&pEventData->systemData; // Where flashCommunications is to get the data from
				spiFlashData.numberOfBytes = SIZE_SYSTEM_DATA;		  // Write the entire Bank Configuration Record
				break;												  // End of case COMMAND_SAVE_BANK_SWITCH_DATA:
			case COMMAND_READ_MSP430:
				spiFlashData.currentCommand = msp430Read; // We want to do a continuous read for the length selected
				spiFlashData.data = pEventData->msp430Data.dataAddressPointer;
				spiFlashData.numberOfBytes = pEventData->msp430Data.lengthOfData; // Get the entire message, defined elsewhere
				break;															  // End of case COMMAND_READ_MSP430:
			case COMMAND_WRITE_MSP430:
				spiFlashData.currentCommand = msp430Write; // We want to write the message to the MSP430
				spiFlashData.data = pEventData->msp430Data.dataAddressPointer;
				spiFlashData.numberOfBytes = pEventData->msp430Data.lengthOfData; // Do the entire message, defined elsewhere
				break;															  // End of case COMMAND_WRITE_MSP430:
			case COMMAND_THORS_HAMMER:
				spiFlashData.currentCommand = flashThorsHammer; // Bring the Hammer down with all the wrath of Thor
				break;											// End of case COMMAND_THORS_HAMMER:
			default:
				break; // End of case COMMAND_NO_ACTION: and default
			}
			spiFlashData.commandComplete = FALSE; // Preset commandComplete to FALSE so we can execute a SPI interface command
			pEventData->subState++;				  // Ready for the next subState operation
			break;								  // End of subState 1
		case 2:									  // Step 2: Wait for commandComplete then update parameters
			if (spiFlashData.commandComplete)
			{ // Is the SPI Interface done?
				switch (pEventData->commands[pEventData->commandReadIndex])
				{ // Switch on command for cleanup
				case COMMAND_READ_FIRST:
				case COMMAND_READ_LAST:
				case COMMAND_READ_CURRENT:
				case COMMAND_READ_NEXT:
				case COMMAND_READ_PREVIOUS:
				case COMMAND_FIND_RECORD:
					pEventData->readEvent.dataReady = TRUE; // Signal the user that the data is now good
					pEventData->currentState = EVENT_IDLE;  // Now it is time to rest
					break;
				case COMMAND_READ_MSP430:
				case COMMAND_WRITE_MSP430:
					pEventData->msp430Data.dataReady = TRUE; // MSP430 data is Ready and usable
					pEventData->currentState = EVENT_IDLE;   // Now it is time to rest
					break;
				case COMMAND_THORS_HAMMER:
					defaultConfiguration();				   // Rest all values to default values
					pEventData->systemReadyForUse = TRUE;  // It is a clean system, so use it
					pEventData->currentState = EVENT_IDLE; // Now to rest
					break;								   // End of Thor's Hammer cleanup
				case COMMAND_SAVE_BANK_SWITCH_DATA:
					pEventData->currentState = EVENT_SYSTEM_SECTOR_CHECK; // We will need to check to see if we need to do a sector erase
					break;												  // End of Bank Save cleanup
				default:
					break;						// default break out
				}								// End of switch on commands for cleanup
				pEventData->commandReadIndex++; // Show we processed this command index
				if (pEventData->commandReadIndex >= EVENT_BUFFER_LENGTH)
				{												// Check to see if we are at the end of the buffer
					pEventData->commandReadIndex = 0;			// Handle rollover
				}												// End of buffer wrap check
				spiFlashData.currentCommand = flashNoOperation; // Idle the SPI Flash State Machine
				pEventData->numberCommandsBuffered--;			// Decrement the counter as we processed a command
				pEventData->subState = 1;						// Preset the subState for the next operation
			}
			else
			{										// Otherwise command not complete
				flashCommunications(&spiFlashData); // Let it process
			}										// End of commandComplete check
			break;									// End of subState 2
		}											// End of subState Switch
		break;										// End of case eventProcessCommand:
	case EVENT_WRITE_FLASH:
		if (pEventData->currentState != pEventData->lastState)
		{ // State Entry Code
			if (pEventData->writeEvents[pEventData->eventReadIndex].dataReady)
			{																									 // Was it saved after a valid configuration file retrieved?
				pEventData->lastState = pEventData->currentState;												 // Yes, so lastState is now same as currentState... our entry point
				pEventData->nextRecordNumber++;																	 // Update the current record number count
				pEventData->writeEvents[pEventData->eventReadIndex].recordNumber = pEventData->nextRecordNumber; // Update the record number on the buffered record
				pEventData->systemData.numberOfRecords++;
				if (iRange(pEventData->writeEvents[pEventData->eventReadIndex].logCode, 0, (int)(EVENT_END)))
				{
					pEventData->systemData.numberOfEvents++;																	   // Update the current event record number count
					pEventData->writeEvents[pEventData->eventReadIndex].eventRecordNumber = pEventData->systemData.numberOfEvents; // Update the record number on the buffered record
				}
				localRTCtime = getRTCtime();
				pEventData->writeEvents[pEventData->eventWriteIndex].rtc_mSeconds = localRTCtime.mSeconds;
				pEventData->writeEvents[pEventData->eventWriteIndex].rtc_Minute_Second = packRTCstructure(localRTCtime.Minutes, localRTCtime.Seconds);
				pEventData->writeEvents[pEventData->eventWriteIndex].rtc_Hour_DOW = packRTCstructure(localRTCtime.Hours, localRTCtime.DayOfWeek);
				pEventData->writeEvents[pEventData->eventWriteIndex].rtc_Day_Month = packRTCstructure(localRTCtime.Day, localRTCtime.Month);
				pEventData->writeEvents[pEventData->eventWriteIndex].rtc_Year = localRTCtime.Year;
				spiFlashData.commandComplete = FALSE;											   // Start with command not complete
				spiFlashData.currentCommand = flashWriteRecord;									   // We want to do Write record command to SPI Flash
				spiFlashData.numberOfBytes = SIZE_LOG_DATA;										   // How many characters to write on this round
				spiFlashData.address = convertRecordToAddress(pEventData->nextWriteRecord, FALSE); // Where to write the data to
				pEventData->tempWrite = spiFlashData.address;									   // Save a copy for the check coming up after this
				spiFlashData.data = (ubyte *)&pEventData->writeEvents[pEventData->eventReadIndex]; // Where is the data
			}
			else
			{ // Otherwise the event was logged before a valid configuration retrieval
				if (pEventData->systemReadyForUse)
				{																											  // Has the configuration data been retrieved yet?
					pEventData->writeEvents[pEventData->eventReadIndex].lifeTime_days = pEventData->systemData.lifeTime_days; // Yes so do a default update of time for this record
					pEventData->writeEvents[pEventData->eventReadIndex].lifeTime_milliSeconds = pEventData->systemData.lifeTime_milliSeconds;
					pEventData->writeEvents[pEventData->eventReadIndex].dataReady = TRUE; // Mark it as valid now for the next loop through to save it
				}																		  // End of configurationReadyForUse check
				break;																	  // Break out of this state without updating anything else
			}																			  // End of data time stamp valid check
#ifdef VERBOSE_DEBUG_EVENTS			
#ifdef VERBOSE_DEBUG
            if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					if (timedOutDebugOutputDisplayEventTimer())
					log_trace("EVENT_WRITE_FLASH");
				}
		    }
#endif
#endif
		}																				  // End of State Entry Code
		if (spiFlashData.commandComplete)
		{																		// Is the SPI function done?
			spiFlashData.currentCommand = flashNoOperation;						// Yes, so Idle the SPI State Machine
			pEventData->systemData.lastSpiRecord = pEventData->nextWriteRecord; // Update the last event address
			if (pEventData->writeEvents[pEventData->eventReadIndex].logCode < (int)EVENT_END)
			{
				pEventData->systemData.eventCounts[pEventData->writeEvents[pEventData->eventReadIndex].logCode]++; // Increment the event count
			}
			pEventData->eventReadIndex++; // Increment the  read index pointer
			if (pEventData->eventReadIndex >= EVENT_BUFFER_LENGTH)
			{									// Check to see if we are at the end of the buffer
				pEventData->eventReadIndex = 0; // Buffer rollover, go back to the beginning
			}									// End of buffer wrap check
			pEventData->numberEventsBuffered--; // Decrement the number of buffered events counter as we have processed one
#if defined DSP_INTERFACE
			if (upsBoss.notifyCustomerMsg == ON)
			{								// Are we to notify the customer that an event has occurred?
				upsBoss.notifyEvent = TRUE; // Yes, set the flag for ups_com to handle
			}								// End of notify check
#endif
			pEventData->currentState = EVENT_DATA_SECTOR_CHECK; // We need to check if we need to erase any memory
		}
		else
		{
			flashCommunications(&spiFlashData); // Give it a kick
		}										// End of commandComplete check
		break;									// End of case eventWriteFlash:
	case EVENT_SYSTEM_SECTOR_CHECK:
		if (pEventData->currentState != pEventData->lastState)
		{													  // State Entry Code
			pEventData->lastState = pEventData->currentState; // lastState is now same as currentState... our entry point
			pEventData->subState = 1;						  // Get read to rock and roll
		}													  // End of State Entry Code
		switch (pEventData->subState)
		{		// Switch on subState
		case 1: // Step 1: Check for Sector Erase Function needed for Bank Area
			if (pEventData->systemAddress == BANK_ADDRESS_START)
			{														// Did we just write to first location of first sector?
				spiFlashData.currentCommand = flashEraseNextSector; // Yes, so clear out the last sector
				spiFlashData.commandComplete = FALSE;				// Start with command not complete
				spiFlashData.address = SECTOR_SIZE;					// Point into start of second sector
				pEventData->subState++;								// Go to next subState to process
			}
			else if (pEventData->systemAddress == (DATA_ADDRESS_START - (Uint32)(SIZE_SYSTEM_DATA)))
			{														// Or is it the last location of the second sector?
				spiFlashData.currentCommand = flashEraseNextSector; // Yes, last write to second sector, so clear out the first sector
				spiFlashData.commandComplete = FALSE;				// Start with command not complete
				spiFlashData.address = BANK_ADDRESS_START;			// Point into start of first sector
				pEventData->subState++;								// Go to next subState to process
			}
			else
			{														 // Otherwise no worries yet...
				pEventData->subState = 1;							 // Ready the subState for the next system state usage
				pEventData->currentState = EVENT_IDLE;				 // We can Idle now
			}														 // End of Trigger check
			pEventData->systemAddress += (Uint32)(SIZE_SYSTEM_DATA); // Point to the next open space for bank configuration data
			if (pEventData->systemAddress >= DATA_ADDRESS_START)
			{													// Do we need to rollover?
				pEventData->systemAddress = BANK_ADDRESS_START; // Yes, it is wrapping around
			}													// End of rollover check
			break;												// End of subState 1
		case 2:													// Step 2: Wait for the SPI State Machine to finish
			if (spiFlashData.commandComplete)
			{													// Is it done?
				spiFlashData.currentCommand = flashNoOperation; // Yes, so Idle the SPI State Machine
				pEventData->subState = 1;						// Ready the subState for the next system state usage
				pEventData->currentState = EVENT_IDLE;			// We can go Idle now ourselves
			}
			else
			{										// Otherwise...
				flashCommunications(&spiFlashData); // Continue to process the issued command
			}										// End of commandComplete check
			break;									// End of subState 2
		}											// End of subState switch
		break;										// End of case eventBankSectorEraseCheck:
	case EVENT_DATA_SECTOR_CHECK:
		if (pEventData->currentState != pEventData->lastState)
		{													  // State Entry Code
			pEventData->lastState = pEventData->currentState; // lastState is now same as currentState... our entry point
			pEventData->subState = 1;						  // Get ready to rock and roll
#ifdef VERBOSE_DEBUG_EVENTS			
#ifdef VERBOSE_DEBUG
            if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					if (timedOutDebugOutputDisplayEventTimer()) 
					log_trace("EVENT_DATA_SECTOR_CHECK:subState:1");
				}
		    }
#endif
#endif			
		}													  // End of State Entry Code
		switch (pEventData->subState)
		{		// Switch on subState
		case 1: // Step 1: Check for Sector Erase Function needed for Data Area
			if ((pEventData->tempWrite % SECTOR_SIZE) == 0)
			{																						// Is it the first write to a new sector?
				pEventData->triggerDataSectorErase = pEventData->tempWrite;							// Set trigger to where we just wrote to
				pEventData->triggerDataSectorErase += (RECORDS_PER_SECTOR * (Uint32)SIZE_LOG_DATA); // Point it up to the end of writes for the sector
				pEventData->triggerDataSectorErase -= (Uint32)SIZE_LOG_DATA;						// Adjust for the zero memory addressing
			}																						// End of trigger update check
			if (pEventData->tempWrite == pEventData->triggerDataSectorErase)
			{																							   // Did we just write to the trigger point?
				pEventData->triggerDataSectorErase = pEventData->nextWriteRecord;						   // Yes, so make a copy of the next write record (going from address to record)
				pEventData->triggerDataSectorErase++;													   // Bump the copy up by one
				pEventData->tempWrite = convertRecordToAddress(pEventData->triggerDataSectorErase, FALSE); // Get the physical address for the next write
				spiFlashData.currentCommand = flashEraseNextSector;										   // We need to clear out the next sector pointed to by the address above
				spiFlashData.commandComplete = FALSE;													   // Start with command not complete
				spiFlashData.address = pEventData->tempWrite;											   // Point into start of next sector
				pEventData->subState++;																	   // Do it
			}
			else
			{										   // Otherwise...
				pEventData->currentState = EVENT_IDLE; // We can try and rest for now
			}										   // End of sector erase check
			pEventData->nextWriteRecord++;			   // Increment the next Write Record number (official)
			break;									   // End of subState 1
		case 2:										   // Step 2: Wait for the SPI State Machine to finish
			if (spiFlashData.commandComplete)
			{													// Is it done?
				spiFlashData.currentCommand = flashNoOperation; // Yes, so Idle the SPI State Machine
				pEventData->subState = 1;						// Ready the subState for the next system state usage
				if (pEventData->systemData.numberOfRecords > pEventData->fullRecordCount)
				{																					// Check for tail eating update of the first SPI record
					pEventData->systemData.firstSpiRecord += RECORDS_PER_SECTOR;					// Yes, eating our tail... Move the first to the next sector
				}																					// End of tail eating check
				pEventData->currentState = EVENT_IDLE;												// We can go Idle now ourselves
				pEventData->triggerDataSectorErase = pEventData->tempWrite;							// Set trigger to where we just wrote to
				pEventData->triggerDataSectorErase += (RECORDS_PER_SECTOR * (Uint32)SIZE_LOG_DATA); // Point it up to the end of writes for the sector
				pEventData->triggerDataSectorErase -= (Uint32)SIZE_LOG_DATA;						// Adjust for the zero memory addressing
			}
			else
			{										// Otherwise...
				flashCommunications(&spiFlashData); // Continue to process the issued command
			}										// End of commandComplete check
			break;									// End of subState 2
		}											// End of subState switch
		break;										// End of case eventDataSectorEraseCheck:
	case EVENT_NO_FLASH:
	default:
		NO_OPERATION; // Catch our breathe briefly, also a spot to catch on the emulator
		break;		  // End of case eventNoFlash: and default
	}				  // End of currentState switch
	return;			  // Bye
} // End of eventStateController

void checkFlashEraseAtStartup (uint8_t reset)
{
	if (reset == 1)
	{													// Do we Thor?
		spiFlashData.currentCommand = flashThorsHammer; // Set the command to bring the hammer down
		spiFlashData.commandComplete = FALSE;			// Preset commandComplete and launch
		while (spiFlashData.commandComplete == FALSE)
		{										// Sorry, but wait until SPI Flash Erased
			flashCommunications(&spiFlashData); // Push it through quickly
		}										// End of while not commandComplete
	}											// End of Thor check for startup
	pEventData->spiRecoveryTimer = getTime();
	while ((!timer(pEventData->spiRecoveryTimer, 250)) & (!pEventData->systemReadyForUse))
	{							// Sorry but wait for 250 milliseconds to get configuration data or break out if Ready for Use
		eventStateController(); // Process the command
	}							// End of while
	if (!pEventData->systemReadyForUse)
	{ // Need a little more time...
		pEventData->spiRecoveryTimer = getTime();
		while ((!timer(pEventData->spiRecoveryTimer, 200)) & (!pEventData->systemReadyForUse))
		{							// Sorry but wait for 200 milliseconds to get configuration data or break out if Ready for Use
			eventStateController(); // Process the command
		}							// End of while
	}								// End of extra time request	
} 

event_controller_data_t* getEvebtData ()
{
	return pEventData;
}
