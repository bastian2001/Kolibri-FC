#pragma once
#include global.h

#define KiB (1024U)

#define DEFINE_REALM(NAME, START, LENGTH)       \
	constexpr bool REALM_##NAME = true;         \
	constexpr u32 REALM_##NAME##_START = START; \
	constexpr size_t REALM_##NAME##_END = START + LENGTH - 1;

#define ADD_SETTING(NAME, POS, LENGTH, REALM)                                                                           \
	static_assert(REALM_##REALM, "Realm " #REALM " is not defined for setting " #NAME);                                 \
	static_assert(POS >= REALM_##REALM##_START && (POS + LENGTH - 1) <= REALM_##REALM##_END,                            \
				  "Setting " #NAME " at position " #POS " with length " #LENGTH " is out of bounds for realm " #REALM); \
	constexpr uint16_t SETTING_##NAME##_POS = POS;                                                                      \
	constexpr uint16_t SETTING_##NAME##_LENGTH = LENGTH;

DEFINE_REALM(TEST, 0, 500)

ADD_SETTING(TEST, 0, 24, TEST);

// accelerometer settings
#define SETTING_ACC_CAL "accel_calibration"
#define SETTING_ACC_FILTER_CUTOFF "accel_filter_cutoff"

// blackbox settings
#define SETTING_BB_FLAGS "blackbox_flags"
#define SETTING_BB_DIV "blackbox_freq_divider"

// GPS settings
#define SETTING_GPS_UPDATE_RATE "gps_update_rate"
#define SETTING_GPS_VEL_FILTER_CUTOFF "gps_velocity_filter_cutoff"

// magnetometer settings
#define SETTING_MAG_CAL_HARD "compass_calibration_hard"
#define SETTING_MAG_FILTER_CUTOFF "compass_filter_cutoff"

// motor and ESC settings
#define SETTING_BEEP_TONE "dshot_beep_tone"

// battery settings
#define SETTING_EMPTY_VOLTAGE "bat_empty_voltage"

// flight performance settings
#define SETTING_PID_GAINS "pid_gains"
#define SETTING_IDLE_PERMILLE "motor_idle_per_mille"
#define SETTING_IFALLOFF "pid_ifalloff"
#define SETTING_MAX_ANGLE "angle_max_angle"
#define SETTING_RATE_COEFFS "rate_coeffs"
#define SETTING_DFILTER_CUTOFF "filter_d_cutoff"
#define SETTING_GYRO_FILTER_CUTOFF "filter_gyro_cutoff"
#define SETTING_SETPOINT_DIFF_CUTOFF "setpoint_diff_cutoff"
#define SETTING_PID_BOOST_CUTOFF "pid_boost_cutoff"
#define SETTING_PID_BOOST_START "pid_boost_start"
#define SETTING_PID_BOOST_FULL "pid_boost_full"
#define SETTING_PID_VVEL "althold_pid"
#define SETTING_VVEL_FF_FILTER_CUTOFF "althold_vvel_ff_cutoff"
#define SETTING_VVEL_D_FILTER_CUTOFF "althold_vvel_d_cutoff"
#define SETTING_MAX_TARGET_HVEL "poshold_max_target_hvel"
#define SETTING_PID_HVEL "poshold_pid"
#define SETTING_HVEL_FF_FILTER_CUTOFF "poshold_hvel_ff_cutoff"
#define SETTING_HVEL_I_RELAX_FILTER_CUTOFF "poshold_hvel_irelax_cutoff"
#define SETTING_HVEL_PUSH_FILTER_CUTOFF "poshold_hvel_push_cutoff"
#define SETTING_MAX_ANGLE_BURST "poshold_max_angle_burst"
#define SETTING_ANGLE_BURST_TIME "poshold_angle_burst_time"
#define SETTING_ANGLE_BURST_COOLDOWN "poshold_angle_cooldown_time"
#define SETTING_HVEL_STICK_DEADBAND "poshold_hvel_stick_deadband"

// general settings
#define SETTING_UAV_NAME "uav_name"
#define SETTING_TIMEZONE_OFFSET "timezone_offset_mins"
#define SETTING_START_SOUND "start_wav"
#define SETTING_START_FALLBACK_RTTTL "start_sound_fallback"

#undef KiB
