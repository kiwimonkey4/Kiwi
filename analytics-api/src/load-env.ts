/**
 * Loads environment variables from .env before other modules run.
 * Uses KiwiPlugin/.env (project root). Supports multiline JSON (e.g. FIREBASE_SERVICE_ACCOUNT_JSON).
 * Imported by firebase.ts so credentials are available at init time.
 */
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { config as loadDotenv } from "dotenv";

/** Path to .env file */
const envPath =
  process.env.ANALYTICS_API_ENV_FILE ??
  path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../../../.env");

if (fs.existsSync(envPath)) {
  loadDotenv({ path: envPath, override: false });
}

/** Check if a string is valid JSON*/
function canParseJson(value: string): boolean {
  try {
    JSON.parse(value);
    return true;
  } catch {
    return false;
  }
}

/** Extract a multiline JSON value from .env (e.g. FIREBASE_SERVICE_ACCOUNT_JSON={...}) */
function extractMultilineJsonValue(filePath: string, key: string): string | null {
  if (!fs.existsSync(filePath)) {
    return null;
  }

  const lines = fs.readFileSync(filePath, "utf8").split(/\r?\n/);
  const keyPrefix = `${key}=`;

  for (let lineIndex = 0; lineIndex < lines.length; lineIndex += 1) {
    const line = lines[lineIndex].trimStart();
    if (!line.startsWith(keyPrefix)) {
      continue;
    }

    const valueStart = line.slice(keyPrefix.length).trimStart();
    if (!valueStart.startsWith("{")) {
      return null;
    }

    let jsonText = valueStart;
    let depth = (valueStart.match(/\{/g) ?? []).length - (valueStart.match(/\}/g) ?? []).length;

    while (depth > 0 && lineIndex + 1 < lines.length) {
      lineIndex += 1;
      const nextLine = lines[lineIndex];
      jsonText += `\n${nextLine}`;
      depth += (nextLine.match(/\{/g) ?? []).length - (nextLine.match(/\}/g) ?? []).length;
    }

    return jsonText.trim();
  }

  return null;
}

/** If FIREBASE_SERVICE_ACCOUNT_JSON is missing or invalid, try extracting from .env as multiline */
const existingServiceAccountJson = process.env.FIREBASE_SERVICE_ACCOUNT_JSON?.trim();
const needsMultilineFallback =
  !existingServiceAccountJson ||
  existingServiceAccountJson === "{" ||
  !canParseJson(existingServiceAccountJson);

if (needsMultilineFallback) {
  const multilineValue = extractMultilineJsonValue(envPath, "FIREBASE_SERVICE_ACCOUNT_JSON");
  if (multilineValue && canParseJson(multilineValue)) {
    process.env.FIREBASE_SERVICE_ACCOUNT_JSON = multilineValue;
  }
}
