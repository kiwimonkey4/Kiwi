"use client";

import { useEffect, useMemo, useState } from "react";
import { Bar, BarChart, CartesianGrid, LabelList, ResponsiveContainer, Tooltip, XAxis, YAxis } from "recharts";
import { DashboardShell } from "@/components/dashboard-shell";
import { KpiCard } from "@/components/kpi-card";
import { fetchEvents } from "@/lib/api";
import { getFunnel } from "@/lib/metrics";
import type { AnalyticsEvent } from "@/lib/types";
import { useDashboardFilters } from "@/lib/use-dashboard-filters";

export default function FunnelPage() {
  const filters = useDashboardFilters();
  const [events, setEvents] = useState<AnalyticsEvent[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

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

  const funnelRows = useMemo(() => getFunnel(events), [events]);
  const dropOffPct = useMemo(() => {
    if (funnelRows.length < 2) {
      return 0;
    }
    const start = funnelRows[0].count;
    const end = funnelRows[funnelRows.length - 1].count;
    if (start === 0) {
      return 0;
    }
    return Math.round(((start - end) / start) * 1000) / 10;
  }, [funnelRows]);

  return (
    <DashboardShell title="Conversion funnel">
      {error ? (
        <div className="rounded border border-red-200 bg-red-50 p-4 text-sm text-red-700">{error}</div>
      ) : null}

      <div className="grid gap-4 md:grid-cols-3">
        <KpiCard label="Start events" value={funnelRows[0]?.count ?? 0} hint="editor_opened" />
        <KpiCard label="Final conversion" value={`${funnelRows[funnelRows.length - 1]?.pctFromStart ?? 0}%`} hint="midi_dragged from start" />
        <KpiCard label="Drop-off" value={`${dropOffPct}%`} hint="From first to final step" />
      </div>

      <article className="h-96 rounded-xl border border-gray-200 bg-white p-4 shadow-sm">
        <h2 className="mb-3 text-base font-semibold text-gray-900">Funnel steps</h2>
        {loading ? (
          <p className="text-sm text-gray-500">Loading...</p>
        ) : (
          <ResponsiveContainer width="100%" height="84%">
            <BarChart data={funnelRows} layout="vertical" margin={{ left: 35, right: 30 }}>
              <CartesianGrid strokeDasharray="3 3" />
              <XAxis type="number" allowDecimals={false} />
              <YAxis type="category" dataKey="step" width={170} />
              <Tooltip />
              <Bar dataKey="count" fill="#f97316">
                <LabelList dataKey="pctFromStart" position="right" formatter={(value: number) => `${value}%`} />
              </Bar>
            </BarChart>
          </ResponsiveContainer>
        )}
      </article>

      <article className="rounded-xl border border-gray-200 bg-white p-4 shadow-sm">
        <h2 className="mb-3 text-base font-semibold text-gray-900">Step conversions</h2>
        <div className="overflow-x-auto">
          <table className="min-w-full text-left text-sm">
            <thead className="border-b border-gray-200 bg-gray-50 text-gray-600">
              <tr>
                <th className="px-3 py-2 font-medium">Step</th>
                <th className="px-3 py-2 font-medium">Count</th>
                <th className="px-3 py-2 font-medium">From previous</th>
                <th className="px-3 py-2 font-medium">From start</th>
              </tr>
            </thead>
            <tbody>
              {funnelRows.map((row) => (
                <tr key={row.step} className="border-b border-gray-100 text-gray-700">
                  <td className="px-3 py-2">{row.step}</td>
                  <td className="px-3 py-2">{row.count}</td>
                  <td className="px-3 py-2">{row.pctFromPrevious}%</td>
                  <td className="px-3 py-2">{row.pctFromStart}%</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      </article>
    </DashboardShell>
  );
}
