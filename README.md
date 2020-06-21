# rpi-i2c
A small binary utility to update I2C timing registers on a Raspberry Pi 4B board.

## Overview
Some sensors such as the [SCD30](https://www.sensirion.com/en/environmental-sensors/carbon-dioxide-sensors/carbon-dioxide-sensors-co2/)
require I²C clock stretching support with long timeout values. As of the time of writing, the Raspberry Pi 4B uses the older bcm2835-i2c
driver, which contains a hardcoded timeout of 35ms - in line with the SMBus spec, but potentially incompatible with slower plain-I²C
sensors.

`rpi-i2c` is a simple executable binary written in C to directly modify memory-mapped registers of the I2C1 controller of the BCM2711.
It can be used on a one-off basis (with SUID bit set) to change the bus timings without re-booting and/or configured as a `systemd` service
to update timings upon system boot. The utility also updates the falling edge and rising edge delay values based on the clock divider.

## Usage

### Meaning of `DIV.CDIV` and `CLKT.TOUT`
As described in the [ARM Peripherals Manual](https://www.raspberrypi.org/documentation/hardware/raspberrypi/bcm2711/rpi_DATA_2711_1p0.pdf),
the `CDIV` value within the `DIV` register is used to compute the effective I²C rate as follows:
```
i2c_clock_hz = core_clock_hz / CDIV

CDIV = core_clock_hz / i2c_clock_hz
```
The datasheet lists a *"nominal"* core_clock value of 150MHz and a default `CDIV` value of 1500. However, that is clearly not the case:
`/sys/kernel/debug/clk/vpu/clk_rate` reports a speed of 500MHz, and the default divider reads 5000. An I²C bus capture confirms this:

![I2C 100KHz capture](img/i2c-100khz.png "I2C 100KHz capture")

The core_clock value in Hz can be obtained using `rpi-i2c`.

To slow the clock down to 10KHz with a core clock of 500MHz:

```
CDIV = core_clock_hz / i2c_clock_hz = 500000000Hz / 10000Hz = 50000.
```

The clock stretching timeout is specified in *SCL cycles*. Therefore the effective timeout *duration in seconds* is also tied to
the clock frequency:

```
timeout_sec = TOUT / i2c_clock_hz

TOUT = timeout_sec * i2c_clock_hz
```

Following the above example of 10KHz i2c_clock and a desired timeout of 200ms:
```
TOUT = 200ms * 10KHz = 0.2s * 10000Hz = 2000
```

### Running rpi-i2c

Get current register values:
```bash
$ sudo rpi-i2c # read current values
Raspberry Pi I2C timing utility

To read current timing values, run the program without arguments.
To set new timing values: rpi-i2c <div.cdiv> <clkt.tout>

ARM peripheral address base: 0xfe000000
Core clock (Hz): 500000000
I2C1 controller address base: 0xfe804000
DIV.CDIV: 5000
CLKT.TOUT: 3500
```

Set new register values:
```bash
$ sudo rpi-i2c 5000 20000 # keep clock at 500MHz/5000 = 100KHz, update timeout to 20K cycles = 200ms
Raspberry Pi I2C timing utility

To read current timing values, run the program without arguments.
To set new timing values: /usr/bin/rpi-i2c <div.cdiv> <clkt.tout>

ARM peripheral address base: 0xfe000000
Core clock (Hz): 500000000
I2C1 controller address base: 0xfe804000
DIV.CDIV: 5000
CLKT.TOUT: 3500
Updating delay values to: FEDL=312, REDL=1250.
Timing values updated: CDIV=5000, CLKT=20000.
```

See [rpi-i2c-timeout.service](rpi-i2c-timeout.service) for a sample `systemd` service to run the utility upon boot.

## Notes & caveats

This utility has been developed and tested on a Raspberry Pi 4B 8GB board, driving a handful of I²C sensors directly
via the I2C1 hardware bus. It was motivated by a requirement of one of the sensors (SCD30) to support clock stretching
of up to 150ms.

The binary calls `bcm_host_get_peripheral_address()` to get the peripheral address offset; this may or may not work on
other versions of the board. Older boards were also susceptible to a hardware bug in the clock stretching implementation,
and this is highly unlikely to change that behaviour.
