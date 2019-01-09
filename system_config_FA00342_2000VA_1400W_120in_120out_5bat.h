// system_config_FA00342_2000VA_1400W_120in_120out_5bat
//
// Version 1.0.11 01-08-2019 Keith Abell
//
// Version History
//
// 1.0.0 11-30-2018 1.0.0 Initial Version
//
// 1.0.1 12-18-2018 Keith Abell
//                  Updates for DUAL_INPUT_VOLTAGE, WRPFC, DUAL_INPUT_FREQ -- can effect basic SNMP 
//                  information and Brightcom datum
//
// 1.0.3 12-19-2018 Bill B./Mark James
//                  Fixes for current/power reporting (now using 1.0A limit to rise soon to 1.5A)			
//
// 1.0.4 12-21-2018 Keith Abell
//                  Modifications to the configuration file to handle either DUAL or WRPFC input
//                      with implications related to multiple nominal input voltage and the
//                      "complication" of 50/60 Hz input as a frequency changer.  Common core code will
//                       need to be updated during development for the FA00405 system.
//
// 1.0.5 12-21-2018 Bill B, Mark James
//                  Update for current/power reporting -- raised limit to 1.55A instead of 1.0A
//
// 1.0.6 12-26-2018 Keith ABell
//                  Changed frequency range bit for 1.5 or 3.0 Hz +/- to select 3.0 range (bit == 0)
//                  Corrected current gain value by multiplying by 1.5176, the average error for the
//                      highest loads reported by Eduardo on 12/26/18.  New value 1.3981527
//                  Made sure Bypass on overload switches were activated
//                  Verified overload condition is set to >= 25%
//                  Changed overload timeout to 5 secs from 10 secs -- provides short ride through, 
//                      Dana's Forth code goes "immediately" but takes a couple of seconds to go to bypass
//                  Commented out COLDSTART switch (N/A for 342)
//
// 1.0.7 12-26-2018 Keith Abell
//                  Went the wrong direction on correcting current gain -- repoted values were too high,
//                  not too low.  Proper correction is OLD_GAIN * 0.6589344 (~2/3 of the old values,
//                  not ~50% higher.
//
// 1.0.8 12-28-2018 Keith Abell
//                  Added droop compensation based on data collected by Eduardo due to input transformer.
//
// 1.0.9 01-04-2019 Keith Abell
//                  Moved WRPFC #define where it will be tested to define other items
//                  Corrected ordering some #defines to group like ones together
//                  Aligned comments for easier reading
// 
// 1.0.10 01-07-2019 Keith Abell
//                  Made modifications to Low Battery Alarm and Shutdown voltages (per cell) based
//                     on Eduardo's testing report earlier today, 1/7/19
//
// 1.0.11 01-08-2019 Keith Abell
//                  Corrected simple editing mistake using the 'L' qualifier instead of 'f' for single precision 
//					   floating point on a small group of #defines for limits and alarm levels.
//
//**************************************************************************************************************


#ifndef _system_config_system_h // inclusion guard
#define _system_config_system_h

#include "types.h"
#include "log.h"

// Verbose Debug Log Output Definitions
#define VERBOSE_DEBUG // Define to enable verbose debug output
#ifdef VERBOSE_DEBUG
  #include "uart.h" 
  //#define USE_VERBOSE_DEBUG_AUX_PORT // Comment out to use primary debug port
  #ifndef USE_VERBOSE_DEBUG_AUX_PORT
    #define VERBOSE_DEBUG_PORT UART_PORT0      // Primary   Debug Port
  #else
    #define VERBOSE_DEBUG_PORT UART_PORT1      // Auxiliary Debug Port
  #endif
  #define DEFAULT_VERBOSE_DEBUG FALSE
  //#define DEFAULT_VERBOSE_DEBUG TRUE
  #define LOG_LEVEL LOG_TRACE     // Set the logging level to either LOG_TRACE, LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, or LOG_FATAL.
  //#define VERBOSE_DEBUG_START   // Define to debug controllers; In Test Mode, Enable verbose debug by pressing '[', press 'K', 'k' to start.
  //#define VERBOSE_DEBUG_EVENTS  // Define to display verbose debug in the event controller
  //#define VERBOSE_DEBUG_SYNC
  //#define VERBOSE_DEBUG_ADC
#endif

#define PART_VERSION ("Part Version 1.0.11")
#define PART_VERSION_DATE ("01-08-2019 ")

#define UPS_MANUFACTURER ("INTELLIPOWER")
#define UPS_PART ("FA00342")
#define UPS_PART_DESCRIPTION ("120VAC in/out, 2000VA, 1400W, 5 Batteries")
#define AUTO_RESTART (TRUE) // TRUE, FALSE

// Added support for either DUAL_INPUT or WRPFC AC sources, both have impacts on nominal input voltages/frequencies
// The original nom vales become the low nominal value, the higher nominal value is defined in accordance with
// the type of input.  For widerange PFC with a range of 85-270 we picked 120 for the low nom, 240 for the high
// with 177 as the midpoint as defined below.

#define INPUT_TRANSFORMER_DROOP 5.11f			// Based on data from Eduardo
#define INPUT_TRANSFORMER_DROOP_OFFSET 2.81f	// Based on data from Eduardo
#define DUAL_INPUT_FREQ 						// 342 can be 50 or 60Hz input
#ifdef DUAL_INPUT_FREQ
	#define UPS_FREQINNOMLO (50.0f)		// Can be used for SNMP nom freq for Basic Info Page if we are @ 50Hz input
#endif // DUAL_INPUT_FREQ
#define UPS_FREQINNOM (60.0f)			    // Normal nominal for 60Hz input systems, can be SNMP nom freq for Basic Info Page

#define DUAL_INPUT_VOLTAGE			        // dual input
#ifdef DUAL_INPUT_VOLTAGE
	#define UPS_VOLTINMONHI	230				// Can be used for SNMP nom VOLTIN for Basic Info Page if we are @ 230V input
#endif

#define WRPFC (0) // 0=No WRPFC, 1=WRPFC	// no WRPFC
#if WRPFC == 1
    #define WRPFC_ADC_PSAMP_HEATSINK
    #define UPS_VOLTINNOMHI (240)			// Hi range nominal
    #deiine UPS_VOLTINLO (85)
    #define UPS_VOLTINHI (270)
    #define UPS_VOLTINMIDPOINT (177)    	// VOLTINLO_RANGE ( >= UPSVOLTINLO < UPSVOLTINMIDPOINT)
									   		// VOLTINHI_RANGE ( >= UPSVOLTINMIDPOINT <= UPSVOLTINHI)
#endif
#define UPS_TIMELOBATNOM (30.0f)
#define UPS_VOLTINNOM (120.0f)		   //  See UPS_VOLTINNOMHI above if either DUAL INPUT or WIDERANGE PFC
#define UPS_FREQOUTNOM (60.0f)		  
#define UPS_VAOUTNOM (2000) // 625, 800, 1100, 1500, 2000, 3000
#define UPS_POWOUTNOM (1400)
#define UPS_VOLTOUTNOM (120) // Use integer for #if later in file
#define UPS_AMPOUTNOM (UPS_VAOUTNOM / UPS_VOLTOUTNOM)

#define INVERTER_SYNC (TRUE)    // TRUE, FALSE used to set data switch below
#define INVERTER_OUTPUT_CURRENT_GAIN  0.5845755f // Inverter output current gain (OLD gain == 0.8871527f)
//#define INVERTER_OUTPUT_CURRENT_GAIN 0.3913894325   // Highest current, highest attenuation
//#define INVERTER_OUTPUT_CURRENT_GAIN 0.7142857143   // Middle current
//#define INVERTER_OUTPUT_CURRENT_GAIN 1.127201566    // Lowest current
#define PHASE_TOLERANCE (0.065f) // What is our lock range on Phasing in float milliseconds (within  0.03072 degrees)
#define FREQ_TOLERANCE (0.095f) // What is our lock range on Frequency in Hz
// #define BATTERY_COLD_START   // Cannot start on batteries if AC not available
#define DC_BUS_VOLTAGE (400) // 380, 400, 430
/*
  KRA NOTE:  This is a dual input transformer based system so there are two settings required for 120 vs 230 volt usage
  Comment out both #defines to disable droop compensation, at no output load the transformer secondary voltage
  will be higher than input primary voltage so first subtract offset then add (droop * (load%/100.0)).
  This is done in adc.c adcProcess() when the accumulated ADC channel counts are first converted to engineering units.
  In any case, support for the 230V droop numbers are not yet supported in the core code
*/
#define INPUT_TRANSFORMER_DROOP 5.11f             // 120V numbers
#define INPUT_TRANSFORMER_DROOP_OFFSET 2.81f
#define INPUT_TRANSFORMER_DROOP_230 8.57f         // 230V numbers
#define INPUT_TRANSFORMER_DROOP_OFFSET_230 6.27f

//#define MOMENTARY_BYPASS_ENABLED 

#define NUM_BAT (5)
#define MAX_CHARGE_CYCLES (14.0f)
#define NUM_CELLS (NUM_BAT * 6.0f)
#define LOW_BATTERY_ALARM (1.82733f * NUM_CELLS)			// Test data from Eduardo, 1/7/2019
#define LOW_BATTERY_ALARM_LOW_POWER (1.783f * NUM_CELLS)    // Test data from Eduardo, 1/7/2019
#define LOW_BATTERY_SD (1.677667f * NUM_CELLS)              // Test data from Eduardo, 1/7/2019
#define LOW_BATTERY_SD_LOW_POWER (1.751f * NUM_CELLS)       // Test data from Eduardo, 1/7/2019
#define BATTERY_OHMS_PER_CELL (0.013)
#define BATTERY_RESISTANCE (BATTERY_OHMS_PER_CELL * NUM_CELLS)
#define BATTERY_V_MIN_0_PCT (1.67f * NUM_CELLS)
// KRA EDIT This threshold does not trigger the error the alarms with batteries pulled out in the battery function of the ups.c module
//   (at least with my orig test system).  These items are related to the fix for issue #126 reported by Hector
// KRA EDIT What is the correct level?  Using per cell voltage of 2.4583 for trip of 118V (use 2.4792 for 119V, 2.50 for 120V)  Note:  orig was 2.52
#define BATTERY_V_MAX_0_PCT (2.50f * NUM_CELLS)	 // This define sets the threshold for a charger over-volt error, no batteries connected
#define BATTERY_V_MIN_100_PCT (2.02f * NUM_CELLS)
#define BATTERY_V_MAX_100_PCT (2.15f * NUM_CELLS)
#define ChgrFaultMsecs 5*(1000)                           // Apply charger over-volt error condition for at least X seconds after light show
                                                          //   fault indication with Battery drawer not connected will be asserter for 10 secs or
                                                          //   whenever the Battery voltage drops below BATTERY_V_MAX_0_PCT
#define BATTERY_MAX_JOULES (600000L)                      // Test value to get Joule count correct
#define BAT_CAP_METHOD (2)                                // Table lookup = 0, Crude linear = 1, Joules = 2
// Computed correction factor (4) for difference of (Bus V)/2 and Nom Bat voltage (48V for 4 ea 12V batteries)
//   for the next two thresholds times a correction factor for charger current circuit efficiency -- 48%
#define BAT_CHG_FAST_THRESHOLD (1.92f*1.0f)               // if charge current higher, go to Fast Charge, adjusted by correction factor for charger circuit
#define BAT_CHG_SLOW_THRESHOLD (1.92f*0.8f)               // if charge current lower, go to Slow Charge, adjusted by correction factor for charger circuit

#define BAT_FAST_CHG_TIMEOUT ( 48L * 60L * 60L * 1000L)   // leftmost number hours, converts to milliseconds
//#define BATTERY_CHARGE_CYCLING
//#define BATTERY_CHARGE_CYCLING2
#define BATTERY_CYCLING_CHARGE_ON_MINUTES  (30L)          // for charger cycling, On  time in minutes
#define BATTERY_CYCLING_CHARGE_OFF_MINUTES (30L)          // for charger cycling, Off time in minutes
// Number of charge cycles before turning off and going to charge fail and turning on Service Battery LED
#define BATTERY_CYCLING_CHARGE_MAX_CYCLES  (14L)          // for charger cycling
//#define CYCLON_BATTERIES
//#define BATTERY_MAX_TIME				(300)             // maximum battery time in seconds

// IMPORTANT -- only one of BYPASS ACTIVE and SHUTDOWN_ON_OVERLOAD should be TRUE
#define BYPASS_ACTIVE TRUE       	// Enable bypass operation
#define BYPASS_ON_OVERLOAD TRUE 	// Dynamic bypass
#define SHUTDOWN_ON_OVERLOAD FALSE	// Bypass, not shutdown

//#define DYNAMIC_BYPASS
//#define BYPASS_RECOVER TRUE                   // AUTO, ON, MANUAL **note: what is this??
#define BYPASS_OVERLOAD_TIME ( 5L * 1000L )     // Time for dynamic bypass to turn off in msec
#define CHECK_INPUT_VRANGE                      // check input voltage before turning on Inverter
#define UPS_VOLTIN_LOW (0.76f)                  // -20% ...check utility in range before...allow margin
#define UPS_VOLTIN_HIGH (1.15f)                 // +15% ...turning on inverter or going to UPS_ON_UTILITY
#define UPS_LOAD_ALM_ON (1.05f)                 // Ratio to 100%
#define UPS_LOAD_ALM_OFF (1.02f)
#define UPS_LOAD_TRIP_ON (1.25f)                // long term overload shutdown -- not related to fault bit from inverter
#define UPS_LOAD_TRIP_OFF (1.20f)
#define OVERLOAD0_MSEC ( 5L * 1000L )           // Allow some ride-through for load steps requiring extra current

//#define HEAT_SYNC_1_ENABLE // Enable heat sync 1 functionality
#define HEAT_SYNC_2_ENABLE   // Enable heat sync 2 functionality

#define TEMP_AMB_ALM_ON 60.0f
#define TEMP_AMB_ALM_OFF 55.0f
#define TEMP_AMB_TRIP_ON 65.0f
#define TEMP_AMB_TRIP_OFF 55.0f

#define TEMP_HS_ALM_ON 85.0f
#define TEMP_HS_ALM_OFF 75.0f
#define TEMP_HS_TRIP_ON 90.0f
#define TEMP_HS_TRIP_OFF 85.0f

#define INVERTER_OC_MSEC 500    // Number of milliseconds before Inverter shuts down on overcurrent/fault signal from Inverter
#define OVERLOAD1_PERCENT 150.0f // the higher the load, the lower the time for this to work right
#define OVERLOAD1_MSEC 5000L
#define OVERLOAD2_PERCENT 200.0f
#define OVERLOAD2_MSEC 2000L

#define BANK_CONFIG_INPUT_BATTERY_SETPOINT 0 // 0=95VAC, 1=100, 2=105, 3=107

// Input Relays

#define INPUT_RELAYS_MIN_OFF (95.0)
#define INPUT_RELAYS_MIN_ON (105.0)
#define INPUT_RELAYS_MAX_OFF (135.0)
#define INPUT_RELAYS_MAX_ON (130.0)

// UPS Communication

#define COM_PARSE 1 // 1=SNMP, 2=UPSILON or 3=USER
#define SERIAL_A_BAUD_RATE (9600)
#define SERIAL_B_BAUD_RATE (9600)
// #define SERIAL_B_PORT_TYPE_RS485

// Remote input
#define REMOTE_MODE (0) // 0=none, 1=startup/shutdown, 2=battleshort, 3=transformer tap sense

/* 	Implemented in ups.c externalDioInput(channel), senses battery pack jumper connected (pack plugged in).
	Sensed at J4 pin 20 with pull-up resistor, 5VDC pack not plugged in, 0VDC plugged in.
*/
//#define EXTERNAL_BATTERY_PACK_JUMPER_SENSE
// Number of batteries in external battery pack
//#define EXTERNAL_BATTERY_PACK_JUMPER_SENSE_NUMBER_OF_BATTERIES 16
/*
    This ratio is used to compensate between an internal string of batteries
    and optional external battery enclosure of 1 or more strings.
    Since the battery losses will be different when there are more battery
    strings are connected I've added an adjustment term.

    number of batteries internal/((internal + external) * adjustment)
*/
// 1/(3*1.666667) = 1/5 per testing this will make the Joules 5 times single string
//#define EXTERNAL_BATTERY_PACT_JOULE_RATIO ((float)NUM_BAT / (((float)NUM_BAT + (float)EXTERNAL_BATTERY_PACK_JUMPER_SENSE_NUMBER_OF_BATTERIES) * 1.666667f))

// Program configuration

#define OLD_MICRO_COMPATIBLE (1) // 1=yes, old, non-direct PWM
// DANGER!!  Will blow up inverter if Model F modified for direct connect and this turned off
// No underlap
#define INVERTER_DIRECT_CONNECT (0) // use EPWM2 to run upper and lower IGBTs directly
#define INVERTER_PULSE_BY_PULSE (0) // Use if updating sine on pulse by pulse, else update on timer
#define UPS_CHECKSUM (TRUE)
#define INVERTER_UPDATES_PER_CYCLE (128)
#define CAPTURE_CYCLES (5)
#define PWM_FREQUENCY 27e3
#define PWM_DEADTIME (1.0)  // in microseconds
/*
    When there is no external load set threshold where Britecom and SNMP reports zero load use
    either Watts, Amps or Load, one but not more than one.
*/
//#define ZERO_POWER_REPORTING_WATTS (100.0f)
#define ZERO_POWER_REPORTING_AMPS (1.55f)
// Set a threshold of percent Watts/Total Watts, below Load% will be reporting Watts, above reporting highest Watts or VA
//#define ZERO_POWER_REPORTING_PERCENT_LOAD (30.0f)

#define BAT_TABLE_SIZE (20) // number of entries in each tab
//	Volts per Battery=Capacity		100%	95%		90%		85%		80%		75%		70%		65%		60%		55%
#define BAT_TABLE1_LOWER_POWER 12.42, 12.37, 12.32, 12.30, 12.27, 12.21, 12.15, 12.10, 12.04, 11.95,
//									50%		45%		40%		35%		30%		25%		20%		15%		10%		5%
#define BAT_TABLE2_LOWER_POWER 11.88, 11.81, 11.71, 11.64, 11.52, 11.42, 11.30, 11.15, 10.97, 10.70

//									100%	95%		90%		85%		80%		75%		70%		65%		60%		55%
#define BAT_TABLE1_HIGHER_POWER 12.20, 12.15, 12.10, 12.05, 12.00, 11.95, 11.90, 11.85, 11.80, 11.75,
//									50%		45%		40%		35%		30%		25%		20%		15%		10%		5%
#define BAT_TABLE2_HIGHER_POWER 11.70, 11.65, 11.60, 11.55, 11.50, 11.43, 11.33, 11.13, 10.93, 10.73

// Bank Switch Settings (Default) loaded when unit first started or when commanded
#define BANK1_1_VERTICAL_DISPLAY (1)  // 1=vertical, 0=horizontal (not implemented)
#define BANK1_2_AUTOSTART (1)         // 1=Energize output on startup, 0=not
#define BANK1_3_LOW_BATTERY_ALARM (0) // 1=Timed, 0=Calculated -- check bat voltage/cell against LOW_BATTERY_ALARM_LOW_POWER ( powOut <= 150W )
                                      //     or LOW_BATTERY_ALARM ( powOut > 150W )
// use with bank 1, bit 3
// Bank 1 bit 4&5	Timed	Calculated
// 		0			120			120
//		1			360			900
//		2			240			300
//		3			600			1800
#define BANK1_4_5_LOW_BAT_ALM_SET (0)  // 120 sec (two minutes before batteries expire)
#define BANK1_6_WIRE_FAULT_SU_INH (1)  // 1=Inhibit start up if there is a wire fault
#define BANK1_7_WIRE_FAULT_ALM_INH (1) // 1=Inhibit Wire Fault Alarm
#define BANK1_8_RESERVED (0)           //
#define BANK1_9_10_RS232_BAUD_RATE (3) // 0=1200, 1=4800, 2=2400, 3=9600

// 0=Turn Off Enabled, 1=Disabled, 2=Turn On/Off Enabled, 3=Turn On Enabled
#define BANK2_1_2_REMOTE_ON_OFF (0)
// 0=Turn Off Enabled, 1=Disabled, 2=Turn On/Off Enabled, 3=Turn On Enabled
#define BANK2_3_4_REMOTE_DELAY (0)        // Turn off delay (sec), 0=0, 1=300, 2=120, 3=900
#define BANK2_5_BATTERY_TEST_INTERVAL (0) // 1=1 Month, 0=3 Months
#if (UPS_VOLTOUTNOM == 100)
#define BANK2_6_7_INVERTER_VOLT_SET (0) // 0=100, 1=110, 2=115, 3=120
#elif (UPS_VOLTOUTNOM == 110)
#define BANK2_6_7_INVERTER_VOLT_SET (1) // 0=100, 1=110, 2=115, 3=120
#elif (UPS_VOLTOUTNOM == 115)
#define BANK2_6_7_INVERTER_VOLT_SET (2) // 0=100, 1=110, 2=115, 3=120
#elif (UPS_VOLTOUTNOM == 120)
#define BANK2_6_7_INVERTER_VOLT_SET (3) // 0=100, 1=110, 2=115, 3=120
#endif
#define BANK2_8_REMOTE_OFF_BAT (0)
#define BANK2_9_RESERVED (0)         //
#define BANK2_10_SYNC_FREQ_RANGE (0) // 0=+/- 3Hz, 1=+/- 1.5Hz

#define BANK3_1_BYPASS_STARTUP (1) // 0=Bypass on, 1=Bypass off
#define BANK3_2_BYPASS_DYNAMIC (1) // Dynamic Bypass 0=On, 1=Off
// Utility Voltage on/off battery, 0=85/95, 1=85/90, 2=80/85, 3=100/107
#define BANK3_3_4_BAT_TRANSFER_SET (1)
// Not needed in 780 board, only used on model C which does not have regulated DC Bus voltage
#define BANK3_5_INV_REG_RATE          (0)     // Inverter Regulation Rate 0=Fast, 1=Slow
#define BANK_REMOTE_MANUAL_START (0)  // 1 = Startup in Manual, 0 = Startup in Auto. New Bank Switch for DSP
#define BANK_REMOTE_INPUT_IO (0)	  // Which input signal used for Remote command, (0=Din0 (DIO0), 1=Din1 (DIO1), 2=Din2 (DIO2), 0=Din3 (DIO3))
// Use to indicate inverter will synchronize with line frequency and phase
#if (INVERTER_SYNC == TRUE)
#define BANK3_5_SYNC_INV_TO_LINE (1) // Sync
#else
#define BANK3_5_SYNC_INV_TO_LINE (0) // Don't sync
#endif
#define BANK3_6_7_8_BAT_SMART_SET (0) // Smart Battery Setpoints ????
#define BANK3_9_RESERVED (0)          //
#define BANK3_10_RESET_BANKS (0)      // Reset all bank switches to defaults

// load in hardware dependencies
#include "system_config_ModelF_Hardware.h"

#endif // _system_config_h
