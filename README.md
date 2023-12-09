# RP2040 Flight Controller for FPV

I wanted to have some fun designing my own flight controller in both hardware and software. [This project of mine](https://github.com/bastian2001/Hardware-DShot-on-RP2040) serves as a proof of concept that the RP2040 is capable of driving DShot through the PIO (Programmable IO) hardware.

The goals are not yet defined, however the PCB is done and working in its second revision, which includes (hardware features):
- 3-6S (8S possible with very little modification)
- the usual 30.5mm stack mounting
- Barometer
- Analog character based OSD similar to Betaflight/iNav (fixed with v0.2)
- BMI270

Working software features:
- ELRS
- Acro Mode
- DShot (up to 4800)
- Variable frequency beeper
- Angle mode (semi-working)
- Blackbox (semi-working)
- Configurator
- Barometer

V0.3 plans:
- ELRS onboard
- SD slot to fix problems with blackbox
- new SPI Barometer (likely Goertek SPL06-001)

Future shit:
- Position hold, return to home and waypoint mission (incl. emergency switch)
- 3D camera dolly
- trick trainer, that does a trick for you and displays in the OSD how good or bad you were at repeating that trick

Practically speaking, is there any reason to choose this over Betaflight or iNav? Likely not, but I want to have a challenge.
