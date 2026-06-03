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

## Later findings

The following points describe the difference between the [betaflight docs](https://betaflight.com/docs/development/API/DisplayPort) and the implementations.

### General

- MSP_DP_OPTIONS consists of 2 bytes: font (unknown usage, just set to 0), and the actual resolution enum

### FPVWTF: msp-osd

- has a BTFL special case: When it sees BTFL with MSP API version >= x.45, it automatically sends a new canvas size (MSP_SET_OSD_CANVAS, 60x22), as it should (well kinda, it is not in response to a heartbeat).
- for all other BTFL (< x.45) or any other FC firmware it does not do that.
- Even if the FC specifically sends a MSP_DISPLAYPORT.MSP_DP_OPTIONS frame, it does not confirm this selection via MSP_SET_OSD_CANVAS. It applies it though (SD, HD, Fake HD, 60x22).
- Only understands MSP V1, not V2. This could also be the case for the other video systems though (not checked).
- stops writing to the screen if a null character is seen, even if the MSP message is longer

### Walksnail

- has no BTFL special case.
- does not respond to MSP_DISPLAYPORT.MSP_DP_OPTIONS via MSP_SET_OSD_CANVAS => FC has no way to know whether it worked and cannot adjust its canvas size.
- "Luckily" the default canvas (at least of betaflight) matches the walksnail canvas size (53x20)

### HDZero

- no reverse engineering done, it probably respects the spec, except [according to inav](https://github.com/iNavFlight/inav/blob/e630ecf9662219f39414983faeb86b4691c27dc1/src/main/io/displayport_msp_osd.c), it uses a different enum for the resolutions than the betaflight docs describe (60x22 in a different slot, 53x20 nonexistent in BF docs, fake HD nonexistent in inav source code)

### Newer DJI

- No clue

### Newer Walksnail (Ascent)

- No clue

### Betaflight

- Does not send a MSP_DISPLAYPORT.MSP_DP_OPTIONS, hopes that it automatically gets a MSP_SET_OSD_CANVAS (which is according to their docs)
- Assumes a default size of 53x20

### INAV

- uses a different enum (see HDZero)
- has a DJIWTF compatibility mode (maybe only for character remapping)
- Has a different setting for the user depending on MSP Displayport or DJI FPV VTX

## Solution (hopefully temporary until stuff gets unified)

Kolibri will have an option to specify which HD provider (WTFOS, other, other manual) is attached. Some issues/PR to WS/WTFOS could help resolve these problems.

- WTFOS: whenever the FC switches the serial from offline to online, it sends out a MSP_DISPLAYPORT.MSP_DP_OPTIONS to request 60x22, then switches the canvas to 60x22 without waiting for a response (switches if it gets a MSP_SET_OSD_CANVAS request). Identical to "other manual" with size option 3.
- Other (BTFL alike): Sets canvas to 53x20, does not request anything, will switch if it receives MSP_SET_OSD_CANVAS.
- Other manual: just like WTFOS, it requests a size from the BTFL enum, defined by the user. Sets that size, adjusts if it receives a MSP_SET_OSD_CANVAS request.
