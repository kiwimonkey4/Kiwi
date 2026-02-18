"use client";

import { useEffect, useState } from "react";
import Link from "next/link";
import { usePathname } from "next/navigation";
import ASCIIText from "@/components/ascii-text";
import { FiltersBar } from "@/components/filters-bar";

const LINKS = [
  { href: "/", label: "Overview" },
  { href: "/funnel", label: "Conversion Data" }
];

export function DashboardShell({ title, titleInfo, children }: { title: string; titleInfo?: string; children: React.ReactNode }) {
  const pathname = usePathname();
  const [queryString, setQueryString] = useState("");
  const [showTitleInfo, setShowTitleInfo] = useState(false);

  useEffect(() => {
    setQueryString(window.location.search ?? "");
  }, [pathname]);

  return (
    <main className="mx-auto max-w-7xl p-6">
      <Link href="/" className="block">
        <div className="relative mx-auto mb-7 h-36 w-full max-w-5xl overflow-hidden rounded-xl border border-kiwi-brown-300 bg-kiwi-brown-700 cursor-pointer transition-opacity hover:opacity-90">
          <ASCIIText text="Kiwi Analytics" enableWaves={false} asciiFontSize={2} textFontSize={260} textColor="#f0f9f4" planeBaseHeight={10} />
        </div>
      </Link>
      <header className="relative mb-6 rounded-xl border border-kiwi-green-200 bg-kiwi-brown-50 p-6 shadow-sm backdrop-blur-sm">
        {titleInfo && (
          <div className="absolute right-3 top-3">
            <button
              onMouseEnter={() => setShowTitleInfo(true)}
              onMouseLeave={() => setShowTitleInfo(false)}
              onClick={() => setShowTitleInfo(!showTitleInfo)}
              className="flex h-5 w-5 items-center justify-center rounded-full border border-kiwi-brown-400 bg-kiwi-brown-50 text-xs font-bold text-kiwi-brown-600 hover:bg-kiwi-brown-100 transition-colors"
              aria-label="More information"
            >
              i
            </button>
            {showTitleInfo && (
              <div className="absolute right-0 top-7 z-20 w-80 rounded-lg border border-kiwi-brown-400 bg-kiwi-brown-600 px-3 py-2 text-sm leading-snug text-kiwi-brown-50 shadow-lg">
                {titleInfo}
              </div>
            )}
          </div>
        )}
        <div className="mb-4 flex flex-wrap items-center justify-center gap-3">
          <h1 className="text-3xl font-bold uppercase tracking-tight text-kiwi-green-800">{title}</h1>
        </div>
        <nav className="relative z-10 mb-4 flex flex-wrap gap-2">
          {LINKS.map((link) => {
            const active = pathname === link.href;
            return (
              <Link
                key={link.href}
                href={queryString ? `${link.href}?${queryString.replace(/^\?/, "")}` : link.href}
                className={`inline-block cursor-pointer rounded border px-3 py-1.5 text-xs uppercase tracking-wider font-semibold transition select-none ${
                  active
                    ? "border-kiwi-green-600 bg-kiwi-green-600 text-white"
                    : "border-kiwi-green-200 bg-kiwi-brown-50 text-kiwi-brown-700 hover:bg-kiwi-brown-100"
                }`}
              >
                {link.label}
              </Link>
            );
          })}
        </nav>
        <FiltersBar />
      </header>
      <section className="space-y-6">{children}</section>
    </main>
  );
}
