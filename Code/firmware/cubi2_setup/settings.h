// Cubi 2 - the settings kept in the Teensy's EEPROM.
// The SAME file is in cubi2_setup and cubi2_brain: the setup program writes
// them, the brain program reads them. Change one, copy it to the other.
#pragma once
#include <stdint.h>
#include <string.h>
#include <math.h>

#define CUBI_MAGIC   0xC0B12002UL
#define CUBI_VERSION 3

// The three drivers: X, Y, Z. Index 0, 1, 2 everywhere.
static const char AXIS_NAME[3] = {'X', 'Y', 'Z'};

// Motor torque constant of the 4006 350 KV per amp of the driver's Iq: the driver's
// Iq is amplitude-invariant, so Kt = 0.866 x 60 / (2 pi KV) = 0.0236 Nm/A
#define KT_NM_PER_A      0.0236f
// The driver's position sensor: one turn = 8192 ticks
#define TICKS_PER_TURN   8192.0f
#define TICKS_TO_RAD     (6.2831853f / TICKS_PER_TURN)

// Balance modes
enum { MODE_CORNER = 0, MODE_EDGE_X = 1, MODE_EDGE_Y = 2, MODE_EDGE_Z = 3, MODE_COUNT = 4 };
static const char *MODE_NAME[MODE_COUNT] = {"corner", "edge x", "edge y", "edge z"};

struct CubiSettings {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  // --- the drivers
  uint8_t nodeId[3];        // CAN ID of driver X, Y, Z (1, 2, 3)
  int8_t  wheelAxis[3];     // driver i turns the wheel on cube axis wheelAxis[i] (0 x, 1 y, 2 z)
  int8_t  wheelSign[3];     // +1 / -1: driver's positive = + or - cube axis
  uint8_t wheelChecked;     // 1 = the wheel test was done
  float   iqLimit;          // A, per motor (also written to the drivers)
  // --- the IMU
  float   R[9];             // cube = R * imu (row major)
  uint8_t orientChecked;    // 1 = measured on the 3 faces (else: from the model)
  float   gyroBias[3];      // rad/s, imu frame
  // --- balance
  uint8_t corner;           // 0 = corner 0 (three wheels at the bottom), 7 = the opposite one
  float   up[MODE_COUNT][3];// the balance direction (cube frame) for each mode
  float   K[MODE_COUNT][3]; // gains: angle (Nm/rad), rate (Nm s/rad), wheel (Nm s/rad)
  float   trimRate;         // 1/s, how fast the balance point is learned (0 = off)
  float   armDeg;           // start only when this close to the balance point
  float   stopDeg;          // give up beyond this
  float   wheelMaxRad;      // rad/s, stop above this wheel speed
  uint32_t crc;
};

// The IMU on the cube (from the model, cubi2.py: imuSpin 165):
//   imu +X = (-2, 1, 1)/V6   imu +Y = (0, -1, 1)/V2   imu +Z = (1, 1, 1)/V3
// R has these as COLUMNS: cube = R * imu.
static void cubiDefaultR(float *R) {
  const float a = 0.40824829f, b = 0.70710678f, c = 0.57735027f;
  const float M[9] = {-2 * a, 0.0f, c,
                       a,    -b,    c,
                       a,     b,    c};
  memcpy(R, M, sizeof(M));
}

static uint32_t cubiCrc(const CubiSettings &s) {
  const uint8_t *p = (const uint8_t *)&s;
  uint32_t h = 2166136261UL;
  for (size_t i = 0; i < offsetof(CubiSettings, crc); i++) { h ^= p[i]; h *= 16777619UL; }
  return h;
}

static void norm3(float *v) {
  float n = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  if (n > 1e-9f) { v[0] /= n; v[1] /= n; v[2] /= n; }
}

// The balance direction of each mode, from the corner direction d:
// an edge along axis a keeps only the part of d across the edge.
static void cubiUpsFromCorner(CubiSettings &s, const float *d) {
  for (int k = 0; k < 3; k++) s.up[MODE_CORNER][k] = d[k];
  for (int a = 0; a < 3; a++) {
    float *u = s.up[MODE_EDGE_X + a];
    for (int k = 0; k < 3; k++) u[k] = (k == a) ? 0.0f : d[k];
    norm3(u);
  }
}

// Gains from the model (LQR, 250 Hz, cubi2: 1.92 kg, wheel J 5.9e-4 kg m2):
// corner: I 0.035 kg m2 about the tip, center 124 mm up | edge: I 0.026, 102 mm
static void cubiDefaults(CubiSettings &s) {
  memset(&s, 0, sizeof(s));
  s.magic = CUBI_MAGIC; s.version = CUBI_VERSION; s.size = sizeof(CubiSettings);
  for (int i = 0; i < 3; i++) { s.nodeId[i] = i + 1; s.wheelAxis[i] = i; s.wheelSign[i] = 1; }
  s.iqLimit = 8.0f;
  cubiDefaultR(s.R);
  s.corner = 0;
  float d[3] = {0.5748f, 0.5853f, 0.5718f};     // corner 0 -> center of gravity (the model)
  cubiUpsFromCorner(s, d);
  const float Kc[3] = {7.82f, 0.890f, 0.000356f};
  const float Ke[3] = {7.13f, 0.775f, 0.000352f};
  for (int m = 0; m < MODE_COUNT; m++)
    for (int k = 0; k < 3; k++) s.K[m][k] = (m == MODE_CORNER) ? Kc[k] : Ke[k];
  s.trimRate = 0.2f;
  s.armDeg = 4.0f;                             // (26/10/2026) 4 deg: easy to hold by hand (the teacher)
  s.stopDeg = 20.0f;
  s.wheelMaxRad = 430.0f;                       // 4100 rpm (the drivers allow at most 4390)
}

#ifdef ARDUINO
#include <EEPROM.h>
static bool cubiLoad(CubiSettings &s) {
  EEPROM.get(0, s);
  if (s.magic == CUBI_MAGIC && s.version == CUBI_VERSION && s.size == sizeof(CubiSettings) && s.crc == cubiCrc(s))
    return true;
  cubiDefaults(s);
  return false;
}
static void cubiSave(CubiSettings &s) {
  s.crc = cubiCrc(s);
  EEPROM.put(0, s);
}
#endif
