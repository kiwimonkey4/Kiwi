"use client";

import { useEffect, useState } from "react";
import Link from "next/link";
import { usePathname } from "next/navigation";
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
      <header className="mb-6 rounded-xl bg-white p-5 shadow-sm">
        <div className="mb-4 flex flex-wrap items-center justify-between gap-3">
          <div>
            <h1 className="text-2xl font-semibold text-gray-900">{title}</h1>
            <p className="text-sm text-gray-600">Kiwi plugin analytics dashboard</p>
          </div>
        </div>
        <nav className="mb-4 flex flex-wrap gap-2">
          {LINKS.map((link) => {
            const active = pathname === link.href;
            return (
              <Link
                key={link.href}
                href={`${link.href}${queryString}`}
                className={`rounded-full border px-3 py-1.5 text-sm transition ${
                  active ? "border-gray-900 bg-gray-900 text-white" : "border-gray-200 bg-white text-gray-700 hover:bg-gray-100"
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
