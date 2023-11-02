# STM32_SPIDriver

## General description
SPI – alongside UART and I2C – is one of the prime communication protocols one uses in mcu system. It can be scaled to rather high speed – at maximum, half the speed of the mcu’s internal clock – albeit it rarely is used faster than 10 MHz. (This is a purely practical reason since past 10 MHz, conventional jumper cables start to become inadequate to transfer the SPI data, for more information check “cables”.) Thus, it is hands down the fastest solution amongst the other aforementioned com protocols. It is also very easy to scale up to have multiple devices, thanks to the designated chip select/slave select pin.

Yet, it has its flaws, mostly in the form of control. Implementing – and managing – SPI in its standard form is usually rather complex.

Firstly, I do need to explain, what “standard” means. We can have multiple variants of SPI, some can even run on lower pin number and higher speed than what was indicated above. Nevertheless, any deviation from the standard means that we are less and less likely to be compatible with whatever device we intend to communicate with.

At any rate, in standard SPI:
-we can either be master or slave
-the communication is duplex
-we need 4 pins

### Master or slave?
Depending on which side of the coms we are, we can either be in master or slave mode. In a practical sense, this means that we can either control the chip select (CS) pin and the clock (SCK), or we can expect them to be controlled by an external source. Evidently, the behaviour of these two pins drive the entire bus, thus running our L0x3 as a master or a slave will demand a different approach from us. Unlike UART, we can’t drive an SPI bus without a master.

I will be showing only a master controller here, though I might make a project with a slave controller as well.

Of note, the peripheral on the L0x3 resets to SPI slave mode in case of a fault or inadequate setting up.

### Duplex coms
Duplex communication means that EVERY TIME when we send a byte, we receive a byte. As a matter of fact, we send the bytes over in pairs, one on the rising edge of the SCK clock signal, the other, on the falling edge (thus, the transfer doesn’t actually happen at the same time, albeit it does happen within one SCK clock period). From a practical point, we need to always manage both the Tx and the Rx side of the SPI protocol, otherwise the communication will be blocked.

### GPIOs
As mentioned, we need 4 pins to driver the SPI. These are:
-	MOSI: master-out-slave-in, one of the data lines
-	MISO: master-in-slave-out, the other data lines
-	SCK: the clock signal
-	CS/SS: the chip select/slave select signal

On MISO/MOSI, each is driven by either the slave or the master and uniquely by them (unlike I2C where both can drive the same data line). Thus, when connecting up SPI between devices, MISO pins connect to each other, similar to MOSI pins. There is no “Tx goes to Rx and Rx to Tx” like in UART.

SCK is the communication master clock, driven by the master only.

CS/SS is what selects the device for activity. The benefit of it is that multiple devices can be connected to the same bus, yet they won’t start communicating on the bus as long as this pin is not activated properly. I am emphasizing “used properly” since the CS/SS pins are, almost always, active LOW pins…meaning that they must be pulled HIGH by an external pullup resistor if they don’t have internal pullups added to them by design. We then “active” the pin by pulling it LOW.

## To read
The following sections are necessary for SPI:
- 31.3 SPI functional description (except 31.3.11 which discusses DMA): general description of how SPI works and how to set it up
- 31.7 SPI and I2S registers (I2S part is not necessary)

## Particularities
We will be using SPI1 here, which is clocked on APB2. From the ClockDriver project, we know that APB2 – the bus, not the connected timer – clocks at 16 MHz, so pre-scaling this value by 2 will lead us to a clocking speed of 8 MHz. This is an ideal timing for the SPI.

We are using the ClockDriver as clocking - not HAL - because we can have smaller delays than what HAL allows.

Be aware that CS/SS has an internal and an external element to it. The external element is what we discussed above, but the SPI driver itself will also have to have its own CS/SS driven in master mode so as to indicate that we activate the SPI bus. Luckily, we can connect the internal and the external CS/SS pins together, so whenever we enable the SPI, we will have both elements properly activated. Why I am not doing it here though is that this limits the CS/SS pin to PA4 or PA15 on the L0x3, which wasn’t convenient. As such, we will drive the external CS/SS as a simple GPIO output, and the internal CS/SS by interacting with the appropriate register bits (SSI in the CR1 register). 

Mind, the combination of where is the data sent (rising edge or falling edge) will be a quality of the bus and thus must be set the same way on both the master and the slave. Here we choose SPIMODE0 as the bus type, where both the clock phase and the polarity remains standard (we will send data on the falling edge and capture it on the rising one). The SPI mode demanded by a device is usually indicated within its datasheet.

Same can be said about the frame sizes. 8-bit frames is the standard value, so we keep it as such.

Since SPI is duplex, Rx and Tx will happen at the same time. This has the benefit that Rx and Tx will have a different buffer for their own data, plus different control flags (but also will demand two DMA channels to run, see below).

### DMA
DMA is particularly useful to manage SPI since the solves the duplex timing issue for us. To reiterate, the peripheral will always send a request to the DMA whenever it is necessary for it to have something in the Tx buffer or having something removed from the Rx buffer, ensuring that the communication remains fluent. As such, we do need two DMA channels just to properly run one peripheral, which is not so ideal… Without DMA though, a circular SPI communication would be difficult to implement, not to mention, it would likely be very sensitive timing deviation.

At any rate, for shorter message transitions (no data dumps), DMA is completely unnecessary.

## User guide
We should be aware that the guide above only describes, how to set up the hardware to behave as an SPI master. Afterwards though, we still would need to control this hardware in a way that SPI-compliant messages will be formed. The datasheet of the sensors usually detail, how SPI messages are constructed so I won't detail it here. The only thing to be aware of is that our peripheral handles the start and the stop parts, we merely need to control the external part of the CS/SS and manage to information that is being sent over/received from the bus.

Both the writing and reading side of the ths SPI takes arrays as input. On the write side, the first element of the row in the array (0th to be corrected) will be address of the register we want to write to, the rest will be the data we want to write to the register with each column holding one byte to write (thus the "number_of_bytes" input will be the number of columns in the array, minus 1). The write array will not be modified by the SPI write function and can be reused. On the read side, things are a bit different since we need to first write to the device (send over the read command and the register we wish to read from) followed by some dummy bytes that allows the reception of the readout. In how the main function works here, we send over the read command and the register we wish to read from, then use the same byte as the dummy, effectively replacing it with the readout value. This works since we have one readout value only. Mind, this approach means that the readout message array is being overwritten during the readout procedure (that's why I defined it within the while loop instead of the setup part, like for the reset and the standard function definition).

The SPI pins here are PA5, PA6 and PA7 with PB6 as the external CS/SS. These are the "standard" SPI pins. Of note, PA5 is also the inbuilt LED, so don't be surpirsed to see it light up as well.

We will use our mcu to communicate with a BMP280 temperature sensor using SPI. This sensor is cheap and very common in electronics design due to its relatively high sensitivity and robustness.

Be aware that the mcu clocks at 32 MHz, while the sensor will clock at lower speed than that (probably 10 MHz or something). This means that we need to "allow" a sensor to react to any command, we can't send the next command immediatelly after the other one, using 32 MHz. I have found a delay of 100 us as a good rough number to bridge over most timings using the BMP280.

I am not going to explain the messaging and what needs to be written to the BMP280 since that is not the aim of this guide. Just to give some guidelines:
- reading out is register address with MSB being 1
- writing to is register address with MSB being 0
- the F4 register is the control register, writing 0x27 will allow a standard sensor function
- the measured raw values are stored in registers 0xFA, 0xFB and 0xFC
- raw values are ADC values that need to be reconstructed and managed to turn them into temperature values
- necessary constants to calculate the temperature are stored in registers from 0x88 to 0x8D

Of note, once you have set the sensor, it won't be reset unless you tell it to or de-power it. You may find yourself running a sensor on a faulty setup command simply because the previous setup command was fine and not cleared prior.

All in all, it is highly recommended to read the BMP280 datasheet provided, for instance, here:

https://learn.adafruit.com/adafruit-bmp280-barometric-pressure-plus-temperature-sensor-breakout/downloads
