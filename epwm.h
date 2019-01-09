// epwm.h
#ifndef _epwm_h_ // inclusion guard
#define _epwm_h_

#include "F2806x_Device.h"
#include "types.h"

/*
Uint16	EPwm1_DB_Direction;
Uint16	EPwm2_DB_Direction;
Uint16	EPwm3_DB_Direction;
*/

// Maximum Dead Band values
#define EPWM1_MAX_DB 0x03FF
#define EPWM2_MAX_DB 0x03FF
#define EPWM3_MAX_DB 0x03FF

#define EPWM1_MIN_DB 450 // 1uSec 1e-6/(1/150e6) * 2 for rise and fall deadtime
#define EPWM2_MIN_DB 0
#define EPWM3_MIN_DB 0

#define PWM_PERIOD ((DSPCLOCK / PWM_FREQUENCY) / 2) // center aligned
#define PWM_CENTER (PWM_PERIOD / 2)

#define ISO_PS_PWM_PERIOD	(uint16_t) (DSPCLOCK / 50.0e3)			// Set to DSPCLOCK divided by 50kHz (operating frequency of the gate drive transformer)
#define ISO_PS_DUTY			(uint16_t) (DSPCLOCK / 50.0e3 / 2.0)	// Set to 50% of Period register

// function prototypes

void initInverter(void);
void turnOnInverter(void);
void turnOffInverter(void);
void setDeadtime(float uSec);
interrupt void epwm1_isr(void);
interrupt void epwm2_isr(void);
interrupt void epwm3_isr(void);
void InitEPwmGpio(void);
void InitEPwm2Gpio(void);
void InitEPwm3Gpio(void);
void InitEPwm();
void InitEPwm2Example();
void InitEPwm3Example();
interrupt void epwm1_isr(void);
interrupt void epwm2_isr(void);
interrupt void epwm3_isr(void);

#endif // _epwm_h_
