/*
 * ESP8266 (Wemos D1 mini) + 8x32 MAX7219 Dot-Matrix + DHT11
 * ---------------------------------------------------------
 * WLAN-Steuerung einer Laufschrift / statischem Text -- komplett OFFLINE.
 * Der ESP spannt ein eigenes WLAN (Access Point) auf, es wird KEIN
 * Router und KEIN externer Server benoetigt.
 *
 * Optional: Ein DHT11 misst Temperatur + Luftfeuchte. In der Weboberflaeche
 * kann man auswaehlen, ob diese Werte hinter dem Text mitlaufen sollen.
 *
 * Bedienung:
 *   1) ESP mit Strom versorgen (USB)
 *   2) Am Handy/Laptop mit dem WLAN "MatrixDisplay" verbinden
 *      Passwort: 12345678
 *   3) Im Browser http://192.168.4.1  aufrufen
 *
 * Benoetigte Bibliotheken (Arduino IDE -> Bibliotheksverwalter):
 *   - MD_Parola   (von majicDesigns)
 *   - MD_MAX72XX  (von majicDesigns)   <- wird bei Parola meist mit installiert
 *   - DHTesp      (von beegee-tokyo)   <- fuer den DHT11-Sensor
 *
 * Board (Arduino IDE -> Boardverwalter-URL eintragen, siehe README):
 *   "LOLIN(WEMOS) D1 R2 & mini"  (ESP8266)
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <DHTesp.h>
#include <EEPROM.h>

// ---------------------------------------------------------------------------
// HARDWARE-KONFIGURATION
// ---------------------------------------------------------------------------
// Fuer dieses Modul getestet und korrekt: ICSTATION_HW.
// Falls die Anzeige mal spiegelverkehrt / Bloecke vertauscht sind, hier eine
// der anderen Varianten testen: FC16_HW, GENERIC_HW, PAROLA_HW
#define HARDWARE_TYPE MD_MAX72XX::ICSTATION_HW

// Matrix-Groesse: 8x32 = 4 Module, 8x64 = 8 Module. Der Wert wird in der
// Weboberflaeche gewaehlt und im EEPROM gespeichert; DEFAULT gilt beim ersten
// Start (noch nichts gespeichert). Die Kette wird beim Boot damit angelegt.
#define MODULES_DEFAULT  4

// Verkabelung Wemos D1 mini <-> MAX7219
//   MAX7219 VCC  -> 5V (VBUS/5V vom Wemos)
//   MAX7219 GND  -> GND
//   MAX7219 DIN  -> D7 (GPIO13, MOSI)   [durch SPI vorgegeben]
//   MAX7219 CLK  -> D5 (GPIO14, SCK)    [durch SPI vorgegeben]
//   MAX7219 CS   -> D8 (GPIO15)         [frei waehlbar, unten definiert]
#define CS_PIN  D8

// DHT11 (3-Pin-Modul mit eingebautem Pull-up):
//   VCC  -> 3V3
//   GND  -> GND
//   DATA -> D6 (GPIO12)
#define DHT_PIN  D6

// Display wird beim Boot mit der gespeicherten Modulzahl erzeugt (Pointer).
MD_Parola* display = nullptr;
uint8_t    moduleCount = MODULES_DEFAULT;   // 4 (8x32) oder 8 (8x64)
DHTesp     dht;

// EEPROM: Modulzahl dauerhaft speichern
#define EEPROM_SIZE   16
#define EEPROM_MAGIC  0xA6          // Kennung fuer "gueltig gespeichert"

void loadModuleCount() {
  EEPROM.begin(EEPROM_SIZE);
  if (EEPROM.read(0) == EEPROM_MAGIC) {
    uint8_t m = EEPROM.read(1);
    if (m == 4 || m == 8) moduleCount = m;
  }
}

void saveModuleCount(uint8_t m) {
  EEPROM.write(0, EEPROM_MAGIC);
  EEPROM.write(1, m);
  EEPROM.commit();
}

// ---------------------------------------------------------------------------
// WLAN ACCESS POINT
// ---------------------------------------------------------------------------
const char* AP_SSID     = "MatrixDisplay";
const char* AP_PASSWORD = "12345678";        // mind. 8 Zeichen
const byte  DNS_PORT    = 53;

// --- Fehlersuche-Schalter ---------------------------------------------------
// OPEN_AP = true  -> Netz OHNE Passwort (offen). Offene Netze werden von Handys
//                    am zuverlaessigsten angezeigt und verbunden -> guter Test.
// AP_CHANNEL       -> falls das Netz nicht auftaucht, 6 oder 11 statt 1 testen.
#define OPEN_AP     false
#define AP_CHANNEL  1

IPAddress apIP(192, 168, 4, 1);
DNSServer dnsServer;
ESP8266WebServer server(80);

// ---------------------------------------------------------------------------
// ANZEIGE-ZUSTAND
// ---------------------------------------------------------------------------
char message[256]       = "Hallo!";   // vom Nutzer eingegebener Text (mit Tokens)
char displayBuffer[384] = "Hallo!";   // fertig zusammengesetzter Anzeigetext
uint8_t brightness  = 5;              // 0..15
uint8_t scrollSpeed = 50;             // Frame-Verzoegerung in ms (kleiner = schneller)
bool    scrollMode  = true;           // true = Laufschrift, false = statisch/zentriert
bool    showTemp    = false;          // Temperatur hinter dem Text mitlaufen lassen
bool    showHum     = false;          // Luftfeuchte hinter dem Text mitlaufen lassen

textEffect_t effectIn  = PA_SCROLL_LEFT;
textEffect_t effectOut = PA_NO_EFFECT;

// ---------------------------------------------------------------------------
// SENSORWERTE
// ---------------------------------------------------------------------------
float    curTemp = NAN;
float    curHum  = NAN;
bool     sensorOk = false;
uint32_t lastSensorRead = 0;
const uint32_t SENSOR_INTERVAL = 3000;   // DHT11 max. ~alle 2 s abfragen

uint32_t pendingRestart = 0;             // != 0 -> Neustart nach kurzer Wartezeit

// ---------------------------------------------------------------------------
// EIGENE / INDIVIDUELLE ZEICHEN
// ---------------------------------------------------------------------------
// Jedes Zeichen ist 8 Pixel hoch. Jedes Byte ist eine senkrechte Pixelspalte
// (Bit0 = oben ... Bit7 = unten). Ueber einen internen Code (1..5) werden sie
// im Text referenziert. In der Weboberflaeche fuegt man LESBARE Tokens wie
// "{herz}" ein -- diese werden hier im Code in den Code umgewandelt (das ist
// robuster als echte Steuerzeichen durch HTML/HTTP zu schicken).
struct CustomChar {
  uint8_t     code;
  const char* token;
  uint8_t     width;
  uint8_t     data[8];
};

CustomChar customChars[] = {
  { 1, "{herz}",   5, { 0b00001100, 0b00011110, 0b00111100, 0b00011110, 0b00001100 } },
  { 2, "{smiley}", 5, { 0b00111100, 0b01000010, 0b10010101, 0b01000010, 0b00111100 } },
  { 3, "{pfeil}",  5, { 0b00011000, 0b00011000, 0b00011000, 0b01111110, 0b00111100 } },
  { 4, "{grad}",   3, { 0b00000110, 0b00001001, 0b00000110 } },
  { 5, "{note}",   5, { 0b01100000, 0b01111110, 0b00000010, 0b00001100, 0b00001100 } },
};
const uint8_t NUM_CUSTOM = sizeof(customChars) / sizeof(customChars[0]);

void registerCustomChars() {
  for (uint8_t i = 0; i < NUM_CUSTOM; i++) {
    // addChar erwartet einen Puffer im Font-Format: [Breite][Spalte0..N].
    uint8_t buf[9];
    buf[0] = customChars[i].width;
    for (uint8_t c = 0; c < customChars[i].width; c++) {
      buf[c + 1] = customChars[i].data[c];
    }
    display->addChar(customChars[i].code, buf);
  }
}

// Ersetzt alle "{token}" im Text durch das jeweilige Ein-Byte-Zeichen (Code 1..5).
String applyTokens(const String& in) {
  String out = in;
  for (uint8_t i = 0; i < NUM_CUSTOM; i++) {
    out.replace(customChars[i].token, String((char)customChars[i].code));
  }
  return out;
}

// ---------------------------------------------------------------------------
// WEB-OBERFLAECHE (im Flash gespeichert, um RAM zu sparen)
// ---------------------------------------------------------------------------
const char PAGE_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Matrix Display</title>
<style>
  :root { color-scheme: dark; }
  * { box-sizing: border-box; }
  body { font-family: system-ui, sans-serif; margin: 0; background:#111; color:#eee; }
  .wrap { max-width: 480px; margin: 0 auto; padding: 20px; }
  h1 { font-size: 1.3rem; text-align:center; }
  label { display:block; margin: 14px 0 4px; font-size:.9rem; color:#bbb; }
  input[type=text], textarea { width:100%; padding:10px; border-radius:8px;
    border:1px solid #444; background:#1c1c1c; color:#fff; font-size:1rem; }
  textarea { resize:vertical; min-height:60px; }
  input[type=range] { width:100%; }
  .row { display:flex; gap:10px; align-items:center; }
  .val { min-width:40px; text-align:right; color:#9ad; }
  button { width:100%; padding:12px; margin-top:18px; border:none; border-radius:8px;
    background:#2d7; color:#012; font-size:1rem; font-weight:600; cursor:pointer; }
  button:active { opacity:.8; }
  .chips { display:flex; flex-wrap:wrap; gap:6px; margin-top:6px; }
  .chip { padding:8px 12px; background:#333; border-radius:6px; cursor:pointer; font-size:1.1rem; }
  .seg { display:flex; border:1px solid #444; border-radius:8px; overflow:hidden; margin-top:4px; }
  .seg label { flex:1; margin:0; text-align:center; cursor:pointer; }
  .seg input { display:none; }
  .seg input:checked + span { background:#2d7; color:#012; }
  .seg span { display:block; padding:10px; }
  .checks { display:flex; gap:18px; margin-top:6px; }
  .checks label { display:flex; align-items:center; gap:8px; margin:0; color:#eee; font-size:1rem; cursor:pointer; }
  .checks input { width:20px; height:20px; }
  .sensor { margin-top:6px; padding:10px; background:#1c1c1c; border-radius:8px;
    text-align:center; color:#9ad; font-size:1.05rem; }
  small { color:#888; }
  #status { text-align:center; margin-top:10px; height:1.2em; color:#2d7; font-size:.85rem; }
</style>
</head>
<body>
<div class="wrap">
  <h1>&#128241; Matrix Display</h1>
  <form id="f">
    <label for="msg">Text</label>
    <textarea id="msg" name="msg" maxlength="255">%MSG%</textarea>

    <label>Sonderzeichen einfuegen</label>
    <div class="chips">
      <span class="chip" data-c="{herz}">&#10084;</span>
      <span class="chip" data-c="{smiley}">&#128578;</span>
      <span class="chip" data-c="{pfeil}">&#10132;</span>
      <span class="chip" data-c="{grad}">&#176;</span>
      <span class="chip" data-c="{note}">&#9834;</span>
    </div>
    <small>Fuegt eigene Pixel-Zeichen an der Cursorposition ein.</small>

    <label>Sensor (DHT11)</label>
    <div class="sensor">%SENSOR%</div>
    <div class="checks">
      <label><input type="checkbox" name="temp" value="1" %TEMP_CHK%> Temperatur mitlaufen</label>
      <label><input type="checkbox" name="hum" value="1" %HUM_CHK%> Feuchte mitlaufen</label>
    </div>
    <small>Laeuft jeweils hinter dem Text durch. Beides abwaehlen = nur Text.</small>

    <label>Modus</label>
    <div class="seg">
      <label><input type="radio" name="mode" value="1" %SCROLL_ON%><span>Laufschrift</span></label>
      <label><input type="radio" name="mode" value="0" %SCROLL_OFF%><span>Statisch</span></label>
    </div>

    <label>Matrix-Groesse</label>
    <div class="seg">
      <label><input type="radio" name="modules" value="4" %MOD32%><span>8&times;32</span></label>
      <label><input type="radio" name="modules" value="8" %MOD64%><span>8&times;64</span></label>
    </div>
    <small>Muss zur angeschlossenen Hardware passen. Bei Aenderung startet das Display kurz neu.</small>

    <label for="speed">Geschwindigkeit <small>(kleiner = schneller)</small></label>
    <div class="row">
      <input type="range" id="speed" name="speed" min="10" max="150" value="%SPEED%">
      <span class="val" id="speedv">%SPEED%</span>
    </div>

    <label for="bri">Helligkeit</label>
    <div class="row">
      <input type="range" id="bri" name="bri" min="0" max="15" value="%BRI%">
      <span class="val" id="briv">%BRI%</span>
    </div>

    <button type="submit">An Display senden</button>
  </form>
  <div id="status"></div>
</div>
<script>
  const $ = s => document.querySelector(s);
  $('#speed').oninput = e => $('#speedv').textContent = e.target.value;
  $('#bri').oninput   = e => $('#briv').textContent   = e.target.value;

  document.querySelectorAll('.chip').forEach(ch => {
    ch.onclick = () => {
      const t = $('#msg'), c = ch.dataset.c;
      const s = t.selectionStart, e = t.selectionEnd;
      t.value = t.value.slice(0, s) + c + t.value.slice(e);
      t.focus(); t.selectionStart = t.selectionEnd = s + c.length;
    };
  });

  $('#f').onsubmit = async ev => {
    ev.preventDefault();
    const data = new URLSearchParams(new FormData(ev.target));
    $('#status').textContent = 'senden...';
    try {
      const res = await fetch('/set', { method:'POST', body:data });
      const txt = await res.text();
      $('#status').textContent = (txt.trim() === 'RESTART')
        ? 'Groesse geaendert – Display startet neu…' : 'gesendet ✓';
    } catch (err) {
      $('#status').textContent = 'Fehler';
    }
    setTimeout(() => $('#status').textContent = '', 2500);
  };
</script>
</body>
</html>
)HTML";

// ---------------------------------------------------------------------------
// HELFER
// ---------------------------------------------------------------------------
String sensorLabel() {
  if (!sensorOk) return "kein Sensor / Lesefehler";
  String s = String((int)round(curTemp)) + " &#176;C  /  ";
  s += String((int)round(curHum)) + " %";
  return s;
}

String buildPage() {
  String html = FPSTR(PAGE_HTML);
  html.replace("%MSG%", String(message));
  html.replace("%SPEED%", String(scrollSpeed));
  html.replace("%BRI%", String(brightness));
  html.replace("%SCROLL_ON%",  scrollMode ? "checked" : "");
  html.replace("%SCROLL_OFF%", scrollMode ? "" : "checked");
  html.replace("%TEMP_CHK%", showTemp ? "checked" : "");
  html.replace("%HUM_CHK%",  showHum  ? "checked" : "");
  html.replace("%MOD32%", moduleCount == 4 ? "checked" : "");
  html.replace("%MOD64%", moduleCount == 8 ? "checked" : "");
  html.replace("%SENSOR%", sensorLabel());
  return html;
}

// Baut den kompletten Anzeigetext: Nutzertext + optional Temperatur + Feuchte.
// Die Sensorwerte laufen dadurch hinter dem Text mit und werden bei jedem
// Scroll-Durchlauf mit frischen Messwerten neu zusammengesetzt.
void composeText() {
  String text = applyTokens(String(message));

  if (showTemp) {
    text += "   ";
    // "{grad}" -> Grad-Zeichen (Code 4)
    text += sensorOk ? (String((int)round(curTemp)) + applyTokens("{grad}") + "C")
                     : String("--" ) + applyTokens("{grad}") + "C";
  }
  if (showHum) {
    text += "   ";
    text += sensorOk ? (String((int)round(curHum)) + "%") : String("--%");
  }

  text.toCharArray(displayBuffer, sizeof(displayBuffer));
}

void showCurrent() {
  composeText();
  display->setIntensity(brightness);
  textPosition_t align;
  if (scrollMode) {
    effectIn  = PA_SCROLL_LEFT;
    effectOut = PA_SCROLL_LEFT;
    align     = PA_LEFT;
  } else {
    effectIn  = PA_PRINT;
    effectOut = PA_NO_EFFECT;
    align     = PA_CENTER;
  }
  display->displayText(displayBuffer, align,
                       scrollSpeed, scrollMode ? 0 : 3000,
                       effectIn, effectOut);
  display->displayReset();
}

void readSensor() {
  TempAndHumidity th = dht.getTempAndHumidity();
  if (!isnan(th.temperature) && !isnan(th.humidity)) {
    curTemp  = th.temperature;
    curHum   = th.humidity;
    sensorOk = true;
  } else {
    sensorOk = false;
  }
}

// ---------------------------------------------------------------------------
// WEB-HANDLER
// ---------------------------------------------------------------------------
void handleRoot() {
  server.send(200, "text/html", buildPage());
}

void handleSet() {
  // Matrix-Groesse geaendert? -> speichern und neu starten (Kettenlaenge steht
  // beim Boot fest, daher ist ein Neustart noetig).
  if (server.hasArg("modules")) {
    uint8_t m = server.arg("modules").toInt();
    if ((m == 4 || m == 8) && m != moduleCount) {
      saveModuleCount(m);
      server.send(200, "text/plain", "RESTART");
      pendingRestart = millis();     // kurz warten, damit die Antwort rausgeht
      return;
    }
  }

  if (server.hasArg("msg")) {
    server.arg("msg").toCharArray(message, sizeof(message));
  }
  if (server.hasArg("bri"))   brightness  = constrain(server.arg("bri").toInt(), 0, 15);
  if (server.hasArg("speed")) scrollSpeed = constrain(server.arg("speed").toInt(), 5, 300);
  if (server.hasArg("mode"))  scrollMode  = (server.arg("mode").toInt() == 1);
  // Checkboxen: nur vorhanden, wenn angehakt.
  showTemp = server.hasArg("temp");
  showHum  = server.hasArg("hum");

  showCurrent();
  server.send(200, "text/plain", "OK");
}

// Captive-Portal: jede unbekannte Adresse leitet auf die Startseite.
void handleNotFound() {
  server.sendHeader("Location", String("http://") + apIP.toString(), true);
  server.send(302, "text/plain", "");
}

// ---------------------------------------------------------------------------
// SETUP
// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  Serial.println();

  // Sensor starten
  dht.setup(DHT_PIN, DHTesp::DHT11);

  // Gespeicherte Matrix-Groesse laden und Display damit anlegen
  loadModuleCount();
  display = new MD_Parola(HARDWARE_TYPE, CS_PIN, moduleCount);

  // Display starten
  display->begin();
  display->setIntensity(brightness);
  display->displayClear();
  registerCustomChars();

  readSensor();
  showCurrent();

  Serial.print("Matrix-Module: ");
  Serial.print(moduleCount);
  Serial.println(moduleCount == 8 ? " (8x64)" : " (8x32)");

  // Access Point starten
  WiFi.persistent(false);            // Flash-Schreibzugriffe vermeiden
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP);
  WiFi.setPhyMode(WIFI_PHY_MODE_11B);
  delay(100);

  bool apOk = OPEN_AP
    ? WiFi.softAP(AP_SSID, (const char*)nullptr, AP_CHANNEL, 0 /*sichtbar*/)
    : WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, 0 /*sichtbar*/);
  delay(100);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

  Serial.println();
  Serial.print("softAP() Ergebnis: ");
  Serial.println(apOk ? "OK" : "FEHLER");
  Serial.print("SSID: ");
  Serial.println(AP_SSID);
  Serial.print("AP-MAC: ");
  Serial.println(WiFi.softAPmacAddress());
  Serial.print("Access Point IP: ");
  Serial.println(WiFi.softAPIP());

  // DNS fuer Captive-Portal (alles auf den ESP umleiten)
  dnsServer.start(DNS_PORT, "*", apIP);

  // Webserver-Routen
  server.on("/", handleRoot);
  server.on("/set", HTTP_POST, handleSet);
  server.onNotFound(handleNotFound);
  server.begin();

  Serial.println("Bereit. Verbinde dich mit dem WLAN 'MatrixDisplay'.");
}

// ---------------------------------------------------------------------------
// LOOP
// ---------------------------------------------------------------------------
void loop() {
  dnsServer.processNextRequest();
  server.handleClient();

  // Nach Groessenwechsel: kurz warten (Antwort rausschicken), dann neu starten
  if (pendingRestart && millis() - pendingRestart > 800) {
    ESP.restart();
  }

  // Sensor regelmaessig auslesen (DHT11 vertraegt max. ~alle 2 s)
  if (millis() - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = millis();
    readSensor();
  }

  // Animation weiterlaufen lassen; am Ende mit frischen Werten neu aufbauen
  if (display->displayAnimate()) {
    showCurrent();
  }
}
