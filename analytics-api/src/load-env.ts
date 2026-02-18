import fs from "node:fs";
import path from "node:path";
import { config as loadDotenv } from "dotenv";

const explicitEnvPath = process.env.ANALYTICS_API_ENV_FILE;

const candidatePaths = [
  explicitEnvPath,
  path.resolve(process.cwd(), ".env"),
  path.resolve(process.cwd(), ".env.local"),
  path.resolve(process.cwd(), "..", ".env"),
  path.resolve(process.cwd(), "..", ".env.local"),
  path.resolve(process.cwd(), "..", "..", ".env"),
  path.resolve(process.cwd(), "..", "..", ".env.local")
].filter((value): value is string => Boolean(value));

const seen = new Set<string>();
const resolvedEnvPaths: string[] = [];
for (const envPath of candidatePaths) {
  const normalizedPath = path.normalize(envPath);
  if (seen.has(normalizedPath)) {
    continue;
  }
  seen.add(normalizedPath);
  resolvedEnvPaths.push(normalizedPath);

  if (fs.existsSync(normalizedPath)) {
    loadDotenv({ path: normalizedPath, override: false });
  }
}

function canParseJson(value: string): boolean {
  try {
    JSON.parse(value);
    return true;
  } catch {
    return false;
  }
}

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

const existingServiceAccountJson = process.env.FIREBASE_SERVICE_ACCOUNT_JSON?.trim();
const needsMultilineFallback =
  !existingServiceAccountJson ||
  existingServiceAccountJson === "{" ||
  !canParseJson(existingServiceAccountJson);

if (needsMultilineFallback) {
  for (const envPath of resolvedEnvPaths) {
    const multilineValue = extractMultilineJsonValue(envPath, "FIREBASE_SERVICE_ACCOUNT_JSON");
    if (!multilineValue) {
      continue;
    }

    if (canParseJson(multilineValue)) {
      process.env.FIREBASE_SERVICE_ACCOUNT_JSON = multilineValue;
      break;
    }
  }
}
