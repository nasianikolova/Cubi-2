// =====================================================================
// Cubi 2 - BRAIN program for the Teensy 4.1 (board Y): it balances the cube.
// Run cubi2_setup first (drivers, IMU, wheel test, saved).
//
// Commands - from the Serial Monitor (115200, "Newline") or the Cubi 2 app (Terminal):
//   go corner | go edge x | go edge y | go edge z
//                   (24/10/2026) 'go edge x|y|z' WHILE BALANCING ON THE CORNER: it lowers itself onto that edge
//                   (leans at 15 deg/s, the two other wheels brake the fall, then the edge takes over)
//                   (24/10/2026) and back: WHILE BALANCING ON AN EDGE, lift the cube BY HAND onto corner 0 -
//                   within the corner's take-over angle, held still 0.3 s, the corner takes over by itself
//                   (the wheels cannot lift it: 0.83 N m at most against 1.23 N m of gravity)
//   go sphere       (23/10/2026) the cube in the rolling sphere: holds still; then
//                   fwd CM | back CM | left DEG | right DEG | navel 0-7 | halt | front | speed CM/S | turnrate DEG/S
//   go navel 0-7    (24/10/2026) THE SPHERE STANDING ON A NAVEL, balanced like the cube on its corner: hold the
//                   sphere by hand on navel N, bring it slowly upright - it takes over within the corner's angle.
//                   The 8 balance points: 'zero hang N' on each navel (the model's until then - NAVEL_UP below)
//   go arc          (25/10/2026) THE SPHERE ON AN ARC (arcs.h): only after the 8 navels are measured ('zero hang N'):
//                   the arcs are computed from them (4/11/2026). Hold it on an arc, upright across it - within
//                   3 deg of its balance point it takes over and balances across the arc; standing, it
//                   rolls back to where it started (that arc's rest point)
//   roll DEG        on the arc: out DEG along it (at most 30) and back, at 10 deg/s. 'halt' stops a roll
//   where | swing   the sphere resting / rocking, wheels off: where its center of gravity is
//   cg              (24/10/2026) in the sphere: held on each of the 8 navels, the wheels measure the center of
//                   gravity to a tenth of a mm (center_of_gravity.h)
//                   get ready; it starts by itself when you hold the cube
//                   still within 4 deg (corner and edge) of its balance point
//   stop            wheels off at once (also: the cube tips over, a wheel
//                   too fast, the battery low, a driver silent)
//   zero            hold the cube BY HAND exactly on its balance point, then
//                   type "zero" (for the mode of the last "go", or corner)
//   zero hang       the cube hangs still from corner 0 (hanging plate + string): corner point
//   zero hang N     (25/10/2026) the sphere hangs still from navel N (the U hook on its center screw): navel N's
//                   point. Both ask 'keep it and save?'; the 8 navel points are saved too ('save')
//   fix             after a try: moves the balance point from the flight recorder
//                   (go ... stop ... fix ... save ... go again, until it moves < 0.3 deg)
//   up X Y Z        type the balance point of the current mode (as 'show' prints it)
//   k A R W         gains of the current mode: angle, rate, wheel
//   learn L         how fast it finds its balance point (0 = off, 0.2 normal)
//   shake HZ [W]    the shake filter on the wheel torque (25 Hz, width 0.7; 0 = off)
//   spin F          how hard the wheels' spin along the diagonal is slowed (1; 0 = off)
//   fall DEG        stop beyond this tilt (20)    wheelmax RPM   stop above this wheel speed (4100)
//   dump all        the flight recorder, every line (250 a second) instead of every 4th
//   sphere mass KG  (4/11/2026) the weight of this cube in its sphere, with the battery (2.055 at first)
//   'save' keeps all of these, and twist, filter, start, learn, the points and the gains
//   status | show | save | tele on | tele off | help
//   fw ...          (24/10/2026) a new program over the Wi-Fi - the app sends it (program_update.h)
// (25/10/2026) THE COACH (coach.h) - it learns from the real tries and ASKS before it changes anything:
//   yes | no        the answer to the open question ('? ...'); the app shows Yes / No buttons
//                   after a try: the balance point it learned (keep and save?), how the try went, at most
//                   one change of a setting (shake filter, rate filter, twist, rate gain), and after a
//                   change the next try of that mode is compared with the one before (keep? go back?)
//   check           the self-check (also at power-up): gyro zero, |g|, the IMU's noise, battery, drivers;
//                   standing still for 10 s it also watches the gyro zero by itself
//   coach on|off    the coach's report and proposals after each try (the balance point always asks)
//   profile         the settings learned on this cube as one line - the app keeps it as a profile
//   profile load v1 ...   the app sends a profile back (to this cube or another one); it asks to save
//   slope | slope turn    the cube lying on a face: how much the floor slopes; turned half a turn on the same spot,
//                   'slope turn' splits the floor's slope from the IMU's own error (as with the openings, 24/10)
//                   Always on: digital spikes, jolts and vibration from the raw IMU (coach.h, Disturb) - reported
//                   after a try, in 'check', and no change is proposed from a try with a jolt in it
// =====================================================================
#include <Arduino.h>
#include "settings.h"             // the settings kept in the EEPROM
#include "motor_drivers.h"        // the 3 motor drivers on the CAN bus (it uses motor_driver_tables.h)
#include "motion_sensor.h"        // the motion sensor (IMU: gyroscope + accelerometer), in the cube frame
#include "balance.h"              // balancing on a corner or an edge
#include "sphere.h"               // moving inside the sphere
#include "arcs.h"                 // moving along the arcs
#include "center_of_gravity.h"    // 'cg': the center of gravity, measured on the 8 navels
#include "coach.h"                // the coach: learns from the real runs, asks before a change
#include "program_update.h"       // a new program over the Wi-Fi

const char *FIRMWARE_VERSION = "2026110501";   // the date, then the change of that day (YYYYMMDDNN)

#define LINK Serial2           // the Wi-Fi board: RX2 pin 7, TX2 pin 8
#define BAT_PIN A13            // pin 27, battery through 100k / 10k
#define LOOP_HZ 250
#define VBAT_STOP 21.0f        // 3.5 V per cell under load
#define VBAT_WARN 21.6f

CubiSettings S;
ImuCube IMU;
UpFilter UPF;
// (22/10/2026) THE RATE FILTER. The flight recorder on edge z: the gyro swings +-30...60 deg/s at about
// 30 Hz on all 3 axes alike (a shake of the IMU's mount, about the diagonal) and the rate gain turns it
// into full torque, + and - in turn. A 2nd-order low-pass (Butterworth) on the rate used for the
// control only; 'filter HZ' changes it (0 = off). At 15 Hz: 30 Hz shake / 4, ~15 deg lag at 3 Hz.
struct LowPass2 {
  float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, x1[3] = {0, 0, 0}, x2[3] = {0, 0, 0}, y1[3] = {0, 0, 0}, y2[3] = {0, 0, 0};
  float hz = 15.0f;
  void set(float f, float fs) {
    hz = f;
    if (f <= 0) { b0 = 1; b1 = b2 = a1 = a2 = 0; return; }
    float k = tanf(3.14159265f * f / fs), q = 1.41421356f, n = 1.0f / (1.0f + q * k + k * k);
    b0 = k * k * n; b1 = 2 * b0; b2 = b0; a1 = 2 * (k * k - 1) * n; a2 = (1 - q * k + k * k) * n;
  }
  void reset(const float *v) { for (int i = 0; i < 3; i++) x1[i] = x2[i] = y1[i] = y2[i] = v[i]; }
  void step(const float *x, float *y) {
    for (int i = 0; i < 3; i++) {
      y[i] = b0 * x[i] + b1 * x1[i] + b2 * x2[i] - a1 * y1[i] - a2 * y2[i];
      x2[i] = x1[i]; x1[i] = x[i]; y2[i] = y1[i]; y1[i] = y[i];
    }
  }
} RATEF;
float gyrF[3] = {0, 0, 0};
// (23/10/2026) THE SHAKE FILTER. On a hard surface the cube shakes at about 20-30 Hz (corner dumps 10-14:
// rate +-20...50 deg/s, the torque at the limit, + and - in turn) and the controller feeds the shake:
// it answers every swing with full torque. A notch (band-stop) on the wheel torque takes out the
// 20-30 Hz band and keeps the slow balancing (1-3 Hz: ~10 deg lag). 'shake HZ [WIDTH]' changes it
// (0 = off; WIDTH = Q, smaller = wider band). 25 Hz, Q 0.7 until 'save' keeps another.
struct Notch2 {
  float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, x1[3] = {0, 0, 0}, x2[3] = {0, 0, 0}, y1[3] = {0, 0, 0}, y2[3] = {0, 0, 0};
  float hz = 0, q = 0.7f;
  void set(float f, float Q, float fs) {
    hz = f; q = Q;
    if (f <= 0) { b0 = 1; b1 = b2 = a1 = a2 = 0; return; }
    float w0 = 2.0f * 3.14159265f * f / fs, c = cosf(w0), al = sinf(w0) / (2.0f * Q), n = 1.0f / (1.0f + al);
    b0 = n; b1 = -2.0f * c * n; b2 = n; a1 = -2.0f * c * n; a2 = (1.0f - al) * n;
  }
  void reset() { for (int i = 0; i < 3; i++) x1[i] = x2[i] = y1[i] = y2[i] = 0; }
  void step(const float *x, float *y) {
    for (int i = 0; i < 3; i++) {
      y[i] = b0 * x[i] + b1 * x1[i] + b2 * x2[i] - a1 * y1[i] - a2 * y2[i];
      x2[i] = x1[i]; x1[i] = x[i]; y2[i] = y1[i]; y1[i] = y[i];
    }
  }
} SHAKE;
// (22/10/2026) the take-over angle: on an EDGE it starts within 4 deg (the teacher's choice; one wheel
// can still hold about 5-6 deg at 8 A), on the corner within S.armDeg (4 - 26/10/2026: easy to hold by hand; never less). 'start DEG' changes the
// one of the current mode ('save' keeps it).
float edgeArmDeg = 4.0f;
static float armDegFor(int m) { return m == MODE_CORNER ? S.armDeg : edgeArmDeg; }
Balance BAL;
// (23/10/2026) THE SPHERE (sphere.h): the cube in the rolling sphere. 'go sphere' starts it; then
// fwd CM | back CM | left DEG | right DEG | halt | front. 'where' and 'swing' measure it (wheels off).
Sphere SPH;
bool sphereOn = false;          // running in the sphere
bool recSphere = false;         // the flight recorder holds a sphere run (its columns: position, heading)
// (4/11/2026) FACTORY FIRMWARE: the weight is a setting of each cube ('sphere mass KG', then 'save').
// A new cube starts with the Cubi 2 of the guides: 2.055 kg with the sphere and the battery.
float sphereMass = 2.055f;      // kg
#define SPHERE_IC   0.011f      // kg m2 about the center (the model's estimate: the cube + the shell)
CgMeasure CGM(2.055f, 5.9e-4f);   // (24/10/2026) 'cg' (one wheel's inertia, as in sphere.h); its mass follows sphereMass
// (24/10/2026) THE SPHERE STANDING ON A NAVEL ('go navel N'). The sphere is an open cage: on a navel it is an
// inverted pendulum ~130 mm high (gravity 2.6 N m per rad - the cube on its corner: 2.3), so it is balanced
// with the corner's controller (balance.h, all 3 wheels). Gains: the corner's LQR weights (they give the
// corner's and the edges' gains exactly) for 2.055 kg, 0.046 kg m2 about the navel. A host simulation with
// balance.h, the sensors' errors and the 1-sample delay: holds on all 8 navels, a point 1 deg off is learned
// with the wheels up to ~2000 rpm, 2 deg off ~4000 rpm (the limit).
// (4/11/2026) FACTORY FIRMWARE: the 8 navel points start with the model's - the center of gravity in the middle,
// straight over the navel: up = (+-1, +-1, +-1) / V3. Each cube measures its own with 'zero hang N' (the sphere
// hangs still from navel N) and keeps them with 'save'. The arcs come from them (arcs.h).
static const float NV = 0.57735027f;
static const float NAVEL_UP[8][3] = {
  {+NV, +NV, +NV}, {-NV, +NV, +NV}, {+NV, -NV, +NV}, {-NV, -NV, +NV},
  {+NV, +NV, -NV}, {-NV, +NV, -NV}, {+NV, -NV, -NV}, {-NV, -NV, -NV}};
bool navelMeasured[8] = {false, false, false, false, false, false, false, false};   // hung on this cube?
static bool allNavelsMeasured() { for (int j = 0; j < 8; j++) if (!navelMeasured[j]) return false; return true; }
Vec3 navelD[8];                 // the balance points in use (learned while balancing)
float navelK[3] = {8.31f, 1.01f, 0.000359f};   // angle, rate, wheel - 'k A R W' while on a navel changes these
int navelOn = -1;               // the navel it balances on (or is ready for); -1: none
bool recNavel = false;          // the flight recorder holds a navel or arc run (no 'fix' from it)
// (25/10/2026) THE SPHERE ON AN ARC ('go arc', 'roll DEG' - arcs.h)
ArcCtl ARC;
bool arcOn = false;             // ready for / balancing on an arc
bool recArc = false;            // the recorder holds an arc run (its columns: across, its point, along)
const float ARC_ARM_DEG = 3.0f; // it takes over within this of the arc's balance point
const float ARC_FALL_DEG = 12.0f;   // more than this across the arc: it has left it
// (23/10/2026) THE BRAIN'S OWN SAVED SETTINGS - the ones cubi2_setup does not know: twist, the two
// filters, the edge take-over angle. Kept in the EEPROM AFTER the common settings (at byte 1024), with
// their own check, so the common block (and cubi2_setup) stay as they are. 'save' writes both.
#define BRAIN_EXTRA_ADDR  1024
#define BRAIN_EXTRA_MAGIC 0xB4A1E002UL           // (23/10/2026) 2: + spin
struct BrainExtra {
  uint32_t magic; float twist, rateHz, shakeHz, shakeQ, edgeArm, spin; uint32_t crc;
  uint32_t check() const {
    const uint8_t *p = (const uint8_t *)this; uint32_t h = 2166136261UL;
    for (size_t i = 0; i < offsetof(BrainExtra, crc); i++) { h ^= p[i]; h *= 16777619UL; }
    return h;
  }
};
// (25/10/2026) THE 8 NAVEL POINTS, saved (from 'zero hang N'): at byte 1200, their own check. Not there or not
// valid: the model's (NAVEL_UP). (4/11/2026) 2: + which navels were measured on this cube.
#define NAVEL_SAVE_ADDR  1200
#define NAVEL_SAVE_MAGIC  0xB4A1E0A2UL
#define NAVEL_SAVE_MAGIC1 0xB4A1E0A1UL         // the first one (all 8 measured, 25/10/2026) is still read
struct NavelSaved {
  uint32_t magic; float d[8][3]; uint8_t measured[8]; uint32_t crc;
  uint32_t check() const {
    const uint8_t *p = (const uint8_t *)this; uint32_t h = 2166136261UL;
    for (size_t i = 0; i < offsetof(NavelSaved, crc); i++) { h ^= p[i]; h *= 16777619UL; }
    return h;
  }
};
struct NavelSaved1 {
  uint32_t magic; float d[8][3]; uint32_t crc;
  uint32_t check() const {
    const uint8_t *p = (const uint8_t *)this; uint32_t h = 2166136261UL;
    for (size_t i = 0; i < offsetof(NavelSaved1, crc); i++) { h ^= p[i]; h *= 16777619UL; }
    return h;
  }
};
// (4/11/2026) THE SPHERE SETTINGS OF THIS CUBE, saved: at byte 1400, their own check - its weight and the
// navels' gains ('k' on a navel). Not there or not valid: those of the Cubi 2 of the guides.
#define SPHERE_SAVE_ADDR  1400
#define SPHERE_SAVE_MAGIC 0xB4A1E0C1UL
struct SphereSaved {
  uint32_t magic; float mass; float navelGains[3]; uint32_t crc;
  uint32_t check() const {
    const uint8_t *p = (const uint8_t *)this; uint32_t h = 2166136261UL;
    for (size_t i = 0; i < offsetof(SphereSaved, crc); i++) { h ^= p[i]; h *= 16777619UL; }
    return h;
  }
};
// (22/10/2026) THE FLIGHT RECORDER: the last 4 s of balancing, 250 times a second, kept in memory.
// 'dump' prints it after a stop - tilt, tilt rate, wheel speed and torque of the balancing axes.
// (10 lines a second from 'tele' are too slow to see a fall that takes half a second.)
struct RecRow { float t, ax, ay, az, rx, ry, rz, wx, wy, wz, tx, ty, tz; };
const int REC_N = 1000;
RecRow rec[REC_N];
int recHead = 0, recCount = 0, recMode = 0;    // recMode: the mode that was recorded
Vec3 recD = {0, 0, 0};                         // the balance point used in that try
float recLearn = 0;                            // 'learn' in that try (0 = the point stayed put)
uint32_t recT0 = 0;
static void recAdd(const Vec3 &phi, const Vec3 &g, const Vec3 &w, const Vec3 &T) {
  RecRow &r = rec[recHead];
  r.t = (millis() - recT0) * 0.001f;
  r.ax = phi.x * 57.29578f; r.ay = phi.y * 57.29578f; r.az = phi.z * 57.29578f;
  r.rx = g.x * 57.29578f; r.ry = g.y * 57.29578f; r.rz = g.z * 57.29578f;
  r.wx = w.x * 9.549f; r.wy = w.y * 9.549f; r.wz = w.z * 9.549f;
  r.tx = T.x * 1000.0f; r.ty = T.y * 1000.0f; r.tz = T.z * 1000.0f;
  recHead = (recHead + 1) % REC_N; if (recCount < REC_N) recCount++;
}

enum State { OFF, ARMED, RUNNING, FAULT };
const char *STATE_NAME[] = {"OFF", "READY", "BALANCING", "STOPPED"};
State state = OFF;
int mode = MODE_CORNER;
char why[48] = "";
bool tele = false;
bool imuOk = false, canOk = false;
uint32_t armOkSince = 0, lowBatSince = 0, runSince = 0;
float vbat = 0;
// (23/10/2026) THE BATTERY GAUGE: % from the voltage of the 6 cells (a LiPo's usual curve, at rest),
// on a 2 s average (the wheels' current makes the voltage jump). 0 % = 3.50 V a cell, where the cube
// stops ('battery low'). Under load it reads a little low; after a stop it comes back up a little.
#define BAT_CELLS 6
float vbatAvg = 0;
static float batteryPct(float v) {
  static const float V[12] = {3.50f, 3.61f, 3.69f, 3.73f, 3.77f, 3.80f, 3.84f, 3.87f, 3.95f, 4.02f, 4.11f, 4.20f};
  static const float P[12] = {0, 5, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
  float c = v / BAT_CELLS;
  if (c <= V[0]) return 0;
  if (c >= V[11]) return 100;
  for (int i = 1; i < 12; i++)
    if (c < V[i]) return P[i - 1] + (P[i] - P[i - 1]) * (c - V[i - 1]) / (V[i] - V[i - 1]);
  return 100;
}
Vec3 wheelRad = {0, 0, 0};     // wheel speeds, cube frame (rad/s)
float iqCmd[3] = {0, 0, 0};


// ---------------------------------------------------------------------
static void say(const char *t) { Serial.println(t); LINK.println(t); }
static void sayf(const char *fmt, ...) {
  char b[220]; va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof(b), fmt, ap); va_end(ap); say(b);
}

// (25/10/2026) THE COACH'S QUESTIONS: one at a time, '? ...' in the log and in the telemetry ("ask", "aq");
// 'yes' / 'no' answers the first one. A new 'go' drops them (after that try it asks again what still applies).
enum AskKind { ASK_POINT = 1, ASK_FIXPOINT, ASK_NAVEL, ASK_TUNE, ASK_EXP, ASK_BIAS, ASK_DRIVER, ASK_PROFILE, ASK_LOADED, ASK_HANG };
struct Ask { int kind, mode, sub; float a, b, c; Vec3 v; char text[150]; };
const int ASK_MAX = 4;
Ask askQ[ASK_MAX];
int askCount = 0, askId = 0;
bool askHold = false;                      // while a report is printed: its questions come after it
static void askShow() { if (askCount && !askHold) sayf("? %s (yes / no)", askQ[0].text); }
static void askPush(int kind, int md, int sub, float a, float b, float c, Vec3 v, const char *text) {
  for (int i = 0; i < askCount; i++)                       // the same question again: the newer one replaces it
    if (askQ[i].kind == kind && askQ[i].sub == sub) { for (int j = i; j < askCount - 1; j++) askQ[j] = askQ[j + 1]; askCount--; break; }
  if (askCount == ASK_MAX) { for (int j = 0; j < ASK_MAX - 1; j++) askQ[j] = askQ[j + 1]; askCount--; }
  Ask &q = askQ[askCount++];
  q.kind = kind; q.mode = md; q.sub = sub; q.a = a; q.b = b; q.c = c; q.v = v;
  strncpy(q.text, text, sizeof(q.text) - 1); q.text[sizeof(q.text) - 1] = 0;
  askId++;
  if (askCount == 1) askShow();
}
static void askRelease() { bool was = askHold; askHold = false; if (was) askShow(); }
// the balance points learned in this session, not kept yet (in use until the answer)
Vec3 pointCand[MODE_COUNT];
bool pointHas[MODE_COUNT] = {false, false, false, false};
// a setting the coach changed: the next try of that mode is compared with the one before
struct CoachExp { bool on; int mode, kind; float oldV, newV, scoreBefore; };
CoachExp EXP = {false, 0, 0, 0, 0, 0};
bool coachOn = true;
float lastRunSecs = 0;
CoachStats CS;
Disturb DIST;                              // (25/10/2026) spikes, jolts, vibration from the raw IMU
uint32_t distT0 = 0;                       // its clock: since power-up, or since the try began
static float coachA[4][REC_N], coachQ[3][REC_N], coachTl[REC_N];   // the coach's working copies of the recorder
// a profile loaded from the app: what was there before (for 'no')
CubiSettings profBak;
float profBakX[9];
static float angDeg(Vec3 a, Vec3 b) { return acosf(fmaxf(-1.0f, fminf(1.0f, dot(unit(a), unit(b))))) * 57.29578f; }

// (22/10/2026) STOPPING A FAST WHEEL. 'idle' at ~4000 rpm tripped the driver (controller error 1, its
// current over 12 A): the spinning motor (about 12 V back-EMF at 4000 rpm, 0.3 ohm) is shorted by the
// idle outputs. So a wheel faster than SPIN_IDLE_RAD is first braked gently (BRAKE_A, the current
// controller still on), and only set idle when it is slow.
#define SPIN_IDLE_RAD 40.0f        // ~380 rpm: below this 'idle' is harmless
#define BRAKE_A       2.0f         // the gentle brake: ~4 s from 4000 rpm
bool spinDown[3] = {false, false, false};
// (22/10/2026) the teacher: 'stop' -> 'off', but the wheels did not slow down (8 x 'stop', then the battery
// out). The brake was sent ONCE: if the driver missed it (or ignored it) the wheel kept its last balancing
// current. Now: the brake is sent EVERY cycle; each wheel must lose 10 rad/s every 0.5 s (2 A gives ~40),
// else its driver goes idle (it may trip with a click - 'go' restarts it) and the app says which one;
// after 8 s all go idle. The app says the speeds at 'stop' and "wheels stopped" at the end.
#define SPIN_CHECK_MS 500
#define SPIN_MIN_DROP 10.0f        // rad/s per SPIN_CHECK_MS
#define SPIN_MAX_MS   8000
bool spinGiveUp[3] = {false, false, false};
float spinRef[3] = {0, 0, 0};
uint32_t spinRefMs[3] = {0, 0, 0};
uint32_t spinSince = 0;            // when braking began (0 = not braking)
static void driversOff() {
  uint32_t now = millis();
  bool braking = false;
  for (int i = 0; i < 3; i++) {
    uint8_t n = S.nodeId[i];
    if (!tmNodes[n].spec) continue;
    float v = tmNodes[n].vel * TICKS_TO_RAD;              // the driver's own wheel speed, rad/s
    bool fresh = now - tmNodes[n].velMs < 100;
    if (!fresh || fabsf(v) <= SPIN_IDLE_RAD) spinGiveUp[i] = false;
    if (fresh && fabsf(v) > SPIN_IDLE_RAD && !spinGiveUp[i]) {
      if (!spinDown[i]) {                                  // braking begins for this wheel
        spinDown[i] = true; spinRef[i] = fabsf(v); spinRefMs[i] = now;
        if (!spinSince) spinSince = now;
      }
      bool slows = true;
      if (now - spinRefMs[i] >= SPIN_CHECK_MS) {
        slows = fabsf(v) < spinRef[i] - SPIN_MIN_DROP;
        spinRef[i] = fabsf(v); spinRefMs[i] = now;
      }
      if (slows && now - spinSince < SPIN_MAX_MS) {
        tmWriteF(n, tmNodes[n].spec->iq_setpoint, v > 0 ? -BRAKE_A : BRAKE_A);   // every cycle
        iqCmd[i] = 0;
        braking = true;
        continue;
      }
      spinGiveUp[i] = true;
      sayf("wheel %c does not slow down (%.0f rpm) - its driver set to idle", "XYZ"[i], v * 9.549f);
    }
    spinDown[i] = false;
    tmWriteF(n, tmNodes[n].spec->iq_setpoint, 0.0f);
    tmCall(n, tmNodes[n].spec->idle);
    iqCmd[i] = 0;
  }
  if (!braking && spinSince) { spinSince = 0; say("wheels stopped (or free-running)"); }
}
static bool anySpinDown() { return spinDown[0] || spinDown[1] || spinDown[2]; }

// what the cube learned about its balance point stays (in memory - 'save' keeps it)
// (24/10/2026) LOWERING FROM THE CORNER ONTO AN EDGE. The balance point is moved from the corner's to the
// edge's at 15 deg/s. After ~8 deg the wheels cannot hold the lean any more (only the two wheels across the
// edge help, 0.31 N m against up to 1.2 N m of gravity at 35 deg): the cube falls, the controller brakes it
// at full torque, and corner 1 of that edge lands at ~0.5 m/s (like a 1.3 cm drop; the simulation: wheels
// up to ~2000 rpm). The moment the cube is within 4 deg of the edge's balance point the edge takes over.
const float LOWER_RATE = 15.0f / 57.29578f;      // rad/s, how fast the balance point moves
const float LOWER_LAND = 4.0f / 57.29578f;       // rad, the edge takes over this close to its point
const uint32_t LOWER_MS = 4000;                  // it must reach the edge within this time
bool lowering = false;
int lowerTo = MODE_EDGE_X;
float lowerAng = 0, lowerTotal = 0;
Vec3 lowerC = {0, 0, 0}, lowerE = {0, 0, 0};
uint32_t lowerSince = 0;
uint32_t liftOkSince = 0;
bool recLowering = false;                        // (24/10/2026) the recorder holds a lowering: no 'fix' from it                        // (24/10/2026) on an edge, lifted by hand near the corner's point since
static Vec3 slerp3(Vec3 a, Vec3 b, float t) {
  float c = fmaxf(-1.0f, fminf(1.0f, dot(a, b))), th = acosf(c);
  if (th < 1e-5f) return b;
  return unit(add(mul(a, sinf((1.0f - t) * th) / sinf(th)), mul(b, sinf(t * th) / sinf(th))));
}

static void keepLearned() {
  liftOkSince = 0; CGM.stop();                                         // no stale hand-over timer after a stop / fault
  if (lowering) { BAL.d = lowerC; lowering = false; }   // keep the corner's point, not the moving one
  if (arcOn) {                                          // (25/10/2026) an arc: what it learned, shown
    if (state == RUNNING)
      sayf("arc (circle %d): balanced %.2f deg across (learned %.2f deg from the computed point), %.0f deg from where it started",
           ARC.circle, ARC.sStar * 57.29578f, (ARC.sStar - ARC.sStar0) * 57.29578f, ARC.pos * 57.29578f);
    return;
  }
  if (navelOn >= 0) {                                   // (24/10/2026) a navel: its point, in memory only
    if (state == RUNNING) {
      Vec3 prev = navelD[navelOn];
      navelD[navelOn] = BAL.d;                          // (25/10/2026) in use until the answer
      Vec3 m0 = v3(NAVEL_UP[navelOn][0], NAVEL_UP[navelOn][1], NAVEL_UP[navelOn][2]);
      sayf("navel %d point: up %+.4f %+.4f %+.4f (learned %.2f deg from the model's)", navelOn, BAL.d.x, BAL.d.y, BAL.d.z,
           angDeg(BAL.d, m0));
      if (angDeg(BAL.d, prev) > 0.05f) {
        char q[150];
        snprintf(q, sizeof(q), "navel %d: it learned a point %.2f deg from the one it started with - keep it and save?", navelOn, angDeg(BAL.d, prev));
        askPush(ASK_NAVEL, MODE_CORNER, navelOn, 0, 0, 0, prev, q);
      }
    }
    return;
  }
  if (state != RUNNING || sphereOn) return;
  // (25/10/2026) not written into the settings any more: kept aside and ASKED after the try (afterRun)
  pointCand[mode] = BAL.d; pointHas[mode] = true;
}

const float POINT_ASK_DEG = 0.5f;          // (26/10/2026) it asks about a new balance point only from this far on
// (22/10/2026) a driver's state and errors in one line - why it did not start
static void driverReport(int i) {
  uint8_t n = S.nodeId[i];
  const TmSpec *s = tmNodes[n].spec;
  if (!s) { sayf("  driver %c (id %d): not found", AXIS_NAME[i], n); return; }
  uint8_t st = 255, e = 0, ce = 0, me = 0, se = 0, cal = 0; float v = 0;
  bool ok = tmReadU8(n, s->state, st);
  tmReadU8(n, s->errors, e); tmReadU8(n, s->ctrl_errors, ce); tmReadU8(n, s->motor_errors, me);
  tmReadU8(n, s->sensor_errors, se); tmReadU8(n, s->calibrated, cal); tmReadF(n, s->vbus, v);
  if (!ok) { sayf("  driver %c (id %d): no answer", AXIS_NAME[i], n); return; }
  sayf("  driver %c (id %d): %s  %s  %.1f V  errors: system %d controller %d motor %d sensor %d",
       AXIS_NAME[i], n, st == 0 ? "IDLE" : st == 1 ? "CALIBRATING" : st == 2 ? "RUNNING" : "?",
       cal ? "calibrated" : "NOT calibrated", v, e, ce, me, se);
}

static bool driversStart() {
  for (int i = 0; i < 3; i++) {
    uint8_t n = S.nodeId[i];
    const TmSpec *s = tmNodes[n].spec;
    if (!s) return false;
    tmWriteF(n, s->iq_setpoint, 0.0f);
    tmWriteF(n, s->wd_timeout, 0.1f);         // the driver stops by itself if the Teensy goes quiet
    tmWriteU8(n, s->wd_enabled, 1);
    tmCall(n, s->current_mode); delay(5);
    tmWriteU8(n, s->mode, 0); delay(5);
  }
  // (22/10/2026) the start, checked and tried again: 'driver Z did not start' once, with no reason shown
  for (int i = 0; i < 3; i++) {
    uint8_t n = S.nodeId[i]; const TmSpec *s = tmNodes[n].spec;
    uint8_t st = 0;
    for (int k = 0; k < 3 && st != 2; k++) {
      tmWriteU8(n, s->state, 2);
      delay(30); tmPoll();
      if (!tmReadU8(n, s->state, st)) st = 0;
    }
    if (st != 2) { sayf("driver %c did not start:", AXIS_NAME[i]); driverReport(i); driversOff(); return false; }
  }
  return true;
}

static void findDrivers() {
  int found = 0;
  for (int i = 0; i < 3; i++) {
    uint8_t n = S.nodeId[i];
    if (tmDetect(n) && tmNodes[n].spec) found++;
    else if (tmIsBootloader(tmNodes[n].hash)) {          // (21/10/2026) stuck: restart it
      tmBootReset(n, tmNodes[n].hash);
      sayf("!! driver %c (id %d) was stuck starting - restarted: wait 10 s, then 'go' again", AXIS_NAME[i], n);
    }
    else sayf("!! driver %c (id %d) not found or unknown firmware", AXIS_NAME[i], n);
  }
  canOk = (found == 3);
  if (canOk) { driversOff(); say("3 drivers ok"); }
}

static void applyMode(int m) {
  mode = m;
  BAL.Ka = S.K[m][0]; BAL.Kr = S.K[m][1]; BAL.Kw = S.K[m][2];
  BAL.edgeAxis = (m == MODE_CORNER) ? -1 : m - MODE_EDGE_X;
  BAL.d = pointHas[m] ? pointCand[m] : v3(S.up[m][0], S.up[m][1], S.up[m][2]);   // (25/10/2026) a learned one not kept yet is in use
  BAL.tMax = S.iqLimit * KT_NM_PER_A;
  BAL.learn = S.trimRate;
}

static float tiltFromMode(int m) {
  Vec3 d = pointHas[m] ? pointCand[m] : v3(S.up[m][0], S.up[m][1], S.up[m][2]);
  float c = dot(UPF.up, d);
  return acosf(fmaxf(-1.0f, fminf(1.0f, c))) * 57.29578f;
}
// (24/10/2026) the sphere on navel j: the corner's controller with the navel's gains and point
static void applyNavel(int j) {
  applyMode(MODE_CORNER);                   // the corner's way (3 wheels); its own point and gains are not changed
  BAL.Ka = navelK[0]; BAL.Kr = navelK[1]; BAL.Kw = navelK[2];
  BAL.d = navelD[j];
}
static float tiltFromNavel(int j) {
  return acosf(fmaxf(-1.0f, fminf(1.0f, dot(UPF.up, navelD[j])))) * 57.29578f;
}
// (25/10/2026) ready on an arc: how far from the nearest arc's balance point (deg)
static float arcOffDeg() { float off; ArcCtl::nearest(UPF.up, off); return off * 57.29578f; }
// the name shown for the mode (status, the app)
static const char *modeLabel() {
  static char b[12];
  if (arcOn) { float off; snprintf(b, sizeof(b), "arc %d", state == RUNNING ? ARC.circle : ArcCtl::nearest(UPF.up, off)); return b; }
  if (navelOn < 0) return MODE_NAME[mode];
  snprintf(b, sizeof(b), "navel %d", navelOn); return b;
}

// =====================================================================
// (25/10/2026) THE COACH: it learns from the real tries (the flight recorder, the IMU standing still) and
// ASKS before it changes anything. coach.h does the arithmetic; here: the questions and their answers.
// =====================================================================
static void doSave() {                     // the settings + the brain's own ones (as 'save')
  cubiSave(S);
  BrainExtra e = {BRAIN_EXTRA_MAGIC, BAL.twist, RATEF.hz, SHAKE.hz, SHAKE.q, edgeArmDeg, BAL.spin, 0};
  e.crc = e.check(); EEPROM.put(BRAIN_EXTRA_ADDR, e);
  NavelSaved nv; nv.magic = NAVEL_SAVE_MAGIC;                // (25/10/2026) the navel points too
  for (int j = 0; j < 8; j++) { nv.d[j][0] = navelD[j].x; nv.d[j][1] = navelD[j].y; nv.d[j][2] = navelD[j].z; nv.measured[j] = navelMeasured[j]; }
  nv.crc = nv.check(); EEPROM.put(NAVEL_SAVE_ADDR, nv);
  SphereSaved sphere; sphere.magic = SPHERE_SAVE_MAGIC;      // (4/11/2026) its weight and the navels' gains
  sphere.mass = sphereMass;
  for (int j = 0; j < 3; j++) sphere.navelGains[j] = navelK[j];
  sphere.crc = sphere.check(); EEPROM.put(SPHERE_SAVE_ADDR, sphere);
}
// (25/10/2026) THE LEVEL BY HANGING: 2 s still -> 'up' = the other way round from what the accelerometer reads
// (hanging, the center of gravity is straight under the hanging point). Turning slowly about the string is fine:
// that does not change where gravity is in the cube; only the swinging counts.
static bool hangRead(Vec3 &u, float &swingDeg) {
  Vec3 sum = v3(0, 0, 0); float gmax = 0;
  uint32_t t0 = millis();
  while (millis() - t0 < 2000) {
    IMU.read();
    Vec3 a = v3(IMU.acc[0], IMU.acc[1], IMU.acc[2]), g = v3(IMU.gyr[0], IMU.gyr[1], IMU.gyr[2]);
    sum = add(sum, a);
    Vec3 ah = unit(a); Vec3 gp = sub(g, mul(ah, dot(g, ah)));   // the swing: the rate across gravity
    gmax = fmaxf(gmax, len(gp)); delay(4); tmPoll();
  }
  swingDeg = gmax * 57.29578f;
  if (gmax > 0.05f) return false;
  u = mul(unit(sum), -1.0f);
  return true;
}

// THE BALANCE POINT FROM THE FLIGHT RECORDER (as 'fix'). Over a try the cube is still on average, so gravity =
// the wheels' mean torque: mgl x (where it sat - true point) = mean torque, so
//   true point = where it sat - mean torque / mgl          (mgl = mass x g x height of the CG)
// A stuck wheel (torque at the limit, corner dump 3) and a calm try (torque ~0, dumps 6-7) both fit. It starts
// from the point used in that try, so the same try gives the same point. The first 0.25 s (the take-over) out.
static bool fixEstimate(Vec3 &b, float &leanDeg, float &tqMnm, float &moved) {
  const int skip = 62;                                   // 0.25 s at 250 Hz
  if (recCount < skip + 63) return false;
  int m = recMode;
  int start = (recHead - recCount + REC_N) % REC_N;
  Vec3 sa = v3(0, 0, 0), st = v3(0, 0, 0);
  for (int k = skip; k < recCount; k++) {
    const RecRow &r = rec[(start + k) % REC_N];
    sa = add(sa, v3(r.ax, r.ay, r.az)); st = add(st, v3(r.tx, r.ty, r.tz));
  }
  float n = (float)(recCount - skip);
  Vec3 phi = mul(sa, 1.0f / (n * 57.29578f));           // mean tilt, rad
  Vec3 T = mul(st, 0.001f / n);                          // mean wheel torque, Nm
  // (23/10/2026) from the weighing: 2.055 kg WITH the sphere -> the bare cube ~1.75 kg (the model said 1.96)
  const float MGL_CORNER = 2.13f, MGL_EDGE = 1.79f;     // Nm: ~1.75 kg, CG ~124 mm over the tip, ~104 mm over an edge
  float mgl = (m == MODE_CORNER) ? MGL_CORNER : MGL_EDGE;
  Vec3 d = unit(recD);
  T = sub(T, mul(d, dot(T, d)));                         // the twist on the tip has nothing to do with gravity
  b = unit(add(d, cross(d, sub(phi, mul(T, 1.0f / mgl)))));
  if (m != MODE_CORNER) { setc(b, m - MODE_EDGE_X, 0.0f); b = unit(b); }
  leanDeg = len(phi) * 57.29578f; tqMnm = len(T) * 1000.0f;
  moved = angDeg(b, d);
  return true;
}

// a setting the coach changes (in memory; 'yes' after the next try saves it)
static void applyTune(int kind, int md, float v) {
  if (kind == CO_SHAKE) { SHAKE.set(v, SHAKE.q > 0.29f ? SHAKE.q : 0.7f, LOOP_HZ); SHAKE.reset(); }
  else if (kind == CO_RATEF) { RATEF.set(v, LOOP_HZ); RATEF.reset(IMU.gyr); }
  else if (kind == CO_TWIST) BAL.twist = v;
  else if (kind == CO_KR_DOWN || kind == CO_KR_UP) { S.K[md][1] = v; if (md == mode && state != RUNNING) applyMode(md); }
}
static float tuneValue(int kind, int md) {
  if (kind == CO_SHAKE) return SHAKE.hz;
  if (kind == CO_RATEF) return RATEF.hz;
  if (kind == CO_TWIST) return BAL.twist;
  return S.K[md][1];
}

// the settings of this cube as one line: 4 points, 4 x 3 gains, the navels' gains, twist, spin, rate filter,
// shake filter + width, learn, take-over corner / edges. The app keeps it as a profile.
static void printProfile() {
  char b[420]; int p = snprintf(b, sizeof(b), "profile v1");
  for (int m = 0; m < MODE_COUNT; m++) {
    Vec3 u = pointHas[m] ? pointCand[m] : v3(S.up[m][0], S.up[m][1], S.up[m][2]);
    p += snprintf(b + p, sizeof(b) - p, " %.4f %.4f %.4f", u.x, u.y, u.z);
  }
  for (int m = 0; m < MODE_COUNT; m++) p += snprintf(b + p, sizeof(b) - p, " %.4f %.4f %.6f", S.K[m][0], S.K[m][1], S.K[m][2]);
  p += snprintf(b + p, sizeof(b) - p, " %.4f %.4f %.6f", navelK[0], navelK[1], navelK[2]);
  snprintf(b + p, sizeof(b) - p, " %.3f %.3f %.1f %.1f %.2f %.3f %.2f %.2f", BAL.twist, BAL.spin, RATEF.hz, SHAKE.hz, SHAKE.q, S.trimRate, S.armDeg, edgeArmDeg);
  say(b);
}
// 'profile load v1 <35 numbers>': checked, put in use, then ASKED to be saved ('no' = back as it was)
static void loadProfile(String a) {
  if (state == RUNNING || state == ARMED) { say("stop first"); return; }
  a.trim();
  if (!a.startsWith("v1 ")) { say("profile: unknown version (this program reads v1)"); return; }
  const char *c = a.c_str() + 3; char *e;
  float x[35]; int k = 0;
  while (k < 35) { float v = strtof(c, &e); if (e == c) break; x[k++] = v; c = e; }
  if (k != 35) { sayf("profile: %d numbers, 35 expected - not loaded", k); return; }
  float worst = 0; int worstM = 0;
  for (int m = 0; m < MODE_COUNT; m++) {
    Vec3 u = v3(x[3 * m], x[3 * m + 1], x[3 * m + 2]);
    if (len(u) < 0.5f) { say("profile: a balance point is not a direction - not loaded"); return; }
    float d = angDeg(u, v3(S.up[m][0], S.up[m][1], S.up[m][2]));
    if (d > worst) { worst = d; worstM = m; }
  }
  if (worst > 10.0f) { sayf("profile: its %s point is %.1f deg from this cube's - not loaded (another kind of cube?)", MODE_NAME[worstM], worst); return; }
  for (int m = 0; m < MODE_COUNT + 1; m++) {
    const float *g = x + 12 + 3 * m;
    if (g[0] < 2 || g[0] > 15 || g[1] < 0.2f || g[1] > 2 || g[2] < 0 || g[2] > 0.002f) { say("profile: gains outside what is safe - not loaded"); return; }
  }
  const float *o = x + 27;     // twist spin rateHz shakeHz shakeQ learn armCorner armEdges
  if (o[0] < 0 || o[0] > 1 || o[1] < 0 || o[1] > 5 || o[2] < 0 || o[2] > 100 || o[3] < 0 || o[3] > 100 ||
      (o[3] > 0 && (o[4] < 0.3f || o[4] > 5)) || o[5] < 0 || o[5] > 0.5f || o[6] < 0.5f || o[6] > 8 || o[7] < 0.5f || o[7] > 8) {
    say("profile: a setting outside its range - not loaded"); return;
  }
  profBak = S;                                           // what was there, for 'no'
  float bx[9] = {BAL.twist, BAL.spin, RATEF.hz, SHAKE.hz, SHAKE.q, edgeArmDeg, navelK[0], navelK[1], navelK[2]};
  memcpy(profBakX, bx, sizeof(bx));
  for (int m = 0; m < MODE_COUNT; m++) {
    Vec3 u = unit(v3(x[3 * m], x[3 * m + 1], x[3 * m + 2]));
    S.up[m][0] = u.x; S.up[m][1] = u.y; S.up[m][2] = u.z; pointHas[m] = false;
    for (int j = 0; j < 3; j++) S.K[m][j] = x[12 + 3 * m + j];
  }
  for (int j = 0; j < 3; j++) navelK[j] = x[24 + j];
  BAL.twist = o[0]; BAL.spin = o[1]; RATEF.set(o[2], LOOP_HZ); RATEF.reset(IMU.gyr); SHAKE.set(o[3], o[4] > 0.29f ? o[4] : 0.7f, LOOP_HZ); SHAKE.reset();
  S.trimRate = o[5]; S.armDeg = fmaxf(4.0f, o[6]); edgeArmDeg = fmaxf(4.0f, o[7]);   // (26/10/2026) never under 4 deg
  applyMode(mode); EXP.on = false;
  char q[150];
  snprintf(q, sizeof(q), "profile loaded and in use (points moved up to %.2f deg, %s) - save it on this cube?", worst, MODE_NAME[worstM]);
  askPush(ASK_LOADED, mode, 0, 0, 0, 0, v3(0, 0, 0), q);
}

static void askAnswer(bool yes) {
  if (!askCount) { say("no question is open"); return; }
  if (state == RUNNING) { say("answer after the try ('stop' first)"); return; }
  Ask a = askQ[0];
  for (int j = 0; j < askCount - 1; j++) askQ[j] = askQ[j + 1];
  askCount--;
  switch (a.kind) {
    case ASK_POINT:                                      // the points it learned while balancing
      if (yes) {
        for (int m = 0; m < MODE_COUNT; m++) if (pointHas[m]) { S.up[m][0] = pointCand[m].x; S.up[m][1] = pointCand[m].y; S.up[m][2] = pointCand[m].z; pointHas[m] = false; }
        doSave(); say("kept and saved");
      } else {
        for (int m = 0; m < MODE_COUNT; m++) pointHas[m] = false;
        say("not kept - back to the saved balance point");
      }
      applyMode(mode);
      break;
    case ASK_FIXPOINT:                                   // the point from the wheels' average push
      if (yes) { S.up[a.mode][0] = a.v.x; S.up[a.mode][1] = a.v.y; S.up[a.mode][2] = a.v.z; pointHas[a.mode] = false; applyMode(mode); doSave(); say("moved and saved - 'go' again"); }
      else say("left as it is");
      break;
    case ASK_NAVEL:
      if (yes) { doSave(); say("kept and saved"); }
      else { navelD[a.sub] = a.v; say("back to the point it started with"); }
      break;
    case ASK_TUNE:                                       // a change for the next try
      if (yes) {
        applyTune(a.sub, a.mode, a.b);
        EXP = {true, a.mode, a.sub, a.a, a.b, a.c};
        sayf("%s %.3g -> %.3g (not saved yet) - 'go %s' again: after that try it compares", COACH_NAME[a.sub], a.a, a.b, a.mode == MODE_CORNER ? "corner" : MODE_NAME[a.mode]);
      } else say("left as it is");
      break;
    case ASK_EXP:                                        // after the compared try: c 1 = keep?, c 2 = go back?
      if ((a.c == 1) == yes) { doSave(); sayf("%s %.3g kept and saved", COACH_NAME[a.sub], tuneValue(a.sub, a.mode)); }
      else { applyTune(a.sub, a.mode, a.a); sayf("%s back to %.3g", COACH_NAME[a.sub], a.a); }
      break;
    case ASK_BIAS:
      if (yes) { S.gyroBias[0] = a.v.x; S.gyroBias[1] = a.v.y; S.gyroBias[2] = a.v.z; doSave(); say("new gyro zero taken and saved"); }
      else say("left as it is");
      break;
    case ASK_DRIVER:
      if (yes) { uint8_t nd = S.nodeId[a.sub]; if (tmNodes[nd].spec) tmCall(nd, tmNodes[nd].spec->reset); canOk = false; say("restarted - wait 10 s (type nothing), then 'check' or 'go'"); }
      else say("left as it is");
      break;
    case ASK_PROFILE:
      if (yes) { printProfile(); say("(the app keeps it - give it a name there)"); }
      else say("not sent");
      break;
    case ASK_HANG:                                       // a level by hanging: sub -1 corner 0, else navel sub
      if (yes) { if (a.sub >= 0) navelMeasured[a.sub] = true; doSave(); say(a.sub < 0 ? "corner 0 point saved" : "navel point saved"); }
      else if (a.sub < 0) { S.up[MODE_CORNER][0] = a.v.x; S.up[MODE_CORNER][1] = a.v.y; S.up[MODE_CORNER][2] = a.v.z; applyMode(mode); say("back to the point it had"); }
      else { navelD[a.sub] = a.v; say("back to the point it had"); }
      break;
    case ASK_LOADED:
      if (yes) { doSave(); say("profile saved on this cube"); }
      else {
        S = profBak;
        BAL.twist = profBakX[0]; BAL.spin = profBakX[1]; RATEF.set(profBakX[2], LOOP_HZ); RATEF.reset(IMU.gyr);
        SHAKE.set(profBakX[3], profBakX[4] > 0.29f ? profBakX[4] : 0.7f, LOOP_HZ); SHAKE.reset(); edgeArmDeg = profBakX[5];
        for (int j = 0; j < 3; j++) navelK[j] = profBakX[6 + j];
        applyMode(mode); say("back to the settings it had before the profile");
      }
      break;
  }
  askShow();
}

// after every try (stop or fault): the point it learned, how it went, at most one change - all ASKED
static void afterRunBody() {
  if (recSphere || recArc) return;
  const int skip = 62;
  int n = recCount - skip;
  // a jolt in the recorder's seconds (on DIST's clock): no point and no change is learned from it
  float runT = (recT0 - distT0) * 0.001f + (recCount ? rec[(recHead - 1 + REC_N) % REC_N].t : 0);
  bool joltInWindow = DIST.lastJoltT > runT - 4.0f;
  // 1. the balance point (corner, edges): what it learned while balancing, else what the wheels' push says
  if (!recNavel) {
    char lst[110]; int p = 0; bool any = false;
    lst[0] = 0;
    // (26/10/2026) the teacher: a "better" point a few tenths of a degree away is the cube's own sway, not a
    // better point - asking about it after every try goes round in a loop. It asks only from 0.5 deg on.
    float small = 0; int smallM = -1;
    for (int m = 0; m < MODE_COUNT; m++) if (pointHas[m]) {
      float a = angDeg(pointCand[m], v3(S.up[m][0], S.up[m][1], S.up[m][2]));
      if (a < POINT_ASK_DEG) { pointHas[m] = false; if (a > small) { small = a; smallM = m; } continue; }
      if (p < 90) p += snprintf(lst + p, sizeof(lst) - p, "%s%s %.2f deg", any ? ", " : "", MODE_NAME[m], a);
      any = true;
    }
    char q[150];
    if (!any && smallM >= 0 && small >= 0.05f)
      sayf("the %s balance point is good - it moved only %.2f deg while it balanced (less than %.1f) - nothing to change", MODE_NAME[smallM], small, POINT_ASK_DEG);
    if (any) {
      snprintf(q, sizeof(q), "it learned a new balance point while it balanced (%s) - keep it and save?", lst);
      askPush(ASK_POINT, recMode, 0, 0, 0, 0, v3(0, 0, 0), q);
    } else if (!recLowering && n >= 375 && !joltInWindow) {
      Vec3 b; float lean, tq, moved;
      if (fixEstimate(b, lean, tq, moved) && moved >= POINT_ASK_DEG && moved < 5.0f) {
        snprintf(q, sizeof(q), "the wheels pushed %.0f mNm one way on average: the true %s point looks %.2f deg away - move it there and save?",
                 tq, MODE_NAME[recMode], moved);
        askPush(ASK_FIXPOINT, recMode, 0, 0, 0, 0, b, q);
      }
    }
  }
  if (!coachOn || recLowering || recNavel) return;
  if (n < 375) { say("(too short for the coach - it needs 1.5 s of balancing)"); return; }
  // 2. how it went: the recorder, split into the tipping rate, the twist and the torque of the axes that balance
  int start = (recHead - recCount + REC_N) % REC_N;
  bool corner = recMode == MODE_CORNER;
  int ax = recMode - MODE_EDGE_X;
  Vec3 d = unit(recD), tSum = v3(0, 0, 0);
  float lim = S.iqLimit * KT_NM_PER_A * 1000.0f * 0.97f;
  int satN = 0;
  for (int k = 0; k < n; k++) {
    const RecRow &r = rec[(start + skip + k) % REC_N];
    Vec3 g = v3(r.rx, r.ry, r.rz), T = v3(r.tx, r.ty, r.tz), ph = v3(r.ax, r.ay, r.az);
    if (corner) {
      float tw = dot(g, d); Vec3 gp = sub(g, mul(d, tw)); Vec3 Tp = sub(T, mul(d, dot(T, d)));
      coachA[0][k] = gp.x; coachA[1][k] = gp.y; coachA[2][k] = gp.z; coachA[3][k] = tw;
      coachQ[0][k] = Tp.x; coachQ[1][k] = Tp.y; coachQ[2][k] = Tp.z;
      coachTl[k] = len(ph); tSum = add(tSum, Tp);
      if (fabsf(T.x) >= lim || fabsf(T.y) >= lim || fabsf(T.z) >= lim) satN++;
    } else {
      coachA[0][k] = comp(g, ax); coachQ[0][k] = comp(T, ax); coachTl[k] = fabsf(comp(ph, ax));
      setc(tSum, ax, comp(tSum, ax) + comp(T, ax));
      if (fabsf(comp(T, ax)) >= lim) satN++;
    }
  }
  float *rp[3] = {coachA[0], coachA[1], coachA[2]}, *tq[3] = {coachQ[0], coachQ[1], coachQ[2]};
  coachAnalyze(rp, tq, corner ? 3 : 1, corner ? coachA[3] : nullptr, coachTl, n, (float)LOOP_HZ, satN / (float)n, len(tSum) / n, CS);
  char b[200];
  coachLine(CS, b, sizeof(b)); say(b);
  // the disturbances of this try (the raw IMU): from outside, not the controller
  if (DIST.spikes || DIST.jolts) {
    int p2 = snprintf(b, sizeof(b), "disturbances: %d jolt(s), %d digital spike(s) | vibration %.0f mg", DIST.jolts, DIST.spikes, DIST.vibMg());
    for (int i = 0; i < DIST.nEv && p2 < 170; i++)
      p2 += snprintf(b + p2, sizeof(b) - p2, "%s %s %.1f s", i ? "," : " |", DIST.ev[i].kind == Disturb::EV_JOLT ? "jolt" : "spike", DIST.ev[i].t);
    say(b);
    if (DIST.spikes >= 3) say("  many single-reading spikes: a loose wire on the IMU's cable, or electrical noise - check its plug");
  } else sayf("disturbances: none | vibration %.0f mg", DIST.vibMg());
  // 3. a change made before this try: better, worse, the same?
  if (EXP.on && EXP.mode == recMode) {
    EXP.on = false;
    float r = CS.score / fmaxf(1e-3f, EXP.scoreBefore);
    char q[150];
    if (r > 1.1f) {
      snprintf(q, sizeof(q), "worse than before the change (%s %.3g -> %.3g: calm %.2f -> %.2f) - go back to %.3g?",
               COACH_NAME[EXP.kind], EXP.oldV, EXP.newV, EXP.scoreBefore, CS.score, EXP.oldV);
      askPush(ASK_EXP, EXP.mode, EXP.kind, EXP.oldV, EXP.newV, 2, v3(0, 0, 0), q);
    } else {
      snprintf(q, sizeof(q), "%s than before the change (%s %.3g -> %.3g: calm %.2f -> %.2f) - keep it and save?",
               r < 0.9f ? "better" : "about the same", COACH_NAME[EXP.kind], EXP.oldV, EXP.newV, EXP.scoreBefore, CS.score);
      askPush(ASK_EXP, EXP.mode, EXP.kind, EXP.oldV, EXP.newV, 1, v3(0, 0, 0), q);
    }
  } else if (joltInWindow) {
    say("  a jolt in the analyzed seconds - nothing proposed from this try (it would learn the knock, not the cube)");
  } else {
    CoachNow c; c.corner = corner; c.Kr = S.K[recMode][1]; c.twist = BAL.twist; c.rateHz = RATEF.hz; c.shakeHz = SHAKE.hz;
    CoachProposal P;
    if (coachPropose(CS, c, P)) askPush(ASK_TUNE, recMode, P.kind, P.oldV, P.newV, CS.score, v3(0, 0, 0), P.why);
    else if (CS.sat < 0.05f && CS.tiltRms < 0.6f) sayf("  calm %.2f - nothing to change", CS.score);
    else sayf("  calm %.2f - no clear reason for a change", CS.score);
  }
  // 4. a long, calm try: offer its settings to the app as a profile
  if (lastRunSecs >= 60.0f && CS.tiltRms < 0.5f && CS.sat < 0.05f) {
    char q[150];
    snprintf(q, sizeof(q), "a long, calm try (%.0f s on the %s, tilt %.2f deg) - send these settings to the app as a profile?", lastRunSecs, MODE_NAME[recMode], CS.tiltRms);
    askPush(ASK_PROFILE, recMode, 0, 0, 0, 0, v3(0, 0, 0), q);
  }
}
static void afterRun() {                   // the report first, then its questions
  int before = askCount;
  askHold = before == 0;
  afterRunBody();
  askRelease();
}

// STOP with a reason: the wheels off, the reason shown, then the coach's report
static void fault(const char *t) {
  bool wasRun = state == RUNNING;
  if (wasRun) lastRunSecs = (millis() - runSince) / 1000.0f;
  keepLearned();
  driversOff();
  sphereOn = false; navelOn = -1; arcOn = false;
  if (state != FAULT) sayf("STOPPED: %s", t);
  if (wasRun) {                              // (25/10/2026) was something from outside the reason?
    float tn = (millis() - distT0) * 0.001f;
    if (tn - DIST.lastJoltT < 0.6f) sayf("  %.1f s before it: a jolt (%.2f g) - a knock, a hand or the table moved", tn - DIST.lastJoltT, DIST.lastJoltSize);
  }
  strncpy(why, t, sizeof(why) - 1);
  state = FAULT;
  if (wasRun) afterRun();                    // (25/10/2026) the coach
}

// THE SELF-CHECK: 2 s still - gyro zero, |g|, the IMU's noise; the battery; the drivers. Changes are asked.
static void selfCheckBody(bool atStart) {
  if (state == RUNNING || state == ARMED) { say("stop first"); return; }
  if (anySpinDown() || len(wheelRad) > SPIN_IDLE_RAD) { say("a wheel is still turning - wait until it stops"); return; }
  char sum[200]; int p = snprintf(sum, sizeof(sum), "check:");
  int problems = 0;
  if (!imuOk) { p += snprintf(sum + p, sizeof(sum) - p, " !! IMU not found |"); problems++; }
  else {
    say(atStart ? "self-check - keep the cube still for 2 s ..." : "check - keep the cube still for 2 s ...");
    int sp0 = DIST.spikes;
    double gs[3] = {0, 0, 0}, gq[3] = {0, 0, 0}; double amag = 0; float gmax = 0; int k = 0;
    uint32_t t0 = millis();
    while (millis() - t0 < 2000) {
      if (IMU.read()) {
        { float gd[3] = {IMU.gyr[0] * 57.29578f, IMU.gyr[1] * 57.29578f, IMU.gyr[2] * 57.29578f}; DIST.step(gd, IMU.acc, (millis() - distT0) * 0.001f, 0.004f); }
        for (int i = 0; i < 3; i++) { gs[i] += IMU.gyrImu[i]; gq[i] += (double)IMU.gyrImu[i] * IMU.gyrImu[i]; }
        amag += len(v3(IMU.acc[0], IMU.acc[1], IMU.acc[2]));
        gmax = fmaxf(gmax, len(v3(IMU.gyrImu[0], IMU.gyrImu[1], IMU.gyrImu[2])));
        k++;
      }
      delay(4); tmPoll();
    }
    if (DIST.spikes - sp0 > 0) { p += snprintf(sum + p, sizeof(sum) - p, " %d digital spike(s) in 2 s - the IMU's plug? |", DIST.spikes - sp0); problems++; }
    if (k < 100) { p += snprintf(sum + p, sizeof(sum) - p, " !! the IMU gave %d readings |", k); problems++; }
    else if (gmax > 0.052f) {                            // 3 deg/s: it was moved
      p += snprintf(sum + p, sizeof(sum) - p, " moved during the check (gyro zero and |g| skipped) |");
    } else {
      float m[3], sd = 0;
      for (int i = 0; i < 3; i++) { m[i] = gs[i] / k; sd = fmaxf(sd, sqrtf(fmaxf(0.0f, gq[i] / k - (double)m[i] * m[i]))); }
      float md = fmaxf(fabsf(m[0]), fmaxf(fabsf(m[1]), fabsf(m[2]))) * 57.29578f;
      float g = amag / k;
      if (md > 0.03f) {
        char q[150];
        snprintf(q, sizeof(q), "the gyro zero has moved %.3f %.3f %.3f deg/s since it was measured - take the new zero and save?",
                 m[0] * 57.29578f, m[1] * 57.29578f, m[2] * 57.29578f);
        askPush(ASK_BIAS, 0, 0, 0, 0, 0, v3(S.gyroBias[0] + m[0], S.gyroBias[1] + m[1], S.gyroBias[2] + m[2]), q);
        p += snprintf(sum + p, sizeof(sum) - p, " gyro zero off %.3f deg/s (asked) |", md); problems++;
      } else p += snprintf(sum + p, sizeof(sum) - p, " gyro zero ok |");
      if (fabsf(g - 1.0f) > 0.05f) { p += snprintf(sum + p, sizeof(sum) - p, " |g| %.3f - recalibrate (cubi2_setup) |", g); problems++; }
      else p += snprintf(sum + p, sizeof(sum) - p, " |g| %.3f ok |", g);
      if (sd * 57.29578f > 0.3f) { p += snprintf(sum + p, sizeof(sum) - p, " IMU noisy %.2f deg/s - its mount? |", sd * 57.29578f); problems++; }
      else p += snprintf(sum + p, sizeof(sum) - p, " noise %.2f deg/s ok |", sd * 57.29578f);
      float vib = DIST.vibMg();
      if (vib > 15.0f) { p += snprintf(sum + p, sizeof(sum) - p, " the table vibrates %.0f mg |", vib); problems++; }
      else p += snprintf(sum + p, sizeof(sum) - p, " vibration %.0f mg ok |", vib);
    }
  }
  if (vbatAvg > 5.0f) {
    float c = vbatAvg / BAT_CELLS;
    p += snprintf(sum + p, sizeof(sum) - p, " battery %.1f V %.0f %%%s |", vbatAvg, batteryPct(vbatAvg), c < 3.65f ? " - charge it" : "");
    if (c < 3.65f) problems++;
  }
  if (!canOk) { p += snprintf(sum + p, sizeof(sum) - p, " !! drivers not found"); problems++; }
  else {
    float vmin = 99, vmax = 0; int bad = 0;
    for (int i = 0; i < 3; i++) {
      uint8_t nd = S.nodeId[i]; const TmSpec *sp = tmNodes[nd].spec; if (!sp) continue;
      float v = 0; uint8_t e = 0, ce = 0, me = 0;
      if (tmReadF(nd, sp->vbus, v)) { vmin = fminf(vmin, v); vmax = fmaxf(vmax, v); }
      tmReadU8(nd, sp->errors, e); tmReadU8(nd, sp->ctrl_errors, ce); tmReadU8(nd, sp->motor_errors, me);
      if (e || ce || me) {
        bad++; char q[150];
        snprintf(q, sizeof(q), "driver %c has errors (system %d controller %d motor %d) - restart it?", AXIS_NAME[i], e, ce, me);
        askPush(ASK_DRIVER, 0, i, 0, 0, 0, v3(0, 0, 0), q);
      }
    }
    if (bad) { p += snprintf(sum + p, sizeof(sum) - p, " drivers: %d with errors (asked)", bad); problems++; }
    else p += snprintf(sum + p, sizeof(sum) - p, " drivers ok");
    if (vmax > 0 && vmax - vmin > 0.6f) { p += snprintf(sum + p, sizeof(sum) - p, " | their voltages differ %.1f V - a power wire loose?", vmax - vmin); problems++; }
  }
  say(sum);
  if (!problems) say("  all fine");
}
static void selfCheck(bool atStart) { askHold = askCount == 0; selfCheckBody(atStart); askRelease(); }

// standing still (wheels off) for 10 s: the gyro should read 0. If it does not, it asks (at most every 10 min).
// (25/10/2026) wheels off: digital spikes from the IMU are reported once a minute (a loose plug shows here first)
static void spikeWatch(uint32_t now) {
  static uint32_t last = 0; static int was = 0;
  if (now - last < 60000UL) return;
  last = now;
  if (state != RUNNING && DIST.spikes - was >= 3) sayf("the IMU gave %d single-reading spikes in the last minute - a loose wire on its cable, or electrical noise?", DIST.spikes - was);
  was = DIST.spikes;
}
static void biasWatch(uint32_t now) {
  static double bs[3] = {0, 0, 0}; static int bn = 0; static Vec3 a0 = {0, 0, 0}; static uint32_t lastAsk = 0;
  bool ok = imuOk && coachOn && (state == OFF || state == FAULT) && !anySpinDown() && len(wheelRad) < 5.0f && !FW.active;
  Vec3 g = v3(IMU.gyrImu[0], IMU.gyrImu[1], IMU.gyrImu[2]), acc = v3(IMU.acc[0], IMU.acc[1], IMU.acc[2]);
  if (ok && bn > 0 && (len(g) > 0.026f || len(sub(acc, a0)) > 0.02f)) ok = false;   // 1.5 deg/s, or it moved
  if (!ok) { bn = 0; bs[0] = bs[1] = bs[2] = 0; return; }
  if (bn == 0) a0 = acc;
  for (int i = 0; i < 3; i++) bs[i] += IMU.gyrImu[i];
  if (++bn < LOOP_HZ * 10) return;
  float m[3] = {(float)(bs[0] / bn), (float)(bs[1] / bn), (float)(bs[2] / bn)};
  bn = 0; bs[0] = bs[1] = bs[2] = 0;
  float md = fmaxf(fabsf(m[0]), fmaxf(fabsf(m[1]), fabsf(m[2]))) * 57.29578f;
  if (md > 0.05f && askCount == 0 && (lastAsk == 0 || now - lastAsk > 600000UL)) {
    lastAsk = now;
    char q[150];
    snprintf(q, sizeof(q), "standing still for 10 s the gyro read %.3f %.3f %.3f deg/s (should be 0) - take the new zero and save?",
             m[0] * 57.29578f, m[1] * 57.29578f, m[2] * 57.29578f);
    askPush(ASK_BIAS, 0, 0, 0, 0, 0, v3(S.gyroBias[0] + m[0], S.gyroBias[1] + m[1], S.gyroBias[2] + m[2]), q);
  }
}

// ---------------------------------------------------------------------
static void command(String l) {
  l.trim();
  if (!l.length()) return;
  if (fwCommand(l, state != RUNNING && state != ARMED && !anySpinDown() && len(wheelRad) < SPIN_IDLE_RAD, say)) return;   // wheels really still   // (24/10/2026) a new program over the Wi-Fi
  if (l == "ping") { LINK.println("pong"); return; }
  if (l == "help") { say("go corner | go edge x|y|z (on the corner: lowers onto it) | go sphere | go navel 0-7 | go arc | sphere mass KG | roll DEG | fwd CM | back CM | left DEG | right DEG | navel 0-7 | cg | halt | front | speed CM/S | turnrate DEG/S | where | swing | stop | zero | zero hang | k A R W | learn L | status | show | save | tele on|off | drivers | dump | fix | up X Y Z | filter HZ | shake HZ [W] | twist F | spin F | fall DEG | wheelmax RPM | dump all | start DEG | corner from edges | zero hang N | slope | slope turn | yes | no | check | coach on|off | profile | profile load ..."); return; }
  // (22/10/2026) the flight recorder: every 4th line (60 a second) of the last balancing, oldest first
  if (l == "dump" || l == "dump all") {   // (23/10/2026) 'dump all': every line, 250 a second
    int stepK = (l == "dump all") ? 1 : 4;
    if (state == RUNNING) { say("stop first"); return; }
    if (!recCount) { say("nothing recorded - 'go', then 'dump' after it stops"); return; }
    if (recSphere) say("t s | position cm x y, heading deg | rate (world) deg/s x y z | wheel rpm x y z | torque mNm x y z");
    else if (recArc) say("t s | across deg, its balance point deg, along deg | rate deg/s x y z | wheel rpm x y z | torque mNm x y z");
    else say("t s | tilt deg x y z | rate deg/s x y z | wheel rpm x y z | torque mNm x y z");
    int start = (recHead - recCount + REC_N) % REC_N;
    for (int k = 0; k < recCount; k += stepK) {
      const RecRow &r = rec[(start + k) % REC_N];
      sayf("%.3f | %+.2f %+.2f %+.2f | %+.1f %+.1f %+.1f | %+.0f %+.0f %+.0f | %+.0f %+.0f %+.0f",
           r.t, r.ax, r.ay, r.az, r.rx, r.ry, r.rz, r.wx, r.wy, r.wz, r.tx, r.ty, r.tz);
      if ((k / stepK) % 20 == 19) delay(20);    // the Wi-Fi link keeps up
    }
    say("end of dump");
    return;
  }
  // (22/10/2026) the 3 drivers: found? state, errors - nothing moves
  if (l == "drivers") {
    if (state == RUNNING) { say("stop first"); return; }
    canOk = false; findDrivers();
    for (int i = 0; i < 3; i++) driverReport(i);
    return;
  }
  if (l == "stop") {
    bool wasRun = state == RUNNING;
    if (wasRun) lastRunSecs = (millis() - runSince) / 1000.0f;
    keepLearned(); driversOff(); state = OFF; why[0] = 0; sphereOn = false; navelOn = -1; arcOn = false;
    sayf("off - wheels %.0f %.0f %.0f rpm%s", wheelRad.x * 9.549f, wheelRad.y * 9.549f, wheelRad.z * 9.549f, anySpinDown() ? " - braking" : "");
    if (wasRun) afterRun();                  // (25/10/2026) the coach
    return;
  }
  // (25/10/2026) THE COACH: the answers, the self-check, the profiles
  if (l == "yes" || l == "y" || l == "no" || l == "n") { askAnswer(l.startsWith("y")); return; }
  if (l == "check") { selfCheck(false); return; }
  if (l == "coach on" || l == "coach off") { coachOn = l == "coach on"; say(coachOn ? "coach on - after each try: how it went, and at most one change (asked)" : "coach off (the balance point still asks)"); return; }
  if (l == "profile") { printProfile(); return; }
  if (l.startsWith("profile load ")) { loadProfile(l.substring(13)); return; }
  if (l.startsWith("go")) {
    if (FW.active) { say("a new program is being received - wait, or 'fw abort'"); return; }
    int m = MODE_CORNER;
    if (l.endsWith("edge x")) m = MODE_EDGE_X;
    else if (l.endsWith("edge y")) m = MODE_EDGE_Y;
    else if (l.endsWith("edge z")) m = MODE_EDGE_Z;
    int nv = -1;                                   // (24/10/2026) 'go navel N'
    if (l.startsWith("go navel")) {
      if (l.length() != 10 || l.charAt(9) < '0' || l.charAt(9) > '7') { say("go navel 0-7"); return; }
      nv = l.charAt(9) - '0';
    }
    if (state == RUNNING && (nv >= 0 || navelOn >= 0 || arcOn || l == "go arc")) { say("stop first"); return; }
    if (state == RUNNING) {
      if (!sphereOn && mode == MODE_CORNER && m != MODE_CORNER && !lowering) {     // (24/10/2026) lower onto that edge
        lowerC = BAL.d; lowerE = v3(S.up[m][0], S.up[m][1], S.up[m][2]);
        lowerTotal = acosf(fmaxf(-1.0f, fminf(1.0f, dot(lowerC, lowerE)))); lowerAng = 0;
        lowerTo = m; lowerSince = millis(); lowering = true;
        recHead = 0; recCount = 0; recLowering = true;   // the recorder: a lowering is no try for 'fix'
        sayf("lowering onto %s (%.0f deg) - it falls the last part, braked", MODE_NAME[m], lowerTotal * 57.29578f);
      } else if (!sphereOn && mode != MODE_CORNER && l == "go corner")
        sayf("lift it by hand onto corner 0 - the corner takes over within %.0f deg", armDegFor(MODE_CORNER));
      else say(lowering ? "lowering - wait" : "stop first");
      return;
    }
    if (!imuOk) { say("no IMU - cannot start"); return; }
    if (!canOk) findDrivers();
    if (!canOk) { say("drivers not found - cannot start (battery on?)"); return; }
    if (!S.wheelChecked) { say("the wheel test is not done - run cubi2_setup: wheels, save"); return; }
    if (anySpinDown() || len(wheelRad) > SPIN_IDLE_RAD) { say("a wheel is still turning - wait until it stops"); return; }
    // (22/10/2026) a driver keeps an error until it restarts (e.g. 'controller 1' = its current went over
    // its trip level when the wheel was stopped at full speed) and then refuses to start: restart it here
    {
      bool restarted = false;
      for (int i = 0; i < 3; i++) {
        uint8_t n = S.nodeId[i]; const TmSpec *s = tmNodes[n].spec;
        uint8_t e = 0, ce = 0, me = 0;
        tmReadU8(n, s->errors, e); tmReadU8(n, s->ctrl_errors, ce); tmReadU8(n, s->motor_errors, me);
        if (e || ce || me) {
          sayf("driver %c had errors (system %d controller %d motor %d) - restarted", AXIS_NAME[i], e, ce, me);
          tmCall(n, s->reset); restarted = true;
        }
      }
      if (restarted) { canOk = false; say("wait 10 s (type nothing), then 'go' again"); return; }
    }
    if (askCount) { askCount = 0; say("(the open question is dropped - after this try it asks again what still applies)"); }
    if (l == "go sphere") {                // (23/10/2026) the cube in the sphere: no take-over angle
      SPH.begin(UPF.up);
      SPH.tMax = S.iqLimit * KT_NM_PER_A;
      uint32_t now = millis();
      if (!driversStart()) { fault("drivers did not start"); return; }
      state = RUNNING; sphereOn = true; navelOn = -1; arcOn = false; runSince = now; recHead = 0; recCount = 0; recSphere = true; recNavel = false; recArc = false; recT0 = now; why[0] = 0;
      DIST.reset(); distT0 = now;
      say("SPHERE - holding. 'front' is where wheel X's side points now. fwd CM | back CM | left DEG | right DEG | halt | stop");
      return;
    }
    if (l == "go arc") {                           // (25/10/2026) the sphere on an arc
      if (!allNavelsMeasured()) {                  // (4/11/2026) the arcs come from the 8 navels
        char missing[40] = ""; int p = 0;
        for (int j = 0; j < 8; j++) if (!navelMeasured[j]) p += snprintf(missing + p, sizeof(missing) - p, " %d", j);
        sayf("first the navels:%s - hang the sphere from each ('zero hang N', then yes); then 'go arc'", missing);
        return;
      }
      circlesFromNavels(navelD);
      navelOn = -1; arcOn = true; applyMode(MODE_CORNER);
      float off; int k = ArcCtl::nearest(UPF.up, off);
      state = ARMED; armOkSince = 0; liftOkSince = 0; recLowering = false; why[0] = 0;
      sayf("ready: arc (circle %d, now %.1f deg from its balance point). Hold the sphere on the arc, upright across it - it takes over within %.0f deg.",
           k, off * 57.29578f, ARC_ARM_DEG);
      return;
    }
    if (nv >= 0) {                                 // (24/10/2026) the sphere on a navel: like 'go corner'
      arcOn = false; navelOn = nv; applyNavel(nv);
      state = ARMED; armOkSince = 0; liftOkSince = 0; recLowering = false; why[0] = 0;
      sayf("ready: navel %d (now %.1f deg from its point). Hold the sphere on navel %d, slowly bring it upright - it takes over within %.0f deg.",
           nv, tiltFromNavel(nv), nv, armDegFor(MODE_CORNER));
      return;
    }
    navelOn = -1; arcOn = false;
    applyMode(m);
    state = ARMED; armOkSince = 0; liftOkSince = 0; recLowering = false; why[0] = 0;
    sayf("ready: %s. Hold the cube on its %s, slowly bring it upright - it takes over within %.0f deg.",
         MODE_NAME[m], m == MODE_CORNER ? "corner" : "edge", armDegFor(m));
    return;
  }
  // (23/10/2026) ZERO BY HANGING. The cube hangs from corner 0 (the hanging plate in the tip insert,
  // one string sliding through its 2 rings, over a smooth nail): the center of gravity is then exactly
  // under corner 0, so "down" seen by the cube = its true balance direction. Corner only.
  // (25/10/2026) THE FLOOR'S SLOPE. Lying on a face, gravity should be straight along that face's axis; what is
  // across it is the floor's slope + the IMU's own error. Turned half a turn on the same spot, the floor's part
  // turns round with the cube and the IMU's part does not: half the difference = the floor, half the sum = the IMU.
  if (l == "slope" || l == "slope turn") {
    if (state == RUNNING || state == ARMED) { say("stop first"); return; }
    static Vec3 t1 = {0, 0, 0}; static int face1 = -1;
    Vec3 u; float sw;
    if (!hangRead(u, sw)) { sayf("it moves (%.1f deg/s) - lay it still on a face, then again", sw); return; }
    Vec3 dn = u;                                               // straight down, in the cube (hangRead gives minus the accelerometer)
    int f = 0; for (int i = 1; i < 3; i++) if (fabsf(comp(dn, i)) > fabsf(comp(dn, f))) f = i;
    float c = fabsf(comp(dn, f));
    if (c < cosf(10.0f / 57.29578f)) { sayf("it does not lie on a face (%.0f deg from one) - lay it flat on a face", acosf(c) * 57.29578f); return; }
    Vec3 tv = dn; setc(tv, f, 0.0f);                            // what is across the face: its tilt (rad)
    char face[4] = {comp(dn, f) > 0 ? '+' : '-', "xyz"[f], 0, 0};
    if (l == "slope") {
      t1 = tv; face1 = f * 2 + (comp(dn, f) > 0);
      sayf("lying on face %s: tilted %.2f deg (the floor and the IMU together). Turn it half a turn on the same spot, then 'slope turn'", face, len(tv) * 57.29578f);
      return;
    }
    if (face1 != f * 2 + (comp(dn, f) > 0)) { say("first 'slope' on this face, then turn it half a turn and 'slope turn'"); return; }
    Vec3 fl = mul(sub(t1, tv), 0.5f), im = mul(add(t1, tv), 0.5f);
    sayf("the floor slopes %.2f deg | the IMU reads %.2f deg off on face %s (its own error, it stays)", len(fl) * 57.29578f, len(im) * 57.29578f, face);
    if (len(fl) * 57.29578f > 1.0f) say("  more than 1 deg: the sphere rolls downhill on it - a flatter place, or something under the low side");
    face1 = -1;
    return;
  }
  // (25/10/2026) 'zero hang' = corner 0 always (the hanging plate in its tip insert); 'zero hang N' = navel N of the
  // sphere (the U hook on the navel's center screw, the string over its bar). Both ASK 'keep it and save?'.
  if (l == "zero hang" || l.startsWith("zero hang ")) {
    if (state == RUNNING) { say("stop first"); return; }
    int nv = -1;
    if (l.length() > 9) {
      String a = l.substring(10); a.trim();
      if (a.length() != 1 || a.charAt(0) < '0' || a.charAt(0) > '7') { say("zero hang (corner 0)  |  zero hang N (the sphere hanging from navel N, 0-7)"); return; }
      nv = a.charAt(0) - '0';
    }
    Vec3 u; float sw;
    if (!hangRead(u, sw)) { sayf("it still swings (%.1f deg/s) - wait until it hangs still (turning about the string is fine), then again", sw); return; }
    char q[150];
    if (nv < 0) {
      if (mode != MODE_CORNER) applyMode(MODE_CORNER);
      Vec3 prev = v3(S.up[MODE_CORNER][0], S.up[MODE_CORNER][1], S.up[MODE_CORNER][2]);
      float moved = angDeg(u, prev);
      if (moved > 15.0f) { sayf("that is %.1f deg from the balance point it has - not taken (hanging from corner 0?)", moved); return; }
      S.up[MODE_CORNER][0] = u.x; S.up[MODE_CORNER][1] = u.y; S.up[MODE_CORNER][2] = u.z; pointHas[MODE_CORNER] = false;
      applyMode(MODE_CORNER);
      sayf("corner 0 point set by hanging: up %+.4f %+.4f %+.4f (moved %.2f deg)", u.x, u.y, u.z, moved);
      snprintf(q, sizeof(q), "corner 0 point set by hanging (moved %.2f deg) - keep it and save?", moved);
      askPush(ASK_HANG, MODE_CORNER, -1, 0, 0, 0, prev, q);
    } else {
      Vec3 prev = navelD[nv];
      float moved = angDeg(u, prev);
      if (moved > 15.0f) { sayf("that is %.1f deg from navel %d's point - not taken (hanging from navel %d?)", moved, nv, nv); return; }
      navelD[nv] = u;
      sayf("navel %d point set by hanging: up %+.4f %+.4f %+.4f (moved %.2f deg)", nv, u.x, u.y, u.z, moved);
      snprintf(q, sizeof(q), "navel %d point set by hanging (moved %.2f deg) - keep it and save?", nv, moved);
      askPush(ASK_HANG, MODE_CORNER, nv, 0, 0, 0, prev, q);
    }
    return;
  }
  if (l == "zero") {
    if (state == RUNNING) { say("stop first"); return; }
    Vec3 sum = v3(0, 0, 0);
    uint32_t t0 = millis();
    while (millis() - t0 < 1000) { IMU.read(); sum = add(sum, v3(IMU.acc[0], IMU.acc[1], IMU.acc[2])); delay(4); }
    Vec3 u = unit(sum);
    if (mode != MODE_CORNER) {                     // an edge: keep it across the edge
      setc(u, mode - MODE_EDGE_X, 0.0f); u = unit(u);
    }
    float moved = acosf(fminf(1.0f, dot(u, v3(S.up[mode][0], S.up[mode][1], S.up[mode][2])))) * 57.29578f;
    if (moved > 15.0f) { sayf("that is %.1f deg from the expected balance point - not taken (wrong corner or edge?)", moved); return; }
    S.up[mode][0] = u.x; S.up[mode][1] = u.y; S.up[mode][2] = u.z; pointHas[mode] = false;
    applyMode(mode);
    sayf("balance point of %s set (moved %.2f deg) - 'save' to keep it", MODE_NAME[mode], moved);
    return;
  }
  // (22/10/2026) THE BALANCE POINT FROM THE FLIGHT RECORDER. Over a try the cube is still on average, so
  // gravity = the wheels' mean torque: mgl x (where it sat - true point) = mean torque. So
  //   true point = where it sat - mean torque / mgl          (mgl = mass x g x height of the CG)
  // A stuck wheel (torque at the limit, corner dump 3) and a calm try (torque ~0, dumps 6-7) both fit.
  // (The first 'fix' used only the lean - right for a stuck wheel, the WRONG way for a calm try.)
  // It starts from the point used in that try, so typing 'fix' twice gives the same point.
  // The first 0.25 s (the take-over) left out. With 'learn' on the point moves during the try - use 'learn 0'.
  if (l == "fix") {
    if (state == RUNNING) { say("stop first"); return; }
    if (recSphere || recNavel) { say("that was the sphere - 'fix' is for the corner and the edges (a navel learns its point while it balances)"); return; }
    if (recLowering) { say("that try was a lowering onto an edge - 'fix' needs a plain try (go ... stop ... fix)"); return; }
    Vec3 b; float leanDeg, tq, moved;
    if (!fixEstimate(b, leanDeg, tq, moved)) { say("too short - it must balance at least half a second, then 'fix'"); return; }
    int m = recMode;
    if (moved > 10.0f) { sayf("that would move it %.1f deg - not taken (try 'zero hang' first)", moved); return; }
    S.up[m][0] = b.x; S.up[m][1] = b.y; S.up[m][2] = b.z; pointHas[m] = false;
    if (mode == m) applyMode(m);
    sayf("%s: sat %.2f deg off its point, mean wheel torque %.0f mNm - point moved %.2f deg: up %+.4f %+.4f %+.4f",
         MODE_NAME[m], leanDeg, tq, moved, b.x, b.y, b.z);
    if (recLearn > 0) say("  (learn was on in that try - for a clean 'fix' use 'learn 0')");
    say(moved < 0.3f ? "  (small - the point is good) 'save' to keep it" : "  'save' to keep it, then 'go' again");
    return;
  }
  // (22/10/2026) the balance point typed in - to go back to a point that worked ('show' prints it).
  // For the mode of the last 'go'; not more than 15 deg from the point it has now (a typing mistake).
  if (l.startsWith("up ")) {
    if (state == RUNNING) { say("stop first"); return; }
    float x, y, z;
    if (sscanf(l.c_str() + 3, "%f %f %f", &x, &y, &z) != 3) { say("up X Y Z  (as 'show' prints it)"); return; }
    Vec3 u = unit(v3(x, y, z));
    if (mode != MODE_CORNER) { setc(u, mode - MODE_EDGE_X, 0.0f); u = unit(u); }
    float moved = acosf(fminf(1.0f, dot(u, unit(v3(S.up[mode][0], S.up[mode][1], S.up[mode][2]))))) * 57.29578f;
    if (moved > 15.0f) { sayf("that is %.1f deg from the point it has - not taken (a typing mistake?)", moved); return; }
    S.up[mode][0] = u.x; S.up[mode][1] = u.y; S.up[mode][2] = u.z; pointHas[mode] = false;
    applyMode(mode);
    sayf("balance point of %s: %+.4f %+.4f %+.4f (moved %.2f deg) - 'save' to keep it", MODE_NAME[mode], u.x, u.y, u.z, moved);
    return;
  }
  if (l.startsWith("sphere mass ")) {               // (4/11/2026) the weight of this cube in its sphere
    if (state == RUNNING || state == ARMED) { say("stop first"); return; }
    float kilograms = l.substring(12).toFloat();
    if (kilograms < 1.0f || kilograms > 4.0f) { say("sphere mass KG - the cube in its sphere, with the battery (about 2.055)"); return; }
    sphereMass = kilograms; CGM.m = kilograms;
    sayf("sphere mass %.3f kg - 'save' to keep it", kilograms);
    return;
  }
  if (l.startsWith("k ")) {
    float a, r, w;
    if (sscanf(l.c_str() + 2, "%f %f %f", &a, &r, &w) == 3) {
      if (arcOn) {                                 // (25/10/2026) on an arc: the arc's gains (memory only)
        ARC.Ka = a; ARC.Kr = r; ARC.Kw = w;
        sayf("gains arc: angle %.3f  rate %.4f  wheel %.6f (until power off)", a, r, w);
        return;
      }
      if (navelOn >= 0) {                          // (24/10/2026) on a navel: the navels' gains ('save' keeps them - 4/11/2026)
        navelK[0] = a; navelK[1] = r; navelK[2] = w;
        BAL.Ka = a; BAL.Kr = r; BAL.Kw = w;
        sayf("gains navel: angle %.3f  rate %.4f  wheel %.6f - 'save' to keep them", a, r, w);
        return;
      }
      S.K[mode][0] = a; S.K[mode][1] = r; S.K[mode][2] = w;
      if (state == RUNNING) { BAL.Ka = a; BAL.Kr = r; BAL.Kw = w; }   // keep what it learned
      else applyMode(mode);
      sayf("gains %s: angle %.3f  rate %.4f  wheel %.6f", MODE_NAME[mode], a, r, w);
    } else say("k ANGLE RATE WHEEL");
    return;
  }
  // (22/10/2026) THE CORNER'S BALANCE POINT FROM THE 3 EDGES. Each balanced edge tells the direction of
  // the center of gravity across that edge: edge x -> gy/gz, edge y -> gx/gz, edge z -> gx/gy. The best
  // fit of the three (least squares) is the corner's balance direction - by hand on a corner it is
  // hard to find. Do it after the 3 edges have balanced and learned (and 'save' after).
  if (l == "corner from edges") {
    if (state == RUNNING) { say("stop first"); return; }
    const float *ux = S.up[MODE_EDGE_X], *uy = S.up[MODE_EDGE_Y], *uz = S.up[MODE_EDGE_Z];
    if (fabsf(ux[2]) < 0.1f || fabsf(uy[2]) < 0.1f || fabsf(uz[1]) < 0.1f) { say("the edges do not look right - not taken"); return; }
    float rx = ux[1] / ux[2], ry = uy[0] / uy[2], rz = uz[0] / uz[1];
    // unknowns gx, gy (gz = 1): rows (0,1)=rx, (1,0)=ry, (1,-rz)=0
    float a11 = 1 + 1, a12 = -rz, a22 = 1 + rz * rz, b1 = ry, b2 = rx;   // normal equations
    float det = a11 * a22 - a12 * a12;
    if (fabsf(det) < 1e-6f) { say("cannot fit"); return; }
    float gx = (b1 * a22 - a12 * b2) / det, gy = (a11 * b2 - a12 * b1) / det;
    Vec3 g = unit(v3(gx, gy, 1.0f));
    Vec3 old = v3(S.up[MODE_CORNER][0], S.up[MODE_CORNER][1], S.up[MODE_CORNER][2]);
    float moved = acosf(fminf(1.0f, dot(g, unit(old)))) * 57.29578f;
    if (moved > 8.0f) { sayf("that is %.1f deg from the corner's point - not taken", moved); return; }
    S.up[MODE_CORNER][0] = g.x; S.up[MODE_CORNER][1] = g.y; S.up[MODE_CORNER][2] = g.z; pointHas[MODE_CORNER] = false;
    if (mode == MODE_CORNER) applyMode(mode);
    sayf("corner balance point from the 3 edges: %+.4f %+.4f %+.4f (moved %.2f deg) - 'save' to keep it", g.x, g.y, g.z, moved);
    return;
  }
  if (l.startsWith("start ")) {
    float a = l.substring(6).toFloat();
    if (a < 4.0f || a > 8.0f) { say("start 4 ... 8 deg"); return; }   // (26/10/2026) at least 4: easy to hold by hand
    if (mode == MODE_CORNER) S.armDeg = a; else edgeArmDeg = a;
    sayf("%s: takes over within %.1f deg", mode == MODE_CORNER ? "corner" : "edges", a);
    return;
  }
  if (l.startsWith("filter ")) {
    float f = l.substring(7).toFloat();
    if (f < 0 || f > 100) { say("filter 0 ... 100 Hz (0 = off)"); return; }
    RATEF.set(f, LOOP_HZ); RATEF.reset(IMU.gyr);
    sayf("rate filter %.0f Hz%s", f, f <= 0 ? " (off)" : "");
    return;
  }
  if (l.startsWith("shake ")) {          // (23/10/2026) the shake filter on the torque: shake HZ [Q] ('save' keeps it)
    String a = l.substring(6); a.trim();
    int sp = a.indexOf(' ');
    float f = (sp < 0 ? a : a.substring(0, sp)).toFloat();
    float q = sp < 0 ? SHAKE.q : a.substring(sp + 1).toFloat();
    if (f < 0 || f > 100 || (f > 0 && (q < 0.3f || q > 5.0f))) { say("shake 0 ... 100 Hz (0 = off), width 0.3 ... 5 (smaller = wider)"); return; }
    if (state == RUNNING) { say("stop first"); return; }
    SHAKE.set(f, q, LOOP_HZ); SHAKE.reset();
    if (f <= 0) say("shake filter off"); else sayf("shake filter %.0f Hz, width %.1f", f, q);
    return;
  }
  if (l.startsWith("fall ")) {           // (23/10/2026) gives up beyond this tilt ('save' keeps it)
    float a = l.substring(5).toFloat();
    if (a < 3.0f || a > 30.0f) { say("fall 3 ... 30 deg"); return; }
    if (state == RUNNING) { say("stop first"); return; }
    S.stopDeg = a; sayf("stops (tipped over) beyond %.1f deg", a);
    return;
  }
  if (l.startsWith("wheelmax ")) {       // (23/10/2026) stops above this wheel speed ('save' keeps it)
    float r = l.substring(9).toFloat();
    if (r < 1000.0f || r > 4100.0f) { say("wheelmax 1000 ... 4100 rpm (the drivers allow at most 4390)"); return; }
    if (state == RUNNING) { say("stop first"); return; }
    S.wheelMaxRad = r / 9.549f; sayf("stops (a wheel too fast) above %.0f rpm", r);
    return;
  }
  if (l.startsWith("spin ")) {           // (23/10/2026) how hard the wheels' spin along the diagonal is slowed ('save' keeps it)
    float f = l.substring(5).toFloat();
    if (f < 0 || f > 5) { say("spin 0 ... 5 (x the wheel gain; 1 normal, 0 = off)"); return; }
    BAL.spin = f; sayf("spin %.2f", f);
    return;
  }
  // (25/10/2026) on an arc: 'roll DEG' (out and back, at most 30), 'halt'
  if (l.startsWith("roll ") || (l == "halt" && arcOn)) {
    if (!(state == RUNNING && arcOn)) { say("first 'go arc' (and let it take over)"); return; }
    if (l == "halt") { ARC.halt(); say("halt - it rolls back to where it started"); return; }
    float v = l.substring(5).toFloat();
    if (fabsf(v) < 1 || fabsf(v) > 30) { say("roll DEG: 1 ... 30 (or -1 ... -30)"); return; }
    ARC.roll(v / 57.29578f); sayf("rolling %+.0f deg along the arc and back, at %.0f deg/s", v, ARC.rollRate * 57.29578f);
    return;
  }
  // (23/10/2026) THE SPHERE: moves (while 'go sphere' runs) and its measurements (wheels off)
  // (24/10/2026) 'navel N': the sphere rolls until navel N (corner N of the cube) is straight down, and stays.
  // The names: NAVEL in the sphere mode, CORNER in the cube mode.
  if (l.length() == 8 && l.startsWith("corner ") && l.charAt(7) >= '0' && l.charAt(7) <= '7') { say("in the sphere it is 'navel N'"); return; }
  // (24/10/2026) 'cg': the center of gravity, measured by holding the sphere on each of the 8 navels (center_of_gravity.h)
  if (l == "cg") {
    if (!(state == RUNNING && sphereOn)) { say("first 'go sphere'"); return; }
    CGM.start(); say("measuring the center of gravity on the 8 navels (~1-2 min; 'halt' stops it) ...");
    return;
  }
  if (CGM.active && (l.startsWith("navel ") || l.startsWith("fwd ") || l.startsWith("back ") || l.startsWith("left ") || l.startsWith("right ") || l == "halt")) {
    CGM.stop(); SPH.halt(); say("cg measurement stopped");
    if (l == "halt") return;
  }
  if (l.length() == 7 && l.startsWith("navel ") && l.charAt(6) >= '0' && l.charAt(6) <= '7') {
    if (!(state == RUNNING && sphereOn)) { say("first 'go sphere'"); return; }
    int k = l.charAt(6) - '0'; SPH.restOn(k);
    sayf("rolling onto navel %d%s", k, k == 0 ? "" : " (the wheels hold it there unless the center of gravity is at the center)");
    return;
  }
  if (l.startsWith("fwd ") || l.startsWith("back ") || l.startsWith("left ") || l.startsWith("right ") || l == "halt" || l == "front") {
    if (!(state == RUNNING && sphereOn)) { say("first 'go sphere'"); return; }
    if (l == "halt") { SPH.halt(); sayf("halt - at %+.1f %+.1f cm", SPH.px * 100, SPH.py * 100); return; }
    if (l == "front") { SPH.front(); say("front = the heading now"); return; }
    int sp = l.indexOf(' '); float v = l.substring(sp + 1).toFloat();
    if (l.startsWith("fwd ") || l.startsWith("back ")) {
      if (v <= 0 || v > 500) { say("fwd / back 1 ... 500 cm"); return; }
      SPH.move((l.startsWith("fwd ") ? 1.0f : -1.0f) * v / 100.0f);
      sayf("%s %.0f cm at %.0f cm/s", l.startsWith("fwd ") ? "forward" : "back", v, SPH.speed * 100);
    } else {
      if (v <= 0 || v > 720) { say("left / right 1 ... 720 deg"); return; }
      SPH.turn((l.startsWith("left ") ? 1.0f : -1.0f) * v / 57.29578f);
      sayf("turning %s %.0f deg at %.0f deg/s", l.startsWith("left ") ? "left" : "right", v, SPH.turnRate * 57.29578f);
    }
    return;
  }
  if (l.startsWith("speed ")) {
    float v = l.substring(6).toFloat();
    if (v < 1 || v > 40) { say("speed 1 ... 40 cm/s"); return; }
    SPH.speed = v / 100.0f; sayf("sphere speed %.0f cm/s", v); return;
  }
  if (l.startsWith("turnrate ")) {
    float v = l.substring(9).toFloat();
    if (v < 5 || v > 120) { say("turnrate 5 ... 120 deg/s"); return; }
    SPH.turnRate = v / 57.29578f; sayf("sphere turning %.0f deg/s", v); return;
  }
  // 'where': how the sphere rests (wheels off, still). Its center of gravity hangs straight under the center,
  // so the cube's 'down' = the direction of the center of gravity from the sphere's center.
  if (l == "where") {
    if (state == RUNNING) { say("stop first"); return; }
    Vec3 sa = v3(0, 0, 0); float gmax = 0; uint32_t t0 = millis(); int na = 0;
    while (millis() - t0 < 2000) { IMU.read(); sa = add(sa, v3(IMU.acc[0], IMU.acc[1], IMU.acc[2])); na++; gmax = fmaxf(gmax, len(v3(IMU.gyr[0], IMU.gyr[1], IMU.gyr[2]))); delay(4); }
    if (gmax > 0.05f) { sayf("it still moves (%.1f deg/s) - let it settle, then 'where' again", gmax * 57.29578f); return; }
    Vec3 u = unit(sa), dn = mul(u, -1.0f), c0 = unit(v3(-1, -1, -1));
    // (24/10/2026) the measured gravity itself and its length: gravity is always 1 g, so from ~15 still positions the
    // accelerometer's own offset comes out (the IMU calibration for hanging the sphere)
    { Vec3 ga = mul(sa, 1.0f / (na > 0 ? na : 1)); sayf("g %+.5f %+.5f %+.5f  |g| %.5f  (measured, cube frame, %d readings)", ga.x, ga.y, ga.z, len(ga), na); }
    sayf("up %+.4f %+.4f %+.4f (cube frame)", u.x, u.y, u.z);
    sayf("the center of gravity points %+.3f %+.3f %+.3f from the sphere's center; navel 0 is %.1f deg from straight down",
         dn.x, dn.y, dn.z, acosf(fminf(1.0f, dot(dn, c0))) * 57.29578f);
    return;
  }
  // 'swing': push the sphere a little and type 'swing' - it times the rocking for 20 s (wheels off).
  // A rolling pendulum: T = 2 pi sqrt(I / (m g e)), I = Ic + m R^2 -> e = the center of gravity's distance from the center.
  if (l == "swing") {
    if (state == RUNNING) { say("stop first"); return; }
    say("timing the rocking for 20 s ...");
    static float gx[1000], gy[1000], gz[1000];
    int n = 0; uint32_t t0 = millis();
    while (n < 1000) { IMU.read(); gx[n] = IMU.gyr[0]; gy[n] = IMU.gyr[1]; gz[n] = IMU.gyr[2]; n++; while (millis() - t0 < (uint32_t)(n * 20)) {} }
    // the main axis of the rocking: power iteration on the rates' covariance
    float C[9] = {0}; for (int k = 0; k < n; k++) { float a[3] = {gx[k], gy[k], gz[k]}; for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) C[3 * i + j] += a[i] * a[j]; }
    Vec3 ax = v3(1, 0.3f, 0.1f);
    for (int it = 0; it < 30; it++) ax = unit(v3(C[0] * ax.x + C[1] * ax.y + C[2] * ax.z, C[3] * ax.x + C[4] * ax.y + C[5] * ax.z, C[6] * ax.x + C[7] * ax.y + C[8] * ax.z));
    int cross_ = 0; float tFirst = 0, tLast = 0, peak = 0; int sgn = 0;
    for (int k = 0; k < n; k++) {
      float r = gx[k] * ax.x + gy[k] * ax.y + gz[k] * ax.z; peak = fmaxf(peak, fabsf(r));
      int sg = r > 0.0087f ? 1 : (r < -0.0087f ? -1 : 0);          // 0.5 deg/s hysteresis
      if (sg && sg != sgn) { if (sgn) { cross_++; if (cross_ == 1) tFirst = k * 0.02f; tLast = k * 0.02f; } sgn = sg; }
    }
    if (cross_ < 3 || peak < 0.035f) { sayf("not enough rocking (%d crossings, peak %.1f deg/s) - push it a little more, then 'swing'", cross_, peak * 57.29578f); return; }
    float T = 2.0f * (tLast - tFirst) / (cross_ - 1);
    float I = SPHERE_IC + sphereMass * SPH.Rs * SPH.Rs;
    float e = 4.0f * 3.14159265f * 3.14159265f * I / (sphereMass * 9.81f * T * T);
    sayf("rocking: period %.2f s (%d half-swings, peak %.0f deg/s) -> the center of gravity is %.1f mm from the center", T, cross_ - 1, peak * 57.29578f, e * 1000);
    return;
  }
  if (l.startsWith("twist ")) {          // (22/10/2026) the gain against the twist on the tip, part of Kr ('save' keeps it)
    float t = l.substring(6).toFloat();
    if (t < 0 || t > 1) { say("twist 0 ... 1 (part of the rate gain, corner only)"); return; }
    BAL.twist = t; sayf("twist %.2f", t);
    return;
  }
  if (l.startsWith("learn ")) { S.trimRate = l.substring(6).toFloat(); BAL.learn = S.trimRate; sayf("learn %.2f", S.trimRate); return; }
  if (l == "save") {
    // writing the flash stops the Teensy for a moment - never while balancing
    if (state == RUNNING) { say("hold the cube, 'stop', then 'save' (what it learned is kept)"); return; }
    doSave();
    say("saved");
    for (int m = 0; m < MODE_COUNT; m++) if (pointHas[m]) { say("  (the balance point it learned is not in it - answer its question: yes / no)"); break; }
    return;
  }
  if (l == "tele on") { tele = true; return; }
  if (l == "tele off") { tele = false; return; }
  if (l == "status" || l == "show") {
    sayf("%s %s | tilt %.2f deg | battery %.1f V %.0f %% | wheels %.0f %.0f %.0f rpm | %s",
         STATE_NAME[state], modeLabel(), state == RUNNING ? BAL.tiltDeg : (arcOn ? arcOffDeg() : navelOn >= 0 ? tiltFromNavel(navelOn) : tiltFromMode(mode)), vbat, batteryPct(vbatAvg),
         wheelRad.x * 9.549f, wheelRad.y * 9.549f, wheelRad.z * 9.549f, why);
    if (l == "show") sayf("  firmware %s", FIRMWARE_VERSION);
    if (l == "show") sayf("  rate filter %.0f Hz | shake filter %.0f Hz width %.1f | twist %.2f | spin %.2f | learn %.2f /s | takes over within: corner %.1f, edges %.1f deg", RATEF.hz, SHAKE.hz, SHAKE.q, BAL.twist, BAL.spin, S.trimRate, S.armDeg, edgeArmDeg);
    if (l == "show") sayf("  stops beyond %.1f deg (fall) | above %.0f rpm (wheelmax)", S.stopDeg, S.wheelMaxRad * 9.549f);
    if (l == "show") sayf("  sphere: speed %.0f cm/s | turning %.0f deg/s", SPH.speed * 100, SPH.turnRate * 57.29578f);
    if (sphereOn) sayf("  sphere: at %+.1f %+.1f cm, heading %+.0f deg%s", SPH.px * 100, SPH.py * 100, SPH.psi * 57.29578f, SPH.moving ? " (moving)" : "");
    if (l == "show")
      for (int m = 0; m < MODE_COUNT; m++)
        sayf("  %-7s up %+.4f %+.4f %+.4f  k %.3f %.4f %.6f", MODE_NAME[m], S.up[m][0], S.up[m][1], S.up[m][2], S.K[m][0], S.K[m][1], S.K[m][2]);
    if (l == "show")                               // (25/10/2026) learned, not kept yet
      for (int m = 0; m < MODE_COUNT; m++)
        if (pointHas[m]) sayf("  %-7s learned %+.4f %+.4f %+.4f (%.2f deg away, not kept yet)", MODE_NAME[m], pointCand[m].x, pointCand[m].y, pointCand[m].z,
                              angDeg(pointCand[m], v3(S.up[m][0], S.up[m][1], S.up[m][2])));
    if (l == "show") sayf("  coach %s%s", coachOn ? "on" : "off", EXP.on ? " - a changed setting waits for the next try to be compared" : "");
    if (l == "show")                               // (24/10/2026) the sphere's navels
      for (int j = 0; j < 8; j++)
        sayf("  navel %d up %+.4f %+.4f %+.4f", j, navelD[j].x, navelD[j].y, navelD[j].z);
    if (l == "show") sayf("  navels  k %.3f %.4f %.6f", navelK[0], navelK[1], navelK[2]);
    if (l == "show") {                             // (4/11/2026) which navels are measured, the weight
      char list[40] = ""; int p = 0;
      for (int j = 0; j < 8; j++) p += snprintf(list + p, sizeof(list) - p, " %d%s", j, navelMeasured[j] ? "" : "(model)");
      sayf("  navels hung:%s  |  sphere mass %.3f kg", list, sphereMass);
    }
    return;
  }
  say("unknown - 'help'");
}

static void readCommands(Stream &in, String &buf) {
  while (in.available()) {
    char c = in.read();
    if (c == '\r') continue;
    if (c == '\n') { String l = buf; buf = ""; command(l); }
    else if (buf.length() < 600) buf += c;        // (25/10/2026) 'profile load ...' is ~300 characters
  }
}

static void telemetry() {
  // (24/10/2026) + the cube's orientation for the 3D view in the app: "q" = the quaternion w x y z
  // (cube -> world); in the sphere also "p" (position cm), "h" (heading deg), "mv" (0 hold, 1 rolling,
  // 2 turning). Outside the sphere the orientation comes from the tilt only (no heading).
  Mat3 R;
  if (sphereOn) R = SPH.R;
  else { Sphere tmp; tmp.begin(UPF.up); R = tmp.R; }
  const float *m = R.m; float qw, qx, qy, qz, tr = m[0] + m[4] + m[8];
  if (tr > 0) { float s = sqrtf(tr + 1.0f) * 2; qw = 0.25f * s; qx = (m[7] - m[5]) / s; qy = (m[2] - m[6]) / s; qz = (m[3] - m[1]) / s; }
  else if (m[0] > m[4] && m[0] > m[8]) { float s = sqrtf(1.0f + m[0] - m[4] - m[8]) * 2; qw = (m[7] - m[5]) / s; qx = 0.25f * s; qy = (m[1] + m[3]) / s; qz = (m[2] + m[6]) / s; }
  else if (m[4] > m[8]) { float s = sqrtf(1.0f + m[4] - m[0] - m[8]) * 2; qw = (m[2] - m[6]) / s; qx = (m[1] + m[3]) / s; qy = 0.25f * s; qz = (m[5] + m[7]) / s; }
  else { float s = sqrtf(1.0f + m[8] - m[0] - m[4]) * 2; qw = (m[3] - m[1]) / s; qx = (m[2] + m[6]) / s; qy = (m[5] + m[7]) / s; qz = 0.25f * s; }
  char b[640];
  snprintf(b, sizeof(b),
           "{\"st\":\"%s\",\"m\":\"%s\",\"tilt\":%.2f,\"v\":%.2f,\"bp\":%.0f,\"w\":[%.0f,%.0f,%.0f],\"i\":[%.2f,%.2f,%.2f],"
           "\"up\":[%.3f,%.3f,%.3f],\"k\":[%.3f,%.4f,%.6f],\"sph\":%d,\"q\":[%.4f,%.4f,%.4f,%.4f],\"p\":[%.1f,%.1f],\"h\":%.1f,\"mv\":%d,\"nv\":%d,\"why\":\"%s\",\"ask\":%d,\"aq\":\"%.100s\"}",
           STATE_NAME[state], sphereOn ? "sphere" : modeLabel(), state == RUNNING ? BAL.tiltDeg : (arcOn ? arcOffDeg() : navelOn >= 0 ? tiltFromNavel(navelOn) : tiltFromMode(mode)), vbat, batteryPct(vbatAvg),
           wheelRad.x * 9.549f, wheelRad.y * 9.549f, wheelRad.z * 9.549f, iqCmd[0], iqCmd[1], iqCmd[2],
           UPF.up.x, UPF.up.y, UPF.up.z, navelOn >= 0 ? navelK[0] : S.K[mode][0], navelOn >= 0 ? navelK[1] : S.K[mode][1], navelOn >= 0 ? navelK[2] : S.K[mode][2],
           sphereOn ? 1 : 0, qw, qx, qy, qz, SPH.px * 100.0f, SPH.py * 100.0f, SPH.psi * 57.29578f, SPH.moving, (SPH.moving == 3) ? (SPH.navelReached ? SPH.navel : 10 + SPH.navel) : -1, why,
           askCount ? askId : 0, askCount ? askQ[0].text : "");
  LINK.println(b);
  if (tele) Serial.println(b);
}

// ---------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  LINK.begin(115200);
  static uint8_t txb[2048]; LINK.addMemoryForWrite(txb, sizeof(txb));   // telemetry never waits
  static uint8_t rxb[4096]; LINK.addMemoryForRead(rxb, sizeof(rxb));    // (24/10/2026) 64 hex lines of a new program at once
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 1500) {}
  analogReadResolution(10);
  sayf("=== Cubi 2 - BRAIN %s ===", FIRMWARE_VERSION);
  if (!cubiLoad(S)) say("!! no saved settings: run cubi2_setup first (model values used)");
  if (S.armDeg < 4.0f) S.armDeg = 4.0f;    // (26/10/2026) it takes over within 4 deg at least: easy to hold by hand
  imuOk = IMU.begin(&S);
  say(imuOk ? "IMU ok" : "!! IMU not found");
  RATEF.set(15.0f, LOOP_HZ);               // (22/10/2026) the rate filter, 15 Hz
  SHAKE.set(25.0f, 0.7f, LOOP_HZ);         // (23/10/2026) the shake filter, 25 Hz, wide
  BAL.twist = 0.3f;                        // (23/10/2026) the value that worked (corner dumps 6-9)
  {                                        // (23/10/2026) the brain's own saved settings, if there are
    BrainExtra e; EEPROM.get(BRAIN_EXTRA_ADDR, e);
    if (e.magic == BRAIN_EXTRA_MAGIC && e.crc == e.check()) {
      BAL.twist = e.twist; RATEF.set(e.rateHz, LOOP_HZ); SHAKE.set(e.shakeHz, e.shakeQ, LOOP_HZ); edgeArmDeg = fmaxf(4.0f, e.edgeArm); BAL.spin = e.spin;
    }
  }
  tmBegin();
  // (21/10/2026) the drivers got power with the Teensy: they run their bootloader first.
  // A question in those seconds would keep them there - so wait until 10 s after power-up.
  if (millis() < TM_BOOT_MS) { say("waiting 10 s for the motor drivers to start ..."); while (millis() < TM_BOOT_MS) delay(10); }
  findDrivers();
  for (int j = 0; j < 8; j++) navelD[j] = unit(v3(NAVEL_UP[j][0], NAVEL_UP[j][1], NAVEL_UP[j][2]));   // (24/10/2026)
  {                                        // (25/10/2026) the navel points saved from 'zero hang N', if there are
    NavelSaved nv; EEPROM.get(NAVEL_SAVE_ADDR, nv);
    NavelSaved1 nv1; EEPROM.get(NAVEL_SAVE_ADDR, nv1);
    if (nv.magic == NAVEL_SAVE_MAGIC && nv.crc == nv.check()) {
      for (int j = 0; j < 8; j++) {
        Vec3 u = v3(nv.d[j][0], nv.d[j][1], nv.d[j][2]);
        if (len(u) > 0.5f) { navelD[j] = unit(u); navelMeasured[j] = nv.measured[j]; }
      }
    } else if (nv1.magic == NAVEL_SAVE_MAGIC1 && nv1.crc == nv1.check()) {   // the first kind: all 8 were hung
      for (int j = 0; j < 8; j++) {
        Vec3 u = v3(nv1.d[j][0], nv1.d[j][1], nv1.d[j][2]);
        if (len(u) > 0.5f) { navelD[j] = unit(u); navelMeasured[j] = true; }
      }
    }
  }
  {                                        // (4/11/2026) this cube's weight and the navels' gains, if saved
    SphereSaved sphere; EEPROM.get(SPHERE_SAVE_ADDR, sphere);
    if (sphere.magic == SPHERE_SAVE_MAGIC && sphere.crc == sphere.check()) {
      if (sphere.mass > 1.0f && sphere.mass < 4.0f) sphereMass = sphere.mass;
      if (sphere.navelGains[0] > 0) for (int j = 0; j < 3; j++) navelK[j] = sphere.navelGains[j];
    }
    CGM.m = sphereMass;
  }
  circlesFromNavels(navelD);               // (4/11/2026) the arcs, from the 8 navel points
  applyMode(MODE_CORNER);
  selfCheck(true);                         // (25/10/2026) the self-check at power-up
  say("type 'go corner' (or 'help')");
}

void loop() {
  static String bufU, bufL;
  static elapsedMicros tick;
  static uint32_t n = 0;
  readCommands(Serial, bufU);
  readCommands(LINK, bufL);
  tmPoll();
  if (tick < 1000000 / LOOP_HZ) return;
  float dt = tick * 1e-6f;
  tick = 0;
  n++;
  if (dt > 0.02f) dt = 0.02f;

  // --- sensors
  if (imuOk) {
    if (!IMU.read()) { if (state == RUNNING) fault("IMU read failed"); }
    UPF.update(v3(IMU.acc[0], IMU.acc[1], IMU.acc[2]), v3(IMU.gyr[0], IMU.gyr[1], IMU.gyr[2]), dt);
    RATEF.step(IMU.gyr, gyrF);
    float gd[3] = {IMU.gyr[0] * 57.29578f, IMU.gyr[1] * 57.29578f, IMU.gyr[2] * 57.29578f};
    DIST.step(gd, IMU.acc, (millis() - distT0) * 0.001f, dt);   // (25/10/2026)
  }
  Vec3 w = v3(0, 0, 0);
  uint32_t now = millis();
  bool fresh = true;
  for (int i = 0; i < 3; i++) {
    TmNode &t = tmNodes[S.nodeId[i]];
    setc(w, S.wheelAxis[i], S.wheelSign[i] * t.vel * TICKS_TO_RAD);
    if (now - t.velMs > 60) fresh = false;
  }
  wheelRad = w;

  // --- battery (driver X's bus voltage, else board Y's divider)
  float vb = tmNodes[S.nodeId[0]].vbus;
  vbat = (vb > 5.0f) ? vb : analogRead(BAT_PIN) * 3.3f / 1023.0f * 11.0f;
  if (vbat > 5.0f && vbat < VBAT_STOP) { if (!lowBatSince) lowBatSince = now; } else lowBatSince = 0;
  if (vbat > 5.0f) vbatAvg = (vbatAvg < 5.0f) ? vbat : vbatAvg + (vbat - vbatAvg) * dt / 2.0f;
  {                                        // (23/10/2026) a line at every 10 % down (also while balancing)
    static int lastStep = -1;
    if (vbatAvg > 5.0f && now > TM_BOOT_MS + 3000) {
      int st = (int)(batteryPct(vbatAvg) / 10.0f);
      if (lastStep < 0 || st > lastStep + 1) lastStep = st;          // start, or a new battery
      else if (st < lastStep) { lastStep = st; sayf("battery %.0f %% (%.1f V)", batteryPct(vbatAvg), vbatAvg); }
    }
  }

  // --- states
  if (state == ARMED) {
    float t = navelOn >= 0 ? tiltFromNavel(navelOn) : tiltFromMode(mode);
    int arcK = -1;
    if (arcOn) { float off; arcK = ArcCtl::nearest(UPF.up, off); t = off * 57.29578f; }
    if (t < (arcOn ? ARC_ARM_DEG : armDegFor(mode)) && len(v3(IMU.gyr[0], IMU.gyr[1], IMU.gyr[2])) < 0.3f) {
      if (!armOkSince) armOkSince = now;
      if (now - armOkSince > 300) {
        if (arcOn) { ARC.begin(arcK, UPF.up); ARC.tMax = S.iqLimit * KT_NM_PER_A; }
        else if (navelOn >= 0) applyNavel(navelOn); else applyMode(mode);
        SHAKE.reset();
        if (driversStart()) {
          state = RUNNING; runSince = now; recHead = 0; recCount = 0; recSphere = false; recNavel = navelOn >= 0 || arcOn; recArc = arcOn; recMode = mode; recD = BAL.d; recLearn = BAL.learn; recT0 = now;
          DIST.reset(); distT0 = now;              // (25/10/2026) the disturbances of this try
          if (arcOn) sayf("BALANCING on the arc (circle %d)", ARC.circle);
          else if (navelOn >= 0) sayf("BALANCING on navel %d", navelOn); else say("BALANCING");
        }
        else fault("drivers did not start");
      }
    } else armOkSince = 0;
  }
  if (state == RUNNING && sphereOn) {     // (23/10/2026) the sphere
    BAL.T = SPH.step(v3(IMU.acc[0], IMU.acc[1], IMU.acc[2]), v3(gyrF[0], gyrF[1], gyrF[2]), w, dt);
    // the recorder: position cm x y and heading deg in the 'tilt' columns (recAdd prints them x 57.3)
    recAdd(v3(SPH.px * 100.0f / 57.29578f, SPH.py * 100.0f / 57.29578f, SPH.psi), SPH.wW, w, BAL.T);
    if (fabsf(w.x) > S.wheelMaxRad || fabsf(w.y) > S.wheelMaxRad || fabsf(w.z) > S.wheelMaxRad) fault("a wheel too fast");
    else if (!fresh && now - runSince > 200) fault("a driver went silent");
    else if (lowBatSince && now - lowBatSince > 2000) fault("battery low - charge it");
    else {
      for (int i = 0; i < 3; i++) {
        uint8_t nid = S.nodeId[i];
        const TmSpec *s = tmNodes[nid].spec;
        float iq = S.wheelSign[i] * comp(BAL.T, S.wheelAxis[i]) / KT_NM_PER_A;
        iqCmd[i] = iq;
        tmWriteF(nid, s->iq_setpoint, iq);
      }
      static int lastMoving = 0; static bool lastReached = false;
      if (lastMoving && !SPH.moving) sayf("done - at %+.1f %+.1f cm, heading %+.0f deg", SPH.px * 100, SPH.py * 100, SPH.psi * 57.29578f);
      if (SPH.moving == 3 && SPH.navelReached && !lastReached && !CGM.active) sayf("resting on navel %d - at %+.1f %+.1f cm", SPH.navel, SPH.px * 100, SPH.py * 100);
      if (CGM.active) { static char cgOut[300]; if (CGM.step(SPH, v3(gyrF[0], gyrF[1], gyrF[2]), w, dt, cgOut, sizeof(cgOut))) say(cgOut); }
      lastMoving = SPH.moving; lastReached = SPH.moving == 3 && SPH.navelReached;
    }
  } else if (state == RUNNING && arcOn) {   // (25/10/2026) the sphere on an arc
    bool done = ARC.step(UPF.up, v3(gyrF[0], gyrF[1], gyrF[2]), w, dt);
    Vec3 T = ARC.T;
    if (SHAKE.hz > 0) {                                  // the same shake band out of the torque
      float tin[3] = {ARC.Traw.x, ARC.Traw.y, ARC.Traw.z}, tout[3];
      SHAKE.step(tin, tout);
      T = ARC.clampKeep(v3(tout[0], tout[1], tout[2]));
    }
    BAL.T = T; BAL.tiltDeg = fabsf(ARC.acrossDeg());
    // the recorder: across now, its learned point, how far along (deg, in the 'tilt' columns)
    recAdd(v3(ARC.s, ARC.sStar, ARC.pos), v3(gyrF[0], gyrF[1], gyrF[2]), w, T);
    if (fabsf(ARC.acrossDeg()) > ARC_FALL_DEG) fault("left the arc (fell to the side)");
    else if (fabsf(w.x) > S.wheelMaxRad || fabsf(w.y) > S.wheelMaxRad || fabsf(w.z) > S.wheelMaxRad) fault("a wheel too fast");
    else if (!fresh && now - runSince > 200) fault("a driver went silent");
    else if (lowBatSince && now - lowBatSince > 2000) fault("battery low - charge it");
    else {
      for (int i = 0; i < 3; i++) {
        uint8_t nid = S.nodeId[i];
        const TmSpec *s = tmNodes[nid].spec;
        float iq = S.wheelSign[i] * comp(T, S.wheelAxis[i]) / KT_NM_PER_A;
        iqCmd[i] = iq;
        tmWriteF(nid, s->iq_setpoint, iq);
      }
      if (done) sayf("roll done - back where it started; wheels %.0f %.0f %.0f rpm", w.x * 9.549f, w.y * 9.549f, w.z * 9.549f);
    }
  } else if (state == RUNNING) {
    if (lowering) {                                      // (24/10/2026) the balance point moves to the edge's
      lowerAng = fminf(lowerTotal, lowerAng + LOWER_RATE * dt);
      BAL.d = slerp3(lowerC, lowerE, lowerTotal > 1e-5f ? lowerAng / lowerTotal : 1.0f);
    }
    BAL.step(UPF.up, v3(gyrF[0], gyrF[1], gyrF[2]), w, dt, !lowering && now - runSince > 2000);   // the filtered rate
    if (lowering && acosf(fmaxf(-1.0f, fminf(1.0f, dot(UPF.up, lowerE)))) < LOWER_LAND) {   // on the edge: it takes over
      pointCand[MODE_CORNER] = lowerC; pointHas[MODE_CORNER] = true;   // what the corner learned (asked after the try)
      lowering = false; applyMode(lowerTo); SHAKE.reset();
      BAL.phi = cross(UPF.up, BAL.d); BAL.tiltDeg = tiltFromMode(mode);   // the tilt against the NEW point (the stop check below)
      runSince = now; recHead = 0; recCount = 0; recMode = mode; recD = BAL.d; recLearn = BAL.learn; recT0 = now; recLowering = false;
      sayf("on %s - BALANCING (%.1f s)", MODE_NAME[mode], (now - lowerSince) / 1000.0f);
    }
    if (SHAKE.hz > 0) {                                  // (23/10/2026) the shake band out of the torque
      float tin[3] = {BAL.Traw.x, BAL.Traw.y, BAL.Traw.z}, tout[3];
      SHAKE.step(tin, tout);
      BAL.T = BAL.clampT(v3(tout[0], tout[1], tout[2]));
    }
    recAdd(BAL.phi, v3(gyrF[0], gyrF[1], gyrF[2]), w, BAL.T);
    // (24/10/2026) ON AN EDGE, LIFTED BY HAND ONTO THE CORNER: within the corner's take-over angle and still
    // for 0.3 s (the same rule as 'go corner'), the corner takes over - no 'stop', no new 'go'
    if (mode != MODE_CORNER && !lowering) {
      if (tiltFromMode(MODE_CORNER) < armDegFor(MODE_CORNER) && len(v3(IMU.gyr[0], IMU.gyr[1], IMU.gyr[2])) < 0.3f) {
        if (!liftOkSince) liftOkSince = now;
        if (now - liftOkSince > 300) {
          int was = mode; keepLearned(); applyMode(MODE_CORNER); SHAKE.reset(); liftOkSince = 0;
          BAL.phi = cross(UPF.up, BAL.d); BAL.tiltDeg = tiltFromMode(mode);   // the tilt against the corner's point, not the edge's
          runSince = now; recHead = 0; recCount = 0; recMode = mode; recD = BAL.d; recLearn = BAL.learn; recT0 = now;
          sayf("on the corner - BALANCING (from %s)", MODE_NAME[was]);
        }
      } else liftOkSince = 0;
    }
    // on an edge only the tilt ABOUT the edge can make it fall (lifting a corner by hand is not falling)
    float tiltStop = (BAL.edgeAxis >= 0) ? asinf(fminf(1.0f, fabsf(comp(BAL.phi, BAL.edgeAxis)))) * 57.29578f : BAL.tiltDeg;
    if (lowering && now - lowerSince > LOWER_MS) fault("did not reach the edge");
    else if ((lowering ? BAL.tiltDeg : tiltStop) > (lowering ? 40.0f : S.stopDeg)) fault("tipped over");   // a face is 45 deg from the edge's point
    else if (fabsf(w.x) > S.wheelMaxRad || fabsf(w.y) > S.wheelMaxRad || fabsf(w.z) > S.wheelMaxRad) fault("a wheel too fast");
    else if (!fresh && now - runSince > 200) fault("a driver went silent");
    else if (lowBatSince && now - lowBatSince > 2000) fault("battery low - charge it");
    else {
      for (int i = 0; i < 3; i++) {
        uint8_t nid = S.nodeId[i];
        const TmSpec *s = tmNodes[nid].spec;
        float iq = S.wheelSign[i] * comp(BAL.T, S.wheelAxis[i]) / KT_NM_PER_A;
        iqCmd[i] = iq;
        tmWriteF(nid, s->iq_setpoint, iq);
      }
    }
  }
  // --- ask the drivers for what we need next time
  if (canOk) {
    for (int i = 0; i < 3; i++) { uint8_t nid = S.nodeId[i]; tmSend(nid, tmNodes[nid].spec->vel_estimate, nullptr, 0, true); }
    if (n % 25 == 0) { uint8_t nid = S.nodeId[0]; tmSend(nid, tmNodes[nid].spec->vbus, nullptr, 0, true); }
  }
  // not balancing: tell the drivers 'idle' again every 0.1 s - one lost frame cannot leave a wheel on
  if (canOk && state != RUNNING && (anySpinDown() || n % (LOOP_HZ / 10) == 5)) driversOff();
  if (n % (LOOP_HZ / 10) == 0) telemetry();
  if (n % (LOOP_HZ * 5) == 0 && vbat > 5.0f && vbat < VBAT_WARN && state != RUNNING) sayf("battery %.1f V %.0f %% - charge soon", vbat, batteryPct(vbatAvg));
  biasWatch(now);                          // (25/10/2026) standing still: is the gyro zero still right?
  spikeWatch(now);
}
