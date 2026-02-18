"use client";

import { useEffect, useMemo, useState } from "react";
import { Bar, BarChart, CartesianGrid, LabelList, ResponsiveContainer, Tooltip, XAxis, YAxis } from "recharts";
import { DashboardShell } from "@/components/dashboard-shell";
import { KpiCard } from "@/components/kpi-card";
import { fetchEvents } from "@/lib/api";
import { formatEventDisplayName } from "@/lib/filters";
import { getFunnel } from "@/lib/metrics";
import type { AnalyticsEvent } from "@/lib/types";
import { useDashboardFilters } from "@/lib/use-dashboard-filters";

export default function FunnelPage() {
  const filters = useDashboardFilters();
  const funnelFilters = useMemo(
    () => ({ ...filters, event: "all" as const }),
    [filters]
  );
  const [events, setEvents] = useState<AnalyticsEvent[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    let cancelled = false;
    setLoading(true);
    setError(null);

    fetchEvents(funnelFilters)
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
  }, [funnelFilters]);

  const funnelRows = useMemo(() => getFunnel(events), [events]);
  const funnelRowsWithLabels = useMemo(
    () => funnelRows.map((row) => ({ ...row, stepLabel: formatEventDisplayName(row.step) })),
    [funnelRows]
  );
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
    <DashboardShell 
      title="Conversion Data"
      titleInfo="Conversion in this case refers to the percent of the user's prompts that are translated into the event of dropping the generated MIDI file into their DAW. This metric gives insight into the plugin's effectiveness, as a user will likely only accept the MIDI stem if they are satisfied with the quality of the generated response"
    >
      {error ? (
        <div className="rounded border border-red-200 bg-red-50 p-4 text-sm text-red-700">{error}</div>
      ) : null}

      <div className="grid gap-4 md:grid-cols-3">
        <KpiCard 
          label="Start events" 
          value={funnelRows[0]?.count ?? 0}
          info="Number of prompts submitted. A prompt submission is the first step in the conversion pipeline"
        />
        <KpiCard 
          label="Final conversion" 
          value={`${funnelRows[funnelRows.length - 1]?.pctFromStart ?? 0}%`}
          info="Ratio of prompt submissions that lead to MIDI stems being accepted to the total number of prompt submissions"
        />
        <KpiCard 
          label="Drop-off" 
          value={`${dropOffPct}%`}
          info="Ratio of prompt submissions that do not lead to MIDI stem acceptances to total number of prompt submissions"
        />
      </div>

      <article className="h-96 rounded-xl border border-kiwi-green-200 bg-kiwi-brown-50 p-4 shadow-sm backdrop-blur-sm">
        <h2 className="mb-3 text-sm uppercase tracking-wider font-bold text-kiwi-green-800">Funnel steps</h2>
        {loading ? (
          <p className="text-sm text-kiwi-brown-500">Loading...</p>
        ) : (
          <ResponsiveContainer width="100%" height="84%">
            <BarChart data={funnelRowsWithLabels} layout="vertical" margin={{ left: 35, right: 30 }}>
              <CartesianGrid strokeDasharray="3 3" stroke="#bae5cd" />
              <XAxis type="number" allowDecimals={false} tick={{ fill: "#614938" }} />
              <YAxis type="category" dataKey="stepLabel" width={170} tick={{ fill: "#614938" }} />
              <Tooltip />
              <Bar dataKey="count" fill="#3a9f6a">
                <LabelList dataKey="pctFromStart" position="right" formatter={(value: number) => `${value}%`} fill="#614938" />
              </Bar>
            </BarChart>
          </ResponsiveContainer>
        )}
      </article>

      <article className="rounded-xl border border-kiwi-green-200 bg-kiwi-brown-50 p-4 shadow-sm backdrop-blur-sm">
        <h2 className="mb-3 text-sm uppercase tracking-wider font-bold text-kiwi-green-800">Step conversions</h2>
        <div className="overflow-x-auto">
          <table className="min-w-full text-left text-sm">
            <thead className="border-b border-kiwi-green-200 bg-kiwi-green-50 text-kiwi-brown-600">
              <tr>
                <th className="px-3 py-2 font-medium">Step</th>
                <th className="px-3 py-2 font-medium">Count</th>
                <th className="px-3 py-2 font-medium">From previous</th>
                <th className="px-3 py-2 font-medium">From start</th>
              </tr>
            </thead>
            <tbody>
              {funnelRows.map((row) => (
                <tr key={row.step} className="border-b border-kiwi-green-100 text-kiwi-brown-700">
                  <td className="px-3 py-2">{formatEventDisplayName(row.step)}</td>
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
