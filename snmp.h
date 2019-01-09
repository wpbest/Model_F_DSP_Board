// snmp.h

#ifndef _snmp_h_ // inclusion guard
#define _snmp_h_

#include "timer.h"
#include "types.h"

// snmpParse definitions

typedef enum
{
	SNMP_IDLE,
	SNMP_WAITING,
	SNMP_RESPONSE,
	SNMP_USER_MODE
} snmpStates_t;

typedef enum
{
	SNMP,
	UPSILON
} snmpParserT;

typedef enum
{
	START,
	LEN,
	DATA
} snmpPartT;

typedef enum
{
	POLL,
	SET,
	FAIL,
	UNKNOWN
} snmpTypeT;

typedef enum
{
	AP1, // Poll commands, some Set commands
	AP2,
	ATR, // Poll and Set
	BTS,
	MAN,
	MOD,
	NOM,
	OTC,
	OTD,
	RWD,
	ST1,
	ST2,
	ST3,
	ST4,
	ST5,
	ST6,
	ST7,
	SDA, // Poll and Set
	STD,
	STR,
	UBR,
	UID,
	VER,
	XD1,
	PSD, // Set command unique
	NO_SNMP_CMD
} snmpParsePollEnumT;

#define SNMP_STR_MAX 200
#define SNMP_STR2_MAX 50

extern struct snmpDataStruct
{
	int snmpPort;
	snmpParserT parser;
	int snmpUid;
	snmpStates_t snmpComState, lastSnmpComState;
	char str[SNMP_STR_MAX], aLen[SNMP_STR2_MAX], aData[SNMP_STR_MAX], responseStr[SNMP_STR_MAX], snmpChar;
	int pos, subPos, dataLen, comOkay;
	snmpTypeT type;
	snmpPartT phase;
	snmpParsePollEnumT snmpCmd;
	timeT timeOutStart;					  // timeOut for waiting for response
	char StrParams[PARAM_NUM_MAX][PARAM_LEN_MAX]; //	when parsed each parameter will be in a string form up to 20 params
	int lastParam;								  //	last parameter location, -1 if none, 0 if 1
} snmpDataStruct;

// function prototypes

void init_snmp_com(struct snmpDataStruct *parseType);
void snmp_com(struct upsDataStrucT *upsData, struct snmpDataStruct *parseType, int port);

#endif // _snmp_h_
