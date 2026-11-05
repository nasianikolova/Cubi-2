// =====================================================================
// Cubi 2 - RADIO program for the Wi-Fi board on board X (ESP32-C6, Beetle).
// It makes its own Wi-Fi network; the Cubi 2 app talks to the cube through it.
//
//   Wi-Fi network : Cubi2        password : cubi2cubi
//   address       : http://192.168.4.1   (/c commands, /t telemetry and log, /fw new programs)
//
// It talks to the Teensy (board Y) over the link wires:
//   this board TX (pin 16) -> Teensy RX2 (pin 7)     this board RX (pin 17) <- Teensy TX2 (pin 8)
//
// VS Code + PlatformIO: open this folder; platformio.ini has all the settings
// (the USB cable shows this board's messages). No extra libraries.
// (5/11/2026) No page of its own any more: the app is the one tool.
//
// (24/10/2026) NEW PROGRAMS OVER THE WI-FI, from the Cubi 2 app (Update):
//   POST /fw/t?op=begin|data|end|abort&n=N  the Teensy's program (.hex), passed on to it 64 lines at a time;
//                                           the answer is the Teensy's own 'fw ...' line
//   POST /fw/r  (a file upload)             this board's own program (.bin), then it restarts
// =====================================================================
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>

const char *FIRMWARE_VERSION = "2026110501";   // the date, then the change of that day (YYYYMMDDNN)

const char *WIFI_NAME = "Cubi2";
const char *WIFI_PASS = "cubi2cubi";     // at least 8 characters
#define LINK_RX 17
#define LINK_TX 16
// which serial port is the link to the Teensy
#if ARDUINO_USB_CDC_ON_BOOT
#define Link Serial1                 // USB is the Serial Monitor, UART1 the link
#define HAVE_USB 1
#else
#define Link Serial0                 // no USB messages: UART0 is the link
#define HAVE_USB 0
#endif

WebServer server(80);

String lastJson = "{}";
uint32_t lastJsonMs = 0;
const int LOG_N = 80;                // lines kept for the app (the setup program talks a lot)
String logLines[LOG_N];
uint32_t logSeq = 0;               // lines ever received

static void addLog(const String &l) { logLines[logSeq % LOG_N] = l; logSeq++; }

static void handleRoot() { server.send(200, "text/plain", "Cubi 2 - open the Cubi 2 app"); }

// (24/10/2026) the app runs on the COMPUTER (in its browser) and asks the cube for the
// data: the browser allows that only if the cube says so (CORS), also for a local network (Private
// Network Access). '*' is fine: the cube's own Wi-Fi with its password is the only way in.
static void allowTheApp() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Private-Network", "true");
}
static void handleOptions() {
  allowTheApp();
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "*");
  server.send(204);
}

static void handleCmd() {
  String x = server.arg("x");
  x.trim();
  if (x.length()) { Link.println(x); addLog("> " + x); }
  allowTheApp();
  server.send(200, "text/plain", "ok");
}

static void handleTele() {
  uint32_t from = server.arg("s").toInt();
  if (logSeq > LOG_N && from < logSeq - LOG_N) from = logSeq - LOG_N;
  String out = "{\"t\":" + lastJson + ",\"age\":" + String(millis() - lastJsonMs) + ",\"seq\":" + String(logSeq) + ",\"log\":[";
  for (uint32_t k = from; k < logSeq; k++) {
    String l = logLines[k % LOG_N];
    l.replace("\\", "/"); l.replace("\"", "'");
    out += "\"" + l + "\"";
    if (k + 1 < logSeq) out += ",";
  }
  out += "]}";
  allowTheApp();
  server.send(200, "application/json", out);
}

// one line from the Teensy: telemetry, the log, or the setup program's link test
static void linkLine(String buf) {
  buf.trim();
  if (buf == "ping") Link.println("pong");                 // the setup program's link test
  else if (buf.startsWith("{")) { lastJson = buf; lastJsonMs = millis(); }
  else if (buf.length()) {
    addLog(buf);
    if (HAVE_USB) Serial.println(buf);
  }
}
static String linkBuf;
static bool linkRead(String &line) {                       // true when a whole line arrived
  while (Link.available()) {
    char ch = Link.read();
    if (ch == '\r') continue;
    if (ch == '\n') { line = linkBuf; linkBuf = ""; return true; }
    if (linkBuf.length() < 600) linkBuf += ch;           // (the help line is ~420 characters)
  }
  return false;
}

// (24/10/2026) THE TEENSY'S NEW PROGRAM. Wait for its 'fw ...' answer; the other lines go on as usual.
static String fwWait(uint32_t ms) {
  uint32_t t0 = millis(); String l;
  while (millis() - t0 < ms) {
    if (linkRead(l)) { l.trim(); if (l.startsWith("fw ")) return l; linkLine(l); }
    else delay(1);
  }
  return "fw e no answer from the Teensy (is cubi2_brain with program_update.h on it?)";
}
static void handleFwTeensy() {
  String op = server.arg("op"), r, old;
  while (linkRead(old)) { old.trim(); if (!old.startsWith("fw ")) linkLine(old); }   // a late answer to an earlier request is not this one's
  int n = server.arg("n").toInt();
  if (op == "begin") { Link.println("fw begin"); r = fwWait(30000); addLog(r); }
  else if (op == "data") {                                 // the body: up to 64 hex lines
    String body = server.arg("plain"); int k = 0, from = 0;
    while (from < (int)body.length()) {
      int e = body.indexOf('\n', from); if (e < 0) e = body.length();
      String line = body.substring(from, e); line.trim(); from = e + 1;
      if (line.startsWith(":")) { Link.println(line); k++; }
    }
    if (k != n) r = "fw e " + String(k) + " of " + String(n) + " lines reached the Wi-Fi board";
    else { Link.println("fw w " + String(n)); r = fwWait(5000); }
  }
  else if (op == "end") { Link.println("fw end " + String(n)); r = fwWait(5000); addLog(r); }
  else if (op == "abort") { Link.println("fw abort"); r = fwWait(30000); addLog(r); }
  else r = "fw e unknown";
  allowTheApp();
  server.send(200, "text/plain", r);
}
// THIS BOARD'S NEW PROGRAM: the file arrives in pieces, straight into the free program slot
static bool fwRadioOk = false;
static bool cubeRunning() { return lastJson.indexOf("\"st\":\"BALANCING\"") >= 0 || lastJson.indexOf("\"st\":\"READY\"") >= 0; }
static void handleFwRadioPiece() {
  HTTPUpload &u = server.upload();
  if (u.status == UPLOAD_FILE_START) {               // not while the cube runs: during the restart 'stop' could not be sent
    if (cubeRunning() && millis() - lastJsonMs < 2000) { fwRadioOk = false; addLog("Wi-Fi board: the cube is running - 'stop' first"); return; }
    fwRadioOk = Update.begin(UPDATE_SIZE_UNKNOWN); addLog("Wi-Fi board: receiving a new program ...");
  }
  else if (u.status == UPLOAD_FILE_WRITE) { if (fwRadioOk && Update.write(u.buf, u.currentSize) != u.currentSize) fwRadioOk = false; }
  else if (u.status == UPLOAD_FILE_END) { if (fwRadioOk) fwRadioOk = Update.end(true); }
  else if (u.status == UPLOAD_FILE_ABORTED) { Update.abort(); fwRadioOk = false; }
}
static void handleFwRadioDone() {
  allowTheApp();
  if (fwRadioOk) {
    server.send(200, "text/plain", "fw done - the Wi-Fi board restarts with the new program (join the Cubi2 Wi-Fi again)");
    addLog("Wi-Fi board: new program - restarting"); delay(500); ESP.restart();
  } else {
    String e = (cubeRunning() && millis() - lastJsonMs < 2000) ? String("fw e the cube is running - 'stop' first") : String("fw e the Wi-Fi board refused the file: ") + Update.errorString();
    addLog(e); server.send(200, "text/plain", e);
  }
}

void setup() {
  if (HAVE_USB) Serial.begin(115200);
  addLog(String("Wi-Fi board: program ") + FIRMWARE_VERSION);   // the app sees it in its Terminal
  Link.setRxBufferSize(4096);                       // one request (a 64-line batch of a new program) keeps us busy ~0.3 s
  Link.begin(115200, SERIAL_8N1, LINK_RX, LINK_TX);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_NAME, WIFI_PASS);
  server.on("/", handleRoot);
  server.on("/c", HTTP_OPTIONS, handleOptions);       // (24/10/2026) the browser's question before a request
  server.on("/t", HTTP_OPTIONS, handleOptions);
  server.on("/fw/t", HTTP_OPTIONS, handleOptions);      // (24/10/2026) new programs over the Wi-Fi
  server.on("/fw/r", HTTP_OPTIONS, handleOptions);
  server.on("/fw/t", HTTP_POST, handleFwTeensy);
  server.on("/fw/r", HTTP_POST, handleFwRadioDone, handleFwRadioPiece);
  server.on("/c", handleCmd);
  server.on("/t", handleTele);
  server.begin();
  if (HAVE_USB) {
    Serial.print("Cubi 2 radio - Wi-Fi '"); Serial.print(WIFI_NAME); Serial.print("', address http://");
    Serial.println(WiFi.softAPIP());
  }
}

void loop() {
  server.handleClient();
  String l;
  while (linkRead(l)) linkLine(l);
  // the USB cable too: type a command into the Serial Monitor of this board
  static String ubuf;
  while (HAVE_USB && Serial.available()) {
    char ch = Serial.read();
    if (ch == '\n') { ubuf.trim(); if (ubuf.length()) Link.println(ubuf); ubuf = ""; }
    else if (ch != '\r') ubuf += ch;
  }
}
