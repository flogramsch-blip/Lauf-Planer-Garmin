// Zentrale Typen fuer den Lauf-Planer.

/** Ein Punkt als [Laengengrad, Breitengrad] (GeoJSON-Reihenfolge!). */
export type LngLat = [number, number];

/** Hoehen-Praeferenz fuer die Streckenwahl. */
export type ElevationPreference = "flat" | "any" | "hilly";

/** Fuss-Profil des Routing-Engines. */
export type FootProfile = "foot-walking" | "foot-hiking";

/**
 * Routing-Parameter, die entweder direkt aus den Slidern kommen
 * oder von der KI aus dem Freitext extrahiert werden.
 */
export interface RouteParams {
  /** Ziel-Distanz in Kilometern. */
  distanceKm: number;
  /** Hoehen-Praeferenz. */
  elevation: ElevationPreference;
  /** Strassen/Autoverkehr meiden? */
  avoidRoads: boolean;
  /** Treppen meiden (nuetzlich fuers Laufen)? */
  avoidSteps: boolean;
  /** Fuss-Profil (walking = Stadt/Wege, hiking = mehr Trails). */
  profile: FootProfile;
  /** Seed fuer die Rundkurs-Variation (gleiche Params, andere Runde). */
  seed?: number;
}

/** Sinnvolle Defaults, falls Felder fehlen. */
export const DEFAULT_PARAMS: RouteParams = {
  distanceKm: 5,
  elevation: "any",
  avoidRoads: false,
  avoidSteps: true,
  profile: "foot-walking",
};

/** Eine berechnete Route inkl. Statistik. */
export interface RoutePlan {
  /** Streckenpunkte als [lng, lat, ele?]. */
  coordinates: number[][];
  /** Tatsaechliche Distanz in Metern. */
  distanceMeters: number;
  /** Aufstieg in Metern. */
  ascent: number;
  /** Abstieg in Metern. */
  descent: number;
  /** Die tatsaechlich verwendeten Parameter. */
  params: RouteParams;
  /** Kurze, menschenlesbare Beschreibung (optional, von der KI). */
  description?: string;
}

/** Request-Body fuer /api/plan. */
export interface PlanRequest {
  /** Startpunkt [lat, lng] (UI-Reihenfolge). */
  start: [number, number];
  /** Freitext-Wunsch des Nutzers (optional). */
  text?: string;
  /** Explizite Slider-Werte (ueberschreiben KI-Werte). */
  sliders?: Partial<RouteParams>;
  /** Seed fuer "Neu generieren". */
  seed?: number;
}
