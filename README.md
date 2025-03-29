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

-   Configurator
-   Acro and angle modes
-   altitude and position hold modes (working, but need to be tuned)
-   ELRS
-   GPS (UBlox) and Compass (HMC5883 + QMC5883L)
-   Bidirectional DShot 4800 (tested up to 1200)
-   Variable frequency beeper with WAV support
-   SD-Blackbox incl. viewer
-   Barometer

Future shit:

-   Position hold, return to home and waypoint mission (incl. emergency switch)
-   3D camera dolly
-   trick trainer, that does a trick for you and displays in the OSD how good or bad you were at repeating that trick

Practically speaking, is there any reason to choose this over Betaflight or iNav? Likely not, but I want to have a challenge.

## Configurator screenshots

Home Page with a general preview of the quad's orientation, some status info and general controls
![grafik](https://github.com/user-attachments/assets/725f180a-3289-4ce4-969a-6d8dde11a078)

Blackbox viewer integrated into the configurator, you can record ELRS data, setpoints, rotation, attitude, PIDs, GPS, RPM, magnetometer and much more. Size is practically unlimited (SD card logging), but 1MB is about 10 seconds when logging practically everything. Integrated filtering to smooth noisy data, multiple graphs and multiple traces per graph, JSON converter for external analysis, pinch to zoom (touchscreen support). Don't mind the "Start Time" - it's wrong. The blackbox got some updates, which changed the interpretation of the timestamp.
![grafik](https://github.com/user-attachments/assets/bb819397-651f-4cb0-9b38-71407371d939)

Tasks viewer shows the execution duration and frequency of all the tasks to check they're running fast enough and to aid optimization of code (counting of frequency, max, min and average durations are not always done the same, so the results need to be checked against the code to see how each task is counted)
![grafik](https://github.com/user-attachments/assets/9b534745-9b4a-4360-bc34-2277b3d5ddf8)

Receiver page shows status of the RX and each channel
![grafik](https://github.com/user-attachments/assets/bbb3fcbe-68ee-4360-bbdb-47f93a442701)

Tuning is limited to basic PIDs and rates for now - filters have hardcoded cutoffs 😅
![grafik](https://github.com/user-attachments/assets/6c2688b9-4fe8-4278-bf45-2c150c2fc7e7)

GPS + Mag page shows X/Y Y/Z and Z/X graphs of the magnetometer to check calibration and skew. Lower side shows data obtained by the GPS (no GPS fix right now, so time is off)
![grafik](https://github.com/user-attachments/assets/e8557ebf-9b01-48e6-96f9-f8104626bf0b)



