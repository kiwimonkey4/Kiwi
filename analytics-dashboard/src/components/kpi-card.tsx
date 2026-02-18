"use client";

import { useState } from "react";

export function KpiCard({ label, value, hint, info }: { label: string; value: string | number; hint?: string; info?: string }) {
  const [showInfo, setShowInfo] = useState(false);

  return (
    <article className="relative rounded-xl border border-kiwi-green-200 bg-kiwi-brown-50 p-4 shadow-sm backdrop-blur-sm">
      {info && (
        <div className="absolute right-3 top-3">
          <button
            onMouseEnter={() => setShowInfo(true)}
            onMouseLeave={() => setShowInfo(false)}
            onClick={() => setShowInfo(!showInfo)}
            className="flex h-4 w-4 items-center justify-center rounded-full border border-kiwi-brown-400 bg-kiwi-brown-50 text-[10px] font-bold text-kiwi-brown-600 hover:bg-kiwi-brown-100 transition-colors"
            aria-label="More information"
          >
            i
          </button>
          {showInfo && (
            <div className="absolute right-0 top-6 z-10 w-64 rounded-lg border border-kiwi-brown-400 bg-kiwi-brown-600 p-3 text-xs text-kiwi-brown-50 shadow-lg">
              {info}
            </div>
          )}
        </div>
      )}
      <p className="text-xs uppercase tracking-wider font-semibold text-kiwi-brown-600">{label}</p>
      <p className="mt-2 text-3xl font-bold tabular-nums text-kiwi-green-800">{value}</p>
      {hint ? <p className="mt-2 text-xs text-kiwi-brown-500">{hint}</p> : null}
    </article>
  );
}
