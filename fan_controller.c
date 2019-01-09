/*!	@file		fan_controller.c
 *	@author 	Argil Shaver
 *	@author		Mark James
 *	@date		12 January 2015
 *	@version	1.00
 *	@copyright	IntelliPower, Inc. (c) 2015
 *
 *	@brief		Performs Fan Speed Control with Tach feedback for DSP and FanJr Control Boards.
 *
 * TARGET: 		MSP430G2553 and TMS320x2806x devices
 *
 * HISTORY:
 * -			1.00	Initial Version	12 January 2015
 */

#include "fan_controller.h"

tFan Fans[FAN_COUNT];

void initializeFans(void)
{
	int fan;							  // Variable for fan count and cycling
	int fanType[] = {FAN_TYPE_TABLE};	 // Get what types of fans we are connected to in this application
	for (fan = 0; fan < FAN_COUNT; fan++) // Run through the controlled fans in this application
	{
		Fans[fan].Enable = TRUE;		  // Fan control is active
		Fans[fan].Status = FAN_GOOD;	  // Start off with a good fan condition
		Fans[fan].fanControl = FAN_AUTO;  // Fans are to run in automatic control loop
		Fans[fan].RPM = 0;				  // Have not started it as of yet
		Fans[fan].fanTemperature = 20.0;  // Preset the fan temperature to a known value
		Fans[fan].fanPwmValue = 0;		  // Clear the decks
		Fans[fan].speed_cmd_pct = 0.0;	// Last speed command
		Fans[fan].TachCount = 0;		  // Not running so there are zero tach counts
		Fans[fan].tachToRpmMult = 60 / 2; // 2 pulses per revolution, 60 seconds in a minute
		switch (fanType[fan])			  // These values will change depending on type of fan, so switch through them
		{
		case 1: // Fan Type 1 is a YS Tech YW0902501BS-12(A326), 98.6 CFM, 4700 RPM
			Fans[fan].targetRPM = 4670;
			Fans[fan].RPM_Threshold = 3500;
			Fans[fan].RPM_High = 5000;
			break;
		case 2: // Fan Type 2 is a Delta AUC0512DB-AF00, 20.22 CFM, 7400 RPM
			Fans[fan].targetRPM = 6800;
			Fans[fan].RPM_Threshold = 6000;
			Fans[fan].RPM_High = 7200;
			break;
		case 3: // Fan Type 3 is something from the BAEFanJrPWM code for a fan... need to check
			Fans[fan].targetRPM = 8160;
			Fans[fan].RPM_Threshold = 2800;
			Fans[fan].RPM_High = 9700;
			break;
		default: // Default fan type is a Sanyo Denki 9GA0812P2S001(0011), 86.5 CFM, 9700 RPM
			Fans[fan].targetRPM = 5460;
			Fans[fan].RPM_Threshold = 4500;
			Fans[fan].RPM_High = 6000;
			break;
		}			 // End of fan switch settings
		switch (fan) // Make LED Off for the appropriate fan
		{
		case 0:
			FAN_1_LED_OFF;
			break;
		case 1:
			FAN_2_LED_OFF;
			break;
		case 2:
			FAN_3_LED_OFF;
			break;
		case 3:
			FAN_4_LED_OFF;
			break;
		default:
			break;
		}
	}		// End of for loop running through fan setup
	return; // Bye
} // End of initializeFans

void fanRpmCheck(void)
{
	int fan;
	DISABLE_RPM_INTERRUPTS;				  // Disable the interrupts for the tachs while we grab data
	for (fan = 0; fan < FAN_COUNT; fan++) // Loop through all of the fans quickly
	{
		Fans[fan].RPM = Fans[fan].TachCount * Fans[fan].tachToRpmMult; // Update the RPM
		Fans[fan].TachCount = 0;									   // Reset the tach count for the next run interval
	}																   // End of looping through the fans
	ENABLE_RPM_INTERRUPTS;											   // Enable the interrupts for the tachs
	return;															   // Bye
} // End of fanRpmCheck

void fanStateController(void)
{
	int fan;							  // Variable for fan count and cycling
	float rpmLoopClosure;				  // Variable for closure of the RPM loop
	fanRpmCheck();						  // Get the updated RPM of the fans quickly for further processing below
	for (fan = 0; fan < FAN_COUNT; fan++) // Run through the controlled fans in this application
	{
		switch (Fans[fan].fanControl) // Are we automatic, manual, or other?
		{
		case FAN_AUTO:
			if (Fans[fan].RPM < Fans[fan].RPM_Threshold) // Check the read RPM against our operating limits
			{
				switch (fan) // Make LED Red for the appropriate fan
				{
				case 0:
					FAN_1_LED_RED;
					break;
				case 1:
					FAN_2_LED_RED;
					break;
				case 2:
					FAN_3_LED_RED;
					break;
				case 3:
					FAN_4_LED_RED;
					break;
				default:
					break;
				}
				Fans[fan].Status = FAN_FAIL; // Huston we have a problem
			}
			else if (Fans[fan].RPM > Fans[fan].RPM_High)
			{
				switch (fan) // Make LED Yellow for the appropriate fan
				{
				case 0:
					FAN_1_LED_YEL;
					break;
				case 1:
					FAN_2_LED_YEL;
					break;
				case 2:
					FAN_3_LED_YEL;
					break;
				case 3:
					FAN_4_LED_YEL;
					break;
				default:
					break;
				}
				Fans[fan].Status = FAN_OVER; // Huston we have a problem
			}
			else
			{
				switch (fan) // Make LED Green for the appropriate fan
				{
				case 0:
					FAN_1_LED_GRN;
					break;
				case 1:
					FAN_2_LED_GRN;
					break;
				case 2:
					FAN_3_LED_GRN;
					break;
				case 3:
					FAN_4_LED_GRN;
					break;
				default:
					break;
				}
				Fans[fan].Status = FAN_GOOD;			   // We are clear to go
			}											   // End of switch on RPM indicating limits
			if (Fans[fan].fanTemperature < TEMPERATURE_50) // If temperature is less than the Lower Trigger Point then keep speed at 50%
			{
				Fans[fan].speed_cmd_pct = 0.5;
			}
			else if (Fans[fan].fanTemperature > TEMPERATURE_100) // Else if temperature is greater than the Upper Trigger Point then keep speed at 100%
			{
				Fans[fan].speed_cmd_pct = 1.0;
			}
			else // Otherwise we are in the control region so calculate the speed command
			{
				// Speed command percent is the temperature of the fan times the delta output divided by the delta input
				Fans[fan].speed_cmd_pct = (Fans[fan].fanTemperature * DELTA_CONTROL);
			}							  // End of temperature control checks
			if (Fans[fan].targetRPM != 0) // Prevent a divide by zero
			{
				rpmLoopClosure = (float)(Fans[fan].targetRPM - Fans[fan].RPM); // Get the difference
				rpmLoopClosure /= (float)(Fans[fan].targetRPM);				   // Normalize the value to an overall percentage
				if (rpmLoopClosure > 0.1)
				{
					rpmLoopClosure = 0.1;
				} // Limit the step size in the positive direction
				if (rpmLoopClosure < -0.1)
				{
					rpmLoopClosure = -0.1;
				}										   // Limit the step size in the negative direction
				Fans[fan].speed_cmd_pct += rpmLoopClosure; // Put in the offset to close the loop
			}											   // End if divide by zero check
			if (Fans[fan].speed_cmd_pct > 1.0)
			{
				Fans[fan].speed_cmd_pct = 1.0;
			} // Limit to 100% or less
			if (Fans[fan].speed_cmd_pct < 0.0)
			{
				Fans[fan].speed_cmd_pct = 0.0;
			}																		   // Limit to 0% or greater
			Fans[fan].fanPwmValue = (int)((FAN_PWM_PERIOD * Fans[fan].speed_cmd_pct)); // Set with the PWM value for the required speed
			break;
		case FAN_MANUAL:
			break;
		default:
			break;
		}
	}		// End of loop running through fan checks
	return; // Bye
} // End of fanStateController

int fanFailCheck(void)
{
	int returnStatus = FALSE;			  // Status variable preset to False
	int fan;							  // Integer value to scan through the fans
	for (fan = 0; fan < FAN_COUNT; fan++) // Check all of the fans in this application
	{
		returnStatus |= (Fans[fan].Status == FAN_FAIL); // Test to see if any of the fans have failed
		returnStatus |= (Fans[fan].Status == FAN_OVER); // Test to see if any of the fans are over shooting RPM
	}													// End of fan loop through
	return returnStatus;								// Return the fan fail status
} // End of fanFailCheck

//	 Fan Tach Count ISR for both boards
#if defined FANS_DSP_BOARD
#endif
#if defined FANS_FANJR_BOARD
// Fan Tach PORT2 Interrupt Service Routine
#pragma vector = PORT2_VECTOR
__interrupt void isrPORT2(void)
{
	if (P2IFG & TACH_FAN_1) // Tachometer pulse detected ?
	{
		Fans[0].TachCount++;  // Increment tach counter
		P2IFG &= ~TACH_FAN_1; // Clear interrupt flag
	}
	if (P2IFG & TACH_FAN_2) // Tachometer pulse detected ?
	{
		Fans[1].TachCount++;  // Increment tach counter
		P2IFG &= ~TACH_FAN_2; // Clear interrupt flag
	}
	if (P2IFG & TACH_FAN_3) // Tachometer pulse detected ?
	{
		Fans[2].TachCount++;  // Increment tach counter
		P2IFG &= ~TACH_FAN_3; // Clear interrupt flag
	}
	if (P2IFG & TACH_FAN_4) // Tachometer pulse detected ?
	{
		Fans[3].TachCount++;  // Increment tach counter
		P2IFG &= ~TACH_FAN_4; // Clear interrupt flag
	}
}
#endif
