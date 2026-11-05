// =====================================================================
// Cubi 2 - SETUP program for the Teensy 4.1 (board Y).
// Run it once after the build, step by step, from the Cubi 2 app (Wi-Fi
// Cubi2, Live Cubi > Terminal) - or from
// the Serial Monitor (115200 baud, "Newline"). "help" lists the commands.
// UPLOAD with the battery OUT; USE it with the battery IN and the USB cable OUT.
//
//   scan            which drivers answer, their firmware, voltage, state
//   find            drivers on ANY CAN id 1 ... 255, and two drivers sharing one id
//   id OLD NEW      give a driver a new CAN id (ONLY that driver plugged in)
//   cal ID          calibrate one driver (its wheel must turn freely)
//   limits          current and speed limits into all 3 drivers, saved there
//   spin ID AMPS    turn one wheel for 2 s (AMPS up to 1.0; use 1.0 - under ~0.4 A the heavy wheel does not start)
//   enc ID          the wheel angle from the driver's sensor - turn the wheel by hand
//   ical ID [AMPS]  show (or set) the calibration current of one driver
//   imu             live IMU in the cube frame (any key stops)
//   i2c             which I2C devices answer, on the 3 I2C buses (finds a missing or swapped IMU)
//   bias            gyroscope zero (cube still on the table, 3 s)
//   face x|y|z      orientation: the cube lies on that face (do x, y and z)
//   wheels          wheel test: which wheel is which, which way (cube in the hands)
//   corner 0|7      the balance corner (0 = the corner between the 3 wheels)
//   battery         battery voltage (drivers and board Y)
//   link            test the cable to the Wi-Fi board on board X
//   show            all settings
//   save            write the settings into the Teensy (they survive power off)
//   defaults        back to the settings from the model (not saved yet)
//
// Libraries: FlexCAN_T4, Wire, EEPROM (with the Teensy platform) and "SparkFun BMI270"
// (platformio.ini, lib_deps). Board: Teensy 4.1. VS Code + PlatformIO: open this folder.
// =====================================================================
#include <Arduino.h>
#include "settings.h"
#include "motor_drivers.h"
#include "motion_sensor.h"

const char *FIRMWARE_VERSION = "2026110501";   // the date, then the change of that day (YYYYMMDDNN)

CubiSettings S;
ImuCube IMU;
bool imuOk = false;
float faceVec[3][3];         // imu-frame "up" measured on face x, y, z
bool faceDone[3] = {false, false, false};

#define LINK Serial2           // to the Wi-Fi board: RX2 pin 7, TX2 pin 8
#define BAT_PIN A13            // pin 27: the battery through 100k / 10k (board Z, SENSE)

// Everything this program says goes to BOTH the USB Serial Monitor and the
// Cubi 2 app (through the Wi-Fi board) - and it listens to both. So with the
// battery in, the setup runs from the app, without a USB cable.
class Tee : public Print {
 public:
  size_t write(uint8_t c) override { Serial.write(c); LINK.write(c); return 1; }
  size_t write(const uint8_t *b, size_t n) override { Serial.write(b, n); LINK.write(b, n); return n; }
};
Tee OUT;

// ---------------------------------------------------------------------
static String readFrom(Stream &in, String &buf) {
  while (in.available()) {
    char c = in.read();
    if (c == '\r') continue;
    if (c == '\n') { String l = buf; buf = ""; l.trim(); return l; }
    if (buf.length() < 80) buf += c;
  }
  return String();
}
static String readLine() {
  static String bu, bl;
  String l = readFrom(Serial, bu);
  if (l.length()) return l;
  return readFrom(LINK, bl);
}
static bool keyPressed() {                         // any line or key, from USB or the app
  bool k = false;
  while (Serial.available()) { Serial.read(); k = true; }
  while (LINK.available()) { LINK.read(); k = true; }
  return k;
}
static void waitMs(uint32_t ms) {
  uint32_t t0 = millis();
  while (millis() - t0 < ms) tmPoll();
}
static void ok(const char *t) { OUT.print("  OK   "); OUT.println(t); }
static void bad(const char *t) { OUT.print("  !!   "); OUT.println(t); }

static float batteryBoardY() {
  // 100k over 10k: Vbat = Vpin * 11. Teensy: 3.3 V = 1023.
  return analogRead(BAT_PIN) * 3.3f / 1023.0f * 11.0f;
}

// ---------------------------------------------------------------------
// (21/10/2026) a driver stuck in its bootloader (motor_drivers.h): restart it
static void bootloaderReset(uint8_t n) {
  tmBootReset(n, tmNodes[n].hash);
  OUT.println("  -> the driver was stuck in its BOOTLOADER: it was restarted. Wait 10 s (type nothing), then 'scan'");
}

static void cmdScan() {
  OUT.println("Scanning CAN ids 1 ... 8");
  int found = 0;
  for (uint8_t n = 1; n <= TM_MAX_NODES; n++) {
    if (!tmDetect(n)) continue;
    found++;
    TmNode &t = tmNodes[n];
    OUT.printf("  id %d  firmware %s  hash %lu", n, t.fw[0] ? t.fw : "?", (unsigned long)t.hash);
    if (tmIsBootloader(t.hash)) { OUT.println(); bootloaderReset(n); continue; }
    if (!t.spec) { OUT.println("  -> UNKNOWN to this program (see the guide: 'Driver firmware')"); continue; }
    OUT.printf("  (table %s)", t.spec->version);
    float v = 0; uint8_t st = 0, cal = 0, e = 0, me = 0;
    tmReadF(n, t.spec->vbus, v);
    tmReadU8(n, t.spec->state, st);
    tmReadU8(n, t.spec->calibrated, cal);
    tmReadU8(n, t.spec->errors, e);
    tmReadU8(n, t.spec->motor_errors, me);
    OUT.printf("  %.1f V  %s  %s  errors %d/%d", v, tmStateName(st), cal ? "calibrated" : "NOT calibrated", e, me);
    for (int i = 0; i < 3; i++) if (S.nodeId[i] == n) OUT.printf("  = driver %c", AXIS_NAME[i]);
    OUT.println();
  }
  if (!found) bad("no driver answers: battery on? CAN plugs in? termination plug in driver Z?");
  else OUT.printf("  %d driver(s) found. Driver X = id %d, Y = id %d, Z = id %d.\n", found, S.nodeId[0], S.nodeId[1], S.nodeId[2]);
}

static bool needNode(int n) {
  if (n < 1 || n > TM_MAX_NODES) { bad("driver ids are 1 ... 8"); return false; }
  if (!tmNodes[n].spec && !tmDetect(n)) { bad("that driver does not answer"); return false; }
  if (tmIsBootloader(tmNodes[n].hash)) { bootloaderReset(n); tmNodes[n].spec = nullptr; return false; }
  if (!tmNodes[n].spec) { bad("that driver's firmware is unknown to this program"); return false; }
  return true;
}

static void cmdId(int from, int to) {
  if (from < 1 || from > 8 || to < 1 || to > 8 || from == to) { bad("ids 1 ... 8, two different"); return; }
  if (!needNode(from)) return;
  if (tmDetect(to)) { bad("the new id is already taken by another driver - unplug the others"); return; }
  const TmSpec *s = tmNodes[from].spec;
  tmWriteU32(from, s->can_id, (uint32_t)to);
  waitMs(100);
  tmCall(to, s->save_config);
  waitMs(500);
  tmCall(to, s->reset);
  // (21/10/2026) after the reset the bootloader runs first: ask nothing for 10 s
  OUT.println("  the driver restarts - wait 10 s ...");
  waitMs(10000);
  if (tmDetect(to) && !tmIsBootloader(tmNodes[to].hash)) ok("the driver now answers on its new id (saved)");
  else bad("no answer on the new id - battery out and in, wait 10 s, then 'scan'");
}

// (21/10/2026) find: drivers on ANY id 1 ... 255 (scan looks at 1 ... 8 only). For each id that
// answers it reads the chip number (uid) 5 times: two drivers on one id answer together and
// their answers clash - the uid then changes or goes missing.
static void cmdFind() {
  OUT.println("Searching CAN ids 1 ... 255 (about 10 s) ...");
  int found = 0;
  for (int n = 1; n <= 255; n++) {
    uint8_t b[8];
    if (tmRead((uint8_t)n, 0, b, 25) < 4) continue;
    found++;
    uint32_t h; memcpy(&h, b, 4);
    const char *ver = "?";
    for (int k = 0; k < TM_SPEC_COUNT; k++) if (TM_SPECS[k].hash == h) ver = TM_SPECS[k].version;
    uint32_t u0 = 0; int same = 0, got = 0;
    for (int k = 0; k < 5; k++) {
      uint32_t u;
      if (tmRead((uint8_t)n, 1, b, 25) >= 4) { memcpy(&u, b, 4); if (!got) u0 = u; got++; if (u == u0) same++; }
      waitMs(5);
    }
    OUT.printf("  id %d  hash %lu (%s)  chip %08lX", n, (unsigned long)h, tmIsBootloader(h) ? "BOOTLOADER" : ver, (unsigned long)u0);
    if (got == 5 && same == 5) OUT.println("  - one driver");
    else OUT.printf("  - the chip number is not steady (%d/5): MORE THAN ONE driver on this id?\n", same);
    if (tmIsBootloader(h)) { tmBootReset((uint8_t)n, h); OUT.println("     it was stuck in its BOOTLOADER: restarted - wait 10 s, then 'scan'"); }
    for (int i = 0; i < 3; i++) if (S.nodeId[i] == n) OUT.printf("     (this program expects driver %c here)\n", AXIS_NAME[i]);
  }
  OUT.printf("  %d id(s) answer.\n", found);
  if (found < 3) OUT.println("  A driver that is missing: no battery to it, its CAN plug, or another CAN speed.");
}

static void cmdCal(int n) {
  if (!needNode(n)) return;
  const TmSpec *s = tmNodes[n].spec;
  OUT.println("  Calibrating: the wheel beeps and turns slowly one way, then back. Hands off. 20-40 s.");
  tmCall(n, s->idle);
  waitMs(200);
  tmCall(n, s->calibrate);
  uint32_t t0 = millis(); uint8_t st = 1; bool started = false;
  while (millis() - t0 < 60000) {
    waitMs(500);
    if (!tmReadU8(n, s->state, st)) continue;
    if (st == 1) started = true;
    if (started && st != 1) break;
    if (!started && millis() - t0 > 3000) break;
    OUT.print(".");
  }
  OUT.println();
  uint8_t cal = 0, me = 0, se = 0, pp = 0; float R = 0, L = 0;
  tmReadU8(n, s->calibrated, cal);
  tmReadU8(n, s->motor_errors, me);
  tmReadU8(n, s->sensor_errors, se);
  tmReadU8(n, s->pole_pairs, pp);
  tmReadF(n, s->motor_R, R);
  tmReadF(n, s->motor_L, L);
  OUT.printf("  R %.3f ohm  L %.1f uH  pole pairs %d  motor errors %d  sensor errors %d\n", R, L * 1e6f, pp, me, se);
  if (cal && !me && !se) {
    tmCall(n, s->save_config);
    waitMs(500);
    ok("calibrated and saved in the driver");
  } else bad("calibration failed - see the guide: 'Calibration fails'");
}

static void cmdLimits() {
  for (int i = 0; i < 3; i++) {
    uint8_t n = S.nodeId[i];
    if (!needNode(n)) continue;
    const TmSpec *s = tmNodes[n].spec;
    tmCall(n, s->idle); waitMs(50);
    tmWriteF(n, s->iq_limit, S.iqLimit); waitMs(20);
    // the drivers refuse more than 600000 ticks/s - stay under it
    tmWriteF(n, s->vel_limit, min((S.wheelMaxRad + 20.0f) / TICKS_TO_RAD, 590000.0f)); waitMs(20);
    tmCall(n, s->save_config); waitMs(500);
    float iq = 0, vl = 0;
    tmReadF(n, s->iq_limit, iq); tmReadF(n, s->vel_limit, vl);
    OUT.printf("  driver %c (id %d): current limit %.1f A, speed limit %.0f rpm\n", AXIS_NAME[i], n, iq, vl / TICKS_PER_TURN * 60.0f);
  }
}

static void idleAll() {
  for (uint8_t n = 1; n <= 3; n++) {               // the 3 drivers' usual ids, known firmware or not
    if (tmNodes[n].spec) { tmWriteF(n, tmNodes[n].spec->iq_setpoint, 0.0f); tmCall(n, tmNodes[n].spec->idle); }
  }
}

// keep the driver's watchdog fed (any frame to it) - and learn its speed
static void feed(uint8_t n) { tmSend(n, tmNodes[n].spec->vel_estimate, nullptr, 0, true); }

static bool startCurrent(uint8_t n) {
  const TmSpec *s = tmNodes[n].spec;
  tmWriteF(n, s->iq_setpoint, 0.0f);
  tmWriteF(n, s->wd_timeout, 0.3f);           // the driver stops by itself if the Teensy goes quiet
  tmWriteU8(n, s->wd_enabled, 1);
  tmCall(n, s->current_mode); waitMs(20);
  tmWriteU8(n, s->mode, 0); waitMs(20);
  tmWriteU8(n, s->state, 2); waitMs(50);
  uint8_t st = 0;
  tmReadU8(n, s->state, st);
  if (st != 2) { tmCall(n, s->idle); bad("the driver did not start - calibrated? ('scan')"); return false; }
  return true;
}

static void cmdSpin(int n, float amps) {
  if (!needNode(n)) return;
  amps = constrain(amps, -1.0f, 1.0f);
  const TmSpec *s = tmNodes[n].spec;
  if (!startCurrent(n)) return;
  tmWriteF(n, s->iq_setpoint, amps);
  uint32_t t0 = millis(), tp = 0;
  float v = 0;
  while (millis() - t0 < 2000) {
    waitMs(20);
    tmReadF(n, s->vel_estimate, v);                  // also feeds the watchdog
    if (millis() - tp >= 250) {
      // (21/10/2026) also the current the driver measures and its state - shows why a wheel does not turn
      tp = millis();
      float iq = 0, sp = 0; uint8_t st = 0;
      tmReadF(n, s->iq_estimate, iq);
      tmReadF(n, s->iq_setpoint, sp);
      tmReadU8(n, s->state, st);
      OUT.printf("  %.0f rpm   current %.2f A (asked %.2f A)   %s\n", v / TICKS_PER_TURN * 60.0f, iq, sp, tmStateName(st));
    }
  }
  tmWriteF(n, s->iq_setpoint, 0.0f);
  uint8_t e = 0, ce = 0, me = 0, se = 0;
  tmReadU8(n, s->errors, e); tmReadU8(n, s->ctrl_errors, ce);
  tmReadU8(n, s->motor_errors, me); tmReadU8(n, s->sensor_errors, se);
  OUT.printf("  errors: system %d  controller %d  motor %d  sensor %d\n", e, ce, me, se);
  tmCall(n, s->idle);
  ok("stopped (the wheel coasts down)");
}

// (22/10/2026) enc ID: the wheel's angle as the driver's sensor sees it - turn the wheel BY HAND.
// One full turn of the wheel must read 1.00 turn (360 deg); a quarter turn 0.25. Any key stops.
static void cmdEnc(int n) {
  if (!needNode(n)) return;
  const TmSpec *s = tmNodes[n].spec;
  tmCall(n, s->idle);                               // the motor free: it does not hold the wheel
  float p0 = 0;
  if (!tmReadF(n, s->pos_estimate, p0)) { bad("no answer from the sensor"); return; }
  OUT.println("  Mark the wheel. Turn it slowly BY HAND, exactly one full turn. Any key stops.");
  while (!keyPressed()) {
    float p = 0; uint8_t se = 0;
    tmReadF(n, s->pos_estimate, p);
    tmReadU8(n, s->sensor_errors, se);
    float t = (p - p0) / TICKS_PER_TURN;
    OUT.printf("  %+.3f turn  (%+.0f deg)   sensor errors %d\n", t, t * 360.0f, se);
    delay(250);
  }
}

// (22/10/2026) ical ID [AMPS]: the calibration current. A heavy wheel needs enough current to follow
// the turning field smoothly during 'cal'; too little and it jerks and slips (wrong pole pairs).
// Without AMPS: only shows it. With AMPS (1 ... 10): sets it - 'cal ID' then saves it in the driver.
static void cmdIcal(int n, float amps, bool set) {
  if (!needNode(n)) return;
  const TmSpec *s = tmNodes[n].spec;
  float v = 0;
  if (set) {
    if (amps < 1.0f || amps > 10.0f) { bad("calibration current 1 ... 10 A"); return; }
    tmCall(n, s->idle); waitMs(20);
    tmWriteF(n, s->motor_I_cal, amps); waitMs(20);
  }
  if (!tmReadF(n, s->motor_I_cal, v)) { bad("no answer"); return; }
  uint8_t pp = 0; float R = 0, L = 0;
  tmReadU8(n, s->pole_pairs, pp); tmReadF(n, s->motor_R, R); tmReadF(n, s->motor_L, L);
  OUT.printf("  driver id %d: calibration current %.1f A   (now: R %.3f ohm  L %.1f uH  pole pairs %d)\n",
             n, v, R, L * 1e6f, pp);
  if (set) OUT.println("  set - now 'cal ID': the calibration uses it, then saves it in the driver");
}

static void cmdImu() {
  if (!imuOk) { bad("no IMU"); return; }
  OUT.println("  cube frame: acc in g, gyro in deg/s. On a face, the 'up' axis reads +1.00. Send anything to stop.");
  while (!keyPressed()) {
    IMU.read();
    float up[3] = {IMU.acc[0], IMU.acc[1], IMU.acc[2]}; norm3(up);
    const float *d = S.up[MODE_CORNER];
    float c = up[0] * d[0] + up[1] * d[1] + up[2] * d[2];
    OUT.printf("  acc %+.2f %+.2f %+.2f   gyro %+7.1f %+7.1f %+7.1f   tilt from the corner balance %.1f deg\n",
                  IMU.acc[0], IMU.acc[1], IMU.acc[2], IMU.gyr[0] * 57.3f, IMU.gyr[1] * 57.3f, IMU.gyr[2] * 57.3f,
                  acosf(constrain(c, -1.0f, 1.0f)) * 57.3f);
    delay(250);                                    // 4 lines a second - the app keeps up
  }
}

static void cmdBias() {
  if (!imuOk) { bad("no IMU"); return; }
  OUT.println("  Keep the cube STILL for 3 s ...");
  float sum[3] = {0, 0, 0}; int k = 0;
  for (int i = 0; i < 3; i++) S.gyroBias[i] = 0;
  uint32_t t0 = millis();
  while (millis() - t0 < 3000) { IMU.read(); for (int i = 0; i < 3; i++) sum[i] += IMU.gyrImu[i]; k++; delay(5); }
  for (int i = 0; i < 3; i++) S.gyroBias[i] = sum[i] / k;
  OUT.printf("  gyro zero %.3f %.3f %.3f deg/s\n", S.gyroBias[0] * 57.3f, S.gyroBias[1] * 57.3f, S.gyroBias[2] * 57.3f);
  ok("done - 'save' to keep it");
}

static void cmdFace(char f) {
  if (!imuOk) { bad("no IMU"); return; }
  int a = f - 'x';
  if (a < 0 || a > 2) { bad("face x, y or z"); return; }
  OUT.printf("  The cube lies on face %c (wheel %c at the bottom). Still for 2 s ...\n", f - 32, f - 32);
  float sum[3] = {0, 0, 0};
  uint32_t t0 = millis();
  while (millis() - t0 < 2000) { IMU.read(); for (int i = 0; i < 3; i++) sum[i] += IMU.accImu[i]; delay(5); }
  norm3(sum);
  memcpy(faceVec[a], sum, sizeof(sum)); faceDone[a] = true;
  OUT.printf("  up in the IMU frame: %+.3f %+.3f %+.3f\n", sum[0], sum[1], sum[2]);
  // compare with the model
  float R0[9]; cubiDefaultR(R0);
  float c = R0[3 * a] * sum[0] + R0[3 * a + 1] * sum[1] + R0[3 * a + 2] * sum[2];
  OUT.printf("  the model expects this face %.1f deg away\n", acosf(constrain(c, -1.0f, 1.0f)) * 57.3f);
  if (faceDone[0] && faceDone[1] && faceDone[2]) {
    float r0[3], r1[3], r2[3];
    memcpy(r0, faceVec[0], 12); norm3(r0);
    float p = faceVec[1][0] * r0[0] + faceVec[1][1] * r0[1] + faceVec[1][2] * r0[2];
    for (int i = 0; i < 3; i++) r1[i] = faceVec[1][i] - p * r0[i];
    norm3(r1);
    r2[0] = r0[1] * r1[2] - r0[2] * r1[1];
    r2[1] = r0[2] * r1[0] - r0[0] * r1[2];
    r2[2] = r0[0] * r1[1] - r0[1] * r1[0];
    float chk = r2[0] * faceVec[2][0] + r2[1] * faceVec[2][1] + r2[2] * faceVec[2][2];
    if (chk < 0.95f) { bad("faces x, y, z do not fit together - repeat them, cube flat and still"); return; }
    for (int i = 0; i < 3; i++) { S.R[i] = r0[i]; S.R[3 + i] = r1[i]; S.R[6 + i] = r2[i]; }
    S.orientChecked = 1;
    ok("orientation measured - 'save' to keep it");
  } else OUT.println("  now the other faces (face x, face y, face z)");
}

static void cmdWheels() {
  if (!imuOk) { bad("no IMU"); return; }
  for (int i = 0; i < 3; i++) if (!needNode(S.nodeId[i])) return;
  OUT.println("  Hold the cube IN THE AIR with both hands, loosely, wheels free.");
  OUT.println("  Each wheel kicks for a moment; the cube twists the other way. Send 'go' to start.");
  while (!Serial.available() && !LINK.available()) tmPoll();
  delay(50);
  keyPressed();
  int axisUsed[3] = {-1, -1, -1};
  for (int i = 0; i < 3; i++) {
    uint8_t n = S.nodeId[i];
    const TmSpec *s = tmNodes[n].spec;
    if (!startCurrent(n)) return;
    float base[3] = {0, 0, 0}, sum[3] = {0, 0, 0}; int k = 0;
    uint32_t t0 = millis();
    while (millis() - t0 < 300) { IMU.read(); for (int j = 0; j < 3; j++) base[j] += IMU.gyr[j]; k++; feed(n); tmPoll(); delay(2); }
    for (int j = 0; j < 3; j++) base[j] /= k;
    // (22/10/2026) a stronger, longer kick: 3 A for 0.2 s gave the 2 kg cube only ~0.01 rad in the hands
    // (wheel ~250 rpm). 6 A for 0.3 s: wheel ~750 rpm, about 4 times the twist.
    tmWriteF(n, s->iq_setpoint, 6.0f);
    t0 = millis();
    uint32_t tu = micros();
    while (millis() - t0 < 300) {
      IMU.read();
      float dt = (micros() - tu) * 1e-6f; tu = micros();
      for (int j = 0; j < 3; j++) sum[j] += (IMU.gyr[j] - base[j]) * dt;
      feed(n); tmPoll(); delay(2);
    }
    float v = 0; tmReadF(n, s->vel_estimate, v);
    tmWriteF(n, s->iq_setpoint, -6.0f);          // brake back
    waitMs(300);
    tmWriteF(n, s->iq_setpoint, 0.0f);
    tmCall(n, s->idle);
    int ax = 0;
    for (int j = 1; j < 3; j++) if (fabsf(sum[j]) > fabsf(sum[ax])) ax = j;
    float other = 0;
    for (int j = 0; j < 3; j++) if (j != ax) other = max(other, fabsf(sum[j]));
    OUT.printf("  driver %c: wheel %.0f rpm, cube twist %+.3f %+.3f %+.3f rad\n", AXIS_NAME[i], v / TICKS_PER_TURN * 60.0f, sum[0], sum[1], sum[2]);
    if (v < 0.05f * TICKS_PER_TURN) { bad("the wheel did not speed up forward - check the driver ('spin')"); return; }
    if (fabsf(sum[ax]) < 0.02f || fabsf(sum[ax]) < 2.0f * other) {
      bad("not clear - hold the cube looser (on open hands, not gripped) and repeat 'wheels'"); return; }
    S.wheelAxis[i] = ax;
    S.wheelSign[i] = (sum[ax] < 0) ? 1 : -1;      // the cube turns against the wheel
    axisUsed[i] = ax;
    OUT.printf("    -> driver %c turns the wheel on axis %c, %s\n", AXIS_NAME[i], 'x' + ax, S.wheelSign[i] > 0 ? "same way (+)" : "reversed (-)");
    waitMs(800);
  }
  if (axisUsed[0] == axisUsed[1] || axisUsed[1] == axisUsed[2] || axisUsed[0] == axisUsed[2]) { bad("two drivers on one axis - repeat"); return; }
  for (int i = 0; i < 3; i++) if (S.wheelAxis[i] != i) OUT.printf("  NOTE: driver %c is on axis %c, not %c - check the CAN ids\n", AXIS_NAME[i], 'x' + S.wheelAxis[i], 'x' + i);
  S.wheelChecked = 1;
  ok("wheel test done - 'save' to keep it");
}

static void cmdBattery() {
  for (int i = 0; i < 3; i++) {
    uint8_t n = S.nodeId[i]; float v = 0;
    if (tmNodes[n].spec || tmDetect(n)) if (tmNodes[n].spec && tmReadF(n, tmNodes[n].spec->vbus, v)) OUT.printf("  driver %c: %.2f V\n", AXIS_NAME[i], v);
  }
  OUT.printf("  board Y (SENSE): %.2f V   (6 cells: full 25.2 V, empty 21.0 V - charge below 22.0 V)\n", batteryBoardY());
}

static void cmdLink() {
  while (LINK.available()) LINK.read();
  LINK.println("ping");
  uint32_t t0 = millis(); String r;
  while (millis() - t0 < 1000) {
    while (LINK.available()) { char c = LINK.read(); if (c == '\n') { r.trim(); if (r == "pong") { ok("the Wi-Fi board answers"); return; } r = ""; } else r += c; }
  }
  bad("no answer from the Wi-Fi board - is cubi2_radio on it? link wires BRX / BTX?");
}

static void cmdShow() {
  OUT.printf("  firmware %s\n", FIRMWARE_VERSION);
  OUT.printf("  drivers X Y Z: ids %d %d %d | wheel axes %c %c %c | signs %+d %+d %+d | wheel test %s\n",
                S.nodeId[0], S.nodeId[1], S.nodeId[2], 'x' + S.wheelAxis[0], 'x' + S.wheelAxis[1], 'x' + S.wheelAxis[2],
                S.wheelSign[0], S.wheelSign[1], S.wheelSign[2], S.wheelChecked ? "done" : "NOT done");
  OUT.printf("  current limit %.1f A | wheel stop %.0f rpm\n", S.iqLimit, S.wheelMaxRad * 9.549f);
  OUT.printf("  IMU orientation: %s\n", S.orientChecked ? "measured" : "from the model (do 'face x/y/z')");
  for (int r = 0; r < 3; r++) OUT.printf("    %+.3f %+.3f %+.3f\n", S.R[3 * r], S.R[3 * r + 1], S.R[3 * r + 2]);
  OUT.printf("  gyro zero %.3f %.3f %.3f deg/s\n", S.gyroBias[0] * 57.3f, S.gyroBias[1] * 57.3f, S.gyroBias[2] * 57.3f);
  OUT.printf("  balance corner %d\n", S.corner);
  for (int m = 0; m < MODE_COUNT; m++)
    OUT.printf("  %-7s up %+.3f %+.3f %+.3f  gains %.2f %.3f %.6f\n", MODE_NAME[m], S.up[m][0], S.up[m][1], S.up[m][2], S.K[m][0], S.K[m][1], S.K[m][2]);
  OUT.printf("  learn %.2f /s | start within %.1f deg | stop beyond %.0f deg\n", S.trimRate, S.armDeg, S.stopDeg);
}

// (21/10/2026) i2c: the IMU should answer on Wire2 (SDA2 pin 25 = y4, SCL2 pin 24 = y5) at 0x68 or 0x69.
// Each line is tested first: with nothing on it, it reads LOW (the pin pulls down a little); the IMU
// board's resistors pull SDA and SCL HIGH. Then every address is tried, 100 kHz.
static void i2cBus(TwoWire &w, const char *name, int sda, int scl) {
  w.end();
  pinMode(sda, INPUT_PULLDOWN); pinMode(scl, INPUT_PULLDOWN);
  delay(3);
  int ls = digitalRead(sda), lc = digitalRead(scl);
  w.begin(); w.setClock(100000);
  OUT.printf("  %-5s SDA pin %d %s, SCL pin %d %s:", name, sda, ls ? "high" : "LOW ", scl, lc ? "high" : "LOW ");
  int found = 0;
  if (ls && lc) {
    for (uint8_t a = 1; a < 127; a++) {
      w.beginTransmission(a);
      if (w.endTransmission() == 0) { OUT.printf(" 0x%02X%s", a, (a == 0x68 || a == 0x69) ? " (the IMU)" : ""); found++; }
    }
    if (!found) OUT.print(" nothing answers");
  } else OUT.print(" not scanned (a line is LOW: not joined, or a short)");
  OUT.println();
}

static void cmdI2c() {
  OUT.println("  I2C buses (the IMU belongs on Wire2):");
  i2cBus(Wire, "Wire", 18, 19);
  i2cBus(Wire1, "Wire1", 17, 16);
  i2cBus(Wire2, "Wire2", 25, 24);
  imuOk = IMU.begin(&S);                      // back to the IMU's own settings (400 kHz)
  if (imuOk) ok("the IMU answers on Wire2");
  else bad("no IMU on Wire2 - both lines LOW: blue/yellow not at y4/y5 or the plug out; high but nothing: "
           "blue and yellow swapped, or the IMU on another bus (see above)");
}

static void help() {
  OUT.println("Commands: scan | find | id OLD NEW | cal ID | limits | spin ID AMPS | enc ID | ical ID [A] | imu | i2c | bias | face x|y|z |");
  OUT.println("          wheels | corner 0|7 | battery | link | show | save | defaults");
}

// ---------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  LINK.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 4000) {}
  analogReadResolution(10);
  OUT.printf("\n=== Cubi 2 - SETUP %s ===\n", FIRMWARE_VERSION);
  if (cubiLoad(S)) ok("settings found in the Teensy");
  else OUT.println("  (no saved settings yet - the model's values are used)");
  static uint8_t txb[1024]; LINK.addMemoryForWrite(txb, sizeof(txb));
  tmBegin();
  // (21/10/2026) no questions to the drivers here: if they got power with the Teensy, they are
  // still in their bootloader and a question would keep them there. Each command finds them.
  idleAll();                                        // nothing turns before you ask
  imuOk = IMU.begin(&S);
  if (imuOk) OUT.printf("  OK   IMU at 0x%02X\n", IMU.address);
  else bad("IMU not found on the I2C cable (y4 SDA, y5 SCL, y6 3V3, y7 GND) - type i2c");
  help();
}

void loop() {
  tmPoll();
  String l = readLine();
  if (!l.length()) return;
  Serial.print("> "); Serial.println(l);           // the app shows what you typed itself
  char c[16] = {0}; float a = 0, b = 0;
  int n = sscanf(l.c_str(), "%15s %f %f", c, &a, &b);
  String cmd(c);
  if (cmd == "help" || cmd == "?") help();
  else if (cmd == "scan") cmdScan();
  else if (cmd == "find") cmdFind();
  else if (cmd == "id" && n == 3) cmdId((int)a, (int)b);
  else if (cmd == "cal" && n == 2) cmdCal((int)a);
  else if (cmd == "limits") cmdLimits();
  else if (cmd == "spin" && n == 3) cmdSpin((int)a, b);
  else if (cmd == "enc" && n == 2) cmdEnc((int)a);
  else if (cmd == "ical" && (n == 2 || n == 3)) cmdIcal((int)a, b, n == 3);
  else if (cmd == "imu") cmdImu();
  else if (cmd == "i2c") cmdI2c();
  else if (cmd == "bias") cmdBias();
  else if (cmd == "face" && l.length() >= 6) cmdFace(l.charAt(5));
  else if (cmd == "wheels") cmdWheels();
  else if (cmd == "corner" && n == 2 && ((int)a == 0 || (int)a == 7)) {
    if ((int)a != S.corner) {
      float d[3] = {-S.up[MODE_CORNER][0], -S.up[MODE_CORNER][1], -S.up[MODE_CORNER][2]};
      cubiUpsFromCorner(S, d);
      S.corner = (int)a;
    }
    ok("corner set - 'save' to keep it");
  }
  else if (cmd == "battery") cmdBattery();
  else if (cmd == "link") cmdLink();
  else if (cmd == "show") cmdShow();
  else if (cmd == "save") { cubiSave(S); ok("saved"); }
  else if (cmd == "defaults") { cubiDefaults(S); ok("model values (not saved yet)"); }
  else { bad("unknown command"); help(); }
}
