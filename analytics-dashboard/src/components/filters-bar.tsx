"use client";

import { useEffect, useState } from "react";
import { usePathname } from "next/navigation";
import { EVENT_FILTER_OPTIONS, filtersFromSearchParams, getDefaultFilters } from "@/lib/filters";
import type { DashboardFilters } from "@/lib/types";

export function FiltersBar() {
  const pathname = usePathname();
  const isFunnelPage = pathname === "/funnel";
  const eventOptions = isFunnelPage ? EVENT_FILTER_OPTIONS.filter((option) => option.value === "all") : EVENT_FILTER_OPTIONS;
  const [draft, setDraft] = useState<DashboardFilters>(getDefaultFilters());
  const [baseline, setBaseline] = useState<DashboardFilters>(getDefaultFilters());

  useEffect(() => {
    const parsed = filtersFromSearchParams(new URLSearchParams(window.location.search));
    if (isFunnelPage) {
      parsed.event = "all";
    }
    setDraft(parsed);
    setBaseline(parsed);
  }, [isFunnelPage, pathname]);

  const hasChanges =
    draft.from !== baseline.from ||
    draft.to !== baseline.to ||
    (!isFunnelPage && draft.event !== baseline.event);

  function applyFilters() {
    const normalizedTo =
      draft.from && draft.to && draft.to < draft.from
        ? draft.from
        : draft.to;

    const nextParams = new URLSearchParams();
    if (draft.from) {
      nextParams.set("from", draft.from);
    }
    if (normalizedTo) {
      nextParams.set("to", normalizedTo);
    }
    if (!isFunnelPage && draft.event && draft.event !== "all") {
      nextParams.set("event", draft.event);
    }
    const qs = nextParams.toString();
    window.location.assign(qs ? `${pathname}?${qs}` : pathname);
  }

  function resetFilters() {
    const defaults = getDefaultFilters();
    setDraft(defaults);
    window.location.assign(pathname);
  }

  return (
    <div className="grid gap-3 rounded-lg border border-kiwi-green-200 bg-kiwi-green-50/90 p-3 md:grid-cols-4 backdrop-blur-sm">
      <label className="text-xs uppercase tracking-wider font-semibold text-kiwi-brown-600">
        From
        <input
          type="date"
          value={draft.from}
          max={draft.to || undefined}
          onChange={(event) =>
            setDraft((prev) => {
              const nextFrom = event.target.value;
              const nextTo = prev.to && nextFrom && prev.to < nextFrom ? nextFrom : prev.to;
              return { ...prev, from: nextFrom, to: nextTo };
            })
          }
          className="mt-1 w-full rounded border border-kiwi-green-300 bg-kiwi-brown-50 px-2 py-2 text-sm text-kiwi-green-900"
        />
      </label>
      <label className="text-xs uppercase tracking-wider font-semibold text-kiwi-brown-600">
        To
        <input
          type="date"
          value={draft.to}
          min={draft.from || undefined}
          onChange={(event) =>
            setDraft((prev) => {
              const nextTo = event.target.value;
              if (prev.from && nextTo && nextTo < prev.from) {
                return { ...prev, to: prev.from };
              }
              return { ...prev, to: nextTo };
            })
          }
          className="mt-1 w-full rounded border border-kiwi-green-300 bg-kiwi-brown-50 px-2 py-2 text-sm text-kiwi-green-900"
        />
      </label>
      <label className="text-xs uppercase tracking-wider font-semibold text-kiwi-brown-600">
        Event type
        <select
          value={draft.event}
          onChange={(event) => setDraft((prev) => ({ ...prev, event: event.target.value }))}
          disabled={isFunnelPage}
          className="mt-1 w-full rounded border border-kiwi-green-300 bg-kiwi-brown-50 px-2 py-2 text-sm text-kiwi-green-900"
        >
          {eventOptions.map((option) => (
            <option key={option.value} value={option.value}>
              {option.label}
            </option>
          ))}
        </select>
      </label>
      <div className="flex items-end gap-2">
        <button
          type="button"
          onClick={applyFilters}
          disabled={!hasChanges}
          className="rounded bg-kiwi-green-600 px-3 py-2 text-xs uppercase tracking-wider font-bold text-white hover:bg-kiwi-green-700 disabled:cursor-not-allowed disabled:bg-kiwi-green-300"
        >
          Apply
        </button>
        <button
          type="button"
          onClick={resetFilters}
          className="rounded border border-kiwi-brown-300 bg-kiwi-brown-50 px-3 py-2 text-xs uppercase tracking-wider font-bold text-kiwi-brown-700 hover:bg-kiwi-brown-100"
        >
          Reset
        </button>
      </div>
    </div>
  );
}
