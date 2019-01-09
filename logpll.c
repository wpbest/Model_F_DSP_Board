#include "system_config.h"   // Include file name for configuration
#include SYSTEM_CONFIG       // Configuration header file
#include "timer.h"
#include "log.h"
#include "logpll.h"

EnhancedPLL enhancedPLLs[INVERTER_UPDATES_PER_CYCLE*CAPTURE_CYCLES];
uint16_t countEnhancedPLL = INVERTER_UPDATES_PER_CYCLE*CAPTURE_CYCLES;
extern real_T zeroCrossDiffMsec;

void logpll()
{
    static timeT logTimer;
    static int logStart = TRUE;

    if (logStart == TRUE) // first pass
    {
        logStart = FALSE;     // reset
        logTimer = getTime(); // set timer
    }

    if (timer(logTimer, 500))
    {
        logTimer = getTime();

#if ((defined VERBOSE_DEBUG) && (defined VERBOSE_DEBUG_SYNC))
        if ( (upsBoss.verboseDebug == TRUE) || (VERBOSE_DEBUG_PORT == UART_PORT1) )
        {
            if ( ( upsBoss.upsComMode == UPS_COM_USER ) || ( VERBOSE_DEBUG_PORT == UART_PORT1 ) )
            {
                // real_T PLL_Vin;                        /* '<Root>/PLLVin' */
                // real_T DCFilter;                       /* '<S11>/sum1' */
                // real_T Vin;                            /* '<S1>/Sum' */
                // real_T Theta_i;                        /* '<S1>/Delay' */
                // real_T Vsync;                          /* '<S1>/Fcn1' */
                // real_T Fsync;                          /* '<S1>/Sum1' */
                // real_T Fnow;                           /* '<S1>/Slew_Rate' */
                // real_T Ftarget;                        /* '<S1>/State_Machine' */
                // real_T Theta_t;                        /* '<S1>/Integrator' */
                // real_T setFrq;                         /* '<S1>/Derivitive' */
                // real_T Theta_o;                        /* '<S1>/Angle' */
                // real_T Theta_Error;                    /* '<S1>/Angle' */
                // boolean_T Sync;                        /* '<S1>/AND' */
                // boolean_T FInRng;                      /* '<S1>/State_Machine' */                

                log_trace("PLL_Vin=%f Fnow=%f Ftarget=%f setFrq=%f Fsync=%f Sync=%d FInRng=%d Theta_i=%f Theta_o=%f Theta_t=%f zeroCrossDiffMsec=%f",
                           PLL_Vin,   Fnow,   Ftarget,   setFrq,   Fsync,   Sync,   FInRng,   Theta_i,   Theta_o,   Theta_t,   zeroCrossDiffMsec);            
            }
        }
#endif // ((defined VERBOSE_DEBUG) && (defined VERBOSE_DEBUG_SYNC))
    }    
}

void storeEnhancePLL (EnhancedPLL enhancedPLL)
{
    static int index = 0;

    enhancedPLLs[index] = enhancedPLL;

    index++;

    if (index >= INVERTER_UPDATES_PER_CYCLE*CAPTURE_CYCLES) index = 0;
}

void logEnhancedPLL ()
{
    char output[275];
    int i = 0;
    static int startIndex = -5;
    static int endIndex = 0;

    startIndex += 5;
    endIndex = startIndex + 5;

    if (endIndex > INVERTER_UPDATES_PER_CYCLE*CAPTURE_CYCLES) endIndex = INVERTER_UPDATES_PER_CYCLE;  

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

    for (i=startIndex; i<endIndex; i++)
    {
        sprintf
        (output,
         "i=%d PLL_Vin=%f Vout=%f Vin=%f Theta_i=%f Vsync=%f Fsync=%f Fnow=%f Ftarget=%f setFrq=%f Theta_t=%f Theta_o=%f Theta_Error=%f Sync=%d FInRng=%d sampleTime=%.10e\n\r",    
          i, enhancedPLLs[i].PLL_Vin, enhancedPLLs[i].Vout, enhancedPLLs[i].Vin, enhancedPLLs[i].Theta_i, enhancedPLLs[i].Vsync, enhancedPLLs[i].Fsync, enhancedPLLs[i].Fnow, enhancedPLLs[i].Ftarget,  enhancedPLLs[i].setFrq, enhancedPLLs[i].Theta_t, enhancedPLLs[i].Theta_o, enhancedPLLs[i].Theta_Error, enhancedPLLs[i].Sync, enhancedPLLs[i].FInRng, enhancedPLLs[i].sampleTime);
        usart_putstr(output, VERBOSE_DEBUG_PORT);        
    }

    if (startIndex + 5 >= INVERTER_UPDATES_PER_CYCLE*CAPTURE_CYCLES)
    {
        startIndex = -5;
        countEnhancedPLL = 0;
    }
}
