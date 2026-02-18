export function KpiCard({ label, value, hint }: { label: string; value: string | number; hint?: string }) {
  return (
    <article className="rounded-xl border border-kiwi-green-200 bg-kiwi-brown-50 p-4 shadow-sm backdrop-blur-sm">
      <p className="text-xs uppercase tracking-wider font-semibold text-kiwi-brown-600">{label}</p>
      <p className="mt-2 text-3xl font-bold tabular-nums text-kiwi-green-800">{value}</p>
      {hint ? <p className="mt-2 text-xs text-kiwi-brown-500">{hint}</p> : null}
    </article>
  );
}
