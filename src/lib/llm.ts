// KI-Schicht: uebersetzt einen Freitext-Wunsch in Routing-Parameter.
// Nutzt Claude mit Tool-Use, damit die Ausgabe garantiert strukturiertes JSON ist.
// Faellt sauber zurueck, wenn kein API-Key gesetzt ist.

import Anthropic from "@anthropic-ai/sdk";
import { DEFAULT_PARAMS, RouteParams } from "./types";

const MODEL = process.env.ANTHROPIC_MODEL || "claude-haiku-4-5";

/** JSON-Schema fuer die extrahierten Parameter (Tool-Use Input). */
const paramTool: Anthropic.Tool = {
  name: "set_route_params",
  description:
    "Legt die Routing-Parameter fuer eine Laufstrecke aus der Nutzer-Beschreibung fest.",
  input_schema: {
    type: "object",
    properties: {
      distanceKm: {
        type: "number",
        description: "Ziel-Distanz in Kilometern (z.B. 5, 10, 21.1).",
      },
      elevation: {
        type: "string",
        enum: ["flat", "any", "hilly"],
        description:
          "Hoehen-Praeferenz: 'flat' (flach), 'hilly' (bergig/Hoehenmeter gewuenscht), sonst 'any'.",
      },
      avoidRoads: {
        type: "boolean",
        description:
          "true, wenn der Nutzer Strassen/Autoverkehr meiden moechte (z.B. 'durch den Park', 'keine Hauptstrassen').",
      },
      avoidSteps: {
        type: "boolean",
        description: "true, wenn Treppen gemieden werden sollen.",
      },
      profile: {
        type: "string",
        enum: ["foot-walking", "foot-hiking"],
        description:
          "'foot-hiking' fuer Trails/Natur/Wald, sonst 'foot-walking' (Stadt/Wege).",
      },
      description: {
        type: "string",
        description:
          "Ein kurzer, freundlicher deutscher Satz, der die geplante Runde zusammenfasst.",
      },
    },
    required: ["distanceKm", "elevation", "avoidRoads", "profile"],
  },
};

export interface ParsedIntent {
  params: RouteParams;
  description?: string;
}

/**
 * Extrahiert Routing-Parameter aus Freitext.
 * @param text  Nutzer-Wunsch, z.B. "8 km, flach, viel Gruen, keine Hauptstrassen".
 * @returns     Geparste Parameter + optionale Beschreibung, oder null wenn kein LLM verfuegbar.
 */
export async function parseIntent(text: string): Promise<ParsedIntent | null> {
  const apiKey = process.env.ANTHROPIC_API_KEY;
  if (!apiKey || !text.trim()) return null;

  const client = new Anthropic({ apiKey });

  const msg = await client.messages.create({
    model: MODEL,
    max_tokens: 512,
    tools: [paramTool],
    tool_choice: { type: "tool", name: "set_route_params" },
    messages: [
      {
        role: "user",
        content: `Der Nutzer plant eine Laufstrecke und beschreibt seinen Wunsch. Extrahiere die Routing-Parameter. Wenn keine Distanz genannt ist, nimm 5 km an.\n\nWunsch: "${text}"`,
      },
    ],
  });

  const toolUse = msg.content.find(
    (b): b is Anthropic.ToolUseBlock => b.type === "tool_use",
  );
  if (!toolUse) return null;

  const input = toolUse.input as Partial<RouteParams> & {
    description?: string;
  };

  const params: RouteParams = {
    distanceKm:
      typeof input.distanceKm === "number" && input.distanceKm > 0
        ? input.distanceKm
        : DEFAULT_PARAMS.distanceKm,
    elevation: input.elevation ?? DEFAULT_PARAMS.elevation,
    avoidRoads: input.avoidRoads ?? DEFAULT_PARAMS.avoidRoads,
    avoidSteps: input.avoidSteps ?? DEFAULT_PARAMS.avoidSteps,
    profile: input.profile ?? DEFAULT_PARAMS.profile,
  };

  return { params, description: input.description };
}
