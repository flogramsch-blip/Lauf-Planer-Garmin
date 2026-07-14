/*
 * ESP8266 (Wemos D1 mini) + 8x32 MAX7219 Dot-Matrix
 * -------------------------------------------------
 * WLAN-Steuerung einer Laufschrift / statischem Text -- komplett OFFLINE.
 * Der ESP spannt ein eigenes WLAN (Access Point) auf, es wird KEIN
 * Router und KEIN externer Server benoetigt.
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

// ---------------------------------------------------------------------------
// HARDWARE-KONFIGURATION
// ---------------------------------------------------------------------------
// Fuer dieses Modul getestet und korrekt: ICSTATION_HW.
// Falls die Anzeige mal spiegelverkehrt / Bloecke vertauscht sind, hier eine
// der anderen Varianten testen: FC16_HW, GENERIC_HW, PAROLA_HW
#define HARDWARE_TYPE MD_MAX72XX::ICSTATION_HW
#define MAX_DEVICES   4          // 8x32 = 4 Bloecke a 8x8

// Verkabelung Wemos D1 mini <-> MAX7219
//   MAX7219 VCC  -> 5V (VBUS/5V vom Wemos)
//   MAX7219 GND  -> GND
//   MAX7219 DIN  -> D7 (GPIO13, MOSI)   [durch SPI vorgegeben]
//   MAX7219 CLK  -> D5 (GPIO14, SCK)    [durch SPI vorgegeben]
//   MAX7219 CS   -> D8 (GPIO15)         [frei waehlbar, unten definiert]
#define CS_PIN  D8

MD_Parola display = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

// ---------------------------------------------------------------------------
// WLAN ACCESS POINT
// ---------------------------------------------------------------------------
const char* AP_SSID     = "MatrixDisplay";
const char* AP_PASSWORD = "12345678";        // mind. 8 Zeichen, sonst offenes Netz
const byte  DNS_PORT    = 53;

IPAddress apIP(192, 168, 4, 1);
DNSServer dnsServer;
ESP8266WebServer server(80);

// ---------------------------------------------------------------------------
// ANZEIGE-ZUSTAND
// ---------------------------------------------------------------------------
char message[256] = "Hallo!";      // aktueller Text
uint8_t brightness = 5;            // 0..15
uint8_t scrollSpeed = 50;          // Frame-Verzoegerung in ms (kleiner = schneller)
bool    scrollMode  = true;        // true = Laufschrift, false = statisch/zentriert

textEffect_t effectIn  = PA_SCROLL_LEFT;
textEffect_t effectOut = PA_NO_EFFECT;

// ---------------------------------------------------------------------------
// EIGENE / INDIVIDUELLE ZEICHEN
// ---------------------------------------------------------------------------
// Jedes Zeichen ist 8 Pixel hoch. Die Zahlen sind Spalten (hier 5 Spalten breit),
// jedes Byte ist eine senkrechte Pixelspalte (Bit0 = oben ... Bit7 = unten).
// Ueber einen Platzhalter-Code (hier ASCII 1..6) koennen sie im Text mit '\x01'
// usw. verwendet werden -- z.B. Herz, Smiley, Pfeile.
struct CustomChar {
  uint8_t code;
  uint8_t width;
  uint8_t data[8];
};

CustomChar customChars[] = {
  // Herz
  { 1, 5, { 0b00001100, 0b00011110, 0b00111100, 0b00011110, 0b00001100 } },
  // Smiley :)
  { 2, 5, { 0b00111100, 0b01000010, 0b10010101, 0b01000010, 0b00111100 } },
  // Pfeil rechts
  { 3, 5, { 0b00011000, 0b00011000, 0b00011000, 0b01111110, 0b00111100 } },
  // Grad-Zeichen
  { 4, 3, { 0b00000110, 0b00001001, 0b00000110 } },
  // Note
  { 5, 5, { 0b01100000, 0b01111110, 0b00000010, 0b00001100, 0b00001100 } },
};
const uint8_t NUM_CUSTOM = sizeof(customChars) / sizeof(customChars[0]);

void registerCustomChars() {
  for (uint8_t i = 0; i < NUM_CUSTOM; i++) {
    // addChar erwartet einen Puffer im Font-Format: [Breite][Spalte0..N].
    // Deshalb setzen wir die Breite als erstes Byte vor die Pixeldaten.
    uint8_t buf[9];
    buf[0] = customChars[i].width;
    for (uint8_t c = 0; c < customChars[i].width; c++) {
      buf[c + 1] = customChars[i].data[c];
    }
    display.addChar(customChars[i].code, buf);
  }
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
  .seg label { flex:1; margin:0; text-align:center; padding:10px; cursor:pointer; }
  .seg input { display:none; }
  .seg input:checked + span { background:#2d7; color:#012; }
  .seg span { display:block; padding:10px; }
  .seg label span { padding:0; }
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
      <span class="chip" data-c="&#1;">&#10084;</span>
      <span class="chip" data-c="&#2;">&#128578;</span>
      <span class="chip" data-c="&#3;">&#10132;</span>
      <span class="chip" data-c="&#4;">&#176;</span>
      <span class="chip" data-c="&#5;">&#9834;</span>
    </div>
    <small>Fuegt eigene Pixel-Zeichen an der Cursorposition ein.</small>

    <label>Modus</label>
    <div class="seg">
      <label><input type="radio" name="mode" value="1" %SCROLL_ON%><span>Laufschrift</span></label>
      <label><input type="radio" name="mode" value="0" %SCROLL_OFF%><span>Statisch</span></label>
    </div>

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
      await fetch('/set', { method:'POST', body:data });
      $('#status').textContent = 'gesendet ✓';
    } catch (err) {
      $('#status').textContent = 'Fehler';
    }
    setTimeout(() => $('#status').textContent = '', 1500);
  };
</script>
</body>
</html>
)HTML";

// ---------------------------------------------------------------------------
// HELFER
// ---------------------------------------------------------------------------
String buildPage() {
  String html = FPSTR(PAGE_HTML);
  html.replace("%MSG%", String(message));
  html.replace("%SPEED%", String(scrollSpeed));
  html.replace("%BRI%", String(brightness));
  html.replace("%SCROLL_ON%",  scrollMode ? "checked" : "");
  html.replace("%SCROLL_OFF%", scrollMode ? "" : "checked");
  return html;
}

void applyDisplaySettings() {
  display.setIntensity(brightness);
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
  display.displayText(message, align,
                      scrollSpeed, scrollMode ? 0 : 3000,
                      effectIn, effectOut);
  display.displayReset();
}

// ---------------------------------------------------------------------------
// WEB-HANDLER
// ---------------------------------------------------------------------------
void handleRoot() {
  server.send(200, "text/html", buildPage());
}

void handleSet() {
  if (server.hasArg("msg")) {
    server.arg("msg").toCharArray(message, sizeof(message));
  }
  if (server.hasArg("bri"))   brightness  = constrain(server.arg("bri").toInt(), 0, 15);
  if (server.hasArg("speed")) scrollSpeed = constrain(server.arg("speed").toInt(), 5, 300);
  if (server.hasArg("mode"))  scrollMode  = (server.arg("mode").toInt() == 1);

  applyDisplaySettings();
  server.send(200, "text/plain", "OK");
}

// Captive-Portal: jede unbekannte Adresse leitet auf die Startseite,
// damit sich beim Verbinden automatisch die Seite oeffnet.
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

  // Display starten
  display.begin();
  display.setIntensity(brightness);
  display.displayClear();
  registerCustomChars();
  applyDisplaySettings();

  // Access Point starten
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(AP_SSID, AP_PASSWORD);
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

  // Animation weiterlaufen lassen; bei statischem Text automatisch neu starten
  if (display.displayAnimate()) {
    display.displayReset();
  }
}
