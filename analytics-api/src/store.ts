/**
 * Firestore data access layer for analytics events.
 * appendEvents writes to Firestore; readEvents reads all events with pagination.
 */
import { FieldPath, type DocumentData, type QueryDocumentSnapshot } from "firebase-admin/firestore";
import { db, EVENTS_COLLECTION } from "./firebase.js";
import type { AnalyticsEvent } from "./types.js";

const MAX_BATCH_WRITES = 500; // Firestore batch limit 
const READ_PAGE_SIZE = 1000;

/** Split array into chunks of given size */
function chunk<T>(rows: T[], size: number): T[][] {
  const chunks: T[][] = [];
  for (let index = 0; index < rows.length; index += size) {
    chunks.push(rows.slice(index, index + size));
  }
  return chunks;
}

/** Write events to Firestore collection. Each event becomes a document in EVENTS_COLLECTION. */
export async function appendEvents(events: AnalyticsEvent[]) {
  if (events.length === 0) {
    return;
  }

  // Split events into a chunk of 500 events to respect Firestore limits (more precautionary than anything, likely won't approach batch size anywhere near 500)
  for (const batchRows of chunk(events, MAX_BATCH_WRITES)) {
    const batch = db.batch(); // Create a new batch for the current chunk of events 

    // Add each event in the chunk to the batch with a new document reference 
    for (const event of batchRows) {
      const docRef = db.collection(EVENTS_COLLECTION).doc();
      batch.set(docRef, event);
    }
    await batch.commit(); // Commit the batch to write all events in the current chunk to Firestore 
  }
}

/** Read all events from Firestore, paginated by document ID. */
export async function readEvents(): Promise<AnalyticsEvent[]> {
  const events: AnalyticsEvent[] = []; // Array to hold all events read from Firestore 
  let lastDoc: QueryDocumentSnapshot<DocumentData> | null = null; // Keeps track of where previous page ended 

  while (true) {

    // Read a page of events ordered by document ID, starting after the last document from the previous page 
    let query = db.collection(EVENTS_COLLECTION).orderBy(FieldPath.documentId()).limit(READ_PAGE_SIZE);
    if (lastDoc) {
      query = query.startAfter(lastDoc);
    }

    // Send query to Firestore and store results in snapshot 
    const snapshot = await query.get();
    if (snapshot.empty) {
      break;
    }

    // Add each event document from the page to the AnalyticsEvent array 
    for (const doc of snapshot.docs) {
      events.push(doc.data() as AnalyticsEvent);
    }

    lastDoc = snapshot.docs[snapshot.docs.length - 1] ?? null; // update position of last-read document

    // All documents are on the same page, so stop reading more pages 
    if (snapshot.size < READ_PAGE_SIZE) {
      break;
    }
  }

  return events; 
}