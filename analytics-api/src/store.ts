import fs from "node:fs";
import path from "node:path";
import type { AnalyticsEvent } from "./types.js";

const DATA_DIR = path.resolve(process.cwd(), "data");
const EVENTS_FILE = path.join(DATA_DIR, "events.jsonl");

function ensureDataDir() {
  if (!fs.existsSync(DATA_DIR)) {
    fs.mkdirSync(DATA_DIR, { recursive: true });
  }
}

export function appendEvents(events: AnalyticsEvent[]) {
  ensureDataDir();
  const payload = events.map((event) => JSON.stringify(event)).join("\n") + "\n";
  fs.appendFileSync(EVENTS_FILE, payload, "utf8");
}

export function readEvents(): AnalyticsEvent[] {
  ensureDataDir();
  if (!fs.existsSync(EVENTS_FILE)) {
    return [];
  }

  const text = fs.readFileSync(EVENTS_FILE, "utf8");
  if (!text.trim()) {
    return [];
  }

  const rows = text.split(/\r?\n/).map((row) => row.trim()).filter(Boolean);
  const events: AnalyticsEvent[] = [];

  for (const row of rows) {
    try {
      events.push(JSON.parse(row) as AnalyticsEvent);
    } catch {
      continue;
    }
  }

  return events;
}
