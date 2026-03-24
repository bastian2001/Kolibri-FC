# MSP Displayport Reverse Engineering

## Just why? It's documented!

Documentation is not really the best. But more importantly, my VTX just doesn't send me any data at all. So it's easier to just test what Betaflight does.

## What I did

1. Attach BF TX to pin 10 and Avatar RX
2. Attach BF RX to pin 11 and Avatar TX
3. Create 2 serials, with each having an RX pin on 10 and 11 to sniff both
4. Turning the 10V rail on, immediately(!) results in a bunch of MSP_DISPLAYPORT (182) packets from the FC (to pin 10). Somehow there are no MSP packets from the VTX. A single byte (probably just jitter on the UART line) is received
5. After a while, the VTX also sends a bunch of data
6. Somehow the Betaflight FC knows to stop sending data right as soon as I power off the VTX. Maybe the VTX also just clamps the voltage low or so when powered off so that's why I don't receive anything
7. Theory confirmed: unplugging the VTX leads to a constant flow of 182s
8. Ok a bunch of debugging later: FC continuously sends out 182s (response without a prior request!), VTX (after booting up for ~30s) asks for a bunch of 101s, nothing else (Goggles turned off)
9. Now with Goggles on, the output is filtered from 182s and 101s: VTX asks for 3 and 2 about once a second each (get FW version and variant). A 188 is nowhere to be seen. For some reason, betaflight still knows correctly that the grid is 53x20 chars
10. Looking at betaflight source: main/osd/osd.h yields the default size of 53x20. So: walksnail is not spec compliant by just not sending a resolution, and walksnail also just assumes this 53x20 grid, wtf. Betaflight assumes this grid size if nothing else is given, so it appeared like everything was working well, when this was just a "coincidence"
