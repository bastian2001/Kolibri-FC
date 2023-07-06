## Design errors and fixes between PCB revisions

### V0.1

-   Known issues:
    1. Current Sensor
        - If there is no current sensor wire attached, the INA139 will output 4.3V, killing the ADC and potentially the Microcontroller
        - Solution: Wire a pullup resistor from the C pad to the V+ pad to ensure that the measured current is 0 rather than infinity when there is nothing attached to the C pad.
