// adc.h
#ifndef _drv_adc_h_ // inclusion guard
#define _drv_adc_h_

#include "F2806x_Device.h"
#include "types.h"

#define ADC_usDELAY 5000L
#define Device_cal (void (*)(void))0x3D7C80

// prototype
//void InitAdc(void);
void InitializeADC(void);
interrupt void adc_isr(void);
void adcProcess(void);

// Global variables used in this example:

#endif
