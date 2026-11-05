// =====================================================================
// Cubi 2 - THE COACH (pure math, no hardware). (25/10/2026)
//
// After every try the brain hands the coach its flight recorder (the last 4 s, 250 readings a second)
// and the coach says how the try went: the tilt, the rate, how often the wheels were at their limit,
// and the strongest shake in three bands (slow 0.5-4 Hz, middle 4.5-12 Hz, fast 12.5-40 Hz).
// From that it proposes AT MOST ONE change of a setting. The brain ASKS before it changes anything
// ('yes' / 'no'), and after a change it compares the next try of the same mode with the one before.
//
// The rules come from the real tries of 22-23/10/2026 (cubi2.json, testLog):
//   a fast shake 12.5-40 Hz that the torque follows (the IMU's mount, a hard floor: corner dumps 10-14)
//                                                                                     -> the shake filter there
//   a twist on the corner 5-12 Hz (all 3 wheels together, ~8 Hz: corner try 2)        -> less twist
//   a wobble 4.5-12 Hz in the tilt (the rate gain amplifies noise: tries 11-12)       -> less rate gain
//   a slow sway 0.5-4 Hz with a large tilt, wheels not at the limit                   -> more rate gain
// Only the 10 readings a second over the Wi-Fi could not show any of this: a 24 Hz shake needs
// the 250 a second of the recorder (at least 2 readings per swing).
// =====================================================================
#pragma once
#include <math.h>
#include <stdio.h>

struct CoachStats {
  float secs = 0;               // s analyzed
  float tiltRms = 0;            // deg, the tilt from the balance point
  float rateRms = 0;            // deg/s, the tipping rate (the twist on the tip not included)
  float twistRms = 0;           // deg/s, the corner's twist about the diagonal
  float sat = 0;                // 0...1, part of the time a wheel was at its limit
  float meanTq = 0;             // mNm, the wheels' average push (a balance point off)
  float fLow = 0, fMid = 0, fHigh = 0, fTwist = 0;   // Hz, the strongest frequency of each band
  float pLow = 0, pMid = 0, pHigh = 0, pTot = 0;     // the power of each band (the tipping rate)
  float pTwBand = 0, pTwTot = 0;                     // the twist: 5-12 Hz, and all of it
  float tqHigh = 0;             // 0...1, part of the wheels' torque in the fast band (the controller feeding a shake)
  float score = 0;              // smaller = calmer: tilt + 0.05 x rate + 2 x at-the-limit
};

enum CoachKind { CO_NONE = 0, CO_SHAKE, CO_RATEF, CO_TWIST, CO_KR_DOWN, CO_KR_UP };
static const char *COACH_NAME[] = {"-", "shake filter", "rate filter", "twist", "rate gain", "rate gain"};

struct CoachNow {               // the settings in use
  bool corner = true;
  float Kr = 0.6f, twist = 0.3f, rateHz = 15, shakeHz = 25;
};
struct CoachProposal { int kind = CO_NONE; float oldV = 0, newV = 0; char why[150] = ""; };

// the power of x (n readings, already windowed) at f - one Goertzel filter
static float coachPower(const float *x, int n, float fs, float f) {
  float c = 2.0f * cosf(2.0f * 3.14159265f * f / fs), s1 = 0, s2 = 0;
  for (int i = 0; i < n; i++) { float s = x[i] + c * s1 - s2; s2 = s1; s1 = s; }
  return (s1 * s1 + s2 * s2 - c * s1 * s2) / ((float)n * (float)n);
}
// the strongest frequency between f0 and f1 (0.5 Hz steps) and the band's whole power, over nx signals
static void coachBand(float *const *x, int nx, int n, float fs, float f0, float f1, float &fPeak, float &pSum) {
  fPeak = f0; pSum = 0; float best = -1;
  for (float f = f0; f <= f1 + 1e-3f; f += 0.5f) {
    float p = 0;
    for (int k = 0; k < nx; k++) p += coachPower(x[k], n, fs, f);
    pSum += p;
    if (p > best) { best = p; fPeak = f; }
  }
}
// the mean out, and the Hann window on (in place); returns the rms before the window
static float coachPrep(float *x, int n) {
  float m = 0; for (int i = 0; i < n; i++) m += x[i]; m /= n;
  float q = 0;
  for (int i = 0; i < n; i++) {
    x[i] -= m; q += x[i] * x[i];
    x[i] *= 0.5f - 0.5f * cosf(2.0f * 3.14159265f * i / (n - 1));
  }
  return q / n;                                          // the mean square
}

// rp[0..nx-1]: the tipping rate, deg/s (corner: 3 axes with the twist taken out; an edge: its axis);
// tq[0..nx-1]: the wheels' torque on the same axes, mNm (a shake the IMU only FEELS is not the controller's -
// only one the torque follows is); tw: the twist rate about the diagonal, deg/s (corner) or nullptr;
// tilt: deg. The arrays are changed.
static void coachAnalyze(float *const *rp, float *const *tq, int nx, float *tw, const float *tilt, int n, float fs,
                         float sat, float meanTq, CoachStats &s) {
  s = CoachStats();
  if (n < 64) return;
  s.secs = n / fs; s.sat = sat; s.meanTq = meanTq;
  float t2 = 0; for (int i = 0; i < n; i++) t2 += tilt[i] * tilt[i];
  s.tiltRms = sqrtf(t2 / n);
  float r2 = 0; for (int k = 0; k < nx; k++) r2 += coachPrep(rp[k], n);
  s.rateRms = sqrtf(r2);
  coachBand(rp, nx, n, fs, 0.5f, 4.0f, s.fLow, s.pLow);
  coachBand(rp, nx, n, fs, 4.5f, 12.0f, s.fMid, s.pMid);
  coachBand(rp, nx, n, fs, 12.5f, 40.0f, s.fHigh, s.pHigh);
  s.pTot = s.pLow + s.pMid + s.pHigh;
  {
    for (int k = 0; k < nx; k++) coachPrep(tq[k], n);
    float f, a, b, c;
    coachBand(tq, nx, n, fs, 0.5f, 4.0f, f, a); coachBand(tq, nx, n, fs, 4.5f, 12.0f, f, b); coachBand(tq, nx, n, fs, 12.5f, 40.0f, f, c);
    s.tqHigh = (a + b + c) > 0 ? c / (a + b + c) : 0;
  }
  if (tw) {
    s.twistRms = sqrtf(coachPrep(tw, n));
    float *t1[1] = {tw}; float f, pa, pb;
    coachBand(t1, 1, n, fs, 5.0f, 12.0f, s.fTwist, s.pTwBand);
    coachBand(t1, 1, n, fs, 0.5f, 4.5f, f, pa); coachBand(t1, 1, n, fs, 12.5f, 40.0f, f, pb);
    s.pTwTot = s.pTwBand + pa + pb;
  }
  s.score = s.tiltRms + 0.05f * s.rateRms + 2.0f * s.sat;
}

// one line on how it went (fits the 160-character lines of the brain)
static void coachLine(const CoachStats &s, char *b, int nb) {
  float tot = s.pTot > 0 ? s.pTot : 1;
  snprintf(b, nb, "how it went: tilt %.2f deg | rate %.1f deg/s | wheels at the limit %.0f %% | slow %.0f %% (%.1f Hz) middle %.0f %% (%.1f Hz) fast %.0f %% (%.0f Hz)",
           s.tiltRms, s.rateRms, s.sat * 100, 100 * s.pLow / tot, s.fLow, 100 * s.pMid / tot, s.fMid, 100 * s.pHigh / tot, s.fHigh);
}

// at most one change, the strongest reason first. Bounds keep every proposal inside what has worked.
static bool coachPropose(const CoachStats &s, const CoachNow &c, CoachProposal &p) {
  p = CoachProposal();
  if (s.secs < 1.5f || s.pTot <= 0) return false;
  float hi = s.pHigh / s.pTot, mid = s.pMid / s.pTot, lo = s.pLow / s.pTot;
  // 1. a fast shake: the shake filter onto it (or, if it is there already, a lower rate filter)
  if (hi > 0.5f && s.rateRms > 8.0f && s.tqHigh > 0.3f) {
    float f = roundf(s.fHigh);
    if (c.shakeHz <= 0 || fabsf(c.shakeHz - f) > 3.0f) {
      p.kind = CO_SHAKE; p.oldV = c.shakeHz; p.newV = f;
      if (c.shakeHz <= 0)
        snprintf(p.why, sizeof(p.why), "a fast shake at %.0f Hz (%.0f %% of the motion) - turn the shake filter on at %.0f Hz?", f, hi * 100, f);
      else
        snprintf(p.why, sizeof(p.why), "a fast shake at %.0f Hz (%.0f %% of the motion) - move the shake filter from %.0f to %.0f Hz?",
                 f, hi * 100, c.shakeHz, f);
      return true;
    }
    if (c.rateHz > 9.0f) {
      p.kind = CO_RATEF; p.oldV = c.rateHz; p.newV = fmaxf(8.0f, c.rateHz - 3.0f);
      snprintf(p.why, sizeof(p.why), "a fast shake at %.0f Hz, the shake filter is on it already - rate filter %.0f -> %.0f Hz?",
               f, p.oldV, p.newV);
      return true;
    }
  }
  // 2. the corner twisting on its tip
  if (c.corner && s.twistRms > 10.0f && s.pTwTot > 0 && s.pTwBand / s.pTwTot > 0.5f && c.twist > 0.06f) {
    p.kind = CO_TWIST; p.oldV = c.twist; p.newV = fmaxf(0.05f, roundf(c.twist * 0.6f * 100) / 100);
    snprintf(p.why, sizeof(p.why), "it twisted on its tip at %.1f Hz (%.0f deg/s) - twist %.2f -> %.2f?", s.fTwist, s.twistRms, p.oldV, p.newV);
    return true;
  }
  // 3. a wobble in the middle band: less rate gain
  if (mid > 0.5f && s.rateRms > 8.0f && c.Kr > 0.35f) {
    p.kind = CO_KR_DOWN; p.oldV = c.Kr; p.newV = fmaxf(0.3f, c.Kr * 0.85f);
    snprintf(p.why, sizeof(p.why), "a wobble at %.1f Hz (%.0f %% of the motion) - less rate gain, %.3f -> %.3f?", s.fMid, mid * 100, p.oldV, p.newV);
    return true;
  }
  // 4. a slow sway with a large tilt, wheels not at the limit: more rate gain (damping)
  if (lo > 0.6f && s.tiltRms > 0.8f && s.sat < 0.25f && c.Kr < 1.45f) {
    p.kind = CO_KR_UP; p.oldV = c.Kr; p.newV = fminf(1.5f, c.Kr * 1.15f);
    snprintf(p.why, sizeof(p.why), "a slow sway at %.1f Hz, %.1f deg - more rate gain (damping), %.3f -> %.3f?", s.fLow, s.tiltRms, p.oldV, p.newV);
    return true;
  }
  return false;
}

// =====================================================================
// (25/10/2026) DISTURBANCES: what the raw IMU (250 readings a second) says about the world around the cube.
//   a DIGITAL spike  one reading far from the ones before and after it, which agree with each other: no
//                    real motion does that in 4 ms - a loose wire on the IMU's bus or electrical noise
//                    (21/10/2026: the yellow SCL wire loose in its terminal)
//   a JOLT           a jump that lasts (2 readings or more): a knock on the table, a hand, the table moved
//   VIBRATION        a steady fast shaking of the accelerometer (a machine, steps, a loose part): its rms
// =====================================================================
struct Disturb {
  enum { EV_SPIKE = 1, EV_JOLT = 2 };
  struct Ev { int kind; float t, size; };
  float g1[3] = {0, 0, 0}, g2[3] = {0, 0, 0}, a1[3] = {0, 0, 0}, a2[3] = {0, 0, 0};
  float slow[3] = {0, 0, 0};
  int seen = 0;
  int spikes = 0, jolts = 0;
  float vibMs = 0;              // g^2, the accelerometer's fast part (over ~1 s)
  float lastJoltT = -100, lastJoltSize = 0;
  Ev ev[8]; int nEv = 0;
  float GYR_JUMP = 80.0f;       // deg/s in one reading
  float ACC_JUMP = 0.5f;        // g in one reading
  void reset() { spikes = jolts = 0; nEv = 0; lastJoltT = -100; lastJoltSize = 0; }
  void add(int kind, float t, float size) {
    if (nEv && ev[nEv - 1].kind == kind && t - ev[nEv - 1].t < 0.2f) { if (size > ev[nEv - 1].size) ev[nEv - 1].size = size; return; }
    if (kind == EV_SPIKE) spikes++; else { jolts++; lastJoltT = t; lastJoltSize = size; }
    if (nEv < 8) ev[nEv++] = {kind, t, size};
  }
  // one reading: gyro deg/s, accelerometer g, t s. The reading before is judged now (it needs its neighbor).
  void step(const float *g0, const float *a0, float t, float dt) {
    if (seen >= 2) {
      float sg = 0, sa = 0, jg = 0, ja = 0;
      bool spike = false;
      for (int i = 0; i < 3; i++) {
        float dg1 = fabsf(g1[i] - g2[i]), dg0 = fabsf(g1[i] - g0[i]), dg = fabsf(g0[i] - g2[i]);
        float da1 = fabsf(a1[i] - a2[i]), da0 = fabsf(a1[i] - a0[i]), da = fabsf(a0[i] - a2[i]);
        if (dg1 > GYR_JUMP && dg0 > GYR_JUMP && dg < GYR_JUMP * 0.3f) { spike = true; sg = fmaxf(sg, fminf(dg1, dg0)); }
        if (da1 > ACC_JUMP && da0 > ACC_JUMP && da < ACC_JUMP * 0.3f) { spike = true; sa = fmaxf(sa, fminf(da1, da0)); }
        jg = fmaxf(jg, dg1); ja = fmaxf(ja, da1);
      }
      if (spike) {
        add(EV_SPIKE, t - dt, sa > 0 ? sa : sg / 100.0f);
        for (int i = 0; i < 3; i++) { g1[i] = 0.5f * (g0[i] + g2[i]); a1[i] = 0.5f * (a0[i] + a2[i]); }   // the wrong reading out
      }
      else if (ja > ACC_JUMP || jg > GYR_JUMP) add(EV_JOLT, t - dt, ja > ACC_JUMP ? ja : jg / 100.0f);
    }
    float k = dt / 0.2f, q = 0;
    for (int i = 0; i < 3; i++) {
      if (seen == 0) slow[i] = a0[i];
      slow[i] += (a0[i] - slow[i]) * k;
      float h = a0[i] - slow[i]; q += h * h;
    }
    vibMs += (q - vibMs) * dt / 1.0f;
    for (int i = 0; i < 3; i++) { g2[i] = g1[i]; g1[i] = g0[i]; a2[i] = a1[i]; a1[i] = a0[i]; }
    if (seen < 2) seen++;
  }
  float vibMg() const { return sqrtf(vibMs) * 1000.0f; }
};
