import type { DashboardFilters, EventsApiResponse } from "@/lib/types";
import { toQueryParams } from "@/lib/filters";

const ANALYTICS_API_BASE_URL = process.env.NEXT_PUBLIC_ANALYTICS_API_URL ?? "http://127.0.0.1:8787";

export async function fetchEvents(filters: DashboardFilters): Promise<EventsApiResponse> {
  const query = toQueryParams(filters).toString();
  const url = `${ANALYTICS_API_BASE_URL}/api/events${query ? `?${query}` : ""}`;

  const response = await fetch(url, {
    method: "GET",
    cache: "no-store"
  });

  if (!response.ok) {
    throw new Error(`Failed to fetch events (${response.status})`);
  }

  return (await response.json()) as EventsApiResponse;
}
