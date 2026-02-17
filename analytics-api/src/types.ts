import { z } from "zod";

export const eventSchema = z.object({
  event: z.string().min(1),
  ts_iso: z.string().min(1),
  user_id: z.string().min(1),
  session_id: z.string().min(1),
  app: z.string().min(1),
  app_version: z.string().min(1),
  props: z.record(z.any()).optional()
});

export const batchSchema = z.object({
  source: z.string().optional(),
  app: z.string().optional(),
  app_version: z.string().optional(),
  events: z.array(eventSchema).min(1)
});

export type AnalyticsEvent = z.infer<typeof eventSchema>;
