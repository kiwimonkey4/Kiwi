import type { Metadata } from "next";
import "./globals.css";

export const metadata: Metadata = {
  title: "Kiwi Analytics Dashboard",
  description: "Analytics dashboard for Kiwi plugin usage metrics"
};

export default function RootLayout({ children }: Readonly<{ children: React.ReactNode }>) {
  return (
    <html lang="en">
      <body>{children}</body>
    </html>
  );
}
