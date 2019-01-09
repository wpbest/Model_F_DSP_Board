/*
 * File: Enhanced_PLL.c
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

#include "Enhanced_PLL.h"
#include "Enhanced_PLL_private.h"
#include "system_config.h" // Includes file name for configuration
#include SYSTEM_CONFIG	 // Configuration header file
#include "event_controller.h"

extern event_controller_data_t eventData, *pEventData;
extern real_T zeroCrossDiffMsec;
extern uint8_t enhancedPLLInitialized;

/* Exported block signals */
real_T PLL_Vin = 0.0;                        /* '<Root>/PLLVin' */
real_T DCFilter = 0.0;                       /* '<S11>/sum1' */
real_T Vin = 0.0;                            /* '<S1>/Sum' */
real_T Theta_i = 0.0;                        /* '<S1>/Delay' */
real_T Vsync = 0.0;                          /* '<S1>/Fcn1' */
real_T Fsync = 0.0;                          /* '<S1>/Sum1' */
real_T Fnow = 0.0;                           /* '<S1>/Slew_Rate' */
real_T Ftarget = 0.0;                        /* '<S1>/State_Machine' */
real_T Theta_t = 0.0;                        /* '<S1>/Integrator' */
real_T setFrq = 0.0;                         /* '<S1>/Derivitive' */
real_T Theta_Error = 0.0;                    /* '<S1>/Angle' */
real_T Theta_o = 0.0;                        /* '<S1>/Angle' */
boolean_T Sync = 0;                        /* '<S1>/AND' */
boolean_T FInRng = 0;                      /* '<S1>/State_Machine' */

/* Exported block parameters */
//wpb
//real_T Fhyst = 0.02;                   /* Variable: Fhyst
real_T Fhyst = 0.03;                   /* Variable: Fhyst
                                        * Referenced by: '<S1>/State_Machine'
                                        */
real_T Fnom = 60.0;                    /* Variable: Fnom
                                        * Referenced by:
                                        *   '<S1>/Derivitive'
                                        *   '<S1>/PID'
                                        *   '<S1>/State_Machine'
                                        *   '<S1>/F_ff'
                                        *   '<S1>/Slew_Rate'
                                        */
//WPB                                       
//real_T Ftol = 3.0;                     /* Variable: Ftol
real_T Ftol = 1.5;         /* Variable: Ftol
                                        * Referenced by:
                                        *   '<S1>/Derivitive'
                                        *   '<S1>/State_Machine'
                                        */

/* Block states (auto storage) */
DW rtDW;

/* External inputs (root inport signals with auto storage) */
ExtU rtU;

/* Real-time model */
RT_MODEL rtM_;
RT_MODEL *const rtM = &rtM_;

/* Model step function */
void Enhanced_PLL_step(void)
{
  real_T Teta_err;
  real_T rtb_Sum3;
  real_T rtb_Product;
  real_T Fmin_tmp;

  /* Sum: '<S11>/sum1' incorporates:
   *  Gain: '<S11>/C'
   *  Gain: '<S11>/D'
   *  Inport: '<Root>/PLLVin'
   *  UnitDelay: '<S11>/Delay_x'
   */
  DCFilter = 0.00012998310292885741 * PLL_Vin + 0.00012998310219661924 *
    rtDW.Delay_x_DSTATE;

  /* Sum: '<S1>/Sum' incorporates:
   *  Inport: '<Root>/PLLVin'
   */
  Vin = PLL_Vin - DCFilter;

  /* Delay: '<S1>/Delay' */
  Theta_i = rtDW.Delay_DSTATE;

  /* Fcn: '<S1>/Fcn1' */
  Vsync = 82.0 * sin(Theta_i);

  /* Sum: '<S1>/Sum3' incorporates:
   *  DiscreteIntegrator: '<S1>/Vintegrator'
   *  Product: '<S1>/Product2'
   */
  rtb_Sum3 = Vin - rtDW.Vintegrator_DSTATE * Vsync;

  /* Product: '<S1>/Product' incorporates:
   *  Fcn: '<S1>/Fcn'
   */
  rtb_Product = 82.0 * cos(Theta_i) * rtb_Sum3;

  /* MATLAB Function: '<S1>/PID' */
  rtDW.outputOld = ((rtb_Product - rtDW.ErrOld) * 0.02 + rtDW.outputOld) +
    0.0013 * rtDW.ErrOld;
  if (rtDW.outputOld >= Fnom) {
    rtDW.outputOld = Fnom;
  } else {
    if (rtDW.outputOld < -Fnom) {
      rtDW.outputOld = -Fnom;
    }
  }

  rtDW.ErrOld = rtb_Product;

  /* Sum: '<S1>/Sum1' incorporates:
   *  Constant: '<S1>/F_ff'
   *  MATLAB Function: '<S1>/PID'
   */
  Fsync = Fnom + rtDW.outputOld;

  /* Sum: '<S12>/sum1' incorporates:
   *  Gain: '<S12>/C'
   *  Gain: '<S12>/D'
   *  UnitDelay: '<S12>/Delay_x'
   */
  //WPB
  Ftarget = 0.05 + 0.00012998310292885741 * Fsync + 0.00012998310219661924 *
  // Ftarget = 0.035 + 6.4995775366136934E-5 * Fsync + 0.0001299915505492024 *

    rtDW.Delay_x_DSTATE_l;

  /* MATLAB Function: '<S1>/State_Machine' incorporates:
   *  MATLAB Function: '<S1>/Derivitive'
   */
  
  rtb_Product = Fnom + Ftol;
  Fmin_tmp = Fnom - Ftol;
//WPB
//  if ((Ftarget > rtb_Product + Fhyst) || (Ftarget < Fmin_tmp - Fhyst)) {
  if ((Ftarget > rtb_Product + 0.01) || (Ftarget < Fmin_tmp - 0.01)) {
    rtDW.bitsForTID0.FIR = false;
  } else {
//    if ((Ftarget > Fmin_tmp + Fhyst) || (Ftarget < rtb_Product - Fhyst)) {
    if ((Ftarget > Fmin_tmp + 0.01) || (Ftarget < rtb_Product - 0.01)) {
      rtDW.bitsForTID0.FIR = true;
    }
  }

  if (!rtDW.bitsForTID0.FIR) {
    Ftarget = Fnom;
  }

  FInRng = rtDW.bitsForTID0.FIR;

  /* End of MATLAB Function: '<S1>/State_Machine' */

  /* RateLimiter: '<S1>/Slew_Rate' */
  Teta_err = Ftarget - rtDW.PrevY;
  //WPB
  if (Teta_err > 6.5E-5) {
    Fnow = rtDW.PrevY + 6.5E-5;
  } else if (Teta_err < -6.5E-5) {
    Fnow = rtDW.PrevY + -6.5E-5;
  } else {
    Fnow = Ftarget;
  }
  // Too fast can't stay in the frequency range
  //if (Teta_err > 0.5E-5) {
  //  Fnow = rtDW.PrevY + 0.5E-5;
  //} else if (Teta_err < -0.5E-5) {
  //  Fnow = rtDW.PrevY + -0.5E-5;
  //} else {
  //  Fnow = Ftarget;
  //}

  rtDW.PrevY = Fnow;

  /* End of RateLimiter: '<S1>/Slew_Rate' */

  /* Logic: '<S1>/AND' incorporates:
   *  Abs: '<S1>/Abs'
   *  Constant: '<S3>/Constant'
   *  RelationalOperator: '<S3>/Compare'
   *  Sum: '<S1>/Sum2'
   */
  Sync = ((fabs(Ftarget - Fnow) < 0.1) && FInRng);

  /* MATLAB Function: '<S1>/Integrator' incorporates:
   *  Constant: '<S1>/CPUTimer1PLL'
   */
  Teta_err = 0.00013;
  if (!rtDW.bitsForTID0.Theta_not_empty) {
    rtDW.bitsForTID0.Theta_not_empty = true;
    Teta_err = 0.0;
  }

  rtDW.Theta_n += 6.28318530718 * Fnow * Teta_err;
  if (rtDW.Theta_n >= 6.28318530718) {
    rtDW.Theta_n -= 6.28318530718;
  } else {
    if (rtDW.Theta_n < 0.0) {
      rtDW.Theta_n += 6.28318530718;
    }
  }

  Theta_t = rtDW.Theta_n;

  /* End of MATLAB Function: '<S1>/Integrator' */

  /* MATLAB Function: '<S1>/Angle' incorporates:
   *  Inport: '<Root>/zeroCrossDiffMsec'
   */
  //WPB
#define R_FILTER_1 (5.0e3)   // RN1C and RN1D paralleled, 10k values, so half
#define R_FILTER_2 (4990.0)  // R260 value on the Model F Board
#define C_FILTER_1 (0.1e-6)  // C185 value on the Model F Board
#define C_FILTER_2 (47.0e-9) // C183 value on the Model F Board  
  const real_T rcFilterOne = (2.0 * 3.141592654 * R_FILTER_1 * C_FILTER_1);     // Part of Phase shift calculations based on Model F PWM RC circuit
  const real_T rcFilterTwo = (2.0 * 3.141592654 * R_FILTER_2 * C_FILTER_2);     // Part of Phase shift calculations based on Model F PWM RC circuit
  const real_T rad2degTime = (1000.0 * 180.0 / 3.141592654 / 360.0);            // Convert radians to frequency based in degrees value and scale to milliseconds

  real_T deltaAdjust = zeroCrossDiffMsec;
  deltaAdjust += (atan (rcFilterOne * setFrq) * rad2degTime / setFrq);  // Adjust timing of Phase by first  RC Filter on Model F board
	deltaAdjust += (atan (rcFilterTwo * setFrq) * rad2degTime / setFrq);  // Adjust timing of Phase by second RC Filter on Model F board
  // WPB
  //rtU.zeroCrossDiffMsec = deltaAdjust;
  rtU.zeroCrossDiffMsec = deltaAdjust - 0.3;

  if (FInRng) {
    if (Sync) {
//WPB      
//      rtDW.Theta_c += 1.2999999999999998 * rtU.zeroCrossDiffMsec;
      //FF
      if (rtU.zeroCrossDiffMsec > 16.6)
      rtU.zeroCrossDiffMsec = 16.6;
      else if (rtU.zeroCrossDiffMsec < -16.6)
        rtU.zeroCrossDiffMsec = -16.6;

      rtDW.Theta_c += 0.00015 * rtU.zeroCrossDiffMsec;
      if (rtDW.Theta_c > 3.14159265359) {
        rtDW.Theta_c -= 6.28318530718;
      } else {
        if (rtDW.Theta_c < -3.14159265359) {
          rtDW.Theta_c += 6.28318530718;
        }
      }
//WPB
      Theta_o = rtDW.Theta_c + Theta_t;
      //Theta_o = rtDW.Theta_c + Theta_t + 0.2262;
    } else {
      Theta_o = rtDW.Theta_c + Theta_t;
    }
  } else {
    Theta_o = rtDW.Theta_c + Theta_t;
  }

  //FF
  if (Theta_o >= 2*6.28318530718)
    Theta_o = 2*6.28318530718;
  else if (Theta_o < -2*6.28318530718)
    Theta_o = -2*6.28318530718;

  if (Theta_o >= 6.28318530718) {
    Theta_o -= 6.28318530718;
  } else {
    if (Theta_o < 0.0) {
      Theta_o += 6.28318530718;
    }
  }

  Theta_Error = rtU.zeroCrossDiffMsec;

  /* End of MATLAB Function: '<S1>/Angle' */

  /* MATLAB Function: '<S1>/Derivitive' incorporates:
   *  Constant: '<S1>/CPUTimer1PLL'
   */
  if (!rtDW.bitsForTID0.Theta_not_empty_i) {
    rtDW.bitsForTID0.Theta_not_empty_i = true;
    rtDW.Fold = Fnom;
  }

  Teta_err = Theta_o - rtDW.Theta_k;
  if (Teta_err < 0.0) {
    Teta_err += 6.28318530718;
  }

  setFrq = 0.15915494309188485 * Teta_err / 0.00013;
  if ((setFrq < 0.0) || (setFrq > rtb_Product) || (setFrq < Fmin_tmp)) {
    setFrq = rtDW.Fold;
  }

  rtDW.Fold = setFrq;
  rtDW.Theta_k = Theta_o;

  /* MATLAB Function: '<S1>/Integrator1' incorporates:
   *  Constant: '<S1>/CPUTimer1PLL'
   *  Delay: '<S1>/Delay'
   */
  rtDW.Theta += 6.28318530718 * Fsync * 0.00013;
  if (rtDW.Theta >= 6.28318530718) {
    rtDW.Theta -= 6.28318530718;
  } else {
    if (rtDW.Theta < 0.0) {
      rtDW.Theta += 6.28318530718;
    }
  }

  rtDW.Delay_DSTATE = rtDW.Theta;

  /* End of MATLAB Function: '<S1>/Integrator1' */

  /* Sum: '<S12>/A*x(k) + B*u(k)' incorporates:
   *  Gain: '<S12>/A'
   *  Gain: '<S12>/B'
   *  UnitDelay: '<S12>/Delay_x'
   */
  // WPB
   rtDW.Delay_x_DSTATE_l = 0.99974003379414234 * rtDW.Delay_x_DSTATE_l +
   1.9997400450593448 * Fsync;

   //rtDW.Delay_x_DSTATE_l = 1.0005 * (0.99974003379414234 * rtDW.Delay_x_DSTATE_l +
   //1.9997400450593448 * Fsync);

  /* Sum: '<S11>/A*x(k) + B*u(k)' incorporates:
   *  Gain: '<S11>/A'
   *  Gain: '<S11>/B'
   *  Inport: '<Root>/PLLVin'
   *  UnitDelay: '<S11>/Delay_x'
   */
  rtDW.Delay_x_DSTATE = 0.99974003379414234 * rtDW.Delay_x_DSTATE +
    1.9997400450593448 * PLL_Vin;

  /* Update for DiscreteIntegrator: '<S1>/Vintegrator' incorporates:
   *  Product: '<S1>/Product1'
   */
  rtDW.Vintegrator_DSTATE += rtb_Sum3 * Vsync * 2.5999999999999997E-6;
}

/* Model initialize function */
void Enhanced_PLL_initialize(void)
{
  /* Registration code */

  /* initialize error status */
  rtmSetErrorStatus(rtM, (NULL));

  /* block I/O */

  /* exported global signals */
  DCFilter = 0.0;
  Vin = 0.0;
  Theta_i = 0.0;
  Vsync = 0.0;
  Fsync = 0.0;
  Fnow = 0.0;
  Ftarget = 60.0;
  Theta_t = 0.0;
  Theta_Error = 0.0;
  Theta_o = 0.0;
  Sync = false;
  FInRng = false;
 
  // WPB
  rtU.zeroCrossDiffMsec = 0.0;

  Fnom = UPS_FREQOUTNOM;


  /* states (dwork) */
  (void) memset((void *)&rtDW, 0,
                sizeof(DW));

  /* InitializeConditions for UnitDelay: '<S11>/Delay_x' */
  rtDW.Delay_x_DSTATE = -40903.076923076929;

  /* InitializeConditions for UnitDelay: '<S12>/Delay_x' */
  rtDW.Delay_x_DSTATE_l = 461538.46153846156;

  /* InitializeConditions for RateLimiter: '<S1>/Slew_Rate' */
  rtDW.PrevY = Fnom;

  /* SystemInitialize for MATLAB Function: '<S1>/PID' */
  rtDW.ErrOld = 0.0;
  rtDW.outputOld = 0.0;

  /* SystemInitialize for MATLAB Function: '<S1>/State_Machine' */
  rtDW.bitsForTID0.FIR = false;

  /* SystemInitialize for MATLAB Function: '<S1>/Integrator' */
  rtDW.bitsForTID0.Theta_not_empty = false;
  rtDW.Theta_n = 0.0;

  /* SystemInitialize for MATLAB Function: '<S1>/Angle' */
  rtDW.Theta_c = 0.0;

  /* SystemInitialize for MATLAB Function: '<S1>/Derivitive' */
  rtDW.bitsForTID0.Theta_not_empty_i = false;
  rtDW.Theta_k = 0.0;

  /* SystemInitialize for MATLAB Function: '<S1>/Integrator1' */
  rtDW.Theta = 0.0;

// WPB
  if (pEventData->systemData.bankSwitches.bit.NARROW_INPUT_FREQ_RANGE)
    Ftol = 1.5;
  else
    Ftol = 3.0;

  Ftol = Ftol + Fhyst;

  enhancedPLLInitialized = TRUE;
}

/* Model terminate function */
void Enhanced_PLL_terminate(void)
{
  /* (no terminate code required) */
}

void Enhanced_PLL_UpdateParameters(void)
{
  if (pEventData->systemData.bankSwitches.bit.NARROW_INPUT_FREQ_RANGE)
    Ftol = 1.5;
  else
    Ftol = 3.0;

   Ftol = Ftol + Fhyst;
}
/*
 * File trailer for generated code.
 *
 * [EOF]
 */
