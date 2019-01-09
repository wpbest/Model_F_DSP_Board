/*!	@file		event_display.c
 *	@author 	Argil Shaver
 *	@date		16 April 2015
 *	@version	1.00
 *	@copyright	IntelliPower, Inc. (c) 2015
 *
 *	@brief		Performs Event and Diagnostic Display functions in three output formats.
 *
 * TARGET: 		MSP430x5419, MSP430x5419A, and TMS320x2806x devices
 *
 * HISTORY:
 * -			1.00	Initial Version	16 April 2015
 */

#include "event_display.h"	// Our own header
#include "event_controller.h" // Event Controller Access
#include "string.h"			  // Standard string functions that we require
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "utilities.h" // Common Utilities that we use
#include "ups.h"

#define EVENT_MSG_MAX 40
#define EVENT_MSG_LENGTH 30
#define DIAGNOSTIC_MSG_MAX 5
#define DIAGNOSTIC_MSG_LENGTH 30

extern char diagnosticTable[DIAGNOSTIC_MSG_MAX][DIAGNOSTIC_MSG_LENGTH] = {
	"Diagnostic Test",
	"Diagnostic Type 1",
	"Diagnostic Type 2",
	"Diagnostic End",
	"Memory is Blank"};

// Map these messages to enumerated eventNum_T
extern char eventTable[EVENT_MSG_MAX][EVENT_MSG_LENGTH] = {
	"Inverter Current Shutdown",            // Inverter over-load signal from Inverter (large loads)
	"Inverter Overload Shutdown",		    // Inverter shutdown after Bypass, loads > 111%
	"Inverter Current Alarm",
	"Inverter Power Shutdown",
	"Inverter Power Alarm",
	"Inverter Fault",						// 5
	"Inverter On",
	"Inverter Off",
	"Heatsink Temperature Shutdown",
	"Heatsink Temperature Alarm",
	"Ambient Temperature Shutdown",			// 10
	"Ambient Temperature Alarm",
	"On Battery",
	"Battery Low Alarm",
	"Battery Test",
	"Battery Warning",						// 15
	"Battery Fail",
	"Chgr Ovrvolt, bats unplugged",
	"Fast Charge",
	"Float Charge",
	"Charger Off",							// 20
	"On Utility",
	"On Bypass",
	"Off Bypass",
	"Auto Bypass",
	"Cold Start",			                // 25
	"System Reset",
	"System Shutdown",
	"Battleshort On",
	"Battleshort Off",
	"Utility Over Voltage",		            // 30
	"Non Volatile Memory Error",
	"Remote Signal High",
	"Remote Signal Low",
	"Relay Signal AC On",
	"Relay Signal AC Off",                  // 35
	"Communication Timeout",
	"Test Command",	
	"Event End",
	"Memory is Blank"};                     // 39  0-39  Therefore 40 event messages

extern event_controller_data_t eventData, *pEventData;
extern uint8_t THOR;

// Prototypes
void print_Brightcom_Response(int port);
void print_Event_Response(int port);
void print_Diagnostic_Response(int port);

event_display_data_t eventDisplay, *pEventDisplay; // Make these variables accessible to the outside world
uint16_t displayFlag = FALSE;
char eventTable[EVENT_MSG_MAX][EVENT_MSG_LENGTH];
char diagnosticTable[DIAGNOSTIC_MSG_MAX][DIAGNOSTIC_MSG_LENGTH];

void initEventDisplayController(void)
{
	pEventDisplay = &eventDisplay;					  // Initialize Event Data Pointer and memory structure
	pEventDisplay->displayState = DISPLAY_FIRST_TIME; // displayState to point to the entry
	pEventDisplay->lastDisplayState = DISPLAY_EXIT;   // lastDisplayState points to end to force update
	pEventDisplay->lastDump = FALSE;				  // This is the fix for issue #164 -- no events for first 'E' cmd
	return;											  // Bye
} // End of init_Event_Controller

uint16_t eventDisplayHandler(event_display_format format, int port)
{
	switch (pEventDisplay->displayState)
	{ // What display state are we in?
	case DISPLAY_FIRST_TIME:
		if (pEventDisplay->displayState != pEventDisplay->lastDisplayState)
		{																   // State Entry Code
			pEventDisplay->lastDisplayState = pEventDisplay->displayState; // Update our state
			displayFlag = TRUE;											   // Set displayFlag to TRUE, we are processing and want control for now
		}																   // End of State Entry Code
		switch (pEventDisplay->eventDisplayFunction)
		{ // Switch on our Function request
		case EVENT_DISPLAY_RECORDS:
			if (format == FORMAT_DIAGNOSTIC)
			{																				 // Are we doing diagnostics?
				usart_putstr("\r\nTotal Diagnostics = ", port);								 // Yes, so first total number of diagnostic/events
				ltoa(pEventData->systemData.numberOfRecords, (char *)pEventDisplay->buffer); // Convert the number to a string
				usart_putstr((char *)pEventDisplay->buffer, port);							 // Print that out
				usart_putstr(" printing from ", port);										 // Now to continue from our viewing point
				if (pEventData->viewingRecord == pEventData->systemData.firstSpiRecord)
				{																	  // Are we at the beginning of time?
					pEventData->viewingRecord = pEventData->systemData.lastSpiRecord; // Yes, so jump to the end of time just for the fun of it
				}																	  // End of beginning check
				ltoa(pEventData->viewingRecord, (char *)pEventDisplay->buffer);		  // So announce from where we will begin the dumping of diagnostics
				usart_putstr((char *)pEventDisplay->buffer, port);					  // Spit it out
				usart_putstr(" at ", port);											  // and now for something completely different
				usart_putstr(rtcString(getRTCtime()), port);						  // Print out the RTC time string
				pEventDisplay->eventsTotal = pEventData->viewingRecord;				  // Load eventsTotal with the viewing record pointer... so we can print from a find
				readEvent(COMMAND_READ_CURRENT);									  // Get that record for processing later
			}
			else if (format == FORMAT_EVENT)
			{																				// Otherwise if we are doing an event...
				usart_putstr("\r\nDumping Total = ", port);									// Update the user as to our starting conditions
				ltoa(pEventData->systemData.numberOfEvents, (char *)pEventDisplay->buffer); // Print out only events
				usart_putstr((char *)pEventDisplay->buffer, port);
				usart_putstr(" Events, Printed at ", port);
				usart_putstr(runtimeString(), port);
				pEventDisplay->eventsTotal = pEventData->systemData.numberOfEvents; // Load eventsTotal with the number of event records only
				readEvent(COMMAND_READ_LAST);										// Get the last one as this is still old school
				pEventData->viewingRecord = pEventData->systemData.lastSpiRecord;   // Jump to the end of time just for the fun of it
			}
			else
			{									 // Otherwise...
				pEventDisplay->eventsTotal = 1L; // Default catch
			}									 // End of format checks and adjustments
			usart_putstr("\r\n\r\n", port);		 // Double tap the CRLF
			if (pEventDisplay->eventsTotal == (Uint32)(0))
			{														  // If zero then we adjust to prevent error
				pEventDisplay->eventsTotal = (Uint32)(1);			  // Minimum of 1 (may be blank memory)
			}														  // End of zero check
			pEventDisplay->displayState = DISPLAY_EVENTS_DIAGNOSTICS; // Go straight to the Dump Routine, do not pass go
			break;													  // End of case EVENT_DISPLAY_RECORDS: request
		case EVENT_DISPLAY_COUNTERS:
			pEventDisplay->displayState = DISPLAY_COUNTERS; // Go FORTH and print out the Event Counters
			break;											// End of case EVENT_DISPLAY_COUNTERS: request
		case EVENT_DISPLAY_ASTERISK:
			readEvent(COMMAND_READ_LAST);							  // Bright-UPS Interface requesting the Last Buffered Event
			pEventDisplay->displayState = DISPLAY_EVENTS_DIAGNOSTICS; // Next step is to print it out when ready if in event range
			break;													  // End of EVENT_DISPLAY_ASTERISK: request
		case EVENT_DISPLAY_FIRST:
			readEvent(COMMAND_READ_FIRST);							  // User requesting the First Buffered Event
			pEventDisplay->displayState = DISPLAY_EVENTS_DIAGNOSTICS; // Next step is to print it out when ready
			break;													  // End of EVENT_DISPLAY_FIRST: request
		case EVENT_DISPLAY_PLUS:
			readEvent(COMMAND_READ_PREVIOUS);						  // Bright-UPS Interface requesting the Previous Buffered Event, pointer moved
			pEventDisplay->displayState = DISPLAY_EVENTS_DIAGNOSTICS; // Next step is to print it out when ready if in event range
			break;													  // End of EVENT_DISPLAY_PLUS: request
		case EVENT_DISPLAY_MINUS:
			readEvent(COMMAND_READ_NEXT);							  // Bright-UPS Interface requesting the Next Buffered Event, pointer moved
			pEventDisplay->displayState = DISPLAY_EVENTS_DIAGNOSTICS; // Next step is to print it out when ready if in event range
			break;													  // End of EVENT_DISPLAY_MINUS: request
		case EVENT_DISPLAY_EQUAL:
			readEvent(COMMAND_READ_CURRENT);						  // Read the current, so that the pointer is unchanged
			pEventDisplay->displayState = DISPLAY_EVENTS_DIAGNOSTICS; // Next step is to print it out when ready if in event range
			break;													  // End of EVENT_DISPLAY_EQUAL: request
		case EVENT_DISPLAY_FIND:
			usart_putstr("Enter Record Number and press <enter>  ", port); // Inform user of additional information required
			pEventDisplay->displayState = DISPLAY_FIND_RECORD;			   // Time to get the data
			break;														   // End of EVENT_DISPLAY_FIND: request
		case EVENT_DISPLAY_THOR:
			usart_putstr("Password? ", port); // Prompt the user for the password
			pEventDisplay->displayState = DISPLAY_THOR;		 // Time to get the password
			break;											 // End of EVENT_DISPLAY_THOR: request
		case EVENT_DISPLAY_SET_RTC:
			usart_putstr("Setting the RTC...\r\n", port); // Prompt the user for the RTC data
			pEventDisplay->displayState = DISPLAY_RTC;	// Time to get the data
			break;										  // End of EVENT_DISPLAY_SET_RTC: request
		default:
			displayFlag = FALSE;						// Default catch set displayFlag to FALSE, unknown or illegal request
			pEventDisplay->displayState = DISPLAY_EXIT; // Run as fast as you can
			break;										// Abandon all hope
		}												// End of eventDisplayFunction switch
		break;											// End of case DISPLAY_FIRST_TIME:
	case DISPLAY_EVENTS_DIAGNOSTICS:
		if (pEventDisplay->displayState != pEventDisplay->lastDisplayState)
		{																   // State Entry Code
			pEventDisplay->lastDisplayState = pEventDisplay->displayState; // Update our state
			pEventDisplay->dumpCount = 0;								   // Nothing printed yet, so zero
			pEventDisplay->subDisplay = 1;								   // Start with the first subState
			pEventDisplay->stringArraySelect = FALSE;
		} // End of State Entry Code
		switch (pEventDisplay->subDisplay)
		{		// What shall we do today?
		case 1: // Process record when ready
			if (pEventData->readEvent.dataReady)
			{ // Check to see if we have valid data to play with
				if (usart_tx_buffer_count(port))
				{
					break;
				}															   // Prevent transmit buffer overflow
				pEventDisplay->buffer[0] = NULL;							   // "Clear" the string
				pEventDisplay->eventCode = (int)pEventData->readEvent.logCode; // Get the integer value of the enumerated Event/Diagnostic code
				if ((pEventDisplay->eventCode == 0xFFFF) & (pEventData->readEvent.recordNumber == 0xFFFFFFFF))
				{																	  // Is the memory blank?
					pEventData->readEvent.lifeTime_days = 0;						  // Blank memory would equal -1 days, so adjust it to zero
					pEventData->readEvent.lifeTime_milliSeconds = 0L;				  // Blank memory would equal -1 milliseconds, so adjust it also
					pEventData->readEvent.rtc_Day_Month = 0x0101;					  // Blank memory default is January 1st
					pEventData->readEvent.rtc_Hour_DOW = 0x0001;					  // Blank memory default is midnight on a Monday
					pEventData->readEvent.rtc_Minute_Second = 0x0000;				  // Blank memory default minutes and seconds are both zero
					pEventData->readEvent.rtc_Year = 1900;							  // Blank memory default year is 1900
					pEventData->readEvent.rtc_mSeconds = 0;							  // Blank memory default milliseconds is zero
					pEventData->viewingRecord = pEventData->systemData.lastSpiRecord; // Climb back from the edge and point to the last SPI Address
					if (format == FORMAT_DIAGNOSTIC)
					{													   // Is it a diagnostic or event?
						pEventDisplay->eventCode = DIAGNOSTIC_MSG_MAX - 1; // Oops, report it as blank memory
						pEventDisplay->stringArraySelect = TRUE;
						print_Diagnostic_Response(port);
						pEventDisplay->displayState = DISPLAY_EXIT;
						break;
					}
					else
					{
						pEventDisplay->eventCode = EVENT_MSG_MAX - 1; // Oops, report it as blank memory
						print_Event_Response(port);
						pEventDisplay->displayState = DISPLAY_EXIT;
						break;
					} // End of diagnostic or event check and set
				}	 // End of memory is blank check
				if ((format == FORMAT_BRIGHTCOM) || (format == FORMAT_EVENT))
				{ // Is it a BrightCom or Event formatted output?
					if (iRange(pEventData->readEvent.logCode, 0, (int)(EVENT_END)))
					{ // Is it an event?
						if (format == FORMAT_BRIGHTCOM)
						{												// BrightCom format?
							print_Brightcom_Response(port);				// Spit out the BrightCom information
							pEventDisplay->displayState = DISPLAY_EXIT; // BrightCom is a one shot deal, so leave this place
						}
						else
						{								// Otherwise it must be an Event format
							print_Event_Response(port); // Spit out the Event information
							if (pEventDisplay->eventDisplayFunction == EVENT_DISPLAY_RECORDS)
							{								 // Is this a dump?
								pEventDisplay->subDisplay++; // Go and check to see if there are more to handle
							}
							else
							{												// Otherwise...
								pEventDisplay->displayState = DISPLAY_EXIT; // This is a one shot deal, so leave this place
							}												// End of multiple or single record dumping check
						}													// End of format check for old school stuff
					}
					else if (iRange(pEventData->readEvent.logCode, (int)DIAGNOSTIC_TEST, (int)DIAGNOSTIC_END))
					{ // Is it a diagnostic?
						if (pEventDisplay->eventDisplayFunction == EVENT_DISPLAY_MINUS)
						{								  // Check our direction
							readEvent(COMMAND_READ_NEXT); // Continue moving forwards through the records to find event
						}
						else
						{											 // Otherwise...
							readEvent(COMMAND_READ_PREVIOUS);		 // Continue moving backwards through the records to find event
						}											 // End of directional check and adjustment
						pEventData->readEvent.logCode = FAKE_UNUSED; // Make logCode illegal so we do not do a double tap on next round through
					}
					else
					{				  // Otherwise it is unknown...
						NO_OPERATION; // Place a NOP for peace of mind, emulator break catch, and double tap catch
					}				  // End of event/diagnostic check on the available record
				}
				else
				{									 // Otherwise it must be a diagnostic
					print_Diagnostic_Response(port); // Spit out the Diagnostic information
					if (pEventDisplay->eventDisplayFunction == EVENT_DISPLAY_RECORDS)
					{								 // Is it multiple or single?
						pEventDisplay->subDisplay++; // Go and check to see if there are more to handle
					}
					else
					{												// Otherwise...
						pEventDisplay->displayState = DISPLAY_EXIT; // This is a one shot deal, so leave this place
					}												// End of multiple or single record dumping check
				}													// End of display format selection
			}														// End of dataReady check
			if (pEventDisplay->lastDump)
			{															 // Check to see if we are at the end of the dump list
				pEventDisplay->lastDump = FALSE;						 // Reset the lastDump Flag for safety
				pEventDisplay->displayState = DISPLAY_EXIT_WITH_MESSAGE; // Our time is done, ready to leave
			}															 // End of lastDump check
			break;														 // End of subState 1
		case 2:															 // subState 2 : Process multiple dump output
			pEventDisplay->dumpCount++;									 // Increment the dumpCount(er)
			pEventDisplay->eventsTotal--;								 // Decrease the events processed
			pEventDisplay->buffer[0] = NULL;							 // "Clear" the string
			if (pEventDisplay->dumpCount < EVENTS_PER_DUMP)
			{ // Check if our count is less than the number of events per dump
				if (pEventDisplay->eventsTotal == (Uint32)(0))
				{									// Are we done?
					pEventDisplay->lastDump = TRUE; // Yes we are
				}									// End of zero check for we are done
				readEvent(COMMAND_READ_PREVIOUS);   // Do the dirty deed
				pEventDisplay->subDisplay = 1;		// Jump back and wait for it
			}
			else
			{ // Otherwise...
				if (pEventDisplay->eventsTotal == (Uint32)(0))
				{															 // Are we done?
					pEventDisplay->displayState = DISPLAY_EXIT_WITH_MESSAGE; // Our time is done, ready to leave
				}
				else
				{													 // Otherwise
					usart_putstr("Press any key, <Q>uit\r\n", port); // Listing complete prompt user for action
					pEventDisplay->subDisplay++;					 // Go and get input from user
				}													 // End of done or continue check
			}														 // End of dump count checking
			break;													 // End of subState 2
		case 3:														 // subState 3 : Get input from the user
			if (usart_rx_buffer_count(port) != 0)
			{												   // Hold here in state machine until we have a pending character
				pEventDisplay->tempChar = usart_getchar(port); // Get the character from the serial port
				switch (pEventDisplay->tempChar)
				{ // What shall we do today?
				case 'Q':
				case 'q':													 // Are we going to quit?
					pEventDisplay->displayState = DISPLAY_EXIT_WITH_MESSAGE; // Do our exit cleanup
					break;													 // Done so run away
				default:													 // Anything else we continue
					pEventDisplay->dumpCount = 0;							 // Reset the count for lines to display
					readEvent(COMMAND_READ_PREVIOUS);						 // Do the dirty deed
					pEventDisplay->subDisplay = 1;							 // Jump back and wait for it
				}															 // End of input character switch
			}																 // End of wait for buffer empty check
			break;															 // End of subState 3
		default:															 // Just for standard compliance
			pEventDisplay->displayState = DISPLAY_EXIT;						 // Abandon all hope
			break;															 // Run you clever boy
		}																	 // End of subState switch
		break;																 // End of case DISPLAY_EVENTS_DIAGNOSTICS:
	case DISPLAY_COUNTERS:
		if (pEventDisplay->displayState != pEventDisplay->lastDisplayState)
		{																   // State Entry Code
			pEventDisplay->lastDisplayState = pEventDisplay->displayState; // Update our state
			pEventDisplay->dumpCount = 0;								   // Nothing printed yet, so zero
			pEventDisplay->counterIndex = 0;							   // Set counter index pointer to zero
			pEventDisplay->subDisplay = 1;								   // Start with the first subState
		}																   // End of State Entry Code
		switch (pEventDisplay->subDisplay)
		{		// What shall we do today?
		case 1: // Print out a line of the Event Counters
			if (usart_tx_buffer_count(port))
			{
				break;
			}																									 // Prevent transmit buffer overflow
			pEventDisplay->buffer[0] = NULL;																	 // Initialize the buffer with a NULL
			strcpy((char *)pEventDisplay->buffer, eventTable[pEventDisplay->counterIndex]);						 // Get the counter name text based on the counter index
			tabWPeriods(33, (char *)pEventDisplay->buffer);														 // Tab it with periods to fill
			ltoa(pEventData->systemData.eventCounts[pEventDisplay->counterIndex], (char *)pEventDisplay->check); // Convert the count to a string
			strcat((char *)pEventDisplay->buffer, (char *)pEventDisplay->check);								 // Add it to the output buffer string
			strcat((char *)pEventDisplay->buffer, "\r\n");														 // Both are terminated with a CR LF
			usart_putstr((char *)pEventDisplay->buffer, port);													 // Spit it out where told
			if (pEventDisplay->lastDump)
			{															 // Check for last dump
				pEventDisplay->lastDump = FALSE;						 // Reset the flag
				pEventDisplay->displayState = DISPLAY_EXIT_WITH_MESSAGE; // Gracefully exit with a goodbye message
			}
			else
			{								   // Otherwise...
				pEventDisplay->dumpCount++;	// Increment the dumpCount
				pEventDisplay->counterIndex++; // Increment the Counter Index Pointer
				if (pEventDisplay->dumpCount < EVENTS_PER_DUMP)
				{ // Check for end of dump
					if (pEventDisplay->counterIndex >= ((int)(EVENT_END)-1))
					{									// Check for end of counter array
						pEventDisplay->lastDump = TRUE; // If it is the end, then set lastDump flag for exit
					}									// End of counter array check
				}
				else
				{													 // Otherwise...
					usart_putstr("Press any key, <Q>uit\r\n", port); // Dump end reached, announce
					pEventDisplay->subDisplay++;					 // Go and wait for user input
				}													 // End of dump check
			}														 // End of lastDump check
			break;													 // End of subState 1
		case 2:														 // subState 2 : Get user input
			if (usart_rx_buffer_count(port) != 0)
			{												   // Hold here in state machine until we have a pending character
				pEventDisplay->tempChar = usart_getchar(port); // Get the character from the serial port
				switch (pEventDisplay->tempChar)
				{ // What shall we do today?
				case 'Q':
				case 'q':													 // Are we going to quit?
					pEventDisplay->displayState = DISPLAY_EXIT_WITH_MESSAGE; // Do our exit cleanup
					break;													 // Done so run away
				default:													 // Anything else means we continue
					pEventDisplay->dumpCount = 0;							 // Reset the count for lines to display
					pEventDisplay->subDisplay--;							 // Jump back and wait for it
				}															 // End of input character switch
			}																 // End of wait for buffer empty check
			break;															 // End of subState 2
		default:															 // Just for standard compliance
			break;															 // Run
		}																	 // End of subState switch
		break;																 // End of case DISPLAY_COUNTERS:
	case DISPLAY_THOR:
		if (pEventDisplay->displayState != pEventDisplay->lastDisplayState)
		{																   // State Entry Code
			pEventDisplay->lastDisplayState = pEventDisplay->displayState; // Update our state
			pEventDisplay->buffer[0] = NULL;							   // Make ready the buffer for use
			pEventDisplay->charPointer = 0;								   // Move the character pointer to the first position
		}																   // End of State Entry Code
		if (usart_rx_buffer_count(port) != 0)
		{												   // Do we have a character?
			pEventDisplay->tempChar = usart_getchar(port); // Yes, so grab it for checks and processing
			if (pEventDisplay->tempChar == 0x0D)
			{ // Is it a carriage return?
				if (strcmp(pEventDisplay->buffer, "THOR") == 0)
				{
					THOR = TRUE;																			  // Yes, so check the string for the correct password
				    usart_putstr("\r\n", port);
					usart_putstr("Clearing Events and History in RAM / external FLASH\r\n", port); // We have liftoff
					nonDataEventCommand(COMMAND_THORS_HAMMER);	// Throw down the hammer
					usart_putstr("Done\r\n", port);
					usart_putstr("Time will be reset after the UPS is powered off\r\n", port);
				}
				else
				{											  // Otherwise...
					usart_putstr("Sorry, Charlie\r\n", port); // Loud nasty buzzing sound
				}											  // End of password check
				pEventDisplay->displayState = DISPLAY_EXIT;   // Next state is to exit
			}
			else if ((pEventDisplay->tempChar >= ' ') && (pEventDisplay->tempChar <= '~'))
			{																				   // Check to see if it is a valid data range
				pEventDisplay->buffer[pEventDisplay->charPointer++] = pEventDisplay->tempChar; // Yes, so add it to the string and bump the character pointer
				usart_putchar(pEventDisplay->tempChar, port); // Echo back to the user the accepted character
				pEventDisplay->buffer[pEventDisplay->charPointer] = NULL; // Terminate with null so string processes correctly in function strcmp
			}															  // End of valid data range check
		}																  // End of character check
		break;															  // End of DISPLAY_THOR:
	case DISPLAY_FIND_RECORD:
		if (pEventDisplay->displayState != pEventDisplay->lastDisplayState)
		{																   // State Entry Code
			pEventDisplay->lastDisplayState = pEventDisplay->displayState; // Update our state
			pEventDisplay->charPointer = 0;								   // Set the character pointer into the array
			pEventDisplay->buffer[0] = NULL;							   // "Clear" the string
			pEventDisplay->subDisplay = 1;								   // Start with the first subState
		}																   // End of State Entry Code
		switch (pEventDisplay->subDisplay)
		{		// What shall we do today?
		case 1: // subState 1 : Collect some data
			if (usart_rx_buffer_count(port) != 0)
			{												   // Do we have a character?
				pEventDisplay->tempChar = usart_getchar(port); // Yes, so grab it for checks and processing
				if (pEventDisplay->tempChar == 0x0D)
				{																			 // Is it a carriage return?
					pEventDisplay->tempLong = (Uint32)(atol((char *)pEventDisplay->buffer)); // Yes, so convert the string to a long value
					findEvent(pEventDisplay->tempLong);										 // Issue the command to go and find the record
					usart_putstr("\r\n", port);												 // Echo a carriage return and line feed to the user
					pEventDisplay->subDisplay++;											 // Display the record...
				}
				else if ((pEventDisplay->tempChar >= '0') && (pEventDisplay->tempChar <= '9'))
				{																				   // Only collect numbers for the conversion
					pEventDisplay->buffer[pEventDisplay->charPointer++] = pEventDisplay->tempChar; // Save the number and bump the pointer
					usart_putchar(pEventDisplay->tempChar, port);								   // Echo it back so they can see what they typed
					pEventDisplay->buffer[pEventDisplay->charPointer] = NULL;					   // Terminate with null so string processes correctly in function atol
					if (pEventDisplay->charPointer > 11)
					{												  // Check to see if user is asleep at the keyboard
						usart_putstr("Not a valid number\r\n", port); // Runaway from this mess
						pEventDisplay->displayState = DISPLAY_EXIT;
					} // End of overrun check
				}	 // End of numbers only check
			}		  // End of character available check
			break;	// End of subState 1
		case 2:		  // subState 2 : Get the record and print it
			if (pEventData->readEvent.dataReady)
			{												// Check to see if we have valid data to play with
				print_Diagnostic_Response(port);			// Print it new age style
				pEventDisplay->displayState = DISPLAY_EXIT; // It is OK to leave now
			}												// End of dataReady check
			break;											// End of subState 2
		default:											// Just for standard compliance
			break;											// Run
		}													// End of subState switch
		break;												// Done with case DISPLAY_FIND_RECORD:
	case DISPLAY_RTC:
		if (pEventDisplay->displayState != pEventDisplay->lastDisplayState)
		{																   // State Entry Code
			pEventDisplay->lastDisplayState = pEventDisplay->displayState; // Update our state
			pEventDisplay->charPointer = 0;								   // Set the character pointer into the array
			pEventDisplay->buffer[0] = NULL;							   // "Clear" the string
			pEventDisplay->subAnnounce = TRUE;							   // Announce then get
			pEventDisplay->subRtcParse = 1;								   // Start off with the Year first
			pEventDisplay->subDisplay = 1;								   // Start with the first subState
		}																   // End of State Entry Code
		switch (pEventDisplay->subDisplay)
		{		// What shall we do today?
		case 1: // subState 1 : Collect and process some data for the RTC
			if (pEventDisplay->subAnnounce)
			{ // Is it time to announce our entrance?
				switch (pEventDisplay->subRtcParse)
				{ // Yes, What part of the RTC is being processed
				case 1:
					usart_putstr("Enter the Year... ", port); // We want the Year of the RTC
					break;									  // End of subRtcParse 1
				case 2:
					usart_putstr("Enter the Month... ", port); // We want the Month of the RTC
					break;									   // End of subRtcParse 2
				case 3:
					usart_putstr("Enter the Day... ", port); // We want the Day of the RTC
					break;									 // End of subRtcParse 3
				case 4:
					usart_putstr("Enter the Day of the Week (0=Sun, Sat=7)... ", port); // We want the Day Of Week of the RTC
					break;																// End of subRtcParse 4
				case 5:
					usart_putstr("Enter the Hours (24H format)... ", port); // We want the Hour of the RTC
					break;													// End of subRtcParse 5
				case 6:
					usart_putstr("Enter the Minutes... ", port); // We want the Minute of the RTC
					break;										 // End of subRtcParse 6
				case 7:
					usart_putstr("Enter the Seconds... ", port); // We want the Second of the RTC
					break;										 // End of subRtcParse 7
				default:
					break; // End of subRtcParse default
				}
				pEventDisplay->subAnnounce = FALSE; // Lock out announcement for now
				pEventDisplay->subDisplay = 3;		// Go FORTH and get the data
			}
			else
			{ // Otherwise...
				switch (pEventDisplay->subRtcParse)
				{ // What part of the RTC are we updating
				case 1:
					pEventData->rtcTime.Year = atoi((char *)pEventDisplay->buffer); // We have data so convert the string to an integer value
					if (pEventData->rtcTime.Year < 2015)
					{ // Year check
						pEventData->rtcTime.Year = 2015;
					}
					break; // End of subRtcParse 1
				case 2:
					pEventData->rtcTime.Month = atoi((char *)pEventDisplay->buffer); // We have data so convert the string to an integer value
					if (pEventData->rtcTime.Month > 12)
					{ // Month check
						pEventData->rtcTime.Month = 12;
					}
					if (pEventData->rtcTime.Month < 1)
					{
						pEventData->rtcTime.Month = 1;
					}
					break; // End of subRtcParse 2
				case 3:
					pEventData->rtcTime.Day = atoi((char *)pEventDisplay->buffer); // We have data so convert the string to an integer value
					if (pEventData->rtcTime.Day > 31)
					{ // Day check
						pEventData->rtcTime.Day = 31;
					}
					if (pEventData->rtcTime.Day < 1)
					{
						pEventData->rtcTime.Day = 1;
					}
					break; // End of subRtcParse 3
				case 4:
					pEventData->rtcTime.DayOfWeek = atoi((char *)pEventDisplay->buffer); // We have data so convert the string to an integer value
					if (pEventData->rtcTime.DayOfWeek > 6)
					{ // Day Of Week check
						pEventData->rtcTime.Day = 6;
					}
					if (pEventData->rtcTime.Day < 1)
					{
						pEventData->rtcTime.Day = 0;
					}
					break; // End of subRtcParse 4
				case 5:
					pEventData->rtcTime.Hours = atoi((char *)pEventDisplay->buffer); // We have data so convert the string to an integer value
					if (pEventData->rtcTime.Hours > 23)
					{ // Hour check
						pEventData->rtcTime.Hours = 23;
					}
					if (!iRange(pEventData->rtcTime.Hours, 0, 23))
					{
						pEventData->rtcTime.Hours = 0;
					}
					break; // End of subRtcParse 5
				case 6:
					pEventData->rtcTime.Minutes = atoi((char *)pEventDisplay->buffer); // We have data so convert the string to an integer value
					if (pEventData->rtcTime.Minutes > 59)
					{ // Minute check
						pEventData->rtcTime.Minutes = 59;
					}
					if (!iRange(pEventData->rtcTime.Minutes, 0, 59))
					{
						pEventData->rtcTime.Minutes = 0;
					}
					break; // End of subRtcParse 6
				case 7:
					pEventData->rtcTime.Seconds = atoi((char *)pEventDisplay->buffer); // We have data so convert the string to an integer value
					if (pEventData->rtcTime.Seconds > 59)
					{ // Second check
						pEventData->rtcTime.Seconds = 59;
					}
					if (!iRange(pEventData->rtcTime.Seconds, 0, 59))
					{
						pEventData->rtcTime.Seconds = 0;
					}
					pEventDisplay->subDisplay++; // Ready to set the RTC time now
					break;						 // End of subRtcParse 7
				default:
					break;						   // End of subRtcParse default
				}								   // End of entrance announcement check
				pEventDisplay->subAnnounce = TRUE; // Let the next state make an entrance
				pEventDisplay->subRtcParse++;
				pEventDisplay->charPointer = 0;  // Set the character pointer into the array
				pEventDisplay->buffer[0] = NULL; // "Clear" the string
			}
			break; // End of subState 1
		case 2:
			setRTCtime(pEventData->rtcTime); // Update fake RTC
#if defined RTC_AVAILABLE
#if defined DSP_INTERFACE
				// Do the DSP call for an off processor write for real RTC
#else
			setRTCperipheral(pEventData->rtcTime); // Do the RTC Write for the real one
#endif
#endif
			pEventDisplay->displayState = DISPLAY_EXIT_WITH_MESSAGE;
			break; // End of subState 2
		case 3:	// subState 3 : Collect some data
			if (usart_rx_buffer_count(port) != 0)
			{												   // Do we have a character?
				pEventDisplay->tempChar = usart_getchar(port); // Yes, so grab it for checks and processing
				if (pEventDisplay->tempChar == 0x0D)
				{								   // Is it a carriage return?
					usart_putstr("\r\n", port);	// Echo a carriage return and line feed to the user
					pEventDisplay->subDisplay = 1; // Return to set the data...
				}
				else if ((pEventDisplay->tempChar >= '0') && (pEventDisplay->tempChar <= '9'))
				{																				   // Only collect numbers for the conversion
					pEventDisplay->buffer[pEventDisplay->charPointer++] = pEventDisplay->tempChar; // Save the number and bump the pointer
					usart_putchar(pEventDisplay->tempChar, port);								   // Echo it back so they can see what they typed
					pEventDisplay->buffer[pEventDisplay->charPointer] = NULL;					   // Terminate with null so string processes correctly in function atol
					if (pEventDisplay->charPointer > 4)
					{												  // Check to see if user is asleep at the keyboard
						usart_putstr("Not a valid number\r\n", port); // Runaway from this mess
						pEventDisplay->displayState = DISPLAY_EXIT;   // I want to vomit
					}												  // End of overrun check
				}													  // End of numbers only check
			}														  // End of character available check
			break;													  // End of subState 3
		default:													  // Just for standard compliance
			break;													  // Run
		}															  // End of subState switch
		break;														  // Done with case DISPLAY_RTC:
	case DISPLAY_EXIT_WITH_MESSAGE:
		usart_putstr("*** Done ***\r\n", port); // User decided to end the process or automated end
	case DISPLAY_EXIT:
	default:
		pEventDisplay->lastDisplayState = DISPLAY_EXIT; // Update our state
		pEventDisplay->displayState = DISPLAY_FIRST_TIME;
		displayFlag = FALSE; // We are done or something bad happened... either way we will release control now
		break;
	}					// End of switch on displayState
	return displayFlag; // Return flag indicating if we keep control or not
} // End of eventDisplayHandler

void print_Brightcom_Response(int port)
{
	ltoa(pEventData->readEvent.eventRecordNumber, (char *)pEventDisplay->buffer);	 // Translate the event record number into readable format
	strcat((char *)pEventDisplay->buffer, ",");										  // Old school... comma delimited
	strcat((char *)pEventDisplay->buffer, eventTable[pEventDisplay->eventCode]);	  // Add text for translated enumerated event code
	strcat((char *)pEventDisplay->buffer, ",");										  // Comma delimited
	strcat((char *)pEventDisplay->buffer, itoa(pEventData->readEvent.lifeTime_days)); // Add the number of days in readable text
	strcat((char *)pEventDisplay->buffer, ",");										  // Comma delimited
	ltoa(pEventData->readEvent.lifeTime_milliSeconds, (char *)pEventDisplay->check);  // Use temporary storage to convert milliseconds into text
	strcat((char *)pEventDisplay->buffer, (char *)pEventDisplay->check);			  // Add it to the overall string
	strcat((char *)pEventDisplay->buffer, "\r\n");									  // Both are terminated with a CR LF
	usart_putstr((char *)pEventDisplay->buffer, port);								  // Spit it out where told
	return;																			  // Bye
} // End of print_Brightcom_Response

void print_Event_Response(int port)
{
	ltoa(pEventData->readEvent.eventRecordNumber, (char *)pEventDisplay->buffer); // Translate the record number into readable format
	tabWPeriods(12, (char *)pEventDisplay->buffer);								  // Tab it with periods
	strcat((char *)pEventDisplay->buffer, eventTable[pEventDisplay->eventCode]);  // Add text for translated enumerated event code
	tabWPeriods(45, (char *)pEventDisplay->buffer);								  // Tab it with periods
	strcat((char *)pEventDisplay->buffer, " @ ");								  // Text fill for indication
	pEventDisplay->tempTime.days = pEventData->readEvent.lifeTime_days;			  // Get the days
	pEventDisplay->tempTime.msec = pEventData->readEvent.lifeTime_milliSeconds;   // Get the milliseconds
	strcat((char *)pEventDisplay->buffer, timeString(pEventDisplay->tempTime));   // Format and add the time string to overall string
	strcat((char *)pEventDisplay->buffer, "\r\n");								  // Both are terminated with a CR LF
	usart_putstr((char *)pEventDisplay->buffer, port);							  // Spit it out where told
	return;																		  // Bye
} // End of print_Event_Response

void formatDiagnosticData(void)
{ // This is where we make it pretty for the various conditions
	switch (pEventData->readEvent.logCode)
	{ // Select output display based on logCode
	default:
		strcat((char *)pEventDisplay->buffer, itoa(pEventData->readEvent.event_diagnostic_data_1)); // Default is to print the integer value
		strcat((char *)pEventDisplay->buffer, ", ");												// with comma separation
		strcat((char *)pEventDisplay->buffer, itoa(pEventData->readEvent.event_diagnostic_data_2)); // Default is to print the integer value
		strcat((char *)pEventDisplay->buffer, ", ");												// with comma separation
		strcat((char *)pEventDisplay->buffer, itoa(pEventData->readEvent.event_diagnostic_data_3)); // Default is to print the integer value
		strcat((char *)pEventDisplay->buffer, ", ");												// with comma separation
		strcat((char *)pEventDisplay->buffer, itoa(pEventData->readEvent.event_diagnostic_data_4)); // Default is to print the integer value
		break;																						// End of default
	}																								// End of switch on logCode
	tabWPeriods(55, (char *)pEventDisplay->buffer);													// Tab it with periods to the next display location
	return;																							// Bye
} // End of formatDiagnosticData

void print_Diagnostic_Response(int port)
{
	ltoa(pEventData->readEvent.recordNumber, (char *)pEventDisplay->buffer); // Translate the record number into readable format
	tabWPeriods(12, (char *)pEventDisplay->buffer);							 // Tab it with periods
	if (pEventDisplay->stringArraySelect)
	{
		pEventDisplay->stringArraySelect = FALSE;
		strcat((char *)pEventDisplay->buffer, diagnosticTable[pEventDisplay->eventCode]); // Add text for translated enumerated diagnostic code of blank memory
	}
	else
	{
		if (iRange(pEventDisplay->eventCode, 0, (int)(EVENT_END)))
		{
			strcat((char *)pEventDisplay->buffer, eventTable[pEventDisplay->eventCode]); // Add text for translated enumerated event code
		}
		else if (iRange(pEventDisplay->eventCode, (int)DIAGNOSTIC_TEST, (int)DIAGNOSTIC_END))
		{																					  // Is it a diagnostic?
			pEventDisplay->eventCode -= (int)DIAGNOSTIC_TEST;								  // Bring the pointer back into the array (oh my)
			strcat((char *)pEventDisplay->buffer, diagnosticTable[pEventDisplay->eventCode]); // Add text for translated enumerated diagnostic code
		}
		else
		{
			strcat((char *)pEventDisplay->buffer, "Error Translating");
		}
	}
	tabWPeriods(45, (char *)pEventDisplay->buffer); // Tab it with periods
	formatDiagnosticData();
	strcat((char *)pEventDisplay->buffer, " at "); // Text fill for indication
	pEventDisplay->rtcTime.mSeconds = pEventData->readEvent.rtc_mSeconds;
	pEventDisplay->rtcTime.Year = pEventData->readEvent.rtc_Year;
	unPackRTCstructure(pEventData->readEvent.rtc_Day_Month, &pEventDisplay->rtcTime.Day, &pEventDisplay->rtcTime.Month);
	unPackRTCstructure(pEventData->readEvent.rtc_Hour_DOW, &pEventDisplay->rtcTime.Hours, &pEventDisplay->rtcTime.DayOfWeek);
	unPackRTCstructure(pEventData->readEvent.rtc_Minute_Second, &pEventDisplay->rtcTime.Minutes, &pEventDisplay->rtcTime.Seconds);
	strcat((char *)pEventDisplay->buffer, rtcString(pEventDisplay->rtcTime)); // Format and add the time string to overall string
	strcat((char *)pEventDisplay->buffer, "\r\n");							  // Both are terminated with a CR LF
	usart_putstr((char *)pEventDisplay->buffer, port);						  // Spit it out where told
	return;																	  // Bye
} // End of print_Diagnostic_Response
