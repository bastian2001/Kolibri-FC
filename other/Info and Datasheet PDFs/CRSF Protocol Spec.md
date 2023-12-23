# CRSF Protocol

Sourced from https://github.com/ExpressLRS/ExpressLRS/wiki/CRSF-Protocol

## Packet Format

All packets are in the CRSF format `[dest] [len] [type] [payload] [crc8]` with a maximum total size of 64 bytes.

### DEST

Destination address or "sync" byte

-   Going to the transmitter module, CRSF_ADDRESS_CRSF_TRANSMITTER (0xEE)
-   Going to the handset, CRSF_ADDRESS_RADIO_TRANSMITTER (0xEA)
-   Going to the flight controller, CRSF_ADDRESS_FLIGHT_CONTROLLER (0xC8)
-   Going to the receiver (from FC), CRSF_ADDRESS_CRSF_RECEIVER (0xEC)

### LEN

Length of bytes that follow, including type, payload, and CRC (PayloadLength+2). Overall packet length is PayloadLength+4 (dest, len, type, crc), or LEN+2 (dest, len).

### TYPE

CRSF_FRAMETYPE. Examples:

-   CRSF_FRAMETYPE_LINK_STATISTICS = 0x14
-   CRSF_FRAMETYPE_RC_CHANNELS_PACKED = 0x16
-   CRSF_FRAMETYPE_DEVICE_PING = 0x28
-   CRSF_FRAMETYPE_DEVICE_INFO = 0x29
-   CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY = 0x2B
-   CRSF_FRAMETYPE_PARAMETER_READ = 0x2C
-   CRSF_FRAMETYPE_PARAMETER_WRITE = 0x2D
-   CRSF_FRAMETYPE_COMMAND = 0x32

### PAYLOAD

Data specific to the frame type. Maximum of 60 bytes.

### CRC

CRC8 using poly 0xD5, includes all bytes from type (buffer[2]) to end of payload.

### Baud Rate

Standard receiver baud is reported to be 416666 baud, Betaflight/iNav/ExpressLRS use 420000 baud (CRSF v2). ExpressLRS transmitter modules support 115200, 400000, 921600, 1.87M, 2.25M, 3.75M, 5.25M baud (automatic switching).

## Extended Packet Format

To support tunneling of packets through the CRSF protocol to remote endpoints, there is an extended packet format, which includes an additional
extended dest and source as part of the payload. In this case, the LEN field is actually PayloadLength+4, as the first two bytes are the
extended dest and source. `[dest] [len] [type] [[ext dest] [ext src] [payload]] [crc8]`

I _believe_ any CRSF*FRAMETYPE 0x28 or higher uses Extended Packet Format. The dest for all extended packets \_might* always be 0xc8.

## Channels Packets

RC channels data going from the handset to the transmitter module, and going from the receiver to the flight controller use the same packet type: `CRSF_FRAMETYPE_RC_CHANNELS_PACKED` (0x16). Coming from the handset the dest should be `CRSF_ADDRESS_CRSF_TRANSMITTER` (0xEE) and coming from the receiver the dest should be `CRSF_ADDRESS_FLIGHT_CONTROLLER` (0xC8). The data is just 16x channels packed to 11 bits (LSB-first, little endian). `[dest] 0x18 0x16 [channel00:11] [channel01:11] ... [channel15:11] [crc8]`. Depending on the endianness of your platform, this struct may work to encode/decode the channels data:

```c
typedef struct crsf_channels_s
{
    unsigned ch0 : 11;
    unsigned ch1 : 11;
    unsigned ch2 : 11;
    unsigned ch3 : 11;
    unsigned ch4 : 11;
    unsigned ch5 : 11;
    unsigned ch6 : 11;
    unsigned ch7 : 11;
    unsigned ch8 : 11;
    unsigned ch9 : 11;
    unsigned ch10 : 11;
    unsigned ch11 : 11;
    unsigned ch12 : 11;
    unsigned ch13 : 11;
    unsigned ch14 : 11;
    unsigned ch15 : 11;
} PACKED crsf_channels_t;
```

The values are CRSF channel values (0-1984). CRSF 172 represents 988us, CRSF 992 represents 1500us, and CRSF 1811 represents 2012us. Example: All channels set to 1500us (992) `ee 18 16 e0 3 1f f8 c0 7 3e f0 81 f 7c e0 3 1f f8 c0 7 3e f0 81 f 7c ad`

## LinkStatistics

Packet sent to both the flight controller and handset with information about the RF link: RSSI, LQ, etc.

```
 * uint8_t Uplink RSSI Ant. 1 ( dBm * -1 )
 * uint8_t Uplink RSSI Ant. 2 ( dBm * -1 )
 * uint8_t Uplink Package success rate / Link quality ( % )
 * int8_t Uplink SNR ( dB, or dB*4 for TBS I believe )
 * uint8_t Diversity active antenna ( enum ant. 1 = 0, ant. 2 = 1 )
 * uint8_t RF Mode ( 500Hz, 250Hz etc, varies based on ELRS Band or TBS )
 * uint8_t Uplink TX Power ( enum 0mW = 0, 10mW, 25 mW, 100 mW, 500 mW, 1000 mW, 2000mW, 50mW )
 * uint8_t Downlink RSSI ( dBm * -1 )
 * uint8_t Downlink package success rate / Link quality ( % )
 * int8_t Downlink SNR ( dB )
 * Uplink is the connection from the ground to the UAV and downlink the opposite direction.
```

## Command

Commands are sent using extended CRSF_FRAMETYPE_COMMAND (0x32) type packets. The variable-length payload of these packets includes a single byte command type/realm, a single byte (?) command ID, and any data needed for the command.

### Set ReceiverID / ModelID

The ReceiverID is stored on the handset and passed to the transmitter module when a connection to it is established, which MUST BE prior to the module establishing a connection to the receiver. This is done using an extended CRSF_FRAMETYPE_COMMAND (0x32) type packet with command type CRSF (0x10) and command COMMAND_MODEL_SELECT_ID (0x05).

Example: Set ReceiverID to 20 (0x14) `c8 08 32 ee ea 10 05 14 crc`.

## Config Protocol

### Device Ping (0x28)

The host (handset) sends an extended frame of type CRSF_FRAMETYPE_DEVICE_PING to discover the connected device and the number of config parameters available.

```
HOST: EE 04 28 00 EA 54
EE = dest
04 = len
28 = type
00 EA = extended packet
        00 = CRSF_ADDRESS_BROADCAST (extended destination)
        EA = CRSF_ADDRESS_RADIO_TRANSMITTER (extended source)
54 = CRC
```

### Device Info / device ping response (0x29)

The slave (transmitter module, receiver, flight controller, etc) replies to device ping with an extended frame type CRSF_FRAMETYPE_DEVICE_INFO. This contains
the device display name, serial number, harware version, software version, field count, and parameter protocol version.

```
TX: EA 1C 29 EA EE                                  к.)ко
    53 49 59 49 20 46 4D 33 30 00 45 4C 52 53 00 00 SIYI FM30.ELRS..
    00 00 00 00 00 00 13 00                         ........
    CA
EA = dest
1C = len
29 = type
EA EE = extended packet dest/src
53 49 59 49 20 46 4D 33 30 00 = display name, null terminated string
45 4C 52 53 = serial number, always "ELRS" for ExpressLRS
00 00 00 00 = hardware version
00 00 00 00 = software version, all 00 for ELRSv2, 00 MAJ MIN REV for ELRSv3
13 = number of config parameters = 19
00 = parameter protocol version
CA = CRC
```

### Read Parameter (0x2C)

For each parameter reported in the Device Info packet, a read parameter extended packet is sent to read the parameter details.
The order and index of these parameters is not fixed, i.e. you can not simply request parameter 0x11 and expect it to be BIND.
The parameters are broken into chunks to support chunking of the response across multiple packets. `[field id] [field chunk]`
The first field index is 01, and the first chunk index is 00. There is also a "00" field index, which is the top level folder
item.

```
HOST: EE 06 2C EE EF 01 00 76
EE = dest
06 = len
2C = type
EE EF = extended packet dest/src
01 = config field index, this is requesting field 00
00 = chunk index, requesting chunk 00 of field 00
76 = CRC
```

### Parameter info (0x2B)

Response to 0x2C.
Payload is `[field index] [field chunks remaining] [parent] [type] [label] [opts+value] [min] [max] [default] [units]`

```
TX: EA 21 2B EA EE                               к!+ко
03 00 00 09 42 54 20 54 65 6C 65 6D 65 74 72 79 ....BT Telemetry
00 4F 66 66 3B 4F 6E 00 00 00 01 00 00          .Off;On......
4C
EA = dest
21 = len
2B = type
EA EE = extended packet dest/src
03 = config field index, parameter field 03
00 = chunks remaining, 0 chunks remaining after this one
00 = parent, this field is parented under folder (config index) 00
09 = field type
     00 = UINT8
     01 = INT8
     02 = UINT16
     03 = INT16
     04 - 07 = ??
     08 = FLOAT (4 byte)
     09 = SELECT
     0A = STRING (null-terminated)
     0B = FOLDER
     0C = INFO (display string, not configurable)
     0D = COMMAND
42 54 20 54 65 6C 65 6D 65 74 72 79 00  = field label "BT Telemetry"
4F 66 66 3B 4F 6E 00 = SELECT options "Off;On"
00 = field value = 0
00 = minimum allowed value = 0
01 = maximum allowed value = 1
00 = default value = 0
00 = units (null-terminated string)
4C = CRC
```

### Write Parameter (0x2D)

Write parameter is used to set a value or execute a command. Payload is `[field index] [value]` where value's length is dependent on the
field type. The field indexes are not fixed, you can not simply set field 11 and expect it to be BIND.
For command-type fields, the value indicates what step the command is on.

Note that ExpressLRS's implementation of the CRSF Config protocol assumes the client will reload all same-level settable parameters (not folders, items in sub folders, or commands) to account for any changes to other items caused by a Write Parameter.

```
HOST: EE 06 2D EE EF 11 01 A5                         î.-îï..¥
EE = dest
06 = len
2D = type
EE EF = extended packet dest/src
11 = config field index (this is the Bind field index for this TX)
01 = value, step 1 since this field happens to be a COMMAND type
A5 = CRC
```

For the above command, the response is a parameter info (0x2B) update, indicating the value has changed to 02 with a string message "Binding..."

```
TX: EA 1A 2B EA EE                               ê.+êî
11 00 00 0D 42 69 6E 64 00 02 C8 42 69 6E 64 69 ....Bind..ÈBindi
6E 67 2E 2E 2E 00                               ng....
66
```

# Example loading

A full config load from tx module's perspective

```
IN: EE 04 28 00 EA 54 2B C0                         о.(.кT+А
OUT: EA 1C 29 EA EE                               к.)ко
53 49 59 49 20 46 4D 33 30 00 45 4C 52 53 00 00 SIYI FM30.ELRS..
00 00 00 00 00 00 13 00                         ........
CA

IN: EE 06 2D EE EF 00 00 CB                         о.-оп..Л //CRSF_FRAMETYPE_ELRS_STATUS
OUT: EA 0A 2E EA EE                               к..ко
00 00 FA 00 00 00                               ..ъ...
76

IN: EE 06 2C EE EF 01 00 76                         о.,оп..v
OUT: EA 3E 2B EA EE                               к>+ко
01 01 00 09 50 61 63 6B 65 74 20 52 61 74 65 00 ....Packet Rate.
35 30 28 2D 31 31 37 64 62 6D 29 3B 31 35 30 28 50(-117dbm);150(
2D 31 31 32 64 62 6D 29 3B 32 35 30 28 2D 31 30 -112dbm);250(-10
38 64 62 6D 29 3B 35 30 30 28                   8dbm);500(
E5

IN: EE 06 2C EE EF 01 01 A3                         о.,оп..Ј
OUT: EA 16 2B EA EE                               к.+ко
01 00 2D 31 30 35 64 62 6D 29 00 02 00 03 00 48 ..-105dbm).....H
7A 00                                           z.
E6

IN: EE 06 2C EE EF 02 00 6B                         о.,оп..k
OUT: EA 3E 2B EA EE                               к>+ко
02 00 00 09 54 65 6C 65 6D 20 52 61 74 69 6F 00 ....Telem Ratio.
4F 66 66 3B 31 3A 31 32 38 3B 31 3A 36 34 3B 31 Off;1:128;1:64;1
3A 33 32 3B 31 3A 31 36 3B 31 3A 38 3B 31 3A 34 :32;1:16;1:8;1:4
3B 31 3A 32 00 02 00 07 00 00                   ;1:2......
29

IN: EE 06 2C EE EF 03 00 60                         о.,оп..`
OUT: EA 21 2B EA EE                               к!+ко
03 00 00 09 42 54 20 54 65 6C 65 6D 65 74 72 79 ....BT Telemetry
00 4F 66 66 3B 4F 6E 00 00 00 01 00 00          .Off;On......
4C

IN: EE 06 2C EE EF 04 00 51                         о.,оп..Q
OUT: EA 25 2B EA EE                               к%+ко
04 00 00 09 53 77 69 74 63 68 20 4D 6F 64 65 00 ....Switch Mode.
48 79 62 72 69 64 3B 57 69 64 65 00 01 00 01 00 Hybrid;Wide.....
00                                              .
BF

IN: EE 06 2C EE EF 05 00 5A                         о.,оп..Z
OUT: EA 20 2B EA EE                               к +ко
05 00 00 09 4D 6F 64 65 6C 20 4D 61 74 63 68 00 ....Model Match.
4F 66 66 3B 4F 6E 00 00 00 01 00 00             Off;On......
D4

IN: EE 06 2C EE EF 06 00 47                         о.,оп..G
OUT: EA 11 2B EA EE                               к.+ко
06 00 00 0B 54 58 20 50 6F 77 65 72 00          ....TX Power.
75

IN: EE 06 2C EE EF 07 00 4C                         о.,оп..L
OUT: EA 2A 2B EA EE                               к*+ко
07 00 06 09 4D 61 78 20 50 6F 77 65 72 00 31 30 ....Max Power.10
3B 32 35 3B 35 30 3B 31 30 30 3B 32 35 30 00 04 ;25;50;100;250..
00 04 00 6D 57 00                               ...mW.
7B

IN: EE 06 2C EE EF 08 00 25                         о.,оп..%
OUT: EA 33 2B EA EE                               к3+ко
08 00 06 09 44 79 6E 61 6D 69 63 00 4F 66 66 3B ....Dynamic.Off;
4F 6E 3B 41 55 58 39 3B 41 55 58 31 30 3B 41 55 On;AUX9;AUX10;AU
58 31 31 3B 41 55 58 31 32 00 01 00 05 00 00    X11;AUX12......
C9

IN: EE 06 2C EE EF 09 00 2E                         о.,оп...
OUT: EA 1A 2B EA EE                               к.+ко
09 00 00 0B 56 54 58 20 41 64 6D 69 6E 69 73 74 ....VTX Administ
72 61 74 6F 72 00                               rator.
30

IN: EE 06 2C EE EF 0A 00 33                         о.,оп..3
OUT: EA 22 2B EA EE                               к"+ко
0A 00 09 09 42 61 6E 64 00 4F 66 66 3B 41 3B 42 ....Band.Off;A;B
3B 45 3B 46 3B 52 3B 4C 00 05 00 06 00 00       ;E;F;R;L......
E3

IN: EE 06 2C EE EF 0B 00 38                         о.,оп..8
OUT: EA 25 2B EA EE                               к%+ко
0B 00 09 09 43 68 61 6E 6E 65 6C 00 31 3B 32 3B ....Channel.1;2;
33 3B 34 3B 35 3B 36 3B 37 3B 38 00 00 00 07 00 3;4;5;6;7;8.....
00                                              .
66

IN: EE 06 2C EE EF 0C 00 09                         о.,оп...
OUT: EA 27 2B EA EE                               к'+ко
0C 00 09 09 50 77 72 20 4C 76 6C 00 2D 3B 31 3B ....Pwr Lvl.-;1;
32 3B 33 3B 34 3B 35 3B 36 3B 37 3B 38 00 00 00 2;3;4;5;6;7;8...
08 00 00                                        ...
50

IN: EE 06 2C EE EF 0D 00 02                         о.,оп...
OUT: EA 1C 2B EA EE                               к.+ко
0D 00 09 09 50 69 74 6D 6F 64 65 00 4F 66 66 3B ....Pitmode.Off;
4F 6E 00 00 00 01 00 00                         On......
13

IN: EE 06 2C EE EF 0E 00 1F                         о.,оп...
OUT: EA 14 2B EA EE                               к.+ко
0E 00 09 0D 53 65 6E 64 20 56 54 78 00 00 C8 00 ....Send VTx..И.
BE

IN: EE 06 2C EE EF 0F 00 14                         о.,оп...
OUT: 1C EA 1A 2B EA EE                               .к.+ко
0F 00 00 0B 57 69 46 69 20 43 6F 6E 6E 65 63 74 ....WiFi Connect
69 76 69 74 79 00                               ivity.
81

IN: EE 06 2C EE EF 10 00 CD                         о.,оп..Н
OUT: EA 1A 2B EA EE                               к.+ко
10 00 0F 0D 45 6E 61 62 6C 65 20 52 78 20 57 69 ....Enable Rx Wi
46 69 00 00 C8 00                               Fi..И.
B6

IN: EE 06 2C EE EF 11 00 C6                         о.,оп..Ж
OUT: EA 10 2B EA EE                               к.+ко
11 00 00 0D 42 69 6E 64 00 00 C8 00             ....Bind..И.
9D

IN: EE 06 2C EE EF 12 00 DB                         о.,оп..Ы
OUT: EA 17 2B EA EE                               к.+ко
12 00 00 8C 42 61 64 2F 47 6F 6F 64 00 30 2F 32 ...Bad/Good.0/2
35 30 00                                        50.
27

IN: EE 06 2C EE EF 13 00 D0                         о.,оп..Р
OUT: EA 1D 2B EA EE                               к.+ко
13 00 00 0C 6D 61 73 74 65 72 20 49 53 4D 32 47 ....master ISM2G
34 00 38 32 35 65 64 38 00                      4.825ed8.
4A
```

### Binding Example

```
IN: EE 06 2D EE EF 11 01 A5                         î.-îï..¥

DEBUG: Entered binding mode at freq = 12302619

OUT: 1C EA 1A 2B EA EE                               .ê.+êî
11 00 00 0D 42 69 6E 64 00 02 C8 42 69 6E 64 69 ....Bind..ÈBindi
6E 67 2E 2E 2E 00                               ng....
66

DEBUG: Exiting binding mode

IN: EE 06 2D EE EF 11 06 F1                         î.-îï..ñ
OUT: EA 10 2B EA EE                               ê.+êî
11 00 00 0D 42 69 6E 64 00 00 C8 00             ....Bind..È.
9D
```
