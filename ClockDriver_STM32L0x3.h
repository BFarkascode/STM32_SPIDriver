/*
 *  Created on: Oct 30, 2023
 *  Project: STM32_SPI_Driver
 *  File: ClockDriver_STM32L0x3.h
 *  Author: BalazsFarkas
 *  Processor: STM32L053R8
 *  Compiler: ARM-GCC (STM32 IDE)
 *  Program version: 1.0
 *  Program description: N/A
 *  Hardware description/pin distribution: N/A
 *  Modified from: N/A
 *  Change history: N/A
 */

#ifndef INC_RCCTIMPWMDELAY_CUSTOM_H_
#define INC_RCCTIMPWMDELAY_CUSTOM_H_

#include "stdint.h"

//LOCAL CONSTANT

//FUNCTION PROTOTYPES
void SysClockConfig(void);
void TIM6Config (void);
void Delay_us(int micro_sec);
void Delay_ms(int milli_sec);

#endif /* RCCTIMPWMDELAY_CUSTOM_H_ */
