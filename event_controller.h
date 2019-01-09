/*!	@file		event_controller.h
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
#ifndef EVENT_CONTROLLER_H_
#define EVENT_CONTROLLER_H_

#include "hal_spi_flash.h"
#include "types.h"

//! Number of event records to hold for writing out to the flash
#define EVENT_BUFFER_LENGTH (16)
#define MAX_MSP430_STRING (32)
//! Define the Record Key Version value for the data structures
#define NONEVENT_KEY_VERSION (0xAA04)

//! Define EventData Record Key Version value for the data structures
#define EVENT_KEY_VERSION (0x5507)

#define FAKE_UNUSED (0xFFFF)
#if defined DSP_INTERFACE
#define NO_OPERATION NOP; // Platform Specific Definition for a No Operation Command
#endif
#if defined MSP430_INTERFACE
#define NO_OPERATION _NOP(); // Platform Specific Definition for a No Operation Command
#endif

//! Automatically calculate the padding for the system data structure
//					Record Size - Number of Longs * 4 - Number of Integers * 2 - Number of Longs in Events * 4 all divided by two
#define SIZE_SYSTEM_DATA (256)
#define SYSTEM_PAD ((SIZE_SYSTEM_DATA - ((8 * 4) + (8 * 2) + ((int)EVENT_END * 4))) / 2) // 34 - Integers left in the configuration record
#define SIZE_LOG_DATA (36)																 // Do not save the flag in the data field
#define ORPHAN_BYTES (SECTOR_SIZE % (Uint32)(SIZE_LOG_DATA))
#define RECORDS_PER_SECTOR (SECTOR_SIZE / (Uint32)(SIZE_LOG_DATA))

//! Definition of the Event data record for Flash storage (38-bytes)
typedef struct loggingData
{
	Uint32 recordNumber;			  //! Incremental Long Count of Flash Saves for total Event/Diagnostic Data Record
	Uint32 eventRecordNumber;		  //! Incremental Long Count of Flash Saves of Events ONLY  Data Record
	uint16_t logCode;				  //! Integer value of the event defined in eventNum_T or diagnostic defined in diagnosticNum_T
	uint16_t lifeTime_days;			  //! Integer value of Life Time Run Days
	Uint32 lifeTime_milliSeconds;	 //! Long value of Life Time Run milliSeconds (rolls over to days and resets)
	uint16_t rtc_mSeconds;			  //! Integer value of RTC data for milliseconds
	uint16_t rtc_Minute_Second;		  //! Integer value of packed RTC data for Minutes and Seconds
	uint16_t rtc_Hour_DOW;			  //! Integer value of packed RTC data for Hours and Day Of Week (DOW)
	uint16_t rtc_Day_Month;			  //! Integer value of packed RTC data for Day and Month
	uint16_t rtc_Year;				  //! Integer value of RTC data for the Year
	uint16_t recordKeyVersion;		  //! Event Data Key/Version identifier for data validation of records
	uint16_t event_diagnostic_data_1; //! May hold data for event or diagnostic
	uint16_t event_diagnostic_data_2; //! May hold data for event or diagnostic
	uint16_t event_diagnostic_data_3; //! May hold data for event or diagnostic
	uint16_t event_diagnostic_data_4; //! May hold data for event or diagnostic
	uint16_t dataReady;				  //! Flag used in Read operations to indicate a valid record
} loggingData_t;

//! Definition of the system data record for Flash storage (256-bytes)
typedef struct systemData
{
	Uint32 recordNumber;				//! Incremental Count of Flash Saves for System Configuration Data Record
	BankSwitchConfigurationBits_t bankSwitches;	//! Bank Switch Configuration Data, can be remapped into BANK_SW_CONFIG_T
	Uint32 lastSpiRecord;				//! Unsigned long pointing to the last  Event record saved
	Uint32 firstSpiRecord;				//! Unsigned long pointing to the first Event record saved
	Uint32 numberOfRecords;				//! Unsigned long indicating total number of Event/Diagnostic Records stored
	Uint32 numberOfEvents;				//! Unsigned long indicating total number of Event Records saved
	Uint32 eventCounts[(int)EVENT_END]; //! Running count of number of times an event has occurred
	Uint32 jouleCount;					//! Running Joule Count for some operations
	Uint32 lifeTime_milliSeconds;		//! Long value of Life Time Run milliSeconds (rolls over to days and resets)
	uint16_t lifeTime_days;				//! Integer value of Life Time Run Days
	uint16_t snmpUid;					//! SNMP Unit ID in case we have more than one unit networked
	uint16_t recordKeyVersion;			//! Configuration Data Key/Version identifier for data validation of records
	uint16_t rtc_mSeconds;				//! Integer value of RTC data for milliseconds
	uint16_t rtc_Minute_Second;			//! Integer value of packed RTC data for Minutes and Seconds
	uint16_t rtc_Hour_DOW;				//! Integer value of packed RTC data for Hours and Day Of Week (DOW)
	uint16_t rtc_Day_Month;				//! Integer value of packed RTC data for Day and Month
	uint16_t rtc_Year;					//! Integer value of RTC data for the Year
	uint16_t reserved[SYSTEM_PAD];		//! Padding to make the record fit nicely into 4k blocks as 256-byte records
} systemData_t;

//! Definition of the MSP430 data access structure for the DSP interface
typedef struct msp430Data
{
	uint8_t *dataAddressPointer; //! Address to reference data transfer to and from calling program
	int lengthOfData;			 //! Length of the transfer
	uint16_t dataReady;			 //! Flag used in Read operations to indicate a valid record
} msp430Data_t;

typedef enum  eventControllerCommand
{
	COMMAND_READ_FIRST,			   //! Read the oldest logged Event (always points to first event wrote) uses struct runningEventData_t
	COMMAND_READ_LAST,			   //! Read the most Current Event  (always points to last event wrote)  uses struct runningEventData_t
	COMMAND_READ_CURRENT,		   //! Read the Current Event (uses current viewing pointer) uses struct runningEventData_t
	COMMAND_READ_NEXT,			   //! Read the Next Event (bounded by the most current event) uses struct runningEventData_t
	COMMAND_READ_PREVIOUS,		   //! Read the Previous Event (bounded by earliest event available) uses struct runningEventData_t
	COMMAND_FIND_RECORD,		   //! Find a Record in Memory based on Record Number uses struct runningEventData_t
	COMMAND_READ_MSP430,		   //! Read  Data from the MSP430 Slave Unit on the DSP Card uses struct msp430Data_t
	COMMAND_WRITE_MSP430,		   //! Write Data to   the MSP430 Slave Unit on the DSP Card uses struct msp430Data_t
	COMMAND_SAVE_BANK_SWITCH_DATA, //! Save  any modifications to the Bank Switch Information uses struct configurationData_t
	COMMAND_THORS_HAMMER		   //! Erases the entire chip and resets all counters to zero
} eventControllerCommand_t;

typedef enum event_controller_states
{
	EVENT_INITIALIZE,		   //! Initialize and populate data for the Event Controller State Machine
	EVENT_TEST_LOG_DATA,	   //! Check to make sure event data records are compatible
	EVENT_RECOVERY_CHECK,	  //! Check for Recovery in case system configuration was not saved properly
	EVENT_TIME_RECOVERY,	   //! Recover and update Time and RTC for the system
	EVENT_IDLE,				   //! Idle, just check buffers for anything
	EVENT_PROCESS_COMMAND,	 //! Process any external request. It may also be an internal request.
	EVENT_WRITE_FLASH,		   //! Write the buffered Events to External SPI Flash in the Data Area
	EVENT_SYSTEM_SECTOR_CHECK, //! Is it time to erase a sector in the bank configuration area
	EVENT_DATA_SECTOR_CHECK,   //! Is it time to erase a sector in the data area
	EVENT_DEBUG_START,         // Debug start
	EVENT_NO_FLASH			   //! Invalid or Non-communicating External SPI Flash Device
} event_controller_states_t;

//! Define event controller data record
typedef struct event_controller_data
{
	event_controller_states_t currentState; //! Enumerated variable indicating the currentState in the state machine
	event_controller_states_t lastState;	//! Enumerated variable indicating the lastState in the state machine
	uint16_t subState;					  //! Variable to indicate the current subState of operation in the state machine
	uint16_t eventWriteIndex;			  //! Pointer into writeEvents for storing new events
	uint16_t eventReadIndex;			  //! Pointer into writeEvents for writing event out to the External SPI Flash
	uint16_t numberEventsBuffered;		  //! Count of pending number of events to write out to the External SPI Flash
	uint16_t commandWriteIndex;			  //! Pointer into commands for storing new Commands
	uint16_t commandReadIndex;			  //! Pointer into commands for processing buffered Commands
	uint16_t numberCommandsBuffered;	  //! Count of pending number of Commands for processing
	Uint32 systemAddress;				  //! Unsigned Long variable used for the current system configuration record address
	Uint32 viewingRecord;				  //! Unsigned Long variable used for the current viewing record address by the user
	Uint32 nextWriteRecord;				  //! Next open space in External SPI Flash for writing data
	Uint32 findRecord;					  //! Unsigned Long variable for the Address that we are seeking
	Uint32 triggerDataSectorErase;		  //! Unsigned Long variable for data sector erase trigger checking
	Uint32 tempWrite;
	Uint32 fullRecordCount;									//! Unsigned Long variable indicating the total number of log records in this memory device
	systemData_t systemData;							//! Local copy in RAM of the Bank Configuration Record
	loggingData_t writeEvents[EVENT_BUFFER_LENGTH];  //! Array structure for holding events to write to External SPI Flash
	eventControllerCommand_t commands[EVENT_BUFFER_LENGTH]; //! Array structure for holding pending commands for state machine execution
	loggingData_t readEvent;							//! Buffered data structure for reading Events, linked to array of commands
	msp430Data_t msp430Data;							//! Buffered data structure for accessing the MSP430, linked to array of commands
	uint16_t systemReadyForUse;								//! Boolean flag to indicate that the system is ready for use
	uint16_t systemValid;									//! Boolean flag to indicate that we have valid data available
	timeT spiRecoveryTimer;							//! Timer used on startup to time out the system data request
	rtc_timeT rtcTime;
	Uint32 nextRecordNumber; //! Current available Record Number for writing
} event_controller_data_t;

void initEventStateController(void);
void eventStateController(void);
uint16_t saveConfiguration(void);
uint16_t addSystemEvent(eventNum_T eventCause);
uint16_t addDiagnostic(diagnosticNum_T diagnostic, uint16_t *data);
uint16_t nonDataEventCommand(eventControllerCommand_t command);
uint16_t readWriteMsp430(eventControllerCommand_t command, uint8_t *dataString);
uint16_t readEvent(eventControllerCommand_t command);
uint16_t findEvent(Uint32 recordNumber);
void unPackRTCstructure(uint16_t data, uint8_t *highData, uint8_t *lowData);
uint16_t packRTCstructure(uint16_t highData, uint16_t lowData);
void checkFlashEraseAtStartup (uint8_t reset);
event_controller_data_t* getEventData();

#endif /* EVENT_CONTROLLER_H_ */
