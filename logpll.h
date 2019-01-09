#ifndef _logpll
#define _logpll

#include "Enhanced_PLL.h"

// extern real_T PLL_Vin;                 /* '<Root>/PLLVin' */
// extern real_T DCFilter;                /* '<S11>/sum1' */
// extern real_T Vin;                     /* '<S1>/Sum' */
// extern real_T Theta_i;                 /* '<S1>/Delay' */
// extern real_T Vsync;                   /* '<S1>/Fcn1' */
// extern real_T Fsync;                   /* '<S1>/Sum1' */
// extern real_T Fnow;                    /* '<S1>/Slew_Rate' */
// extern real_T Ftarget;                 /* '<S1>/State_Machine' */
// extern real_T Theta_t;                 /* '<S1>/Integrate1' */
// extern real_T setFrq;                  /* '<S1>/Derivitive' */
// extern real_T Theta_o;                 /* '<S1>/Angle' */
// extern real_T Theta_Error;             /* '<S1>/Angle' */
// extern boolean_T Sync;                 /* '<S1>/AND' */
// extern boolean_T FInRng;               /* '<S1>/State_Machine' */

typedef struct _EnhancedPLL
{
    real_T    PLL_Vin;
    real_T    Vout;
    real_T    Vin;                  
    real_T    Theta_i;   
    real_T    Vsync;                 
    real_T    Fsync;              
    real_T    Fnow;      
    real_T    Ftarget;                 
    real_T    setFrq;    
    real_T    Theta_t;       
    real_T    Theta_o;
    real_T    Theta_Error;   
    boolean_T Sync;
    boolean_T FInRng;
    real_T    sampleTime;
} EnhancedPLL;

void logpll ();
void storeEnhancePLL (EnhancedPLL enhancedPLL);
void logEnhancedPLL ();

#endif
