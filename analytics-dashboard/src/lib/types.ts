export type CohortFilter = "all" | "new" | "returning";

export type AnalyticsEventName =
  | "editor_opened"
  | "prompt_submitted"
  | "generation_completed"
  | "generation_failed"
  | "midi_dragged"
  | "midi_replayed"
  | string;

export interface AnalyticsEvent {
  event: AnalyticsEventName;
  ts_iso: string;
  user_id: string;
  session_id: string;
  app: string;
  app_version: string;
  props?: Record<string, unknown>;
}

export interface DashboardFilters {
  from: string;
  to: string;
  event: string;
  cohort: CohortFilter;
}

export interface EventsApiResponse {
  ok: boolean;
  rows: AnalyticsEvent[];
  first_seen_by_user: Record<string, string>;
}
