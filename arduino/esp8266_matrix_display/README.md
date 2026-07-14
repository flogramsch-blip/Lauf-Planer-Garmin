# ESP8266 + 8×32 Dot-Matrix — WLAN-Laufschrift (offline)

Steuert eine 8×32 MAX7219-Matrix per Handy/Laptop über WLAN — **ohne Router,
ohne Server, komplett offline**. Der Wemos D1 mini spannt sein eigenes WLAN auf
und liefert eine kleine Weboberfläche aus.

## Was kann es?
- Laufschrift **oder** statischer/zentrierter Text
- Geschwindigkeit und Helligkeit live einstellbar
- Eigene Pixel-Zeichen (Herz, Smiley, Pfeil, Grad-Zeichen, Note) per Knopfdruck einfügbar
- Captive-Portal: beim Verbinden öffnet sich die Seite meist automatisch

## Hardware & Verkabelung

| MAX7219-Modul | Wemos D1 mini      | Hinweis                       |
|---------------|--------------------|-------------------------------|
| VCC           | 5V                 | Matrix zieht mehr Strom — an 5V, nicht 3V3 |
| GND           | GND                |                               |
| DIN           | D7 (GPIO13, MOSI)  | durch SPI fest vorgegeben     |
| CLK           | D5 (GPIO14, SCK)   | durch SPI fest vorgegeben     |
| CS            | D8 (GPIO15)        | im Sketch frei wählbar        |

> Bei größeren Modulen kann der USB-Strom knapp werden. Wenn die Anzeige
> flackert oder der ESP neu startet, ein externes 5V-Netzteil verwenden.

## Arduino IDE einrichten

1. **ESP8266-Board installieren**
   - *Datei → Voreinstellungen → Zusätzliche Boardverwalter-URLs*, eintragen:
     `https://arduino.esp8266.com/stable/package_esp8266com_index.json`
   - *Werkzeuge → Board → Boardverwalter* → „esp8266" installieren
   - Board wählen: **LOLIN(WEMOS) D1 R2 & mini**

2. **Bibliotheken installieren** (*Werkzeuge → Bibliotheken verwalten*)
   - `MD_Parola` (von majicDesigns)
   - `MD_MAX72XX` (wird meist automatisch mitinstalliert)

3. **Hochladen**
   - `esp8266_matrix_display.ino` öffnen, Upload-Button drücken.

## Benutzung

1. ESP mit Strom versorgen.
2. Am Handy/Laptop mit dem WLAN **`MatrixDisplay`** verbinden
   (Passwort **`12345678`**).
3. Browser öffnen → **http://192.168.4.1**
   (oder die automatisch aufpoppende Portal-Seite nutzen).
4. Text eingeben, Modus/Geschwindigkeit/Helligkeit wählen, **An Display senden**.

## WLAN erscheint nicht? — Fehlersuche

1. **Seriellen Monitor öffnen** (Werkzeuge → Serieller Monitor), **115200 Baud**
   einstellen, dann am ESP **RESET** drücken. Es sollte erscheinen:
   `softAP() Ergebnis: OK` und `Access Point IP: 192.168.4.1`.
   - Steht dort **OK** → der ESP sendet. Dann liegt es am Endgerät (s. Punkt 2).
   - Steht dort **FEHLER** oder gar nichts / wirre Zeichen → s. Punkt 3.
2. **Am Handy/Laptop:** WLAN-Liste manuell aktualisieren. `MatrixDisplay` ist ein
   **2,4-GHz**-Netz — es taucht nicht in reinen 5-GHz-Listen auf. Manche Handys
   verstecken „Netz ohne Internet"; einmal in den WLAN-Einstellungen suchen.
3. **Stromversorgung / Brownout:** Läuft im Serial-Monitor ein ständiger Neustart
   oder kommt nur Kauderwelsch, versorgt der USB-Port zu wenig Strom (WLAN erzeugt
   Sendespitzen). Abhilfe: anderes/kürzeres USB-Kabel, anderer USB-Port oder ein
   5-V-Netzteil. Zum Testen die Matrix kurz abklemmen — startet das WLAN dann,
   war es die Stromversorgung.
4. **Zeigt die Matrix „Hallo!" aber kein WLAN?** Dann läuft der Sketch, nur der
   AP-Start klappt nicht → Punkt 3 ist am wahrscheinlichsten.
5. **Anderer WLAN-Kanal:** im Sketch bei `WiFi.softAP(..., 1, 0)` die `1` auf
   `6` oder `11` ändern.

## Häufige Anpassungen (im Sketch oben)

- **`HARDWARE_TYPE`:** für dieses Modul ist `ICSTATION_HW` eingestellt (getestet).
  Falls die Anzeige mal gespiegelt / Blöcke vertauscht sind → `FC16_HW`,
  `GENERIC_HW` oder `PAROLA_HW` probieren.
- **WLAN-Name/Passwort:** `AP_SSID` / `AP_PASSWORD` ändern (Passwort ≥ 8 Zeichen).
- **Andere Displaygröße** (z. B. 8×64 = 8 Blöcke): `MAX_DEVICES` anpassen.
- **Eigene Zeichen:** im Array `customChars[]` ergänzen. Jedes Byte ist eine
  senkrechte Pixelspalte (Bit 0 = oben … Bit 7 = unten). Über den `code`
  (1…5) im Text als `\x01`…`\x05` bzw. per Chip-Button nutzbar.

## Zeichen selbst zeichnen

Ein 5 Spalten breites, 8 Pixel hohes Zeichen. Pro Spalte ein Byte, `1` = LED an:

```
Spalte:   1    2    3    4    5
oben     .    X    X    X    .     -> hier als Bitmuster je Spalte kodiert
         ...
unten
```

Am einfachsten mit einem Online-„MAX7219 font generator" die Bytes erzeugen und
in `customChars[]` eintragen.
