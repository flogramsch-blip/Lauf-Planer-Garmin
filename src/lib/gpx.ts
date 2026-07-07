// GPX-1.1-Builder: wandelt eine Route in eine GPX-Datei um,
// die Garmin Connect als Kurs importieren kann.

import { RoutePlan } from "./types";

/** XML-Sonderzeichen escapen. */
function esc(s: string): string {
  return s
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;");
}

/**
 * Erzeugt GPX aus einem RoutePlan.
 * Wir schreiben sowohl <rte> (Route) als auch <trk> (Track):
 * Garmin Connect kann beides zu einem Kurs importieren, ein Track ist
 * am robustesten fuer die spaetere Kurs-Navigation.
 */
export function buildGpx(plan: RoutePlan, name = "Lauf-Planer Route"): string {
  const time = new Date().toISOString();
  const km = (plan.distanceMeters / 1000).toFixed(2);
  const safeName = esc(name);

  const trkpts = plan.coordinates
    .map((c) => {
      const [lng, lat, ele] = c;
      const eleTag =
        typeof ele === "number" ? `<ele>${ele.toFixed(1)}</ele>` : "";
      return `      <trkpt lat="${lat.toFixed(6)}" lon="${lng.toFixed(
        6,
      )}">${eleTag}</trkpt>`;
    })
    .join("\n");

  return `<?xml version="1.0" encoding="UTF-8"?>
<gpx version="1.1" creator="Lauf-Planer-Garmin"
  xmlns="http://www.topografix.com/GPX/1/1"
  xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
  xsi:schemaLocation="http://www.topografix.com/GPX/1/1 http://www.topografix.com/GPX/1/1/gpx.xsd">
  <metadata>
    <name>${safeName}</name>
    <desc>${km} km, +${plan.ascent} m / -${plan.descent} m Hoehe</desc>
    <time>${time}</time>
  </metadata>
  <trk>
    <name>${safeName}</name>
    <trkseg>
${trkpts}
    </trkseg>
  </trk>
</gpx>
`;
}

/** Dateiname aus dem Routennamen ableiten. */
export function gpxFilename(plan: RoutePlan): string {
  const km = (plan.distanceMeters / 1000).toFixed(1).replace(".", "_");
  return `lauf-${km}km.gpx`;
}
