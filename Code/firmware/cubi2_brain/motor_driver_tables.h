// (19/10/2026) GENERATED from the motor driver's own protocol specs (one per firmware line).
// (21/10/2026) firmware 3.0 and 3.1 added (the specs of the driver package 3.2.0).
// The protocol hash a driver reports picks its table. Do not edit by hand.
// (5/11/2026) only the values the programs use are kept.
#pragma once
#include <stdint.h>

struct TmSpec {
  uint32_t hash;
  const char *version;
  int16_t vbus;
  int16_t errors;
  int16_t save_config;
  int16_t reset;
  int16_t calibrated;
  int16_t state;
  int16_t mode;
  int16_t ctrl_errors;
  int16_t iq_setpoint;
  int16_t iq_limit;
  int16_t iq_estimate;
  int16_t vel_limit;
  int16_t calibrate;
  int16_t idle;
  int16_t current_mode;
  int16_t can_id;
  int16_t motor_R;
  int16_t motor_L;
  int16_t pole_pairs;
  int16_t motor_I_cal;
  int16_t motor_errors;
  int16_t pos_estimate;
  int16_t vel_estimate;
  int16_t sensor_errors;
  int16_t wd_enabled;
  int16_t wd_timeout;
};

static const TmSpec TM_SPECS[] = {
  {3273002564UL, "1.3.x", 2, 7, 8, 10, 6, 14, 15, 17, 26, 28, 29, 21, 35, 36, 39, 42, 43, 44, 45, 50, 51, 52, 53, 57, 64, 66},
  {949626128UL, "1.4.x", 2, 7, 8, 10, 6, 14, 15, 17, 26, 28, 29, 21, 35, 36, 39, 42, 43, 44, 45, 50, 51, 52, 53, 57, 67, 69},
  {3667602695UL, "1.5.x", 2, 7, 8, 10, 6, 14, 15, 17, 26, 28, 29, 21, 35, 36, 39, 42, 43, 44, 45, 50, 51, 52, 53, 57, 75, 77},
  {4118115615UL, "1.6.x", 4, 9, 10, 12, 8, 15, 16, 18, 27, 29, 30, 22, 36, 37, 40, 43, 44, 45, 46, 51, 52, 53, 54, 58, 76, 78},
  {3704894695UL, "2.0.x", 4, 9, 11, 13, 8, 18, 19, 21, 30, 32, 33, 25, 39, 40, 43, 46, 48, 49, 50, 53, 54, 55, 56, 60, 94, 96},
  {3296093882UL, "2.1.x", 4, 9, 11, 13, 8, 18, 19, 21, 30, 32, 33, 25, 39, 40, 43, 46, 48, 49, 50, 53, 54, 55, 56, 60, 94, 96},
  {641680925UL, "2.3.x", 4, 9, 11, 13, 8, 18, 19, 21, 30, 32, 33, 25, 39, 40, 43, 46, 48, 49, 50, 53, 54, 55, 56, 60, 94, 96},
  {3999954334UL, "2.4.x", 4, 9, 11, 16, 8, 21, 22, 24, 33, 35, 36, 28, 42, 43, 46, 49, 51, 52, 53, 56, 57, 58, 59, 63, 97, 99},
  {1948876603UL, "2.5.x", 4, 9, 11, 16, 8, 21, 22, 24, 33, 35, 36, 28, 42, 43, 46, 49, 51, 52, 53, 56, 57, 58, 59, 63, 97, 99},
  {722468518UL, "2.6.x", 4, 9, 11, 16, 8, 21, 22, 24, 33, 35, 36, 28, 42, 43, 46, 49, 51, 52, 53, 56, 57, 58, 59, 63, 97, 99},
  {3786962419UL, "3.0.x", 4, 9, 11, 16, 8, 21, 22, 24, 33, 35, 36, 28, 42, 43, 46, 49, 51, 52, 53, 56, 57, 58, 59, 63, 97, 99},
  {957896750UL, "3.1.x", 4, 9, 11, 16, 8, 21, 22, 24, 33, 35, 36, 28, 42, 43, 46, 49, 51, 52, 53, 56, 57, 58, 59, 63, 98, 100},
  {3526126264UL, "1.6.x (alias)", 4, 9, 10, 12, 8, 15, 16, 18, 27, 29, 30, 22, 36, 37, 40, 43, 44, 45, 46, 51, 52, 53, 54, 58, 76, 78},
};
static const int TM_SPEC_COUNT = sizeof(TM_SPECS) / sizeof(TM_SPECS[0]);
