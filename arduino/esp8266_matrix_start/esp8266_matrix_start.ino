/*
 * PROJEKTSTART / HARDWARE-TEST
 * ----------------------------
 * Wemos D1 mini (ESP8266) + 8x32 MAX7219 Dot-Matrix
 *
 * Minimaler Sketch OHNE WLAN. Zweck: pruefen ob Verkabelung, Bibliotheken
 * und der HARDWARE_TYPE stimmen. Es laeuft dauerhaft eine Laufschrift.
 *
 * Wenn das hier sauber laeuft, kannst du den groesseren WLAN-Sketch
 * (../esp8266_matrix_display) hochladen.
 *
 * Bibliotheken (Arduino IDE -> Bibliotheken verwalten):
 *   - MD_Parola   (majicDesigns)
 *   - MD_MAX72XX  (wird meist mitinstalliert)
 *
 * Verkabelung:
 *   VCC -> 5V   |  GND -> GND
 *   DIN -> D7   |  CLK -> D5   |  CS -> D8
 */

#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

// Fast alle guenstigen 8x32 "4-in-1"-Module sind FC16_HW.
// Falls die Anzeige spiegelverkehrt / Bloecke vertauscht sind, hier testen:
//   MD_MAX72XX::GENERIC_HW, ::ICSTATION_HW, ::PAROLA_HW
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES   4          // 8x32 = 4 Bloecke a 8x8
#define CS_PIN        D8

MD_Parola display = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

void setup() {
  display.begin();                 // Matrix initialisieren
  display.setIntensity(5);         // Helligkeit 0..15
  display.displayClear();

  // Laufschrift von rechts nach links, endlos
  display.displayText("Projekt startklar!  ", PA_LEFT, 50, 0,
                      PA_SCROLL_LEFT, PA_SCROLL_LEFT);
}

void loop() {
  // Animation weiterlaufen lassen und am Ende automatisch neu starten
  if (display.displayAnimate()) {
    display.displayReset();
  }
}
