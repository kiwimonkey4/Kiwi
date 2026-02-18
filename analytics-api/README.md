# Kiwi Analytics API

Express API for ingesting and reading Kiwi analytics events from Firestore.

## Setup

1. Install dependencies:

   ```bash
   npm install
   ```

2. Create an environment file:

   ```bash
   cp .env.example .env
   ```

3. Configure Firebase credentials in one of these ways:
   - Set `FIREBASE_SERVICE_ACCOUNT_PATH` to a local service-account JSON file
   - Set `FIREBASE_SERVICE_ACCOUNT_JSON` to the JSON payload directly
   - Or rely on `GOOGLE_APPLICATION_CREDENTIALS` / application default credentials

   The API auto-loads env files from these locations (first one found wins per key):
   - `Source/analytics-api/.env` and `.env.local`
   - `Source/.env` and `.env.local`
   - repository root `.env` and `.env.local` (for shared monorepo env config)

4. Start the API:

   ```bash
   npm run dev
   ```

## Firestore Schema

Collection: `events`

Each document stores one analytics event:

- `event` (string)
- `ts_iso` (string, ISO timestamp)
- `user_id` (string)
- `session_id` (string)
- `app` (string)
- `app_version` (string)
- `props` (map, optional)

## Migrate Existing JSONL Data

If you have legacy data in `data/events.jsonl`, run:

```bash
npm run migrate:jsonl-to-firestore
```

Optional dry run:

```bash
npm run migrate:jsonl-to-firestore -- --dry-run
```

Optional custom file path:

```bash
npm run migrate:jsonl-to-firestore -- ./data/events.jsonl
```
