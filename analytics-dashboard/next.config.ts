import type { NextConfig } from "next";

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
