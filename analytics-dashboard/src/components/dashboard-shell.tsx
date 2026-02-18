"use client";

import { useEffect, useState } from "react";
import Link from "next/link";
import { usePathname } from "next/navigation";
import ASCIIText from "@/components/ascii-text";
import { FiltersBar } from "@/components/filters-bar";

const LINKS = [
  { href: "/", label: "Overview" },
  { href: "/funnel", label: "Conversion funnel" }
];

export function DashboardShell({ title, children }: { title: string; children: React.ReactNode }) {
  const pathname = usePathname();
  const [queryString, setQueryString] = useState("");

  useEffect(() => {
    setQueryString(window.location.search ?? "");
  }, [pathname]);

  return (
    <main className="mx-auto max-w-7xl p-6">
      <div className="relative mx-auto mb-7 h-36 w-full max-w-5xl overflow-hidden rounded-xl border border-kiwi-brown-300 bg-kiwi-brown-700">
        <ASCIIText text="Kiwi Analytics" enableWaves={false} asciiFontSize={2} textFontSize={260} textColor="#f0f9f4" planeBaseHeight={10} />
      </div>
      <header className="mb-6 rounded-xl border border-kiwi-green-200 bg-kiwi-brown-50 p-6 shadow-sm backdrop-blur-sm">
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
