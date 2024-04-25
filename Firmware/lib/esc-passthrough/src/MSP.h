// /*
//  * This file is part of Cleanflight and Betaflight.
//  *
//  * Cleanflight and Betaflight are free software. You can redistribute
//  * this software and/or modify this software under the terms of the
//  * GNU General Public License as published by the Free Software
//  * Foundation, either version 3 of the License, or (at your option)
//  * any later version.
//  *
//  * Cleanflight and Betaflight are distributed in the hope that they
//  * will be useful, but WITHOUT ANY WARRANTY; without even the implied
//  * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//  * See the GNU General Public License for more details.
//  *
//  * You should have received a copy of the GNU General Public License
//  * along with this software.
//  *
//  * If not, see <http://www.gnu.org/licenses/>.
//  */

// /**
//  * MSP Guidelines, emphasis is used to clarify.
//  *
//  * Each FlightController (FC, Server) MUST change the API version when any MSP command is added, deleted, or changed.
//  *
//  * If you fork the FC source code and release your own version, you MUST change the Flight Controller Identifier.
//  *
//  * NEVER release a modified copy of this code that shares the same Flight controller IDENT and API version
//  * if the API doesn't match EXACTLY.
//  *
//  * Consumers of the API (API clients) SHOULD first attempt to get a response from the MSP_API_VERSION command.
//  * If no response is obtained then client MAY try the legacy MSP_IDENT command.
//  *
//  * API consumers should ALWAYS handle communication failures gracefully and attempt to continue
//  * without the information if possible.  Clients MAY log/display a suitable message.
//  *
//  * API clients should NOT attempt any communication if they can't handle the returned API MAJOR VERSION.
//  *
//  * API clients SHOULD attempt communication if the API MINOR VERSION has increased from the time
//  * the API client was written and handle command failures gracefully.  Clients MAY disable
//  * functionality that depends on the commands while still leaving other functionality intact.
//  * that the newer API version may cause problems before using API commands that change FC state.
//  *
//  * It is for this reason that each MSP command should be specific as possible, such that changes
//  * to commands break as little functionality as possible.
//  *
//  * API client authors MAY use a compatibility matrix/table when determining if they can support
//  * a given command from a given flight controller at a given api version level.
//  *
//  * Developers MUST NOT create new MSP commands that do more than one thing.
//  *
//  * Failure to follow these guidelines will likely invoke the wrath of developers trying to write tools
//  * that use the API and the users of those tools.
//  */

// #pragma once

// /* Protocol numbers used both by the wire format, config system, and
//    field setters.
// */

// #define MSP_FEATURE_CONFIG 36
// #define MSP_SET_FEATURE_CONFIG 37

// #define MSP_REBOOT 68 // in message reboot settings

// // Betaflight Additional Commands
// #define MSP_ADVANCED_CONFIG 90
// #define MSP_SET_ADVANCED_CONFIG 91

// // Multwii original MSP commands

// #define MSP_STATUS 101    // out message         cycletime & errors_count & sensor present & box activation & current setting number
// #define MSP_MOTOR 104     // out message         motors
// #define MSP_SET_MOTOR 214 // in message          PropBalance function
// #define MSP_BOXIDS 119    // out message         get the permanent IDs associated to BOXes

// #define MSP_MOTOR_3D_CONFIG 124     // out message         Settings needed for reversible ESCs
// #define MSP_SET_MOTOR_3D_CONFIG 217 // in message          Settings needed for reversible ESCs
// #define MSP_MOTOR_CONFIG 131        // out message         Motor configuration (min/max throttle, etc)
// #define MSP_SET_MOTOR_CONFIG 222    // out message         Motor configuration (min/max throttle, etc)
// #define MSP_V2_FRAME 255            // MSPv2 payload indicator
// #define MSP_UID 160                 // out message         Unique device ID
// #define MSP_SET_4WAY_IF 245         // in message          Sets 4way interface

// uint8_t MSP_Check(uint8_t MSP_buf[], uint8_t buf_size);
