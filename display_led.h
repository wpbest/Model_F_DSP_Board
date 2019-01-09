// display_led.h

#ifndef _display_led_h // inclusion guard
#define _display_led_h

#include "types.h"

#define LED_DISP_ON 0, 0 // Column, Row
#define LED_DISP_BYPASS 0, 1
#define LED_DISP_FAULT 0, 2
#define LED_DISP_SERVICE_BAT 0, 3
#define LED_DISP_LOAD3 1, 0
#define LED_DISP_LOAD2 1, 1
#define LED_DISP_LOAD1 1, 2
#define LED_DISP_REMOTE 1, 3 // Header for remote LED
#define LED_DISP_EXTDC 1, 3  // Another name for the Remote LED
#define LED_DISP_BAT4 2, 0
#define LED_DISP_BAT5 2, 1
#define LED_DISP_LOAD5 2, 2
#define LED_DISP_LOAD4 2, 3
#define LED_DISP_OPTION 3, 0 // Off=Load Bar, On=Input Voltage Bar, only used for GAA
#define LED_DISP_BAT1 3, 1
#define LED_DISP_BAT2 3, 2
#define LED_DISP_BAT3 3, 3

// Note: the mapping for the vertical/horizontal display is the same for Model F, different for Model C
#define LED_DISP_VERT_ON 0, 0 // Column, Row
#define LED_DISP_VERT_BYPASS 0, 1
#define LED_DISP_VERT_FAULT 0, 2
#define LED_DISP_VERT_SERVICE_BAT 0, 3
#define LED_DISP_VERT_LOAD3 1, 0
#define LED_DISP_VERT_LOAD2 1, 1
#define LED_DISP_VERT_LOAD1 1, 2
#define LED_DISP_VERT_REMOTE 1, 3 // Header for remote LED, Same as EXTDC 
#define LED_DISP_VERT_EXTDC 1, 3  // Another name for the remote LED
#define LED_DISP_VERT_BAT4 2, 0
#define LED_DISP_VERT_BAT5 2, 1
#define LED_DISP_VERT_LOAD5 2, 2
#define LED_DISP_VERT_LOAD4 2, 3
#define LED_DISP_VERT_OPTION 3, 0 // Off=Load Bar, On=Input Voltage Bar, only used for GAA
#define LED_DISP_VERT_BAT1 3, 1
#define LED_DISP_VERT_BAT2 3, 2
#define LED_DISP_VERT_BAT3 3, 3

#define LED_DISP_LOAD_BAR 0
#define LED_DISP_BAT_BAR 1

#define LED_DISP_BAR_FLASH 1
#define LED_DISP_BAR_SOLID 0

#define LED_DISP_OFF 0
#define LED_DISP_SOLID 1
#define LED_DISP_FLASH 2
#define LED_DISP_FLASH_ALT 3

//static int LedDisplayOnButton = FALSE, LedDisplayOffButton = FALSE;

// function prototypes
void initDisplay(void);
int displayLed(void);
int displayLedSet(int column, int row, int state); // Off=0, Solid=1, Flash=2, FlashAlt=3
int displayLedBar(int bar, float percent, int flash);

#endif // _display_led_h
