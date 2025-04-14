## Design errors, fixes and improvements between PCB revisions

### V0.1

-   Known issues:
    1. Current Sensor
        - If there is no current sensor wire attached, the INA139 will output 4.3V, killing the ADC and potentially the Microcontroller
        - Solution: Wire a pullup resistor from the C pad to the V+ pad to ensure that the measured current is 0 rather than infinity when there is nothing attached to the C pad.
    2. Buck Converters
        - A diode is misplaced, killing the buck converters
        - Solution: Place the diode in the correct spot on both bucks
    3. Oscillators
        - There is only a spot for crystals but not for their capacitors, and the wiring is wrong for use with oscillators
        - Solution: Change wiring for oscillator for the RP2040
    4. No pullup on the CS line for the flash
    5. Boot button flipped

### V0.2

-   Goal: A working PCB with some tests to make v0.3 the first final version
    -   It uses the same stencil as v0.1, hence I had to make some compromises with regards to routing and other best practices. I didn't want to spend another 15$ on a stencil, when PCBs are so cheap in comparison.
-   Fixes for all 5 problems with v0.1
-   Same stencil
-   Improvements:
    -   0402 solder bridges instead of 0603
    -   8 layers because that allows for free ENIG finish and POFV at JLCPCB
    -   put baro I2C on aux I2C of gyro? => Solder bridge to test this out

### V0.3

-   Goal: V0.2 (and from a purely flying performance perspective v0.1 too) was generally working very well. Goal is to put in some new features especially blackbox on the SD card due to the slow flash writing. Since the microSD has its own buffer, it should be much faster.
-   Improvements
    -   ELRS with true diversity built in
    -   SD card slot for blackbox
    -   new SPI barometer (Goertek SPL06-001)
    -   synchronous buck converters: smaller (, more efficient)
-   Fixes
    -   removed pullup on current sensor (A relic from the INA139)
-   Known issues
    -   100nF 0402 caps on the bottom layer are very close to each other, causing solder bridges - no issue but not ideal
    -   WiFi antenna poor range (5-10 cm), not sure why
    -   ELRS boot pad is hard to reach
    -   SX1281 crystals 90 degrees rotated --> SX1281 doesn't work at all (jank fix works)
    -   forgot I2C pullups. No issue rn (no sensors using I2C), but with a compass it might be
    -   Bad ELRS range --> a few possible reasons
        -   LNA not working --> most likely (yet reason still unknown), because the RSSI has a very fast jump from about -25 to -75dBm, indicating something else than a pure passives issue. Also, in the transmission path (telemetry), everything works fine, indicating that the PCB and passive components are fine in general
        -   pads grounded insufficiently --> my bad, I didn't compare the library footprint with the datasheet, some pads are not grounded, as they were marked NC in the library but GND in the datasheet --> I put a solder bridge on the pads that are not grounded, and still it doesn't work well, so still unlikely
        -   AT2401C just not working? --> unlikely, but maybe there's a difference between this clone and the RFX2401C
        -   All of them are pretty unlikely, given the weird jump, but I don't know what else it could be
    -   OSD crystal somewhat weird. Nothing I tried worked. I tried putting a crystal from a brand new off the shelf FC on it, as well as using a crystal from V0.2 (via separate wires), as well as changing various components like the AT7456E or capacitors. Sometimes (with the off-the-shelf crystal or the V0.2 crystal), I get video for like 20-60 seconds, but after a while they all stop working until I cut power and wait a few seconds. Nothing gets particularly warm, hence my confusion with the issue. Will go back to the design that worked in V0.2, to give me some space to play with all different sizes

### V0.4

-   Goal: V0.3 introduced a lot of new features, but not all work properly. This PCB will hopefully fix all the new issues that emerged with V0.3. Especially the ELRS range and the OSD crystal, but also implement a proper non-jank fixes for the other stuff. No new features, similar to V0.2 compared to V0.1. Biggest issue is that this time I cannot really explain to myself the two big issues and so I just have to guess.
-   Fixes (at least I hope)
    -   ELRS range -> Test RFX2401C instead of AT2401C, and ground all the pins. I will also swap the RFX2401C onto V0.3 after arrival to see if it's a PCB issue or a component issue
    -   ELRS WiFi -> proper filter according to datasheet
    -   ELRS crystal 90° rotated
    -   ELRS boot pad is now a button
    -   A pair of 4.7kOhm pullups for I2C
    -   OSD crystal -> back to V0.2 design, except with more possibilities of testing, like selectable voltage and selectable whether a crystal or oscillator is used
    -   slightly more spacing for 100nF caps on the bottom layer

### V0.5

-   Changes
    -   Switch to RP2350
    -   some changes to the ELRS receiver, maybe it'll work this time?
    -   First version designed in KiCad
    -   Bus layout changed:
        -   Dedicated 3-wire SPI for gyro (PIO) => DMA polling should be possible now
        -   baro has one I2C with external devices like compass
        -   SDIO for SD card (PIO)
        -   Dedicated SPI for OSD
    -   WS2812 LEDs for the swag
    -   better speaker driver
-   What works: Seemingly everything :tada:
-   Idea dump for future:
    -   Reorder 6 pin for GPS/compass: Currently not immediately compatible with Walksnail M10 (RX/TX and SDA/SCL need to be swapped)
    -   RP235xB: more pins for
        -   normal LEDs again
        -   full duplex spi to gyro
        -   separate external I2C for compass
        -   baro via SPI instead of I2C (together with OSD)
        -   more UARTs via PIO (once SPI is separated, pio2 is completely free)
        -   PWM outputs
    -   Speaker driver with optional 5V input for bench testing or in general
    -   more capacitance on 10V rail
    -   separate gyro LDO, or LPF on its supply
    -   custom pixel OSD using PIO/HSTX :thinking:
    -   possible space savings
        -   smaller crystals
        -   2354 instead of 2350 (no separate flash required)
        -   replace USB on SH plug with solder pads [optionally under the USB port]
        -   (pixel OSD instead of AT7456E)
