// snmp.c

#include "snmp.h"
#include "uart.h"
#include "event_controller.h"
#include "ups.h"
#include "ups_state_controller.h"
#include "system_config.h" // includes file name for configuration
#include SYSTEM_CONFIG	 // configuration header file
#include "utilities.h"
#include "timer.h"
#include "types.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

extern uint8_t overloadBypassOn;

// function prototypes
void scanSnmpParams(struct snmpDataStruct *parseType);

snmpParsePollEnumT snmpParsePoll;

#define SNMP_CMD_MAX 25                         // Too many args, last one thrown out -- didn't error, just 'warning' from compiler

extern char snmpParsePollCmd[SNMP_CMD_MAX][5] = {
	"AP1",
	"AP2",
	"ATR",
	"BTS",
	"MAN",
	"MOD",
	"NOM",
	"OTC",
	"OTD",
	"RWD",
	"ST1",
	"ST2",
	"ST3",
	"ST4",
	"ST5",
	"ST6",
	"ST7",
	"SDA",
	"STD",
	"STR",
	"UBR",
	"UID",
	"VER",
	"XD1",
	"PSD"};

/*
This is used to find which command is being sent
*/
extern event_controller_data_t eventData, *pEventData;

snmpParsePollEnumT selectSnmpCmd(char *str)
{

	int i;
	snmpParsePollEnumT result;

	result = NO_SNMP_CMD; // command not found

	for (i = 0; i < SNMP_CMD_MAX; i++)
	{
		if (strncmp(str, &snmpParsePollCmd[i][0], 3) == 0)
		{									// returns 0 when equal, check first 3 char for cmd ex. "ST1"
			result = (snmpParsePollEnumT)i; // if same string, return index of match
		}
	}
	return result;
}

// special version of scanParams for SNMP, needed for new SNMP commands with multiple parameters, extended snmpDataStruct
void scanSnmpParams(struct snmpDataStruct *parseType)
{

	int msgPos, paramPos, paramNum;
	char msgChar;

	parseType->lastParam = -1; // if no params found this will be untouched

	paramPos = paramNum = 0; // reset pointers

	msgPos = 3; // start with first char after "OTC"
	if (strlen(parseType->aData) > 3)
	{
		parseType->lastParam = 0; // something is there besides preamble
	}

	parseType->StrParams[0][0] = NULL; // clear param0 by putting NULL in char 0

	// run until end of message buffer or finding a NULL char
	while ((parseType->aData[msgPos] != NULL) && (msgPos < (SNMP_STR_MAX - 3)))
	{
		msgChar = parseType->aData[msgPos++]; // get character
		if (msgChar == ',')
		{ // comma delimited list
			if (paramNum < PARAM_NUM_MAX)
			{									   // don't overrun max number of parameters, if so keep using last index
				parseType->lastParam = ++paramNum; // update data structure
			}
			paramPos = 0; // reset char pointer for next param
			// clean out previous contents, may skip right to next param if next char is a ','
			parseType->StrParams[paramNum][paramPos] = NULL;
		}
		else
		{
			if (paramPos < (PARAM_LEN_MAX - 2))
			{ // check for overrun, leave room for next char and null
				parseType->StrParams[paramNum][paramPos++] = msgChar;
				// Null will move until last char put into string insuring string terminated with null
				parseType->StrParams[paramNum][paramPos] = NULL;
			}
		}
	}
}

void init_snmp_com(struct snmpDataStruct *parseType)
{
	parseType->str[0] = parseType->aLen[0] = parseType->aData[0] = NULL;
	parseType->pos = parseType->subPos = 0;
	parseType->snmpComState = SNMP_IDLE;
	parseType->lastSnmpComState = SNMP_RESPONSE; // force state initialization
	if (pEventData->systemReadyForUse)
	{
		parseType->snmpUid = (int)pEventData->systemData.snmpUid;
	}

}

void snmp_com(struct upsDataStrucT *upsData, struct snmpDataStruct *parseType, int port)
{
	static int temp1, temp2, temp[10];
	char *pString; // string pointer to pass elements of string array

	switch (parseType->snmpComState)
	{
	case SNMP_IDLE: // Waiting for timed command, state change cmd or ups generated notification (button press, etc.)
		if (parseType->snmpComState != parseType->lastSnmpComState)
		{ // state entry code
			parseType->lastSnmpComState = parseType->snmpComState;
			parseType->timeOutStart = getTime();
		} // end state entry code

		if (usart_rx_buffer_count(port))
		{ // beginning of message
			parseType->snmpComState = SNMP_WAITING;
		}
		if (timer(parseType->timeOutStart, 5000))
		{
			parseType->comOkay = FALSE;
		}
		break;
	case SNMP_WAITING: // waiting for incoming msg to complete
		if (parseType->snmpComState != parseType->lastSnmpComState)
		{ // state entry code
			parseType->lastSnmpComState = parseType->snmpComState;

			parseType->str[0] = parseType->aLen[0] = parseType->aData[0] = NULL;
			parseType->pos = parseType->subPos = 0;
			parseType->phase = START;
			parseType->type = UNKNOWN;
		} // end state entry code
		// second msg started before first one complete!
		if ((usart_peekchar(port) == '^') && (parseType->pos > 0))
		{
			parseType->type = FAIL;
		}
		else
		{ // continue getting message
			if (usart_rx_buffer_count(port))
			{ // if char pending, get it
				if (parseType->pos >= SNMP_STR_MAX)
				{							// str buffer full
					parseType->type = FAIL; // dump message, start again
				}
				else
				{ // str buffer not full
					// get char, put in string, bump pointer
					parseType->str[parseType->pos++] = parseType->snmpChar = usart_getchar(port);
					switch (parseType->phase)
					{
					case START:
						// nothing to process
						break;
					case LEN:
						// make sure it's an ascii code within chars '0' and '9' and buffer not full
						if ((parseType->snmpChar >= '0') && (parseType->snmpChar <= '9') && (parseType->subPos < SNMP_STR2_MAX))
						{
							parseType->aLen[parseType->subPos++] = parseType->snmpChar;
							parseType->aLen[parseType->subPos] = NULL; // terminate with null so string processes correctly atoi
						}
						else
						{
							parseType->type = FAIL;
						}
						break;
					case DATA:
						// make sure buffer not full
						if (parseType->subPos < (SNMP_STR_MAX - 1))
						{																 // at string max - 2, add char and null
							parseType->aData[parseType->subPos++] = parseType->snmpChar; // accumulate data string
							parseType->aData[parseType->subPos] = NULL;					 // easy to process, no matter when we stop
							if (parseType->subPos >= parseType->dataLen)
							{											 // when position+1 = length, stop
								parseType->snmpComState = SNMP_RESPONSE; // message is done, now to respond to it
							}
						}
						else
						{
							parseType->type = FAIL;
						}
						break;
					default:
						break;
					}
					switch (parseType->pos)
					{		// since this is bumped after getting char, 2 means 2nd char in snmpChar
					case 2: // check after we have two characters
						switch (parseType->snmpChar)
						{
						case 'P':
							parseType->type = POLL;
							break;
						case 'S':
							parseType->type = SET;
							break;
						case 'X': // exit from SNMP Protocol to User
							upsBoss.upsComMode = UPS_COM_USER;
							usart_putstr("\r\nUser Mode enabled\r\n", port);
							// switch to User Protocol
							// start again with new msg, old one is ignored
							parseType->snmpComState = SNMP_IDLE;
							break;
						default:
							parseType->type = FAIL;
							break;
						}
						parseType->phase = LEN; // next get number of char in message
						parseType->subPos = 0;  // reset to fill sub field
						break;
					case 5: // now aLen should have string with length number in it
						parseType->dataLen = atoi(parseType->aLen);
						if ((parseType->dataLen > 0) && (parseType->dataLen < SNMP_STR_MAX))
						{
							parseType->phase = DATA; // next get number of char in message
							parseType->subPos = 0;   // reset to fill sub field
						}
						else
						{
							parseType->type = FAIL;
						}
						break;
					default:
						break;
					}
				}
			}
		}
		// message failed to conform to protocol in some way
		if (parseType->type == FAIL)
		{
			usart_putstr("^0", port);			 // inform SNMP that message isn't understood
			parseType->snmpComState = SNMP_IDLE; // start again with new msg, old one is ignored
		}
		else
		{ // message came through and was understood, reset comOkay
			parseType->comOkay = TRUE;
		}
		break;
	case SNMP_RESPONSE: // Not used for timed updating, this is when question is first asked by Upsilon
		if (parseType->snmpComState != parseType->lastSnmpComState)
		{ // state entry code
			parseType->lastSnmpComState = parseType->snmpComState;
		}													  // end state entry code
		parseType->snmpCmd = selectSnmpCmd(parseType->aData); // get enum value related to string command
		switch (parseType->snmpCmd)
		{
		case AP1: // Poll commands, some Set commands, variables implemented by us
			//				snmp_putString("^D0517,8,10,14,16,17,18,19,20,22,24,36,37,38,39,40,43,46");
			strcpy(parseType->str, "7,8,10,14,16,17,18,19,20,22,24,36,37,38,39,40,43,46");
			// sprintf will take the message, put ^D in front, then 3 digit length, then msg ex. "^D0052,230"
			parseType->dataLen = strlen(parseType->str);
			strcpy(parseType->responseStr, "^D");
			strcat(parseType->responseStr, itoa3(parseType->dataLen));
			strcat(parseType->responseStr, parseType->str);
			usart_putstr(parseType->responseStr, port);
			parseType->snmpComState = SNMP_IDLE;
			break;
		case AP2:
			//				snmp_putString("^D07447,51,54,55,56,58,59,60,61,62,63,65,68,69,72,73,76,77,80,82,83,84,85,88,89");
			strcpy(parseType->str, "47,51,54,55,56,58,59,60,61,62,63,65,68,69,72,73,76,77,80,82,83,84,85,88,89");
			// sprintf will take the message, put ^D in front, then 3 digit length, then msg ex. "^D0052,230"
			parseType->dataLen = strlen(parseType->str);
			sprintf(parseType->responseStr, "^D%03d%s", parseType->dataLen, parseType->str);
			usart_putstr(parseType->responseStr, port);
			parseType->snmpComState = SNMP_IDLE;
			break;
		case ATR:												// Autostart Poll and Set
			if (pEventData->systemData.bankSwitches.bit.AUTOSTART == 1) // see if autoStart bit is set
			{
				strcpy(parseType->str, "1");
			}
			else
			{
				strcpy(parseType->str, "2");
			}
			// sprintf will take the message, put ^D in front, then 3 digit length, then msg ex. "^D0052,230"
			parseType->dataLen = strlen(parseType->str);
			strcpy(parseType->responseStr, "^D001");
			strcat(parseType->responseStr, parseType->str);
			usart_putstr(parseType->responseStr, port);
			parseType->snmpComState = SNMP_IDLE;
			break;
		case BTS: // Battleshort Poll and Set
			if (parseType->type == SET)
			{
				if (parseType->str[8] == '1')
				{ // Command to turn Battleshort on
					upsBoss.battleshort = TRUE;
					upsBoss.battleshortSNMP = TRUE;
				}
				else
				{
					upsBoss.battleshortSNMP = FALSE;
					if (upsBoss.battleshortLocal == FALSE)
					{
						upsBoss.battleshort = FALSE;
					}
				}
				strcat(parseType->responseStr, "^1");
				usart_putstr(parseType->responseStr, port);
			}
			else if (parseType->type == POLL)
			{
				if (upsBoss.battleshort == TRUE)
				{ // see if autoStart bit is set
					strcpy(parseType->str, "1");
				}
				else
				{
					strcpy(parseType->str, "0");
				}
				strcpy(parseType->responseStr, "^D001");
				strcat(parseType->responseStr, parseType->str);
				usart_putstr(parseType->responseStr, port);
			}
			parseType->snmpComState = SNMP_IDLE;
			break;
		case MAN: // Manufacturer
			strcpy(parseType->str, upsData->man);
			parseType->dataLen = strlen(parseType->str);
			strcpy(parseType->responseStr, "^D");
			strcat(parseType->responseStr, itoa3(parseType->dataLen));
			strcat(parseType->responseStr, parseType->str);
			usart_putstr(parseType->responseStr, port);
			parseType->snmpComState = SNMP_IDLE;
			break;
		case MOD: // Model
			strcpy(parseType->str, upsData->model);
			parseType->dataLen = strlen(parseType->str);
			strcpy(parseType->responseStr, "^D");
			strcat(parseType->responseStr, itoa3(parseType->dataLen));
			strcat(parseType->responseStr, parseType->str);
			usart_putstr(parseType->responseStr, port);
			parseType->snmpComState = SNMP_IDLE;
			break;
		case NOM: // Nominal values for parameters
			strcpy(parseType->str, itoa((int)upsBoss.voltInNom));
			strcat(parseType->str, ",");
			strcat(parseType->str, itoa((int)upsBoss.freqInNom * 10));
			strcat(parseType->str, ",");
			strcat(parseType->str, itoa((int)upsBoss.voltOutNom));
			strcat(parseType->str, ",");
			strcat(parseType->str, itoa((int)upsBoss.freqOutNom * 10));
			strcat(parseType->str, ",");
			strcat(parseType->str, itoa((int)upsBoss.vaOutNom));
			strcat(parseType->str, ",");
			strcat(parseType->str, itoa((int)upsBoss.powOutNom));
			strcat(parseType->str, ",1,2,100,140");
			parseType->dataLen = strlen(parseType->str);
			strcpy(parseType->responseStr, "^D");
			strcat(parseType->responseStr, itoa3(parseType->dataLen));
			strcat(parseType->responseStr, parseType->str);
			usart_putstr(parseType->responseStr, port);
			parseType->snmpComState = SNMP_IDLE;
			break;
	    case RWD:
		    if (parseType->type == SET)
			{
				scanSnmpParams(parseType);
				if (parseType->lastParam <= 2) // don't use if invalid
				{
					pString = &parseType->StrParams[0][0];
					temp1 = atoi(pString);
					if (iRange(temp1, -1, 999)) // only valid between -1 and 999 sec
					{
				        setSnmpReboot(temp1*1000);
						strcpy(parseType->responseStr, "^1");
					}
					else
					{
						strcpy(parseType->responseStr, "^0"); // out of range, reject command
					}
					usart_putstr(parseType->responseStr, port);
				}

			}
			parseType->snmpComState = SNMP_IDLE;
		    break;
		case PSD: // Timed Shutdown, Set command unique
			if (parseType->type == POLL)
			{
				strcpy(parseType->str, itoa(upsData->secShutdownDelay));
				parseType->dataLen = strlen(parseType->str);
				strcpy(parseType->responseStr, "^D");
				strcat(parseType->responseStr, itoa3(parseType->dataLen));
				strcat(parseType->responseStr, parseType->str);
				usart_putstr(parseType->responseStr, port);
			}
			else if (parseType->type == SET) // UPS timed shutdown command
			{
				scanSnmpParams(parseType);
				if (parseType->lastParam <= 2) // don't use if invalid
				{
					pString = &parseType->StrParams[0][0];
					temp1 = atoi(pString);
					if (iRange(temp1, -1, 999)) // only valid between -1 and 999 sec
					{
						upsData->secShutdownDelay = temp1; // process in UPS state machine
						strcpy(parseType->responseStr, "^1");
					}
					else
					{
						strcpy(parseType->responseStr, "^0"); // out of range, reject command
					}
					usart_putstr(parseType->responseStr, port);
				}
			}
			parseType->snmpComState = SNMP_IDLE;
			break;
		case STD: // Timed Startup, Set command unique
			if (parseType->type == POLL)
			{
				strcpy(parseType->str, itoa(upsData->secStartupDelay));
				parseType->dataLen = strlen(parseType->str);
				strcpy(parseType->responseStr, "^D");
				strcat(parseType->responseStr, itoa3(parseType->dataLen));
				strcat(parseType->responseStr, parseType->str);
				usart_putstr(parseType->responseStr, port);
			}
			else if (parseType->type == SET) // UPS timed shutdown command
			{
				scanSnmpParams(parseType);
				if (parseType->lastParam <= 2) // don't use if invalid
				{
					pString = &parseType->StrParams[0][0];
					temp1 = atoi(pString);
					if (iRange(temp1, -1, 999)) // only valid between -1 and 999 sec
					{
						upsData->secStartupDelay = temp1; // process in UPS state machine
						if (temp1 != -1)				  // if it isn't an abort command
						{
							upsData->delayedStartup = TRUE; // activate delayed startup
						}
						strcpy(parseType->responseStr, "^1");
					}
					else
					{
						strcpy(parseType->responseStr, "^0"); // out of range, reject command
					}
					usart_putstr(parseType->responseStr, port);
				}
			}
			parseType->snmpComState = SNMP_IDLE;
			break;
		case SDA:						 // Shutdown type 1 and 2, Poll and Set, shutdown type 0=type 1, 1=type 2
			if (parseType->type == POLL) // requesting current shutdown type
			{
				strcpy(parseType->responseStr, "^D001");
				temp1 = upsData->shutdownType;
				strcat(parseType->responseStr, itoa(temp1));
				usart_putstr(parseType->responseStr, port);
			}
			else if (parseType->type == SET)
			{
				switch (upsBoss.upsState)
				{
				case UPS_ON_BAT: // only update when system is on
				case UPS_ON_UTIL:
					parseType->responseStr[0] = parseType->str[8];
					parseType->responseStr[1] = NULL;
					temp1 = atoi(parseType->responseStr);
					if ((temp1 == 1) || (temp1 == 2)) // only process 1 or 2
					{
						upsData->shutdownType = temp1;
					}
					strcpy(parseType->responseStr, "^1");
					usart_putstr(parseType->responseStr, port);
					break;
				}
			}
			parseType->snmpComState = SNMP_IDLE;
			break;
		case ST1: // Battery Status, ambient temperature
			if (parseType->parser == SNMP)
			{
				temp1 = (int)(upsData->batChgPct);
			}
			else
			{
				temp1 = (int)((upsData->batChgPct + 0.5) * 10);
			}

			strcpy(parseType->str, itoa(upsData->batCond));
			strcat(parseType->str, ",");
			strcat(parseType->str, itoa(upsBoss.batSts));
			strcat(parseType->str, ",");
			strcat(parseType->str, itoa(upsData->batChgMode));
			strcat(parseType->str, ",");
			strcat(parseType->str, itoa((int)upsData->secOnBat));
			strcat(parseType->str, ",");
			strcat(parseType->str, itoa((int)upsData->estSecBat/60));
			strcat(parseType->str, ",");
			strcat(parseType->str, itoa(temp1));
			strcat(parseType->str, ",");
			strcat(parseType->str, itoa((int)((upsData->voltBat + 0.5) * 10)));
			strcat(parseType->str, ",");
			strcat(parseType->str, itoa((int)((upsData->ampBat + 0.5) * 10)));
			strcat(parseType->str, ",");
			strcat(parseType->str, itoa((int)(upsData->tAmb + 0.5)));
			parseType->dataLen = strlen(parseType->str);
			//			sprintf(parseType->responseStr,"^D%03d%s", parseType->dataLen,parseType->str);
			strcpy(parseType->responseStr, "^D");
			strcat(parseType->responseStr, itoa3(parseType->dataLen));
			strcat(parseType->responseStr, parseType->str);
			usart_putstr(parseType->responseStr, port);
			parseType->snmpComState = SNMP_IDLE;
			break;
		case ST2: // Input and output voltage and frequency
			strcpy(parseType->str, ",1,");
			strcat(parseType->str, itoa((int)(upsData->freqIn * 10)));
			strcat(parseType->str, ",");
#ifdef DUAL_INPUT_VOLTAGE
			if (externalDioInput(8))	// Returns '1' if IVD in 240 volt range
			{
				// multiply secondary voltage by 2
				strcat(parseType->str, itoa((int)(upsData->voltIn * 2.0f * 10.0f)));
			}
			else						// Returns '0' if IVD in 120 volt range
			{
				strcat(parseType->str, itoa((int)(upsData->voltIn * 10.0f)));
			}
#else       // #ifndef DUAL_INPUT_VOLTAGE
			strcat(parseType->str, itoa((int)(upsData->voltIn * 10.0f)));
#endif      // #ifdef DUAL_INPUT_VOLTAGE			
			parseType->dataLen = strlen(parseType->str);
			strcpy(parseType->responseStr, "^D");
			strcat(parseType->responseStr, itoa3(parseType->dataLen));
			strcat(parseType->responseStr, parseType->str);
			usart_putstr(parseType->responseStr, port);
			parseType->snmpComState = SNMP_IDLE;
			break;
		case ST3:
			// UPS on batteries
			if (parseType->parser == SNMP)
			{
				temp1 = (int)((upsData->ampOut + 0.5f) * 10.0f);
			}
			else
			{
				temp1 = (int)((upsData->loadPctOut + 0.5f) * 10.0f);
			}

			if (parseType->parser == SNMP)
			{
				temp2 = (int)(upsData->loadPctOut + 0.5f);
			}
			else
			{
				temp2 = (int)(upsData->ampOut + 0.5f);
			}

			// ST3-1 Output Source
			// The upsState may not use UPS_BYPASS so check bypass mode directly
			if ((upsBoss.bypassMode == MANUAL_ON) || (upsBoss.bypassMode == AUTO_ON) || (upsBoss.bypassMode == ON))
			{
				strcpy(parseType->str, "2"); 		// UPS on bypass
			}
			else									// Check other
			{
				switch (upsBoss.upsState)
				{
				case UPS_OFF:  // UPS on "Other"
				case UPS_INIT: // UPS on "Other"
					strcpy(parseType->str, "5");
					break;
				case UPS_ON_BAT:
				case UPS_SHUTDOWN:
					strcpy(parseType->str, "1"); // UPS on battery
					break;
				case UPS_ON_UTIL:
					if ((upsOne.dcMode == AUTO_ON) || (upsTwo.dcMode == AUTO_ON))
					{
						strcpy(parseType->str, "1"); // UPS on battery (shutting down)
					}
					else
					{
						strcpy(parseType->str, "0"); // UPS on utility "normal"
					}
					break;
				case UPS_BYPASS:
					strcpy(parseType->str, "2"); // UPS on bypass
					break;
				}
			}

			// ST3-1 Add Output Source to responseStr
			strcpy(parseType->responseStr, parseType->str); // output source, set above
			strcat(parseType->responseStr, ",");
			// ST3-2 Output Frequency
			strcat(parseType->responseStr, itoa((int)(upsData->freqOut * 10))); // freqOut
			// ST3-3 Output number of lines
			strcat(parseType->responseStr, ",1,");
			// ST3-4 Output Voltage 1
			if (upsBoss.upsState == UPS_OFF)
				strcat(parseType->responseStr, itoa((int)((0.0f + 0.5) * 10)));
	        else
				strcat(parseType->responseStr, itoa((int)((upsData->voltOut + 0.5) * 10)));
			strcat(parseType->responseStr, ",");

			// report output parameters if load is applied
#ifdef ZERO_POWER_REPORTING_AMPS
			if (upsData->ampOut < ZERO_POWER_REPORTING_AMPS)
			{
				// ST3-5 Output Current 1
				strcat(parseType->responseStr, itoa(0)); // temp1 ampOut or PowOut
				strcat(parseType->responseStr, ",");
				// ST3-6 Output Power 1
				strcat(parseType->responseStr, itoa(0)); // powOut
				strcat(parseType->responseStr, ",");
				// ST3-7 Output Load 1
				strcat(parseType->responseStr, itoa(0)); // temp1 PowOut or ampOut
				strcpy(parseType->str, parseType->responseStr);
			}
			else
			{
				// ST3-5 Output Current 1
				if (upsBoss.upsState == UPS_OFF)
				    strcat(parseType->responseStr, itoa(0));
				else
				    strcat(parseType->responseStr, itoa(temp1));
				strcat(parseType->responseStr, ",");
				// ST3-6 Output Power 1
				if (upsBoss.upsState == UPS_OFF)
				    strcat(parseType->responseStr, itoa((int)(0.0f + 0.5)));
				else
				    strcat(parseType->responseStr, itoa((int)(upsData->powOut + 0.5)));
				strcat(parseType->responseStr, ",");
				// ST3-7 Output Load 1
				if (upsBoss.upsState == UPS_OFF)
				    strcat(parseType->responseStr, itoa(0));
				else
				    strcat(parseType->responseStr, itoa(temp2));
				strcpy(parseType->str, parseType->responseStr);
			}
#endif // ZERO_POWER_REPORTING_AMPS
#ifdef ZERO_POWER_REPORTING_WATTS
			if (upsData->powOut < ZERO_POWER_REPORTING_WATTS)
			{
				// ST3-5 Output Current 1
				strcat(parseType->responseStr, itoa(0)); // temp1 ampOut or PowOut
				strcat(parseType->responseStr, ",");
				// ST3-6 Output Power 1
				strcat(parseType->responseStr, itoa(0)); // powOut
				strcat(parseType->responseStr, ",");
				// ST3-7 Output Load 1
				strcat(parseType->responseStr, itoa(0)); // temp1 PowOut or ampOut
				strcpy(parseType->str, parseType->responseStr);
			}
			else
			{
				// ST3-5 Output Current 1
				if (upsBoss.upsState == UPS_OFF)
				    strcat(parseType->responseStr, itoa(0));
				else
				    strcat(parseType->responseStr, itoa(temp1));
				strcat(parseType->responseStr, ",");
				// ST3-6 Output Power 1
				if (upsBoss.upsState == UPS_OFF)
				    strcat(parseType->responseStr, itoa((int)(0.0f + 0.5)));
				else
				    strcat(parseType->responseStr, itoa((int)(upsData->powOut + 0.5)));
				strcat(parseType->responseStr, ",");
				// ST3-7 Output Load 1
				if (upsBoss.upsState == UPS_OFF)
				    strcat(parseType->responseStr, itoa(0));
				else
				    strcat(parseType->responseStr, itoa(temp2));
				strcpy(parseType->str, parseType->responseStr);
			}
#endif // ZERO_POWER_REPORTING_WATTS
#ifdef ZERO_POWER_REPORTING_PERCENT_LOAD
			if (upsData->loadPctOut < ZERO_POWER_REPORTING_PERCENT_LOAD)
			{
				// ST3-5 Output Current 1
				strcat(parseType->responseStr, itoa(0)); // temp1 ampOut or PowOut
				strcat(parseType->responseStr, ",");
				// ST3-6 Output Power 1
				strcat(parseType->responseStr, itoa(0)); // powOut
				strcat(parseType->responseStr, ",");
				// ST3-7 Output Load 1
				strcat(parseType->responseStr, itoa(0)); // temp1 PowOut or ampOut
				strcpy(parseType->str, parseType->responseStr);
			}
			else
			{
				// ST3-5 Output Current 1
				if (upsBoss.upsState == UPS_OFF)
				    strcat(parseType->responseStr, itoa(0));
				else
				    strcat(parseType->responseStr, itoa(temp1));
				strcat(parseType->responseStr, ",");
				// ST3-6 Output Power 1
				if (upsBoss.upsState == UPS_OFF)
				    strcat(parseType->responseStr, itoa((int)(0.0f + 0.5)));
				else
				    strcat(parseType->responseStr, itoa((int)(upsData->powOut + 0.5)));
				strcat(parseType->responseStr, ",");
				// ST3-7 Output Load 1
				if (upsBoss.upsState == UPS_OFF)
				    strcat(parseType->responseStr, itoa(0));
				else
				    strcat(parseType->responseStr, itoa(temp2));
				strcpy(parseType->str, parseType->responseStr);
			}
#endif // ZERO_POWER_REPORTING_PERCENT_LOAD
			parseType->dataLen = strlen(parseType->str);
			strcpy(parseType->responseStr, "^D");
			strcat(parseType->responseStr, itoa3(parseType->dataLen));
			strcat(parseType->responseStr, parseType->str);
			usart_putstr(parseType->responseStr, port);
			parseType->snmpComState = SNMP_IDLE;
			break;
		case ST4:									  // Bypass information
			strcpy(parseType->responseStr, "^D001,"); // send message that feature is not supported
			usart_putstr(parseType->responseStr, port);
			parseType->snmpComState = SNMP_IDLE;
			break;
		case ST5:
			switch (upsBoss.upsState)
			{				 // Temperature alarm, ST5-1
			case UPS_ON_BAT: // only check Heat Exchanger when it is on
			case UPS_ON_UTIL:
			case UPS_SHUTDOWN: // Shutdown, transition state
				// 		temp warm		UPS					Cabinet Warm		Heat Exchanger warm
				if ((upsBoss.tSink2 > TEMP_HS_ALM_ON) || //(mspIsoIn[1] >= 1) || (mspIsoIn[3] < 1) ||
														 // 		temp Hot		UPS					Cabinet Hot	reversed	Heat Exchanger Hot
					(upsBoss.tSink2 > TEMP_HS_TRIP_ON) /* || (mspIsoIn[2] < 1) || (mspIsoIn[4] < 1) */)
				{
					strcpy(parseType->str, "1,");
				}
				else
				{
					strcpy(parseType->str, "0,");
				}
				break;
			default: // Heat Exchanger off
				// 		temp warm		UPS					Cabinet Warm
				if ((upsBoss.tSink2 > TEMP_HS_ALM_ON) || // (mspIsoIn[1] >= 1) ||
														 // 		temp Hot		UPS					Cabinet Hot	reversed
					(upsBoss.tSink2 > TEMP_HS_TRIP_ON) /* || (mspIsoIn[2] < 1)*/)
				{
					strcpy(parseType->str, "1,");
				}
				else
				{
					strcpy(parseType->str, "0,");
				}
				break;
			}

			if ((upsData->dcMode == AUTO_ON) || (upsData->dcMode == MANUAL_ON)) // Alarm Input Bad Alm = on battery, ST5-2
				strcat(parseType->str, "1,");
			else
				strcat(parseType->str, "0,");

			if (upsData->invFaultAlm == ON_ALARM) // Alarm Output Bad Alm = Inverter fault, ST5-3
			    strcat(parseType->str, "1,");
			else
				strcat(parseType->str, "0,");

			if (upsData->loadPctOut >= 105.0) // Overload Alm = Inverter fault, ST5-4
				strcat(parseType->str, "1,"); 
			else
				strcat(parseType->str, "0,");

			if (upsData->bypassState)
			    strcat(parseType->str, "1,"); // Bypass bad alarm, ST5-5
			else
			    strcat(parseType->str, "0,"); // Bypass bad alarm, ST5-5

			if ((upsData->invMode == AUTO_OFF) || (upsData->invMode == MANUAL_OFF)) // Output Bad Alm = Inverter off, ST5-6
				strcat(parseType->str, "1"); 
			else
				strcat(parseType->str, "0");
/*			
			strcat(parseType->str, ","); // UPS Shutdown, not implemented, ST5-7
			strcat(parseType->str, ","); // Charger Failure, not implemented, ST5-8
			strcat(parseType->str, ","); // System Off, not implemented, ST5-9
			if (upsData->fanFail == TRUE)
			{ // Cabinet Fan Status, ST5-10
				strcat(parseType->str, "1,");
			}
			else
			{
				strcat(parseType->str, "0,");
			}
			strcat(parseType->str, ","); // Fuse Failure, not implemented, ST5-11
										 //if ( (powerConditioner1Fault == TRUE) || 		// General Fault, ST5-12
										 //	(powerConditioner2Fault == TRUE) ) {
										 //	strcat(parseType->str, "1,");
										 //}
										 //else {
			strcat(parseType->str, "0,");
			strcat(parseType->str, ","); // Awaiting Power, not implemented, ST5-13
			strcat(parseType->str, ","); // Shutdown Pending, not implemented, ST5-14
			strcat(parseType->str, ","); // Shutdown Imminent, not implemented, ST5-15
										 //if (fanFailPdu()) {							// PDU Status, ST5-16
										 //	strcat(parseType->str, "1,");
										 //}
										 //else {
			strcat(parseType->str, "0,");
			strcat(parseType->str, "0,");
			strcat(parseType->str, "0,");
			strcat(parseType->str, itoa(upsOne.batSts)); // Battery1 Status, ST5-19
			strcat(parseType->str, ",");
			strcat(parseType->str, itoa(upsTwo.batSts)); // Battery2 Status, ST5-20
			strcat(parseType->str, ",");
			if (upsData->battleshort == TRUE)
			{ // Cabinet Battleshort, ST5-21
				strcat(parseType->str, "1");
			}
			else
			{
				strcat(parseType->str, "0");
			}
*/
			parseType->dataLen = strlen(parseType->str);
			strcpy(parseType->responseStr, "^D");
			strcat(parseType->responseStr, itoa3(parseType->dataLen));
			strcat(parseType->responseStr, parseType->str);
			usart_putstr(parseType->responseStr, port);
			parseType->snmpComState = SNMP_IDLE;
			break;
		case ST6: // Socket Persistant delay
			parseType->snmpComState = SNMP_IDLE;
			break;
		case ST7: // Socket Status
			parseType->snmpComState = SNMP_IDLE;
			break;
		case STR: // Test Results Summary, Summary = 1 (passed), results detail (not implemented)
			usart_putstr("^D0011", port);
			parseType->snmpComState = SNMP_IDLE;
			break;
		case UBR: // Poll and Set, Baud rate 1200, 2400, 4800, 9600, 19200
			strcpy(parseType->str, "9600");
			parseType->dataLen = strlen(parseType->str);
			//			sprintf(parseType->responseStr,"^D%03d%s", parseType->dataLen,parseType->str);
			strcpy(parseType->responseStr, "^D");
			strcat(parseType->responseStr, itoa3(parseType->dataLen));
			strcat(parseType->responseStr, parseType->str);
			usart_putstr(parseType->responseStr, port);
			parseType->snmpComState = SNMP_IDLE;
			break;
		case UID: // UPS Identification, if you had more than one you could number them 1-xx
 			if (parseType->type == POLL)
			{
				// To Do: Add UID to FLASH
				//if (!iRange(parseType->snmpUid, 0, 999))
				//{							// If value not in range
				//	parseType->snmpUid = 0; // then set to default value
				//}
				//strcpy((char *)parseType->str, itoa(parseType->snmpUid));
				strcpy((char *)parseType->str, itoa(1));
				parseType->dataLen = strlen((char *)parseType->str);
				strcpy((char *)parseType->responseStr, "^D");
				strcat((char *)parseType->responseStr, itoa3(parseType->dataLen));
				strcat((char *)parseType->responseStr, (char *)parseType->str);
				usart_putstr(parseType->responseStr, parseType->snmpPort);
			}
			else if (parseType->type == SET)
			{
				scanSnmpParams(parseType);
				temp1 = atoi((char *)&parseType->StrParams[0][0]);
				parseType->snmpUid = (int)temp1;
				strcpy((char *)parseType->responseStr, "^1");
				usart_putstr(parseType->responseStr, parseType->snmpPort);
				pEventData->systemData.snmpUid = (uint16_t)parseType->snmpUid;
				saveConfiguration(); // Save the value into long term storage (aka the SPI Flash for configuration)
			}
			parseType->snmpComState = SNMP_IDLE;
			break;
		case VER:
			if (strlen(upsData->verSoftware) < (SNMP_STR_MAX - 1))
			{
				strcpy(parseType->str, upsData->verSoftware);
			}
			else
			{
				strcpy(parseType->str, "Unknown, string length error");
			}
			parseType->dataLen = strlen(parseType->str);
			strcpy(parseType->responseStr, "^D");
			strcat(parseType->responseStr, itoa3(parseType->dataLen));
			strcat(parseType->responseStr, parseType->str);
			usart_putstr(parseType->responseStr, port);
			parseType->snmpComState = SNMP_IDLE;
			break;
		case XD1:
			temp[1] = (int)((upsData->voltBus + 0.5) * 10);	// average Vbus
			temp[2] = (int)(upsData->tSink2 + 0.5);			   // max tSink2
			temp[4] = (int)((upsData->voltSupply + 0.5) * 10); // max Vsupply
															   //			sprintf(parseType->str,"%1.3f,%d,%d,%1.2f,%d",((upsData->ampChg + 0.5)*10),temp[1],temp[2],upsData->pfOut,temp[4]);
			strcpy(parseType->str, itoa((int)((upsData->ampChg + 0.5) * 10)));
			strcat(parseType->str, ",");
			strcat(parseType->str, itoa(temp[1]));
			strcat(parseType->str, ",");
			strcat(parseType->str, itoa(temp[2]));
			strcat(parseType->str, ",");
			strcat(parseType->str, itoa((int)upsData->pfOut));
			strcat(parseType->str, ",");
			strcat(parseType->str, itoa(temp[4]));
			parseType->dataLen = strlen(parseType->str);
			strcpy(parseType->responseStr, "^D");
			strcat(parseType->responseStr, itoa3(parseType->dataLen));
			strcat(parseType->responseStr, parseType->str);
			usart_putstr(parseType->responseStr, port);
			parseType->snmpComState = SNMP_IDLE;
			break;
		case NO_SNMP_CMD:
		default:
			usart_putstr("^0", port); // cmd not understood or not implemented
			parseType->snmpComState = SNMP_IDLE;
			break;
		}
		break;
	default:
		parseType->snmpComState = SNMP_IDLE;
		parseType->lastSnmpComState = SNMP_RESPONSE;
		break;
	} // end switch
}
