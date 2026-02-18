import fs from "node:fs";
import path from "node:path";
import { applicationDefault, cert, getApps, initializeApp, type ServiceAccount } from "firebase-admin/app";
import { getFirestore } from "firebase-admin/firestore";
import "./load-env.js";

function normalizeServiceAccount(raw: Record<string, unknown>): ServiceAccount {
  const projectId = raw.projectId ?? raw.project_id;
  const clientEmail = raw.clientEmail ?? raw.client_email;
  const privateKeyValue = raw.privateKey ?? raw.private_key;
  const privateKey = typeof privateKeyValue === "string" ? privateKeyValue.replace(/\\n/g, "\n") : undefined;

  if (typeof projectId !== "string" || typeof clientEmail !== "string" || typeof privateKey !== "string") {
    throw new Error("Service account is missing required fields (project_id, client_email, private_key)");
  }

  return {
    projectId,
    clientEmail,
    privateKey
  };
}

function loadServiceAccountFromPath(rawPath: string): ServiceAccount {
  const resolvedPath = path.isAbsolute(rawPath) ? rawPath : path.resolve(process.cwd(), rawPath);
  const contents = fs.readFileSync(resolvedPath, "utf8");
  const parsed = JSON.parse(contents) as Record<string, unknown>;
  return normalizeServiceAccount(parsed);
}

function getFirebaseCredential() {
  const serviceAccountJson = process.env.FIREBASE_SERVICE_ACCOUNT_JSON;
  if (serviceAccountJson) {
    try {
      const parsed = JSON.parse(serviceAccountJson) as Record<string, unknown>;
      return cert(normalizeServiceAccount(parsed));
    } catch (error) {
      throw new Error(
        `Failed to parse FIREBASE_SERVICE_ACCOUNT_JSON. Ensure it is valid JSON (single-line or a valid multiline JSON block in .env). ${
          error instanceof Error ? error.message : String(error)
        }`
      );
    }
  }

  const serviceAccountPath = process.env.FIREBASE_SERVICE_ACCOUNT_PATH ?? process.env.GOOGLE_APPLICATION_CREDENTIALS;
  if (serviceAccountPath) {
    return cert(loadServiceAccountFromPath(serviceAccountPath));
  }

  return applicationDefault();
}

const firebaseApp =
  getApps().length > 0
    ? getApps()[0]
    : initializeApp({
        credential: getFirebaseCredential(),
        projectId: process.env.FIREBASE_PROJECT_ID
      });

export const db = getFirestore(firebaseApp);
export const EVENTS_COLLECTION = process.env.FIRESTORE_EVENTS_COLLECTION ?? "events";
