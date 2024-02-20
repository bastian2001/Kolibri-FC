// https://github.com/catphish/openuav/blob/master/firmware/src/quaternion.c
// https://github.com/MartinWeigel/Quaternion/blob/master/Quaternion.c
//  Permission to use, copy, modify, and/or distribute this software for any
//  purpose with or without fee is hereby granted, provided that the above
//  copyright notice and this permission notice appear in all copies.
//
//  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
//  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
//  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
//  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
//  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
//  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
//  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

/**
 * @file    Quaternion.c
 * @brief   A basic quaternion library written in C
 * @date    2022-05-16
 */
#include "global.h"
#include <math.h>

// const f32 FAST_PI		  = PI;
// const f32 FAST_PI_2	  = PI / 2;
// const f32 FACULTY_THREE = 1. / 6.;
// const f32 FACULTY_FOUR  = 1. / 24.;
// const f32 FACULTY_FIVE  = 1. / 120.;
// const f32 FACULTY_SIX	  = 1. / 720.;
// const f32 FAST_ACOS_A	  = -0.939115566365855;
// const f32 FAST_ACOS_B	  = 0.9217841528914573;
// const f32 FAST_ACOS_C	  = -1.2845906244690837;
// const f32 FAST_ACOS_D	  = 0.295624144969963174;
/*
f32 fastAtan(f32 x) {
	// https://www.dsprelated.com/showarticle/1052.php
	if (x < 0) {
		return -fastAtan(-x);
	}
	if (x > 1) {
		return FAST_PI_2 - fastAtan(f32(1) / x);
	}
	return x / (f32(1) + f32(0.28) * x * x);
}
f32 fastAtan2(f32 fp1, f32 fp2) {
	if (fp1 == 0) // edge cases
		if (fp2 >= 0)
			return FAST_PI_2;
		else
			return -FAST_PI_2;
	if (fp1 > 0) return fastAtan(fp2 / fp1); //"Normal" case
	if (fp2 > 0)
		return fastAtan(fp2 / fp1) + FAST_PI; // less "normal" cases
	else
		return fastAtan(fp2 / fp1) - FAST_PI; // less "normal" cases
}
f32 fastCos(f32 x) {
	// taylor series approximation of cos(x) around 0, up to x^6
	// deviation: max 0.001, at +/- 0.5pi
	if (x < 0) x = -x;
	x %= FAST_PI << 1;
	f32 sign = 1;
	if (x > FAST_PI) {
		x -= FAST_PI;
		sign = -sign;
	}
	if (x > FAST_PI_2) {
		x	 = FAST_PI - x;
		sign = -sign;
	}
	if (x > FAST_PI >> 2)
		return -fastSin(x - FAST_PI_2);
	f32 out	= 1;
	f32 expo2 = x * x;
	f32 expo4 = expo2 * expo2;
	out -= expo2 >> 1;
	out += FACULTY_FOUR * expo4;
	out -= FACULTY_SIX * expo4 * expo2;
	return sign * out;
}
f32 fastSin(f32 x) {
	f32 sign = 1;
	if (x < 0) {
		x	 = -x;
		sign = -sign;
	}
	x %= FAST_PI << 1;
	if (x > FAST_PI) {
		x -= FAST_PI;
		sign = -sign;
	}
	if (x > FAST_PI_2) {
		x	 = FAST_PI - x;
		sign = -sign;
	}
	if (x > FAST_PI >> 2)
		return fastCos(x - FAST_PI_2);
	f32 out	= x;
	f32 expo2 = x * x;
	f32 expo3 = expo2 * x;
	out -= FACULTY_THREE * expo3;
	out += FACULTY_FIVE * expo3 * expo2;
	return sign * out;
}
f32 fastAcos(f32 x) {
	// https://stackoverflow.com/a/36387954/8807019
	f32 xsq = x * x;
	return FAST_PI_2 + (FAST_ACOS_A * x + FAST_ACOS_B * xsq * x) / (FAST_ACOS_C * xsq + FAST_ACOS_D * xsq * xsq + 1);
}
f32 fastAsin(f32 x) { return FAST_PI_2 - fastAcos(x); }
*/
void Quaternion_set(f32 w, f32 v1, f32 v2, f32 v3, Quaternion *output) {
	output->w    = w;
	output->v[0] = v1;
	output->v[1] = v2;
	output->v[2] = v3;
}

void Quaternion_setIdentity(Quaternion *q) {
	Quaternion_set(1, 0, 0, 0, q);
}

void Quaternion_fromAxisAngle(f32 axis[3], f32 angle, Quaternion *output) {
	// Formula from http://www.euclideanspace.com/maths/geometry/rotations/conversions/angleToQuaternion/
	output->w    = cosf(angle * .5f);
	f32 c        = sinf(angle * .5f);
	output->v[0] = axis[0] * c;
	output->v[1] = axis[1] * c;
	output->v[2] = axis[2] * c;
}

f32 Quaternion_toAxisAngle(Quaternion *q, f32 output[3]) {
	// Formula from http://www.euclideanspace.com/maths/geometry/rotations/conversions/quaternionToAngle/
	f32 angle   = acosf(q->w) * 2;
	f32 divider = sqrtf(1 - q->w * q->w);

	if (divider != 0.f) {
		// Calculate the axis
		f32 divNew = 1 / divider;
		output[0]  = q->v[0] * divNew;
		output[1]  = q->v[1] * divNew;
		output[2]  = q->v[2] * divNew;
	} else {
		// Arbitrary normalized axis
		output[0] = 1;
		output[1] = 0;
		output[2] = 0;
	}
	return angle;
}

void Quaternion_fromXRotation(f32 angle, Quaternion *output) {
	output->w    = 1;
	output->v[0] = angle / 2;
	output->v[1] = 0;
	output->v[2] = 0;
}

void Quaternion_fromYRotation(f32 angle, Quaternion *output) {
	output->w    = 1;
	output->v[0] = 0;
	output->v[1] = angle / 2;
	output->v[2] = 0;
}

void Quaternion_fromZRotation(f32 angle, Quaternion *output) {
	output->w    = 1;
	output->v[0] = 0;
	output->v[1] = 0;
	output->v[2] = angle / 2;
}

f32 Quaternion_norm(Quaternion *q) {
	return sqrtf(q->w * q->w + q->v[0] * q->v[0] + q->v[1] * q->v[1] + q->v[2] * q->v[2]);
}

void Quaternion_normalize(Quaternion *q, Quaternion *output) {

	f32 len = Quaternion_norm(q);
	if (len == 0) {
		// Serial.printf("q: %f, %f, %f, %f\n", q->w, q->v[0], q->v[1], q->v[2]);
		Quaternion_setIdentity(output);
		return;
	}
	f32 oneOverLen = 1 / len;
	Quaternion_set(
		q->w * oneOverLen,
		q->v[0] * oneOverLen,
		q->v[1] * oneOverLen,
		q->v[2] * oneOverLen,
		output);
}

void Quaternion_multiply(Quaternion *q1, Quaternion *q2, Quaternion *output) {
	Quaternion result;

	/*
	Formula from http://www.euclideanspace.com/maths/algebra/realNormedAlgebra/quaternions/arithmetic/index.htm
		 a*e - b*f - c*g - d*h
	  + i (b*e + a*f + c*h- d*g)
	  + j (a*g - b*h + c*e + d*f)
	  + k (a*h + b*g - c*f + d*e)
	*/
	result.w    = q1->w * q2->w - q1->v[0] * q2->v[0] - q1->v[1] * q2->v[1] - q1->v[2] * q2->v[2];
	result.v[0] = q1->v[0] * q2->w + q1->w * q2->v[0] + q1->v[1] * q2->v[2] - q1->v[2] * q2->v[1];
	result.v[1] = q1->w * q2->v[1] - q1->v[0] * q2->v[2] + q1->v[1] * q2->w + q1->v[2] * q2->v[0];
	result.v[2] = q1->w * q2->v[2] + q1->v[0] * q2->v[1] - q1->v[1] * q2->v[0] + q1->v[2] * q2->w;

	*output = result;
}

void Quaternion_rotate(const Quaternion *q, f32 v[3], f32 output[3]) {
	f32 result[3];

	f32 ww = q->w * q->w;
	f32 xx = q->v[0] * q->v[0];
	f32 yy = q->v[1] * q->v[1];
	f32 zz = q->v[2] * q->v[2];
	f32 wx = q->w * q->v[0];
	f32 wy = q->w * q->v[1];
	f32 wz = q->w * q->v[2];
	f32 xy = q->v[0] * q->v[1];
	f32 xz = q->v[0] * q->v[2];
	f32 yz = q->v[1] * q->v[2];

	// Formula from http://www.euclideanspace.com/maths/algebra/realNormedAlgebra/quaternions/transforms/index.htm
	// p2.x = w*w*p1.x + 2*y*w*p1.z - 2*z*w*p1.y + x*x*p1.x + 2*y*x*p1.y + 2*z*x*p1.z - z*z*p1.x - y*y*p1.x;
	// p2.y = 2*x*y*p1.x + y*y*p1.y + 2*z*y*p1.z + 2*w*z*p1.x - z*z*p1.y + w*w*p1.y - 2*x*w*p1.z - x*x*p1.y;
	// p2.z = 2*x*z*p1.x + 2*y*z*p1.y + z*z*p1.z - 2*w*y*p1.x - y*y*p1.z + 2*w*x*p1.y - x*x*p1.z + w*w*p1.z;

	result[0] = ww * v[0] + wy * v[2] * 2 - wz * v[1] * 2 +
				xx * v[0] + xy * v[1] * 2 + xz * v[2] * 2 -
				zz * v[0] - yy * v[0];
	result[1] = xy * v[0] * 2 + yy * v[1] + yz * v[2] * 2 +
				wz * v[0] * 2 - zz * v[1] + ww * v[1] -
				wx * v[2] * 2 - xx * v[1];
	result[2] = xz * v[0] * 2 + yz * v[1] * 2 + zz * v[2] -
				wy * v[0] * 2 - yy * v[2] + wx * v[1] * 2 -
				xx * v[2] + ww * v[2];

	// Copy result to output
	output[0] = result[0];
	output[1] = result[1];
	output[2] = result[2];
}

// Calculate the dot product of two 3D vectors
void Vector_dot(const f32 v1[3], const f32 v2[3], f32 *output) {
	*output = v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2];
}

// Calculate the cross product of two 3D vectors
void Vector_cross(const f32 v1[3], const f32 v2[3], f32 output[3]) {
	output[0] = v1[1] * v2[2] - v1[2] * v2[1];
	output[1] = v1[2] * v2[0] - v1[0] * v2[2];
	output[2] = v1[0] * v2[1] - v1[1] * v2[0];
}

const f32 QUATERNION_EPS = 1e-5;
const f32 ONE_MINUS_EPS  = 1 - QUATERNION_EPS;

void Quaternion_from_unit_vecs(const f32 v0[3], const f32 v1[3], Quaternion *output) {
	f32 dot;
	Vector_dot(v0, v1, &dot);

	if (dot > ONE_MINUS_EPS) {
		Quaternion_setIdentity(output);
		return;
	} else if (dot < -ONE_MINUS_EPS) {
		// Rotate along any orthonormal vec to vec1 or vec2 as the axis.
		f32 cross[3];
		f32 vTemp[3] = {1, 0, 0};
		Vector_cross(vTemp, v0, cross);
		Quaternion_fromAxisAngle(cross, (f32)PI, output);
		return;
	}

	output->w = dot + 1;
	Vector_cross(v0, v1, output->v);
	Quaternion_normalize(output, output);
}

void Quaternion_conjugate(Quaternion *q, Quaternion *output) {
	output->w    = q->w;
	output->v[0] = -q->v[0];
	output->v[1] = -q->v[1];
	output->v[2] = -q->v[2];
}