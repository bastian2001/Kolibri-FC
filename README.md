# RP2040 Flight Controller for FPV

I wanted to have some fun designing my own flight controller in both hardware and software. [This project of mine](https://github.com/bastian2001/Hardware-DShot-on-RP2040) served as a proof of concept (and mostly minimal example) that the RP2040 is capable of driving DShot through the PIO (Programmable IO) hardware.

The PCB is done and working in its fourth testing revision, which includes (hardware features):

-   3-8S input voltage
-   5V 2.5A, 10V 2.5A buck converters
-   30.5mm stack mounting
-   BMI270
-   Analog character based OSD similar to Betaflight/iNav
-   SD card slot for blackbox
-   Barometer (SPL06-001)
-   true diversity ELRS receiver (some issues to be investigated, but it works for the most part)
-   hopefully some fixed bugs from v0.3, see [Changelog](PCB/Changelog.md)

Working software features:

-   Acro, angle and altitude hold modes
-   ELRS
-   Bidirectional DShot 4800 (tested up to 1200)
-   Variable frequency beeper with WAV support
-   SD-Blackbox incl. viewer
-   Configurator
-   Barometer

Future shit:

-   Position hold, return to home and waypoint mission (incl. emergency switch)
-   3D camera dolly
-   trick trainer, that does a trick for you and displays in the OSD how good or bad you were at repeating that trick

Practically speaking, is there any reason to choose this over Betaflight or iNav? Likely not, but I want to have a challenge.
