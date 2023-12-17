#include "global.h"

int16_t  bmiDataRaw[6] = {0, 0, 0, 0, 0, 0};
int16_t *gyroDataRaw;
int16_t *accelDataRaw;

#define IDLE_PERMILLE 25
#define MAX_ANGLE 45

/*
 * To avoid a lot of floating point math, fixed point math is used.
 * The gyro data is 16 bit, with a range of +/- 2000 degrees per second.
 * All data is converted to degrees per second (both RC commands as well as gyro data),
 * and calculations will be performed on a 16.16 fixed point number.
 * Additions can be performed like normal, while multiplications require
 * the numbers to be converted to 64 bit before calculation.
 * Accel data is also 16.16 bit fixed point math, just the unit is g.
 */

int16_t throttles[4];

fixedPointInt32 imuData[6];

enum {
    P,
    I,
    D,
    FF,
    S,
    iFalloff
};
fixedPointInt32 pidGains[3][7];
fixedPointInt32 angleModeP = 10;

fixedPointInt32 rollSetpoint, pitchSetpoint, yawSetpoint, rollError, pitchError, yawError, rollLast, pitchLast, yawLast, rollLastSetpoint, pitchLastSetpoint, yawLastSetpoint;
fixedPointInt64 rollErrorSum, pitchErrorSum, yawErrorSum;
fixedPointInt32 rollP, pitchP, yawP, rollI, pitchI, yawI, rollD, pitchD, yawD, rollFF, pitchFF, yawFF, rollS, pitchS, yawS;
fixedPointInt32 tRR, tRL, tFR, tFL;

uint32_t pidLoopCounter = 0;

fixedPointInt32 rateFactors[5][3];
#undef RAD_TO_DEG
const fixedPointInt32 RAD_TO_DEG = fixedPointInt32(180) / PI;
const fixedPointInt32 TO_ANGLE   = fixedPointInt32(MAX_ANGLE) / fixedPointInt32(512);
uint16_t              smoothChannels[4];

void initPID() {
    for (int i = 0; i < 3; i++) {
        pidGains[i][P].setRaw(40 << P_SHIFT);
        pidGains[i][I].setRaw(20 << I_SHIFT);
        pidGains[i][D].setRaw(100 << D_SHIFT);
        pidGains[i][FF].setRaw(0 << FF_SHIFT);
        pidGains[i][S].setRaw(0 << S_SHIFT);
        pidGains[i][iFalloff] = .998;
    }
    for (int i = 0; i < 3; i++) {
        rateFactors[0][i] = 100; // first order, center rate
        rateFactors[1][i] = 0;
        rateFactors[2][i] = 200;
        rateFactors[3][i] = 0;
        rateFactors[4][i] = 800;
    }
}

uint32_t takeoffCounter = 0;
void     pidLoop() {
    gyroGetData(bmiDataRaw);
    for (int i = 0; i < 3; i++) {
        imuData[i].setRaw(gyroDataRaw[i]); // gyro data in range of -.5 ... +.5 due to fixed point math
        imuData[i] *= 4000;                // gyro data in range of -2000 ... +2000 (degrees per second)
        // imuData[i + 3].setRaw(accelDataRaw[i]);
        // imuData[i + 3] *= 32;
    }
    imuData[AXIS_ROLL] = -imuData[AXIS_ROLL];
    imuData[AXIS_YAW]  = -imuData[AXIS_YAW];

    updateAttitude();

    if (ELRS->armed) {
        // Quad armed
        static fixedPointInt32 polynomials[5][3]; // always recreating variables is slow, but exposing is bad, hence static
        ELRS->getSmoothChannels(smoothChannels);
        // calculate setpoints
        polynomials[0][0].setRaw(((int32_t)smoothChannels[0] - 1500) << 7); //-1...+1 in fixed point notation;
        polynomials[0][1].setRaw(((int32_t)smoothChannels[1] - 1500) << 7);
        polynomials[0][2].setRaw(((int32_t)smoothChannels[3] - 1500) << 7);
        rollSetpoint  = 0;
        pitchSetpoint = 0;
        yawSetpoint   = 0;
        if (ELRS->channels[5] < 1300) {
            // Rate mode
            /* at full stick deflection, ...Raw values are either +1 or -1. That will make all the
             * polynomials also +/-1. Thus, the total rate for each axis is equal to the sum of all 5 rateFactors
             * of that axis. The center rate is the ratefactor[x][0].
             */
            for (int i = 1; i < 5; i++) {
                for (int j = 0; j < 3; j++) {
                    polynomials[i][j] = polynomials[i - 1][j] * polynomials[0][j];
                    if (polynomials[0][j] < 0) // on second and fourth order, preserve initial sign
                        polynomials[i][j] = -polynomials[i][j];
                }
            }
            for (int i = 0; i < 5; i++) {
                rollSetpoint += rateFactors[i][0] * polynomials[i][0];
                pitchSetpoint += rateFactors[i][1] * polynomials[i][1];
                yawSetpoint += rateFactors[i][2] * polynomials[i][2];
            }
        } else {
            // Angle mode
            fixedPointInt32 dRoll  = fixedPointInt32(smoothChannels[0] - 1500) * TO_ANGLE - (roll * RAD_TO_DEG);
            fixedPointInt32 dPitch = fixedPointInt32(smoothChannels[1] - 1500) * TO_ANGLE - (pitch * RAD_TO_DEG);
            rollSetpoint           = dRoll * angleModeP;
            pitchSetpoint          = dPitch * angleModeP;
            polynomials[0][2].setRaw(((int32_t)smoothChannels[3] - 1500) << 7);
            for (int i = 1; i < 5; i++) {
                polynomials[i][2] = polynomials[i - 1][2] * polynomials[0][2];
                if (polynomials[0][2] < 0)
                    polynomials[i][2] = -polynomials[i][2];
            }
            yawSetpoint = 0;
            for (int i = 0; i < 5; i++)
                yawSetpoint += rateFactors[i][2] * polynomials[i][2];
        }
        rollError  = rollSetpoint - imuData[AXIS_ROLL];
        pitchError = pitchSetpoint - imuData[AXIS_PITCH];
        yawError   = yawSetpoint - imuData[AXIS_YAW];
        if (ELRS->channels[2] > 1020)
            takeoffCounter++;
        else if (takeoffCounter < 1000) // 1000 = ca. 0.3s
            takeoffCounter = 0;         // if the quad hasn't "taken off" yet, reset the counter
        if (takeoffCounter < 1000)      // enable i term falloff (windup prevention) only before takeoff
        {
            rollErrorSum  = pidGains[0][iFalloff].multiply64(rollErrorSum);
            pitchErrorSum = pidGains[1][iFalloff].multiply64(pitchErrorSum);
            yawErrorSum   = pidGains[2][iFalloff].multiply64(yawErrorSum);
        }

        rollErrorSum += rollError;
        pitchErrorSum += pitchError;
        yawErrorSum += yawError;
        rollP   = pidGains[0][P] * rollError;
        pitchP  = pidGains[1][P] * pitchError;
        yawP    = pidGains[2][P] * yawError;
        rollI   = pidGains[0][I] * rollErrorSum;
        pitchI  = pidGains[1][I] * pitchErrorSum;
        yawI    = pidGains[2][I] * yawErrorSum;
        rollD   = pidGains[0][D] * (imuData[AXIS_ROLL] - rollLast);
        pitchD  = pidGains[1][D] * (imuData[AXIS_PITCH] - pitchLast);
        yawD    = pidGains[2][D] * (imuData[AXIS_YAW] - yawLast);
        rollFF  = pidGains[0][FF] * (rollSetpoint - rollLastSetpoint);
        pitchFF = pidGains[1][FF] * (pitchSetpoint - pitchLastSetpoint);
        yawFF   = pidGains[2][FF] * (yawSetpoint - yawLastSetpoint);
        rollS   = pidGains[0][S] * rollSetpoint;
        pitchS  = pidGains[1][S] * pitchSetpoint;
        yawS    = pidGains[2][S] * yawSetpoint;

        fixedPointInt32 rollTerm  = rollP + rollI + rollD + rollFF + rollS;
        fixedPointInt32 pitchTerm = pitchP + pitchI + pitchD + pitchFF + pitchS;
        fixedPointInt32 yawTerm   = yawP + yawI + yawD + yawFF + yawS;
#ifdef PROPS_OUT
        tRR = fixedPointInt32((smoothChannels[2] - 1000) * 2) - rollTerm + pitchTerm + yawTerm;
        tFR = fixedPointInt32((smoothChannels[2] - 1000) * 2) - rollTerm - pitchTerm - yawTerm;
        tRL = fixedPointInt32((smoothChannels[2] - 1000) * 2) + rollTerm + pitchTerm - yawTerm;
        tFL = fixedPointInt32((smoothChannels[2] - 1000) * 2) + rollTerm - pitchTerm + yawTerm;
#else
        tRR = fixedPointInt32((smoothChannels[2] - 1000) * 2) - rollTerm + pitchTerm - yawTerm;
        tFR = fixedPointInt32((smoothChannels[2] - 1000) * 2) - rollTerm - pitchTerm + yawTerm;
        tRL = fixedPointInt32((smoothChannels[2] - 1000) * 2) + rollTerm + pitchTerm + yawTerm;
        tFL = fixedPointInt32((smoothChannels[2] - 1000) * 2) + rollTerm - pitchTerm - yawTerm;
#endif
        throttles[(uint8_t)MOTOR::RR] = map(tRR.getInt(), 0, 2000, IDLE_PERMILLE * 2, 2000);
        throttles[(uint8_t)MOTOR::RL] = map(tRL.getInt(), 0, 2000, IDLE_PERMILLE * 2, 2000);
        throttles[(uint8_t)MOTOR::FR] = map(tFR.getInt(), 0, 2000, IDLE_PERMILLE * 2, 2000);
        throttles[(uint8_t)MOTOR::FL] = map(tFL.getInt(), 0, 2000, IDLE_PERMILLE * 2, 2000);
        if (throttles[(uint8_t)MOTOR::RR] > 2000) {
            int16_t diff                  = throttles[(uint8_t)MOTOR::RR] - 2000;
            throttles[(uint8_t)MOTOR::RR] = 2000;
            throttles[(uint8_t)MOTOR::FL] -= diff;
            throttles[(uint8_t)MOTOR::FR] -= diff;
            throttles[(uint8_t)MOTOR::RL] -= diff;
        }
        if (throttles[(uint8_t)MOTOR::RL] > 2000) {
            int16_t diff                  = throttles[(uint8_t)MOTOR::RL] - 2000;
            throttles[(uint8_t)MOTOR::RL] = 2000;
            throttles[(uint8_t)MOTOR::FL] -= diff;
            throttles[(uint8_t)MOTOR::FR] -= diff;
            throttles[(uint8_t)MOTOR::RR] -= diff;
        }
        if (throttles[(uint8_t)MOTOR::FR] > 2000) {
            int16_t diff                  = throttles[(uint8_t)MOTOR::FR] - 2000;
            throttles[(uint8_t)MOTOR::FR] = 2000;
            throttles[(uint8_t)MOTOR::FL] -= diff;
            throttles[(uint8_t)MOTOR::RL] -= diff;
            throttles[(uint8_t)MOTOR::RR] -= diff;
        }
        if (throttles[(uint8_t)MOTOR::FL] > 2000) {
            int16_t diff                  = throttles[(uint8_t)MOTOR::FL] - 2000;
            throttles[(uint8_t)MOTOR::FL] = 2000;
            throttles[(uint8_t)MOTOR::RL] -= diff;
            throttles[(uint8_t)MOTOR::RR] -= diff;
            throttles[(uint8_t)MOTOR::FR] -= diff;
        }
        if (throttles[(uint8_t)MOTOR::RR] < IDLE_PERMILLE * 2) {
            int16_t diff                  = IDLE_PERMILLE * 2 - throttles[(uint8_t)MOTOR::RR];
            throttles[(uint8_t)MOTOR::RR] = IDLE_PERMILLE * 2;
            throttles[(uint8_t)MOTOR::FL] += diff;
            throttles[(uint8_t)MOTOR::FR] += diff;
            throttles[(uint8_t)MOTOR::RL] += diff;
        }
        if (throttles[(uint8_t)MOTOR::RL] < IDLE_PERMILLE * 2) {
            int16_t diff                  = IDLE_PERMILLE * 2 - throttles[(uint8_t)MOTOR::RL];
            throttles[(uint8_t)MOTOR::RL] = IDLE_PERMILLE * 2;
            throttles[(uint8_t)MOTOR::FL] += diff;
            throttles[(uint8_t)MOTOR::FR] += diff;
            throttles[(uint8_t)MOTOR::RR] += diff;
        }
        if (throttles[(uint8_t)MOTOR::FR] < IDLE_PERMILLE * 2) {
            int16_t diff                  = IDLE_PERMILLE * 2 - throttles[(uint8_t)MOTOR::FR];
            throttles[(uint8_t)MOTOR::FR] = IDLE_PERMILLE * 2;
            throttles[(uint8_t)MOTOR::FL] += diff;
            throttles[(uint8_t)MOTOR::RL] += diff;
            throttles[(uint8_t)MOTOR::RR] += diff;
        }
        if (throttles[(uint8_t)MOTOR::FL] < IDLE_PERMILLE * 2) {
            int16_t diff                  = IDLE_PERMILLE * 2 - throttles[(uint8_t)MOTOR::FL];
            throttles[(uint8_t)MOTOR::FL] = IDLE_PERMILLE * 2;
            throttles[(uint8_t)MOTOR::RL] += diff;
            throttles[(uint8_t)MOTOR::RR] += diff;
            throttles[(uint8_t)MOTOR::FR] += diff;
        }
        for (int i = 0; i < 4; i++)
            throttles[i] = throttles[i] > 2000 ? 2000 : throttles[i];
        sendThrottles(throttles);
        rollLast          = imuData[AXIS_ROLL];
        pitchLast         = imuData[AXIS_PITCH];
        yawLast           = imuData[AXIS_YAW];
        rollLastSetpoint  = rollSetpoint;
        pitchLastSetpoint = pitchSetpoint;
        yawLastSetpoint   = yawSetpoint;
        if ((pidLoopCounter % bbFreqDivider) == 0)
            writeSingleFrame();
        pidLoopCounter++;
    } else {
        // Quad disarmed or RC disconnected
        // all motors off
        if (configOverrideMotors > 1000)
            for (int i = 0; i < 4; i++)
                throttles[i] = 0;
        if (ELRS->channels[9] < 1500)
            sendThrottles(throttles);
        else {
            static elapsedMillis motorBeepTimer = 0;
            if (motorBeepTimer > 500)
                motorBeepTimer = 0;
            if (motorBeepTimer < 200) {
                uint16_t motors[4] = {DSHOT_CMD_BEACON2, DSHOT_CMD_BEACON2, DSHOT_CMD_BEACON2, DSHOT_CMD_BEACON2};
                sendRaw11Bit(motors);
            } else {
                uint16_t motors[4] = {0, 0, 0, 0};
                sendRaw11Bit(motors);
            }
        }
        rollErrorSum   = 0;
        pitchErrorSum  = 0;
        yawErrorSum    = 0;
        rollLast       = 0;
        pitchLast      = 0;
        yawLast        = 0;
        takeoffCounter = 0;
    }
}