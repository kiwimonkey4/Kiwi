import fs from "node:fs";
import path from "node:path";
import { createHash } from "node:crypto";
import { db, EVENTS_COLLECTION } from "../src/firebase.js";
import { eventSchema, type AnalyticsEvent } from "../src/types.js";

const MAX_BATCH_WRITES = 500;

type ParsedRow = {
  id: string;
  event: AnalyticsEvent;
};

function chunk<T>(rows: T[], size: number): T[][] {
  const chunks: T[][] = [];
  for (let index = 0; index < rows.length; index += size) {
    chunks.push(rows.slice(index, index + size));
  }
  return chunks;
}

function parseJsonl(text: string): { rows: ParsedRow[]; skippedRows: number } {
  const rows = text
    .split(/\r?\n/)
    .map((row) => row.trim())
    .filter(Boolean);

  const parsedRows: ParsedRow[] = [];
  let skippedRows = 0;

  for (const row of rows) {
    try {
      const parsed = JSON.parse(row) as unknown;
      const validated = eventSchema.safeParse(parsed);
      if (!validated.success) {
        skippedRows += 1;
        continue;
      }

      const id = createHash("sha256").update(row).digest("hex");
      parsedRows.push({ id, event: validated.data });
    } catch {
      skippedRows += 1;
    }
  }

  return {
    rows: parsedRows,
    skippedRows
  };
}

async function migrateRows(rows: ParsedRow[]) {
  for (const chunkRows of chunk(rows, MAX_BATCH_WRITES)) {
    const batch = db.batch();
    for (const row of chunkRows) {
      batch.set(db.collection(EVENTS_COLLECTION).doc(row.id), row.event);
    }
    await batch.commit();
  }
}

async function main() {
  const args = process.argv.slice(2);
  const dryRun = args.includes("--dry-run");
  const fileArg = args.find((arg) => !arg.startsWith("--"));
  const filePath = path.resolve(process.cwd(), fileArg ?? "data/events.jsonl");

  if (!fs.existsSync(filePath)) {
    throw new Error(`JSONL file not found: ${filePath}`);
  }

  const text = fs.readFileSync(filePath, "utf8");
  const { rows, skippedRows } = parseJsonl(text);

  console.log(`Loaded ${rows.length} valid events from ${filePath}`);
  if (skippedRows > 0) {
    console.log(`Skipped ${skippedRows} invalid rows`);
  }

  if (dryRun) {
    console.log("Dry run enabled. No writes were performed.");
    return;
  }

  await migrateRows(rows);
  console.log(`Migrated ${rows.length} events into Firestore collection "${EVENTS_COLLECTION}"`);
}

main().catch((error) => {
  console.error("Migration failed", error);
  process.exitCode = 1;
});
