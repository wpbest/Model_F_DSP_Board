// ups_com.h

#ifndef UPS_COM_H
#define UPS_COM_H

#include "ups_state_controller.h"
/*
#define CMD_STR_MAX 15
#define MAX_MASTER_CMDS 20
*/

// Command Processor
typedef enum
{
	CMD_WAIT, // primary command state
	CMD_PROCESS
} cmd_states_t;

/*
typedef enum {
	OFF,
	ON,
	AUTO,
	MANUAL,
	AUTO_OFF,
	AUTO_ON,
	AUTO_SLOW,
	AUTO_MED,
	AUTO_FAST,
	MANUAL_OFF,
	MANUAL_ON,
	MANUAL_SLOW,
	MANUAL_MED,
	MANUAL_FAST,
	ON_ALARM,		// from this point on there isn't Model F equivalent mode expressed in RDAT03
	OFF_ALARM,
	NORMAL,
	WARN,
	FAULT,
	NULL_MODE = -1		// mode not found or invalid
} operatingModesT;
*/
extern struct upsDataStrucT upsOne, upsTwo, upsBoss, *pUpsOne, *pUpsTwo, *pUpsBoss;
extern struct upsDataStrucT *pUpsMain, *pUps, *pUpsDisplay, *pUpsLcd;

// debug
extern int debugCyclicChargingNextCycle;

// function prototypes
//void init_ups_com(struct upsDataStrucT *upsData);
void CommandProcessor(int port);
void tab(int tabTo);
void tabStr(int tabTo, char *str);
void tabularData(struct upsDataStrucT *ups, int testParser, int port);
char *getOpmodeStr(operatingModesT opmode);

#endif // _ups_com_h
