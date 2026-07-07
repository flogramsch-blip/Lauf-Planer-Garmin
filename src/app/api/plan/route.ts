// POST /api/plan
// Nimmt Startpunkt + Freitext + Slider entgegen, extrahiert (per KI) Parameter,
// ruft OpenRouteService fuer einen Rundkurs auf und liefert die Route + Stats zurueck.

import { NextRequest, NextResponse } from "next/server";
import { parseIntent } from "@/lib/llm";
import { planRoundTrip } from "@/lib/ors";
import { DEFAULT_PARAMS, PlanRequest, RouteParams, LngLat } from "@/lib/types";

export const runtime = "nodejs";

export async function POST(req: NextRequest) {
  let payload: PlanRequest;
  try {
    payload = (await req.json()) as PlanRequest;
  } catch {
    return NextResponse.json({ error: "Ungueltiger Request-Body." }, { status: 400 });
  }

  const { start, text, sliders, seed } = payload;
  if (
    !Array.isArray(start) ||
    start.length !== 2 ||
    typeof start[0] !== "number" ||
    typeof start[1] !== "number"
  ) {
    return NextResponse.json(
      { error: "Startpunkt (start: [lat, lng]) fehlt oder ist ungueltig." },
      { status: 400 },
    );
  }

  // 1) KI-Intent aus Freitext (falls Text + Key vorhanden).
  let description: string | undefined;
  let baseParams: RouteParams = { ...DEFAULT_PARAMS };
  try {
    const parsed = text ? await parseIntent(text) : null;
    if (parsed) {
      baseParams = parsed.params;
      description = parsed.description;
    }
  } catch (err) {
    // KI-Fehler sind nicht fatal – mit Slidern/Defaults weitermachen.
    console.warn("Intent-Parsing fehlgeschlagen:", err);
  }

  // 2) Slider ueberschreiben KI-Werte, wo explizit gesetzt.
  const params: RouteParams = {
    ...baseParams,
    ...(sliders ?? {}),
    seed: seed ?? sliders?.seed ?? baseParams.seed,
  };

  // 3) Routing.
  // start kommt als [lat, lng] (UI), ORS erwartet [lng, lat].
  const startLngLat: LngLat = [start[1], start[0]];
  try {
    const plan = await planRoundTrip(startLngLat, params);
    if (description) plan.description = description;
    return NextResponse.json(plan);
  } catch (err) {
    const message =
      err instanceof Error ? err.message : "Unbekannter Routing-Fehler.";
    return NextResponse.json({ error: message }, { status: 502 });
  }
}
