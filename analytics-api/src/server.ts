import express from "express";
import cors from "cors";
import { batchSchema, type AnalyticsEvent } from "./types.js";
import { appendEvents, readEvents } from "./store.js";

const app = express();
const port = Number(process.env.PORT ?? 8787);
type Cohort = "all" | "new" | "returning";

app.use(cors());
app.use(express.json({ limit: "1mb" }));

function parseDateParam(value: unknown): Date | null {
  if (typeof value !== "string" || value.trim().length === 0) {
    return null;
  }

  const parsed = new Date(value);
  return Number.isNaN(parsed.getTime()) ? null : parsed;
}

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

function parseCohort(value: unknown): Cohort {
  if (value === "new" || value === "returning") {
    return value;
  }
  return "all";
}

function buildFirstSeenByUser(events: AnalyticsEvent[]) {
  const firstSeen = new Map<string, Date>();

  for (const event of events) {
    const ts = new Date(event.ts_iso);
    if (Number.isNaN(ts.getTime())) {
      continue;
    }

    const existing = firstSeen.get(event.user_id);
    if (!existing || ts < existing) {
      firstSeen.set(event.user_id, ts);
    }
  }

  return firstSeen;
}

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

function isUserInCohort(
  userId: string,
  firstSeenByUser: Map<string, Date>,
  cohort: Cohort,
  from: Date | null,
  to: Date | null
): boolean {
  if (cohort === "all") {
    return true;
  }

  const firstSeen = firstSeenByUser.get(userId);
  if (!firstSeen) {
    return false;
  }

  const rangeStart = from ?? new Date(0);
  const rangeEnd = to ?? new Date(8640000000000000);
  const isNew = firstSeen >= rangeStart && firstSeen <= rangeEnd;

  return cohort === "new" ? isNew : !isNew;
}

async function getFilteredEvents(req: express.Request) {
  const from = parseDateParam(req.query.from);
  const to = parseDateParam(req.query.to);
  const eventFilter = parseEventFilter(req.query.event);
  const cohort = parseCohort(req.query.cohort);

  const allEvents = await readEvents();
  const firstSeenByUser = buildFirstSeenByUser(allEvents);
  const filtered = allEvents.filter((event) => {
    if (!isEventWithinWindow(event.ts_iso, from, to)) {
      return false;
    }

    if (eventFilter && !eventFilter.has(event.event)) {
      return false;
    }

    return isUserInCohort(event.user_id, firstSeenByUser, cohort, from, to);
  });

  return {
    filtered,
    from,
    to,
    cohort,
    eventFilter,
    firstSeenByUser
  };
}

app.get("/health", (_req, res) => {
  res.json({ ok: true });
});

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

app.get("/api/metrics/overview", async (req, res) => {
  try {
    const { filtered: events, from, to, cohort, eventFilter } = await getFilteredEvents(req);

    const uniqueUsers = new Set(events.map((event) => event.user_id)).size;
    const eventCounts = events.reduce<Record<string, number>>((acc, event) => {
      acc[event.event] = (acc[event.event] ?? 0) + 1;
      return acc;
    }, {});

    const opened = eventCounts.editor_opened ?? 0;
    const promptSubmitted = eventCounts.prompt_submitted ?? 0;
    const generated = eventCounts.generation_completed ?? 0;
    const dragged = eventCounts.midi_dragged ?? 0;

    const avgLatency = (() => {
      const values = events
        .filter((event) => event.event === "generation_completed")
        .map((event) => Number(event.props?.latency_ms ?? 0))
        .filter((value) => Number.isFinite(value) && value > 0);

      if (values.length === 0) {
        return 0;
      }

      return Math.round(values.reduce((sum, value) => sum + value, 0) / values.length);
    })();

    return res.json({
      ok: true,
      window: {
        from: from?.toISOString() ?? null,
        to: to?.toISOString() ?? null,
        cohort,
        event: eventFilter ? Array.from(eventFilter.values()) : null
      },
      totals: {
        users: uniqueUsers,
        events: events.length,
        avg_generation_latency_ms: avgLatency
      },
      funnel: {
        editor_opened: opened,
        prompt_submitted: promptSubmitted,
        generation_completed: generated,
        midi_dragged: dragged
      },
      event_counts: eventCounts
    });
  } catch (error) {
    console.error("Failed to compute overview metrics", error);
    return res.status(500).json({ ok: false, error: "Failed to compute overview metrics" });
  }
});

app.get("/api/metrics/daily", async (req, res) => {
  try {
    const { filtered: events, from, to, cohort, eventFilter } = await getFilteredEvents(req);
    const byDay = events.reduce<Record<string, { events: number; users: Set<string> }>>((acc, event) => {
      const day = event.ts_iso.slice(0, 10);
      if (!acc[day]) {
        acc[day] = { events: 0, users: new Set<string>() };
      }
      acc[day].events += 1;
      acc[day].users.add(event.user_id);
      return acc;
    }, {});

    const rows = Object.entries(byDay)
      .sort(([a], [b]) => a.localeCompare(b))
      .map(([day, dayData]) => ({ day, events: dayData.events, dau: dayData.users.size }));

    return res.json({
      ok: true,
      window: {
        from: from?.toISOString() ?? null,
        to: to?.toISOString() ?? null,
        cohort,
        event: eventFilter ? Array.from(eventFilter.values()) : null
      },
      rows
    });
  } catch (error) {
    console.error("Failed to compute daily metrics", error);
    return res.status(500).json({ ok: false, error: "Failed to compute daily metrics" });
  }
});

app.get("/api/events", async (req, res) => {
  try {
    const { filtered, from, to, cohort, eventFilter, firstSeenByUser } = await getFilteredEvents(req);
    const firstSeenByUserJson = Object.fromEntries(
      Array.from(firstSeenByUser.entries()).map(([userId, ts]) => [userId, ts.toISOString()])
    );

    return res.json({
      ok: true,
      window: {
        from: from?.toISOString() ?? null,
        to: to?.toISOString() ?? null,
        cohort,
        event: eventFilter ? Array.from(eventFilter.values()) : null
      },
      total_rows: filtered.length,
      first_seen_by_user: firstSeenByUserJson,
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
