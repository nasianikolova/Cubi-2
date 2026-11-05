// Cubi 2 - the SPHERE controller (pure math, no hardware). (23/10/2026)
//
// The cube sits in the rolling sphere (radius 130 mm, center = the cube's center). The sphere touches the
// floor straight under its center, so the wheels do not balance it: they TURN it, and turning it rolls it.
//   - the attitude: the gyroscope integrated (body -> world matrix), the tilt pulled slowly to the
//     accelerometer; the heading (turn about the vertical) from the gyroscope only - 'front' resets it;
//   - the position on the floor: from the rolling (no slipping): v = R x (w x up);
//   - HOLD: the swinging damped, the heading kept; the slow speed of the wheels taken away by a small lean
//     (gravity does it - the center of gravity is a few mm off the center);
//   - MOVE: forward / back a distance, left / right an angle, with a smooth speed profile.
// World frame: z up, x = 'front' (where body x pointed, flat, at 'go sphere' or 'front'), y to the left.
// Body frame = the cube frame (x, y, z along the three wheel axes).
// The wheels get the torque T (body frame); the cube gets -T back.
// A center of gravity off the center: the sphere only rests where it hangs straight under the center. A
// stop between two such places has to be held by the wheels - they speed up. 'where' and 'swing' measure it.
#pragma once
#include "balance.h"

struct Mat3 { float m[9]; };
static inline Mat3 m3eye() { Mat3 a = {{1, 0, 0, 0, 1, 0, 0, 0, 1}}; return a; }
static inline Vec3 m3mul(const Mat3 &a, Vec3 v) {
  return v3(a.m[0] * v.x + a.m[1] * v.y + a.m[2] * v.z, a.m[3] * v.x + a.m[4] * v.y + a.m[5] * v.z, a.m[6] * v.x + a.m[7] * v.y + a.m[8] * v.z);
}
static inline Vec3 m3tmul(const Mat3 &a, Vec3 v) {      // transpose times v
  return v3(a.m[0] * v.x + a.m[3] * v.y + a.m[6] * v.z, a.m[1] * v.x + a.m[4] * v.y + a.m[7] * v.z, a.m[2] * v.x + a.m[5] * v.y + a.m[8] * v.z);
}
static inline Vec3 m3col(const Mat3 &a, int c) { return v3(a.m[c], a.m[3 + c], a.m[6 + c]); }
static inline void m3setcol(Mat3 &a, int c, Vec3 v) { a.m[c] = v.x; a.m[3 + c] = v.y; a.m[6 + c] = v.z; }
// R <- R + R [w]x dt  (w in the body frame), then made orthonormal again
static inline void m3rotBody(Mat3 &R, Vec3 w, float dt) {
  Vec3 c0 = m3col(R, 0), c1 = m3col(R, 1), c2 = m3col(R, 2);
  // R [w]x : column j of R*[w]x = R * (e_j x ... ) -> dR columns: c0' = w.z c1 - w.y c2, c1' = -w.z c0 + w.x c2, c2' = w.y c0 - w.x c1
  Vec3 d0 = sub(mul(c1, w.z), mul(c2, w.y)), d1 = sub(mul(c2, w.x), mul(c0, w.z)), d2 = sub(mul(c0, w.y), mul(c1, w.x));
  c0 = add(c0, mul(d0, dt)); c1 = add(c1, mul(d1, dt)); c2 = add(c2, mul(d2, dt));
  c0 = unit(c0); c1 = unit(sub(c1, mul(c0, dot(c0, c1)))); c2 = cross(c0, c1);
  m3setcol(R, 0, c0); m3setcol(R, 1, c1); m3setcol(R, 2, c2);
}
// R <- Rot(a) R  (a small rotation vector in the world frame)
static inline void m3rotWorld(Mat3 &R, Vec3 a) {
  for (int c = 0; c < 3; c++) { Vec3 v = m3col(R, c); m3setcol(R, c, add(v, cross(a, v))); }
  Vec3 c0 = unit(m3col(R, 0)), c1 = m3col(R, 1); c1 = unit(sub(c1, mul(c0, dot(c0, c1))));
  m3setcol(R, 0, c0); m3setcol(R, 1, c1); m3setcol(R, 2, cross(c0, c1));
}

struct Sphere {
  // --- settings
  float Rs = 0.130f;           // m, the sphere's outer radius (CAD: 130.05 mm)
  float Kd = 0.20f;            // N m s/rad: rolling damping (also the speed control)
  float Kth = 0.18f;           // N m/rad: rolling position (while moving: the distance error / Rs)
  float Kyaw = 0.06f;          // N m/rad: heading
  float Kdyaw = 0.05f;         // N m s/rad: turning damping
  float Kdump = 0.20f;         // 1/s: how fast the wheels' speed is taken away by leaning (hold only)
  float Jw = 5.9e-4f;          // kg m2, one wheel
  float tMax = 0.189f;         // N m per wheel
  float speed = 0.05f;         // m/s, forward / back
  float accel = 0.10f;         // m/s2
  float turnRate = 0.5236f;    // rad/s (30 deg/s), left / right
  float tiltTau = 1.0f;        // s, how slowly the accelerometer corrects the tilt
  // --- state
  Mat3 R = m3eye();            // body -> world
  float psi = 0;               // rad, heading (turned about the vertical since 'front')
  float px = 0, py = 0;        // m, position on the floor since 'go sphere'
  Vec3 wW = {0, 0, 0};         // rad/s, body rate in the world frame
  // --- the reference (where it should be)
  float rx = 0, ry = 0, rpsi = 0;          // position and heading it should have
  float vref = 0, wref = 0;                // m/s along (fx, fy); rad/s turning
  float fx = 1, fy = 0;                    // direction of the move
  float left = 0;                          // m (move) or rad (turn) still to go
  int moving = 0;                          // 0 hold, 1 forward/back, 2 turning, 3 resting on a navel
  // (24/10/2026) RESTING ON A NAVEL: the sphere turned (it rolls) until navel k (a corner of the cube)
  // is straight under the center, then held there. Navel k: x, y, z = bits 0, 1, 2 (0 = -x -y -z, the three
  // wheels' corner; 7 = +x +y +z). Free only where the center of gravity is straight under or over the center;
  // anywhere else the wheels have to hold it and speed up.
  float Kc = 0.25f;            // N m/rad: turning toward the navel
  int navel = -1;              // the navel it rests on (-1: none)
  bool navelReached = false;
  Vec3 upT = {0, 0, 1};        // the cube's direction that has to point up (body frame)
  Vec3 T = {0, 0, 0};                      // N m, wheel torques (body frame)

  // start: the world z = the measured up, x = body x laid flat (or body y if x is nearly vertical)
  void begin(Vec3 upBody) {
    Vec3 z = unit(upBody);
    Vec3 x = sub(v3(1, 0, 0), mul(z, z.x));
    if (len(x) < 0.3f) x = sub(v3(0, 1, 0), mul(z, z.y));
    x = unit(x); Vec3 y = cross(z, x);
    // rows of R (body -> world) are the world axes in the body frame
    R.m[0] = x.x; R.m[1] = x.y; R.m[2] = x.z; R.m[3] = y.x; R.m[4] = y.y; R.m[5] = y.z; R.m[6] = z.x; R.m[7] = z.y; R.m[8] = z.z;
    psi = 0; px = py = 0; rx = ry = 0; rpsi = 0; vref = wref = 0; left = 0; moving = 0; T = v3(0, 0, 0);
  }
  void front() { psi = 0; rpsi = 0; }       // the heading now = 'front'
  void halt() { moving = 0; vref = 0; wref = 0; left = 0; rx = px; ry = py; rpsi = psi; navel = -1; }
  void move(float meters) {                 // + forward, - back (along the heading it should have)
    halt(); fx = cosf(rpsi) * (meters >= 0 ? 1 : -1); fy = sinf(rpsi) * (meters >= 0 ? 1 : -1);
    left = fabsf(meters); moving = 1;
  }
  void turn(float rad) {                    // + left (anticlockwise seen from above), - right
    halt(); fx = rad >= 0 ? 1 : -1; left = fabsf(rad); moving = 2;
  }
  void restOn(int k) {                      // navel k straight down
    halt(); navel = k; navelReached = false;
    Vec3 c = v3((k & 1) ? 1 : -1, (k & 2) ? 1 : -1, (k & 4) ? 1 : -1);
    upT = unit(mul(c, -1.0f)); moving = 3;
  }

  Vec3 step(Vec3 acc, Vec3 gyr, Vec3 wheel, float dt) {
    // --- the attitude
    m3rotBody(R, gyr, dt);
    float an = len(acc);
    if (an > 0.8f && an < 1.2f) {
      Vec3 aW = m3mul(R, mul(acc, 1.0f / an));
      m3rotWorld(R, mul(cross(aW, v3(0, 0, 1)), dt / tiltTau));
    }
    wW = m3mul(R, gyr);
    psi += wW.z * dt;
    px += Rs * wW.y * dt; py -= Rs * wW.x * dt;            // v = Rs (w x z)
    // --- the reference
    if (moving == 1) {
      float vmax = fminf(speed, sqrtf(2.0f * accel * fmaxf(left, 0.0f)));
      vref = fminf(vref + accel * dt, vmax);
      float ds = vref * dt; if (ds > left) ds = left;
      rx += fx * ds; ry += fy * ds; left -= ds;
      if (left <= 0.0005f) { moving = 0; vref = 0; left = 0; }
    } else if (moving == 2) {
      float wmax = fminf(turnRate, sqrtf(2.0f * 1.0f * fmaxf(left, 0.0f)));   // 1 rad/s2
      wref = fminf(wref + 1.0f * dt, wmax);
      float da = wref * dt; if (da > left) da = left;
      rpsi += fx * da; left -= da;
      if (left <= 0.001f) { moving = 0; wref = 0; left = 0; }
    }
    float vx = (moving == 1) ? fx * vref : 0, vy = (moving == 1) ? fy * vref : 0;
    float wz = (moving == 2) ? fx * wref : 0;
    // --- the torque the body should feel (world frame)
    Vec3 tau = v3(0, 0, 0);
    // rolling: the rate it should have = (1/Rs) z x v ; while moving also the distance error
    float wdx = -vy / Rs, wdy = vx / Rs;
    tau.x = Kd * (wdx - wW.x); tau.y = Kd * (wdy - wW.y);
    if (moving == 1) {
      float ex = rx - px, ey = ry - py;
      tau.x += Kth * (-ey / Rs); tau.y += Kth * (ex / Rs);
    } else if (moving == 3) {
      // turn the cube so that upT points up: rotate about uW x z (world, flat); the angle limited to 0.6 rad
      Vec3 uW = m3mul(R, upT);
      Vec3 a = cross(uW, v3(0, 0, 1));
      float s = len(a), ang = atan2f(s, uW.z);
      if (s > 1e-6f) { float g = Kc * fminf(ang, 0.6f) / s; tau.x += g * a.x; tau.y += g * a.y; }
      if (ang < 0.035f) navelReached = true;                // 2 deg
      rx = px; ry = py;
    } else {
      rx = px; ry = py;                     // hold: it may settle (where its center of gravity hangs low)
      Vec3 HW = m3mul(R, mul(wheel, Jw));   // the wheels' momentum, world frame
      tau.x += Kdump * HW.x; tau.y += Kdump * HW.y;   // the body leans, gravity slows the wheels
    }
    // heading
    float e = rpsi - psi;
    tau.z = Kyaw * e + Kdyaw * (wz - wW.z);
    // --- the wheels (body frame): the body feels -T
    T = mul(m3tmul(R, tau), -1.0f);
    T.x = fmaxf(-tMax, fminf(tMax, T.x)); T.y = fmaxf(-tMax, fminf(tMax, T.y)); T.z = fmaxf(-tMax, fminf(tMax, T.z));
    return T;
  }
  Vec3 upBody() const { return v3(R.m[6], R.m[7], R.m[8]); }   // the world's up, seen from the cube
};
