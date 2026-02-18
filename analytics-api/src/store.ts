import { FieldPath, type DocumentData, type QueryDocumentSnapshot } from "firebase-admin/firestore";
import { db, EVENTS_COLLECTION } from "./firebase.js";
import type { AnalyticsEvent } from "./types.js";

const MAX_BATCH_WRITES = 500;
const READ_PAGE_SIZE = 1000;

function chunk<T>(rows: T[], size: number): T[][] {
  const chunks: T[][] = [];
  for (let index = 0; index < rows.length; index += size) {
    chunks.push(rows.slice(index, index + size));
  }
  return chunks;
}

export async function appendEvents(events: AnalyticsEvent[]) {
  if (events.length === 0) {
    return;
  }

  for (const batchRows of chunk(events, MAX_BATCH_WRITES)) {
    const batch = db.batch();
    for (const event of batchRows) {
      const docRef = db.collection(EVENTS_COLLECTION).doc();
      batch.set(docRef, event);
    }
    await batch.commit();
  }
}

export async function readEvents(): Promise<AnalyticsEvent[]> {
  const events: AnalyticsEvent[] = [];
  let lastDoc: QueryDocumentSnapshot<DocumentData> | null = null;

  while (true) {
    let query = db.collection(EVENTS_COLLECTION).orderBy(FieldPath.documentId()).limit(READ_PAGE_SIZE);
    if (lastDoc) {
      query = query.startAfter(lastDoc);
    }

    const snapshot = await query.get();
    if (snapshot.empty) {
      break;
    }

    for (const doc of snapshot.docs) {
      events.push(doc.data() as AnalyticsEvent);
    }

    lastDoc = snapshot.docs[snapshot.docs.length - 1] ?? null;
    if (snapshot.size < READ_PAGE_SIZE) {
      break;
    }
  }

  return events;
}
