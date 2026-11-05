// Cubi 2 - the three motor drivers on the CAN bus.
// The SAME file is in cubi2_setup and cubi2_brain.
//
// Teensy 4.1, CAN2: CRX2 = pin 0, CTX2 = pin 1 -> the CAN board on board X.
// 1 Mbit/s. A driver frame: 29-bit id = (driver id << 21) | endpoint.
// Read = a remote frame (RTR), the driver answers with the same id.
// The endpoint numbers change with the driver's firmware: motor_driver_tables.h has one
// table per firmware line, chosen by the protocol hash the driver reports.
#pragma once
#include <FlexCAN_T4.h>       // comes with Teensyduino
#include "motor_driver_tables.h"

#define TM_MAX_NODES 8

static FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_32> tmCan;

struct TmNode {
  uint8_t id = 0;
  const TmSpec *spec = nullptr;
  uint32_t hash = 0;
  char fw[9] = {0};
  // latest values (filled by tmPoll)
  volatile float vel = 0;        // ticks/s
  volatile float iq = 0;         // A
  volatile float vbus = 0;       // V
  volatile uint8_t state = 0;
  volatile uint32_t velMs = 0;   // when vel came
};

static TmNode tmNodes[TM_MAX_NODES + 1];   // by id
// the last answer to a blocking read
static volatile uint32_t tmAnsId = 0;
static uint8_t tmAnsBuf[8];
static volatile uint8_t tmAnsLen = 0;
static volatile bool tmAnsGot = false;

static inline uint32_t tmArb(uint8_t node, uint16_t ep) { return ((uint32_t)node << 21) | (ep & 0x0FFF); }

static void tmBegin() {
  tmCan.begin();
  tmCan.setBaudRate(1000000);
  tmCan.setMaxMB(16);
  tmCan.enableFIFO();
}

static bool tmSend(uint8_t node, int16_t ep, const void *data, uint8_t len, bool rtr) {
  if (ep < 0) return false;
  CAN_message_t m;
  m.id = tmArb(node, ep);
  m.flags.extended = 1;
  m.flags.remote = rtr;
  m.len = len;
  if (len) memcpy(m.buf, data, len);
  for (int k = 0; k < 50; k++) {               // a free mailbox within ~1 ms
    if (tmCan.write(m) > 0) return true;
    delayMicroseconds(20);
  }
  return false;
}

static inline float tmF(const uint8_t *b) { float f; memcpy(&f, b, 4); return f; }

// drain the receive queue; store what we know
static void tmPoll() {
  CAN_message_t m;
  while (tmCan.read(m)) {
    if (!m.flags.extended) continue;
    uint32_t id = m.id;
    if ((id & 0x1FFFFFC0UL) == 0x700UL) continue;  // a heartbeat (0x700 | id): not needed here
    uint8_t n = (id >> 21) & 0xFF;
    uint16_t ep = id & 0x0FFF;
    if (n >= 1 && n <= TM_MAX_NODES && tmNodes[n].spec && !m.flags.remote) {
      TmNode &t = tmNodes[n];
      const TmSpec *s = t.spec;
      if (ep == s->vel_estimate && m.len >= 4) { t.vel = tmF(m.buf); t.velMs = millis(); }
      else if (ep == s->iq_estimate && m.len >= 4) t.iq = tmF(m.buf);
      else if (ep == s->vbus && m.len >= 4) t.vbus = tmF(m.buf);
      else if (ep == s->state && m.len >= 1) t.state = m.buf[0];
    }
    if (!m.flags.remote && id == tmAnsId) {
      memcpy(tmAnsBuf, m.buf, m.len); tmAnsLen = m.len; tmAnsGot = true;
    }
  }
}

// blocking read of one endpoint; len = bytes received, 0 = no answer
static uint8_t tmRead(uint8_t node, int16_t ep, uint8_t *out, uint16_t timeoutMs = 30) {
  if (ep < 0) return 0;
  tmPoll();
  tmAnsId = tmArb(node, ep); tmAnsGot = false;
  if (!tmSend(node, ep, nullptr, 0, true)) return 0;
  uint32_t t0 = millis();
  while (millis() - t0 < timeoutMs) {
    tmPoll();
    if (tmAnsGot) { memcpy(out, tmAnsBuf, tmAnsLen); return tmAnsLen; }
  }
  return 0;
}

static bool tmReadF(uint8_t node, int16_t ep, float &v) {
  uint8_t b[8];
  if (tmRead(node, ep, b) < 4) return false;
  v = tmF(b); return true;
}
static bool tmReadU8(uint8_t node, int16_t ep, uint8_t &v) {
  uint8_t b[8];
  if (tmRead(node, ep, b) < 1) return false;
  v = b[0]; return true;
}
static bool tmWriteF(uint8_t node, int16_t ep, float v) { return tmSend(node, ep, &v, 4, false); }
static bool tmWriteU8(uint8_t node, int16_t ep, uint8_t v) { return tmSend(node, ep, &v, 1, false); }
static bool tmWriteU32(uint8_t node, int16_t ep, uint32_t v) { return tmSend(node, ep, &v, 4, false); }
static bool tmCall(uint8_t node, int16_t ep) { return tmSend(node, ep, nullptr, 0, true); }

// Find out which firmware a driver runs. true = it answered and we know it.
static bool tmDetect(uint8_t node) {
  TmNode &t = tmNodes[node];
  t.id = node; t.spec = nullptr; t.hash = 0; t.fw[0] = 0;
  uint8_t b[8];
  if (tmRead(node, 0, b, 40) < 4) return false;      // endpoint 0 = protocol hash, in every version
  memcpy(&t.hash, b, 4);
  for (int k = 0; k < TM_SPEC_COUNT; k++)
    if (TM_SPECS[k].hash == t.hash) { t.spec = &TM_SPECS[k]; break; }
  // the firmware version text (endpoint 2 since 1.6)
  uint8_t n = tmRead(node, 2, b, 40);
  if (n > 0 && b[0] >= '0' && b[0] <= '9') { memcpy(t.fw, b, n); t.fw[n] = 0; }
  if (!t.spec && t.fw[0]) {                           // unknown hash: try by "major.minor"
    for (int k = 0; k < TM_SPEC_COUNT; k++)
      if (strncmp(TM_SPECS[k].version, t.fw, 3) == 0) { t.spec = &TM_SPECS[k]; break; }
  }
  return true;
}

// (21/10/2026) the driver's own bootloader (DFU 1.0 / 1.1) runs for a few seconds after power-up.
// A question to it in those seconds keeps it there (it then ignores every command).
// Its 'reset' (endpoint 3 in DFU 1.0, 4 in DFU 1.1) starts it again; if nobody asks it
// anything for 10 s, its normal program follows.
static inline bool tmIsBootloader(uint32_t h) { return h == 1550177441UL || h == 513279425UL; }
static void tmBootReset(uint8_t node, uint32_t h) { tmCall(node, h == 1550177441UL ? 3 : 4); }
#define TM_BOOT_MS 10000UL     // ask the drivers nothing in the first 10 s after power-up

static const char *tmStateName(uint8_t s) {
  return s == 0 ? "IDLE" : s == 1 ? "CALIBRATING" : s == 2 ? "RUNNING" : "?";
}
