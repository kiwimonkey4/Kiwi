# Kiwi Analytics Dashboard

Next.js dashboard for Kiwi plugin analytics.

## Run locally

1. Start the analytics API:

```bash
cd ../analytics-api
npm run dev
```

2. Start the dashboard:

```bash
npm install
npm run dev
```

The dashboard auto-loads env files from these locations (first one found wins per key):
- `Source/analytics-dashboard/.env` and `.env.local`
- `Source/.env` and `.env.local`
- repository root `.env` and `.env.local` (for shared monorepo env config)

If needed, set `NEXT_PUBLIC_ANALYTICS_API_URL` to point to your API server (defaults to `http://127.0.0.1:8787`).

3. Open `http://localhost:3000`.

## Implemented pages

- Overview: DAU, total generations, success rate, average latency, event volume
- Conversion funnel: `editor_opened -> prompt_submitted -> generation_completed -> midi_dragged`

## Filters

- Date range (`from`, `to`)
- Event type
