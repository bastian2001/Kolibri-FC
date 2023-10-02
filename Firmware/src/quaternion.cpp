#include "global.h"

// https://github.com/MartinWeigel/Quaternion/

const fixedPointInt32 FAST_PI        = PI;
const fixedPointInt32 FAST_PI_2      = PI / 2;
const fixedPointInt32 FACULTY_FOUR   = 1. / 24.;
const fixedPointInt32 FACULTY_SIX    = 1. / 720.;
const fixedPointInt32 QUATERNION_EPS = 1. / 10000.;
const fixedPointInt32 ONE_MINUS_EPS  = fixedPointInt32::from(1) - QUATERNION_EPS;
const fixedPointInt32 FAST_ACOS_A    = -0.939115566365855;
const fixedPointInt32 FAST_ACOS_B    = 0.9217841528914573;
const fixedPointInt32 FAST_ACOS_C    = -1.2845906244690837;
const fixedPointInt32 FAST_ACOS_D    = 0.295624144969963174;

bool            debugQ = false;
fixedPointInt32 fastCos(fixedPointInt32 x) {
    // taylor series approximation of cos(x) around 0, up to x^6
    // deviation: max 0.001, at +/- 0.5pi
    if (x < 0) x = -x;
    x %= FAST_PI << 1;
    fixedPointInt32 sign = 1;
    if (x > FAST_PI) {
        x -= FAST_PI;
        sign = -sign;
    }
    if (x > FAST_PI_2) {
        x    = FAST_PI - x;
        sign = -sign;
    }
    fixedPointInt32 out   = 1;
    fixedPointInt32 expo2 = x * x;
    fixedPointInt32 expo4 = expo2 * expo2;
    out -= expo2 >> 1;
    out += FACULTY_FOUR * expo4;
    out -= FACULTY_SIX * expo4 * expo2;
    return sign * out;
}
fixedPointInt32 fastSin(fixedPointInt32 x) { return fastCos(x - FAST_PI_2); }
fixedPointInt32 fastAcos(fixedPointInt32 x) {
    // https://stackoverflow.com/a/36387954/8807019
    fixedPointInt32 xsq = x * x;
    return FAST_PI_2 + (FAST_ACOS_A * x + FAST_ACOS_B * xsq * x) / (FAST_ACOS_C * xsq + FAST_ACOS_D * xsq * xsq + 1);
}
fixedPointInt32 fastAsin(fixedPointInt32 x) { return FAST_PI_2 - fastAcos(x); }

Quaternion::Quaternion(fixedPointInt32 w, fixedPointInt32 x, fixedPointInt32 y, fixedPointInt32 z) {
    this->w = w;
    v[0]    = x;
    v[1]    = y;
    v[2]    = z;
}
Quaternion::Quaternion(fixedPointInt32 w, fixedPointInt32 v[3]) {
    this->w    = w;
    this->v[0] = v[0];
    this->v[1] = v[1];
    this->v[2] = v[2];
}
Quaternion::Quaternion(Quaternion &q) {
    w    = q.w;
    v[0] = q.v[0];
    v[1] = q.v[1];
    v[2] = q.v[2];
}
Quaternion *Quaternion::set(fixedPointInt32 w, fixedPointInt32 x, fixedPointInt32 y, fixedPointInt32 z) {
    this->w = w;
    v[0]    = x;
    v[1]    = y;
    v[2]    = z;
    return this;
}
Quaternion *Quaternion::set(fixedPointInt32 w, fixedPointInt32 v[3]) {
    this->w    = w;
    this->v[0] = v[0];
    this->v[1] = v[1];
    this->v[2] = v[2];
    return this;
}
Quaternion *Quaternion::setIdentity() {
    w    = 1;
    v[0] = 0;
    v[1] = 0;
    v[2] = 0;
    return this;
}
Quaternion *Quaternion::fromAxisAngle(fixedPointInt32 axis[3], fixedPointInt32 angle) {
    fixedPointInt32 halfAngle    = angle >> 1;
    fixedPointInt32 sinHalfAngle = fastSin(halfAngle);
    w                            = fastCos(halfAngle);
    v[0]                         = axis[0] * sinHalfAngle;
    v[1]                         = axis[1] * sinHalfAngle;
    v[2]                         = axis[2] * sinHalfAngle;
    return this;
}
fixedPointInt32 Quaternion::toAxisAngle(fixedPointInt32 out[3]) {
    fixedPointInt32 angle = fastAcos(w) * 2;
    // Serial.printf("angle: %.4f\n", angle.getFloat());
    fixedPointInt32 divider = sqrtf(1 - (w * w).getFloat());
    // Serial.printf("divider: %.4f,     ", divider.getFloat());
    // Serial.printf("w: %.4f\n", w.getFloat());
    if (divider == 0) {
        out[0] = 1;
        out[1] = 0;
        out[2] = 0;
    } else {
        out[0] = v[0] / divider;
        out[1] = v[1] / divider;
        out[2] = v[2] / divider;
    }
    return angle;
}
Quaternion *Quaternion::fromXRotation(fixedPointInt32 angle) {
    fixedPointInt32 axis[3] = {1, 0, 0};
    return fromAxisAngle(axis, angle);
}
Quaternion *Quaternion::fromYRotation(fixedPointInt32 angle) {
    fixedPointInt32 axis[3] = {0, 1, 0};
    return fromAxisAngle(axis, angle);
}
Quaternion *Quaternion::fromZRotation(fixedPointInt32 angle) {
    fixedPointInt32 axis[3] = {0, 0, 1};
    return fromAxisAngle(axis, angle);
}
fixedPointInt32 Quaternion::norm() {
    return fixedPointInt32::from(sqrtf(
        (w * w + v[0] * v[0] + v[1] * v[1] + v[2] * v[2]).getFloat()));
}
Quaternion *Quaternion::normalize() {
    fixedPointInt32 n = norm();
    w /= n;
    v[0] /= n;
    v[1] /= n;
    v[2] /= n;
    return this;
}
Quaternion Quaternion::operator*(Quaternion q) {
    Quaternion result;
    if (debugQ) Serial.printf("q1: %.4f %.4f %.4f %.4f\n", w.getFloat(), v[0].getFloat(), v[1].getFloat(), v[2].getFloat());
    if (debugQ) Serial.printf("q2: %.4f %.4f %.4f %.4f\n", q.w.getFloat(), q.v[0].getFloat(), q.v[1].getFloat(), q.v[2].getFloat());
    result.w    = w * q.w - v[0] * q.v[0] - v[1] * q.v[1] - v[2] * q.v[2];
    result.v[0] = v[0] * q.w + w * q.v[0] + v[1] * q.v[2] - v[2] * q.v[1];
    result.v[1] = w * q.v[1] - v[0] * q.v[2] + v[1] * q.w + v[2] * q.v[0];
    result.v[2] = w * q.v[2] + v[0] * q.v[1] - v[1] * q.v[0] + v[2] * q.w;
    if (debugQ) Serial.printf("q: %.4f %.4f %.4f %.4f\n", result.w.getFloat(), result.v[0].getFloat(), result.v[1].getFloat(), result.v[2].getFloat());
    return result;
}
Quaternion Quaternion::operator*=(Quaternion q) {
    *this = *this * q;
    return *this;
}
void Quaternion::rotate(fixedPointInt32 v0[3], fixedPointInt32 out[3]) const {
    fixedPointInt32 ww = w * w;
    fixedPointInt32 xx = v[0] * v[0];
    fixedPointInt32 yy = v[1] * v[1];
    fixedPointInt32 zz = v[2] * v[2];
    fixedPointInt32 wx = w * v[0];
    fixedPointInt32 wy = w * v[1];
    fixedPointInt32 wz = w * v[2];
    fixedPointInt32 xy = v[0] * v[1];
    fixedPointInt32 xz = v[0] * v[2];
    fixedPointInt32 yz = v[1] * v[2];
    out[0]             = v0[0] * (ww + xx - yy - zz) + v0[1] * (xy - wz) * 2 + v0[2] * (xz + wy) * 2;
    out[1]             = v0[0] * (xy + wz) * 2 + v0[1] * (ww - xx + yy - zz) + v0[2] * (yz - wx) * 2;
    out[2]             = v0[0] * (xz - wy) * 2 + v0[1] * (yz + wx) * 2 + v0[2] * (ww - xx - yy + zz);
}

fixedPointInt32 vectorDot(fixedPointInt32 v1[3], fixedPointInt32 v2[3]) {
    return v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2];
}

void vectorCross(fixedPointInt32 v1[3], fixedPointInt32 v2[3], fixedPointInt32 out[3]) {
    out[0] = v1[1] * v2[2] - v1[2] * v2[1];
    out[1] = v1[2] * v2[0] - v1[0] * v2[2];
    out[2] = v1[0] * v2[1] - v1[1] * v2[0];
}

Quaternion *Quaternion::setFromUnitVecs(fixedPointInt32 v0[3], fixedPointInt32 v1[3]) {
    fixedPointInt32 dot = vectorDot(v0, v1);
    if (dot > ONE_MINUS_EPS) {
        return setIdentity();
    } else if (dot < -ONE_MINUS_EPS) {
        // Rotate along any orthonormal vec to vec1 or vec2 as the axis.
        fixedPointInt32 cross[3];
        fixedPointInt32 v[3] = {1, 0, 0};
        vectorCross(v, v0, cross);
        return fromAxisAngle(cross, FAST_PI);
    }
    fixedPointInt32 w = dot + 1;
    fixedPointInt32 cross[3];
    vectorCross(v0, v1, cross);
    set(w, cross);
    return normalize();
}

Quaternion *Quaternion::conjugate() {
    v[0] = -v[0];
    v[1] = -v[1];
    v[2] = -v[2];
    return this;
}