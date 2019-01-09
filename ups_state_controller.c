// ups_state_controller.c

//#include "types.h"
#include "ups_state_controller.h"
#include "ups_com.h"
#include "epwm.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
//#include "ups.h"
#include "display_led.h"
//#include "timer.h"
#include "utilities.h"
#include "system_config.h" // includes file name for configuration
#include SYSTEM_CONFIG	 // configuration header file
#include "Event_Controller.h"
#include "log.h"

#if (defined VERBOSE_DEBUG)
#include "uart.h"
#endif

uint32_t BRT = 0;
uint32_t lastBRT = 0;

// Debug controllers declared in main
extern uint8_t startControllers;
extern event_controller_data_t eventData, *pEventData;

// SNMP Reboot
bool snmpReboot = FALSE;
timeT snmpTimer;
int snmpRebootDuration = 0;

// External variables
extern char cmdStr[50], responseStr[50];
extern int cmdStrPos; // pointer to last char in string
extern timeT runTime, delayedStartupTime;
timeT /*timeStarted, delayedStartupTime,*/ timeUpsOff;
volatile struct BANK_CONFIG_T configurationSwitches;

void setSnmpReboot(int duration)
{
     snmpReboot = TRUE;
	 snmpRebootDuration = duration;
}

void checkAmbientTemp(struct upsDataStrucT *upsData)
{
	static uint8_t ambientTempTrip = FALSE;

    if (!ambientTempTrip)
	{
        if (upsData->tAmb > TEMP_AMB_TRIP_ON)
		{
			sonalert(ON_BEEP);
            ambientTempTrip = TRUE;
#if (defined VERBOSE_DEBUG)
            if (upsData->verboseDebug == TRUE)
			{
				if (upsData->upsComMode == UPS_COM_USER)
				{
					log_trace("Sonalert Beep upsData->tAmb(%f) > TEMP_AMB_TRIP_ON(%f)", upsData->tAmb, TEMP_AMB_TRIP_ON);
				}
		    }
#endif			
		}
    }
		
	if (ambientTempTrip)
	{
		if (upsData->tAmb < TEMP_AMB_TRIP_OFF)
		{
			sonalert(OFF);
			ambientTempTrip = FALSE;
#if (defined VERBOSE_DEBUG)
			if (upsData->verboseDebug == TRUE)
			{
				if (upsData->upsComMode == UPS_COM_USER)
				{
					log_trace("Sonalert Off upsData->tAmb(%f) < TEMP_AMB_TRIP_OFF(%f)", upsData->tAmb, TEMP_AMB_TRIP_OFF);
				}
			}
#endif			
		}
	}

}
void init_ups_state_controller(struct upsDataStrucT *upsData)
{
#ifdef VERBOSE_DEBUG_START
	upsData->upsState = UPS_DEBUG_START;
#else
	upsData->upsState = UPS_INIT;
#endif
	upsData->lastUpsState = UPS_OFF;
	upsData->invMode = AUTO_OFF;
	upsData->dcMode = AUTO_OFF;
	upsData->chgMode = OFF;
	upsData->bypassMode = OFF;
	if (pEventData->systemData.bankSwitches.bit.INV_SYNC_TO_LINE == 1)
	{
		upsData->syncMode = AUTO_OFF;
	}
	else
	{
		upsData->syncMode = MANUAL_OFF;
	}
	upsData->displayButton = BUTTON_NONE;
	upsData->displayButtonCmd = BUTTON_NONE;
	upsData->bypassState = FALSE;
}

void ups_state_controller(struct upsDataStrucT *upsData)
{
	static timeT time1, time2, time3, overTempTimer, batteryRunTimer; // time4 not used
	static int utilityBack = FALSE;
	int inputInRange;

	switch (upsData->upsState)
	{
	case UPS_DEBUG_START:
	    if (startControllers) upsData->upsState = UPS_INIT;
        break;
	case UPS_INIT:										// Initial state when router/system powers up
		if (upsData->upsState != upsData->lastUpsState) // state entry code
		{
			upsData->lastUpsState = upsData->upsState;
			upsData->timeSystemStart = getTime();
			timeUpsOff = time1 = getTime(); // reset timers
			upsData->secOnBat = 0;			// reset battery run timer
			pinLatch(LATCH_IN_RLY1_n, 0);   // Turn on input Hot Relay, 0=closed (de-energized)
			pinLatch(LATCH_IN_RLY2_n, 0);   // Turn on input Neutral Relay
			pinLatch(LATCH_LVPS_SD, 0);
			pinLatch(LATCH_PFC_SD, 0);			// Turn on PFC
			pinLatch(LATCH_DC_DC_DISABLE_n, 1); // Enable DC/DC
			pinLatch(LATCH_PFC_BAT_CHK_n, 1);   // Disable Battery check
			upsBoss.chgCommand = AUTO_OFF;		// Turn off Charger
			BYPASS_FORCE_OFF1;
			BYPASS_FORCE_OFF2;
			upsData->displayMode = OFF;		// start with all Display LEDs off
			pinLatch(LATCH_AC_ENABLE_n, 1); // Make sure inverter is disabled at system startup

			upsData->invRangeMode = INV_120V; // TODO: Change this when 120/240 switch code added
#if (defined VERBOSE_DEBUG)
            if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					log_trace("ups init");
				}
		    }
#endif
		}
		if (timer(timeUpsOff, 2000)) // pause for inrush to finish
		{
			//addSystemEvent(EVENT_COLD_START);
			pinLatch(LATCH_PFC_SD, 0);							// Turn on PFC
			upsData->displayMode = ON;							// Turn on Display LEDs
			fakeButton(BUTTON_TEST);							// do light show

#if (!defined BATTERY_COLD_START)								// set in system_config.h
			if ((pEventData->systemData.bankSwitches.bit.AUTOSTART == 1) // auto restart active
																// input within operational voltage range
				&& (fRange(upsData->voltIn, UPS_VOLTIN_LOW * upsData->voltInNom, UPS_VOLTIN_HIGH * upsData->voltInNom)))
			{
#else
			// Just check autorestart
			if (pEventData->systemData.bankSwitches.bit.AUTOSTART == 1) // auto restart active
			{
#endif

				upsData->upsState = UPS_STARTUP;

#if (defined VERBOSE_DEBUG)
                if ( (upsBoss.verboseDebug == TRUE) || (VERBOSE_DEBUG_PORT == UART_PORT1) )
			    {
					if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						log_trace("ups init, auto restart => ups startup");
					}
				}
#endif
			}
			else
			{
				upsData->upsState = UPS_OFF;
#if (defined VERBOSE_DEBUG)
                if ( (upsBoss.verboseDebug == TRUE) || (VERBOSE_DEBUG_PORT == UART_PORT1) )
			    {
					if ( ( upsBoss.upsComMode == UPS_COM_USER ) || (VERBOSE_DEBUG_PORT == UART_PORT1) )
					{
						log_trace("ups init => ups off");
					}
				}
#endif
			}
		}
		break;
	case UPS_STARTUP:									// System powered but Inverter off
		if (upsData->upsState != upsData->lastUpsState) // state entry code
		{
			upsData->lastUpsState = upsData->upsState;
			time1 = getTime(); // reset timers
			upsData->inverterCommand = AUTO_ON;
			upsData->SubState = 1;
			upsData->SubStateLast = 0;
#if (defined VERBOSE_DEBUG)
            if ( (upsBoss.verboseDebug == TRUE) || (VERBOSE_DEBUG_PORT == UART_PORT1) )
			{
				if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					log_trace("ups startup");
				}
			}
#endif
		}
		switch (upsData->SubState)
		{
		case 1:
			//setFrequency(UPS_FREQOUTNOM, 2.0); // parameters (frequency, hz/sec rate)
			upsData->invVoltSet = 0;		   // make sure starting at zero volts
			if (timer(timeUpsOff, 2000))
			{
				time1 = getTime();
				upsData->SubState++;
			}
			break;
		case 2:
			if (upsData->voltOut >= (0.85 * UPS_VOLTOUTNOM))
			{
				upsData->upsState = UPS_ON_UTIL;
			}
			if (upsData->invMode == FAULT) // this is checked in inverter()
			{
				upsData->upsState = UPS_FAULT;
			}
			break;
		default:
			break;
		}
		break;
	case UPS_ON_UTIL:									// On Utility, inverter on
		if (upsData->upsState != upsData->lastUpsState) // state entry code
		{
			upsData->lastUpsState = upsData->upsState;
			time1 = getTime(); // reset timers
			time2 = getTime(); // shutdown timer reset
			overTempTimer = getTime();			// reset timers
			upsData->secOnBat = 0;				// reset battery run timer
			upsData->dcMode = AUTO_OFF;			// reset this if coming off battery run
			LAN_OFF_BAT;						// LAN Signal relay
			pinLatch(LATCH_DC_DC_DISABLE_n, 0); // Disable DC/DC
			pinLatch(LATCH_PFC_SD, 0);			// Turn on PFC
			pinLatch(LATCH_IN_RLY1_n, 0);		// Turn on input Hot Relay, 0=closed (de-energized)
			pinLatch(LATCH_IN_RLY2_n, 0);		// Turn on input Neutral Relay
			pinLatch(LATCH_LVPS_SD, 0);
#ifndef BATTERY_CHARGE_CYCLING2			
			upsBoss.chgCommand = AUTO_FAST; // Charger to Fast Charge
#endif
			addSystemEvent(EVENT_ON_UTILITY);
			if (upsData->updateLastBRT)
			{
                lastBRT = BRT;
				upsData->updateLastBRT = FALSE;
			}
#if (defined VERBOSE_DEBUG)
            if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					log_trace("on util state");
				}
			}
#endif
		}

        checkAmbientTemp(upsData);

		if (snmpReboot)
		{
			snmpTimer = getTime();
			upsData->upsState = UPS_OFF;
		}
#ifndef NO_BAT
		// use fast bus update (once per frame) to see if there is a loss of input 60/128=130us
		if (timer(upsData->timeSystemStart, 5000)) // wait after cold start before checking bus
		{
			if (upsBossUnfilt.voltBusFast <= (0.875f * DC_BUS_VOLTAGE))
			{
				pinLatch(LATCH_DC_DC_DISABLE_n, 1); // Enable DC/DC
				upsData->upsState = UPS_ON_BAT;		// so switch to battery mode
#if (defined VERBOSE_DEBUG)
                if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			    {
					if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						log_trace("on util => battery, DC bus low: upsBossUnfilt.voltBusFast(%f) <= 0.875f * DC_BUS_VOLTAGE)(%f)", upsBossUnfilt.voltBusFast, (0.875f * DC_BUS_VOLTAGE));
					}
				}
#endif
			}
		}
#endif
		// commanded off
		if (timer(time1, 1000)) // wait a tad before checking
		{
			if (upsBoss.displayButtonCmd == BUTTON_OFF) // if command sent
			{
				upsBoss.displayButtonCmd = BUTTON_NONE; // clear it
				upsData->upsState = UPS_OFF;			// go to off state
				
#if (defined VERBOSE_DEBUG)
                if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			    {
					if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						log_trace("on util, off button pressed");
					}
				}
#endif
			}
			if (upsBoss.displayButtonCmd == BUTTON_BYPASS) // if command sent/
			{

				upsBoss.displayButtonCmd = BUTTON_NONE; // clear it

				// Toggle between Forced On (MANUAL_ON) and Auto (AUTO_OFF)
				if (upsBoss.bypassMode == AUTO_OFF) // Check for Auto Mode
				{
					upsBoss.bypassCommand = MANUAL_ON; // Switch to Forced On Mode
#if (defined VERBOSE_DEBUG)
                    if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			        {
						if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
						{
						    log_trace("Bypass Toggle button pressed: Manual On");
						}
				    }
#endif
				}
				else if (upsBoss.bypassMode == MANUAL_ON) // Check for Forced On Mode
				{
					upsBoss.bypassCommand = AUTO_OFF; // Switch to Auto Mode
#if (defined VERBOSE_DEBUG)
                    if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			        {
						if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
						{
							log_trace("Bypass Toggle button pressed: Auto Off");
						}
					}
#endif
				}
			}
			// WPB

			if (upsBoss.bypassState == FALSE)		// Note:  no OFF, only TRUE or FALSE
			{
				if ((upsBoss.invMode == AUTO_OFF) || (upsBoss.invMode == MANUAL_OFF))
				{
					upsData->upsState = UPS_OFF; // go to off state
#if (defined VERBOSE_DEBUG)
					if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
						{
							log_trace("on util, inverter mode changed to : %s", getOpmodeStr(upsBoss.invMode));
						}
					}
#endif
				}
			}
		} // if (timer(time1, 1000))

		// Check Heatsink over temp (Hot) conditional shutdown
		if (timer(overTempTimer, 20000)) // give fans time to get to speed
		{
			// Check Ambient Tempurature
// 			if ( (upsBoss.battleshort == FALSE) && (upsData->tAmb > TEMP_AMB_TRIP_ON) )
// 			{
// 				upsData->upsState = UPS_SHUTDOWN;
// #if (defined VERBOSE_DEBUG)
//                 if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
// 			    {
// 					if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ))
// 					{
// 						log_trace("on util => ups shutdown, ambient/cabinet overtemp");
// 					}
// 				}
// #endif
// 			}
#ifdef HEAT_SYNC_1_ENABLE
			if ((upsBoss.battleshort == FALSE) && (upsBoss.tSink1 > TEMP_HS_TRIP_ON))
			{
				upsData->upsState = UPS_SHUTDOWN;
#if (defined VERBOSE_DEBUG)
                if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			    {
					if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						log_trace("on util => ups shutdown, hs overtemp");
					}
				}
#endif
			}
#endif
		}
		if (upsData->delayedShutdown == FALSE) // delayed shutdown off
		{
			if (upsData->secShutdownDelay != -1) // shutdown timer command
			{
				time2 = getTime();				 // set timer start time
				upsData->delayedShutdown = TRUE; // set delayed shutdown state
			}
		}
		else // delayed shutdown on, timer running
		{
			if (upsData->secShutdownDelay != -1)
			{
				if (timer(time2, upsBoss.secShutdownDelay * 1000))
				{
					upsData->secShutdownDelay = -1; // reset so after unit is turned back on it doesn't shut down again
					upsData->upsState = UPS_OFF;	// go to shutdown state, cycle outlets off
#if (defined VERBOSE_DEBUG)
                    if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			        {
						if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
						{
							log_trace("on util, shutdown delay");
						}
				    }
#endif
				}
			}
			else // shutdown has been cancelled
			{
				upsData->delayedShutdown = FALSE;
			}
		}

		// input low voltage force to battery
		switch (pEventData->systemData.bankSwitches.bit.INPUT_BAT_SETPOINT)
		{
		case 0:
			if (upsData->voltIn <= 85.0)
			{
				upsData->upsState = UPS_ON_BAT; // so switch to battery mode
				pinLatch(LATCH_IN_RLY1_n, 0);   // open input relay hot, force UPS to battery
				pinLatch(LATCH_IN_RLY2_n, 0);   // open input relay neutral
#if (defined VERBOSE_DEBUG)
                if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			    {
					if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						log_trace("on Utility, out of range case0 => on bat");
					}
				}
#endif
			}
			break;
		case 1:
			if (upsData->voltIn <= 90.0)
			{
				upsData->upsState = UPS_ON_BAT; // so switch to battery mode
				pinLatch(LATCH_IN_RLY1_n, 0);   // open input relay hot, force UPS to battery
				pinLatch(LATCH_IN_RLY2_n, 0);   // open input relay neutral
#if (defined VERBOSE_DEBUG)
                if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			    {
					if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						log_trace("on Utility, out of range case1 => on bat");
					}
				}
#endif
			}
			break;
		case 2:
			if (upsData->voltIn <= 95.0)
			{
				upsData->upsState = UPS_ON_BAT; // so switch to battery mode
				pinLatch(LATCH_IN_RLY1_n, 0);   // open input relay hot, force UPS to battery
				pinLatch(LATCH_IN_RLY2_n, 0);   // open input relay neutral
#if (defined VERBOSE_DEBUG)
                if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			    {
					if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						log_trace("on Utility, out of range case2 => on bat");
					}
				}
#endif
			}
			break;
		case 3:
			if (upsData->voltIn <= 100.0)
			{
				upsData->upsState = UPS_ON_BAT; // so switch to battery mode
				pinLatch(LATCH_IN_RLY1_n, 0);   // open input relay hot, force UPS to battery
				pinLatch(LATCH_IN_RLY2_n, 0);   // open input relay neutral
#if (defined VERBOSE_DEBUG)
                if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			    {
					if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						log_trace("on Utility, out of range case3 => on bat");
					}
				}
#endif
			}
			break;
		}
		break;
	case UPS_ON_BAT:									// On battery, inverter on
		if (upsData->upsState != upsData->lastUpsState) // state entry code
		{
			upsData->lastUpsState = upsData->upsState;
			upsData->updateLastBRT = TRUE;
			time1 = getTime(); // reset timers
			time2 = getTime(); // reset timers
			time3 = getTime(); // reset timers, use shutdown timing
			batteryRunTimer = getTime(); // note start of battery operation
			utilityBack = FALSE;
			upsData->dcMode = AUTO_ON;	 // Reflect this in virtual UPS
			LAN_ON_BAT;					   // LAN Signal relay, low battery in battery function
			pinLatch(LATCH_PFC_SD, 1);	 // Turn off PFC
			pinLatch(LATCH_IN_RLY1_n, 1);  // open input relay hot, allow UPS to transfer to utility
			pinLatch(LATCH_IN_RLY2_n, 1);  // open input relay neutral
			upsBoss.chgCommand = AUTO_OFF; // Turn off Charger
			addSystemEvent(EVENT_ON_BATTERY);
#if (defined VERBOSE_DEBUG)
            if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					log_trace("on battery state");
				}
			}
#endif
		}

        checkAmbientTemp(upsData);

		if (DC_DC_ON_NOT == 1) // DC/DC is not enabled or too high
		{
			pinLatch(LATCH_DC_DC_DISABLE_n, 1); // Enable DC/DC
		}

		inputInRange = fRange(upsData->voltIn, UPS_VOLTIN_LOW * upsData->voltInNom, UPS_VOLTIN_HIGH * upsData->voltInNom);
		if ((inputInRange) && (utilityBack == FALSE) && (timer(time1, 5000)))
		{
			time1 = getTime();
			utilityBack = TRUE;
			pinLatch(LATCH_IN_RLY1_n, 0);		// close input relay hot, allow UPS to transfer to utility
			pinLatch(LATCH_IN_RLY2_n, 0);		// close input relay neutral
			pinLatch(LATCH_DC_DC_DISABLE_n, 0); // Disable DC/DC
			pinLatch(LATCH_PFC_SD, 0);			// Turn on PFC
#if (defined VERBOSE_DEBUG)
            if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					log_trace("on bat, utility in range");
				}
			}
#endif
		}
		switch (utilityBack)
		{
		case TRUE:
			if ((inputInRange) && (upsBossUnfilt.voltBusFast >= (0.95 * DC_BUS_VOLTAGE)))
			{
				if (timer(time1, 5000)) // wait for stable operation PFC
				{
					utilityBack = 2;
				}
			}
			else if (upsBossUnfilt.voltBusFast <= (0.93 * DC_BUS_VOLTAGE)) // lost utility again
			{
				pinLatch(LATCH_DC_DC_DISABLE_n, 1); // Enable DC/DC
				pinLatch(LATCH_PFC_SD, 1);			// Turn off PFC
				pinLatch(LATCH_IN_RLY1_n, 1);		// open input relay hot, allow UPS to transfer to utility
				pinLatch(LATCH_IN_RLY2_n, 1);		// open input relay neutral
				utilityBack = FALSE;
			}
			break;
		case 2:
			pinLatch(LATCH_DC_DC_DISABLE_n, 0); // Disable DC/DC
			upsData->upsState = UPS_ON_UTIL;
#if (defined VERBOSE_DEBUG)
            if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					log_trace("UPS_ON_UTIL");
				}
			}
#endif
			break;
		default:
		    break;
		}

		if (upsBoss.voltBat <= upsBoss.lowBatShutdown) // low battery shutdown, see ups.c batteries()
		{
			upsData->upsState = UPS_SHUTDOWN;
#if (defined VERBOSE_DEBUG)
            if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					log_trace("on bat => low battery shutdown: upsBoss.voltBat(%f) <= upsBoss.lowBatShutdown(%f)", upsBoss.voltBat, upsBoss.lowBatShutdown);
				}
			}
#endif
		}
#ifdef HEAT_SYNC_1_ENABLE
		// High temperature with Battleshort off
		if ((upsBoss.battleshort == FALSE) && (upsBoss.tSink1 > TEMP_HS_TRIP_ON))
		{
			upsData->upsState = UPS_SHUTDOWN;
#if (defined VERBOSE_DEBUG)
            if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					log_trace("on bat, hs overtemp");
				}
			}
#endif
		}
#endif		
		if (upsBoss.displayButtonCmd == BUTTON_OFF) // if command sent
		{
			upsBoss.displayButtonCmd = BUTTON_NONE; // clear it
			upsData->inverterCommand = AUTO_OFF;
#if (defined VERBOSE_DEBUG)
            if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					log_trace("on bat, button off");
				}
			}
#endif
		}
		if (upsBoss.batMode == AUTO_ON_SHUTDOWN)
		{
			upsData->upsState = UPS_SHUTDOWN; // sequence off
#if (defined VERBOSE_DEBUG)
            if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					log_trace("on bat, batMode==auto_on_shutdown");
				}
			}
#endif
		}

		switch (upsBoss.invMode)
		{
		case AUTO_ON:
		case MANUAL_ON:
			time3 = getTime();
			break;
		case AUTO_OFF:
		case MANUAL_OFF:
			if (timer(time3, 5000))
			{
#if (defined VERBOSE_DEBUG)
                if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			    {
					if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						log_trace("on bat, inverter off, going to shutdown");
					}
				}
#endif
				upsData->upsState = UPS_SHUTDOWN; // sequence off
			}
			break;
		case FAULT:						   // Inverter fault or overcurrent
			upsData->upsState = UPS_FAULT; // sequence off
			break;
		}
		if (upsData->delayedShutdown == FALSE) // delayed shutdown off
		{
			if (upsData->secShutdownDelay != -1) // shutdown timer command
			{
				time2 = getTime();				 // set timer start time
				upsData->delayedShutdown = TRUE; // set delayed shutdown state
			}
		}
		else // delayed shutdown on, timer running
		{
			if (upsData->secShutdownDelay != -1)
			{
				if (timer(time2, upsBoss.secShutdownDelay * 1000))
				{
					upsData->secShutdownDelay = -1; // reset so after unit is turned back on it doesn't shut down again
					upsData->upsState = UPS_OFF;	// go to shutdown state, cycle outlets off
#if (defined VERBOSE_DEBUG)
                    if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			        {
						if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
						{
							log_trace("on bat, shutdown delay");
						}
					}
#endif
				}
			}
			else // shutdown has been cancelled
			{
				upsData->delayedShutdown = FALSE;
			}
		}
		upsData->secOnBat = timeDiffMsec(batteryRunTimer, getTime()) * 1.0 / 1000; // how long on battery in sec
		break;
	case UPS_OFF:										// System powered but Inverter off
		if (upsData->upsState != upsData->lastUpsState) // state entry code
		{
			upsData->lastUpsState = upsData->upsState;
			time1 = time2 = time3 = getTime(); // reset timers
			upsData->SubState = 1;
			upsData->SubStateLast = 0;
			pinLatch(LATCH_IN_RLY1_n, 0); // Turn on input Hot Relay, 0=closed (de-energized)
			pinLatch(LATCH_IN_RLY2_n, 0); // Turn on input Neutral Relay
			pinLatch(LATCH_LVPS_SD, 0);
			pinLatch(LATCH_PFC_SD, 0);			// Turn on PFC
			pinLatch(LATCH_DC_DC_DISABLE_n, 0); // Disable DC/DC
			upsBoss.bypassCommand = MANUAL_OFF; // Turn off Bypass
			upsData->inverterCommand = AUTO_OFF;
			// if system starts up with inverter off, make sure charger is turned on
			if (upsBoss.chgMode == AUTO_OFF)
			{
				upsBoss.chgCommand = AUTO_FAST; // Charger to Fast Charge
			}
//addDebugEvent("UPS State entered: UPS_OFF",9);
#if (defined VERBOSE_DEBUG)
            if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					log_trace("ups off state");
				}
			}
#endif
		}
		if (timer(time1, 1000))
		{
#if (defined CHECK_INPUT_VRANGE)
			// button pressed or inverter already on and in operational voltage range
			if (((upsBoss.displayButtonCmd == BUTTON_ON) || (upsBoss.invMode == AUTO_ON) || (upsBoss.invMode == MANUAL_ON)) && (fRange(upsData->voltIn, UPS_VOLTIN_LOW * upsData->voltInNom, UPS_VOLTIN_HIGH * upsData->voltInNom)))
			{
#else
			// button pressed or inverter already on
			if ((upsBoss.displayButtonCmd == BUTTON_ON) || (upsBoss.invMode == AUTO_ON) || (upsBoss.invMode == MANUAL_ON))
			{
#endif
				if (upsBoss.displayButtonCmd == BUTTON_ON) // if it was a command...
				{
					upsBoss.displayButtonCmd = BUTTON_NONE; // clear command
#if (defined VERBOSE_DEBUG)
                    if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			        {
						if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
						{
							log_trace("ups off state, On button pressed");
						}
					}
#endif
				}
				else
				{
#if (defined VERBOSE_DEBUG)
                    if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
						{
							log_trace("ups off state, Inverter on Auto or Manual");
						}
					}
#endif
				}
				upsData->upsState = UPS_STARTUP;
			}
		}
		// use fast bus update (once per frame) to see if there is a loss of input 60/128=130us
		if (upsBossUnfilt.voltBusFast <= (0.95 * DC_BUS_VOLTAGE))
		{
			pinLatch(LATCH_DC_DC_DISABLE_n, 1); // Enable DC/DC
			upsData->upsState = UPS_ON_BAT;		// so switch to battery mode
#if (defined VERBOSE_DEBUG)
            if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					log_trace("off => battery, DC bus low: upsBossUnfilt.voltBusFast(%f) <= (0.95 * DC_BUS_VOLTAGE)(%f)", upsBossUnfilt.voltBusFast, (0.95f * DC_BUS_VOLTAGE));
				}
			}
#endif
		}
		if (upsData->delayedStartup == FALSE) // delayed shutdown off
		{
			if (upsData->secStartupDelay != -1) // shutdown timer command
			{
				time2 = getTime();				// set timer start time
				upsData->delayedStartup = TRUE; // set delayed shutdown state
			}
		}
		else // delayed shutdown on, timer running
		{
			if (upsData->secStartupDelay != -1)
			{
				if (timer(time2, upsBoss.secStartupDelay * 1000))
				{
					upsData->secStartupDelay = -1;   // reset so after unit is turned back on it doesn't shut down again
					upsData->upsState = UPS_STARTUP; // go to shutdown state, cycle outlets off
#if (defined VERBOSE_DEBUG)
                    if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
						{
							log_trace("off, startup delay");
						}
				    }
#endif
				}
			}
			else // shutdown has been cancelled
			{
				upsData->delayedStartup = FALSE;
			}
		}
		if (snmpReboot)
		{
			if ( timer(snmpTimer, snmpRebootDuration))
			{
				snmpReboot = FALSE;
				upsData->upsState = UPS_STARTUP;
			}
		}
		break;
	case UPS_SHUTDOWN:									// Shutdown, transition state
		if (upsData->upsState != upsData->lastUpsState) // state entry code
		{
			upsData->displayMode = LIGHT_SHOW;
			upsData->lastUpsState = upsData->upsState;
			upsData->inverterCommand = AUTO_OFF;
			time1 = getTime(); // reset timers
			time2 = getTime(); // reset timers
			upsData->SubState = 1;
			upsData->SubStateLast = 0;
			upsData->delayedShutdown = FALSE; // if pending, delayed shutdown off
			upsData->secShutdownDelay = -1;   // set shutdown timer command to off
			addSystemEvent(EVENT_SYSTEM_SHUTDOWN);
			saveConfiguration();
#if (defined VERBOSE_DEBUG)
			if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					log_trace("ups shutdown state");
				}
			}
#endif
			sonalert(OFF); // Turn off battery beep
		}
		switch (upsData->SubState)
		{
		case 1:
			upsData->SubState++;
			break;
		case 2:
			upsData->SubState++;
			break;
		case 3:
			upsData->SubState++;
			break;
		case 4:
			upsData->SubState++;
			break;
		case 5:
			if (timer(time1, 5000))
			{
				time1 = getTime(); // reset timer
				upsData->SubState++;
			}
			break;
		case 6:
			if (timer(time1, 10000))
			{
				upsData->upsState = UPS_INIT; // didn't shut down, restart
			}
			else
			{
				if ( upsBoss.displayMode == ON )
                {
			        pinLatch(LATCH_DC_DC_DISABLE_n, 0); // Disable DC/DC
			        pinLatch(LATCH_LVPS_SD, 1);			// Disable LVPS 
				}
			}
			break;
		}
		break;
	case UPS_BYPASS:									// System powered but Inverter off
		if (upsData->upsState != upsData->lastUpsState) // state entry code
		{
			upsData->lastUpsState = upsData->upsState;
			time1 = time2 = getTime(); // reset timers
			//addDebugEvent("UPS State entered: UPS_OFF",9);
			// AUTO_ON
			upsData->invMode = MANUAL_OFF;
#if (defined VERBOSE_DEBUG)
            if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					log_trace("ups bypass state");
				}
			}
#endif
		}
		break;
	case UPS_FAULT:										// System powered, indicating fault
		if (upsData->upsState != upsData->lastUpsState) // state entry code
		{
			upsData->lastUpsState = upsData->upsState;
			time1 = time2 = getTime(); // reset timers
#if (defined VERBOSE_DEBUG)
            if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					log_trace("ups fault state");
				}
			}
#endif
		}
		pinLatch(LATCH_AC_ENABLE_n, 1); // disable Inverter
		break;
	case UPS_COM_SHUTDOWN:								// System powered, indicating fault
		if (upsData->upsState != upsData->lastUpsState) // state entry code
		{
			upsData->lastUpsState = upsData->upsState;
			time1 = getTime();	 // reset timers
			upsData->SubState = 1; // Case sequence, start at 1
#if (defined VERBOSE_DEBUG)
            if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					log_trace("ups comm shutdown state");
				}
			}
#endif
		}
		break;
	default:
		if (upsData->upsState != upsData->lastUpsState) // state entry code
		{
			upsData->lastUpsState = upsData->upsState;
			time1 = getTime(); // reset timers
#if (defined VERBOSE_DEBUG)
            if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			{
				if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
				{
					log_trace("ups default state, no valid state");
				}
			}
#endif
		}
		upsData->upsState = UPS_FAULT;
		break;
	}
	upsData->notifyMsg = upsOne.notifyMsg = upsTwo.notifyMsg = FALSE;
}

