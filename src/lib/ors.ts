// OpenRouteService-Client: erzeugt Rundkurse (round_trip) fuer das Fuss-Profil.
// Doku: https://openrouteservice.org/dev/#/api-docs/v2/directions

import { RouteParams, RoutePlan, LngLat } from "./types";

const ORS_BASE = "https://api.openrouteservice.org/v2/directions";

/** Abbildung der Hoehen-Praeferenz auf ORS-Optionen. */
function elevationOptions(params: RouteParams): Record<string, unknown> {
  // ORS steuert "flach vs. bergig" ueber das Gewicht "green"/"quiet" nicht direkt;
  // die Hoehe kommt aus dem Profil + der round_trip-Geometrie. Wir nutzen daher
  // vor allem die avoid_features und lassen die Hoehe ueber elevation:true messen.
  const avoidFeatures: string[] = [];
  if (params.avoidRoads) avoidFeatures.push("ferries");
  if (params.avoidSteps) avoidFeatures.push("steps");

  const options: Record<string, unknown> = {};
  if (avoidFeatures.length > 0) options.avoid_features = avoidFeatures;
  return options;
}

interface OrsGeoJson {
  features: Array<{
    geometry: { coordinates: number[][] };
    properties: {
      summary: { distance: number; duration: number };
      ascent?: number;
      descent?: number;
    };
  }>;
  error?: { code: number; message: string };
}

/** Ein einzelner round_trip-Aufruf gegen ORS. */
async function callOrs(
  start: LngLat,
  lengthMeters: number,
  params: RouteParams,
  seed: number,
  apiKey: string,
): Promise<RoutePlan> {
  const url = `${ORS_BASE}/${params.profile}/geojson`;

  const body = {
    coordinates: [start],
    elevation: true,
    instructions: false,
    options: {
      ...elevationOptions(params),
      round_trip: {
        length: Math.round(lengthMeters),
        // Mehr Punkte = organischere Runde. 3-5 ist ein guter Bereich.
        points: 4,
        seed,
      },
    },
  };

  const res = await fetch(url, {
    method: "POST",
    headers: {
      Authorization: apiKey,
      "Content-Type": "application/json",
      Accept: "application/geo+json",
    },
    body: JSON.stringify(body),
  });

  const data = (await res.json()) as OrsGeoJson;

  if (!res.ok || data.error) {
    const msg = data.error?.message ?? `ORS-Fehler (HTTP ${res.status})`;
    throw new Error(msg);
  }

  const feature = data.features?.[0];
  if (!feature) throw new Error("ORS lieferte keine Route zurueck.");

  return {
    coordinates: feature.geometry.coordinates,
    distanceMeters: feature.properties.summary.distance,
    ascent: Math.round(feature.properties.ascent ?? 0),
    descent: Math.round(feature.properties.descent ?? 0),
    params: { ...params, seed },
  };
}

/**
 * Erzeugt einen Rundkurs, der die Ziel-Distanz moeglichst gut trifft.
 * ORS trifft `length` nur naeherungsweise – deshalb korrigieren wir bei
 * grosser Abweichung einmal nach (einfache Iteration mit Toleranz).
 */
export async function planRoundTrip(
  start: LngLat,
  params: RouteParams,
): Promise<RoutePlan> {
  const apiKey = process.env.ORS_API_KEY;
  if (!apiKey) {
    throw new Error(
      "ORS_API_KEY fehlt. Bitte in .env.local eintragen (kostenlos auf openrouteservice.org).",
    );
  }

  const seed = params.seed ?? Math.floor(Math.random() * 100000);
  const targetMeters = params.distanceKm * 1000;

  // Erster Versuch.
  let plan = await callOrs(start, targetMeters, params, seed, apiKey);

  // Toleranz 15 %: wenn zu weit daneben, mit skaliertem length nachkorrigieren.
  const tolerance = 0.15;
  const deviation = Math.abs(plan.distanceMeters - targetMeters) / targetMeters;
  if (deviation > tolerance && plan.distanceMeters > 0) {
    const correction = targetMeters / plan.distanceMeters;
    const correctedLength = targetMeters * correction;
    try {
      const retry = await callOrs(start, correctedLength, params, seed, apiKey);
      // Nur uebernehmen, wenn der zweite Versuch naeher dran ist.
      const retryDev =
        Math.abs(retry.distanceMeters - targetMeters) / targetMeters;
      if (retryDev < deviation) plan = retry;
    } catch {
      // Korrektur-Versuch optional – Erstergebnis behalten.
    }
  }

  return plan;
}
