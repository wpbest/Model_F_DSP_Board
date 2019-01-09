// timer.h

//#ifndef _timer_h								// inclusion guard
//#define _timer_h
#ifndef TIMER_H // inclusion guard
#define TIMER_H

#include "F2806x_Device.h"
#include "types.h"

typedef struct _rtc_timeT
{
	unsigned long /* uint16_t */ mSeconds; // Valid range 0 - 1000 * 60 * 60 * 24 milliseconds (one day)
	unsigned char /* uint8_t */ Seconds;   // Valid range 0 - 59 seconds
	unsigned char /* uint8_t */ Minutes;   // Valid range 0 - 59 minutes
	unsigned char /* uint8_t */ Hours;	 // Valid range 0 - 23 (24-Hour format) hours
	unsigned char /* uint8_t */ DayOfWeek; // Valid range 0 - 6  (0 = Sunday) DOW
	unsigned char /* uint8_t */ Day;	   // Valid range 1 - 28, 29, 30, or 31 days
	unsigned char /* uint8_t */ Month;	 // Valid range 1 - 12 months
	unsigned int /* uint16_t */ Year;	  // Valid range 0 - 4095 years
} rtc_timeT;

// function prototypes
void initTime(void);
int timer(timeT timestart, long delay);
timeT getTime(void);
void setRTCtime(rtc_timeT rtc_time);
char *rtcString(rtc_timeT rtcTime);
rtc_timeT getRTCtime(void);
interrupt void cpu_timer0_isr(void);
interrupt void cpu_timer1_isr(void);
interrupt void cpu_timer2_isr(void);
timeT getTime(void);
int timer(timeT timestart, long delay);
int timerUsec(timeT timestart, float delayUsec);
void SetPeriodCpuTimer(struct CPUTIMER_VARS *Timer, float Freq, float Period);
long timeExpired(timeT timeStart, timeT timeNow);
float timeDiffMsec(timeT timeStart, timeT timeNow);
void setTime(timeT timeSet);
#endif // _timer_h
