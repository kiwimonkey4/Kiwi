import type { DashboardFilters } from "@/lib/types";

export const EVENT_FILTER_OPTIONS = [
  { value: "all", label: "All events" },
  { value: "editor_opened", label: "editor_opened" },
  { value: "prompt_submitted", label: "prompt_submitted" },
  { value: "generation_completed", label: "generation_completed" },
  { value: "generation_failed", label: "generation_failed" },
  { value: "midi_dragged", label: "midi_dragged" },
  { value: "midi_replayed", label: "midi_replayed" }
] as const;

export const COHORT_OPTIONS = [
  { value: "all", label: "All users" },
  { value: "new", label: "New users" },
  { value: "returning", label: "Returning users" }
] as const;

function toDateInputString(value: Date): string {
  const year = value.getFullYear();
  const month = String(value.getMonth() + 1).padStart(2, "0");
  const day = String(value.getDate()).padStart(2, "0");
  return `${year}-${month}-${day}`;
}

export function getDefaultFilters(): DashboardFilters {
  const to = new Date();
  const from = new Date();
  from.setDate(to.getDate() - 14);

  return {
    from: toDateInputString(from),
    to: toDateInputString(to),
    event: "all",
    cohort: "all"
  };
}

export function filtersFromSearchParams(searchParams: URLSearchParams): DashboardFilters {
  const defaults = getDefaultFilters();
  const event = searchParams.get("event") ?? defaults.event;
  const cohort = searchParams.get("cohort");

  return {
    from: searchParams.get("from") ?? defaults.from,
    to: searchParams.get("to") ?? defaults.to,
    event,
    cohort: cohort === "new" || cohort === "returning" ? cohort : defaults.cohort
  };
}

export function toQueryParams(filters: DashboardFilters): URLSearchParams {
  const params = new URLSearchParams();
  if (filters.from) {
    params.set("from", `${filters.from}T00:00:00.000Z`);
  }
  if (filters.to) {
    params.set("to", `${filters.to}T23:59:59.999Z`);
  }
  if (filters.event && filters.event !== "all") {
    params.set("event", filters.event);
  }
  if (filters.cohort && filters.cohort !== "all") {
    params.set("cohort", filters.cohort);
  }
  return params;
}
