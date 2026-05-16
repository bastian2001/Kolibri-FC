/**
 * @file typedefs.h
 * @brief Rust-style type definitions for fixed-width integers and floating-point numbers, to improve code readability.
 *
 * Copyright (c) 2026 Kolibri-FC contributors
 *
 * This file is part of Kolibri-FC (https://github.com/bastian2001/Kolibri-FC).
 *
 * Kolibri-FC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Kolibri-FC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Kolibri-FC. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
// Rust just has better names for these types

// deriving from uint32_t etc. would result in problems with function overloading (e.g. when using the same function for i32 variables and int literals, the compiler expects a function for int and one for i32)
typedef float f32;
typedef double f64;
typedef signed char i8;
typedef signed short i16;
typedef signed int i32;
typedef signed long long i64;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

#define CHECK_TYPE_SIZE(type, expected) static_assert((sizeof(type)) == (expected), "Size of " #type " is not as expected.")
