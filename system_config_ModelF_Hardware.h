// system_config_ModelF_Hardware.h
#ifndef _system_config_hardware_h // inclusion guard
#define _system_config_hardware_h

#include "types.h"
#include "system_config.h" // configuration file selected
#include SYSTEM_CONFIG	 // include configuration file

// debug
//#define EVENT_NEW       						// use new event function
// debug end

#ifndef EARLY_BOARD_HARDWARE_SCALING
// Power scaling
//#define VA_SET (int UPS_VAOUTNOM)
#if UPS_VAOUTNOM == 625
#define R163 (15)  // DC/DC Current CT burden resistor
#else
#define R163 (3.3)  // DC/DC Current CT burden resistor
#endif
// MSJ Not use in ADC calculations
/* 
#define R63 (5.11e3) // inverter current opamp input resistor
#define R67 (33.2e3) // inverter current opamp feedback resistor
#define R77 (0.01) // inverter current sense resistor
 */

#define R8 (1.0e3)  // PFC current opamp input resistor
#define R9 (14.0e3) // PFC current opamp feedback resistor
#define R7 (0.01)  // PFC current sense resistor

#if NUM_BAT == 2					 // Battery voltage sense 24V, TODO check this!
#define R109_R111_R113 (130.0e3 * 3) // Battery voltage resistor divider top
#define R120 (133.0e3)				 // Battery voltage resistor divider bottom
#define R125 (787.0)				 // Battery voltage opamp divider top
#define R129 (1.5e3)				 // Battery voltage opamp divider bottom
#define R132 (100.0)				 // Battery current burden resistor
#define R131 (1.5e3)				 // Battery current resistor divider top
#define R134 (1.0e3)				 // Battery current resistor divider bottom
#endif
#if NUM_BAT == 3					 // Battery voltage sense 36V, TODO check this!
#define R109_R111_R113 (215.0e3 * 3) // Battery voltage resistor divider top
#define R120 (133.0e3)				 // Battery voltage resistor divider bottom
#define R125 (787.0)				 // Battery voltage opamp divider top
#define R129 (1.5e3)				 // Battery voltage opamp divider bottom
#define R132 (100.0)				 // Battery current burden resistor
#define R131 (1.5e3)				 // Battery current resistor divider top
#define R134 (1.0e3)				 // Battery current resistor divider bottom
#endif
#if NUM_BAT == 4					 // Battery voltage sense 48V, TODO check this!
#define R109_R111_R113 (301.0e3 * 3) // Battery voltage resistor divider top
#define R120 (133.0e3)				 // Battery voltage resistor divider bottom
#define R125 (787.0)				 // Battery voltage opamp divider top
#define R129 (1.5e3)				 // Battery voltage opamp divider bottom
#define R132 (100.0)				 // Battery current burden resistor
#define R131 (1.5e3)				 // Battery current resistor divider top
#define R134 (1.0e3)				 // Battery current resistor divider bottom
#endif
#if NUM_BAT == 5					 // Battery voltage sense 60V
#define R109_R111_R113 (475.0e3 * 3) // Battery voltage resistor divider top
#define R120 (160.0e3)				 // Battery voltage resistor divider bottom
#define R125 (787.0)				 // Battery voltage opamp divider top
#define R129 (1.5e3)				 // Battery voltage opamp divider bottom
#define R132 (100.0)				 // Battery current burden resistor
#define R131 (1.5e3)				 // Battery current resistor divider top
#define R134 (1.0e3)				 // Battery current resistor divider bottom
#endif
#if NUM_BAT == 6					 // Battery voltage sense 72V
#define R109_R111_R113 (475.0e3 * 3) // Battery voltage resistor divider top
#define R120 (133.0e3)				 // Battery voltage resistor divider bottom
#define R125 (787.0)				 // Battery voltage opamp divider top
#define R129 (1.5e3)				 // Battery voltage opamp divider bottom
#define R132 (100.0)				 // Battery current burden resistor
#define R131 (1.5e3)				 // Battery current resistor divider top
#define R134 (1.0e3)				 // Battery current resistor divider bottom
#endif
#if NUM_BAT == 7					 // Battery voltage sense 84V
#define R109_R111_R113 (475.0e3 * 3) // Battery voltage resistor divider top
#define R120 (113.0e3)				 // Battery voltage resistor divider bottom
#define R125 (787.0)				 // Battery voltage opamp divider top
#define R129 (1.5e3)				 // Battery voltage opamp divider bottom
#define R132 (100.0)				 // Battery current burden resistor
#define R131 (10.0e3)				 // Battery current resistor divider top
#define R134 (1.87e3)				 // Battery current resistor divider bottom
#endif
#if NUM_BAT == 8					 // Battery voltage sense 96V
#define R109_R111_R113 (475.0e3 * 3) // Battery voltage resistor divider top
#define R120 (95.3e3)				 // Battery voltage resistor divider bottom
#define R125 (787.0)				 // Battery voltage opamp divider top
#define R129 (1.5e3)				 // Battery voltage opamp divider bottom
#if UPS_VAOUTNOM == 625				 // 625VA, why 96V batteries?  who knows
#define R131 (3.65e3)				 // Battery current resistor divider top
#define R132 (249.0)				 // Battery current burden resistor
#define R134 (1.0e3)				 // Battery current resistor divider bottom
#else								 // any other
#define R131 (2.21e3)				 // Battery current resistor divider top
#define R132 (100.0)				 // Battery current burden resistor
#define R134 (620.0)				 // Battery current resistor divider bottom
#endif
#endif
#if NUM_BAT == 9					 // Battery voltage sense 108V
#define R109_R111_R113 (475.0e3 * 3) // Battery voltage resistor divider top
#define R120 (84.5e3)				 // Battery voltage resistor divider bottom
#define R125 (787.0)				 // Battery voltage opamp divider top
#define R129 (1.5e3)				 // Battery voltage opamp divider bottom
#define R132 (100.0)				 // Battery current burden resistor
#define R131 (2.37e3)				 // Battery current resistor divider top
#define R134 (665.0)				 // Battery current resistor divider bottom
#endif
#if NUM_BAT == 10					 // Battery voltage sense 120V
#define R109_R111_R113 (475.0e3 * 3) // Battery voltage resistor divider top
#define R120 (75.0e3)				 // Battery voltage resistor divider bottom
#define R125 (787.0)				 // Battery voltage opamp divider top
#define R129 (1.5e3)				 // Battery voltage opamp divider bottom
#define R132 (100.0)				 // Battery current burden resistor
#define R131 (2.37e3)				 // Battery current resistor divider top
#define R134 (665.0)				 // Battery current resistor divider bottom
#endif
#else   // #ifdef EARLY_BOARD_HARDWARE_SCALING
// Power scaling
//#define VA_SET (int UPS_VAOUTNOM)
#if UPS_VAOUTNOM == 625
#define R77 (0.01) // inverter current sense resistor
#define R7 (0.01)  // PFC current sense resistor
#define R163 (15)  // DC/DC Current CT burden resistor
#endif
#if UPS_VAOUTNOM == 800
#define R77 (0.01) // inverter current sense resistor
#define R7 (0.01)  // PFC current sense resistor
#define R163 (3.3) // DC/DC Current CT burden resistor
#endif
#if UPS_VAOUTNOM == 1100
#define R77 (0.01) // inverter current sense resistor
#define R7 (0.01)  // PFC current sense resistor
#define R163 (3.3) // DC/DC Current CT burden resistor
#endif
#if UPS_VAOUTNOM == 1500
#define R77 (0.01) // inverter current sense resistor
#define R7 (0.01)  // PFC current sense resistor
#define R163 (3.3) // DC/DC Current CT burden resistor
#endif
#if UPS_VAOUTNOM == 2000
#define R77 (0.01) // inverter current sense resistor
#define R7 (0.01)  // PFC current sense resistor
#define R163 (3.3) // DC/DC Current CT burden resistor
#endif
#if UPS_VAOUTNOM == 3000
#if UPS_POWOUTNOM == 3000 // 3KVA unit
#define R77 (0.0025)	  // inverter current sense resistor
#define R7 (0.0025)		  // PFC current sense resistor
#else
#define R77 (0.005) // inverter current sense resistor
#define R7 (0.005)  // PFC current sense resistor
#endif
#define R163 (3.3) // DC/DC Current CT burden resistor
#endif
#define R63 (5.11e3) // inverter current opamp input resistor
#define R67 (33.2e3) // inverter current opamp feedback resistor

#define R8 (1.0e3)  // PFC current opamp input resistor
#define R9 (14.0e3) // PFC current opamp feedback resistor

#if NUM_BAT == 2					 // Battery voltage sense 24V, TODO check this!
#define R109_R111_R113 (130.0e3 * 3) // Battery voltage resistor divider top
#define R120 (133.0e3)				 // Battery voltage resistor divider bottom
#define R125 (787.0)				 // Battery voltage opamp divider top
#define R129 (1.5e3)				 // Battery voltage opamp divider bottom
#define R132 (100.0)				 // Battery current burden resistor
#define R131 (1.5e3)				 // Battery current resistor divider top
#define R134 (1.0e3)				 // Battery current resistor divider bottom
#endif
#if NUM_BAT == 3					 // Battery voltage sense 36V, TODO check this!
#define R109_R111_R113 (215.0e3 * 3) // Battery voltage resistor divider top
#define R120 (133.0e3)				 // Battery voltage resistor divider bottom
#define R125 (787.0)				 // Battery voltage opamp divider top
#define R129 (1.5e3)				 // Battery voltage opamp divider bottom
#define R132 (100.0)				 // Battery current burden resistor
#define R131 (1.5e3)				 // Battery current resistor divider top
#define R134 (1.0e3)				 // Battery current resistor divider bottom
#endif
#if NUM_BAT == 4					 // Battery voltage sense 48V, TODO check this!
#define R109_R111_R113 (301.0e3 * 3) // Battery voltage resistor divider top
#define R120 (133.0e3)				 // Battery voltage resistor divider bottom
#define R125 (787.0)				 // Battery voltage opamp divider top
#define R129 (1.5e3)				 // Battery voltage opamp divider bottom
#define R132 (100.0)				 // Battery current burden resistor
#define R131 (1.5e3)				 // Battery current resistor divider top
#define R134 (1.0e3)				 // Battery current resistor divider bottom
#endif
#if NUM_BAT == 5					 // Battery voltage sense 60V
#define R109_R111_R113 (475.0e3 * 3) // Battery voltage resistor divider top
#define R120 (160.0e3)				 // Battery voltage resistor divider bottom
#define R125 (787.0)				 // Battery voltage opamp divider top
#define R129 (1.5e3)				 // Battery voltage opamp divider bottom
#define R132 (100.0)				 // Battery current burden resistor
#define R131 (1.5e3)				 // Battery current resistor divider top
#define R134 (1.0e3)				 // Battery current resistor divider bottom
#endif
#if NUM_BAT == 6					 // Battery voltage sense 72V
#define R109_R111_R113 (475.0e3 * 3) // Battery voltage resistor divider top
#define R120 (133.0e3)				 // Battery voltage resistor divider bottom
#define R125 (787.0)				 // Battery voltage opamp divider top
#define R129 (1.5e3)				 // Battery voltage opamp divider bottom
#define R132 (100.0)				 // Battery current burden resistor
#define R131 (1.5e3)				 // Battery current resistor divider top
#define R134 (1.0e3)				 // Battery current resistor divider bottom
#endif
#if NUM_BAT == 7					 // Battery voltage sense 84V
#define R109_R111_R113 (475.0e3 * 3) // Battery voltage resistor divider top
#define R120 (113.0e3)				 // Battery voltage resistor divider bottom
#define R125 (787.0)				 // Battery voltage opamp divider top
#define R129 (1.5e3)				 // Battery voltage opamp divider bottom
#define R132 (100.0)				 // Battery current burden resistor
#define R131 (10.0e3)				 // Battery current resistor divider top
#define R134 (1.87e3)				 // Battery current resistor divider bottom
#endif
#if NUM_BAT == 8					 // Battery voltage sense 96V
#define R109_R111_R113 (475.0e3 * 3) // Battery voltage resistor divider top
#define R120 (95.3e3)				 // Battery voltage resistor divider bottom
#define R125 (787.0)				 // Battery voltage opamp divider top
#define R129 (1.5e3)				 // Battery voltage opamp divider bottom
#if UPS_VAOUTNOM == 625				 // 625VA, why 96V batteries?  who knows
#define R131 (3.65e3)				 // Battery current resistor divider top
#define R132 (249.0)				 // Battery current burden resistor
#define R134 (1.0e3)				 // Battery current resistor divider bottom
#else								 // any other
#define R131 (2.21e3)				 // Battery current resistor divider top
#define R132 (100.0)				 // Battery current burden resistor
#define R134 (620.0)				 // Battery current resistor divider bottom
#endif
#endif
#if NUM_BAT == 9					 // Battery voltage sense 108V
#define R109_R111_R113 (475.0e3 * 3) // Battery voltage resistor divider top
#define R120 (84.5e3)				 // Battery voltage resistor divider bottom
#define R125 (787.0)				 // Battery voltage opamp divider top
#define R129 (1.5e3)				 // Battery voltage opamp divider bottom
#define R132 (100.0)				 // Battery current burden resistor
#define R131 (2.37e3)				 // Battery current resistor divider top
#define R134 (665.0)				 // Battery current resistor divider bottom
#endif
#if NUM_BAT == 10					 // Battery voltage sense 120V
#define R109_R111_R113 (475.0e3 * 3) // Battery voltage resistor divider top
#define R120 (75.0e3)				 // Battery voltage resistor divider bottom
#define R125 (787.0)				 // Battery voltage opamp divider top
#define R129 (1.5e3)				 // Battery voltage opamp divider bottom
#define R132 (100.0)				 // Battery current burden resistor
#define R131 (2.37e3)				 // Battery current resistor divider top
#define R134 (665.0)				 // Battery current resistor divider bottom
#endif
#endif  // #ifndef EARLY_BOARD_HARDWARE_SCALING

// DC Bus voltage analog gain used in adc.c
#if DC_BUS_VOLTAGE == 380
#define DCV_GAIN (84.2866)
#endif
#if DC_BUS_VOLTAGE == 400
#define DCV_GAIN (86.2069)
#endif
#if DC_BUS_VOLTAGE == 430
#define DCV_GAIN (93.02325)
#endif

// Input Relays

#define INPUT_RELAYS_MIN_OFF (95.0)
#define INPUT_RELAYS_MIN_ON (105.0)
#define INPUT_RELAYS_MAX_OFF (135.0)
#define INPUT_RELAYS_MAX_ON (130.0)

// Program configuration

#define OLD_MICRO_COMPATIBLE (1) // 1=yes, old, non-direct PWM
// DANGER!!  Will blow up inverter if Model F modified for direct connect and this turned off
// No underlap
#define INVERTER_DIRECT_CONNECT (0) // use EPWM2 to run upper and lower IGBTs directly
#define INVERTER_PULSE_BY_PULSE (0) // Use if updating sine on pulse by pulse, else update on timer
#define UPS_CHECKSUM (TRUE)
#define DSPCLOCK_MHZ (DSPCLOCK / 1.0e6) // DSPCLOCK defined in F2806x_90MHZBOOT.c
#define DSPCLOCK_PER_USEC DSPCLOCK_MHZ
// Next two parameters done in configuration files
//#define INVERTER_UPDATES_PER_CYCLE (128)
//#define PWM_FREQUENCY 50e3

#define PWM_DEADTIME (1.0)	// in microseconds
#define foreverLoop while (1) // Looks cooler
#define BAT_TABLE_SIZE (20)   // number of entries in each tab

#endif // _system_config_h
