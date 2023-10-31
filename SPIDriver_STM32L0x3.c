/*
 * SPIDriver_STM32L0x3.c			v.1.0.
 *
 *  Created on: Nov 2, 2023
 *      Author: Balazs Farkas
 *
 *
 * v.1.0
 * Below is a custom SPI driver.
 * No external triggers, no DMA, no interrupts.
 * The driver currently generates only the master side of the driver.
 *
 *
 * SPI commands should be constructed as an array with the command (W/R) and the register to manipulate placed on [0] and the rest as the data to be written for W and 0x0 for R.
 * This "command" array can be used as the buffer for the command and the reply at the same time.
 *
 * Example:
 *    SPI1MasterInit_Custom();
 *    uint8_t reset_sensor[2] = {0x60, 0xB6};
 *    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET);
 *    HAL_Delay(1);															//necessary delay for the slave to react to the CS pin
 *    SPI1MasterWriteReg_Custom(reset_sensor[0], &reset_sensor[1], 1);
 *    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_SET);
 *
 *
 *  v.1.1.
 *  Added CS control for the slave driver - choose port and pin.
 *  Rearranged turn off sequence to disable slave before disabling master.
 *  The rearranged turn off sequence also deals with 50 ns SDO output disable time.
 *  Added redefinition of "MODER" to avoid any issues with reset value on CS pin.
 *  Added DMA callback for SPI.
 *
 * Example:
 *    SPI1MasterInit_Custom(GPIOA, 4);										//sets PA4 as the CS pin
 *
 *  The code above will initialize the SPI1 and thenwrite the reset_sensor array to the slave with [0] as address and [1] as the data.
 *  Here, the slave device is a BMP280 which has [7] as the W/R bit and [6:0] as the 7-bit register address. This is not the same when BMP280 is run using I2C, registers will be different!
 *
 *
 */

#include "SPIDriver_STM32L0x3.h"

//1) Initialise the SPI driver - master mode

void SPI1MasterInit (GPIO_TypeDef* gpio_port_SPI, uint8_t gpio_pin_number_SPI) {
	/*
	 * What are we doing here?
	 * We enable the SPI1 (on APB2). We pick the control mechanics for having the master controller here (software control, with the internal SS activated). We don't care about SSI control activating the bus unnecessarily, we have only one slave on the bus.
	 * We set the GPIOs (including the external CS, for which we take the port and the pin number as the two inputs for this function).
	 * We also pick the Motorola-style SPI, which should be the reset value anyway. Same with SPIMODE0 and 8-bit frames.
	 *
	 * Below we initialise the SPI peripheral. We are using SPI1 for this driver. We driver the master.
	 * This means the following:
	 * 1)Choosing which SPI peripheral we are using and then selecting the way of control for the CS pin. We assign the CS pin if SSM is picked.
	 * 2)Assigning the pins and setting them up properly
	 * 3)Configure SPI: selecting the SPI mode and duplex/simplex, plus if we are master or slave, baud rate
	 * 4)Activate interrupts and DMA (if any)
	 * Note: since SPE drives the data transmission, we don't enable the SPI in the init.
	 * Note: we are in 8-bit mode, SPIOMODE0 with a prescaler of 4. Adjust according to what slave device demands!
	 *
	 * */

	//1) Enable clocking, choose CS control and set up slave CS control

	RCC->APB2ENR |= (1<<12);												//we enable SPI1 clocking
																			//Note: SPI1 runs on the APB2 clock
	SPI1->CR1 |= (1<<9);													//we have SSM enabled. This means that we are controlling the master's CS pin internal to the driver, using the SSI bit and the CS for the slave with a non-specific GPIO.
																			//we can connect the master and the slave side CS, albeit this can only be done at one particular pin - SPI1 NSS
																			//for the time being, we stick with internal CS control for the master and flexible CS control for slave (meaning we can have CS to the slave wherever we want)

	SPI1->CR1 |= (1<<8);													//we set the SSI pin for the SSM to work
																			//Note: resetting this bit pushes the SPI into "mode fault" which generates an interrupt. This interrupt forces the setup to slave mode. SSI should be left HIGH all the time.

	//2) Assign SPI-specific pins
	RCC->IOPENR |= (1<<0);													//PORTA clocking allowed
	RCC->IOPENR |= (1<<1);													//PORTB clocking allowed
	GPIOA->MODER &= ~(1<<10);												//PA5 alternate - SPI1 SCK
	GPIOA->MODER &= ~(1<<14);												//PA7 alternate - SPI1 MOSI
	GPIOB->MODER &= ~(1<<8);												//PB4 alternate - SPI1 MISO
																			//we use push-pull
	GPIOA->OSPEEDR |= (3<<10);												//PA5 very high speed
	GPIOA->OSPEEDR |= (3<<14);												//PA7 very high speed
	GPIOB->OSPEEDR |= (3<<8);												//PB4 very high speed
																			//no pull resistors
	//we leave the AF at reset state
																			//PA5 AF0
																			//PA7 AF0
																			//PB4 AF0

	gpio_port_SPI->OSPEEDR |= (3<<(gpio_pin_number_SPI * 2));				//CS pin is very high speed (likely not necessary)
																			//Note: the slave CS can be whichever pin we wish
	gpio_port_SPI->MODER |= (1<<(gpio_pin_number_SPI * 2));					//CS pin is general output - SPI1 CS
	gpio_port_SPI->MODER &= ~(1<<((gpio_pin_number_SPI * 2) + 1));			//CS pin is general output - SPI1 CS


	//3)Setup
	SPI1->CR1 |= (1<<2);													//master config
	SPI1->CR1 &= ~(1<<3);													//we don't prescale the SPI clock - only by the obligatory /2. We assume the APB2 clock to be 16 MHz.
																			//CPOL and CPHA both stay 0 for SPIMODE0
																			//DFF frame format remains 8 bits
																			//no CRC calculation
																			//we use full duplex, BIDIMODE bit remains 0

	SPI1->CR2 &= ~(1<<4);													//we are in Motorola mode
}


//2) Master write to a register
void SPI1MasterWrite (uint8_t reg_addr_write_to, uint8_t *bytes_to_send, uint8_t number_of_bytes, GPIO_TypeDef* gpio_port_SPI, uint8_t gpio_number) {
	/*
	 * What happens here?
	 * We have already set our peripheral as a master device (with master selected and the SSM/SSI bits properly set), so whenever we activate the SPI, the SCK will also start.
	 * First, we enable the slave by sending an active LOW signal to the external CS/SS (it will be just a GPIO output pulled LOW).
	 * We copy the first value we wish to send to over to the slave into the Tx buffer. Following the construction of SPI messages, this will be the address we of the register we want to write to.
	 * Then, for the number of bytes we want to write to this register, we will one-by-one copy the data into the Tx register by stepping a pointer (we assume that the bytes to send are organized already into an array)
	 * We also read from the Rx buffer every time we have loaded to Tx buffer to clear the Rx buffer and to reset its control flag.
	 * We end by disabling everything.
	 *
	 * We write a set of bytes to the slave.
	 * The function sends a write initialisation command, then follows it up with a set of bytes to be written into the initialised register.
	 * Mind, after every byte transfer, the master needs reset the RXNE flag to avoid any interrupts on the bus.
	 * We send command by writing to the DR register - which then be used as a buffer for the shift register of the SPI bus. TXE is reset when the buffer is empty and data has been transferred to the shift register.
	 * We read a reply by reading from the DR register - which then be used as a buffer for the incoming byte. The RXNE flag is reset when we read from the buffer.
	 * Data flow is controlled by checking the TXE and RXNE flags.
	 *
	 * Note: we must use the same port and pin selection as we did in the config function to enable the external CS/SS. We don't reconfigure that here, we just pull it LOW/HIGH.
	 * */

	gpio_port_SPI->BRR |= (1<<gpio_number);									//we enable the slave before we send the clock. CS setup time is minimum 5 ns, so no need for delay afterwards when using 32 MHz main clock (clock period is 31 ns)
	SPI1->CR1 |= (1<<6);													//SPI enabled. SCK is enabled
	SPI1->DR = reg_addr_write_to;												//we write a value into the DR register
	while(!((SPI1->SR & (1<<1)) == (1<<1)));								//wait for the TXE flag to go HIGH and indicate that the TX buffer has been transferred to the shift register completely
	while(!((SPI1->SR & (1<<0)) == (1<<0)));								//we wait for the RXNE flag to go HIGH, indicating that the command has been transferred successfully
	uint32_t buf_junk = SPI1->DR;											//we reset the RX flag

	while (number_of_bytes)
	{
		SPI1->DR = (volatile uint8_t) *bytes_to_send++;						//we load the byte into the Tx buffer
		while(!((SPI1->SR & (1<<1)) == (1<<1)));							//wait for the TXE flag to go HIGH and indicate that the TX buffer has been transferred to the shift register completely
		while(!((SPI1->SR & (1<<0)) == (1<<0)));							//we wait for the RXNE flag to go HIGH, indicating that the command has been transferred successfully
		buf_junk = SPI1->DR;												//we reset the RX flag
		number_of_bytes--;
	}

	while(((SPI1->SR & (1<<0)) == (1<<0)));									//we check that the Rx buffer is indeed empty
	while(!((SPI1->SR & (1<<1)) == (1<<1)));								//wait until TXE is set, meaning that Tx buffer is also empty
	while((SPI1->SR & (1<<7)) == (1<<7));
	gpio_port_SPI->BSRR |= (1<<gpio_number);								//we disable the slave
	Delay_us(1);															//since the SDO disable time is 50 ns, we need to have a small delay here to avoid data corruption
																			//Note: this might not be necessary here
	SPI1->CR1 &= ~(1<<6);													//disable SPI
}


//3) Master reads from a register
void SPI1MasterRead (uint8_t reg_addr_to_read_from, uint8_t *bytes_received, uint8_t number_of_bytes, GPIO_TypeDef* gpio_port_SPI, uint8_t gpio_pin_number_SPI) {
	/*
	 * What are we doing here?
	 * Initially, we do a similar thing as above since we need to "demand" the information first by sending over to the slave the register's address we want to read from.
	 * Then, in the number_of_bytes loop, we funnel the incoming Rx bytesout of the driver using a pointer, while loading the Tx buffer with a dummy 0xFF command. Mind, this is necessary to comply the duplex nature of SPI: the master can only receive a byte if it sends a byte!
	 * We end with disabling everything, just as before.
	 *
	 * We receive a byte from the slave.
	 * The function sends a read initialisation command, then follows it up with a set of reads.
	 * Mind, during every byte transfer, the master needs to supply either a command, or a dummy. This is due to the duplex nature of SPI.
	 * Every reply to a command will arrive on the next duplex command sent over by the master.
	 * Data flow is controlled by checking the TXE and RXNE flags.
	 *
	 * */

	gpio_port_SPI->BRR |= (1<<gpio_pin_number_SPI);							//we enable the slave before we send the clock. CS setup time is minimum 5 ns, so no need for delay afterwards when using 32 MHz main clock (clock period is 31 ns)
	SPI1->CR1 |= (1<<6);													//SPI enabled
	SPI1->DR = reg_addr_to_read_from;												//we write the address of the register we wish to read from
	while(!((SPI1->SR & (1<<1)) == (1<<1)));								//wait for the TXE flag to go HIGH and indicate that the TX buffer has been transferred to the shift register completely
	while(!((SPI1->SR & (1<<0)) == (1<<0)));								//we wait for the RXNE flag to go HIGH, indicating that the command has been transferred successfully
	uint32_t buf_junk = SPI1->DR;											//we reset the RX flag


	while (number_of_bytes) {
		SPI1->DR = 0xFF;													//we load a dummy command into the DR register
		while(!((SPI1->SR & (1<<0)) == (1<<0)));
		*bytes_received = SPI1->DR;											//we dereference the pointer, thus we can give it a value
																			//we extract the received value and by proxy reset the RX flag
		bytes_received++;													//we step our pointer within the receiving end
		number_of_bytes--;
	}

	while(((SPI1->SR & (1<<0)) == (1<<0)));									//we check that the Rx buffer is indeed empty
	while(!((SPI1->SR & (1<<1)) == (1<<1)));								//wait until TXE is set, meaning that Tx buffer is also empty
	while((SPI1->SR & (1<<7)) == (1<<7));
	gpio_port_SPI->BSRR |= (1<<gpio_pin_number_SPI);						//we disable the slave
	Delay_us(1);
																			//Note: this might not be necessary here
	SPI1->CR1 &= ~(1<<6);													//disable SPI
}
