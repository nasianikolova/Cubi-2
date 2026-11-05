// =====================================================================
// Cubi 2 - 'cg': WHERE THE CENTER OF GRAVITY IS, measured in the sphere by the wheels. (24/10/2026)
//
// The sphere is turned onto each of the 8 navels and held still there by the wheels. Held still, the wheels
// give exactly the gravity torque:   Jw x (the wheels' acceleration) = m (e x g)   (g seen from the cube).
// One navel gives 2 of the 3 components of e (the center of gravity's offset from the sphere's center);
// the 8 together, least squares -> e. Between the navels the wheels are slowed (the sphere holds and leans).
// For the sphere to rest by itself on all 8 navels, e must be ~0 (a tenth of a mm).
// Resolution: 0.1 mm makes the wheels accelerate ~30 rpm/s - easy to see. Above ~2 mm the wheels may get
// too fast before a navel settles; that navel is left out.
// =====================================================================
#pragma once
#include "sphere.h"

// (5/11/2026) the line that shows the center of gravity (e in m, cube frame); n = how many navels 'cg' measured
static void cgResult(Vec3 e, int n, char *out, int outLen) {
  int p = snprintf(out, outLen, "center of gravity %+.2f %+.2f %+.2f mm (cube frame), %.2f mm from the center", e.x * 1000, e.y * 1000, e.z * 1000, len(e) * 1000);
  if (n > 0 && p < outLen) snprintf(out + p, outLen - p, " (%d navels)", n);
}

struct CgMeasure {
  float m, Jw;                          // kg (the whole sphere), kg m2 (one wheel)
  bool active = false, started = false;
  int k = 0, phase = 2, used = 0, n = 0;
  float t = 0, still = 0, st = 0, stt = 0;
  Vec3 sw = {0, 0, 0}, stw = {0, 0, 0}, up = {0, 0, 0};
  float N[9], r[3];
  Vec3 e = {0, 0, 0};

  CgMeasure(float mass, float jw) : m(mass), Jw(jw) {}
  void start() {
    active = true; started = false; k = 0; phase = 2; t = 0; still = 0; used = 0;
    for (int i = 0; i < 9; i++) N[i] = 0;
    r[0] = r[1] = r[2] = 0;
  }
  void stop() { active = false; started = false; }

  // one loop step; returns true when 'out' holds a line to show
  bool step(Sphere &sph, Vec3 gyr, Vec3 w, float dt, char *out, int outLen) {
    float wmax = fmaxf(fabsf(w.x), fmaxf(fabsf(w.y), fabsf(w.z)));
    t += dt;
    if (phase == 0) {                                   // turning onto navel k, then still for 0.3 s
      if (sph.moving != 3 || sph.navel != k) sph.restOn(k);
      still = (t > 1.0f && len(gyr) < 0.05f) ? still + dt : 0;
      if (still > 0.3f) { phase = 1; t = 0; n = 0; st = stt = 0; sw = stw = up = v3(0, 0, 0); return false; }
      if (t > 10.0f || wmax > 300.0f) {
        snprintf(out, outLen, "navel %d: %s - left out", k, wmax > 300.0f ? "the wheels got too fast first" : "did not settle");
        sph.halt(); phase = 2; t = 0; return true;
      }
      return false;
    }
    if (phase == 1) {                                   // measuring: a straight line through the wheels' speeds
      n++; st += t; stt += t * t; sw = add(sw, w); stw = add(stw, mul(w, t));
      up = add(up, sph.upBody());
      if ((t >= 0.6f || wmax > 300.0f) && n >= 30) {
        float d = n * stt - st * st;
        Vec3 sl = mul(sub(mul(stw, (float)n), mul(sw, st)), 1.0f / d);
        Vec3 g = mul(unit(up), -9.81f);
        float A[9] = {0, m * g.z, -m * g.y, -m * g.z, 0, m * g.x, m * g.y, -m * g.x, 0};   // -m [g]x : A e = m (e x g)
        Vec3 b = mul(sl, Jw);
        for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) N[3 * i + j] += A[i] * A[j] + A[3 + i] * A[3 + j] + A[6 + i] * A[6 + j];
        r[0] += A[0] * b.x + A[3] * b.y + A[6] * b.z; r[1] += A[1] * b.x + A[4] * b.y + A[7] * b.z; r[2] += A[2] * b.x + A[5] * b.y + A[8] * b.z;
        used++;
        snprintf(out, outLen, "navel %d: wheels %+.0f %+.0f %+.0f rpm/s", k, sl.x * 9.549f, sl.y * 9.549f, sl.z * 9.549f);
        sph.halt(); phase = 2; t = 0; return true;
      }
      return false;
    }
    // phase 2: holding, the wheels slow down (the sphere leans), then the next navel
    if ((wmax < 60.0f || t > 12.0f) && t > 0.2f) {
      if (started) k++;                                 // (the first pass only waits for the wheels)
      started = true;
      if (k >= 8) { finish(out, outLen); return true; }
      phase = 0; t = 0; still = 0;
    }
    return false;
  }

  void finish(char *out, int outLen) {
    active = false; started = false;
    if (used < 3) { snprintf(out, outLen, "cg: only %d navels measured - not enough", used); return; }
    float det = N[0] * (N[4] * N[8] - N[5] * N[7]) - N[1] * (N[3] * N[8] - N[5] * N[6]) + N[2] * (N[3] * N[7] - N[4] * N[6]);
    float I[9] = {N[4] * N[8] - N[5] * N[7], N[2] * N[7] - N[1] * N[8], N[1] * N[5] - N[2] * N[4],
                  N[5] * N[6] - N[3] * N[8], N[0] * N[8] - N[2] * N[6], N[2] * N[3] - N[0] * N[5],
                  N[3] * N[7] - N[4] * N[6], N[1] * N[6] - N[0] * N[7], N[0] * N[4] - N[1] * N[3]};
    for (int i = 0; i < 9; i++) I[i] /= det;
    e = v3(I[0] * r[0] + I[1] * r[1] + I[2] * r[2], I[3] * r[0] + I[4] * r[1] + I[5] * r[2], I[6] * r[0] + I[7] * r[1] + I[8] * r[2]);
    cgResult(e, used, out, outLen);
  }
};
