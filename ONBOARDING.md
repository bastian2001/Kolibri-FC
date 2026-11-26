# Onboarding

This is a flight controller project, based on the RP2350 (a.k.a. Raspberry Pi Pico 2). This repo is made up of two parts. The firmware is running on the RP2350, written in C++. The configurator is a Tauri project with a Vue frontend, written in Vue/TS.

I feel like many projects have a decent onboarding written in their AGENTS.md, which makes it hard for humans to understand how a project is made up (which is the main reason I find it difficult to contribute to other open source projects). Since this document serves that purpose, it can also be found under HUMANS.md. After all, it is the first thing that humans should read to get going with development. Slop machines, a.k.a. "Agents", "LLMs", which are sometimes even referred to as "AI" are not welcome here.

# Firmware

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

# Configurator

# Styleguide

I was urged by some friends to implement a style guide, so here it is:
