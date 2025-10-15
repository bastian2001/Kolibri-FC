# Blackbox

This Document describes the blackbox logging as of 11th July 2025 (version 0.0.1).

The blackbox is being logged on the SD card on the FC. Blackbox files end in .kbb, e.g. KOLI0001.kbb.
The format is binary, and the first 8 bytes are the magic sequence 0xDC 0xDF 0x4B 0x4F 0x4C 0x49 0x01 0x00, the next 3 bytes indicate the used blackbox format version (binary, e.g. 0x00 0x00 0x01 for 0.0.1).
Magic bytes have the advantage of being able to indicate (not verify) the file type. Many formats use it, e.g. images, I used this post as an inspiration for the type of magic number: https://chaos.social/@zwol@hackers.town/114155807784368727.

The file starts with a header, and then the data frames follow. Numbers of multiple bytes length are stored in Little Endian form. The header is currently 256 bytes long (including the magic number and version), with many bytes being reserved for future use. Since it is planned to store a large part of the settings in the header, this header may grow in the future.

## Header

Format: offset+length: description

-   0+8: magic number
-   8+3: version
-   11+4: UNIX timestamp of the start of the log, UTC
-   15+4: uint32 of the duration of the log in milliseconds. Initially set to 0, filled in at the end of the log. Can be used to indicate a broken log, if this is not set to a value greater than 0.
-   19+1: PID rate index, 0 = 3200Hz
-   20+1: blackbox frequency divider, e.g. 1 = 1:1 or 4 = 1:4. 0 is undefined
-   21+1: indicating the gyro ranges (0 (LSB) ... 2: gyr_range register, 3...4 acc_range register)
-   22+36: rate coefficient (fix32[3][3], 16.16 bits, first dimension is the axis: 0 = roll, 1 = pitch, 2 = yaw, second dimension is the type of coefficient: 0 = center, 1 = max, 2 = expo)
-   58+24: unused
-   82+60: PID gains (fix32[3][5], 16.16 bits, first dimension is the axis, second dimension is PID: P, I, D, FF, S)
-   142+8: enabled blackbox fields (uint64_t, bitmask, 0 = disabled, 1 = enabled)
-   150+1: motor pole count
-   151+1: Disarm reason
-   rest filled with 0x00

### Blackbox Fields

Currently a 64 bit bitmask, where each bit indicates whether a field is enabled or not. The following fields are currently defined. An asterisk (\*) indicates that this field is not logged in the regular frame, but has its own frame type due to a different update frequency or other reasons. Typically, all fields are two bytes in size, any exceptions are marked with the byte size in brackets.

-   0\*: `LOG_ELRS_RAW`: Raw RC data (988-2012us) for the first 4 channels
-   1: `LOG_ROLL_SETPOINT`: 12.4 fixed int of the roll setpoint in degrees per second
-   2: `LOG_PITCH_SETPOINT`: 12.4 fixed int of the pitch setpoint in degrees per second
-   3: `LOG_THROTTLE_SETPOINT`: 12.4 fixed int of the throttle setpoint (0-2000 for 0-100% throttle)
-   4: `LOG_YAW_SETPOINT`: 12.4 fixed int of the yaw setpoint in degrees per second
-   5: `LOG_ROLL_GYRO_RAW`: 12.4 fixed int of the roll rate in degrees per second, unfiltered
-   6: `LOG_PITCH_GYRO_RAW`: 12.4 fixed int of the pitch rate in degrees per second, unfiltered
-   7: `LOG_YAW_GYRO_RAW`: 12.4 fixed int of the yaw rate in degrees per second, unfiltered
-   8: `LOG_ROLL_PID_P`: P term of the roll axis, int16
-   9: `LOG_ROLL_PID_I`: I term of the roll axis, int16
-   10: `LOG_ROLL_PID_D`: D term of the roll axis, int16
-   11: `LOG_ROLL_PID_FF`: FF term of the roll axis, int16
-   12: `LOG_ROLL_PID_S`: S term of the roll axis, int16
-   13: `LOG_PITCH_PID_P`: P term of the pitch axis, int16
-   14: `LOG_PITCH_PID_I`: I term of the pitch axis, int16
-   15: `LOG_PITCH_PID_D`: D term of the pitch axis, int16
-   16: `LOG_PITCH_PID_FF`: FF term of the pitch axis, int16
-   17: `LOG_PITCH_PID_S`: S term of the pitch axis, int16
-   18: `LOG_YAW_PID_P`: P term of the yaw axis, int16
-   19: `LOG_YAW_PID_I`: I term of the yaw axis, int16
-   20: `LOG_YAW_PID_D`: D term of the yaw axis, int16
-   21: `LOG_YAW_PID_FF`: FF term of the yaw axis, int16
-   22: `LOG_YAW_PID_S`: S term of the yaw axis, int16
-   23 (6): `LOG_MOTOR_OUTPUTS`: Quadruple of the motor outputs, each as a uint12. Can be decoded by generating a 48 bit int, and then splitting it into 4 uint12s. From LSB to MSP: RR, FR, RL, FL
-   24: `LOG_FRAMETIME`: Time in microseconds since the last recorded frame, uint16
-   25: `LOG_ALTITUDE`: 10.6 fixed int of the sensor-fused altitude, in meters, resolution is about 1.6cm per LSB
-   26: `LOG_VVEL`: 8.8 fixed int of the sensor-fused vertical velocity, in meters per second, resolution is about 4mm/s per LSB
-   27\*: `LOG_GPS`: Raw GPS data as supplied in the UBX-NAV-PVT message
-   28: `LOG_ATT_ROLL`: int16 of the roll angle in 0.0001 rad per LSB
-   29: `LOG_ATT_PITCH`: int16 of the pitch angle in 0.0001 rad per LSB
-   30: `LOG_ATT_YAW`: int16 of the yaw angle in 0.0001 rad per LSB, not corrected using the magnetometer, purely from the gyro
-   31 (6): `LOG_MOTOR_RPM`: Quadruple of the ERPM packets that were returned from the ESCs. Each is a 12 bit value. Use the same method as for `LOG_MOTOR_OUTPUTS` to split them. 0x000 is used to indicate that the ESC did not return a valid value. Decode it like it is a DShot telemetry packet after GCR decoding and with a stripped CRC, may include Extended DShot Telemetry data.
-   32 (6): `LOG_ACCEL_RAW`: Triple of the accelerometer raw values, each as an int16. The values directly come from the accelerometer and only have the offset calibration orientation applied, no filtering.
-   33 (6): `LOG_ACCEL_FILTERED`: Same scaling etc. as `LOG_ACCEL_RAW`, but after the low pass filter has been applied.
-   34: `LOG_VERTICAL_ACCEL`: 9.7 fixed int of the vertical acceleration in m/s²
-   35: `LOG_VVEL_SETPOINT`: 4.12 fixed int of the vertical velocity setpoint in m/s
-   36: `LOG_MAG_HEADING`: 3.13 fixed int of the magnetometer heading in rad, not filtered or interpolated using the gyro, purely from the magnetometer
-   37: `LOG_COMBINED_HEADING`: 3.13 fixed int of the combined heading in rad, filtered and interpolated using the gyro (fused sensor heading)
-   38 (4): `LOG_HVEL`: Pair of 8.8 fixed ints, each the same as `LOG_VVEL`, but for the horizontal velocity, in m/s. The first value is the north component, the second value is the east component. Fully filtered and after all sensor fusion
-   39 (3): `LOG_BARO`: Raw 24 bit pressure reading from the barometer. Interpretation depends on the barometer used
-   40 (4): `LOG_DEBUG_1`: A 32 bit debug variable
-   41 (4): `LOG_DEBUG_2`: A 32 bit debug variable
-   42: `LOG_DEBUG_3`: A 16 bit debug variable
-   43: `LOG_DEBUG_4`: A 16 bit debug variable

## Data

Each data frame starts with a frame identifier (1 byte) followed by the frame data. The following data frames exist:

-   0: `BB_FRAME_NORMAL`. The length of this frame is dictated by the enabled blackbox fields. A regular frame includes all the enabled fields, except those previously marked with an asterisk.
-   1: `BB_FRAME_FLIGHTMODE`. This frame is 1 byte long and contains the new flight mode index. This frame is stored as the first data frame after the header to indicate the flight mode at the start of the log.
-   2: `BB_FRAME_HIGHLIGHT`. This frame has no data and just indicates that the user pressed the highlight button on the transmitter. It is logged to mark a point of interest in the log.
-   3: `BB_FRAME_GPS`.This frame is logged when a GPS packet arrives. It is 92 bytes long and contains the raw UBX-NAV-PVT message, which includes the time, position, velocity, and other information. This frame (if enabled) is logged roughly 20 times per second, if the GPS receiver allows that. This frame is only stored if the GPS logging is enabled in the blackbox fields (`LOG_GPS`).
-   4: `BB_FRAME_RC`. This frame is always logged when an RC packet arrives. Since ELRS is asynchronous from the PID loop, this frame is also logged asynchronously and not part of the normal frames. The frame is 6 bytes long and stores the first 4 channels of the RC packet, each from 988-2012 as a uint12. This frame is only logged if the `LOG_ELRS_RAW` field is enabled.

These data frames are chained one after another in the file. To write a flight mode frame (with a switch to flight mode 4), then a highlight frame and then a normal frame, the file would look like this:

```
0x01 0x04 0x02 0x00 [...data for the normal frame...]
^^^^^^^^^ ^^^^ ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
   |       |       |
   |       |       +-- normal frame (0x00 identifier + frame data)
   |       +-- highlight frame (just 0x02 identifier)
   +-- flight mode frame (0x01 identifier + flight mode index, 0x04)
```

Since the frames are generated asynchronously (dual core processor), it is uncertain (by one frame) when exactly the new flight mode applies, when exactly the highlight was pressed or when exactly the new ELRS or GPS data was first processed in the PID controller. Blackbox viewers should align the special frames to the _next_ normal frame, so that e.g. a flight mode change between frames 10 and 11 applies in frame 11.
