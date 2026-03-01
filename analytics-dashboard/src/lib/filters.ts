import type { DashboardFilters } from "@/lib/types";

/** Converts snake_case event name to "Firstword Secondword" for display. */
export function formatEventDisplayName(eventKey: string): string {
  if (!eventKey || eventKey === "all") return eventKey;
  return eventKey
    .split("_")
    .map((word) => word.charAt(0).toUpperCase() + word.slice(1).toLowerCase())
    .join(" ");
}

export const EVENT_FILTER_OPTIONS = [
  { value: "all", label: "All events" },
  { value: "prompt_submitted", label: formatEventDisplayName("prompt_submitted") },
  { value: "generation_completed", label: formatEventDisplayName("generation_completed") },
  { value: "generation_failed", label: formatEventDisplayName("generation_failed") },
  { value: "midi_dragged", label: formatEventDisplayName("midi_dragged") }
] as const;

function toDateInputString(value: Date): string {
  const year = value.getFullYear();
  const month = String(value.getMonth() + 1).padStart(2, "0");
  const day = String(value.getDate()).padStart(2, "0");
  return `${year}-${month}-${day}`;
}

export function getDefaultFilters(): DashboardFilters {
  const today = new Date();

  return {
    from: toDateInputString(today),
    to: toDateInputString(today),
    event: "all"
  };
}

export function filtersFromSearchParams(searchParams: URLSearchParams): DashboardFilters {
  const defaults = getDefaultFilters();

  return {
    from: searchParams.get("from") ?? defaults.from,
    to: searchParams.get("to") ?? defaults.to,
    event: searchParams.get("event") ?? defaults.event
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
  return params;
}
