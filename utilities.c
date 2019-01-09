// utilities.c

//#include "types.h"
#include "utilities.h"
#include "string.h"
#include "math.h"
#include "stdlib.h"
#include "stdio.h"
//#include "types.h"

#define TRUE 0x01
#define FALSE 0x00

const Uint32 gpioMap[] = { // mapping channels to bits, GPA and GPB registers
	BIT0, BIT1, BIT2, BIT3, BIT4, BIT5, BIT6, BIT7,
	BIT8, BIT9, BIT10, BIT11, BIT12, BIT13, BIT14, BIT15,
	BIT16, BIT17, BIT18, BIT19, BIT20, BIT21, BIT22, BIT23,
	BIT24, BIT25, BIT26, BIT27, BIT28, BIT29, BIT30, BIT31,
	BIT0, BIT1, BIT2, BIT3, BIT4, BIT5, BIT6, BIT7,
	BIT8, BIT9, BIT10, BIT11, BIT12, BIT13, BIT14, BIT15,
	BIT16, BIT17, BIT18, BIT19, BIT20, BIT21, BIT22, BIT23,
	BIT24, BIT25, BIT26, BIT27, BIT28, BIT29, BIT30, BIT31};

// returns a string conversion of an integer value, I've shortened it to 16 bit integer
// and added range check
// And then I threw it out when I found the Bastards at TI had ltoa() but not itoa().
char *itoa(int i)
{
	/* Room for INT_DIGITS digits, - and '\0' (19+2) */
	static char buf[ITOA_BUF_MAX];

	ltoa((long)i, buf);

	return (&buf[0]);
}

// takes itoa and pads it to 3 characters with leading zeros
char *itoa3(int i)
{
	static char intBuf[ITOA_BUF_MAX], buf[ITOA_BUF_MAX];
	static int bufLen;

	strcpy(buf, NULL); // clear previous stuff
	if ((i >= 0))
	{ // only process positive numbers, 3 numbers in length
		strcpy(intBuf, itoa(i));
		bufLen = strlen(intBuf);
		switch (bufLen)
		{
		case 1:
			strcpy(buf, "00");
			break;
		case 2:
			strcpy(buf, "0");
			break;
		case 3: // buf is clear, no need to change for this
			break;
		default:
			strcpy(intBuf, "000"); // number too big, reset to zero
			break;
		}
		strcat(buf, intBuf); // put pad in front of number
	}
	else
	{						// if negative
		strcpy(buf, "000"); // return zero
	}

	return buf;
}

// return the maximum of two long point numbers
int iMax(int var1, int var2)
{
	if (var1 >= var2)
	{
		return (var1);
	}
	else
	{
		return (var2);
	}
}

// return the minimum of two long point numbers
int iMin(int var1, int var2)
{
	if (var1 <= var2)
	{
		return (var1);
	}
	else
	{
		return (var2);
	}
}

// return True if first number is between the other two numbers
int iRange(int test, int limit1, int limit2)
{

	int varMin, varMax;

	varMin = iMin(limit1, limit2);
	varMax = iMax(limit1, limit2);

	if ((varMin <= test) && (test <= varMax))
	{
		return (TRUE);
	}
	else
	{
		return (FALSE);
	}
}

// return the maximum of two long point numbers
long lMax(long lvar1, long lvar2)
{
	if (lvar1 >= lvar2)
	{
		return (lvar1);
	}
	else
	{
		return (lvar2);
	}
}

// return the minimum of two long point numbers
long lMin(long lvar1, long lvar2)
{
	if (lvar1 <= lvar2)
	{
		return (lvar1);
	}
	else
	{
		return (lvar2);
	}
}

// return True if first number is between the other two numbers
int lRange(long test, long limit1, long limit2)
{

	long varMin, varMax;

	varMin = lMin(limit1, limit2);
	varMax = lMax(limit1, limit2);

	if ((varMin <= test) && (test <= varMax))
	{
		return (TRUE);
	}
	else
	{
		return (FALSE);
	}
}

// return the maximum of two floating point numbers
float fMax(float fvar1, float fvar2)
{
	if (fvar1 >= fvar2)
	{
		return (fvar1);
	}
	else
	{
		return (fvar2);
	}
}

// return the minimum of two floating point numbers
float fMin(float fvar1, float fvar2)
{
	if (fvar1 <= fvar2)
	{
		return (fvar1);
	}
	else
	{
		return (fvar2);
	}
}

// return True if first number is between the other two numbers
int fRange(float test, float limit1, float limit2)
{

	float varMin, varMax;

	varMin = fMin(limit1, limit2);
	varMax = fMax(limit1, limit2);

	if ((varMin <= test) && (test <= varMax))
	{
		return (TRUE);
	}
	else
	{
		return (FALSE);
	}
}

// returns a string conversion of an floating point value with 2 decimal points
// and added range check
char *ftoaDp2(volatile float fnumber)
{

	static volatile char str[15], newStr[15];
	volatile int end, i, pNewStr;

	ltoa((long)((fnumber + 0.005) * 100.0), (char *)str);
	end = strlen((char *)str);
	switch (end)
	{
	case 0:
		strcpy((char *)newStr, "0.00");
		break;
	case 1:
		strcpy((char *)newStr, "0.0");
		newStr[3] = str[0];
		newStr[4] = NULL;
		break;
	case 2:
		strcpy((char *)newStr, "0.");
		newStr[2] = str[0];
		newStr[3] = str[1];
		newStr[4] = NULL;
		break;
	default:
		pNewStr = 0;
		for (i = 0; i <= end;)
		{
			newStr[pNewStr++] = str[i++];
			if ((end - i) == 2)
			{
				newStr[pNewStr++] = '.';
			}
		}
		newStr[pNewStr] = NULL;
		break;
	}
	return ((char *)&newStr[0]);
}

// returns a string conversion of an floating point value with 3 decimal points
// and added range check, still working on this
char *ftoaDp3(volatile float fnumber)
{

	static volatile char str[15], newStr[15];
	volatile int end, i, pNewStr;
	volatile long longTemp;

	ltoa((long)((fnumber + 0.005) * 1000.0), (char *)str);

	end = strlen((char *)str);
	for (i = end; i >= 0; i--)
	{
	}
	longTemp = (long)fnumber * 1000;
	fnumber = (float)longTemp * 0.001;
	//ftoa(str, fnumber);
	return ((char *)&newStr[0]);
}

int gpio(int gpioNum, int state, gpioT dir)
{
	int status;

	status = TRUE;
	if (dir == GPIO_OUT)
	{ // gpio output
		if (iRange(state, 0, 1))
		{ // only on or off
			if (iRange(gpioNum, 0, 31))
			{
				if (state == 1)
				{ // turn the sucker on
					GpioDataRegs.GPASET.all = gpioMap[gpioNum];
				}
				else
				{
					GpioDataRegs.GPACLEAR.all = gpioMap[gpioNum];
				}
			}
			else if (iRange(gpioNum, 32, 63))
			{
				if (state == 1)
				{ // turn the sucker on
					GpioDataRegs.GPBSET.all = gpioMap[gpioNum];
				}
				else
				{
					GpioDataRegs.GPBCLEAR.all = gpioMap[gpioNum];
				}
			}
		}
	}
	else
	{ // gpio input
		if (iRange(gpioNum, 0, 31))
		{
			if (GpioDataRegs.GPADAT.all & gpioMap[gpioNum])
			{				// check bit for that pin
				status = 1; // if true, it's a one
			}
			else
			{
				status = 0; // ziltch
			}
		}
		else if (iRange(gpioNum, 32, 63))
		{
			if (GpioDataRegs.GPBSET.all & gpioMap[gpioNum])
			{
				status = 1;
			}
			else
			{
				status = 0;
			}
		}
	}
	return status;
}

int gpioSetup(gpioT mux, gpioT dir)
{
	int status;

	// Clear Remark 'parameters "mux" and "dir" were never referenced'
	gpioT mux2;
	gpioT dir2;
	mux2 = mux;
	mux = mux2; // clear warning 'mux2 set but not used'
	dir2 = dir;
	dir = dir2; // ''
	// end Clear

	//	TODO:Fix gpioSetup, don't use, need to map Mux since registers use 2 bits for every port
	//eallow;
	status = TRUE;
	/*
	if (mux == GPIO_MUX) {								// gpio alternate function
		if (iRange(gpioNum, 0, 15)) {
			GpioDataRegs.GPAMUX1.all = gpioMap[gpioNum];
		} else if (iRange(gpioNum, 16, 31)) {
			GpioDataRegs.GPAMUX2.all = gpioMap[gpioNum];
		} else if (iRange(gpioNum, 32, 44)) {
			GpioDataRegs.GPBMUX1.all = gpioMap[gpioNum];
		} else if (iRange(gpioNum, 50, 58)) {
			GpioDataRegs.GPBMUX2.all = gpioMap[gpioNum];
		}
	} else if (mux == GPIO_NO_MUX) {								// gpio alternate function
		if (iRange(gpioNum, 0, 15)) {
			GpioDataRegs.GPAMUX1.all &= ~gpioMap[gpioNum];
		} else if (iRange(gpioNum, 16, 31)) {
			GpioDataRegs.GPAMUX2.all &= ~gpioMap[gpioNum];
		} else if (iRange(gpioNum, 32, 44)) {
			GpioDataRegs.GPBMUX1.all &= ~gpioMap[gpioNum];
		} else if (iRange(gpioNum, 50, 58)) {
			GpioDataRegs.GPBMUX2.all &= ~gpioMap[gpioNum];
		}
	}
	if (dir == GPIO_OUT) {								// gpio output
		if (iRange(state, 0, 1)) {						// only on or off
			if (iRange(gpioNum, 0, 31)) {
				if (state == 1) {						// turn the sucker on
					GpioDataRegs.GPASET.all = gpioMap[gpioNum];
				} else {
					GpioDataRegs.GPACLEAR.all = gpioMap[gpioNum];
				}
			} else if (iRange(gpioNum, 32, 63)) {
				if (state == 1) {						// turn the sucker on
					GpioDataRegs.GPBSET.all = gpioMap[gpioNum];
				} else {
					GpioDataRegs.GPBCLEAR.all = gpioMap[gpioNum];
				}
			}
		}
	} else {										// gpio input
		if (iRange(gpioNum, 0, 31)) {
			if (GpioDataRegs.GPADAT.all & gpioMap[gpioNum]) {	// check bit for that pin
				status = 1;										// if true, it's a one
			} else {
				status = 0;										// ziltch
			}
		} else if (iRange(gpioNum, 32, 63)) {
			if (GpioDataRegs.GPBSET.all & gpioMap[gpioNum]) {	
				status = 1;
			} else {
				status = 0;
			}
		}
	}
	*/
	//edis;
	return status;
}

// same as tab() but uses periods instead of spaces, mostly used to dump Event info to serial
void tabWPeriods(int tabTo, char *str)
{
	int pad, lastChar;

	lastChar = strlen(str);
	pad = tabTo - lastChar; // How many spaces to add
	if (pad > 0)
	{ // only add, if needed
		pad += lastChar;
		for (; lastChar < pad; lastChar++)
		{							  // variable already set to start
			str[lastChar] = '.';	  // period
			str[lastChar + 1] = NULL; // string termination
		}
	}
}

char *convertMSecToMinutesSeconds (uint32_t msec, char *buf)
{
	uint32_t min = msec / 60000;
    msec = msec - (60000 * min);
	uint32_t sec = msec / 1000;
	sprintf(buf,"%lu:%02lu", min, sec);
	return buf;
}

