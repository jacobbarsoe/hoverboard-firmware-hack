/*
* This file is part of the hoverboard-firmware-hack project.
*
* Copyright (C) 2017-2018 Rene Hopf <renehopf@mac.com>
* Copyright (C) 2017-2018 Nico Stute <crinq@crinq.de>
* Copyright (C) 2017-2018 Niklas Fauth <niklas.fauth@kit.fail>
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

#pragma once

#include "stm32f1xx_hal.h"
#include "config.h"
#include "defines.h"

void MX_GPIO_Init(void);
void MX_TIM_Init(void);
void MX_ADC1_Init(void);
void MX_ADC2_Init(void);
void UART2_Init(void);
void UART3_Init(void);


#ifdef CONTROL_PPM
void PPM_Init();
void PPM_ISR_Callback();
void PPM_SysTick_Callback();
#endif

// Define Beep functions
void longBeep(uint8_t freq);
void shortBeep(uint8_t freq);

// Define additional functions. Implementation is in main.c
void filtLowPass32(int16_t u, uint16_t coef, int32_t *y);
void mixerFcn(int16_t rtu_speed, int16_t rtu_steer, int16_t *rty_speedR, int16_t *rty_speedL);
void rateLimiter16(int16_t u, int16_t rate, int16_t *y);
void multipleTapDet(int16_t u, uint32_t timeNow, MultipleTap *x);


void Nunchuck_Init();
void Nunchuck_Read();
void consoleScope();
void consoleLog(char *message);
void setScopeChannel(uint8_t ch, int16_t val);