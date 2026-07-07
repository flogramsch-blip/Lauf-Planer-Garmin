// POST /api/gpx
// Nimmt einen RoutePlan entgegen und liefert eine GPX-Datei zum Download.

import { NextRequest, NextResponse } from "next/server";
import { buildGpx, gpxFilename } from "@/lib/gpx";
import { RoutePlan } from "@/lib/types";

export const runtime = "nodejs";

export async function POST(req: NextRequest) {
  let plan: RoutePlan;
  try {
    plan = (await req.json()) as RoutePlan;
  } catch {
    return NextResponse.json({ error: "Ungueltiger Request-Body." }, { status: 400 });
  }

  if (!Array.isArray(plan.coordinates) || plan.coordinates.length < 2) {
    return NextResponse.json(
      { error: "RoutePlan enthaelt keine gueltigen Koordinaten." },
      { status: 400 },
    );
  }

  const gpx = buildGpx(plan);
  const filename = gpxFilename(plan);

  return new NextResponse(gpx, {
    status: 200,
    headers: {
      "Content-Type": "application/gpx+xml",
      "Content-Disposition": `attachment; filename="${filename}"`,
    },
  });
}
