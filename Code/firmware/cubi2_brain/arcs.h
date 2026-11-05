// =====================================================================
// Cubi 2 - THE SPHERE ON AN ARC (pure math, no hardware). (25/10/2026)
//
// The sphere is an open cage of 6 great circles (24 arcs, 8 mm strips). On an arc it is like the cube
// on an edge: across the arc it is an inverted pendulum ~130 mm high (2.6 N m per rad), along the arc
// it rolls freely. So:
//   ACROSS the arc: the edge law of balance.h (angle, rate, wheel speed). Inside the 8 mm strip the
//     sphere only rolls sideways, so it balances on the strip's EDGE (~2-3 deg across); that point is
//     learned from the wheels' speed, like balance.h learns its point.
//   ALONG the arc: a speed loop - 0 to stand, or the rolling speed of 'roll'. Standing, it settles where
//     the center of gravity is straight below along that circle (its 'rest point').
// Everything in the cube frame the IMU gives; its own offset and turn are inside the measured navels (above).
// The circles' normals: (1,0,+-1), (1,+-1,0), (0,1,+-1) / sqrt2.
// The wheels' torque T (N m, along x y z); the cube gets -T.
// Simulated in the open-cage model (8 A, the measured center of gravity): stands on 5 of the 6 circles
// (the 6th's rest point is navel 0), rolls 10 deg/s without leaving the arc; rolling 'uphill' against
// the center of gravity fills the wheels (~100-180 deg along a circle).
// =====================================================================
#pragma once
#include "balance.h"

// (4/11/2026) THE ARCS COME FROM THE 8 NAVELS - nothing of one cube is written here.
// Each of the 6 circles passes through 4 navels. When the sphere hangs still from each navel ('zero hang N'),
// the 8 navel points are known - and from them the 6 circles, exactly as the sensor sees them: its own small
// turn, its offset and the center of gravity are already inside the measured points.
static Vec3 circleAxis[6];        // the axis of each circle, as the sensor sees it
static float circleMiddle[6];     // rad: where the middle line of each circle is, across it, as the sensor sees it
static Vec3 navelDown[8];         // where 'down' points when the sphere stands on navel j, as measured

// the circles and the navels of the perfect sphere (the drawing)
static Vec3 circleAxisInDrawing(int k) {
  static const float N[6][3] = {{1, 0, 1}, {1, 0, -1}, {1, 1, 0}, {1, -1, 0}, {0, 1, 1}, {0, 1, -1}};
  return unit(v3(N[k][0], N[k][1], N[k][2]));
}
static Vec3 navelDownInDrawing(int j) { return unit(v3((j & 1) ? 1 : -1, (j & 2) ? 1 : -1, (j & 4) ? 1 : -1)); }

// the 6 circles from the 8 measured navel points (navelUp[j]: the balance point of navel j)
static void circlesFromNavels(const Vec3 *navelUp) {
  for (int j = 0; j < 8; j++) navelDown[j] = mul(unit(navelUp[j]), -1.0f);
  for (int k = 0; k < 6; k++) {
    Vec3 drawingAxis = circleAxisInDrawing(k);
    int onCircle[4], count = 0;
    for (int j = 0; j < 8 && count < 4; j++)
      if (fabsf(dot(navelDownInDrawing(j), drawingAxis)) < 0.1f && (7 - j) > j) {
        onCircle[count++] = j; onCircle[count++] = 7 - j;      // a navel and the one opposite it (all 3 signs turned)
      }
    // two lines across the circle, each from a navel to the opposite one: the circle's axis is across both
    Vec3 firstLine = sub(navelDown[onCircle[0]], navelDown[onCircle[1]]);
    Vec3 secondLine = sub(navelDown[onCircle[2]], navelDown[onCircle[3]]);
    Vec3 axis = unit(cross(firstLine, secondLine));
    if (dot(axis, drawingAxis) < 0) axis = mul(axis, -1.0f);
    float middle = 0;
    for (int i = 0; i < 4; i++) middle += asinf(fmaxf(-1.0f, fminf(1.0f, dot(navelDown[onCircle[i]], axis))));
    circleAxis[k] = axis;
    circleMiddle[k] = middle / 4.0f;
  }
}

struct ArcCtl {
  // settings (the corner's LQR weights for the Cubi 2 of the guides: 2.055 kg, 0.046 kg m2 about the contact)
  float Ka = 8.31f, Kr = 1.01f, Kw = 0.000359f;
  float Kv = 0.6f;               // N m s/rad: the speed loop along the arc
  float learn = 0.5f;            // 1/s: how fast the point across is learned
  float tMax = 0.19f;            // N m per wheel
  float rollRate = 10.0f / 57.29578f;   // rad/s, 'roll'
  // state
  int circle = -1;               // 0..5
  Vec3 m = {0, 0, 0};            // the circle's normal (IMU frame)
  float sStar = 0;               // rad: across, where it balances (learned)
  float sStar0 = 0;              // rad: where it started (the learned one stays within learnRange of it)
  float s = 0, rate = 0;         // across now (rad), its rate
  float travelled = 0;           // rad along the circle since the last 'roll' started
  float pos = 0;                 // rad along the circle since the take-over (it starts near its rest point)
  float Kp = 0.8f;               // 1/s: standing, how fast it rolls back to where it started
  bool back = false;             // 'roll': on the way back
  float target = 0;              // rad to roll (0 = stand)
  bool rolling = false;
  float t = 0;                   // s since the take-over
  Vec3 T = {0, 0, 0}, Traw = {0, 0, 0};
  Vec3 cPrev = {0, 0, 0};

  // where it starts to balance across circle k. Inside the 8 mm strip it only rolls, so it stands on one of
  // the strip's two edges: the one on the side its middle line leans to. When the middle line leans less than
  // half a degree, the navels cannot tell the side (the sensor's own offset and the center of gravity look the
  // same in them): then it starts on the middle line. Either way it finds the edge itself, learning from the
  // wheels - within half the strip and 2 deg more, on either side of where it started.
  static constexpr float sphereRadius = 0.13005f;
  static constexpr float stripHalfAngle = 0.004f / sphereRadius;     // rad, half of the 8 mm strip
  static constexpr float learnRange = stripHalfAngle + 0.035f;       // rad, around the middle line
  static constexpr float clearLean = 0.5f / 57.29578f;              // rad: from this lean on, the side is clear
  static float balancePointFor(int k) {
    float middle = circleMiddle[k];
    if (fabsf(middle) < clearLean) return middle;
    return middle + (middle > 0 ? stripHalfAngle : -stripHalfAngle);
  }

  // the circle the sphere is on now (the one nearest to 'down') and how far it is from its point
  static int nearest(Vec3 up, float &off) {
    Vec3 d = mul(up, -1.0f); int best = -1; float bo = 9;
    for (int k = 0; k < 6; k++) {
      float s1 = asinf(fmaxf(-1.0f, fminf(1.0f, dot(d, circleAxis[k]))));
      float o = fabsf(s1 - balancePointFor(k));
      if (o < bo) { bo = o; best = k; }
    }
    off = bo; return best;
  }

  void begin(int k, Vec3 up) {
    circle = k; m = circleAxis[k]; sStar = sStar0 = balancePointFor(k);
    travelled = 0; target = 0; rolling = false; t = 0; pos = 0;
    Vec3 d = mul(up, -1.0f); cPrev = unit(sub(d, mul(m, dot(d, m))));
  }
  void roll(float rad) { target = rad; travelled = 0; back = false; rolling = fabsf(rad) > 1e-3f; }
  void halt() { rolling = false; target = 0; }

  // one control step; returns true when a 'roll' has just finished
  bool step(Vec3 up, Vec3 gyr, Vec3 wheel, float dt) {
    t += dt;
    Vec3 d = mul(up, -1.0f);
    float dm = dot(d, m);
    s = asinf(fmaxf(-1.0f, fminf(1.0f, dm)));
    Vec3 c = unit(sub(d, mul(m, dm)));         // where it touches the circle
    Vec3 ta = cross(m, c);                     // tipping across the arc happens about ta
    rate = dot(gyr, ta);
    // how far it rolled along the circle (the contact moves along the circle in the body)
    float step = atan2f(dot(cross(cPrev, c), m), dot(cPrev, c));
    travelled += step; pos += step; cPrev = c;
    bool done = false;
    float wStar = 0;                           // rad/s along the circle (the contact's travel)
    // 'roll A': out A along the circle and back to where it started. Off its rest point gravity pulls it
    // back and the wheels hold it, getting faster all the time it is off - so it does not stay out
    // there: an out-and-back trip, then 'roll -A' the other way gives that speed back.
    if (rolling) {
      float goal = back ? 0.0f : target;
      float left = goal - travelled;
      if (fabsf(left) < 0.5f / 57.29578f || (back ? left * target > 0 : left * target < 0)) {
        if (!back) back = true; else { rolling = false; done = true; }
      } else wStar = (left > 0 ? 1.0f : -1.0f) * rollRate;
    } else {
      // standing: back to where it started (its rest point), slowly
      wStar = fmaxf(-0.5f * rollRate, fminf(0.5f * rollRate, -Kp * pos));
    }
    // the sign of 'along': rolling forward = the contact moving along +ta in the body = the body turning
    // about -m (seen from the body the floor point moves the other way)
    float wAlong = dot(gyr, m);
    float across = -(Ka * (s - sStar) + Kr * rate) - Kw * dot(wheel, ta);   // body torque about ta
    float along = -Kv * (wAlong - (-wStar));                                   // body torque about m
    // learn the point across - but not near a navel: there the other circles' strips and the navel's
    // hub carry it, and what the wheels say is not about this arc's edge
    bool nearNavel = false;
    for (int j = 0; j < 8 && !nearNavel; j++) {
      nearNavel = dot(d, navelDown[j]) > 0.985f;                               // within ~10 deg
    }
    if (t > 0.2f && !nearNavel) sStar -= learn * dt * (Kw / Ka) * dot(wheel, ta);
    sStar = fmaxf(sStar0 - learnRange, fminf(sStar0 + learnRange, sStar));
    Vec3 Tbody = add(mul(ta, across), mul(m, along));
    Traw = mul(Tbody, -1.0f);
    T = clampKeep(Traw);
    return done;
  }
  Vec3 clampKeep(Vec3 v) const {               // the strongest wheel at its limit, the direction kept
    float mx = fmaxf(fabsf(v.x), fmaxf(fabsf(v.y), fabsf(v.z)));
    return mx > tMax ? mul(v, tMax / mx) : v;
  }
  float acrossDeg() const { return s * 57.29578f; }
};
