// ups_com.c

#include "ups_state_controller.h"
#include "ups_com.h"
#include "ups.h"
#include "system_config.h" // includes file name for configuration
#include SYSTEM_CONFIG	 // configuration header file
#include "uart.h"
#include "epwm.h"
#include "utilities.h"
#include "timer.h"
#include "string.h"
#include "stdio.h"
#include <stdlib.h>
#include <ctype.h>
#include "event_controller.h"
#include "event_display.h"
#include "display_led.h"

// Debug controllers declared in main
extern uint8_t startControllers;
extern uint8_t testLEDs;
extern event_controller_data_t eventData, *pEventData;
extern uint8_t overloadBypassOn;
extern uint32_t BRT;
extern uint32_t lastBRT;

char cmdStr[50], responseStr[50];
int cmdStrPos; // pointer to last char in string
#define strMax (100)
char str[strMax];

char *opModeStr[OP_MODE_STR_MAX] =
	{
		"Off",
		"On",
		"Auto",
		"Man",
		"Off/Auto",
		"On/Auto",
		"Slow/Auto",
		"Med/Auto",
		"Fast/Auto",
		"Ramp/Auto",
		"Off/Man",
		"On/Man",
		"Slow/Man",
		"Med/Man",
		"Fast/Man",
		"Fast/Ramp",
		"Alarm On",
		"Alarm Off",
		"Normal",
		"Warning",
		"Fault",
		"On Alarm/Auto",
		"Shutdown/Auto"};
		
char *upsStateStr[20] =
	{
		"Initialize",
		"Off",
		"Startup",
		"On Battery",
		"On Utility",
		"Bypass",
		"Shutdown",
		"Fault",
		"Comm Shutdown"};

typedef enum
{
	COM_PARSE_USER,
	COM_PARSE_TEST
} commandParserT;

// external variables

void CommandProcessor(int port)
{
	char charTemp, buf[20];
	char *pString; // string pointer to pass elements of string array
	static int multiCharMode = FALSE, delayedStartup = FALSE, delayedShutdown = FALSE /*, iTemp */;
	long delayTime = 0;
	int notifyFlag = FALSE;
	float fTemp;
	static cmd_states_t cmdMain = CMD_WAIT, cmdMainLast = CMD_PROCESS;
	static commandParserT parser = COM_PARSE_USER;
	static int subState = 0, heatrun = FALSE, heatrunInterval = 5; // interval in seconds
	static timeT heatrunTimer, notifyTimer;
	static operatingModesT notifyCustomerMsgLast = OFF;
	static float notifyVoltBat = 0, notifyAmpBat = 0, notifyTSink = 0, notifyTAmb = 0, notifyVoltIn = 0, notifyPowOut = 0;
	static event_display_format format;
	static event_display_format testOutput = FORMAT_EVENT;
    static uint8_t passwordPrompted = FALSE;
	buf[0] = NULL; // prevent compiler warning, used before set
	switch (cmdMain)
	{
	case CMD_WAIT:
		if (cmdMain != cmdMainLast) // State entry code
		{
			cmdMainLast = cmdMain;		// only do it once while in this state
			if (multiCharMode == FALSE) // process multiple characters
			{
				cmdStrPos = 0; // reset string pointer
			}
		}
		if (upsBoss.eventDump == ON) // State machine processing event display
		{ // State machine processing event display
 			if (eventDisplayHandler(format, (portName_t)port))
			{							// eventDisplay returns TRUE when processing, returns FALSE when Quit or End of dumps
				upsBoss.eventDump = ON; // Still doing eventDisplay / eventDump
			}
			else
			{							 // Otherwise...
				upsBoss.eventDump = OFF; // Done with eventDisplay / eventDump
			}
 			break; // stop processing User
		}
		if (usart_peekchar(port) == '^') // first char of an SNMP command
		{
// 1=SNMP, 2=UPSILON or 3=USER
// go back to default parser
#if COM_PARSE == 1
			upsBoss.upsComMode = UPS_COM_SNMP;
			upsBoss.upsComModeLast = UPS_COM_USER;
#elif COM_PARSE == 2
			upsBoss.upsComMode = UPS_COM_UPSILON;
			upsBoss.upsComModeLast = UPS_COM_USER;
#elif COM_PARSE == 3
			//upsBoss.upsComMode = UPS_COM_USER;
			//upsBoss.upsComModeLast = UPS_COM_USER;
			charTemp = usart_getchar(port); // eat char
#endif
			break; // stop processing User
		}
		if (usart_rx_buffer_count(port) != 0) // the buffer isn't empty
		{
			charTemp = usart_getchar(port); // get char
			//usart_putchar(charTemp,port);         // echo char
			if (multiCharMode == FALSE) // can process single or multiple characters
			{
				cmdStr[cmdStrPos++] = charTemp; // update cmdStr to process charTemp
				cmdStr[cmdStrPos] = NULL;		// to allow string operations ie strlen()
				cmdMain = CMD_PROCESS;			// multiple characters delimited by carriage return
			}
			else if (charTemp == 13)
			{
				multiCharMode = FALSE; // reset mode for next command
				cmdStrPos = 0;		   // reset pointer for next command
				cmdMain = CMD_PROCESS;
				usart_putstr("\r\n", port);
			}
			else
			{
				// ignore everything other than normal characters
				// from ascii 32			to ascii 126 (127 is Delete)
				if ((charTemp >= ' ') && (charTemp <= '~'))
				{
					cmdStr[cmdStrPos++] = charTemp; // add char, inc pointer
					cmdStr[cmdStrPos] = NULL;		// terminate for string function, if next char it will be overwritten
					usart_putchar(charTemp, port);
				}
			}
		}
		break;
	case CMD_PROCESS:
		if (cmdMain != cmdMainLast) // State entry code
		{
			cmdMainLast = cmdMain; // only do it once while in this state
			usart_putstr("\r\n", port);
		}
		switch (cmdStr[0])
		{
		case '?': // Tabular system information
			switch (parser)
			{
			case COM_PARSE_USER:
			case COM_PARSE_TEST:
				tabularData(pUpsBoss, parser == COM_PARSE_TEST, port); // Give up the Test Data
				break;
			default:
				usart_putstr("Unknown command\r\n", port); // What did you say?
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case 'U': // Turn on UPS
			if ((upsBoss.invMode == MANUAL_OFF) || (upsBoss.invMode == AUTO_OFF))
			{
				switch (parser)
				{
				case COM_PARSE_USER:
					upsBoss.inverterCommand = AUTO_ON; // User Mode: Make it automatic
					break;
				case COM_PARSE_TEST:
					upsBoss.inverterCommand = MANUAL_ON; // Test Mode: If forcing then make it manual
					break;
				default:
					usart_putstr("Unknown command\r\n", port); // What did you say?
					break;
				} // End of parser switch
				usart_putstr("Inverter On\r\n", port);
			}
			else
			{
				usart_putstr("Inverter is already on\r\n", port);
			}
			cmdMain = CMD_WAIT;
			break;
		case 'u': // Turn off UPS
			switch (parser)
			{
			case COM_PARSE_USER:
				if (strlen(&cmdStr[0]) == 1)
				{
					multiCharMode = TRUE;
					usart_putstr("Sure? (y/n)\r\n", port);
				}
				else
				{
					multiCharMode = FALSE;
					pString = &cmdStr[1];
					if ((cmdStr[1] == 'Y') || (cmdStr[1] == 'y'))
					{
						upsBoss.inverterCommand = AUTO_OFF; // User Mode: Make it automatic
						usart_putstr("Inverter Off\r\n", port);
					}
					else if ((cmdStr[1] == 'N') || (cmdStr[1] == 'n'))
					{
						usart_putstr("Command Cancelled\r\n", port);
					}
					else
					{
						usart_putstr("Invalid response. Please respond (y/n)\r\n", port);
					}
				}
				break;
			case COM_PARSE_TEST:
				upsBoss.inverterCommand = MANUAL_OFF; // Test Mode: If forcing then make it manual
				usart_putstr("Inverter Off\r\n", port);
				break;
			default:
				usart_putstr("Unknown command\r\n", port); // What did you say?
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case 'E': // Dump Event Buffer
			switch (parser)
			{
			case COM_PARSE_USER:
			case COM_PARSE_TEST:
				upsBoss.eventDump = ON;
				format = FORMAT_EVENT;
				pEventDisplay->eventDisplayFunction = EVENT_DISPLAY_RECORDS;
				break;
			default:
				usart_putstr("Unknown command\r\n", port); // What did you say?
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case 'e':		
		    switch (parser)
		    {
			    case COM_PARSE_TEST:
			        upsBoss.eventDump = ON;
					format = FORMAT_EVENT;
					// Switch to FORMAT_DIAGNOSTIC when realtime clock is implemented 
			        // format = FORMAT_DIAGNOSTIC;
			        pEventDisplay->eventDisplayFunction = EVENT_DISPLAY_RECORDS;
			        break;
			    case COM_PARSE_USER:
			    default:
			        usart_putstr("Unknown command\r\n", port);
			        break;
		    }
		    cmdMain = CMD_WAIT;
		    break;
		case '1': // Print First Event
			switch (parser)
			{
			case COM_PARSE_TEST:
				upsBoss.eventDump = ON;
				format = FORMAT_DIAGNOSTIC;
				pEventDisplay->eventDisplayFunction = EVENT_DISPLAY_FIRST;
				break;
			case COM_PARSE_USER:
			default:
				usart_putstr("Unknown command\r\n", port); // What did you say?
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case 'W': // Find Event/Diagnostic Record
			switch (parser)
			{
			case COM_PARSE_TEST:
				upsBoss.eventDump = ON;
				format = FORMAT_DIAGNOSTIC;
				pEventDisplay->eventDisplayFunction = EVENT_DISPLAY_FIND;
				break;
			case COM_PARSE_USER:
			default:
				usart_putstr("Unknown command\r\n", port); // What did you say?
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case 'w': // Save System Configuration Data Record
			switch (parser)
			{
			case COM_PARSE_TEST:
				usart_putstr("Saving System Configuration Data...\r\n", port); // Announce it just for the fun of it
				saveConfiguration();
				break;
			case COM_PARSE_USER:
			default:
				usart_putstr("Unknown command\r\n", port); // What did you say?
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case 'Y': // Set RTC values
			switch (parser)
			{
			case COM_PARSE_TEST:
				upsBoss.eventDump = ON;
				pEventDisplay->eventDisplayFunction = EVENT_DISPLAY_SET_RTC; // Real Time Clock
				break;
			case COM_PARSE_USER:
			default:
				usart_putstr("Unknown command\r\n", port); // What did you say?
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case 'y': // Read RTC values
			switch (parser)
			{
			case COM_PARSE_USER:
			case COM_PARSE_TEST:
				usart_putstr(rtcString(getRTCtime()), port);
				usart_putstr("\r\n", port);
				break;
			default:
				usart_putstr("Unknown command\r\n", port); // What did you say?
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case '*': // Bright-UPS Interface: Report Last Event in buffer.
			upsBoss.eventDump = ON;
			pEventDisplay->eventDisplayFunction = EVENT_DISPLAY_ASTERISK;
			switch (parser)
			{
			case COM_PARSE_USER:
				format = FORMAT_BRIGHTCOM;
				break;
			case COM_PARSE_TEST:
				format = testOutput;
				break;
			default:
				usart_putstr("Unknown command\r\n", port); // What did you say?
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case '+': // Bright-UPS Interface:  Report Previous Event in buffer pointer decreases
			upsBoss.eventDump = ON;
			pEventDisplay->eventDisplayFunction = EVENT_DISPLAY_PLUS;
			switch (parser)
			{
			case COM_PARSE_USER:
				format = FORMAT_BRIGHTCOM;
				break;
			case COM_PARSE_TEST:
				format = testOutput;
				break;
			default:
				usart_putstr("Unknown command\r\n", port); // What did you say?
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case '-': // Bright-UPS Interface:  Report Next Event in buffer pointer increases
			upsBoss.eventDump = ON;
			pEventDisplay->eventDisplayFunction = EVENT_DISPLAY_MINUS;
			switch (parser)
			{
			case COM_PARSE_USER:
				format = FORMAT_BRIGHTCOM;
				break;
			case COM_PARSE_TEST:
				format = testOutput;
				break;
			default:
				usart_putstr("Unknown command\r\n", port); // What did you say?
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case '_': // Bright-UPS Interface:  Debug for cyclic charging, bump to next cycle
			debugCyclicChargingNextCycle = TRUE;			
/* 			upsBoss.eventDump = ON;
			pEventDisplay->eventDisplayFunction = EVENT_DISPLAY_MINUS;
			switch (parser)
			{
			case COM_PARSE_USER:
				format = FORMAT_BRIGHTCOM;
				break;
			case COM_PARSE_TEST:
				format = testOutput;
				break;
			default:
				usart_putstr("Unknown command\r\n", port); // What did you say?
				break;
			} // End of parser switch
 */			cmdMain = CMD_WAIT;
			break;
		case '=': // Bright-UPS Interface:  Report Current Event in buffer no pointer movement
			upsBoss.eventDump = ON;
			pEventDisplay->eventDisplayFunction = EVENT_DISPLAY_EQUAL;
			switch (parser)
			{
			case COM_PARSE_USER:
				format = FORMAT_BRIGHTCOM;
				break;
			case COM_PARSE_TEST:
				format = testOutput;
				break;
			default:
				usart_putstr("Unknown command\r\n", port); // What did you say?
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case 't':
			switch (parser)
			{
			case COM_PARSE_TEST:
				if (testOutput == FORMAT_EVENT)
				{
					testOutput = FORMAT_DIAGNOSTIC;
					usart_putstr("Display is now Diagnostic Format\r\n", port);
				}
				else
				{
					testOutput = FORMAT_EVENT;
					usart_putstr("Display is now Event Format\r\n", port);
				}
				break;
			case COM_PARSE_USER:
			default:
				usart_putstr("Unknown command\r\n", port); // What did you say?
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case 'I': // Identify
			usart_putstr("% ", port);
			usart_putstr(UPS_MANUFACTURER, port);
			usart_putstr(" ", port);
			usart_putstr(UPS_PART, port);
			usart_putstr(" UPS", port);
			usart_putstr("\r\n", port);
			usart_putstr("% ", port);
			usart_putstr(UPS_PART_DESCRIPTION, port);
			usart_putstr("\r\n", port);
			usart_putstr("% ", port);
			usart_putstr(VERSION, port);
			usart_putstr(", ", port);
			usart_putstr(VERSION_DATE, port);
			usart_putstr("\r\n", port);
			usart_putstr("% ", port);
			usart_putstr(PART_VERSION, port);
			usart_putstr(", ", port);
			usart_putstr(PART_VERSION_DATE, port);
			usart_putstr("\r\n", port);
			cmdMain = CMD_WAIT;
			break;
		case 's': // Sonalert Beep
			switch (parser)
			{
			case COM_PARSE_USER:
			case COM_PARSE_TEST:
			    sonalert (OFF);
				sonalert (ON_BEEP);
				usart_putstr("Sonalert beep\r\n", port);
				break;
			default:
				usart_putstr("Unknown command\r\n", port); // What did you say?
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case 'S': // Sonalert Solid On
			switch (parser)
			{
			case COM_PARSE_USER:
			case COM_PARSE_TEST:
			    sonalert (OFF);
				sonalert (ON_SOLID);
				usart_putstr("Sonalert on\r\n", port);
				break;
			default:
				usart_putstr("Unknown command\r\n", port); // What did you say?
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case 'q': // Sonalert Off (Quiet)
			switch (parser)
			{
			case COM_PARSE_USER:
			case COM_PARSE_TEST:
				sonalert (OFF);
				usart_putstr("Sonalert off\r\n", port);
				break;
			default:
				usart_putstr("Unknown command\r\n", port); // What did you say?
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case 'T':
			switch (parser)
			{
			case COM_PARSE_USER:
				fakeButton(BUTTON_TEST); // User Mode: do light show
				break;
			case COM_PARSE_TEST:
				upsBoss.eventDump = ON;
				format = FORMAT_DIAGNOSTIC;
				pEventDisplay->eventDisplayFunction = EVENT_DISPLAY_COUNTERS; // Test Mode: print event counters
				break;
			default:
				usart_putstr("Unknown command\r\n", port); // What did you say?
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case 'O': // Test Mode: Bypass Off
			switch (parser)
			{
			case COM_PARSE_TEST:
				upsBoss.bypassCommand = AUTO_OFF; // Bypass Off
				usart_putstr("Bypass Off\r\n", port);
				break;
			case COM_PARSE_USER:
			default:
				usart_putstr("Unknown command\r\n", port); // What did you say?
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case 'B': // Force Bypass On
			switch (parser)
			{
			case COM_PARSE_USER:
				upsBoss.bypassCommand = AUTO_ON;
				usart_putstr("Bypass Auto On\r\n", port);
				break;
			case COM_PARSE_TEST:
				upsBoss.bypassCommand = MANUAL_ON; // Bypass On
				usart_putstr("Bypass Manual On\r\n", port);
				break;
			default:
				usart_putstr("Unknown command\r\n", port);
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case 'b': // Bypass Auto (Off if no failure)
			switch (parser)
			{
			case COM_PARSE_USER:
			case COM_PARSE_TEST:
				upsBoss.bypassCommand = AUTO_OFF; // Bypass Auto
				usart_putstr("Bypass Auto\r\n", port);
				break;
			default:
				usart_putstr("Unknown command\r\n", port);
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case 'M': // Test Mode: Manual
			switch (parser)
			{
			case COM_PARSE_TEST:
				switch (upsBoss.invMode)
				{
				case AUTO_ON:
					upsBoss.invMode = MANUAL_ON;
					break;
				case AUTO_OFF:
					upsBoss.invMode = MANUAL_OFF;
					break;
				}
				switch (upsBoss.dcMode)
				{
				case AUTO_ON:
					upsBoss.dcMode = MANUAL_ON;
					break;
				case AUTO_OFF:
					upsBoss.dcMode = MANUAL_OFF;
					break;
				}
				switch (upsBoss.chgMode)
				{
				case AUTO_ON:
					upsBoss.chgMode = MANUAL_ON;
					break;
				case AUTO_OFF:
					upsBoss.chgMode = MANUAL_OFF;
					break;
				}
				switch (upsBoss.bypassMode)
				{
				case AUTO_ON:
					upsBoss.bypassMode = MANUAL_ON;
					break;
				case AUTO_OFF:
					upsBoss.bypassMode = MANUAL_OFF;
					break;
				}
				switch (upsBoss.syncMode)
				{
				case AUTO_ON:
					upsBoss.syncMode = MANUAL_ON;
					break;
				case AUTO_OFF:
					upsBoss.syncMode = MANUAL_OFF;
					break;
				}
				upsBoss.fanMode = MANUAL;
				switch (upsBoss.batMode)
				{
				case AUTO_ON:
					upsBoss.batMode = MANUAL_ON;
					break;
				case AUTO_OFF:
					upsBoss.batMode = MANUAL_OFF;
					break;
				}
				usart_putstr("Manual\r\n", port);
				break;
			case COM_PARSE_USER:
			default:
				usart_putstr("Unknown command\r\n", port);
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case 'm': // Test Mode: Auto
			switch (parser)
			{
			case COM_PARSE_TEST:
				switch (upsBoss.invMode)
				{
				case MANUAL_ON:
					upsBoss.invMode = AUTO_ON;
					break;
				case MANUAL_OFF:
					upsBoss.invMode = AUTO_OFF;
					break;
				}
				switch (upsBoss.dcMode)
				{
				case MANUAL_ON:
					upsBoss.dcMode = AUTO_ON;
					break;
				case MANUAL_OFF:
					upsBoss.dcMode = AUTO_OFF;
					break;
				}
				switch (upsBoss.chgMode)
				{
				case MANUAL_ON:
					upsBoss.chgMode = AUTO_ON;
					break;
				case MANUAL_OFF:
					upsBoss.chgMode = AUTO_OFF;
					break;
				}
				switch (upsBoss.bypassMode)
				{
				case MANUAL_ON:
					upsBoss.bypassMode = AUTO_ON;
					break;
				case MANUAL_OFF:
					upsBoss.bypassMode = AUTO_OFF;
					break;
				}
				switch (upsBoss.syncMode)
				{
				case MANUAL_ON:
					upsBoss.syncMode = AUTO_ON;
					break;
				case MANUAL_OFF:
					upsBoss.syncMode = AUTO_OFF;
					break;
				}
				upsBoss.fanMode = AUTO;
				switch (upsBoss.batMode)
				{
				case MANUAL_ON:
					upsBoss.batMode = AUTO_ON;
					break;
				case MANUAL_OFF:
					upsBoss.batMode = AUTO_OFF;
					break;
				}
				usart_putstr("Manual\r\n", port);
				break;
			case COM_PARSE_USER:
			default:
				usart_putstr("Unknown command\r\n", port);
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case 'A': // Test Mode: Fan Air Fast
			switch (parser)
			{
			case COM_PARSE_TEST:
				usart_putstr("Not active in this Phase\r\n", port); // Fan Air Fast
				break;
			case COM_PARSE_USER:
			default:
				usart_putstr("Unknown command\r\n", port);
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case 'a': // Test Mode: Fan Air Slow
			switch (parser)
			{
			case COM_PARSE_TEST:
				usart_putstr("Not active in this Phase\r\n", port); // Fan Air Slow
				break;
			case COM_PARSE_USER:
			default:
				usart_putstr("Unknown command\r\n", port);
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case 'n': // Test Mode: Fan Air None
			switch (parser)
			{
			case COM_PARSE_TEST:
				usart_putstr("Not active in this Phase\r\n", port); // Fan Air None
				break;
			case COM_PARSE_USER:
			default:
				usart_putstr("Unknown command\r\n", port);
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case 'J': // Test Mode: Bank 1 (B1) Relay Test On
			switch (parser)
			{
			case COM_PARSE_TEST:
			    testLEDs = TRUE;
			    displayLedSet(LED_DISP_FAULT, LED_DISP_SOLID);
				usart_putstr("FAULT ON\r\n", port);
				break;
			case COM_PARSE_USER:
			default:
				usart_putstr("Unknown command\r\n", port);
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case 'j': // Test Mode: Bank 1 (B1) Relay Test Off
			switch (parser)
			{
			case COM_PARSE_TEST:
			    displayLedSet(LED_DISP_FAULT, LED_DISP_OFF);
				usart_putstr("FAULT OFF\r\n", port);
				break;
			case COM_PARSE_USER:
			default:
				usart_putstr("Unknown command\r\n", port);
				break;
			} // End of parser switch
			testLEDs = FALSE;
			cmdMain = CMD_WAIT;
			break;
		case 'G': // Test Mode: Bank 0 (B0) Relay Test On
			switch (parser)
			{
			case COM_PARSE_TEST:
				pinLatch(LATCH_BANK0, 1); // Turn on Latch Bank 0
				break;
			case COM_PARSE_USER:
			default:
				usart_putstr("Unknown command\r\n", port);
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case 'g': // Test Mode: Bank 0 (B0) Relay Test Off
			switch (parser)
			{
			case COM_PARSE_TEST:
				pinLatch(LATCH_BANK0, 0); // Turn off Latch Bank 0
				break;
			case COM_PARSE_USER:
			default:
				usart_putstr("Unknown command\r\n", port);
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case 'X': // toggle between User and Test parser mode
			switch (parser)
			{
			case COM_PARSE_USER:
				//iTemp = strlen(&cmdStr[0]);   // debug
				if (strlen(&cmdStr[0]) == 1)
				{
					if (passwordPrompted)
					{
						usart_putstr("Sorry, Charlie\r\n", port);
					    multiCharMode = FALSE;
						passwordPrompted = FALSE;
					}
					else
					{
					    multiCharMode = TRUE;
					    usart_putstr("Password? ", port);
					    passwordPrompted = TRUE;
					}
				}
				else
				{
					multiCharMode = FALSE;
					passwordPrompted = FALSE;
					pString = &cmdStr[1];
					if (strcmp(pString, "FRED") == 0)
					{
						usart_putstr("Test Mode enabled\r\n", port);
						parser = COM_PARSE_TEST;
					}
					else
					{
						usart_putstr("Sorry, Charlie\r\n", port);
					}
				}
				break;
			case COM_PARSE_TEST:
				usart_putstr("User Mode enabled\r\n", port);
				// in case any converter put into manual, change to auto
				upsBoss.chgCommand = AUTO_FAST;
				upsBoss.bypassCommand = AUTO_OFF;
				switch (upsBoss.invMode)
				{
				case MANUAL_ON:
					upsBoss.inverterCommand = AUTO_ON;
					break;
				case MANUAL_OFF:
					upsBoss.inverterCommand = AUTO_OFF;
					break;
				}				
				parser = COM_PARSE_USER;
				break;
			default:
				parser = COM_PARSE_USER;
				break;
			}
			cmdMain = CMD_WAIT;
			break;
		case 'R': // Test Parser: Heatrun start
			switch (parser)
			{
			case COM_PARSE_TEST:
				heatrun = TRUE;
				heatrunTimer = getTime(); // Reset timer
				usart_putstr("Starting Heat Run\r\n", port);
				break;
			case COM_PARSE_USER:
			default:
				usart_putstr("Unknown command\r\n", port); // What did you say?
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case 'r': // Test Parser: Heatrun stop
			switch (parser)
			{
			case COM_PARSE_TEST:
				heatrun = FALSE;
				usart_putstr("Ending Heat Run\r\n", port);
				break;
			case COM_PARSE_USER:
			default:
				usart_putstr("Unknown command\r\n", port); // What did you say?
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case 'L': // Test Parser: Heatrun interval, in seconds
			switch (parser)
			{
			case COM_PARSE_TEST:
				if (strlen(&cmdStr[0]) == 1)
				{
					usart_putstr("Enter interval in seconds\r\n", port);
					multiCharMode = TRUE;
				}
				else
				{
					multiCharMode = FALSE;
					pString = &cmdStr[1];
					heatrunInterval = atol(pString);
					heatrunInterval = lMax(heatrunInterval, 1);	// minimum time 0 would just overflow buffer
					heatrunInterval = lMin(heatrunInterval, 3600); // maximum is an hour
				}
				break;
			case COM_PARSE_USER:
			default:
				usart_putstr("Unknown command\r\n", port); // What did you say?
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case '%': // dump parameters group 1
			switch (parser)
			{
			case COM_PARSE_USER:
				if (upsBoss.bypassMode != MANUAL_OFF)					// if not in bypass
				{
					usart_putstr(itoa((int)upsBoss.voltOut), port);		// report output voltage as usual
				}
				else
				{
					usart_putstr(itoa((int)upsBoss.voltIn), port);		// otherwise report input voltage as output voltage
				}
				usart_putstr(",", port);
				usart_putstr(itoa((int)upsBoss.freqOut), port);
				usart_putstr(",", port);
				usart_putstr(itoa((int)upsBoss.ampOut), port);
				usart_putstr(",", port);
				usart_putstr(itoa((int)upsBoss.powOut), port);
				usart_putstr(",", port);
				usart_putstr(itoa((int)upsBoss.vaOut), port);
				usart_putstr(",", port);
				usart_putstr(itoa((int)upsBoss.pfOut), port);
				usart_putstr(",", port);
				usart_putstr(itoa((int)upsBoss.loadPctOut), port);
				usart_putstr(",", port);
#ifdef DUAL_INPUT_VOLTAGE
			if (externalDioInput(8))	// Returns '1' if IVD in 240 volt range
			{
				// multiply secondary voltage by 2
				usart_putstr(itoa((int)upsBoss.voltIn * 2.0f), port);
			}
			else						// Returns '0' if IVD in 120 volt range
			{
				usart_putstr(itoa((int)upsBoss.voltIn), port);
			}
#else       // #ifndef DUAL_INPUT_VOLTAGE
				usart_putstr(itoa((int)upsBoss.voltIn), port);
#endif      // #ifdef DUAL_INPUT_VOLTAGE			
				usart_putstr(",", port);
				usart_putstr(itoa((int)upsBoss.freqIn), port);
				usart_putstr(",", port);
				usart_putstr(itoa((int)upsBoss.tAmb), port);
				usart_putstr(",", port);
				usart_putstr(itoa((int)upsBoss.tSink2), port);
				usart_putstr(",", port);
				usart_putstr(itoa((int)upsBoss.voltBat), port);
				usart_putstr(",", port);
				usart_putstr(itoa((int)upsBoss.ampBat), port);
				usart_putstr(",", port);
				//usart_putstr (runtimeString (), port);
				// should be battery time remaining in minutes on battery or utility based on charge
				ltoa(timeExpired(upsBoss.timeBatRunStarted, getTime()) / 60000, (char *)buf[0]);
				usart_putstr((char *)buf[0], port);
				usart_putstr("\r\n", port);
				break;
			case COM_PARSE_TEST:
				upsBoss.eventDump = ON;
				pEventDisplay->eventDisplayFunction = EVENT_DISPLAY_THOR;
				break;
			default:
				usart_putstr("Unknown command\r\n", port); // What did you say?
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case '&': // dump parameters group 2
			switch (parser)
			{
			case COM_PARSE_USER:
			case COM_PARSE_TEST:
				usart_putstr(itoa((int)upsBoss.voltBus), port);
				usart_putstr(",", port);
				usart_putstr(itoa((int)upsBoss.voltSupply), port);
				usart_putstr(",", port);
				usart_putstr(itoa((int)upsBoss.ampSupply), port);
				// WPB
				//usart_putstr(",", port);
				//ltoa(upsBoss.bank1.all, (char *)buf[0]);
				//usart_putstr((char *)buf[0], port);
				//usart_putstr(",", port);
				//ltoa(upsBoss.bank2.all, (char *)buf[0]);
				//usart_putstr((char *)buf[0], port);
				//usart_putstr(",", port);
				//ltoa(upsBoss.bank3.all, (char *)buf[0]);
				//usart_putstr((char *)buf[0], port);
				usart_putstr("\r\n", port);
				break;
			default:
				usart_putstr("Unknown command\r\n", port); // What did you say?
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case '!': // dump status
			switch (parser)
			{
			case COM_PARSE_USER:
			case COM_PARSE_TEST:
				usart_putstr((char *)opModeStr[upsBoss.invMode], port);
				usart_putstr(",", port);
				usart_putstr((char *)opModeStr[upsBoss.dcMode], port);
				usart_putstr(",", port);
				usart_putstr((char *)opModeStr[upsBoss.chgMode], port);
				usart_putstr(",", port);
				usart_putstr((char *)opModeStr[upsBoss.bypassMode], port);
				usart_putstr(",", port);
				usart_putstr((char *)opModeStr[upsBoss.syncMode], port);
				usart_putstr(",", port);
				// placekeeper until fan control ready, message compatible with original code
				usart_putstr((char *)opModeStr[AUTO_FAST], port);
				usart_putstr("\r\n", port);
				break;
			default:
				usart_putstr("Unknown command\r\n", port); // What did you say?
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case '~': // dump runtime. Test Mode: All LEDs on
			switch (parser)
			{
			case COM_PARSE_USER:
				usart_putstr(runtimeString(), port);
				usart_putstr("\r\n", port);
				break;
			case COM_PARSE_TEST:
				upsBoss.displayMode = LIGHTS_ON;
				usart_putstr("LEDs ON\r\n", port);
				break;
			default:
				usart_putstr("Unknown command\r\n", port); // What did you say?
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case '`': // Bright-UPS Interface: Dump Runtime. Test Mode: All LEDs Off
			switch (parser)
			{
			case COM_PARSE_TEST:
				usart_putstr("LEDs OFF\r\n", port);
				upsBoss.displayMode = LIGHTS_OFF;
				break;
			case COM_PARSE_USER:
			default:
				usart_putstr("Unknown command\r\n", port); // What did you say?
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case '$':						  // Notify Message on
			switch (parser)
			{
			case COM_PARSE_USER:
			case COM_PARSE_TEST:
				usart_putstr("%N\r\n", port); // Acknowledge command
				upsBoss.notifyCustomerMsg = ON;
				notifyTimer = getTime();
				break;
			default:
				usart_putstr("Unknown command\r\n", port); // What did you say?
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case '#':						  // Notify Message off
			switch (parser)
			{
			case COM_PARSE_USER:
			case COM_PARSE_TEST:
				usart_putstr("%N\r\n", port); // Acknowledge command
				upsBoss.notifyCustomerMsg = OFF;
				break;
			default:
				usart_putstr("Unknown command\r\n", port); // What did you say?
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case '"':			  // multi character command follows
			switch (subState) // go to multichar mode to get command
			{
			case 0:
				if (strlen(&cmdStr[0]) == 1)
				{
					multiCharMode = TRUE;
				}
				else
				{
					pString = &cmdStr[1];
					if (strcmp(pString, "DELAY S/D") == 0)
					{
						// local variable, data structure variable set in ups_state_controller if time not -1
						delayedShutdown = TRUE;
					}
					else if (strcmp(pString, "DELAY S/U") == 0)
					{
						delayedStartup = TRUE;
					}
					else if (strcmp(pString, "LAN RELAY") == 0)
					{
						// add lan relay force on code here
					}
					else
					{
						usart_putstr("Error\r\n", port);
						cmdMain = CMD_WAIT; // don't continue command
						break;
					}
					// don't lose first " char (this command), ascii 13 clears it in CMD_WAIT
					cmdStrPos = 1;
					multiCharMode = TRUE; // go to multichar mode to get time
					subState++;
				}
				cmdMain = CMD_WAIT;
				break;
			case 1: // get time, set for command
				pString = &cmdStr[1];
				delayTime = atol(pString);
				delayTime = lMax(delayTime, 10); // minimum time 10 seconds
				if (delayedShutdown == TRUE)
				{
					delayedShutdown = FALSE; // reset local variable
					upsBoss.secShutdownDelay = delayTime;
				}
				else if (delayedStartup == TRUE)
				{
					delayedStartup = FALSE;
					upsBoss.secStartupDelay = delayTime;
				}
				subState = 0;
				cmdMain = CMD_WAIT;
				break;
			default:
				subState = 0; // reset for next time
				cmdMain = CMD_WAIT;
				break;
			}
			break;
		case ')': // battery capacity in percent
			switch (parser)
			{
			case COM_PARSE_USER:
			case COM_PARSE_TEST:
				usart_putstr(ftoaDp2(upsBoss.batChgPct), port);
				usart_putstr("\r\n", port);
				break;
			default:
				usart_putstr("Unknown command\r\n", port); // What did you say?
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case '(': // Bright-UPS Interface: Request additional Status Information
			switch (parser)
			{
			case COM_PARSE_USER:
			case COM_PARSE_TEST:
				usart_putstr((char *)opModeStr[upsBoss.audAlmMode], port);
				usart_putchar(',', port);
				// TODO: AC Relay status
				usart_putchar(',', port);
				// TODO: Battery Relay status
				usart_putstr("\r\n", port);
				break;
			default:
				usart_putstr("Unknown command\r\n", port); // What did you say?
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case 'C': // Bright-UPS Interface: Turn on Checksum
			switch (parser)
			{
			case COM_PARSE_USER:
				//upsBoss.checksumMode = CHKSUM_ON;							// Turn on checksum
				usart_putstr("Not supported at this time.\r\n", port); // Not implemented in code as of this time
				break;
			case COM_PARSE_TEST:
			default:
				usart_putstr("Unknown command\r\n", port); // What did you say?
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case 'c': // Charger off/manual
			switch (parser)
			{
			case COM_PARSE_USER:
				//upsBoss.checksumMode = CHKSUM_OFF;						// Turn off checksum
				usart_putstr("Not supported at this time.\r\n", port); // Not implemented in code as of this time
				break;
			case COM_PARSE_TEST:
				upsBoss.chgCommand = MANUAL_OFF;
				usart_putstr("Charge Off.\r\n", port);
				break;
			default:
				usart_putstr("Unknown command\r\n", port); // What did you say?
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case 'f': // Charger slow/manual
			switch (parser)
			{
			case COM_PARSE_TEST:
				upsBoss.chgCommand = MANUAL_SLOW;
				usart_putstr("Charge Slow.\r\n", port);
				break;
			case COM_PARSE_USER:
			default:
				usart_putstr("Unknown command\r\n", port);
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case 'F': // Charger fast/manual
			switch (parser)
			{
			case COM_PARSE_TEST:
				upsBoss.chgCommand = MANUAL_FAST;
				usart_putstr("Charge Fast.\r\n", port);
				break;
			case COM_PARSE_USER:
			default:
				usart_putstr("Unknown command\r\n", port);
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case 'v':
		case 'V': // Set Voltage, open loop
			switch (parser)
			{
			case COM_PARSE_TEST:
				if (strlen(&cmdStr[0]) == 1)
				{
					multiCharMode = TRUE;
					usart_putstr("Voltage = \r\n", port);
					cmdMain = CMD_WAIT;
				}
				else
				{
					multiCharMode = FALSE;
					pString = &cmdStr[1];
					fTemp = atof(pString);		// pass address of string after first char
					upsBoss.invVoltSet = fTemp; // set commanded volts
					//	sprintf (responseStr, "\rRamping to %f Volts, %f from %f Percent Modulation\r", fTemp, gainTarget, gain);
					usart_putstr("Changing Voltage\r\n", port);
				}
				break;
			case COM_PARSE_USER:
			default:
				usart_putstr("Unknown command\r\n", port);
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case 'd':
			switch (parser)
			{
			    case COM_PARSE_TEST:
		            upsBoss.dcMode = AUTO_OFF;
			        usart_putstr("DC off\r\n", port);
		            break;
				case COM_PARSE_USER:
				default:
				    usart_putstr("Unknown command\r\n", port);
				    break;
			}
			cmdMain = CMD_WAIT;
			break;
		case 'D':
			switch (parser)
			{
			    case COM_PARSE_TEST:
		          upsBoss.dcMode = AUTO_ON;
			      usart_putstr("DC on\r\n", port);
				  break;
				case COM_PARSE_USER:
				default:
				    usart_putstr("Unknown command\r\n", port);
				    break;
			}
		    cmdMain = CMD_WAIT;
		    break;
/*
		case 'D': // Set Frequency, open loop
			switch (parser)
			{
			case COM_PARSE_TEST:
				if (strlen(&cmdStr[0]) == 1)
				{
					multiCharMode = TRUE;
					usart_putstr("Frequency = \r\n", port);
					cmdMain = CMD_WAIT;
				}
				else
				{
					multiCharMode = FALSE;
					pString = &cmdStr[1];
					fTemp = atof(pString); // Pass address of string after first char
					if (fRange(fTemp, 45.0, 65.0))
					{							// check for reasonable range
						usart_putstr("Changing Frequency\r\n", port);
					}
					else
					{
						usart_putstr("Frequency outside range (45Hz-65Hz)\r\n", port);
					}
				}
				break;
			case COM_PARSE_USER:
			default:
				usart_putstr("Unknown command\r\n", port);
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
*/
		case 'P': // Sync On
			switch (parser)
			{
			case COM_PARSE_TEST:
				upsBoss.syncMode = AUTO_ON;
				usart_putstr("Sync On\r\n", port);
				break;
			case COM_PARSE_USER:
			default:
				usart_putstr("Unknown command\r\n", port);
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case 'p': // Sync Off
			switch (parser)
			{
			case COM_PARSE_TEST:
				upsBoss.syncMode = MANUAL_OFF;
				usart_putstr("Sync Off\r\n", port);
				break;
			case COM_PARSE_USER:
			default:
				usart_putstr("Unknown command\r\n", port);
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case '.': // Test Parser: set joules by entering percent
			switch (parser)
			{
			case COM_PARSE_TEST:
				if (strlen(&cmdStr[0]) == 1)
				{
					usart_putstr("Enter Joule Capacity in Percent\r\n", port);
					multiCharMode = TRUE;
				}
				else
				{
					multiCharMode = FALSE;
					pString = &cmdStr[1];
					fTemp = atof(pString);
					fTemp = fMax(fTemp, 0.0);   // minimum 0%
					fTemp = fMin(fTemp, 100.0); // maximum 100%
					upsBoss.batteryJoules = (long)((fTemp / 100.0) * BATTERY_MAX_JOULES);
				}
				break;
			case COM_PARSE_USER:
			default:
				usart_putstr("Unknown command\r\n", port);
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case 'h':
		case 'H': // User Mode and Test Mode Help
			switch (parser)
			{
			case COM_PARSE_USER:
				usart_putstr("\r\nUSER HELP\r\n\r\n", port); // Print the Header
				usart_putstr("UPS, U=on, u=off\r\n", port);
				usart_putstr("Display System Information=?\r\n", port);
				usart_putstr("Event Buffer Dump=E\r\n", port);
				usart_putstr("Test=T\r\n", port); // Light show test when in user mode
				usart_putstr("Identify Model (Model & Software version)=I\r\n", port);
				usart_putstr("Bypass, B=bypass, b=auto\r\n", port);
				usart_putstr("Sonalert, s=beep, S=continuous, q=quiet\r\n", port);
				usart_putstr("Help, h or H=this menu\r\n\r\n", port);
				break;
			case COM_PARSE_TEST:
				usart_putstr("\r\nTEST HELP\r\n\r\n", port); // Print the Header
				str[0] = NULL;
				strcpy(str, "Man, M=man, m=auto");
				tabStr(22, str);
				strcat(str, "| Inv, U=on, u=off\r\n");
				usart_putstr(str, port);
				str[0] = NULL;
				strcpy(str, "Event Buffer Dump=E");
				tabStr(22, str);
				strcat(str, "| History=T\r\n");
				usart_putstr(str, port);
				str[0] = NULL;
				strcpy(str, "Inv Volts, V");
				tabStr(22, str);
				strcat(str, "| Sync, P=sync, p=free run\r\n");
				usart_putstr(str, port);
				str[0] = NULL;
				strcpy(str, "DC/DC, D=on, d=off");
				tabStr(22, str);
				strcat(str, "| Charger, c=off, F=fast, f=slow\r\n");
				usart_putstr(str, port);
				str[0] = NULL;
				strcpy(str, "Ident = I");
				tabStr(22, str);
				strcat(str, "| Bypass, B=bypass, b=auto, O=off\r\n");
				usart_putstr(str, port);
				str[0] = NULL;
				strcpy(str, "Misc Info=*");
				tabStr(22, str);
				strcat(str, "| Fan (Air), A=fast, a=slow, n=none\r\n");
				usart_putstr(str, port);
				str[0] = NULL;
				strcpy(str, "Info=?");
				tabStr(22, str);
				strcat(str, "| Sonalert, s=beep, S=continuous, q=quiet\r\n");
				usart_putstr(str, port);
				str[0] = NULL;
				strcpy(str, "Heat Run Interval=L");
				tabStr(22, str);
				strcat(str, "| Heatrun, R=Start, r=stop, L=interval (sec)\r\n");
				usart_putstr(str, port);
				usart_putstr("Help, h or H=this menu, z=more help\r\n\r\n", port);
				break;
			default:
				usart_putstr("Unknown command\r\n", port);
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case 'z':
			switch (parser)
			{
			case COM_PARSE_TEST:
				usart_putstr("Verbose Debug, '[' = On, ']' = Off\r\n", port);
				usart_putstr("Bank 0 Relay Test, 'G' = On, 'g' = Off\r\n", port);
				usart_putstr("Bank 1 Relay Test, 'J' = On, 'j' = Off\r\n", port);
				usart_putstr("Set Joules, '.', then type in capacity in percent\r\n", port);
				usart_putstr("Turn on Battery DC/DC, 'D'\r\n", port);
				usart_putstr("<space>, Debug Output\r\n", port);
				usart_putstr("\r\nDiagnostic Dump, 'e'\r\n", port);
				usart_putstr("Display First Recorded Event/Diagnostic, '1'\r\n", port);
				usart_putstr("Find Event/Diagnostic, 'W'\r\n", port);
				usart_putstr("Save System Data, 'w'\r\n", port);
				usart_putstr("RTC Functions, 'Y' - Set RTC 'y' - Read RTC\r\n", port);
				usart_putstr("Toggle between Event and Diagnostic Display, 't'\r\n", port);
				break;
			case COM_PARSE_USER:
			default:
				usart_putstr("Unknown command\r\n", port);
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
#ifdef VERBOSE_DEBUG			
		case '[': // Turn on verbose debug mode
			switch (parser)
			{
			case COM_PARSE_TEST:
				upsBoss.verboseDebug = TRUE;
				usart_putstr("Verbose Debug On\r\n", port);
				break;
			case COM_PARSE_USER:
			default:
				usart_putstr("Unknown command\r\n", port);
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case ']': // Turn off verbose debug mode
			switch (parser)
			{
			case COM_PARSE_TEST:
				upsBoss.verboseDebug = FALSE;
				usart_putstr("Verbose Debug Off\r\n", port);
				break;
			case COM_PARSE_USER:
			default:
				usart_putstr("Unknown command\r\n", port);
				break;
			} // End of parser switch
			cmdMain = CMD_WAIT;
			break;
		case 'k':
		case 'K':
		    startControllers = TRUE;
			cmdMain = CMD_WAIT;
		    break;
#endif
		default:
		    usart_putstr("Unknown command\r\n", port);
		    cmdMain = CMD_WAIT;
		    break;
		}
	}
	if (heatrun == TRUE)
	{
		if (timer(heatrunTimer, heatrunInterval * 1000))
		{
			heatrunTimer = getTime();
			// Forth: timestamp, .BRT, Watt, vbat, ibat, ichgr, ips, bat_time? min:sec,
			// "*" if on bat, bat_cap?
			usart_putstr(runtimeString(), port);
			usart_putstr(" BRT=", port);
			//usart_putstr(itoa(upsBoss.batteryRuntime), port);
			usart_putstr(secToMsString(upsBoss.secOnBat), port);	// Convert to Minutes:Seconds
			usart_putstr(" BTR=", port);
			usart_putstr(secToMsString(upsBoss.estSecBat), port);	// Convert to Minutes:Seconds
			usart_putstr(" W=", port);
			usart_putstr(ftoaDp2(upsBoss.powOut), port);
			usart_putstr(" VB=", port);
			usart_putstr(ftoaDp2(upsBoss.voltBat), port);
			usart_putstr(" IB=", port);
			usart_putstr(ftoaDp2(upsBoss.ampBat), port);
			usart_putstr(" ICHGR=", port);
			usart_putstr(ftoaDp2(upsBoss.ampChg), port);
			//usart_putstr(ftoaDp3(upsBoss.ampChg), port);
			/*
			usart_putstr("  IPS=", port);
			usart_putstr(ftoaDp2(upsBoss.ampSupply), port);
			*/
			usart_putstr(" CAP=", port);
			usart_putstr(ftoaDp2(upsBoss.batChgPct), port);
			usart_putstr("%", port);
			// bat_time remaining min:sec
			// on battery asterisk
			if (upsBoss.upsState == UPS_ON_BAT)
			{
				usart_putstr("*", port);
			}
			else
			{
				usart_putstr(" ", port);
			}
			usart_putstr("\r\n", port);
		}
	}
	// if customer sets notifyMsg flag then if any new event occurs or if the following parameters change
	// by more than 5% nominal or 5 degrees C then it will send "%E" (event) or "%P" (parameter) message
	if ((upsBoss.notifyCustomerMsg == ON) && (timer(notifyTimer, 1000)))
	{
		if (upsBoss.notifyCustomerMsg != notifyCustomerMsgLast)
		{
			notifyCustomerMsgLast = upsBoss.notifyCustomerMsg;
			upsBoss.notifyEvent = FALSE; // set true by event()
			notifyVoltBat = upsBoss.voltBat;
			notifyAmpBat = upsBoss.ampBat;
			notifyTSink = upsBoss.tSink2;
			notifyTAmb = upsBoss.tAmb;
#ifdef DUAL_INPUT_VOLTAGE
			if (externalDioInput(8))	// Returns '1' if IVD in 240 volt range
			{
				// multiply secondary voltage by 2
				notifyVoltIn = upsBoss.voltIn * 2.0f;
			}
			else						// Returns '0' if IVD in 120 volt range
			{
				notifyVoltIn = upsBoss.voltIn;
			}
#else       							// #ifndef DUAL_INPUT_VOLTAGE
			notifyVoltIn = upsBoss.voltIn;
#endif      							// #ifdef DUAL_INPUT_VOLTAGE			
			notifyPowOut = upsBoss.powOut;
		}
		notifyTimer = getTime();
		notifyFlag = FALSE;
		// compare last notified level to current level, look for more than 5% change
		if (abs(notifyVoltBat - upsBoss.voltBat) > (upsBoss.voltBatNom * 0.05))
		{
			notifyVoltBat = upsBoss.voltBat; // remember this level
			notifyFlag = TRUE;
		}
		if (abs(notifyAmpBat - upsBoss.ampBat) > ((upsBoss.powOutNom / upsBoss.voltBatNom) * 0.05))
		{
			notifyAmpBat = upsBoss.ampBat; // remember this level
			notifyFlag = TRUE;
		}
		if (abs(notifyTSink - upsBoss.tSink2) > 5.0) // more than 5C change
		{
			notifyTSink = upsBoss.tSink2; // remember this level
			notifyFlag = TRUE;
		}
		if (abs(notifyTAmb - upsBoss.tAmb) > 5.0) // more than 5C change
		{
			notifyTAmb = upsBoss.tAmb; // remember this level
			notifyFlag = TRUE;
		}
#ifdef DUAL_INPUT_VOLTAGE
		if (externalDioInput(8))						// Returns '1' if IVD in 240 volt range
		{
			// multiply secondary voltage by 2
			if (abs(notifyVoltIn - upsBoss.voltIn * 2.0f) > (upsBoss.voltInNom * 2.0f * 0.05f))
			{
				notifyVoltIn = upsBoss.voltIn * 2.0f; 	// remember this level
				notifyFlag = TRUE;
			}
		}
		else											// Returns '0' if IVD in 120 volt range
		{
			if (abs(notifyVoltIn - upsBoss.voltIn) > (upsBoss.voltInNom * 0.05))
			{
				notifyVoltIn = upsBoss.voltIn; 			// remember this level
				notifyFlag = TRUE;
			}
		}
#else       // #ifndef DUAL_INPUT_VOLTAGE
		if (abs(notifyVoltIn - upsBoss.voltIn) > (upsBoss.voltInNom * 0.05))
		{
			notifyVoltIn = upsBoss.voltIn; // remember this level
			notifyFlag = TRUE;
		}
#endif      // #ifdef DUAL_INPUT_VOLTAGE			
		if (abs(notifyPowOut - upsBoss.powOut) > (upsBoss.powOutNom * 0.05))
		{
			notifyPowOut = upsBoss.powOut; // remember this level
			notifyFlag = TRUE;
		}
		if (notifyFlag == TRUE)
		{
			usart_putstr("%P\r\n", port);
		}
		if (upsBoss.notifyEvent == TRUE)
		{
			upsBoss.notifyEvent = FALSE; // set true by event()
			usart_putstr("%E\r\n", port);
		}
	}
}

// this adds spaces from last character in str[] to tabTo column
void tab(int tabTo)
{
	int pad, lastChar;

	lastChar = strlen(str);
	pad = tabTo - lastChar; // How many spaces to add
	if (pad > 0)			// only add, if needed
	{
		pad += lastChar;
		for (; lastChar < pad; lastChar++) // variable already set to start
		{
			str[lastChar] = ' ';	  // space
			str[lastChar + 1] = NULL; // string termination
		}
	}
}

// this adds spaces from last character in str[] to tabTo column
void tabStr(int tabTo, char *str)
{
	int pad, lastChar;

	lastChar = strlen(str);
	pad = tabTo - lastChar; // How many spaces to add
	if (pad > 0)
	{ // only add, if needed
		pad += lastChar;
		for (; lastChar < pad; lastChar++)
		{							  // variable already set to start
			str[lastChar] = ' ';	  // space
			str[lastChar + 1] = NULL; // string termination
		}
	}
}

void tabularData(struct upsDataStrucT *ups, int testParser, int port)
{
	char buf[20];
	volatile int iTemp;

	buf[0] = NULL; // set otherwise compiler will complain

	str[0] = NULL; // clear string
	tab(10);
	strcat(str, "Volts");
	tab(20);
	strcat(str, "Freq");
	tab(30);
	strcat(str, "Amps");
	tab(40);
	strcat(str, "Watts");
	tab(50);
	strcat(str, "VA");
	tab(60);
	strcat(str, "PF");
	tab(70);
	strcat(str, "Load\r\n");
	usart_putstr(str, port);

	str[0] = NULL; // clear string
	strcat(str, "Output:");
	tab(10);
	if (upsBoss.upsState == UPS_OFF)
		strcat(str, ftoaDp2(0.0f));
	else
		strcat(str, ftoaDp2(ups->voltOut));	// report inverter voltage
	tab(20);
	strcat(str, ftoaDp2(ups->freqOut));
#ifdef ZERO_POWER_REPORTING_AMPS
	if (ups->ampOut < ZERO_POWER_REPORTING_AMPS)
	{
		tab(30);
		strcat(str, ftoaDp2(0.0f));
		tab(40);
		strcat(str, ftoaDp2(0.0f));
		tab(50);
		strcat(str, ftoaDp2(0.0f));
		tab(60);
		strcat(str, ftoaDp2(0.0f));
		tab(70);
		strcat(str, ftoaDp2(0.0f));
		strcat(str, "%\r\n");
	}
	else
	{
		tab(30);
		if (upsBoss.upsState == UPS_OFF)
		    strcat(str, ftoaDp2(0.0f));
		else
		    strcat(str, ftoaDp2(ups->ampOut));
		tab(40);
		if (upsBoss.upsState == UPS_OFF)
		    strcat(str, ftoaDp2(0.0f));
		else
		    strcat(str, ftoaDp2(ups->powOut));
		tab(50);
		if (upsBoss.upsState == UPS_OFF)
		    strcat(str, ftoaDp2(0.0f));
		else
		    strcat(str, ftoaDp2(ups->vaOut));
		tab(60);
		if (upsBoss.upsState == UPS_OFF)
		    strcat(str, ftoaDp2(0.0f));
		else
		    strcat(str, ftoaDp2(ups->pfOut));
		tab(70);
		if (upsBoss.upsState == UPS_OFF)
		    strcat(str, ftoaDp2(0.0f));
		else
		    strcat(str, ftoaDp2(ups->loadPctOut));
		strcat(str, "%\r\n");
	}
#endif // ZERO_POWER_REPORTING_AMPS
#ifdef ZERO_POWER_REPORTING_WATTS
	if (ups->powOut < ZERO_POWER_REPORTING_WATTS)
	{
		tab(30);
		strcat(str, ftoaDp2(0.0f));
		tab(40);
		strcat(str, ftoaDp2(0.0f));
		tab(50);
		strcat(str, ftoaDp2(0.0f));
		tab(60);
		strcat(str, ftoaDp2(0.0f));
		tab(70);
		strcat(str, ftoaDp2(0.0f));
		strcat(str, "%\r\n");	}
	else
	{
		tab(30);
		if (upsBoss.upsState == UPS_OFF)
		    strcat(str, ftoaDp2(0.0f));
		else
		    strcat(str, ftoaDp2(ups->ampOut));
		tab(40);
		if (upsBoss.upsState == UPS_OFF)
		    strcat(str, ftoaDp2(0.0f));
		else
		    strcat(str, ftoaDp2(ups->powOut));
		tab(50);
		if (upsBoss.upsState == UPS_OFF)
		    strcat(str, ftoaDp2(0.0f));
		else
		    strcat(str, ftoaDp2(ups->vaOut));
		tab(60);
		if (upsBoss.upsState == UPS_OFF)
		    strcat(str, ftoaDp2(0.0f));
		else
		    strcat(str, ftoaDp2(ups->pfOut));
		tab(70);
		if (upsBoss.upsState == UPS_OFF)
		    strcat(str, ftoaDp2(0.0f));
		else
		    strcat(str, ftoaDp2(ups->loadPctOut));
		strcat(str, "%\r\n");
	}
#endif
#ifdef ZERO_POWER_REPORTING_PERCENT_LOAD
	if (ups->loadPctOut < ZERO_POWER_REPORTING_PERCENT_LOAD)
	{
		tab(30);
		strcat(str, ftoaDp2(0.0f));
		tab(40);
		strcat(str, ftoaDp2(0.0f));
		tab(50);
		strcat(str, ftoaDp2(0.0f));
		tab(60);
		strcat(str, ftoaDp2(0.0f));
		tab(70);
		strcat(str, ftoaDp2(0.0f));
		strcat(str, "%\r\n");	}
	else
	{
		tab(30);
		if (upsBoss.upsState == UPS_OFF)
		    strcat(str, ftoaDp2(0.0f));
		else
		    strcat(str, ftoaDp2(ups->ampOut));
		tab(40);
		if (upsBoss.upsState == UPS_OFF)
		    strcat(str, ftoaDp2(0.0f));
		else
		    strcat(str, ftoaDp2(ups->powOut));
		tab(50);
		if (upsBoss.upsState == UPS_OFF)
		    strcat(str, ftoaDp2(0.0f));
		else
		    strcat(str, ftoaDp2(ups->vaOut));
		tab(60);
		if (upsBoss.upsState == UPS_OFF)
		    strcat(str, ftoaDp2(0.0f));
		else
		    strcat(str, ftoaDp2(ups->pfOut));
		tab(70);
		if (upsBoss.upsState == UPS_OFF)
		    strcat(str, ftoaDp2(0.0f));
		else
		    strcat(str, ftoaDp2(ups->loadPctOut));
		strcat(str, "%\r\n");
	}
#endif
	usart_putstr(str, port);
	str[0] = NULL; // clear string
	strcat(str, "Input:");
	tab(10);
#ifdef DUAL_INPUT_VOLTAGE
	if (externalDioInput(8))				// Returns '1' if IVD in 240 volt range
	{
		// multiply secondary voltage by 2
		strcat(str, ftoaDp2(ups->voltIn * 2.0f));
	}
	else									// Returns '0' if IVD in 120 volt range
	{
		strcat(str, ftoaDp2(ups->voltIn));
	}
#else       								// #ifndef DUAL_INPUT_VOLTAGE
	strcat(str, ftoaDp2(ups->voltIn));
#endif      								// #ifdef DUAL_INPUT_VOLTAGE			
	tab(20);
	strcat(str, ftoaDp2(ups->freqIn));
	strcat(str, "\r\n\r\n");
	usart_putstr(str, port);

	str[0] = NULL; // clear string
	tab(10);
	strcat(str, "BatVolts");
	tab(20);
	strcat(str, "V/Batt");
	tab(30);
	strcat(str, "BatAmps");
	tab(40);
	strcat(str, "BatWatts");
	tab(50);
	strcat(str, "ChrgAmps");
	tab(60);
	strcat(str, "Charger");
	tab(70);
	strcat(str, "BatCap\r\n");
	usart_putstr(str, port);

	str[0] = NULL; // clear string
	tab(10);
	strcat(str, ftoaDp2(ups->voltBat)); // BatVolts
	tab(20);
	strcat(str, ftoaDp2(ups->voltBat/(float)(NUM_BAT))); // V/Batt
	tab(30);
	strcat(str, ftoaDp2(ups->ampBat)); // BatAmps
	tab(40);
	strcat(str, ftoaDp2(ups->voltBat*ups->ampBat)); // BatWatts
	tab(50);
	strcat(str, ftoaDp2(ups->ampChg)); // ChrgAmps
	tab(60);
	strcat(str, opModeStr[ups->chgMode]); // Charger
	tab(70);
	strcat(str, ftoaDp2(ups->batChgPct)); // BatCap
	strcat(str, "\r\n\r\n");
    usart_putstr(str, port);

	str[0] = NULL; // clear string
	tab(10);
	strcat(str, "BusVolts");
	tab(20);
	strcat(str, "PS Volts");
	tab(30);
	strcat(str, "ChrgWarn");
	tab(40);
	strcat(str, "Brd AC");
	tab(50);
	strcat(str, "LastBRT");
	tab(60);
	strcat(str, "BRT");
	tab(70);
	strcat(str, "BRT Left\r\n");
	usart_putstr(str, port);

	str[0] = NULL; // clear string
	tab(10);
	strcat(str, ftoaDp2(ups->voltBus)); // BusVolts
	tab(20);
	strcat(str, ftoaDp2(12.0f)); // PS Volts
	tab(30);
	strcat(str, "00:11:59"); // ChrgWarn
	tab(40);
	strcat(str, ftoaDp2(123.0f)); // Board AC
	tab(50);
	buf[0] = NULL;
	convertMSecToMinutesSeconds(lastBRT, buf); // LastBRT
	strcat(str, buf);
	tab(60);
	buf[0] = NULL;
	if (ups->dcMode == AUTO_ON ||  ups->dcMode == MANUAL_ON)
	  convertMSecToMinutesSeconds(timeExpired(ups->timeBatRunStarted, getTime()), buf);
	else
	  convertMSecToMinutesSeconds(0, buf);
	strcat(str, buf);
	tab(70);
	strcat(str, secToMsString(upsBoss.estSecBat)); // BRT Left, Minutes:Seconds
	strcat(str, "\r\n\r\n");
    usart_putstr(str, port);

	str[0] = NULL; // clear string
	tab(10);
	strcat(str, "Inverter");
	tab(20);
	strcat(str, "Battery");
	tab(30);
	strcat(str, "Bypass");
	tab(40);
	strcat(str, "Sync");
	tab(50);
	strcat(str, "Cab Temp");
	tab(60);
	strcat(str, "HS1 Temp");
	tab(70);
	strcat(str, "HS2 Temp\r\n");
	usart_putstr(str, port);

	str[0] = NULL; // clear string
	tab(10);
	strcat(str, opModeStr[ups->invMode]);
	tab(20);
	strcat(str, opModeStr[ups->dcMode]);
	tab(30);
	strcat(str, opModeStr[ups->bypassMode]); // Bypass
	tab(40);
	strcat(str, opModeStr[ups->syncMode]); // Sync
	tab(50);
	strcat(str, itoa((int)ups->tAmb + 0.5)); // round instead of floored
	strcat(str, "C/");						 // format "27C/80F"
	// convert to degrees F and round
	strcat(str, itoa((int)((ups->tAmb * 1.8) + 32 + 0.5))); // round instead of floored
	strcat(str, "F");
	tab(60);
	strcat(str, itoa((int)ups->tSink1 + 0.5));
	strcat(str, "C/"); // format "27C/80F"
	// convert to degrees F and round
	strcat(str, itoa((int)(ups->tSink1 * 1.8) + 32 + 0.5));
	strcat(str, "F");
	tab(70);
	strcat(str, itoa((int)ups->tSink2 + 0.5));
	strcat(str, "C/"); // format "27C/80F"
	// convert to degrees F and round
	strcat(str, itoa((int)(ups->tSink2 * 1.8) + 32 + 0.5));
	strcat(str, "F");
	strcat(str, "\r\n\r\n");
    usart_putstr(str, port);

	str[0] = NULL; // clear string
	tab(10);
	strcat(str, "FCC MAX");
	tab(20);
	strcat(str, "CXR(1300)");
	tab(30);
	strcat(str, "BTT/FCC");
	tab(40);
	strcat(str, "ON/OFF");
	tab(50);
	strcat(str, "FAST ON");
	tab(60);
	strcat(str, "JOULES/MAX");
	tab(70);
	strcat(str, "\r\n");
	usart_putstr(str, port);

	str[0] = NULL; // clear string
	tab(10);
	strcat(str, ftoaDp2(MAX_CHARGE_CYCLES)); // FCC MAX
	tab(20);
	strcat(str, "1"); // CXR(1300)
	tab(30);
	strcat(str, "Off/"); // BTT/FCC upsBoss.numberOfChargeCycles
	strcat(str, itoa((int)upsBoss.numberOfChargeCycles)); // BTT/FCC
	tab(40);
	strcat(str, "0 /1649"); // ON/OFF
	tab(50);
	if (getFastChargeCyclingOn()) 
	  strcat(str, "ON"); // FAST ON
	else
	   strcat(str, "OFF"); // FAST ON
	tab(60);
#ifdef EXTERNAL_BATTERY_PACK_JUMPER_SENSE
	if (externalDioInput(1))
	{
		// decrement base joule count (without external batteries) by a ratio of internal to total number of batteries
		ltoa((long)((float)upsBoss.batteryJoules * (1.0f/EXTERNAL_BATTERY_PACT_JOULE_RATIO)), (char *)buf[0]); // joules/joulesMax
	}
	else
	{
		// Single set of batteries
		ltoa(upsBoss.batteryJoules, (char *)buf[0]); // joules/joulesMax
	}
#else	// not defined EXTERNAL_BATTERY_PACK_JUMPER_SENSE
	ltoa(upsBoss.batteryJoules, (char *)buf[0]); // joules/joulesMax
#endif	// defined
	strcat(str, (char *)buf[0]);
	strcat(str, "/");
#ifdef EXTERNAL_BATTERY_PACK_JUMPER_SENSE
	if (externalDioInput(1))
	{
		// decrement base joule count (without external batteries) by a ratio of internal to total number of batteries
		ltoa((long)((float)BATTERY_MAX_JOULES * (1.0f/EXTERNAL_BATTERY_PACT_JOULE_RATIO)), (char *)buf[0]);
	}
	else
	{
		// Single set of batteries
		ltoa(BATTERY_MAX_JOULES, (char *)buf[0]);
	}
#else	// not defined EXTERNAL_BATTERY_PACK_JUMPER_SENSE
	ltoa(BATTERY_MAX_JOULES, (char *)buf[0]);
#endif	// defined
	strcat(str, (char *)buf[0]);
	strcat(str, "\r\n\r\n");
    usart_putstr(str, port);

	str[0] = NULL; // clear string
	tab(6);
	strcat(str, "Runtime: ");
	strcat(str, runtimeString());
	strcat(str, " ");
	strcat(str, UPS_PART_DESCRIPTION);
	strcat(str, " ");
	strcat(str, UPS_PART);
	strcat(str, " ");
	strcat(str, PART_VERSION_DATE);
	strcat(str, "\r\n\r\n");
	usart_putstr(str, port);

	if (testParser)
	{
		usart_putstr("\r\n*** Test Information ***\r\n", port);

		str[0] = NULL; // clear string
		tab(10);
		strcat(str, "Inv Gain");
		tab(20);
		strcat(str, "Inv Mod");
		tab(30);
		strcat(str, "Inv Bal");
		tab(40);
		strcat(str, "Bat Time");
		tab(50);
		strcat(str, "Inv FB");
		tab(60);
		strcat(str, "Batt Watt_Sec (ACC/MAX)\r\n");
		usart_putstr(str, port);

		str[0] = NULL; // clear string
		tab(10);
		strcat(str, ftoaDp2(invGetGain()));
		strcat(str, "%");
		tab(20);
		strcat(str, "Inv Mod");
		tab(30);
		strcat(str, "Inv Bal");
		tab(40);
		strcat(str, secToMsString(upsBoss.secOnBat));	// convert seconds to Minutes:Seconds
		tab(50);
		strcat(str, "Inv FB");
		tab(60);
#ifdef EXTERNAL_BATTERY_PACK_JUMPER_SENSE
		if (externalDioInput(1))
		{
			ltoa((long)((float)upsBoss.batteryJoules * (1.0f/EXTERNAL_BATTERY_PACT_JOULE_RATIO)), (char *)buf[0]);
		}
		else
		{
			ltoa(upsBoss.batteryJoules, (char *)buf[0]); // joules/joulesMax
		}
#else	// not defined EXTERNAL_BATTERY_PACK_JUMPER_SENSE
		ltoa(upsBoss.batteryJoules, (char *)buf[0]); // joules/joulesMax
#endif	// defined EXTERNAL_BATTERY_PACK_JUMPER_SENSE
		strcat(str, (char *)buf[0]);
		strcat(str, "/");
#ifdef EXTERNAL_BATTERY_PACK_JUMPER_SENSE
		if (externalDioInput(1))
		{
			ltoa((long)((float)BATTERY_MAX_JOULES  * (1.0f/EXTERNAL_BATTERY_PACT_JOULE_RATIO)), (char *)buf[0]);
		}
		else
		{
			ltoa(BATTERY_MAX_JOULES, (char *)buf[0]);
		}
#else	// not defined EXTERNAL_BATTERY_PACK_JUMPER_SENSE
		ltoa(BATTERY_MAX_JOULES, (char *)buf[0]);
#endif	// defined EXTERNAL_BATTERY_PACK_JUMPER_SENSE
		strcat(str, (char *)buf[0]);
		strcat(str, "\r\n");
		usart_putstr(str, port);

		str[0] = NULL; // clear string
		tab(10);
		strcat(str, "HS1");
		tab(20);
		strcat(str, "HS2");
		tab(30);
		strcat(str, "Ext Temp1");
		tab(40);
		strcat(str, "Ext Temp2");
		tab(50);
		strcat(str, "Ext Vin1");
		tab(60);
		strcat(str, "Ext Vin2");
		strcat(str, "\r\n");
		usart_putstr(str, port);

		str[0] = NULL; // clear string
		tab(10);
		strcat(str, itoa((int)ups->tSink1 + 0.5));
		strcat(str, "C/"); // format "27C/80F"
		// convert to degrees F and round
		strcat(str, itoa((int)(ups->tSink1 * 1.8) + 32 + 0.5));
		strcat(str, "F");
		tab(20);
		strcat(str, itoa((int)ups->tSink2 + 0.5));
		strcat(str, "C/"); // format "27C/80F"
		// convert to degrees F and round
		strcat(str, itoa((int)(ups->tSink2 * 1.8) + 32 + 0.5));
		strcat(str, "F");
		tab(30);
		strcat(str, itoa((int)ups->tExt1 + 0.5));
		strcat(str, "C/"); // format "27C/80F"
		// convert to degrees F and round
		strcat(str, itoa((int)(ups->tExt1 * 1.8) + 32 + 0.5));
		strcat(str, "F");
		tab(40);
		strcat(str, itoa((int)ups->tExt2 + 0.5));
		strcat(str, "C/"); // format "27C/80F"
		// convert to degrees F and round
		strcat(str, itoa((int)(ups->tExt2 * 1.8) + 32 + 0.5));
		strcat(str, "F");
		tab(50);
		strcat(str, ftoaDp2(ups->voltExt1));
		tab(60);
		strcat(str, ftoaDp2(ups->voltExt2));
		strcat(str, "\r\n");
		usart_putstr(str, port);
		/*
		str[0] = NULL;							// clear string
		tab(10);	strcat(str,"Inv Gain");
		tab(20);	strcat(str,"Inv Mod");
		tab(30);	strcat(str,"Inv Bal");
		tab(40);	strcat(str,"Bat Time");
		tab(50);	strcat(str,"Inv FB");
		tab(57);	strcat(str,"Batt Watt_Sec  ACC/MAX\r\n");
		usart_putstr(str, port);
		
		str[0] = NULL;							// clear string
		tab(10);	strcat(str,"Inv Gain");
		tab(20);	strcat(str,"Inv Mod");
		tab(30);	strcat(str,"Inv Bal");
		tab(40);	strcat(str,"Bat Time");
		tab(50);	strcat(str,"Inv FB");
		tab(57);	strcat(str,"Batt Watt_Sec  ACC/MAX\r\n");
		usart_putstr(str, port);
		*/
	}

	strcpy(str, "UPS State: ");
	strcat(str, upsStateStr[ups->upsState]);
	strcat(str, "\r\n\r\n");
	usart_putstr(str, port);
}

char *getOpmodeStr(operatingModesT opmode)
{
	static char buf[25];

	strcpy(buf, opModeStr[opmode]);

	return (&buf[0]);
}
