"use client";

import { ElevationPreference, FootProfile, RouteParams } from "@/lib/types";

interface ControlsProps {
  params: RouteParams;
  onChange: (patch: Partial<RouteParams>) => void;
}

const ELEVATIONS: { key: ElevationPreference; label: string }[] = [
  { key: "flat", label: "Flach" },
  { key: "any", label: "Egal" },
  { key: "hilly", label: "Hügelig" },
];

const PROFILES: { key: FootProfile; label: string }[] = [
  { key: "foot-walking", label: "Wege/Stadt" },
  { key: "foot-hiking", label: "Trails/Natur" },
];

export default function Controls({ params, onChange }: ControlsProps) {
  return (
    <div className="card">
      <h2>Feinjustierung</h2>

      <div className="field">
        <label>
          Distanz <b>{params.distanceKm.toFixed(1)} km</b>
        </label>
        <input
          type="range"
          min={1}
          max={30}
          step={0.5}
          value={params.distanceKm}
          onChange={(e) => onChange({ distanceKm: Number(e.target.value) })}
        />
      </div>

      <div className="field">
        <label>Höhenprofil</label>
        <div className="seg">
          {ELEVATIONS.map((o) => (
            <button
              key={o.key}
              className={params.elevation === o.key ? "active" : ""}
              onClick={() => onChange({ elevation: o.key })}
            >
              {o.label}
            </button>
          ))}
        </div>
      </div>

      <div className="field">
        <label>Untergrund</label>
        <div className="seg">
          {PROFILES.map((o) => (
            <button
              key={o.key}
              className={params.profile === o.key ? "active" : ""}
              onClick={() => onChange({ profile: o.key })}
            >
              {o.label}
            </button>
          ))}
        </div>
      </div>

      <div className="checks">
        <label>
          <input
            type="checkbox"
            checked={params.avoidRoads}
            onChange={(e) => onChange({ avoidRoads: e.target.checked })}
          />
          Fähren / Straßen meiden
        </label>
        <label>
          <input
            type="checkbox"
            checked={params.avoidSteps}
            onChange={(e) => onChange({ avoidSteps: e.target.checked })}
          />
          Treppen meiden
        </label>
      </div>
    </div>
  );
}
