// utilities.h

#ifndef _utilities_h_ // inclusion guard
#define _utilities_h_

#include "types.h"

#define ITOA_BUF_MAX (15)

// prototype
int iMax(int var1, int var2);
int iMin(int var1, int var2);
int iRange(int test, int limit1, int limit2);
long lMax(long lvar1, long lvar2);
long lMin(long lvar1, long lvar2);
int lRange(long test, long limit1, long limit2);
float fMax(float fvar1, float fvar2);
float fMin(float fvar1, float fvar2);
int fRange(float test, float limit1, float limit2);
char *itoa(int i);
char *itoa3(int i);
char *ftoaDp2(volatile float fnumber);
char *ftoaDp3(volatile float fnumber);
int gpio(int gpioNum, int state, gpioT dir);
void tabWPeriods(int tabTo, char *str);
// Global variables
char *convertMSecToMinutesSeconds (uint32_t msec, char *buf); 
#endif // _utilities_h_
