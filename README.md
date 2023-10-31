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

Be aware that CS/SS has an internal and an external element to it. The external element is what we discussed above, but the SPI driver itself will also have to have its own CS/SS driven in master mode so as to indicate that we activate the SPI bus. Luckily, we can connect the internal and the external CS/SS pins together, so whenever we enable the SPI, we will have both elements properly activated. Why I am not doing it here though is that this limits the CS/SS pin to PA4 or PA15 on the L0x3, which wasn’t convenient. As such, we will drive the external CS/SS as a simple GPIO output, and the internal CS/SS by interacting with the appropriate register bits (SSI in the CR1 register). 

Mind, the combination of where is the data sent (rising edge or falling edge) will be a quality of the bus and thus must be set the same way on both the master and the slave. Here we choose SPIMODE0 as the bus type, where both the clock phase and the polarity remains standard (we will send data on the falling edge and capture it on the rising one). The SPI mode demanded by a device is usually indicated within its datasheet.

Same can be said about the frame sizes. 8-bit frames is the standard value, so we keep it as such.

Since SPI is duplex, Rx and Tx will happen at the same time. This has the benefit that Rx and Tx will have a different buffer for their own data, plus different control flags (but also will demand two DMA channels to run, see below).

### DMA
DMA is particularly useful to manage SPI since the solves the duplex timing issue for us. To reiterate, the peripheral will always send a request to the DMA whenever it is necessary for it to have something in the Tx buffer or having something removed from the Rx buffer, ensuring that the communication remains fluent. As such, we do need two DMA channels just to properly run one peripheral, which is not so ideal… Without DMA though, a circular SPI communication would be difficult to implement, not to mention, it would likely be very sensitive timing deviation.

At any rate, for shorter message transitions (no data dumps), DMA is completely unnecessary.

## User guide
We should be aware that the guide above only describes, how to set up the hardware to behave as an SPI master. Afterwards though, we still would need to be control this hardware in a way that SPI-compliant messages will be formed.

We will use our mcu to communicate with a BMP280 temperature sensor using SPI. I am not going to explain the messaging and what needs to be written to the BMP280 since that is not the aim of this guide. Thus, it is highly recommended to read the BMP280 datasheet provided, for instance, here:

https://learn.adafruit.com/adafruit-bmp280-barometric-pressure-plus-temperature-sensor-breakout/downloads
