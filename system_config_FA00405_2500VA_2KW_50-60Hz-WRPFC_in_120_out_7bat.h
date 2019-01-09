// system_config_FA00405_2500VA_20000W_50-60Hz-WideR_in_120_out_7bat.h
// Version 1.0.0  12-14-2018  Keith Abell
//
// Version History
//
// 1.0.0  12-14-2018  Keith Abell
//        Version based on edited version of FA00350 header file
//		  Modified #defines for DUAL_INPUT_FREQ, DUAL_INPUT_VOLTAGE, and WRPFC
//		  -- set it up for dual input freq (50/60), WRPFC instead of dual voltage
//        Brought over Bill B, Mark's new current threshold for low-power reporting
//        -- code to hook these need to be developed while working the FA00342
//           and the FA00405.

#ifndef _system_config_system_h // inclusion guard
#define _system_config_system_h

#include "types.h"
#include "log.h"

// Verbose Debug Log Output Defined
#define VERBOSE_DEBUG // Define to display verbose debug output
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
  #define LOG_LEVEL LOG_TRACE   // Set the logging level to either LOG_TRACE, LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, or LOG_FATAL.
  //#define VERBOSE_DEBUG_START // Define to debug controllers; In Test Mode, Enable verbose debug by pressing '[', press 'K', 'k' to start.
  //#define VERBOSE_DEBUG_EVENTS  // Define to display verbose debug in the event controller
  //#define VERBOSE_DEBUG_SYNC
  //#define VERBOSE_DEBUG_ADC
#endif

#define PART_VERSION ("Part Version 1.0.0")
#define PART_VERSION_DATE ("12-14-2018")

#define UPS_MANUFACTURER ("INTELLIPOWER")
#define UPS_PART ("FA00405")
#define UPS_PART_DESCRIPTION ("50-60Hz-WideR_in_120VAC_out, 2500VA, 2000W, 7 Batteries")
#define AUTO_RESTART (TRUE) // TRUE, FALSE

#define DUAL_INPUT_FREQ 					// 405 can be either 50 or 60 HZ input frequency
#ifdef DUAL_INPUT_FREQ
	#define UPS_FREQINNOMLO (50.0)			// Can be used for SNMP nom freq for Basic Info Page if we are @ 50Hz input
#endif // DUAL_INPUT_FREQ	
#define UPS_FREQINNOM (60.0)				// Normal nominal for 60Hz input systems, can be SNMP nom freq for Basic Info Page

//#define DUAL_INPUT_VOLTAGE				// defined = false
#ifdef DUAL_INPUT_VOLTAGE
	#define UPS_VOLTINMONHI	230				// Can be used for SNMP nom VOLTIN for Basic Info Page if we are @ 230V input
#endif

#define WRPFC (1) // 0=No WRPFC, 1=WRPFC	// has WRPFC, not dual input
#if WRPFC == 1
	#define WRPFC_ADC_PSAMP_HEATSINK
	#define UPSVOLTINNOMHI (240)			// Hi range nominal
	#define UPSVOLTINLO (85)
	#define UPSVOLTINHI (270)
	#define UPSVOLTINMIDPOINT (177)    		// VOLTINLO_RANGE ( >= UPSVOLTINLO < UPSVOLTINMIDPOINT) 
									   		// VOLTINHI_RANGE ( >= UPSVOLTINMIDPOINT <= UPSVOLTINHI)
#endif
#define UPS_VOLTINNOM (120.0)				// Low range nominal
#define UPS_TIMELOBATNOM (30.0)
#define UPS_FREQOUTNOM (60.0)
#define UPS_VAOUTNOM (2500) // 625, 800, 1100, 1500, 2000, 2500, 3000
#define UPS_POWOUTNOM (2000)
#define UPS_VOLTOUTNOM (120) // Use integer for #if later in file
#define UPS_AMPOUTNOM (UPS_VAOUTNOM / UPS_VOLTOUTNOM)

#define INVERTER_SYNC (TRUE)    // TRUE, FALSE used to set data switch below
//#define INVERTER_OUTPUT_CURRENT_GAIN 0.3913894325   // Highest current, highest attenuation
//#define INVERTER_OUTPUT_CURRENT_GAIN 0.7142857143   // Middle current
//#define INVERTER_OUTPUT_CURRENT_GAIN 1.127201566    // Lowest current
#define INVERTER_OUTPUT_CURRENT_GAIN 0.5673973        // Custom current
#define PHASE_TOLERANCE (0.065) // What is our lock range on Phasing in float milliseconds (within  0.03072 degrees)
#define FREQ_TOLERANCE (0.095f) // What is our lock range on Frequency in Hz
//#define BATTERY_COLD_START   // Can start on batteries if AC not available -- not a feature of FA00405
#define DC_BUS_VOLTAGE (400) // 380, 400, 430
/*
  Comment out both #defines to disable droop compensation, at no output load the transformer secondary voltage
  will be higher than input primary voltage so first subtract offset then add (droop * (load%/100.0)).
  This is done in adc.c adcProcess() when the accumulated ADC channel counts are first converted to engineering units.
*/
//#define INPUT_TRANSFORMER_DROOP 10.87f
//#define INPUT_TRANSFORMER_DROOP_OFFSET 08.11f

#define NUM_BAT (7)
#define MAX_CHARGE_CYCLES (14.0f)
#define NUM_CELLS_PER_BAT (4.0f)		// LiFePO4 (12.8V 10.5Ah), K2B12V10EB spec doesn't state this number, requesting info Subrata/K2 Bat

// KRA EDIT -- for now, if we assume 4 cells per 12V battery (based on general LiFePO4 information from https://www.powerstream.com/LLLF.htm
// Power stream cites 3.65V per cell as peak and CV charge voltage (100%) this yields 4*3.65V == 14.6V which is
// the K2B spec voltage for charge voltage cutoff of 14.6V.
// K2B sepc discharge cuttoff is 10.0V for the battery which suggests a shutdown cell voltage of 10/4 = 2.5V

#define NUM_CELLS (NUM_BAT * NUM_CELLS_PER_BAT)			  // Total cells in UPS
#define LOW_BATTERY_ALARM (2.65f * NUM_CELLS)			  // Edited - 0.15V above SD level
#define LOW_BATTERY_ALARM_LOW_POWER (2.85f * NUM_CELLS)   // Edited - 0.15V above SD_LOW_POWER level
#define LOW_BATTERY_SD (2.5f * NUM_CELLS) 				  // Edited - shutdown level, 2.5V per cell	
#define LOW_BATTERY_SD_LOW_POWER (2.70f * NUM_CELLS)	  // Edited - 10.8V/4 == 2.70, 10V is max discharge	- use 10.8V as warning level
#define BATTERY_OHMS_PER_CELL (0.00125f)                  // LiFePO4 (12.8V 10.5Ah), per K2B12V10EB spec - Bat impedance == < 5mOhm
#define BATTERY_RESISTANCE (BATTERY_OHMS_PER_CELL * NUM_CELLS_PER_BAT)
#define BATTERY_V_MIN_0_PCT (2.5 * NUM_CELLS)			  // Shutdown level	  
// This threshold is used to trigger the error alarm with the battery drawer pulled out of the chassis
// KRA EDIT What is the correct level for the LiFePO4?  Educated guesses
#define BATTERY_V_MAX_0_PCT (4.25f * NUM_CELLS)	          // Edited - This define sets threshold for charger over-volt error, battery drawed pulled, based on 4.1V per cell
#define BATTERY_V_MIN_100_PCT (3.50f * NUM_CELLS)	      // Power stream 95% level charge voltage 
#define BATTERY_V_MAX_100_PCT (3.65f * NUM_CELLS)  		  // Edited  - based on 14.6 charge cuttoff voltage div by 4
#define ChgrFaultMsecs 5*(1000)                           // Apply charger over-volt error condition for at least X seconds after light show
                                                          //   fault indication with Battery drawer not connected will be asserter for 10 secs or
                                                          //   whenever the Battery voltage drops below BATTERY_V_MAX_0_PCT
#define BATTERY_MAX_JOULES ((long)270000)                 // Set to half of FA00350 value, 4 instead of 8 batteries
#define BAT_CAP_METHOD (2)                                // Table lookup = 0, Crude linear = 1, Joules = 2
#define BAT_CHG_FAST_THRESHOLD 1.0f                       // if charge current higher, go to Fast Charge
#define BAT_CHG_SLOW_THRESHOLD 0.8f                       // if charge current lower, go to Slow Charge
#define BAT_FAST_CHG_TIMEOUT ((long)48 * 60 * 60 * 1000)  // leftmost number hours, converts to milliseconds
//#define BATTERY_CHARGE_CYCLING
#define BATTERY_CHARGE_CYCLING2
#define BATTERY_CYCLING_CHARGE_ON_MINUTES  (long)(30)     // for charger cycling, On  time in minutes
#define BATTERY_CYCLING_CHARGE_OFF_MINUTES (long)(30)     // for charger cycling, Off time in minutes
// Number of charge cycles before turning off and going to charge fail and turning on Service Battery LED
#define BATTERY_CYCLING_CHARGE_MAX_CYCLES  (long)(14)     // for charger cycling
//#define CYCLON_BATTERIES
//#define BATTERY_MAX_TIME				(300)			  // maximum battery time in seconds

// IMPORTANT -- only one of BYPASS ACTIVE and SHUTDOWN_ON_OVERLOAD should be TRUE
#define BYPASS_ACTIVE FALSE       	// Enable bypass operation
#define BYPASS_ON_OVERLOAD FALSE 	// Dynamic bypass
#define SHUTDOWN_ON_OVERLOAD TRUE	// Set to TRUE for FA00405 -- NOT yet hooked to take place of bypass

//#define DYNAMIC_BYPASS
//#define BYPASS_RECOVER					TRUE					// AUTO, ON, MANUAL **note: what is this??
#define BYPASS_OVERLOAD_TIME ((long)5 * 1000) // Time for dynamic bypass to turn off in msec
#define CHECK_INPUT_VRANGE                    // check input voltage before turning on Inverter
#define UPS_VOLTIN_LOW (0.70)                 // WRPFC -- 85-270VAC: we will use two nominals, 120 and 240 for the two ranges
											  //    w/ 120 as nominal, we need to allow down to 85V or 85/120 => 70.8%
#define UPS_VOLTIN_HIGH (1.20)                //    w/ 120 or 240 as nominal and two ranges 85-177 and 178-270
											  //    we will need 177/120 = 148%, 270/240 = 	125%  Need two upper values?
											  //    or simply use UPSVOLTINLO and UPSVOLTINHI define above if WRPFC
#define UPS_LOAD_ALM_ON (1.15)                // Ratio to 100% -- getting close to shutdown at +20%
#define UPS_LOAD_ALM_OFF (1.10)				  // 
#define UPS_LOAD_TRIP_ON (1.20) 			  // long term overload shutdown (no bypass)
#define UPS_LOAD_TRIP_OFF (1.15)
#define OVERLOAD0_MSEC ((long)5 * 1000)		  // No delay specified in Unit Spec, we'll give it 5 sec for some ride-through

//#define HEAT_SYNC_1_ENABLE // Enable heat sync 1 functionality
#define HEAT_SYNC_2_ENABLE   // Enable heat sync 2 functionality

#define TEMP_AMB_ALM_ON 60.0
#define TEMP_AMB_ALM_OFF 55.0
#define TEMP_AMB_TRIP_ON 65.0
#define TEMP_AMB_TRIP_OFF 55.0

//The following settings are from Dana Helmes as it relates to his Forth code in the micro-controler board
#define TEMP_HS_ALM_ON 85.0
#define TEMP_HS_ALM_OFF 84.0
#define TEMP_HS_TRIP_ON 90.0
#define TEMP_HS_TRIP_OFF 85.0

#define INVERTER_OC_MSEC 500    // Number of milliseconds before Inverter shuts down on overcurrent
#define OVERLOAD1_PERCENT 150.0 // the higher the load, the lower the time for this to work right
#define OVERLOAD1_MSEC (long)6000
#define OVERLOAD2_PERCENT 250.0
#define OVERLOAD2_MSEC (long)50

//Set point of going on battery based on input AC voltage -- currently not implemented in the DSP code
//#define BANK_CONFIG_INPUT_BATTERY_SETPOINT 0 // 0=95VAC, 1=100, 2=105, 3=107

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
#define BANK1_3_LOW_BATTERY_ALARM (0) // 1=Timed, 0=Calculated
// use with bank 1, bit 3
// Bank 1 bit 4&5	Timed	Calculated
// 		0			120			120
//		1			360			900
//		2			240			300
//		3			600			1800
#define BANK1_4_5_LOW_BAT_ALM_SET (0)  // Bank 1 LED 4
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
#define BANK2_10_SYNC_FREQ_RANGE (1) // 0=+/- 3Hz, 1=+/- 1.5Hz  ..not specified in Unit Spec

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
