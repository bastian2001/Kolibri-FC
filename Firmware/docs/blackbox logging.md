# Blackbox

The blackbox is being logged on the LittleFS of the flash chip. Blackbox files end in .rpbb.
The format is binary, and the first 4 bytes are the magic sequence 0x20, 0x27, 0xA1, 0x99, the next 3 bytes indicate the used blackbox format version (binary).
Magic bytes have the advantage of being able to indicate (not verify) the file type. Many formats use it, e.g. images.

### v0.0.0

Each log frame goes from LSB to MSB, all of the enabled fields are logged

-   fixed length header (after version code):
    -   4 bytes UNIX timestamp of start of log (if known, otherwise zeros)
    -   1 byte indicating the PID loop frequency
        -   0: 3.2kHz
        -   1: 1.6kHz
        -   2: 0.8kHz and so on
    -   1 byte indicating a loop divider (1 for no divider, 2 for half rate and so on)
    -   1 byte indicating the gyro ranges (0 (LSB) ... 2: gyr_range register, 3...4 acc_range register)
    -   60 byte rate factors (direct copy of the array from memory)
    -   3x28 bytes PID gains (fixed 16.16 int)
        -   0-3: P gain
        -   4-7: I gain
        -   8-11: D gain
        -   12-15: FF gain
        -   16-19: setpoint follow gain
        -   20-23: iFalloff
        -   24-27: reserved
    -   8 bytes (64 bits), indicating which fields are logged
        -   0 (LSB)...3: R/P/T/Y ELRS raw
        -   4...7: R/P/T/Y setpoint
        -   8...10: R/P/Y Gyro raw
        -   11...25: P/I/D/FF/S roll, then pitch, then yaw
        -   26: motor outputs
        -   27: altitude
-   field description and definition
    -   RPTY raw: stick position from ELRS after scaling to 1000-2000/989-2012 (2 bytes per stick)
    -   RPTY setpoint (smoothed): each setpoint is 2 bytes, in deg/second as a 12.4 bit fixed integer (filtered)
    -   Gyro raw: 2 byte gyro value after scaling (positive directions: roll right, pitch forward, yaw right)
    -   P/I/D/FF/S terms: 16 bit (2 byte) integer values => floored values, after multiplication with gains
    -   Motor outputs: 12 bits per motor => 6 bytes in total, throttle value from 0-2000 (with 1 = DShot value 48), order: RR, FR, RL, FL
    -   altitude: 2 byte integer altitude above takeoff
