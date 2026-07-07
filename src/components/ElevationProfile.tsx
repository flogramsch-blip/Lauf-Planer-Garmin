"use client";

// Leichtgewichtiges Höhenprofil als Inline-SVG (keine Chart-Dependency).

interface ElevationProfileProps {
  /** Koordinaten [lng, lat, ele?]. */
  coordinates: number[][];
  distanceMeters: number;
}

export default function ElevationProfile({
  coordinates,
  distanceMeters,
}: ElevationProfileProps) {
  const eles = coordinates
    .map((c) => (typeof c[2] === "number" ? c[2] : null))
    .filter((e): e is number => e !== null);

  if (eles.length < 2) return null;

  const w = 320;
  const h = 70;
  const pad = 4;
  const min = Math.min(...eles);
  const max = Math.max(...eles);
  const range = max - min || 1;

  const pts = eles.map((e, i) => {
    const x = pad + (i / (eles.length - 1)) * (w - 2 * pad);
    const y = pad + (1 - (e - min) / range) * (h - 2 * pad);
    return `${x.toFixed(1)},${y.toFixed(1)}`;
  });

  const area = `${pad},${h - pad} ${pts.join(" ")} ${w - pad},${h - pad}`;

  return (
    <div>
      <svg
        viewBox={`0 0 ${w} ${h}`}
        width="100%"
        height={h}
        preserveAspectRatio="none"
        role="img"
        aria-label="Höhenprofil"
      >
        <polygon points={area} fill="rgba(20,184,166,0.18)" />
        <polyline
          points={pts.join(" ")}
          fill="none"
          stroke="#14b8a6"
          strokeWidth="2"
        />
      </svg>
      <div
        className="hint"
        style={{ display: "flex", justifyContent: "space-between" }}
      >
        <span>{Math.round(min)} m</span>
        <span>{(distanceMeters / 1000).toFixed(2)} km</span>
        <span>{Math.round(max)} m</span>
      </div>
    </div>
  );
}
