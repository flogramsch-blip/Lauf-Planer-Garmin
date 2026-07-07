"use client";

import { useEffect, useRef } from "react";
import type * as L from "leaflet";
import "leaflet/dist/leaflet.css";

interface MapViewProps {
  /** Startpunkt [lat, lng] oder null. */
  start: [number, number] | null;
  /** Routen-Koordinaten als [lng, lat, ele?] (GeoJSON-Reihenfolge). */
  route: number[][] | null;
  /** Callback beim Klick auf die Karte. */
  onPick: (lat: number, lng: number) => void;
}

const DEFAULT_CENTER: [number, number] = [52.52, 13.405]; // Berlin

export default function MapView({ start, route, onPick }: MapViewProps) {
  const containerRef = useRef<HTMLDivElement>(null);
  const mapRef = useRef<L.Map | null>(null);
  const startMarkerRef = useRef<L.Marker | null>(null);
  const routeLayerRef = useRef<L.Polyline | null>(null);
  const leafletRef = useRef<typeof L | null>(null);
  const onPickRef = useRef(onPick);
  onPickRef.current = onPick;

  // Karte einmalig initialisieren (Leaflet nur clientseitig laden).
  useEffect(() => {
    let cancelled = false;
    (async () => {
      const leaflet = (await import("leaflet")).default;
      if (cancelled || !containerRef.current || mapRef.current) return;
      leafletRef.current = leaflet;

      const map = leaflet.map(containerRef.current, {
        center: DEFAULT_CENTER,
        zoom: 13,
        zoomControl: true,
      });

      leaflet
        .tileLayer("https://tile.openstreetmap.org/{z}/{x}/{y}.png", {
          maxZoom: 19,
          attribution: "&copy; OpenStreetMap-Mitwirkende",
        })
        .addTo(map);

      map.on("click", (e: L.LeafletMouseEvent) => {
        onPickRef.current(e.latlng.lat, e.latlng.lng);
      });

      mapRef.current = map;

      // Nutzerstandort anbieten (best effort).
      if (navigator.geolocation) {
        navigator.geolocation.getCurrentPosition(
          (pos) => {
            if (!cancelled && mapRef.current) {
              mapRef.current.setView(
                [pos.coords.latitude, pos.coords.longitude],
                14,
              );
            }
          },
          () => {},
          { timeout: 5000 },
        );
      }
    })();

    return () => {
      cancelled = true;
      mapRef.current?.remove();
      mapRef.current = null;
    };
  }, []);

  // Startmarker aktualisieren.
  useEffect(() => {
    const leaflet = leafletRef.current;
    const map = mapRef.current;
    if (!leaflet || !map) return;

    if (startMarkerRef.current) {
      startMarkerRef.current.remove();
      startMarkerRef.current = null;
    }
    if (start) {
      const icon = leaflet.divIcon({
        className: "",
        html: '<div style="width:16px;height:16px;background:#14b8a6;border:3px solid #06231f;border-radius:50%;box-shadow:0 0 0 2px #14b8a6"></div>',
        iconSize: [16, 16],
        iconAnchor: [8, 8],
      });
      startMarkerRef.current = leaflet
        .marker(start, { icon })
        .addTo(map)
        .bindTooltip("Start / Ziel");
    }
  }, [start]);

  // Route aktualisieren.
  useEffect(() => {
    const leaflet = leafletRef.current;
    const map = mapRef.current;
    if (!leaflet || !map) return;

    if (routeLayerRef.current) {
      routeLayerRef.current.remove();
      routeLayerRef.current = null;
    }
    if (route && route.length > 1) {
      // [lng, lat] -> [lat, lng] fuer Leaflet.
      const latlngs = route.map((c) => [c[1], c[0]] as [number, number]);
      const line = leaflet
        .polyline(latlngs, { color: "#14b8a6", weight: 5, opacity: 0.9 })
        .addTo(map);
      routeLayerRef.current = line;
      map.fitBounds(line.getBounds(), { padding: [40, 40] });
    }
  }, [route]);

  return <div ref={containerRef} className="leaflet-container" />;
}
