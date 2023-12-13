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
    -   new SPI barometer
    -   synchronous buck converters: smaller (, more efficient)
-   Fixes
    -   removed pullup on current sensor (A relic from the INA139)
