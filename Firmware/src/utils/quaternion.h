// Copyright (C) 2019 Martin Weigel <mail@MartinWeigel.com>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

/**
 * @file    Quaternion.h
 * @brief   A basic quaternion library written in C
 * @date    2019-11-28
 */
#pragma once
#include "fixedPointInt.h"

/**
 * Data structure to hold a quaternion.
 */
typedef struct quaternion {
	f32 v[3], w;
} Quaternion;

/**
 * Sets the given values to the output quaternion.
 */
void Quaternion_set(f32 w, f32 v1, f32 v2, f32 v3, Quaternion *output);

/**
 * Sets quaternion to its identity.
 */
void Quaternion_setIdentity(Quaternion *q);

/**
 * Set the quaternion to the equivalent of axis-angle rotation.
 * @param axis
 *      The axis of the rotation (should be normalized).
 * @param angle
 *      Rotation angle in radians.
 */
void Quaternion_fromAxisAngle(f32 axis[3], f32 angle, Quaternion *output);

/**
 * Calculates the rotation vector and angle of a quaternion.
 * @param output
 *      A 3D vector of the quaternion rotation axis.
 * @return
 *      The rotation angle in radians.
 */
f32 Quaternion_toAxisAngle(Quaternion *q, f32 output[3]);

/**
 * Set the quaternion to the equivalent a rotation around the X-axis. WARNING: assumes angle close to 0
 * @param angle
 *      Rotation angle in radians.
 */
void Quaternion_fromXRotation(f32 angle, Quaternion *output);

/**
 * Set the quaternion to the equivalent a rotation around the Y-axis. WARNING: assumes angle close to 0
 * @param angle
 *      Rotation angle in radians.
 */
void Quaternion_fromYRotation(f32 angle, Quaternion *output);

/**
 * Set the quaternion to the equivalent a rotation around the Z-axis. WARNING: assumes angle close to 0
 * @param angle
 *      Rotation angle in radians.
 */
void Quaternion_fromZRotation(f32 angle, Quaternion *output);

/**
 * Calculates the norm of a given quaternion:
 * norm = sqrt(w*w + v1*v1 + v2*v2 + v3*v3)
 */
f32 Quaternion_norm(Quaternion *q);

/**
 * Normalizes the quaternion.
 */
void Quaternion_normalize(Quaternion *q, Quaternion *output);

/**
 * Multiplies two quaternions: output = q1 * q2
 * @param q1
 *      The rotation to apply on q2.
 * @param q2
 *      The orientation to be rotated.
 */
void Quaternion_multiply(Quaternion *q1, Quaternion *q2, Quaternion *output);

/**
 * Applies quaternion rotation to a given vector.
 */
void Quaternion_rotate(const Quaternion *q, f32 v[3], f32 output[3]);

void Quaternion_from_unit_vecs(const f32 v0[3], const f32 v1[3], Quaternion *output);

/**
 * Calculates the conjugate of the quaternion: (w, -v)
 */
void Quaternion_conjugate(Quaternion *q, Quaternion *output);

extern const f32 QUATERNION_EPS;
extern const f32 ONE_MINUS_EPS;
void Vector_cross(const f32 v1[3], const f32 v2[3], f32 output[3]);