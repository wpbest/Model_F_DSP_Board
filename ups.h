// ups.h

#ifndef _ups_h // inclusion guard
#define _ups_h

#include "F2806x_Device.h"
#include "ups_state_controller.h"
#include "types.h"
#include "system_config.h"
#include "system_config_ModelF_Hardware.h"

// constants
#define UPS_POLL_INTERVAL 1000 // in milliseconds
#define MAX_SINE_VALUES 128
#define DC_BUS_VOLT 300.0
#define MAXVOLT (DC_BUS_VOLT / SQRTOF2)

// definitions: Octal Latch Address/Data/Clock/Enable
#define LATCH_ENABLE_OFF GpioDataRegs.GPBSET.bit.GPIO54 = 1;  // Latch enable off, OE* 0=Enable, 1=Disable
#define LATCH_ENABLE_ON GpioDataRegs.GPBCLEAR.bit.GPIO54 = 1; // Latch enable on

#define LATCH_CLK_OFF GpioDataRegs.GPACLEAR.bit.GPIO20 = 1; // LATCH_CLK off
#define LATCH_CLK_ON GpioDataRegs.GPASET.bit.GPIO20 = 1;	// LATCH_CLK on

#define LATCH_ADDR0_0 GpioDataRegs.GPBCLEAR.bit.GPIO41 = 1; // ADR0
#define LATCH_ADDR0_1 GpioDataRegs.GPBSET.bit.GPIO41 = 1;
#define LATCH_ADDR1_0 GpioDataRegs.GPBCLEAR.bit.GPIO34 = 1; // ADR1
#define LATCH_ADDR1_1 GpioDataRegs.GPBSET.bit.GPIO34 = 1;
#define LATCH_ADDR2_0 GpioDataRegs.GPBCLEAR.bit.GPIO53 = 1; // ADR2
#define LATCH_ADDR2_1 GpioDataRegs.GPBSET.bit.GPIO53 = 1;

#define LATCH_ADDR0 GpioDataRegs.GPBDAT.bit.GPIO41; // ADR0 Read GPIO data
#define LATCH_ADDR1 GpioDataRegs.GPBDAT.bit.GPIO34; // ADR1
#define LATCH_ADDR2 GpioDataRegs.GPBDAT.bit.GPIO53; // ADR2

#define LATCH_DATA0_0 GpioDataRegs.GPBCLEAR.bit.GPIO33 = 1; // DAT0
#define LATCH_DATA0_1 GpioDataRegs.GPBSET.bit.GPIO33 = 1;
#define LATCH_DATA1_0 GpioDataRegs.GPBCLEAR.bit.GPIO32 = 1; // DAT1
#define LATCH_DATA1_1 GpioDataRegs.GPBSET.bit.GPIO32 = 1;
#define LATCH_DATA2_0 GpioDataRegs.GPACLEAR.bit.GPIO22 = 1; // DAT2
#define LATCH_DATA2_1 GpioDataRegs.GPASET.bit.GPIO22 = 1;
#define LATCH_DATA3_0 GpioDataRegs.GPACLEAR.bit.GPIO24 = 1; // DAT3
#define LATCH_DATA3_1 GpioDataRegs.GPASET.bit.GPIO24 = 1;
#define LATCH_DATA4_0 GpioDataRegs.GPACLEAR.bit.GPIO13 = 1; // DAT4
#define LATCH_DATA4_1 GpioDataRegs.GPASET.bit.GPIO13 = 1;
#define LATCH_DATA5_0 GpioDataRegs.GPBCLEAR.bit.GPIO58 = 1; // DAT5
#define LATCH_DATA5_1 GpioDataRegs.GPBSET.bit.GPIO58 = 1;
#define LATCH_DATA6_0 GpioDataRegs.GPBCLEAR.bit.GPIO57 = 1; // DAT6
#define LATCH_DATA6_1 GpioDataRegs.GPBSET.bit.GPIO57 = 1;
#define LATCH_DATA7_0 GpioDataRegs.GPACLEAR.bit.GPIO3 = 1; // DAT7
#define LATCH_DATA7_1 GpioDataRegs.GPASET.bit.GPIO3 = 1;

#define LATCH_DATA_READ_0 GpioDataRegs.GPBDAT.bit.GPIO33
#define LATCH_DATA_READ_1 GpioDataRegs.GPBDAT.bit.GPIO32
#define LATCH_DATA_READ_2 GpioDataRegs.GPADAT.bit.GPIO22
#define LATCH_DATA_READ_3 GpioDataRegs.GPADAT.bit.GPIO24
#define LATCH_DATA_READ_4 GpioDataRegs.GPADAT.bit.GPIO13
#define LATCH_DATA_READ_5 GpioDataRegs.GPBDAT.bit.GPIO58
#define LATCH_DATA_READ_6 GpioDataRegs.GPBDAT.bit.GPIO57
#define LATCH_DATA_READ_7 GpioDataRegs.GPADAT.bit.GPIO3
// Set all data pins to zero
#define LATCH_DATA_ALL_0 LATCH_DATA0_0 LATCH_DATA1_0 LATCH_DATA2_0 LATCH_DATA3_0 LATCH_DATA4_0 LATCH_DATA5_0 LATCH_DATA6_0 LATCH_DATA7_0

#define LATCH_ADDRESS_0 LATCH_ADDR2_0 LATCH_ADDR1_0 LATCH_ADDR0_0
#define LATCH_ADDRESS_1 LATCH_ADDR2_0 LATCH_ADDR1_0 LATCH_ADDR0_1
#define LATCH_ADDRESS_2 LATCH_ADDR2_0 LATCH_ADDR1_1 LATCH_ADDR0_0
#define LATCH_ADDRESS_3 LATCH_ADDR2_0 LATCH_ADDR1_1 LATCH_ADDR0_1
#define LATCH_ADDRESS_4 LATCH_ADDR2_1 LATCH_ADDR1_0 LATCH_ADDR0_0
#define LATCH_ADDRESS_5 LATCH_ADDR2_1 LATCH_ADDR1_0 LATCH_ADDR0_1
#define LATCH_ADDRESS_6 LATCH_ADDR2_1 LATCH_ADDR1_1 LATCH_ADDR0_0
#define LATCH_ADDRESS_7 LATCH_ADDR2_1 LATCH_ADDR1_1 LATCH_ADDR0_1

#define LATCH_DATA_DIR0_IN GpioCtrlRegs.GPBDIR.bit.GPIO33 = 0;
#define LATCH_DATA_DIR0_OUT GpioCtrlRegs.GPBDIR.bit.GPIO33 = 1;
#define LATCH_DATA_DIR1_IN GpioCtrlRegs.GPBDIR.bit.GPIO32 = 0;
#define LATCH_DATA_DIR1_OUT GpioCtrlRegs.GPBDIR.bit.GPIO32 = 1;
#define LATCH_DATA_DIR2_IN GpioCtrlRegs.GPADIR.bit.GPIO22 = 0;
#define LATCH_DATA_DIR2_OUT GpioCtrlRegs.GPADIR.bit.GPIO22 = 1;
#define LATCH_DATA_DIR3_IN GpioCtrlRegs.GPADIR.bit.GPIO24 = 0;
#define LATCH_DATA_DIR3_OUT GpioCtrlRegs.GPADIR.bit.GPIO24 = 1;
#define LATCH_DATA_DIR4_IN GpioCtrlRegs.GPADIR.bit.GPIO13 = 0;
#define LATCH_DATA_DIR4_OUT GpioCtrlRegs.GPADIR.bit.GPIO13 = 1;
#define LATCH_DATA_DIR5_IN GpioCtrlRegs.GPBDIR.bit.GPIO58 = 0;
#define LATCH_DATA_DIR5_OUT GpioCtrlRegs.GPBDIR.bit.GPIO58 = 1;
#define LATCH_DATA_DIR6_IN GpioCtrlRegs.GPBDIR.bit.GPIO57 = 0;
#define LATCH_DATA_DIR6_OUT GpioCtrlRegs.GPBDIR.bit.GPIO57 = 1;
#define LATCH_DATA_DIR7_IN GpioCtrlRegs.GPADIR.bit.GPIO3 = 0;
#define LATCH_DATA_DIR7_OUT GpioCtrlRegs.GPADIR.bit.GPIO3 = 1;

#define LATCH_DATA_DIR0 GpioCtrlRegs.GPBDIR.bit.GPIO33

#define LATCH_DATA_DIR_ALL_IN LATCH_DATA_DIR0_IN LATCH_DATA_DIR1_IN LATCH_DATA_DIR2_IN LATCH_DATA_DIR3_IN LATCH_DATA_DIR4_IN LATCH_DATA_DIR5_IN LATCH_DATA_DIR6_IN LATCH_DATA_DIR7_IN
#define LATCH_DATA_DIR_ALL_OUT LATCH_DATA_DIR0_OUT LATCH_DATA_DIR1_OUT LATCH_DATA_DIR2_OUT LATCH_DATA_DIR3_OUT LATCH_DATA_DIR4_OUT LATCH_DATA_DIR5_OUT LATCH_DATA_DIR6_OUT LATCH_DATA_DIR7_OUT

// definition: Latch chip number, pin
#define LATCH_LED_DISPLAY_COL1 0, 0
#define LATCH_LED_DISPLAY_COL2 0, 1
#define LATCH_LED_DISPLAY_COL3 0, 2
#define LATCH_LED_DISPLAY_COL4 0, 3
#define LATCH_LED_DISPLAY_ROW1 0, 4
#define LATCH_LED_DISPLAY_ROW2 0, 5
#define LATCH_LED_DISPLAY_ROW3 0, 6
#define LATCH_LED_DISPLAY_ROW4 0, 7

#define LATCH_LED_COL0 1, 0
#define LATCH_LED_COL1 1, 1
#define LATCH_LED_COL2 1, 2
#define LATCH_LED_COL3 1, 3
#define LATCH_LED_ROW0 1, 4
#define LATCH_LED_ROW1 1, 5
#define LATCH_LED_ROW2 1, 6
#define LATCH_LED_ROW3 1, 7

#define LATCH_ISO_OUT1 2, 0
#define LATCH_ISO_OUT2 2, 1
#define LATCH_BANK0 2, 2 // on Model F Rev. E Bank0 and Bank1 are swapped
#define LATCH_BANK1 2, 3 // these represent Rev F+ and Micro board
#define LATCH_IN_RLY1_n 2, 4
#define LATCH_IN_RLY2_n 2, 5
#define LATCH_SIGNAL_RLY1 2, 6
#define LATCH_SIGNAL_RLY2 2, 7

#define LATCH_RELAY3 3, 0
#define LATCH_RELAY4 3, 1
#define LATCH_BYP_NORM 3, 2
#define LATCH_BYPASS 3, 3
#define LATCH_PFC_SD 3, 4
#define LATCH_AC_ENABLE_n 3, 5
#define LATCH_DC_DC_DISABLE_n 3, 6
#define LATCH_LVPS_SD 3, 7

#define LATCH_FAST_CHG 4, 0
#define LATCH_DISABLE_CHG_n 4, 1
#define LATCH_PFC_BAT_CHK_n 4, 2
#define LATCH_FAN_SPEED_n 4, 3
#define LATCH_DOUT0_n 4, 4
#define LATCH_DOUT1_n 4, 5
#define LATCH_DOUT2_n 4, 6
#define LATCH_SONALERT 4, 7

#define LATCH_DIN0_n 5, 0
#define LATCH_DIN1_n 5, 1
#define LATCH_DIN2_n 5, 2
#define LATCH_DIN3_n 5, 3
#define LATCH_DIN4_n 5, 4
#define LATCH_DIN5_n 5, 5
#define LATCH_DIN6_n 5, 6
#define LATCH_DIN7_n 5, 7

#define LATCH_REMOTE_IN_n 6, 0 // Remote in
#define LATCH_DIN8_n 6, 0	  // Remote in
#define LATCH_DIN9_n 6, 1
#define LATCH_DIN10_n 6, 2
#define LATCH_ISO_IN2 6, 3
#define LATCH_12V_LOW 6, 4	 // Signal from MSP indicating low 12V supply for shutdown
#define LATCH_CHG_LITHIUM 6, 5 // Indicates using Lithium batteries (also Wire Fault on old board)
#define LATCH_DC_BAL_DIG 6, 6  // Digital signal indicates DC component
#define LATCH_UNUSED 6, 7

// There isn't a Latch 7 (yet...)

#define DC_DC_ON_NOT GpioDataRegs.GPADAT.bit.GPIO1
#define INV_OC_NOT GpioDataRegs.GPADAT.bit.GPIO25
#define BUTTON_PRESSED GpioDataRegs.GPADAT.bit.GPIO6
#define BYPASS_FORCE_ON pinLatch(LATCH_BYPASS, 1)	 // prevents bypass relay from energizing
#define BYPASS_AUTO1 pinLatch(LATCH_BYPASS, 0)		  // allows bypass relay to energize if AUTO2
#define BYPASS_AUTO2 pinLatch(LATCH_BYP_NORM, 1)	  // turns off FORCE_OFF2
#define BYPASS_FORCE_OFF1 pinLatch(LATCH_BYPASS, 0)   // allows bypass relay to energize
#define BYPASS_FORCE_OFF2 pinLatch(LATCH_BYP_NORM, 0) // completes path and energizes coil
#define LAN_LOW_BAT pinLatch(LATCH_DOUT0_n, 0)		  //
#define LAN_OK_BAT pinLatch(LATCH_DOUT0_n, 1)		  //
#define LAN_ON_BAT pinLatch(LATCH_DOUT1_n, 0)		  //
#define LAN_OFF_BAT pinLatch(LATCH_DOUT1_n, 1)		  //
#define CHARGE_FAST pinLatch(LATCH_FAST_CHG, 1)		  // signal inverted on DSP board, low on Model F turns it to fast
#define CHARGE_SLOW pinLatch(LATCH_FAST_CHG, 0)		  // signal inverted on DSP board, low on Model F turns it to fast
#define CHARGE_OFF pinLatch(LATCH_DISABLE_CHG_n, 0)   // signal inverted on DSP board, high on Model F disables charger
#define CHARGE_ON pinLatch(LATCH_DISABLE_CHG_n, 1)	// signal inverted on DSP board, high on Model F disables charger

// external variables
extern volatile int pinLatchState[8][8]; // this array will be used to set latches in millisecond timer routine

//int comBypass = FALSE;	// tells router to directly connect master to slave (UPS to Serial Port)

// function prototypes

int Lucy(void);
int inverter(void);
int batteries(void);
int pfc(void);
int bypass(void);
int fans(void);
int display(void);
int remote(void);
int externalDioInput(int channel);
int inputRelays(operatingModesT command);
float updateInverterGain(float gainCommand);
//float setFrequency(float freq, float hzPerSec);
void invGainSet(float cmdPct);
float invGetGain(void);
float getSineValue(int tableIndex);
float getSineValueShifted(int tableIndex);
void setupPins(void);
int pinLatch(int latch, int pin, int state);
int pinLatchCheck(int latch, int pin);
void init_ups_com(struct upsDataStrucT *upsData); // *TODO - Add virtual UPS Nominal values in initiation
void sonalert(operatingModesT mode);
float batteryChargePercent(void);
float thermistorResistanceToTemp(float resistance, float ohmThermistor, float beta);
float thermistorConvertExternal(float volts);
float thermistorConvertInternal(float volts);
void initUps(void);
void initUpsData(void);
interrupt void utilityZeroCross_isr(void);
interrupt void inverterOvercurrent_isr(void);
char *secToHmsString(long sec);                 // Convert Seconds to Hours:Minutes:Seconds
char *secToMsString(long sec);                  // Convert Seconds to Minutes:Seconds
char *timeString(timeT time);
char *runtimeString(void);
void updateOctalLatchState(void);

#define ALTERNATE_BUTTON_PRESS
#if (defined ALTERNATE_BUTTON_PRESS)
void alternateButtonPress(void);
#endif

void fakeButton(buttonT button);
int vinInRange(struct upsDataStrucT *upsData);

Uint32 initBankSwitches(void);
uint8_t getFastChargeCyclingOn();
#endif // _ups_h
