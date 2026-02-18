import type { NextConfig } from "next";
import fs from "node:fs";
import path from "node:path";
import { config as loadDotenv } from "dotenv";

const candidatePaths = [
  path.resolve(process.cwd(), ".env"),
  path.resolve(process.cwd(), ".env.local"),
  path.resolve(process.cwd(), "..", ".env"),
  path.resolve(process.cwd(), "..", ".env.local"),
  path.resolve(process.cwd(), "..", "..", ".env"),
  path.resolve(process.cwd(), "..", "..", ".env.local")
];

const seen = new Set<string>();
for (const envPath of candidatePaths) {
  const normalizedPath = path.normalize(envPath);
  if (seen.has(normalizedPath)) {
    continue;
  }
  seen.add(normalizedPath);

  if (fs.existsSync(normalizedPath)) {
    loadDotenv({ path: normalizedPath, override: false });
  }
}

const nextConfig: NextConfig = {
  webpack: (config, { dev }) => {
    // OneDrive frequently locks .next webpack cache files on Windows, which can corrupt
    // dev bundles and cause runtime module-manifest errors.
    if (dev) {
      config.cache = false;
    }
    return config;
  }
};

export default nextConfig;
