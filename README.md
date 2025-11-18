# RP2350 Flight Controller for FPV

I wanted to have some fun designing my own flight controller in both hardware and software. [This project of mine](https://github.com/bastian2001/Hardware-DShot-on-RP2040) ([superseded](https://github.com/bastian2001/pico-bidir-dshot/)) served as a proof of concept (and mostly minimal example) that the RP2040 is capable of driving DShot through the PIO (Programmable IO) hardware.

Main software features:

-   Configurator
-   Acro and angle modes
-   altitude and position hold modes (working, but need to be tuned)
-   ELRS
-   GPS (UBlox) and Compass (HMC5883 + QMC5883L)
-   Bidirectional DShot 4800 (tested up to 1200)
-   Variable frequency beeper with WAV support
-   SD-Blackbox incl. viewer
-   Barometer (Goertek SPL06-001 + STM LPS22HB)

> [!NOTE]
> The PCB now lives in [its own repo](https://github.com/bastian2001/Kolibri-FC-Hardware)

Future shit:

-   return to home and waypoint mission (incl. emergency switch)
-   3D camera dolly
-   trick trainer, that does a trick for you and displays in the OSD how good or bad you were at repeating that trick

Practically speaking, is there any reason to choose this over Betaflight or iNav? Likely not, but I want to have a challenge.

## Configurator screenshots

Home Page with a general preview of the quad's orientation, some status info and general controls
<img width="2560" height="1396" alt="Bildschirmfoto_20251118_164835" src="https://github.com/user-attachments/assets/66cc1d67-906b-4c1b-9197-6ae756cfee57" />

Blackbox viewer integrated into the configurator. You can record ELRS data, gyro rates, PID, a bunch of sensor data and much more. Size is practically unlimited (SD card logging), for me it is usually in the range of 80-150KB/s. Integrated filtering to smooth noisy data, multiple graphs and multiple traces per graph, JSON converter for external analysis, pinch to zoom (touchscreen support).
<img width="2560" height="1396" alt="Bildschirmfoto_20251118_171542" src="https://github.com/user-attachments/assets/b8b6045c-7c4c-4b9e-a91a-98f4010165c7" />
<img width="1869" height="1012" alt="Bildschirmfoto_20251118_170242" src="https://github.com/user-attachments/assets/dbdcd3cf-3703-43e0-8ca4-0513d8505ccb" />

Tasks viewer shows the execution duration and frequency of all the tasks to check they're running fast enough and to aid optimization of code. Some functions are not always running, hence the NaN and 0Hz.
<img width="2560" height="1396" alt="Bildschirmfoto_20251118_165556" src="https://github.com/user-attachments/assets/3c1360c3-f7fa-4f25-8e7c-3372519fbaee" />

Receiver page shows status of the RX and each channel
<img width="2560" height="1396" alt="Bildschirmfoto_20251118_171011" src="https://github.com/user-attachments/assets/82dcc761-be12-4017-95ca-204f24d1bcd7" />

Tuning of PIDs and filters, as well as rates, with live preview of rates and Blender-style input boxes
<img width="2560" height="1396" alt="Bildschirmfoto_20251118_165937" src="https://github.com/user-attachments/assets/fffe9d9a-2334-4cf0-b838-2d7611710049" />

GPS + Mag page shows X/Y Y/Z and Z/X graphs of the magnetometer to check calibration and skew. Lower side shows data obtained by the GPS (no GPS fix right now, so time is off)
![grafik](https://github.com/user-attachments/assets/e8557ebf-9b01-48e6-96f9-f8104626bf0b)

CLI for some additional settings and easier development and testing of new ones
<img width="2560" height="1396" alt="Bildschirmfoto_20251118_170111" src="https://github.com/user-attachments/assets/cbf4828b-ccf2-4d48-9f04-373424699846" />

More features: Motor remapping and Analog VTX channel adjustment (IRC Tramp)
<img width="1822" height="519" alt="Bildschirmfoto_20251118_165705" src="https://github.com/user-attachments/assets/d2882062-5b8c-439f-8340-ac16bc7c19ec" />
<img width="902" height="437" alt="Bildschirmfoto_20251118_165611" src="https://github.com/user-attachments/assets/2fd337e3-2290-4c5f-a1d5-70ec91846839" />

You can also connect via you ELRS' WiFi, and everything except very large CLI operations and the Tasks viewer works the same way (512 byte limit on ELRS side). Even blackbox data can be transmitted over WiFi.
