"use client";

import { useEffect, useState } from "react";
import { usePathname } from "next/navigation";
import { EVENT_FILTER_OPTIONS, filtersFromSearchParams, getDefaultFilters } from "@/lib/filters";
import type { DashboardFilters } from "@/lib/types";

export function FiltersBar() {
  const pathname = usePathname();
  const [draft, setDraft] = useState<DashboardFilters>(getDefaultFilters());
  const [baseline, setBaseline] = useState<DashboardFilters>(getDefaultFilters());

  useEffect(() => {
    const parsed = filtersFromSearchParams(new URLSearchParams(window.location.search));
    setDraft(parsed);
    setBaseline(parsed);
  }, [pathname]);

  const hasChanges =
    draft.from !== baseline.from ||
    draft.to !== baseline.to ||
    draft.event !== baseline.event;

  function applyFilters() {
    const nextParams = new URLSearchParams();
    if (draft.from) {
      nextParams.set("from", draft.from);
    }
    if (draft.to) {
      nextParams.set("to", draft.to);
    }
    if (draft.event && draft.event !== "all") {
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
    <div className="grid gap-3 rounded-lg border border-gray-200 bg-gray-50 p-3 md:grid-cols-4">
      <label className="text-xs text-gray-600">
        From
        <input
          type="date"
          value={draft.from}
          onChange={(event) => setDraft((prev) => ({ ...prev, from: event.target.value }))}
          className="mt-1 w-full rounded border border-gray-300 bg-white px-2 py-2 text-sm text-gray-900"
        />
      </label>
      <label className="text-xs text-gray-600">
        To
        <input
          type="date"
          value={draft.to}
          onChange={(event) => setDraft((prev) => ({ ...prev, to: event.target.value }))}
          className="mt-1 w-full rounded border border-gray-300 bg-white px-2 py-2 text-sm text-gray-900"
        />
      </label>
      <label className="text-xs text-gray-600">
        Event type
        <select
          value={draft.event}
          onChange={(event) => setDraft((prev) => ({ ...prev, event: event.target.value }))}
          className="mt-1 w-full rounded border border-gray-300 bg-white px-2 py-2 text-sm text-gray-900"
        >
          {EVENT_FILTER_OPTIONS.map((option) => (
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
          className="rounded bg-gray-900 px-3 py-2 text-sm text-white disabled:cursor-not-allowed disabled:bg-gray-400"
        >
          Apply
        </button>
        <button
          type="button"
          onClick={resetFilters}
          className="rounded border border-gray-300 bg-white px-3 py-2 text-sm text-gray-700 hover:bg-gray-100"
        >
          Reset
        </button>
      </div>
    </div>
  );
}
