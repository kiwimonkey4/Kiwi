import type { AnalyticsEvent, DashboardFilters } from "@/lib/types";

function getDay(isoTs: string): string {
  return isoTs.slice(0, 10);
}

function parseFilterRange(filters: DashboardFilters): { start: Date; end: Date } {
  const start = new Date(`${filters.from}T00:00:00.000Z`);
  const end = new Date(`${filters.to}T23:59:59.999Z`);
  return { start, end };
}

export function getUniqueUsers(events: AnalyticsEvent[]): number {
  return new Set(events.map((event) => event.user_id)).size;
}

export function getEventCounts(events: AnalyticsEvent[]): Record<string, number> {
  return events.reduce<Record<string, number>>((acc, event) => {
    acc[event.event] = (acc[event.event] ?? 0) + 1;
    return acc;
  }, {});
}

export function getGenerationMetrics(events: AnalyticsEvent[]): {
  totalGenerations: number;
  successRatePct: number;
  avgLatencyMs: number;
} {
  let completed = 0;
  let failed = 0;
  let latencySum = 0;
  let latencyCount = 0;

  for (const event of events) {
    if (event.event === "generation_completed") {
      completed += 1;
      const latency = Number(event.props?.latency_ms ?? 0);
      if (Number.isFinite(latency) && latency > 0) {
        latencySum += latency;
        latencyCount += 1;
      }
    } else if (event.event === "generation_failed") {
      failed += 1;
    }
  }

  const totalAttempts = completed + failed;
  const successRatePct = totalAttempts > 0 ? Math.round((completed / totalAttempts) * 1000) / 10 : 0;
  const avgLatencyMs = latencyCount > 0 ? Math.round(latencySum / latencyCount) : 0;

  return {
    totalGenerations: completed,
    successRatePct,
    avgLatencyMs
  };
}

export function getDailyUsage(events: AnalyticsEvent[]): Array<{ day: string; dau: number; events: number }> {
  const byDay = new Map<string, { users: Set<string>; events: number }>();

  for (const event of events) {
    const day = getDay(event.ts_iso);
    const existing = byDay.get(day) ?? { users: new Set<string>(), events: 0 };
    existing.users.add(event.user_id);
    existing.events += 1;
    byDay.set(day, existing);
  }

  return Array.from(byDay.entries())
    .map(([day, metrics]) => ({ day, dau: metrics.users.size, events: metrics.events }))
    .sort((a, b) => a.day.localeCompare(b.day));
}

export function getFunnel(events: AnalyticsEvent[]): Array<{ step: string; count: number; pctFromPrevious: number; pctFromStart: number }> {
  const eventCounts = getEventCounts(events);
  const steps = ["prompt_submitted", "generation_completed", "midi_dragged"] as const;

  return steps.map((step, index) => {
    const count = eventCounts[step] ?? 0;
    const previous = index === 0 ? count : eventCounts[steps[index - 1]] ?? 0;
    const start = eventCounts[steps[0]] ?? 0;

    return {
      step,
      count,
      pctFromPrevious: index === 0 || previous === 0 ? 100 : Math.round((count / previous) * 1000) / 10,
      pctFromStart: start === 0 ? 0 : Math.round((count / start) * 1000) / 10
    };
  });
}

export function getFeatureAdoption(events: AnalyticsEvent[]): Array<{ feature: string; users: number; adoptionPct: number }> {
  const usersByFeature = new Map<string, Set<string>>();
  const allUsers = new Set<string>();
  const trackedFeatures = ["prompt_submitted", "generation_completed", "midi_dragged", "midi_replayed"];

  for (const feature of trackedFeatures) {
    usersByFeature.set(feature, new Set<string>());
  }

  for (const event of events) {
    allUsers.add(event.user_id);
    const users = usersByFeature.get(event.event);
    if (users) {
      users.add(event.user_id);
    }
  }

  const totalUsers = allUsers.size || 1;
  return trackedFeatures.map((feature) => {
    const count = usersByFeature.get(feature)?.size ?? 0;
    return {
      feature,
      users: count,
      adoptionPct: Math.round((count / totalUsers) * 1000) / 10
    };
  });
}

export function getPromptHashBreakdown(events: AnalyticsEvent[]): Array<{ hash: string; count: number }> {
  const counts = new Map<string, number>();

  for (const event of events) {
    if (event.event !== "prompt_submitted") {
      continue;
    }
    const hash = String(event.props?.prompt_hash64 ?? "unknown");
    counts.set(hash, (counts.get(hash) ?? 0) + 1);
  }

  return Array.from(counts.entries())
    .map(([hash, count]) => ({ hash, count }))
    .sort((a, b) => b.count - a.count)
    .slice(0, 12);
}

export function getPromptLengthBuckets(events: AnalyticsEvent[]): Array<{ bucket: string; count: number }> {
  const buckets = new Map<string, number>([
    ["0-20", 0],
    ["21-40", 0],
    ["41-80", 0],
    ["81+", 0]
  ]);

  for (const event of events) {
    if (event.event !== "prompt_submitted") {
      continue;
    }
    const length = Number(event.props?.prompt_length ?? 0);
    if (!Number.isFinite(length) || length <= 0) {
      continue;
    }
    if (length <= 20) {
      buckets.set("0-20", (buckets.get("0-20") ?? 0) + 1);
    } else if (length <= 40) {
      buckets.set("21-40", (buckets.get("21-40") ?? 0) + 1);
    } else if (length <= 80) {
      buckets.set("41-80", (buckets.get("41-80") ?? 0) + 1);
    } else {
      buckets.set("81+", (buckets.get("81+") ?? 0) + 1);
    }
  }

  return Array.from(buckets.entries()).map(([bucket, count]) => ({ bucket, count }));
}

export function getCohortSummary(
  events: AnalyticsEvent[],
  firstSeenByUser: Record<string, string>,
  filters: DashboardFilters
): { totalUsers: number; newUsers: number; returningUsers: number } {
  const range = parseFilterRange(filters);
  const usersInWindow = new Set(events.map((event) => event.user_id));

  let newUsers = 0;
  let returningUsers = 0;

  for (const userId of usersInWindow) {
    const firstSeenIso = firstSeenByUser[userId];
    if (!firstSeenIso) {
      continue;
    }

    const firstSeen = new Date(firstSeenIso);
    const isNew = firstSeen >= range.start && firstSeen <= range.end;
    if (isNew) {
      newUsers += 1;
    } else {
      returningUsers += 1;
    }
  }

  return {
    totalUsers: usersInWindow.size,
    newUsers,
    returningUsers
  };
}

export function getUserRows(
  events: AnalyticsEvent[],
  firstSeenByUser: Record<string, string>
): Array<{
  userId: string;
  firstSeen: string;
  events: number;
  prompts: number;
  generations: number;
  drags: number;
  replays: number;
}> {
  const rows = new Map<
    string,
    { events: number; prompts: number; generations: number; drags: number; replays: number }
  >();

  for (const event of events) {
    const userRow = rows.get(event.user_id) ?? { events: 0, prompts: 0, generations: 0, drags: 0, replays: 0 };
    userRow.events += 1;
    if (event.event === "prompt_submitted") {
      userRow.prompts += 1;
    } else if (event.event === "generation_completed") {
      userRow.generations += 1;
    } else if (event.event === "midi_dragged") {
      userRow.drags += 1;
    } else if (event.event === "midi_replayed") {
      userRow.replays += 1;
    }
    rows.set(event.user_id, userRow);
  }

  return Array.from(rows.entries())
    .map(([userId, row]) => ({
      userId,
      firstSeen: firstSeenByUser[userId]?.slice(0, 10) ?? "-",
      ...row
    }))
    .sort((a, b) => b.events - a.events);
}
