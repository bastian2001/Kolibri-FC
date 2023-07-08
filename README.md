# RP2040 Flight Controller for FPV

I wanted to have some fun designing my own flight controller in both hardware and software. [This project of mine](https://github.com/bastian2001/Hardware-DShot-on-RP2040) serves as a proof of concept that the RP2040 is capable of driving DShot through the PIO (Programmable IO) hardware.

The goals are not yet defined, however the PCB is done in its first revision, which includes (hardware features):
- 3-6S (8S possible with very little modification)
- the usual 30.5mm stack mounting
- Current Sensor (see [Errata](./PCB/Errata.md))
- Barometer
- Analog character based OSD similar to Betaflight/iNav
- BMI270

I have some idea what I am looking forward to (software features):
- Primarily acro
- Position hold, return to home and waypoint mission (incl. emergency switch)
- 3D camera dolly
- trick trainer, that does a trick for you and displays in the OSD how good or bad you were at repeating that trick

Practically speaking, is there any reason to choose this over Betaflight or iNav? Likely not, but I want to have a challenge.
