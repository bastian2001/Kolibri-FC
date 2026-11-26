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

MSP (MultiWii Serial Protocol) is used to talk to the Configurator. INAV has a [good documentation](https://inavflight.github.io/docs/8.0.1/advanced/MSP-V2/) of MSPv2, and betaflight delivers the [basics of MSP(v1)](https://betaflight.com/docs/development/MSP-Protocol-Reference-Dev). Kolibri supports MSPv1, incl. jumbo frames, as well as MSPv2. MSPv2 over v1 is also supported, and so is MSP (all versions) over CRSF. MSPv2 is used by the configurator and strongly encouraged, due to better CRC, but also a bigger command space and longer messages. Most configurator messages would not work on v1.

When data is captured in `serialLoop()` and routed to the MSP parser (`src/serialhandler/msp.cpp` and `.../msp.h`), the MSP parser first tries to assemble a packet. In case a packet is malformed (e.g. wrong characters lead to a broken CRC), the parser tries its best to find the start of a new packet. A broken packet is never accepted and thus never runs any code on the FC. Once a packet successfully passes the CRC check, it is handed over to the `processMspCmd(...)` function. That function is basically a long switch/case that executes some action when a command (sometimes called function or Fn) is received. The simplest command is `MspFn::API_VERSION`. It is part of the handshake at the beginning to see which device is attached (firmware version). Kolibri's MSP identifier is `KOLI`.

MSP functions can be anything. They can be simple getters to receive information about the FC, but they can also set settings on the FC, enter special modes, control the motors and more. They are the one method of communication between FC and configurator. This is the reason, why, if you are searching for "how is x done in this code", the `processMspCmd` is often a good starting point. If you know that the configurator can set something up, you will find an MSP function here that guides you to where the thing happens.

Kolibri is pretty strict about the request/response scheme of MSP, but sometimes, notifications are needed, which do not really have a request preceding them. This is the case with MSP Displayport (Digital OSD) ([okish documentation](https://betaflight.com/docs/development/API/DisplayPort)), debug prints (`printfIndMessage`) and the CLI. If the configurator sends a packet to the FC, it is always at least acknowledged to prevent the configurator from resending it.

## Task Manager

The task manager is currently more of a profiler. There are macros for starting and stopping a task (`TASK_START`, `TASK_END`). `tasks[TASK_XXX].debugInfo` and other fields may be used for debugging. The info you enter there is periodically fetched when you visit the _Task Manager_ page in the configurator. The gap between two runs must be measured manually, as this is mostly unimportant, often misleading and is a huge waste of CPU time.

The Task Manager measures execution time and collects both the min and the max duration, and also measures the average execution time and calling frequency. If you enter another process that has its own measurement taken, make sure to exclude its execution time from the caller (except when there's a hierarchy between the two, then the higher task also includes the execution time of the lower task).

## Fixed Point Int

Fixed point int was moved into a library, out of the main codebase, because it can also be useful in other projects. It allows calculations with fractional numbers similar to floats, except you have a fixed binary point exactly in the middle of the number. They are implemented as two classes called `fix32` (for 32 bit fixed numbers with 16.16 bits) and `fix64` (for 64 bit fixed numbers with 32.32 bits). You can use the numbers just like ints or floats, with the exception that they are not automatically cast to other types. You can use `.getf32()` or `.geti32()` for that purpose.

Fixed points were much faster than floats on the RP2040, though this advantage has mostly faded away with the RP2350's FPU. Key advantages are still faster computation, and the ability to use the interpolator and super duper fast sin/cos/atan/sqrt functions thanks to a combination of lookup tables and interpolation. `fix64` does not have all of the functionality.

## Programmable IO

Programmable IO is a feature of the Raspberry Pi Pico family, which allows you to write a few instructions to define a custom hardware interface. Most notably, this is used for (bidirectional) DShot. The datasheet of the RP2350 has these 9 instructions very well documented, and there are some examples to get you going. This feature is also used for other special interfaces, which are the custom dual SPI to the flash chip (onboard blackbox), WS2812 (Neopixel) communication, SDIO (SD blackbox) and more UARTs (regular and halfduplex).

There are 3 PIO blocks on an RP2350. Each of the 3 blocks has 4 state machines. A PIO block can store 32 instructions, which those 4 state machines have access to. These 32 instructions can be freely shared between all 4 SMs, which means you can have e.g. one SM use instructions 0-10, and the other 3 SMs all run the same larger program stored in the slots 11-31. The instructions are very IO themed: shifting in/out, moving data, looping, but no maths (except for a decrement)

## Blackbox

## OSD

The OSD uses a framebuffer (part of `OsdCanvas`) for maximum flexibility. Multiple `OsdOutput`s can subscribe to the `OsdCanvas`, and the virtual class `OsdOutput` is then implemented further for analog or MSP (digital) output. When they subscribe, their `sendFrame()` function is called once the canvas decides that it is done with drawing. `sendFrame()` is sends the frame from the canvas framebuffer to the individual output framebuffers. These outputs then take their own framebuffer and implement the sending to the devices (SPI, UART). That means that code

- regarding drawing onto the OSD lives inside `osdCanvas.cpp`. This includes all of the OSD elements.
- regarding common output methods (framebuffer copying) can be found inside `osdOutput.cpp`.
- regarding the analog MAX7456 (AT7456E) output is in `analogOsdOutput.cpp`, a class implementing `OsdOutput`.
- regarding the digital MSP output is located in `mspOsdOutput.cpp`, a class implementing `OsdOutput`.

Keep in mind that the implementations of `OsdOutput` copy the framebuffer so that they can slowly send out the data a few characters at a time (without hogging the bus and CPU) while the new frame can already be drawn by `OsdCanvas`. Size mismatches are handled gracefully (in case of mismatch draw to the top left of the destination or crop the bottom right away). The `OsdCanvas` draws the framebuffer 12x per second, or more/less often when the setting was changed. The configurator can also listen to an MSP Displayport output, which means that when you enter the OSD page and click preview it registers itself as an OSD output and then gets its own digital OSD feed to show to the user.

## CLI

The CLI is a powerful tool to speed up development of new features and features most people will not use. Especially the `set` command is quite handy, because adding a variable to this command is a one-liner (`addSetting(...)`). Unlike with other softwares, the CLI is sent over MSP to allow greater flexibility. This means though, that it is incompatible with basic UART-Terminals. CLI commands are not limited to interacting only with certain parts of the FC.

CLI commands can be created by pushing the command onto the `cliCommands` vector. Add arguments using `addXxxArg` (e.g. `addSelectionArg` for something similar to a dropdown), and define the necessary execution function using `setExecuteFunction`, and if the command can run for an extended period of time, set the abort and loop functions using `setLoopFunction` and `setAbortFunction`. The loop and execute function need to return `true` when the command is still running or `false` when it is done. Abort does not have a return value because the command cannot prevent abortion.

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
- `tasks[TASK_XXX].debugInfo`, `errorCount` etc.

# Firmware Styleguide

I was urged by some friends to implement a style guide, so here it is (incomplete, I'm writing things down as I think about them):

- variables, functions and files `camelCase`, class names `PascalCase`, `#define` and `const`: `SCREAMING_SNAKE_CASE`
- Treat acronyms like words, e.g. `OsdCanvas`. Yes, OSD is an acronym, and it may look weird at first glance. But you do get used to it quite quickly (trust me, I thought too that this would not work), and it helps get rid of any ambiguity in naming and generally makes words look more similar and identifiable.
- Use formatting from the `.vscode/settings.json` formatter string
- `if` with a single statement afterwards may be used without brackets, for and while loops must not
- Keep comments limited, just don't write completely f'ed up code. Make sure your code is understandable without them.
- Doxygen comments are mandatory for all functions that may be used globally\*. Doxygen comments must be written into the header file, unless the function only exists in the cpp.
    - Doxygen comments for functions must have the following fields: @brief, regular description, @return, @param. In case numbers have units, add them!
    - Doxygen is meant mostly for IDE hints, less for actually exporting to html or something
    - Add a short @file for each file.
- all global\* variables must have at least a short description in a comment attached to them (in the header file, not the cpp), so that that description is shown when hovering over the variable in VS Code. This must include a unit if it is not a unit-less number
- Create one header file per source file, except when there is a good reason not to. Header files use .h (TBD), implementations use .cpp. Add your header file include to `global.h` (TBD).
- `const` correctness: Use `const` when
    - a function does not alter the state of an object.
    - a pointer or reference is passed to a function whose contents are not altered.
    - Use `constexpr` whenever a function allows it.
- Think ahead: When adding functionality, consider creating a new file if you think that the functionality could justify its own file later, rather than splitting a file later down the road. Do not add unrelated code to existing files.
- Minimize the use of function pointers as much as possible.
- Don't use macros to generate function or variable names, unless you have a very good reason to do so (e.g. Task Manager's temporary variables). This is only allowed if this part of the code is deemed complete and should never ever be touched again.
- Debugging code, that is not expected to be needed in the future, must be removed. Debugging code, that sends a message via USB every now and then can stay in, but must use the debug print helpers. No debug messages may be sent when the quad is in the armed state. Debugging code, that sends data on a regular basis must be disabled and have a `#define` flag to enable it.
- Use `enum class` to not clutter up the global namespace.
- namespaces: TBD
- Do not use `auto` except in for-each loops.

\* definition of **global**:

- In case of member or static class variables: if they are public
- In case of regular globally scoped variables: if they are non-static (but comments are encouraged even if they are static)
- member functions or class-static functions: if they are public
- If they are in a namespace: They are not considered global, but you still have to write the short note.
