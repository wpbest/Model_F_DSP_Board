/*!	@file		event_display.h
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

#ifndef EVENT_DISPLAY_H_
#define EVENT_DISPLAY_H_

#include "types.h"
#include "timer.h"
#include "uart.h" // Serial Port functions

//! Number of events to print per dump request
#define EVENTS_PER_DUMP (15)

//! Number of milliseconds before we declare a command time out condition
#define EVENTS_COMMAND_TIMEOUT (2000)

typedef enum
{
	EVENT_DISPLAY_RECORDS,
	EVENT_DISPLAY_COUNTERS, //! Event Display Function to Process and Display the Event Counters
	EVENT_DISPLAY_ASTERISK, //! Event Display Function to Process and Display the Last Event/Diagnostic buffered
	EVENT_DISPLAY_FIRST,	//! Event Display Function to Process and Display the First Event/Diagnostic buffered
	EVENT_DISPLAY_PLUS,		//! Event Display Function to Process and Display the Previous Event/Diagnostic buffered
	EVENT_DISPLAY_MINUS,	//! Event Display Function to Process and Display the Next Event/Diagnostic buffered
	EVENT_DISPLAY_EQUAL,	//! Event Display Function to Process and Display the Current Event/Diagnostic
	EVENT_DISPLAY_FIND,		//! Event Display Function to Process and Display the requested Diagnostic Record
	EVENT_DISPLAY_THOR,		//! Event Display Function to Process and perform a Mass Erase of the External FLASH device
	EVENT_DISPLAY_SET_RTC   //! Event Display Function to Process and Set the Real Time Clock
} event_display_function;

typedef enum
{
	DISPLAY_FIRST_TIME,
	DISPLAY_THOR,
	DISPLAY_FIND_RECORD,
	DISPLAY_EVENTS_DIAGNOSTICS,
	DISPLAY_COUNTERS,
	DISPLAY_RTC,
	DISPLAY_EXIT_WITH_MESSAGE,
	DISPLAY_EXIT
} event_display_state;

typedef enum
{
	FORMAT_BRIGHTCOM,  //! Output Data will be made in the BrightCom format
	FORMAT_EVENT,	  //! Output Data will be made in the old school Event Dump format
	FORMAT_DIAGNOSTIC, //! Output Data will be made in the new Event/Diagnostic with RTC format
	FORMAT_UNKNOWN	 //! No output... error
} event_display_format;

typedef struct _event_display_data_t
{
	event_display_function eventDisplayFunction; //! What function to perform in the Event Display function
	uint16_t lastDump;							 //! Boolean flag to indicate if this is the last dump of information or not
	uint16_t counterIndex;
	uint16_t dumpCount;
	Uint32 eventsTotal;
	Uint32 tempLong;
	timeT tempTime;
	rtc_timeT rtcTime;
	volatile char check[24];
	char buffer[128];
	char tempChar;
	int eventCode;
	uint16_t subDisplay;
	uint16_t subRtcParse;
	uint16_t subAnnounce;
	uint16_t stringArraySelect;
	int charPointer;
	event_display_state displayState;
	event_display_state lastDisplayState;
} event_display_data_t;

extern event_display_data_t eventDisplay, *pEventDisplay; // Make these variables accessible to the outside world
extern uint16_t displayFlag;

void initEventDisplayController(void);
uint16_t eventDisplayHandler(event_display_format format, int port);

#endif /* EVENT_DISPLAY_H_ */
