// display_led.c

#include "display_led.h"
#include "ups_state_controller.h"
#include "system_config.h" // includes file name for configuration
#include SYSTEM_CONFIG	 // configuration header file
#include "utilities.h"
#include "types.h"
#include "event_controller.h"

// external variables
/*
struct upsDataStrucT upsOne, upsTwo, upsBoss, *pUpsOne, *pUpsTwo, *pUpsBoss;
struct upsDataStrucT *pUpsMain, *pUps, *pUpsDisplay, *pUpsLcd;
*/
extern event_controller_data_t eventData, *pEventData;
extern uint8_t overloadBypassOn;

void initDisplay(void)
{
	int i, j;

	for (i = 0; i < 4; i++)
	{
		for (j = 0; j < 4; j++)
		{
			upsBoss.ledState1[i][j] = upsBoss.ledState2[i][j] = 0;
		}
	}
}

int displayLed(void)
{
	static int bankSetMode = FALSE, bankSetModeLast = TRUE;
	static uint8_t loadBarLEDsSolid = TRUE;
	int status, inverterOn;
    // MSJ following variable declaration does not appear in this function, clearing compiler warnings
    //static uint8_t ambientTempTrip = FALSE;
	status = TRUE;

	inverterOn = (upsBoss.invMode == AUTO_ON) || (upsBoss.invMode == MANUAL_ON);

	if (bankSetMode == FALSE)
	{ // Normal display operation
		if (bankSetMode != bankSetModeLast)
		{
			bankSetModeLast = bankSetMode;
		}
		if (inverterOn == TRUE)
		{
			if (upsBoss.upsState == UPS_STARTUP)
			{
				displayLedSet(LED_DISP_ON, LED_DISP_FLASH);
			}
			else
			{
				displayLedSet(LED_DISP_ON, LED_DISP_SOLID);
			}
		}
		else
		{
			displayLedSet(LED_DISP_ON, LED_DISP_OFF);
		}

		if (upsBoss.loadPctOut > (UPS_LOAD_ALM_ON * 100.0))
		{
			if (overloadBypassOn)
			{
				displayLedBar(LED_DISP_LOAD_BAR, 0.0f, LED_DISP_BAR_SOLID);
				displayLedSet(LED_DISP_LOAD5, LED_DISP_FLASH);
			}
            else 
			    displayLedBar(LED_DISP_LOAD_BAR, upsBoss.loadPctOut, LED_DISP_BAR_FLASH);

			if (loadBarLEDsSolid)
			{
				loadBarLEDsSolid = FALSE;
				sonalert(ON_BEEP);
#if (defined VERBOSE_DEBUG)
                if ((upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1))
			    {
					if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						log_trace("ups warning ON before overload");
					}
				}
#endif
			}
		}
		else if (upsBoss.loadPctOut <= (UPS_LOAD_ALM_OFF * 100.0))
		{
			if (overloadBypassOn) 
			{
				displayLedBar(LED_DISP_LOAD_BAR, 0.0f, LED_DISP_BAR_SOLID);
				displayLedSet(LED_DISP_LOAD5, LED_DISP_FLASH);
			}
            else
			    displayLedBar(LED_DISP_LOAD_BAR, upsBoss.loadPctOut, LED_DISP_BAR_SOLID);

			if (!loadBarLEDsSolid)
			{
				loadBarLEDsSolid = TRUE;
				sonalert(OFF);
#if (defined VERBOSE_DEBUG)
                if ( (upsBoss.verboseDebug == TRUE) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
			    {
					if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
					{
						log_trace("ups warning OFF before overload");
					}
				}
#endif
			}
		}
		else
		{
			if (overloadBypassOn)
			{
			    displayLedBar(LED_DISP_LOAD_BAR, 0.0f, LED_DISP_BAR_SOLID);
				displayLedSet(LED_DISP_LOAD5, LED_DISP_FLASH);
			}
            else
			    displayLedBar(LED_DISP_LOAD_BAR, upsBoss.loadPctOut, LED_DISP_BAR_SOLID);
		}

		if ((upsBoss.dcMode == AUTO_ON) || (upsBoss.dcMode == MANUAL_ON))
		{
			displayLedBar(LED_DISP_BAT_BAR, upsBoss.batChgPct, LED_DISP_BAR_FLASH);
		}
		else
		{
			displayLedBar(LED_DISP_BAT_BAR, upsBoss.batChgPct, LED_DISP_BAR_SOLID);
		}
		if ((upsBoss.invMode == FAULT) || (upsBoss.dcMode == FAULT) || (upsBoss.chgMode == FAULT) || (upsBoss.fanMode == FAULT) || (upsBoss.batMode == FAULT))
		{
			displayLedSet(LED_DISP_FAULT, LED_DISP_SOLID);
		}
		else
		{
			displayLedSet(LED_DISP_FAULT, LED_DISP_OFF);
		}
		if ((upsBoss.bypassMode == AUTO_ON) || (upsBoss.bypassMode == MANUAL_ON))
		{
			displayLedSet(LED_DISP_BYPASS, LED_DISP_SOLID);
		}
		else
		{
			displayLedSet(LED_DISP_BYPASS, LED_DISP_OFF);
		}
		if (upsBoss.batMode == FAULT)
		{
			displayLedSet(LED_DISP_SERVICE_BAT, LED_DISP_SOLID);
		}
		else
		{
			displayLedSet(LED_DISP_SERVICE_BAT, LED_DISP_OFF);
		}
// While testing intially, flash option LED so we can tell if micro board is old or new DSP
#ifdef FLASH_OPTION_LED_FOR_DSP
		displayLedSet(LED_DISP_OPTION, LED_DISP_FLASH);
#endif
#ifdef EXTERNAL_BATTERY_PACK_JUMPER_SENSE
			if (externalDioInput(1))
			{
				displayLedSet(LED_DISP_OPTION, 1); 	// Off=0, Solid=1, Flash=2, FlashAlt=3
			}
			else
			{
				displayLedSet(LED_DISP_OPTION, 0); 	// Off=0, Solid=1, Flash=2, FlashAlt=3
			}
#endif	// EXTERNAL_BATTERY_PACK_JUMPER_SENSE
	}
	else
	{ // bankSetMode = TRUE
		if (bankSetMode != bankSetModeLast)
		{
			bankSetModeLast = bankSetMode;
			// setup display
			// *TODO: Write code to do bank switch configuration on front panel
		}
	}

	return status;
}

int displayLedSet(int column, int row, int state) // Off=0, Solid=1, Flash=2, FlashAlt=3
{
	int status;

	status = TRUE;

	switch (state)
	{
	case 0:
		upsBoss.ledState1[column][row] = upsBoss.ledState2[column][row] = 0;
		break;
	case 1:
		upsBoss.ledState1[column][row] = upsBoss.ledState2[column][row] = 1;
		break;
	case 2:
		upsBoss.ledState1[column][row] = 1;
		upsBoss.ledState2[column][row] = 0;
		break;
	case 3:
		upsBoss.ledState1[column][row] = 0;
		upsBoss.ledState2[column][row] = 1;
		break;
	default:
		upsBoss.ledState1[column][row] = upsBoss.ledState2[column][row] = 0;
		break;
	}

	return status;
}

int displayLedBar(int bar, float percent, int flash)
{
	int state;
	int status;

	status = TRUE;

	if (flash == LED_DISP_BAR_FLASH)
	{
		state = 2;
	}
	else
	{
		state = 1;
	}

	if (bar == LED_DISP_LOAD_BAR)
	{
		// vertical unit uses (mostly) display with horizontal bars
		if (pEventData->systemData.bankSwitches.bit.VERTICAL_DISPLAY == 1)
		{
#ifdef ZERO_POWER_REPORTING_AMPS
            if (upsBoss.ampOut < ZERO_POWER_REPORTING_AMPS)
				displayLedSet(LED_DISP_VERT_LOAD1, 0);
			else
				displayLedSet(LED_DISP_VERT_LOAD1, state);
#endif // ZERO_POWER_REPORTING_AMPS
#ifdef ZERO_POWER_REPORTING_WATTS
            if (upsBoss.powOut < ZERO_POWER_REPORTING_WATTS)
				displayLedSet(LED_DISP_VERT_LOAD1, 0);
			else
				displayLedSet(LED_DISP_VERT_LOAD1, state);
#endif // ZERO_POWER_REPORTING_WATTS
#ifdef ZERO_POWER_REPORTING_PERCENT_LOAD
            if (upsBoss.loadPctOut < ZERO_POWER_REPORTING_PERCENT_LOAD)
				displayLedSet(LED_DISP_VERT_LOAD1, 0);
			else
				displayLedSet(LED_DISP_VERT_LOAD1, state);
#endif // ZERO_POWER_REPORTING_PERCENT_LOAD
			if (percent >= 25.0)
			{
				displayLedSet(LED_DISP_VERT_LOAD2, state);
			}
			else
			{
				displayLedSet(LED_DISP_VERT_LOAD2, 0);
			}
			if (percent >= 50.0)
			{
				displayLedSet(LED_DISP_VERT_LOAD3, state);
			}
			else
			{
				displayLedSet(LED_DISP_VERT_LOAD3, 0);
			}
			if (percent >= 75.0)
			{
				displayLedSet(LED_DISP_VERT_LOAD4, state);
			}
			else
			{
				displayLedSet(LED_DISP_VERT_LOAD4, 0);
			}
			if (percent >= 100.0)
			{
				displayLedSet(LED_DISP_VERT_LOAD5, state);
			}
			else
			{
				displayLedSet(LED_DISP_VERT_LOAD5, 0);
			}
		}
		else	// if (pEventData->systemData.bankSwitches.bit.VERTICAL_DISPLAY == 1)
		{
#ifdef ZERO_POWER_REPORTING_AMPS
            if (upsBoss.ampOut < ZERO_POWER_REPORTING_AMPS)
				displayLedSet(LED_DISP_VERT_LOAD1, 0);
			else
				displayLedSet(LED_DISP_VERT_LOAD1, state);
#endif // ZERO_POWER_REPORTING_AMPS
#ifdef ZERO_POWER_REPORTING_WATTS
            if (upsBoss.powOut < ZERO_POWER_REPORTING_WATTS)
				displayLedSet(LED_DISP_VERT_LOAD1, 0);
			else
				displayLedSet(LED_DISP_VERT_LOAD1, state);
#endif // ZERO_POWER_REPORTING_WATTS
#ifdef ZERO_POWER_REPORTING_PERCENT_LOAD
            if (upsBoss.loadPctOut < ZERO_POWER_REPORTING_PERCENT_LOAD)
				displayLedSet(LED_DISP_VERT_LOAD1, 0);
			else
				displayLedSet(LED_DISP_VERT_LOAD1, state);
#endif // ZERO_POWER_REPORTING_PERCENT_LOAD
			if (percent >= 25.0)
			{
				displayLedSet(LED_DISP_LOAD2, state);
			}
			else
			{
				displayLedSet(LED_DISP_LOAD2, 0);
			}
			if (percent >= 50.0)
			{
				displayLedSet(LED_DISP_LOAD3, state);
			}
			else
			{
				displayLedSet(LED_DISP_LOAD3, 0);
			}
			if (percent >= 75.0)
			{
				displayLedSet(LED_DISP_LOAD4, state);
			}
			else
			{
				displayLedSet(LED_DISP_LOAD4, 0);
			}
			if (percent >= 100.0)
			{
				displayLedSet(LED_DISP_LOAD5, state);
			}
			else
			{
				displayLedSet(LED_DISP_LOAD5, 0);
			}
		}
	}
	else // Battery Charge Bar
	{
		displayLedSet(LED_DISP_BAT1, 0); // turn off all LEDs in bar
		displayLedSet(LED_DISP_BAT2, 0);
		displayLedSet(LED_DISP_BAT3, 0);
		displayLedSet(LED_DISP_BAT4, 0);
		displayLedSet(LED_DISP_BAT5, 0);
		// use int in case charge is determined by joules, otherwise there may be gaps
		if (iRange((int)percent, 0, 20))
		{
			displayLedSet(LED_DISP_BAT1, state);
		}
		if (iRange((int)percent, 21, 40))
		{
			displayLedSet(LED_DISP_BAT2, state);
		}
		if (iRange((int)percent, 41, 60))
		{
			displayLedSet(LED_DISP_BAT3, state);
		}
		if (iRange((int)percent, 61, 99))
		{
			displayLedSet(LED_DISP_BAT4, state);
		}
		if ((int)percent == 100) // only show 100% on slow charge
		{
			displayLedSet(LED_DISP_BAT5, state);
		}
	}

	return status;
}
