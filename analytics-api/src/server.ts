import express from "express";
import cors from "cors";
import { batchSchema } from "./types.js";
import { appendEvents, readEvents } from "./store.js";

const app = express();
const port = Number(process.env.PORT ?? 8787);

app.use(cors());
app.use(express.json({ limit: "1mb" }));

app.get("/health", (_req, res) => {
  res.json({ ok: true });
});

app.post("/api/trackEvent", (req, res) => {
  const parseResult = batchSchema.safeParse(req.body);

  if (!parseResult.success) {
    return res.status(400).json({
      ok: false,
      error: "Invalid payload",
      details: parseResult.error.flatten()
    });
  }

  appendEvents(parseResult.data.events);
  return res.status(202).json({ ok: true, accepted: parseResult.data.events.length });
});

app.get("/api/metrics/overview", (req, res) => {
  const from = typeof req.query.from === "string" ? new Date(req.query.from) : null;
  const to = typeof req.query.to === "string" ? new Date(req.query.to) : null;

  const events = readEvents().filter((event) => {
    const ts = new Date(event.ts_iso);
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
  });

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
      to: to?.toISOString() ?? null
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
});

app.get("/api/metrics/daily", (_req, res) => {
  const events = readEvents();
  const byDay = events.reduce<Record<string, number>>((acc, event) => {
    const day = event.ts_iso.slice(0, 10);
    acc[day] = (acc[day] ?? 0) + 1;
    return acc;
  }, {});

  const rows = Object.entries(byDay)
    .sort(([a], [b]) => a.localeCompare(b))
    .map(([day, count]) => ({ day, events: count }));

  return res.json({ ok: true, rows });
});

app.listen(port, () => {
  console.log(`Analytics API listening on http://localhost:${port}`);
});
