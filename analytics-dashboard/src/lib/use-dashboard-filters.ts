"use client";

import { useEffect, useState } from "react";
import { filtersFromSearchParams, getDefaultFilters } from "@/lib/filters";
import type { DashboardFilters } from "@/lib/types";

export function useDashboardFilters(): DashboardFilters {
  const [filters, setFilters] = useState<DashboardFilters>(getDefaultFilters());

  useEffect(() => {
    const updateFilters = () => {
      setFilters(filtersFromSearchParams(new URLSearchParams(window.location.search)));
    };

    updateFilters();
    window.addEventListener("popstate", updateFilters);
    return () => window.removeEventListener("popstate", updateFilters);
  }, []);

  return filters;
}
