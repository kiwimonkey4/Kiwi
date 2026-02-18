"use client";

import { useEffect, useMemo, useState } from "react";
import { Bar, BarChart, CartesianGrid, ResponsiveContainer, Tooltip, XAxis, YAxis } from "recharts";
import { DashboardShell } from "@/components/dashboard-shell";
import { KpiCard } from "@/components/kpi-card";
import { fetchEvents } from "@/lib/api";
import { formatEventDisplayName } from "@/lib/filters";
import { getEventCounts, getGenerationMetrics } from "@/lib/metrics";
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

  const generation = useMemo(() => getGenerationMetrics(events), [events]);
  const EXCLUDED_EVENTS = ["editor_opened", "midi_replayed"];
  const eventCountsRows = useMemo(
    () =>
      Object.entries(getEventCounts(events))
        .filter(([event]) => !EXCLUDED_EVENTS.includes(event))
        .map(([event, count]) => ({ event, count, label: formatEventDisplayName(event) }))
        .sort((a, b) => b.count - a.count),
    [events]
  );

  return (
    <DashboardShell title="Overview">
      {error ? (
        <div className="rounded border border-red-200 bg-red-50 p-4 text-sm text-red-700">{error}</div>
      ) : null}

      <div className="grid gap-4 md:grid-cols-3">
        <KpiCard 
          label="Total generations" 
          value={generation.totalGenerations}
          info="Total number of successful generations"
        />
        <KpiCard 
          label="Generation success rate" 
          value={`${generation.successRatePct}%`}
          info="Ratio of successful generations to total number of generations attempted (i.e number of prompts submitted)"
        />
        <KpiCard 
          label="Avg generation latency" 
          value={`${generation.avgLatencyMs} ms`}
          info="Average time taken from prompt submission to receipt of generated MIDI stem"
        />
      </div>

      <article className="h-80 rounded-xl border border-kiwi-green-200 bg-kiwi-brown-50 p-4 shadow-sm backdrop-blur-sm">
        <h2 className="mb-3 text-sm uppercase tracking-wider font-bold text-kiwi-green-800">Event counts</h2>
        {loading ? (
          <p className="text-sm text-kiwi-brown-500">Loading...</p>
        ) : (
          <ResponsiveContainer width="100%" height="88%">
            <BarChart data={eventCountsRows}>
              <CartesianGrid strokeDasharray="3 3" stroke="#bae5cd" />
              <XAxis dataKey="label" tick={{ fontSize: 12, fill: "#614938" }} interval={0} angle={-15} textAnchor="end" height={65} />
              <YAxis allowDecimals={false} tick={{ fill: "#614938" }} />
              <Tooltip />
              <Bar dataKey="count" fill="#8f6e4d" />
            </BarChart>
          </ResponsiveContainer>
        )}
      </article>
    </DashboardShell>
  );
}
