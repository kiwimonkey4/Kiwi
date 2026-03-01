/**
 * Express server for the Kiwi analytics API.
 * Receives events from the plugin (POST /api/trackEvent), stores in Firestore,
 * and serves filtered events to the dashboard (GET /api/events).
 */
import express from "express";
import cors from "cors";
import { batchSchema, type AnalyticsEvent } from "./types.js";
import { appendEvents, readEvents } from "./store.js";

const app = express();
const port = Number(process.env.PORT ?? 8787);

app.use(cors());
app.use(express.json({ limit: "1mb" }));

/** Parse query param as Date, or null if invalid */
function parseDateParam(value: unknown): Date | null {
  if (typeof value !== "string" || value.trim().length === 0) {
    return null;
  }

  const parsed = new Date(value);
  return Number.isNaN(parsed.getTime()) ? null : parsed;
}

/** Parse event filter query param (comma-separated event names), or null for "all" */
function parseEventFilter(value: unknown): Set<string> | null {
  if (typeof value !== "string" || value.trim().length === 0 || value === "all") {
    return null;
  }

  const events = value
    .split(",")
    .map((event) => event.trim())
    .filter(Boolean);

  if (events.length === 0) {
    return null;
  }

  return new Set(events);
}

/** Check if event timestamp falls within the from/to date range */
function isEventWithinWindow(eventIsoTs: string, from: Date | null, to: Date | null): boolean {
  const ts = new Date(eventIsoTs);
  if (Number.isNaN(ts.getTime())) {
    return false;
  }
  if (from && ts < from) {
    return false;
  }
  if (to && ts > to) {
    return false;
  }
  return true;
}

/** Load all events from Firestore, filter by date/event query params */
async function getFilteredEvents(req: express.Request) {

  // Reads filtering parameters from the URL and parses them into usable formats 
  const from = parseDateParam(req.query.from);
  const to = parseDateParam(req.query.to);
  const eventFilter = parseEventFilter(req.query.event); // Type of event to filter by 

  // Get all events from Firestore instance and filter them based on the above from, to and eventFilter parameters
  const allEvents = await readEvents();
  const filtered = allEvents.filter((event) => {
    if (!isEventWithinWindow(event.ts_iso, from, to)) {
      return false;
    }

    if (eventFilter && !eventFilter.has(event.event)) {
      return false;
    }

    return true;
  });

  return {
    filtered,
    from,
    to,
    eventFilter
  };
}

/** Health check (used by Railway, load balancers, etc.) */
app.get("/health", (_req, res) => {
  res.json({ ok: true });
});

/** Receive event batches from the plugin. Validates with Zod, writes to Firestore */
app.post("/api/trackEvent", async (req, res) => {
  try {
    const parseResult = batchSchema.safeParse(req.body);

    if (!parseResult.success) {
      return res.status(400).json({
        ok: false,
        error: "Invalid payload",
        details: parseResult.error.flatten()
      });
    }

    await appendEvents(parseResult.data.events);
    return res.status(202).json({ ok: true, accepted: parseResult.data.events.length });
  } catch (error) {
    console.error("Failed to store events", error);
    return res.status(500).json({ ok: false, error: "Failed to store events" });
  }
});

/** Return filtered events for the dashboard. Dashboard aggregates client-side. */
app.get("/api/events", async (req, res) => {
  try {
    const { filtered, from, to, eventFilter } = await getFilteredEvents(req);

    return res.json({
      ok: true,
      window: {
        from: from?.toISOString() ?? null,
        to: to?.toISOString() ?? null,
        event: eventFilter ? Array.from(eventFilter.values()) : null
      },
      total_rows: filtered.length,
      rows: filtered
    });
  } catch (error) {
    console.error("Failed to fetch events", error);
    return res.status(500).json({ ok: false, error: "Failed to fetch events" });
  }
});

app.listen(port, () => {
  console.log(`Analytics API listening on http://localhost:${port}`);
});