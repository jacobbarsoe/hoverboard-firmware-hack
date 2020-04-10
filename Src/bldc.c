/*
* This file implements FOC motor control.
* This control method offers superior performanace
* compared to previous cummutation method. The new method features:
* ► reduced noise and vibrations
* ► smooth torque output
* ► improved motor efficiency -> lower energy consumption
*
* Copyright (C) 2019-2020 Emanuel FERU <aerdronix@gmail.com>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "stm32f1xx_hal.h"
#include "defines.h"
#include "setup.h"
#include "config.h"
#include "SEGGER_RTT.h"

// Matlab includes and defines - from auto-code generation
// ###############################################################################
#include "BLDC_controller.h"           /* Model's header file */
#include "rtwtypes.h"

extern RT_MODEL *const rtM_Left;
extern RT_MODEL *const rtM_Right;

extern DW rtDW_Left;                    /* Observable states */
extern ExtU rtU_Left;                   /* External inputs */
extern ExtY rtY_Left;                   /* External outputs */

extern DW rtDW_Right;                   /* Observable states */
extern ExtU rtU_Right;                  /* External inputs */
extern ExtY rtY_Right;                  /* External outputs */
// ###############################################################################

static int16_t pwm_margin = 100;        /* This margin allows to always have a window in the PWM signal for proper Phase currents measurement */

extern uint8_t ctrlModReq;
static int16_t curDC_max = (I_DC_MAX * A2BIT_CONV);
volatile int16_t curL_phaA = 0, curL_phaB = 0, curL_DC = 0;
volatile int16_t curR_phaB = 0, curR_phaC = 0, curR_DC = 0;
uint8_t errCode_Left = 0;
uint8_t errCode_Right = 0;

volatile int pwml = 0;
volatile int pwmr = 0;

extern volatile adc_buf_t adc_buffer;

extern volatile uint32_t timeout;

uint8_t buzzerFreq          = 0;
uint8_t buzzerPattern       = 0;
static uint32_t buzzerTimer = 0;

uint8_t        enable       = 0;        // initially motors are disabled for SAFETY
static uint8_t enableFin    = 0;

static const uint16_t pwm_res  = 72000000 / 2 / PWM_FREQ; // = 2000

uint16_t offsetcount = 0;
static int16_t offsetrl1    = 2000;
static int16_t offsetrl2    = 2000;
static int16_t offsetrr1    = 2000;
static int16_t offsetrr2    = 2000;
static int16_t offsetdcl    = 2000;
static int16_t offsetdcr    = 2000;

/*int16_t _offsetrl1    = 2000;
int16_t _offsetrl2    = 2000;
int16_t _offsetrr1    = 2000;
int16_t _offsetrr2    = 2000;
int16_t _offsetdcl    = 2000;
int16_t _offsetdcr    = 2000;*/

int16_t        batVoltage       = (400 * BAT_CELLS * BAT_CALIB_ADC) / BAT_CALIB_REAL_VOLTAGE;
static int32_t batVoltageFixdt  = (400 * BAT_CELLS * BAT_CALIB_ADC) / BAT_CALIB_REAL_VOLTAGE << 20;  // Fixed-point filter output initialized at 400 V*100/cell = 4 V/cell converted to fixed-point

boolean_T OverrunFlag = false;

//Reset the offset and reprocess it
uint8_t ResetOffset = 0;

struct RTT_WritePackage{
	//int16_t pha;
	//int16_t phb;
	//int16_t phc;
	uint16_t adc1;
	uint16_t adc2;
};

void InitDMAValues(){
	offsetcount = 0;
	offsetrl1    = 2000;
	offsetrl2    = 2000;
	offsetrr1    = 2000;
	offsetrr2    = 2000;
	offsetdcl    = 2000;
	offsetdcr    = 2000;
	batVoltage       = (400 * BAT_CELLS * BAT_CALIB_ADC) / BAT_CALIB_REAL_VOLTAGE;
	batVoltageFixdt  = (400 * BAT_CELLS * BAT_CALIB_ADC) / BAT_CALIB_REAL_VOLTAGE << 20;  // Fixed-point filter output initialized at 400 V*100/cell = 4 V/cell converted to fixed-point
	enable       = 0;
	enableFin    = 0;
	pwml = 0;
	pwmr = 0;
	adc_buffer.rl1 = 0;
	adc_buffer.rl2 = 0;
	adc_buffer.rr1 = 0;
	adc_buffer.rr2 = 0;
	adc_buffer.dcl = 0;
	adc_buffer.dcr = 0;
}

// =================================
// DMA interrupt frequency =~ 16 kHz
// =================================
void DMA1_Channel1_IRQHandler(void) {

  DMA1->IFCR = DMA_IFCR_CTCIF1;
  // HAL_GPIO_WritePin(LED_PORT, LED_PIN, 1);
  //HAL_GPIO_TogglePin(LED_PORT, LED_PIN);

  struct RTT_WritePackage package = {adc_buffer.rl1, offsetrl1};
  SEGGER_RTT_Write(1, &package, sizeof(struct RTT_WritePackage));
  
  if(offsetcount < OFFSET_COUNT_VALUE) {  // calibrate ADC offsets
    offsetcount++;
    /*offsetrl1 = (adc_buffer.rl1 + offsetrl1) / 2;
    offsetrl2 = (adc_buffer.rl2 + offsetrl2) / 2;
    offsetrr1 = (adc_buffer.rr1 + offsetrr1) / 2;
    offsetrr2 = (adc_buffer.rr2 + offsetrr2) / 2;
    offsetdcl = (adc_buffer.dcl + offsetdcl) / 2;
    offsetdcr = (adc_buffer.dcr + offsetdcr) / 2;*/
	offsetrl1 = (adc_buffer.rl1 + offsetrl1*11) / 12;
    offsetrl2 = (adc_buffer.rl2 + offsetrl2*11) / 12;
    offsetrr1 = (adc_buffer.rr1 + offsetrr1*11) / 12;
    offsetrr2 = (adc_buffer.rr2 + offsetrr2*11) / 12;
    offsetdcl = (adc_buffer.dcl + offsetdcl*11) / 12;
    offsetdcr = (adc_buffer.dcr + offsetdcr*11) / 12;
	if(ResetOffset == 0) return;
  } else if (ResetOffset == 1) {
	  ResetOffset = 255;
  }

  if (buzzerTimer % 1000 == 0) {  // because you get float rounding errors if it would run every time -> not any more, everything converted to fixed-point
    filtLowPass32(adc_buffer.batt1, BAT_FILT_COEF, &batVoltageFixdt);
    batVoltage = (int16_t)(batVoltageFixdt >> 20);  // convert fixed-point to integer
  }

  // Get Left motor currents
  //Flip the phases, experimental
  curL_phaA = (int16_t)(offsetrl1 - adc_buffer.rl1);
  curL_phaB = (int16_t)(offsetrl2 - adc_buffer.rl2);
  curL_DC   = (int16_t)(offsetdcl - adc_buffer.dcl);
  
  // Get Right motor currents
  //on AT32 board the motor phase current mesurement is switched
  curR_phaB = (int16_t)(offsetrr2 - adc_buffer.rr2);
  curR_phaC = (int16_t)(offsetrr1 - adc_buffer.rr1);
  curR_DC   = (int16_t)(offsetdcr - adc_buffer.dcr);

  // Disable PWM when current limit is reached (current chopping)
  // This is the Level 2 of current protection. The Level 1 should kick in first given by I_MOT_MAX
  if(ABS(curL_DC) > curDC_max || timeout > TIMEOUT || enable == 0) {
    LEFT_TIM->BDTR &= ~TIM_BDTR_MOE;
  } else {
    LEFT_TIM->BDTR |= TIM_BDTR_MOE;
  }

  if(ABS(curR_DC)  > curDC_max || timeout > TIMEOUT || enable == 0) {
    RIGHT_TIM->BDTR &= ~TIM_BDTR_MOE;
  } else {
    RIGHT_TIM->BDTR |= TIM_BDTR_MOE;
  }

  //create square wave for buzzer
  buzzerTimer++;
  if (buzzerFreq != 0 && (buzzerTimer / 5000) % (buzzerPattern + 1) == 0) {
    if (buzzerTimer % buzzerFreq == 0) {
      HAL_GPIO_TogglePin(BUZZER_PORT, BUZZER_PIN);
    }
  } else {
      HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
  }

  // ############################### MOTOR CONTROL ###############################

  int ul, vl, wl;
  int ur, vr, wr;
  

  /* Check for overrun */
  if (OverrunFlag) {
    return;
  }
  OverrunFlag = true;

  /* Make sure to stop BOTH motors in case of an error */
  enableFin = enable && !errCode_Left && !errCode_Right;
 
  // ========================= LEFT MOTOR ============================ 
    // Get hall sensors values
    uint8_t hall_ul = !(LEFT_HALL_U_PORT->IDR & LEFT_HALL_U_PIN);
    uint8_t hall_vl = !(LEFT_HALL_V_PORT->IDR & LEFT_HALL_V_PIN);
    uint8_t hall_wl = !(LEFT_HALL_W_PORT->IDR & LEFT_HALL_W_PIN);

    /* Set motor inputs here */
    rtU_Left.b_motEna     = enableFin;
    rtU_Left.z_ctrlModReq = ctrlModReq;  
    rtU_Left.r_inpTgt     = pwml;
    rtU_Left.b_hallA      = hall_ul;
    rtU_Left.b_hallB      = hall_vl;
    rtU_Left.b_hallC      = hall_wl;
    rtU_Left.i_phaAB      = curL_phaA;
    rtU_Left.i_phaBC      = curL_phaB;
    rtU_Left.i_DCLink     = curL_DC;    
    
    /* Step the controller */
    BLDC_controller_step(rtM_Left);

    /* Get motor outputs here */
    ul            = rtY_Left.DC_phaA;
    vl            = rtY_Left.DC_phaB;
    wl            = rtY_Left.DC_phaC;
    errCode_Left  = rtY_Left.z_errCode;
  // motSpeedLeft = rtY_Left.n_mot;
  // motAngleLeft = rtY_Left.a_elecAngle;

    /* Apply commands */
    LEFT_TIM->LEFT_TIM_U    = (uint16_t)CLAMP(ul + pwm_res / 2, pwm_margin, pwm_res-pwm_margin);
    LEFT_TIM->LEFT_TIM_V    = (uint16_t)CLAMP(vl + pwm_res / 2, pwm_margin, pwm_res-pwm_margin);
    LEFT_TIM->LEFT_TIM_W    = (uint16_t)CLAMP(wl + pwm_res / 2, pwm_margin, pwm_res-pwm_margin);
  // =================================================================
  

  // ========================= RIGHT MOTOR ===========================  
    // Get hall sensors values
    uint8_t hall_ur = !(RIGHT_HALL_U_PORT->IDR & RIGHT_HALL_U_PIN);
    uint8_t hall_vr = !(RIGHT_HALL_V_PORT->IDR & RIGHT_HALL_V_PIN);
    uint8_t hall_wr = !(RIGHT_HALL_W_PORT->IDR & RIGHT_HALL_W_PIN);

    /* Set motor inputs here */
    rtU_Right.b_motEna      = enableFin;
    rtU_Right.z_ctrlModReq  = ctrlModReq;
    rtU_Right.r_inpTgt      = pwmr;
    rtU_Right.b_hallA       = hall_ur;
    rtU_Right.b_hallB       = hall_vr;
    rtU_Right.b_hallC       = hall_wr;
    rtU_Right.i_phaAB       = curR_phaB;
    rtU_Right.i_phaBC       = curR_phaC;
    rtU_Right.i_DCLink      = curR_DC;

    /* Step the controller */
    BLDC_controller_step(rtM_Right);

    /* Get motor outputs here */
    ur            = rtY_Right.DC_phaA;
    vr            = rtY_Right.DC_phaB;
    wr            = rtY_Right.DC_phaC;
    errCode_Right = rtY_Right.z_errCode;
 // motSpeedRight = rtY_Right.n_mot;
 // motAngleRight = rtY_Right.a_elecAngle;

    /* Apply commands */
    RIGHT_TIM->RIGHT_TIM_U  = (uint16_t)CLAMP(ur + pwm_res / 2, pwm_margin, pwm_res-pwm_margin);
    RIGHT_TIM->RIGHT_TIM_V  = (uint16_t)CLAMP(vr + pwm_res / 2, pwm_margin, pwm_res-pwm_margin);
    RIGHT_TIM->RIGHT_TIM_W  = (uint16_t)CLAMP(wr + pwm_res / 2, pwm_margin, pwm_res-pwm_margin);
  // =================================================================

  /* Indicate task complete */
  OverrunFlag = false;
  //struct RTT_WritePackage package = {rtY_Left.DC_phaA, rtY_Left.DC_phaB, rtY_Left.DC_phaC};//, adc_buffer.rl1, adc_buffer.rl2};
 // ###############################################################################

}
