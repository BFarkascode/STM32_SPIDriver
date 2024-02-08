/*
 *  Created on: Nov 2, 2023
 *  Author: BalazsFarkas
 *  Project: STM32_SPIDriver
 *  Processor: STM32L053R8
 *  HEader version: 1.0
 *  File: SPIDriver_STM32L0x3.h
 */

#ifndef INC_SPIDRIVER_CUSTOM_H_
#define INC_SPIDRIVER_CUSTOM_H_

#include "stdint.h"
#include "stm32l053xx.h"											//device specific header file for registers
#include "ClockDriver_STM32L0x3.h"									//custom clocking for the core and the peripherals

//EXTERNAL VARIABLE

//FUNCTION PROTOTYPES
void SPI1MasterInit (GPIO_TypeDef* gpio_port_SPI, uint8_t gpio_pin_number_SPI);
void SPI1MasterWrite (uint8_t reg_addr_write_to, uint8_t *bytes_to_send, uint8_t number_of_bytes, GPIO_TypeDef* gpio_port_SPI, uint8_t gpio_number);
void SPI1MasterRead (uint8_t reg_addr_to_read_from, uint8_t *bytes_received, uint8_t number_of_bytes, GPIO_TypeDef* gpio_port_SPI, uint8_t gpio_number);

#endif /* INC_SPIDRIVER_CUSTOM_H_ */
