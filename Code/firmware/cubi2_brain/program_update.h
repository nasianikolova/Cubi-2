// =====================================================================
// Cubi 2 - the Teensy 4.1 updates ITS OWN PROGRAM over the Wi-Fi link. (24/10/2026)
//
// The Cubi 2 app sends the new program (the .hex file PlatformIO builds) to the
// Wi-Fi board, the Wi-Fi board sends it here line by line, 64 lines at a time:
//   fw begin      -> the free flash above this program is cleared       answer: fw ready <bytes>
//   :10....       -> hex lines, kept in RAM (nothing is written while lines arrive)
//   fw w N        -> the N kept lines checked and written into the free flash   answer: fw k <lines> / fw e <why>
//   fw end N      -> all N lines arrived? end of file? the new program can update itself too?
//                    then it is copied over this program and the Teensy restarts   answer: fw done / fw e <why>
//   fw abort      -> the free flash cleared again, nothing changed      answer: fw aborted
// Nothing changes in the running program until 'fw end' - a broken transfer leaves it as it was.
// Only with the wheels off. The settings (EEPROM, the top 256 KB of the flash) are never touched.
// A program WITHOUT this file is refused (it must contain FW_ID): the next update must work too.
// If something goes wrong anyway: the USB cable and the Teensy's button always bring it back.
//
// Based on the public-domain flash code of FlasherX (Jon Zeeff, Joe Pasquariello, Deb Hollenback,
// Paul Stoffregen's Teensy 4 flash routines) - Teensy 4.1 only, the settings area kept.
// =====================================================================
#pragma once
#include <Arduino.h>

extern "C" {
  void eepromemu_flash_write(void *addr, const void *data, uint32_t len);   // the Teensy 4 core (eeprom.c)
  void eepromemu_flash_erase_sector(void *addr);
  extern unsigned long _flashimagelen;           // the linker: how long THIS program is in the flash
}

#define FW_BASE     0x60000000u                 // the program starts here
#define FW_SIZE     0x00800000u                 // 8 MB
#define FW_SECTOR   0x1000u                     // 4 KB
#define FW_RESERVE  0x00040000u                 // the top 256 KB: the settings (EEPROM emulation) - never touched
#define FW_BATCH    64                          // hex lines kept before 'fw w' (64 x 45 = 2.9 KB, the link keeps 4 KB)
#define FW_LINE     48                          // a hex line of 16 bytes is 43 characters
#define FW_RAM      __attribute__((section(".fastrun"), noinline, noclone, optimize("Os")))
#ifndef FW_REBOOT
#define FW_REBOOT   (*(volatile uint32_t *)0xE000ED0C = 0x5FA0004)
#endif

__attribute__((used)) const char FW_ID[] = "CUBI2-WIFI-UPDATE-1";   // a program that can be updated over the Wi-Fi

struct FwUpdate {
  bool active = false, eof = false;
  uint32_t buf = 0, bufSize = 0;               // the free flash above this program
  uint32_t base = 0, minA = 0xFFFFFFFFu, maxA = 0;
  uint32_t lines = 0;                          // hex lines written
  char batch[FW_BATCH][FW_LINE];
  int nBatch = 0;
  bool overflow = false;
};
FwUpdate FW;

FW_RAM static int fwSectorErased(uint32_t a) {
  const uint32_t *p = (const uint32_t *)(a & ~(FW_SECTOR - 1));
  for (uint32_t i = 0; i < FW_SECTOR / 4; i++) if (p[i] != 0xFFFFFFFFu) return 0;
  return 1;
}
static void fwErase(uint32_t from, uint32_t to) {
  for (uint32_t a = from; a < to; a += FW_SECTOR) if (!fwSectorErased(a)) eepromemu_flash_erase_sector((void *)a);
}

// copy the new program over this one, clear the rest, restart (runs from RAM; the code runs from RAM
// on the Teensy 4 anyway, so erasing the flash under it is safe)
FW_RAM static void fwMove(uint32_t dst, uint32_t src, uint32_t size) {
  uint32_t off = 0;
  __disable_irq();
  SYST_CSR = 0;                                   // no interrupt may run while the program is replaced:
  for (int i = 0; i < NVIC_NUM_INTERRUPTS; i++) NVIC_DISABLE_IRQ(i);   // the flash routines switch them on again
  static uint32_t page[64];                       // 256 bytes in RAM: the flash cannot be read while it is written
  while (off < size) {                            // one flash page (256 bytes) at a time; size is a multiple of 256
    uint32_t a = dst + off;
    if ((a & (FW_SECTOR - 1)) == 0 && !fwSectorErased(a)) eepromemu_flash_erase_sector((void *)a);
    for (int i = 0; i < 64; i++) page[i] = ((const uint32_t *)(src + off))[i];
    eepromemu_flash_write((void *)a, page, 256);
    off += 256;
  }
  off = (off + FW_SECTOR - 1) & ~(FW_SECTOR - 1);  // from the NEXT sector on (the last one holds the end of the new program)
  while (off < FW_SIZE - FW_RESERVE) {              // the old copy and the buffer: cleared
    uint32_t a = dst + off;
    if (!fwSectorErased(a)) eepromemu_flash_erase_sector((void *)a);
    off += FW_SECTOR;
  }
  FW_REBOOT;
  for (;;) {}
}

static int fwHex(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; }

// one hex line -> the free flash. 0 = fine, else a short reason in 'why'
static int fwLine(const char *l, char *why) {
  int n = strlen(l);
  if (n < 11 || l[0] != ':') { strcpy(why, "not a hex line"); return 1; }
  uint8_t b[40]; int nb = (n - 1) / 2; if (nb > 40) { strcpy(why, "line too long"); return 1; }
  uint8_t sum = 0;
  for (int i = 0; i < nb; i++) {
    int h = fwHex(l[1 + 2 * i]), lo = fwHex(l[2 + 2 * i]);
    if (h < 0 || lo < 0) { strcpy(why, "bad character"); return 1; }
    b[i] = h * 16 + lo; sum += b[i];
  }
  int cnt = b[0]; uint32_t ad = (b[1] << 8) | b[2]; int typ = b[3];
  if (nb != cnt + 5) { strcpy(why, "wrong length"); return 1; }
  if (sum != 0) { strcpy(why, "checksum"); return 1; }
  if (typ == 1) { FW.eof = true; return 0; }
  if (typ == 2) { FW.base = (uint32_t)((b[4] << 8) | b[5]) << 4; return 0; }
  if (typ == 4) { FW.base = (uint32_t)((b[4] << 8) | b[5]) << 16; return 0; }
  if (typ == 3 || typ == 5) return 0;          // start address: not needed
  if (typ != 0) { strcpy(why, "record type"); return 1; }
  uint32_t a = FW.base + ad;
  if (a < FW_BASE || a + cnt > FW_BASE + FW.bufSize) { strcpy(why, "address outside the flash"); return 1; }
  if (a < FW.minA) FW.minA = a;
  if (a + cnt > FW.maxA) FW.maxA = a + cnt;
  uint32_t to = FW.buf + (a - FW_BASE);
  if ((to & 3) == 0 && (cnt & 3) == 0 && (to & 0xFF) + cnt <= 0x100) {   // the usual 16 bytes: one write
    static uint32_t tmp[10];
    memcpy(tmp, b + 4, cnt);
    eepromemu_flash_write((void *)to, tmp, cnt);
  } else {                                       // odd ones: word by word (0xFF leaves the other bytes as they are)
    for (int i = 0; i < cnt; i++) {
      uint32_t w = to + i, wa = w & ~3u; uint32_t v = 0xFFFFFFFFu;
      ((uint8_t *)&v)[w - wa] = b[4 + i];
      eepromemu_flash_write((void *)wa, &v, 4);
    }
  }
  if (memcmp((const void *)to, b + 4, cnt) != 0) { strcpy(why, "flash did not take it"); return 1; }
  return 0;
}

// the protocol lines (the command reader passes every line here first); true = it was ours
static bool fwCommand(const String &l, bool wheelsOff, void (*answer)(const char *)) {
  char out[96], why[40];
  if (l.startsWith(":")) {
    if (!FW.active) return true;                 // stray hex: ignored
    if (FW.nBatch < FW_BATCH && l.length() < FW_LINE) { strcpy(FW.batch[FW.nBatch++], l.c_str()); }
    else FW.overflow = true;
    return true;
  }
  if (!l.startsWith("fw ")) return false;
  if (l == "fw begin") {
    if (!wheelsOff) { answer("fw e stop the wheels first ('stop')"); return true; }
    // the free flash: from the end of THIS program (the linker's length) up to the settings area; anything
    // there (a broken earlier transfer) is cleared
    uint32_t a = FW_BASE + (uint32_t)(uintptr_t)&_flashimagelen;
    if (a & (FW_SECTOR - 1)) a += FW_SECTOR - (a & (FW_SECTOR - 1));
    FW.buf = a; FW.bufSize = FW_BASE + FW_SIZE - FW_RESERVE - a;
    fwErase(FW.buf, FW.buf + FW.bufSize);
    FW.active = true; FW.eof = false; FW.base = 0; FW.minA = 0xFFFFFFFFu; FW.maxA = 0; FW.lines = 0; FW.nBatch = 0; FW.overflow = false;
    snprintf(out, sizeof(out), "fw ready %lu", (unsigned long)FW.bufSize); answer(out);
    return true;
  }
  if (!FW.active) { answer("fw e no update running ('fw begin' first)"); return true; }
  if (l == "fw abort") {
    fwErase(FW.buf, FW.buf + FW.bufSize); FW.active = false; FW.nBatch = 0;
    answer("fw aborted - the program is as it was"); return true;
  }
  if (l.startsWith("fw w ")) {
    int n = l.substring(5).toInt();
    if (FW.overflow || n != FW.nBatch) { snprintf(out, sizeof(out), "fw e lines lost (%d of %d arrived)", FW.nBatch, n); FW.nBatch = 0; FW.overflow = false; answer(out); return true; }
    bool eof0 = FW.eof; uint32_t base0 = FW.base;
    for (int i = 0; i < FW.nBatch; i++) {
      if (FW.eof) { strcpy(why, "lines after the end of file"); }
      else if (fwLine(FW.batch[i], why) == 0) continue;
      snprintf(out, sizeof(out), "fw e line %lu: %s", (unsigned long)(FW.lines + i + 1), why);
      FW.eof = eof0; FW.base = base0; FW.nBatch = 0; answer(out); return true;   // nothing of this batch counted
    }
    FW.lines += FW.nBatch; FW.nBatch = 0;
    snprintf(out, sizeof(out), "fw k %lu", (unsigned long)FW.lines); answer(out);
    return true;
  }
  if (l.startsWith("fw end ")) {
    uint32_t n = l.substring(7).toInt();
    if (!wheelsOff) { answer("fw e stop the wheels first"); return true; }
    if (!FW.eof) { answer("fw e the end of the file did not arrive"); return true; }
    if (n != FW.lines) { snprintf(out, sizeof(out), "fw e %lu lines arrived, the file has %lu", (unsigned long)FW.lines, (unsigned long)n); answer(out); return true; }
    if (FW.minA != FW_BASE) { answer("fw e not a Teensy 4.1 program (it does not start at the flash start)"); return true; }
    uint32_t size = FW.maxA - FW_BASE, k = strlen(FW_ID); bool found = false;
    for (uint32_t i = 0; i + k <= size && !found; i++) found = memcmp((const void *)(FW.buf + i), FW_ID, k) == 0;
    if (!found) { answer("fw e this program cannot update itself over the Wi-Fi (no program_update.h) - refused, use the USB cable"); return true; }
    snprintf(out, sizeof(out), "fw done - %lu bytes, the Teensy restarts with the new program", (unsigned long)size); answer(out);
    Serial.flush(); Serial2.flush(); delay(20);   // the answer leaves first (also past any telemetry waiting)
    fwMove(FW_BASE, FW.buf, (size + 255) & ~255u);  // whole pages (the buffer beyond the image is 0xFF)
    return true;                                  // (never reached)
  }
  answer("fw e unknown"); return true;
}
