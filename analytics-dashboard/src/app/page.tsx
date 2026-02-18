"use client";

import { useEffect, useMemo, useState } from "react";
import { Bar, BarChart, CartesianGrid, Line, LineChart, ResponsiveContainer, Tooltip, XAxis, YAxis } from "recharts";
import { DashboardShell } from "@/components/dashboard-shell";
import { KpiCard } from "@/components/kpi-card";
import { fetchEvents } from "@/lib/api";
import { getDailyUsage, getEventCounts, getGenerationMetrics, getUniqueUsers } from "@/lib/metrics";
import type { AnalyticsEvent } from "@/lib/types";
import { useDashboardFilters } from "@/lib/use-dashboard-filters";

export default function OverviewPage() {
  const [events, setEvents] = useState<AnalyticsEvent[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const filters = useDashboardFilters();

  useEffect(() => {
    let cancelled = false;
    setLoading(true);
    setError(null);

    fetchEvents(filters)
      .then((response) => {
        if (!cancelled) {
          setEvents(response.rows);
        }
      })
      .catch((err: unknown) => {
        if (!cancelled) {
          setError(err instanceof Error ? err.message : "Unknown error");
        }
      })
      .finally(() => {
        if (!cancelled) {
          setLoading(false);
        }
      });

    return () => {
      cancelled = true;
    };
  }, [filters.cohort, filters.event, filters.from, filters.to]);

  const uniqueUsers = useMemo(() => getUniqueUsers(events), [events]);
  const generation = useMemo(() => getGenerationMetrics(events), [events]);
  const dailyUsage = useMemo(() => getDailyUsage(events), [events]);
  const eventCountsRows = useMemo(
    () =>
      Object.entries(getEventCounts(events))
        .map(([event, count]) => ({ event, count }))
        .sort((a, b) => b.count - a.count),
    [events]
  );

  return (
    <DashboardShell title="Overview">
      {error ? (
        <div className="rounded border border-red-200 bg-red-50 p-4 text-sm text-red-700">{error}</div>
      ) : null}

      <div className="grid gap-4 md:grid-cols-2 xl:grid-cols-4">
        <KpiCard label="Active users" value={uniqueUsers} hint="Unique users in selected range" />
        <KpiCard label="Total generations" value={generation.totalGenerations} hint="generation_completed events" />
        <KpiCard label="Generation success rate" value={`${generation.successRatePct}%`} hint="completed / (completed + failed)" />
        <KpiCard label="Avg generation latency" value={`${generation.avgLatencyMs} ms`} />
      </div>

      <div className="grid gap-4 xl:grid-cols-2">
        <article className="h-80 rounded-xl border border-gray-200 bg-white p-4 shadow-sm">
          <h2 className="mb-3 text-base font-semibold text-gray-900">Daily active users</h2>
          {loading ? (
            <p className="text-sm text-gray-500">Loading...</p>
          ) : (
            <ResponsiveContainer width="100%" height="88%">
              <LineChart data={dailyUsage}>
                <CartesianGrid strokeDasharray="3 3" />
                <XAxis dataKey="day" />
                <YAxis allowDecimals={false} />
                <Tooltip />
                <Line type="monotone" dataKey="dau" stroke="#111827" strokeWidth={2} dot={false} />
              </LineChart>
            </ResponsiveContainer>
          )}
        </article>

        <article className="h-80 rounded-xl border border-gray-200 bg-white p-4 shadow-sm">
          <h2 className="mb-3 text-base font-semibold text-gray-900">Event counts</h2>
          {loading ? (
            <p className="text-sm text-gray-500">Loading...</p>
          ) : (
            <ResponsiveContainer width="100%" height="88%">
              <BarChart data={eventCountsRows}>
                <CartesianGrid strokeDasharray="3 3" />
                <XAxis dataKey="event" tick={{ fontSize: 12 }} interval={0} angle={-15} textAnchor="end" height={65} />
                <YAxis allowDecimals={false} />
                <Tooltip />
                <Bar dataKey="count" fill="#2563eb" />
              </BarChart>
            </ResponsiveContainer>
          )}
        </article>
      </div>
    </DashboardShell>
  );
}
