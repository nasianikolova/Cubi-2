// Cubi 2 - the balance controller (pure math, no hardware).
//
// Everything is in the cube frame. d = the balance direction ("up" when the
// center of gravity is exactly over the tip). The IMU gives up_meas.
//   tilt vector  phi = up_meas x d        (small: tilt angle about each axis)
//   its rate     phi' = body rate (from the gyroscope)
// Wheel torques (Nm, one per cube axis):
//   T = Ka * phi + Kr * phi' + Kw * wheel speed
// The cube gets -T back from the wheels, which pushes it upright.
// Kw lets the wheels slow down. A small "learn" moves d to where the cube
// really balances (the wheels then stop drifting).
#pragma once
#include <math.h>

struct Vec3 { float x, y, z; };
static inline Vec3 v3(float x, float y, float z) { Vec3 v = {x, y, z}; return v; }
static inline Vec3 add(Vec3 a, Vec3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline Vec3 sub(Vec3 a, Vec3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
static inline Vec3 mul(Vec3 a, float k) { return v3(a.x * k, a.y * k, a.z * k); }
static inline float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline Vec3 cross(Vec3 a, Vec3 b) { return v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
static inline float len(Vec3 a) { return sqrtf(dot(a, a)); }
static inline Vec3 unit(Vec3 a) { float n = len(a); return n > 1e-9f ? mul(a, 1.0f / n) : a; }
static inline float comp(Vec3 a, int i) { return i == 0 ? a.x : i == 1 ? a.y : a.z; }
static inline void setc(Vec3 &a, int i, float v) { if (i == 0) a.x = v; else if (i == 1) a.y = v; else a.z = v; }

// The "up" estimate: gyroscope integration, pulled slowly to the accelerometer.
struct UpFilter {
  Vec3 up = {0, 0, 1};
  bool started = false;
  float tau = 0.5f;            // s, how slowly the accelerometer corrects
  void update(Vec3 acc_g, Vec3 gyr, float dt) {
    float n = len(acc_g);
    if (!started) { if (n > 0.5f) { up = unit(acc_g); started = true; } return; }
    up = sub(up, mul(cross(gyr, up), dt));          // a fixed world direction, seen from the turning cube
    if (n > 0.8f && n < 1.2f) {
      float a = dt / tau;
      up = add(mul(up, 1.0f - a), mul(unit(acc_g), a));
    }
    up = unit(up);
  }
};

struct Balance {
  // settings
  float Ka = 7.8f, Kr = 0.89f, Kw = 0.000356f;
  float tMax = 0.22f;          // Nm per wheel (current limit x Kt)
  float learn = 0.2f;          // 1/s
  float twist = 0.1f;          // part of Kr used against the twist on the tip (corner only)
  float spin = 1.0f;           // (23/10/2026) the wheels' speed along d is slowed with -spin x Kw (corner only)
  int edgeAxis = -1;           // -1: corner (3 wheels); 0/1/2: on an edge along x/y/z (1 wheel)
  float brakeK = 0.001f;       // (24/10/2026) on an edge: the two other wheels slowed gently (N m s/rad) ...
  float brakeT = 0.03f;        // ... at most this (N m) - after the lowering from the corner they turn ~2000 rpm
  Vec3 d = {0.577f, 0.577f, 0.577f};
  // outputs
  Vec3 T = {0, 0, 0};          // Nm, wheel torques along x, y, z
  Vec3 Traw = {0, 0, 0};       // (23/10/2026) the same before the limit (the shake filter works on it)
  Vec3 phi = {0, 0, 0};
  float tiltDeg = 0;

  Vec3 clampT(Vec3 t) const {
    return v3(fmaxf(-tMax, fminf(tMax, t.x)), fmaxf(-tMax, fminf(tMax, t.y)), fmaxf(-tMax, fminf(tMax, t.z)));
  }

  void step(Vec3 up, Vec3 gyr, Vec3 wheel, float dt, bool learning) {
    phi = cross(up, d);
    tiltDeg = asinf(fminf(1.0f, len(phi))) * 57.29578f;
    if (dot(up, d) < 0) tiltDeg = 180.0f - tiltDeg;
    // the body rate: across d it is the tilt rate (full Kr); along d it is the cube
    // twisting on its tip. (22/10/2026) There is no gravity in the twist, only the
    // cube's inertia, so the full Kr there made an ~8 Hz back-and-forth twist with
    // all three wheels at the limit (corner dump 2). The twist now gets only
    // twist x Kr - enough to stop the three wheels running away together.
    Vec3 gAlong = mul(d, dot(gyr, d));
    Vec3 rate = add(sub(gyr, gAlong), mul(gAlong, twist));
    Vec3 w = wheel;
    if (edgeAxis >= 0) {                       // only the wheel along the edge
      Vec3 a = v3(0, 0, 0); setc(a, edgeAxis, 1.0f);
      phi = mul(a, comp(phi, edgeAxis));
      rate = mul(a, comp(gyr, edgeAxis));
      w = mul(a, comp(wheel, edgeAxis));
    }
    // (23/10/2026) THE WHEELS' SPIN ALONG d. Across d, +Kw is right: to slow the wheels the cube leans and
    // gravity takes the speed away. Along d (the cube turning on its tip) there is no gravity: +Kw there
    // only pushed the wheels faster while the cube turned the other way, and the friction of the tip fed
    // it - after 2 minutes 2500 rpm along d and the cube turning 26 deg/s (corner dump 18). Along d the
    // wheels are now slowed directly (-spin x Kw): the cube's turn that this gives is taken away by the
    // same friction, so both go to zero.
    Vec3 wAlong = v3(0, 0, 0);
    if (edgeAxis < 0) { wAlong = mul(d, dot(w, d)); w = sub(w, wAlong); }
    Traw = add(add(add(mul(phi, Ka), mul(rate, Kr)), mul(w, Kw)), mul(wAlong, -spin * Kw));
    // (24/10/2026) on an edge the other two wheels have no job: slow them gently (the floor takes their
    // small torque - the cube lies on the whole edge, gravity holds it there)
    if (edgeAxis >= 0)
      for (int k = 0; k < 3; k++)
        if (k != edgeAxis) setc(Traw, k, fmaxf(-brakeT, fminf(brakeT, -brakeK * comp(wheel, k))));
    T = clampT(Traw);
    if (learning && learn > 0 && tiltDeg < 3.0f) {
      // where the wheels' steady speed says the true balance point is
      Vec3 wp = sub(w, mul(d, dot(w, d)));
      Vec3 th0 = mul(wp, -Kw / Ka);
      d = unit(add(d, mul(cross(d, th0), learn * dt)));
    }
  }
};
