# 🏃 Lauf-Planer für Garmin

KI-gestützter Laufstrecken-Planer: Startpunkt wählen, Wunsch als Freitext oder per
Regler angeben, eine passende **Rundstrecke** generieren lassen und als **GPX**
exportieren – zum Import in **Garmin Connect** und Sync auf die **Forerunner**.

Als PWA gebaut (Next.js/React): läuft im Browser und lässt sich auf Android als App
installieren.

## Wie es funktioniert (3 Schichten)

```
Eingabe (Freitext + Regler + Startpunkt auf Karte)
  → KI (Claude): Freitext → Routing-Parameter
  → Routing (OpenRouteService round_trip, Fuß-Profil, OpenStreetMap)
  → Route + Höhenprofil
  → GPX-Export → Garmin Connect → Forerunner
```

Wichtig: Die **KI erfindet keine Wege**. Sie versteht nur den Wunsch und übersetzt ihn
in Parameter. Die echte, GPS-genaue Strecke berechnet der Routing-Engine auf
OpenStreetMap-Daten.

## Setup

1. **Abhängigkeiten installieren**

   ```bash
   npm install
   ```

2. **API-Keys eintragen** – `.env.example` nach `.env.local` kopieren und ausfüllen:

   ```bash
   cp .env.example .env.local
   ```

   - `ORS_API_KEY` – **erforderlich**. Kostenlos registrieren:
     https://openrouteservice.org/dev/#/signup
   - `ANTHROPIC_API_KEY` – *optional*. Nur für die Freitext-Eingabe. Ohne diesen Key
     funktionieren die Regler weiterhin.

3. **Starten**

   ```bash
   npm run dev
   ```

   → http://localhost:3000

## Benutzung

1. Auf die Karte tippen → **Start/Ziel** setzen (oder Standortfreigabe zulassen).
2. Wunsch als Freitext eingeben *oder* die Regler nutzen (Distanz, Höhenprofil,
   Untergrund, Straßen/Treppen meiden).
3. **Route planen** → Strecke + Höhenprofil erscheinen. **Neue Variante** würfelt eine
   andere Runde mit gleichen Vorgaben.
4. **⬇ GPX** → Datei herunterladen.

## GPX auf die Garmin Forerunner bringen

**Variante A – nur mit dem Handy (kein Kabel/PC):**

1. GPX-Datei herunterladen (z. B. in Google Drive / Dateien speichern).
2. Datei antippen → „Öffnen mit" → **Garmin Connect**.
3. Garmin Connect legt daraus einen **Kurs** an.
4. Uhr per Bluetooth syncen → auf der Uhr unter **Navigation → Kurse** starten.

**Variante B – am PC über den Browser:**

1. In [Garmin Connect Web](https://connect.garmin.com): **Training → Strecken →
   Importieren**.
2. GPX-Datei hochladen → als Kurs speichern.
3. Uhr syncen → **Navigation → Kurse**.

> Die Kurs-/Navigationsfunktion hängt vom Forerunner-Modell ab. Modelle mit
> Streckennavigation (z. B. FR 255/265/955/965) zeigen Turn-by-Turn; einfachere
> Modelle folgen der Linie.

## Projektstruktur

```
src/
  app/
    page.tsx              Haupt-UI (Karte, Freitext, Regler, Ergebnis)
    api/plan/route.ts     POST: Startpunkt + Wunsch → Route
    api/gpx/route.ts      POST: Route → GPX-Download
  components/
    MapView.tsx           Leaflet-Karte (Startpunkt, Route)
    Controls.tsx          Regler & Presets
    ElevationProfile.tsx  Höhenprofil (SVG)
  lib/
    ors.ts                OpenRouteService-Client (round_trip + Distanz-Toleranz)
    llm.ts                Claude Intent-Parser (Freitext → Parameter)
    gpx.ts                GPX-1.1-Builder
    types.ts              Gemeinsame Typen
```

## Nächste Ausbaustufen

- Direkte Garmin-Connect-Integration (Courses API, OAuth) – aktuell manueller GPX-Import.
- Accounts + gespeicherte Routen (DB).
- Mehrere Varianten nebeneinander vergleichen.
- Produktions-Kartenkacheln (MapTiler/Protomaps) statt OSM-Raster.
- Native Android-Verpackung via Capacitor.

## Tech-Stack

Next.js · React · TypeScript · Leaflet · OpenStreetMap · OpenRouteService · Anthropic Claude
