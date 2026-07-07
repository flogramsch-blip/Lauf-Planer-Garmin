"use client";

import { useEffect, useState } from "react";
import dynamic from "next/dynamic";
import Controls from "@/components/Controls";
import ElevationProfile from "@/components/ElevationProfile";
import { DEFAULT_PARAMS, RouteParams, RoutePlan } from "@/lib/types";

// Leaflet nur clientseitig laden (kein SSR).
const MapView = dynamic(() => import("@/components/MapView"), { ssr: false });

const STORAGE_KEY = "lauf-planer-state-v1";

export default function Home() {
  const [start, setStart] = useState<[number, number] | null>(null);
  const [text, setText] = useState("");
  const [params, setParams] = useState<RouteParams>(DEFAULT_PARAMS);
  const [plan, setPlan] = useState<RoutePlan | null>(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  // Zustand aus localStorage wiederherstellen.
  useEffect(() => {
    try {
      const raw = localStorage.getItem(STORAGE_KEY);
      if (raw) {
        const s = JSON.parse(raw);
        if (s.start) setStart(s.start);
        if (s.text) setText(s.text);
        if (s.params) setParams({ ...DEFAULT_PARAMS, ...s.params });
      }
    } catch {
      /* ignore */
    }
  }, []);

  // Zustand persistieren.
  useEffect(() => {
    try {
      localStorage.setItem(
        STORAGE_KEY,
        JSON.stringify({ start, text, params }),
      );
    } catch {
      /* ignore */
    }
  }, [start, text, params]);

  function patchParams(patch: Partial<RouteParams>) {
    setParams((p) => ({ ...p, ...patch }));
  }

  async function generate(newSeed = false) {
    if (!start) {
      setError("Bitte zuerst einen Startpunkt auf der Karte wählen.");
      return;
    }
    setLoading(true);
    setError(null);
    try {
      const seed = newSeed ? Math.floor(Math.random() * 100000) : params.seed;
      const res = await fetch("/api/plan", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ start, text, sliders: params, seed }),
      });
      const data = await res.json();
      if (!res.ok) throw new Error(data.error ?? "Route konnte nicht geplant werden.");
      const p = data as RoutePlan;
      setPlan(p);
      // Von der KI/ORS zurueckgegebene Params in die UI uebernehmen.
      setParams((prev) => ({ ...prev, ...p.params }));
    } catch (err) {
      setError(err instanceof Error ? err.message : "Unbekannter Fehler.");
      setPlan(null);
    } finally {
      setLoading(false);
    }
  }

  async function downloadGpx() {
    if (!plan) return;
    const res = await fetch("/api/gpx", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(plan),
    });
    if (!res.ok) {
      setError("GPX-Export fehlgeschlagen.");
      return;
    }
    const blob = await res.blob();
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = `lauf-${(plan.distanceMeters / 1000).toFixed(1)}km.gpx`;
    a.click();
    URL.revokeObjectURL(url);
  }

  return (
    <div className="app">
      <header className="app-header">
        <h1>🏃 Lauf-Planer für Garmin</h1>
        <p>Runde planen · als GPX exportieren · in Garmin Connect laden</p>
      </header>

      <div className="layout">
        <aside className="sidebar">
          <div className="card">
            <h2>Was für eine Runde?</h2>
            <textarea
              rows={3}
              placeholder="z. B. „10 km, flach, viel Grün, keine Hauptstraßen“"
              value={text}
              onChange={(e) => setText(e.target.value)}
            />
            <p className="hint" style={{ marginTop: 8 }}>
              Freitext ist optional – die KI übersetzt ihn in die Regler unten.
              Ohne Anthropic-Key nutzt du einfach die Regler.
            </p>
          </div>

          <Controls params={params} onChange={patchParams} />

          <button
            className="btn"
            onClick={() => generate(false)}
            disabled={loading}
          >
            {loading ? "Plane Route…" : "Route planen"}
          </button>

          {plan && (
            <div className="card">
              <h2>Ergebnis</h2>
              {plan.description && <p className="desc">{plan.description}</p>}
              <div className="stats">
                <div className="stat">
                  <div className="val">
                    {(plan.distanceMeters / 1000).toFixed(2)}
                  </div>
                  <div className="lbl">km</div>
                </div>
                <div className="stat">
                  <div className="val">+{plan.ascent}</div>
                  <div className="lbl">m Aufstieg</div>
                </div>
                <div className="stat">
                  <div className="val">−{plan.descent}</div>
                  <div className="lbl">m Abstieg</div>
                </div>
              </div>
              <div style={{ marginTop: 12 }}>
                <ElevationProfile
                  coordinates={plan.coordinates}
                  distanceMeters={plan.distanceMeters}
                />
              </div>
              <div className="row" style={{ marginTop: 12 }}>
                <button
                  className="btn btn-secondary"
                  onClick={() => generate(true)}
                  disabled={loading}
                >
                  🔀 Neue Variante
                </button>
                <button className="btn" onClick={downloadGpx}>
                  ⬇ GPX
                </button>
              </div>
            </div>
          )}

          {error && <div className="error">{error}</div>}
        </aside>

        <div className="map-wrap">
          {!start && (
            <div className="map-tip">
              👆 Tippe auf die Karte, um Start/Ziel deiner Runde zu setzen.
            </div>
          )}
          <MapView
            start={start}
            route={plan?.coordinates ?? null}
            onPick={(lat, lng) => {
              setStart([lat, lng]);
              setError(null);
            }}
          />
        </div>
      </div>
    </div>
  );
}
