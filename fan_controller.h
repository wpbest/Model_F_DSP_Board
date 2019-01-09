/*!	@file		fan_controller.h
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
#ifndef FAN_CONTROLLER_H_
#define FAN_CONTROLLER_H_

#include "types.h"

// Uncomment only one of the following three defines to select the processor that the code is running on
//! MSP430G2553 processor family on FanJr Board
//#define FANS_FANJR_BOARD
//! TMS320x2806x processor family
#define FANS_DSP_BOARD

#if defined FANS_DSP_BOARD
#include "ups.h"
#endif

#if defined FANS_FANJR_BOARD
#include <msp430g2553.h>
#endif

#define FAN_TYPE_TABLE 1, 2, 3, 0 // TODO: Define Types here for fans to initialize and control, plus it will move to the configuration file

typedef enum
{
	FAN_GOOD,
	FAN_FAIL,
	FAN_OVER
} fanStatusT;

typedef enum
{
	FAN_AUTO,
	FAN_MANUAL
} fanControlT;

typedef struct tFan
{
	unsigned int Enable; // Fan present
	fanStatusT Status;
	fanControlT fanControl;
	unsigned int RPM;
	unsigned int targetRPM; // Target RPM for speed control
	unsigned int fanPwmValue;
	float speed_cmd_pct;
	float fanTemperature; // Temperature sense for fan control
	unsigned int TachCount;
	unsigned int tachToRpmMult;
	unsigned int RPM_Threshold;
	unsigned int RPM_High;
} tFan;

//! Number of fans running on this processor
#define FAN_COUNT (4)
#define TEMPERATURE_100 (60.0)
#define TEMPERATURE_50 (30.0)
#define SPEED_MINIMUM (0.5)
#define SPEED_MAXIMUM (1.0)
#define DELTA_CONTROL ((SPEED_MAXIMUM - SPEED_MINIMUM) / (TEMPERATURE_100 - TEMPERATURE_50))
#define FAN_PWM_PERIOD (1024)

#if defined FANS_DSP_BOARD
#define FAN_1_LED_RED            \
	pinLatch(LATCH_LED_COL0, 1); \
	pinLatch(LATCH_LED_ROW0, 0); \
	pinLatch(LATCH_LED_ROW1, 1);
#define FAN_1_LED_YEL            \
	pinLatch(LATCH_LED_COL0, 1); \
	pinLatch(LATCH_LED_ROW0, 0); \
	pinLatch(LATCH_LED_ROW1, 0);
#define FAN_1_LED_GRN            \
	pinLatch(LATCH_LED_COL0, 1); \
	pinLatch(LATCH_LED_ROW0, 1); \
	pinLatch(LATCH_LED_ROW1, 0);
#define FAN_1_LED_OFF            \
	pinLatch(LATCH_LED_COL0, 0); \
	pinLatch(LATCH_LED_ROW0, 1); \
	pinLatch(LATCH_LED_ROW1, 1);
#define FAN_2_LED_RED            \
	pinLatch(LATCH_LED_COL0, 1); \
	pinLatch(LATCH_LED_ROW2, 0); \
	pinLatch(LATCH_LED_ROW3, 1);
#define FAN_2_LED_YEL            \
	pinLatch(LATCH_LED_COL0, 1); \
	pinLatch(LATCH_LED_ROW2, 0); \
	pinLatch(LATCH_LED_ROW3, 0);
#define FAN_2_LED_GRN            \
	pinLatch(LATCH_LED_COL0, 1); \
	pinLatch(LATCH_LED_ROW2, 1); \
	pinLatch(LATCH_LED_ROW3, 0);
#define FAN_2_LED_OFF            \
	pinLatch(LATCH_LED_COL0, 0); \
	pinLatch(LATCH_LED_ROW2, 1); \
	pinLatch(LATCH_LED_ROW3, 1);
#define FAN_3_LED_RED            \
	pinLatch(LATCH_LED_COL1, 1); \
	pinLatch(LATCH_LED_ROW0, 0); \
	pinLatch(LATCH_LED_ROW1, 1);
#define FAN_3_LED_YEL            \
	pinLatch(LATCH_LED_COL1, 1); \
	pinLatch(LATCH_LED_ROW0, 0); \
	pinLatch(LATCH_LED_ROW1, 0);
#define FAN_3_LED_GRN            \
	pinLatch(LATCH_LED_COL1, 1); \
	pinLatch(LATCH_LED_ROW0, 1); \
	pinLatch(LATCH_LED_ROW1, 0);
#define FAN_3_LED_OFF            \
	pinLatch(LATCH_LED_COL1, 0); \
	pinLatch(LATCH_LED_ROW0, 1); \
	pinLatch(LATCH_LED_ROW1, 1);
#define FAN_4_LED_RED            \
	pinLatch(LATCH_LED_COL1, 1); \
	pinLatch(LATCH_LED_ROW2, 0); \
	pinLatch(LATCH_LED_ROW3, 1);
#define FAN_4_LED_YEL            \
	pinLatch(LATCH_LED_COL1, 1); \
	pinLatch(LATCH_LED_ROW2, 0); \
	pinLatch(LATCH_LED_ROW3, 0);
#define FAN_4_LED_GRN            \
	pinLatch(LATCH_LED_COL1, 1); \
	pinLatch(LATCH_LED_ROW2, 1); \
	pinLatch(LATCH_LED_ROW3, 0);
#define FAN_4_LED_OFF            \
	pinLatch(LATCH_LED_COL1, 0); \
	pinLatch(LATCH_LED_ROW2, 1); \
	pinLatch(LATCH_LED_ROW3, 1);
#define DISABLE_RPM_INTERRUPTS
#define ENABLE_RPM_INTERRUPTS
#endif

#if defined FANS_FANJR_BOARD
#define FAN_1_LED_RED            \
	pinLatch(LATCH_LED_COL0, 1); \
	pinLatch(LATCH_LED_ROW0, 0); \
	pinLatch(LATCH_LED_ROW1, 1);
#define FAN_1_LED_YEL            \
	pinLatch(LATCH_LED_COL0, 1); \
	pinLatch(LATCH_LED_ROW0, 0); \
	pinLatch(LATCH_LED_ROW1, 0);
#define FAN_1_LED_GRN            \
	pinLatch(LATCH_LED_COL0, 1); \
	pinLatch(LATCH_LED_ROW0, 1); \
	pinLatch(LATCH_LED_ROW1, 0);
#define FAN_1_LED_OFF            \
	pinLatch(LATCH_LED_COL0, 0); \
	pinLatch(LATCH_LED_ROW0, 1); \
	pinLatch(LATCH_LED_ROW1, 1);
#define FAN_2_LED_RED            \
	pinLatch(LATCH_LED_COL0, 1); \
	pinLatch(LATCH_LED_ROW2, 0); \
	pinLatch(LATCH_LED_ROW3, 1);
#define FAN_2_LED_YEL            \
	pinLatch(LATCH_LED_COL0, 1); \
	pinLatch(LATCH_LED_ROW2, 0); \
	pinLatch(LATCH_LED_ROW3, 0);
#define FAN_2_LED_GRN            \
	pinLatch(LATCH_LED_COL0, 1); \
	pinLatch(LATCH_LED_ROW2, 1); \
	pinLatch(LATCH_LED_ROW3, 0);
#define FAN_2_LED_OFF            \
	pinLatch(LATCH_LED_COL0, 0); \
	pinLatch(LATCH_LED_ROW2, 1); \
	pinLatch(LATCH_LED_ROW3, 1);
#define FAN_3_LED_RED            \
	pinLatch(LATCH_LED_COL1, 1); \
	pinLatch(LATCH_LED_ROW0, 0); \
	pinLatch(LATCH_LED_ROW1, 1);
#define FAN_3_LED_YEL            \
	pinLatch(LATCH_LED_COL1, 1); \
	pinLatch(LATCH_LED_ROW0, 0); \
	pinLatch(LATCH_LED_ROW1, 0);
#define FAN_3_LED_GRN            \
	pinLatch(LATCH_LED_COL1, 1); \
	pinLatch(LATCH_LED_ROW0, 1); \
	pinLatch(LATCH_LED_ROW1, 0);
#define FAN_3_LED_OFF            \
	pinLatch(LATCH_LED_COL1, 0); \
	pinLatch(LATCH_LED_ROW0, 1); \
	pinLatch(LATCH_LED_ROW1, 1);
#define FAN_4_LED_RED            \
	pinLatch(LATCH_LED_COL1, 1); \
	pinLatch(LATCH_LED_ROW2, 0); \
	pinLatch(LATCH_LED_ROW3, 1);
#define FAN_4_LED_YEL            \
	pinLatch(LATCH_LED_COL1, 1); \
	pinLatch(LATCH_LED_ROW2, 0); \
	pinLatch(LATCH_LED_ROW3, 0);
#define FAN_4_LED_GRN            \
	pinLatch(LATCH_LED_COL1, 1); \
	pinLatch(LATCH_LED_ROW2, 1); \
	pinLatch(LATCH_LED_ROW3, 0);
#define FAN_4_LED_OFF            \
	pinLatch(LATCH_LED_COL1, 0); \
	pinLatch(LATCH_LED_ROW2, 1); \
	pinLatch(LATCH_LED_ROW3, 1);
#define DISABLE_RPM_INTERRUPTS P2IE &= ~(TACH_FAN_INTS); // Disable tachometer interrupts
#define ENABLE_RPM_INTERRUPTS P2IE |= (TACH_FAN_INTS);   // Re-enable tachometer interrupt
#endif

// Prototypes
void initializeFans(void);
void fanStateController(void);
int fanFailCheck(void);

#endif /* FAN_CONTROLLER_H_ */
