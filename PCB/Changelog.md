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
        - Solution: Change wiring for oscillators
    4. No pullup on the CS line for the flash
        - Solution: Add a pullup resistor to the CS line

### V0.2 (not finished yet)

-   Fixes for all 4 problems with v0.1
-   Same stencil
-   Improvements:
    -   0402/0201? solder bridges instead of 0603
    -   solder bridge to select 4.5V or 5V for the 4 pin SH1.0 connector
    -   Increased space for some components on the underside
    -   8 layers because that allows for free ENIG finish and POFV at JLCPCB

### V0.3 (lookahead)

-   Improvements:
    -   New synchronous buck converters?
    -   PMS150C or PMS154C to get more pins?
