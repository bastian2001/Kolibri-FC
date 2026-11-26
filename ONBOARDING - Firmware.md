# Firmware Onboarding

The first three subchapters explain the general concept of what runs where and how. After that, you will find explanations of some processes/functions.

## Entry point

main.cpp serves as the entry point. It houses the `setup()` and `loop()`, as well as `setup1()` and `loop1()` functions. They work identically to Arduino's `setup()` and `loop()`, except that the former two run on core 0, while the latter two run on core 1. Generally, stuff that runs on a core is also set up on that core, though this is kinda not important, except in a few cases like registering interrupt handlers. It is a good idea to check out what processes the code is made up of and how they are running on the CPUs.

## Cores

Core 0 does all of the less frequent stuff, that is not immediately tied to flying. That means, core 0 runs e.g.

- Speaker
- Blackbox saving
- Magnetometer and baro updates
- OSD
- Serial (UART) communication, including RX/ELRS and GPS

Core 1 does the stuff that needs more timing precision and runs fast and frequently: It runs the PID loop, IMU and ESC communication.

Many functions employ tactics to keep each run short, so as to not stall the CPUs for too long and let other functions achieve lower latencies for their actions. Some will be explained in further sections in this document. Many times, a state machine (simple switch/case with an enum of states) is used to split up workload over multiple runs. Other times, the maximum amount of data reception or transmission is limited.

## RTOS

Currently, Kolibri uses neither an RTOS (Real-time operating system) nor a manual scheduler. All functions have their own timing internally, and when called, they check whether they should actually run. I am considering to adopt FreeRTOS or at least some sort of scheduler for the future though. Especially for interrupting purposes, an RTOS seems like a solid choice.

## Serial data capture

`src/serial.cpp` > `serialLoop()` goes round robin through all the serial ports (one serial port per call) and captures their data (up to 16 characters per call). Each serial port has flags (`serial.functions()` getter) attached to it, that define what handlers the captured bytes are routed to. Sometimes the data is handled right away (e.g. MSP), other times it is just put into a ring buffer which another function reads (e.g. GPS).

## MSP

MSP (MultiWii Serial Protocol) is used to talk to the Configurator. INAV has a [good documentation](https://inavflight.github.io/docs/8.0.1/advanced/MSP-V2/) of MSPv2, and betaflight delivers the [basics of MSP(v1)](https://betaflight.com/docs/development/MSP-Protocol-Reference-Dev). Kolibri supports MSPv1, incl. jumbo frames, as well as MSPv2. MSPv2 over v1 is also supported, and so is MSP over CRSF. MSPv2 is used by the configurator and strongly encouraged, due to better CRC, but also a bigger command space and longer messages. Most configurator messages would not work on v1.

When data is captured in `serialLoop()` and routed to the MSP parser (`src/serialhandler/msp.cpp` and `.../msp.h`), the MSP parser first tries to assemble a packet. In case a packet is malformed (e.g. wrong characters lead to a broken CRC), the parser tries its best to find the start of a new packet. A broken packet is never accepted and thus never runs any code on the FC. Once a packet successfully passes the CRC check, it is handed over to the `processMspCmd(...)` function. That function is basically a long switch/case that executes some action when a command (sometimes called function or Fn) is received. The simplest command is `MspFn::API_VERSION`. It is part of the handshake at the beginning to see which device is attached (firmware version). Kolibri's MSP identifier is `KOLI`.

MSP functions can be anything. They can be simple getters to receive information about the FC, but they can also set settings on the FC, enter special modes, control the motors and more. They are the one method of communication between FC and configurator. Kolibri is pretty strict about the request/response scheme of MSP, but sometimes, notifications are needed, which do not really have a request preceding them. This is the case with MSP Displayport (Digital OSD) ([okish documentation](https://betaflight.com/docs/development/API/DisplayPort)), debug prints (`printfIndMessage`) and the CLI. If the configurator sends a packet to the FC, it is always at least acknowledged to prevent the configurator from resending it.

## Task Manager

## Fixed Point Int

## Programmable IO

## Blackbox

## OSD

## CLI

## Sensor Fusion

## PID

## Typedefs

This project uses Rust-style typedefs for number types. This ensures that one thinks about the size of variables ahead of time by making their size more apparent and using shorter type names. That prevents overflow and wasted space. The letter at the beginning (u, i, f) tells you whether something is an unsigned int (u), a signed int (i), or a floating point number (f). The number tells you the size, e.g. 8 for 8 bits (-128..127, 0..255).

In a similar fashion, fixed point integers exist. [See above](#fixed-point-int) for how they work. These use the typenames `fix32` and `fix64`.

## Debugging

Debugging can be done in a few ways:

- Print messages over USB Serial: Use `DEBUG_PRINTF` and its friends to include the file and line number in your print message.
- Print messages to the configurator: When the configurator is connected, you cannot attach a second listener to listen to arbitrary non-MSP data. Use `printfIndMessage` and `printIndMessage` (print indication message, may be renamed at some point) to print a log entry into the configurator log. There is no way to print a message to the JS console (except technically an MSP Error frame, or unknown MSP frame).
- Using "proper" debug techniques such as Serial Wire Debug (SWD) for halting the program, inspecting variables etc. Take special care of Heisen-bugs in this case, e.g. a reboot due to the watchdog timer, or data corruption due to discarded data.
- Debug LED

# Firmware Styleguide

I was urged by some friends to implement a style guide, so here it is (incomplete, I'm writing things down as I think about them):

- variables and functions `camelCase`, class names `PascalCase`, `#define` and `const`: `SCREAMING_SNAKE_CASE`
- use formatting from the `.vscode/settings.json` formatter string
- `if` with a single statement afterwards can be used without brackets, for and while loops can not
- keep comments limited, just don't write completely f'ed up code
- Doxygen comments are mandatory for all functions that may be used globally\*. Doxygen comments must be written into the header file, unless the function only exists in the cpp.
    - Doxygen comments for functions must have the following fields: @brief, regular description, @return, @param. In case numbers have units, add them!
    - Doxygen is meant mostly for IDE hints, less for actually exporting to html or something
    - Add a short @file for each file.
- all global\* variables must have at least a short description in a comment attached to them, so that that description is shown when hovering over the variable in VS Code. This must include a unit if it is not a unit-less number
- one header file per source file, except when there is a good reason not to. Header files use .h (TBD), implementations use .cpp
- `const` correctness: Use `const` when
    - a function does not alter the state of an object.
    - a pointer or reference is passed to a function whose contents are not altered.
    - Use `constexpr` whenever a function allows it.
- Think ahead: When adding functionality, consider creating a new file if you think that the functionality could justify its own file later, rather than splitting a file later down the road. Do not add unrelated code to existing files.
- Minimize the use of function pointers as much as possible.
- Debugging code, that is not expected to be needed in the future, must be removed. Debugging code, that sends a message via USB every now and then can stay in, but must use the debug print helpers. No debug messages may be sent when the quad is in the armed state. Debugging code, that sends data on a regular basis must be disabled and have a `#define` flag to enable it.
- Use `enum class` to not clutter up the global namespace.
- namespaces: TBD

\* definition of **global**:

- In case of member or static class variables: if they are public
- In case of regular globally scoped variables: if they are non-static (but comments are encouraged even if they are static)
- member functions or class-static variables: if they are public
