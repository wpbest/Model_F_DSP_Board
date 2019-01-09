/*
 * File: Enhanced_PLL.h
 *
 * Code generated for Simulink model 'Enhanced_PLL'.
 *
 * Model version                  : 1.1414
 * Simulink Coder version         : 8.13 (R2017b) 24-Jul-2017
 * C/C++ source code generated on : Mon Oct 29 13:33:34 2018
 *
 * Target selection: ert.tlc
 * Embedded hardware selection: Texas Instruments->C2000
 * Code generation objectives: Unspecified
 * Validation result: Not run
 */

#ifndef RTW_HEADER_Enhanced_PLL_h_
#define RTW_HEADER_Enhanced_PLL_h_
#include <math.h>
#include <stddef.h>
#include <string.h>
#ifndef Enhanced_PLL_COMMON_INCLUDES_
# define Enhanced_PLL_COMMON_INCLUDES_
#include "rtwtypes.h"
#endif                                 /* Enhanced_PLL_COMMON_INCLUDES_ */

#include "Enhanced_PLL_types.h"

/* Macros for accessing real-time model data structure */
#ifndef rtmGetErrorStatus
# define rtmGetErrorStatus(rtm)        ((rtm)->errorStatus)
#endif

#ifndef rtmSetErrorStatus
# define rtmSetErrorStatus(rtm, val)   ((rtm)->errorStatus = (val))
#endif

/* Block states (auto storage) for system '<Root>' */
typedef struct {
  real_T Delay_x_DSTATE;               /* '<S11>/Delay_x' */
  real_T Vintegrator_DSTATE;           /* '<S1>/Vintegrator' */
  real_T Delay_DSTATE;                 /* '<S1>/Delay' */
  real_T Delay_x_DSTATE_l;             /* '<S12>/Delay_x' */
  real_T PrevY;                        /* '<S1>/Slew_Rate' */
  real_T ErrOld;                       /* '<S1>/PID' */
  real_T outputOld;                    /* '<S1>/PID' */
  real_T Theta;                        /* '<S1>/Integrator1' */
  real_T Theta_n;                      /* '<S1>/Integrator' */
  real_T Theta_k;                      /* '<S1>/Derivitive' */
  real_T Fold;                         /* '<S1>/Derivitive' */
  real_T Theta_c;                      /* '<S1>/Angle' */
  struct {
    uint_T FIR:1;                      /* '<S1>/State_Machine' */
    uint_T Theta_not_empty:1;          /* '<S1>/Integrator' */
    uint_T Theta_not_empty_i:1;        /* '<S1>/Derivitive' */
  } bitsForTID0;
} DW;

/* External inputs (root inport signals with auto storage) */
typedef struct {
  real_T zeroCrossDiffMsec;            /* '<Root>/zeroCrossDiffMsec' */
} ExtU;

/* Real-time Model Data Structure */
struct tag_RTM {
  const char_T * volatile errorStatus;
};

/* Block states (auto storage) */
extern DW rtDW;

/* External inputs (root inport signals with auto storage) */
extern ExtU rtU;

/*
 * Exported Global Signals
 *
 * Note: Exported global signals are block signals with an exported global
 * storage class designation.  Code generation will declare the memory for
 * these signals and export their symbols.
 *
 */
extern real_T PLL_Vin;                 /* '<Root>/PLLVin' */
extern real_T DCFilter;                /* '<S11>/sum1' */
extern real_T Vin;                     /* '<S1>/Sum' */
extern real_T Theta_i;                 /* '<S1>/Delay' */
extern real_T Vsync;                   /* '<S1>/Fcn1' */
extern real_T Fsync;                   /* '<S1>/Sum1' */
extern real_T Fnow;                    /* '<S1>/Slew_Rate' */
extern real_T Ftarget;                 /* '<S1>/State_Machine' */
extern real_T Theta_t;                 /* '<S1>/Integrator' */
extern real_T setFrq;                  /* '<S1>/Derivitive' */
extern real_T Theta_Error;             /* '<S1>/Angle' */
extern real_T Theta_o;                 /* '<S1>/Angle' */
extern boolean_T Sync;                 /* '<S1>/AND' */
extern boolean_T FInRng;               /* '<S1>/State_Machine' */

/*
 * Exported Global Parameters
 *
 * Note: Exported global parameters are tunable parameters with an exported
 * global storage class designation.  Code generation will declare the memory for
 * these parameters and exports their symbols.
 *
 */
extern real_T Fhyst;                   /* Variable: Fhyst
                                        * Referenced by: '<S1>/State_Machine'
                                        */
extern real_T Fnom;                    /* Variable: Fnom
                                        * Referenced by:
                                        *   '<S1>/Derivitive'
                                        *   '<S1>/PID'
                                        *   '<S1>/State_Machine'
                                        *   '<S1>/F_ff'
                                        *   '<S1>/Slew_Rate'
                                        */
extern real_T Ftol;                    /* Variable: Ftol
                                        * Referenced by:
                                        *   '<S1>/Derivitive'
                                        *   '<S1>/State_Machine'
                                        */

/* Model entry point functions */
extern void Enhanced_PLL_initialize(void);
extern void Enhanced_PLL_step(void);
extern void Enhanced_PLL_terminate(void);
extern void Enhanced_PLL_UpdateParameters(void);

/* Real-time Model object */
extern RT_MODEL *const rtM;

/*-
 * The generated code includes comments that allow you to trace directly
 * back to the appropriate location in the model.  The basic format
 * is <system>/block_name, where system is the system number (uniquely
 * assigned by Simulink) and block_name is the name of the block.
 *
 * Note that this particular code originates from a subsystem build,
 * and has its own system numbers different from the parent model.
 * Refer to the system hierarchy for this subsystem below, and use the
 * MATLAB hilite_system command to trace the generated code back
 * to the parent model.  For example,
 *
 * hilite_system('PLL_20181029V1/Enhanced_PLL')    - opens subsystem PLL_20181029V1/Enhanced_PLL
 * hilite_system('PLL_20181029V1/Enhanced_PLL/Kp') - opens and selects block Kp
 *
 * Here is the system hierarchy for this model
 *
 * '<Root>' : 'PLL_20181029V1'
 * '<S1>'   : 'PLL_20181029V1/Enhanced_PLL'
 * '<S2>'   : 'PLL_20181029V1/Enhanced_PLL/Angle'
 * '<S3>'   : 'PLL_20181029V1/Enhanced_PLL/Compare'
 * '<S4>'   : 'PLL_20181029V1/Enhanced_PLL/DCFilter'
 * '<S5>'   : 'PLL_20181029V1/Enhanced_PLL/Derivitive'
 * '<S6>'   : 'PLL_20181029V1/Enhanced_PLL/FsyncLPF'
 * '<S7>'   : 'PLL_20181029V1/Enhanced_PLL/Integrator'
 * '<S8>'   : 'PLL_20181029V1/Enhanced_PLL/Integrator1'
 * '<S9>'   : 'PLL_20181029V1/Enhanced_PLL/PID'
 * '<S10>'  : 'PLL_20181029V1/Enhanced_PLL/State_Machine'
 * '<S11>'  : 'PLL_20181029V1/Enhanced_PLL/DCFilter/Model'
 * '<S12>'  : 'PLL_20181029V1/Enhanced_PLL/FsyncLPF/Model'
 */
#endif                                 /* RTW_HEADER_Enhanced_PLL_h_ */

/*
 * File trailer for generated code.
 *
 * [EOF]
 */
