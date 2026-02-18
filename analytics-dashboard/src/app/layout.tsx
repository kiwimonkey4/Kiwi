import type { Metadata } from "next";
import Dither from "@/components/dither";
import "./globals.css";

export const metadata: Metadata = {
  title: "Kiwi Analytics Dashboard",
  description: "Analytics dashboard for Kiwi plugin usage metrics"
};

export default function RootLayout({ children }: Readonly<{ children: React.ReactNode }>) {
  return (
    <html lang="en">
      <body className="relative min-h-screen overflow-x-hidden">
        <div className="fixed inset-0 z-0">
          <Dither
            baseColor={[0.25, 0.26, 0.19]}
            waveColor={[0.227, 0.624, 0.416]}
            disableAnimation={false}
            enableMouseInteraction
            mouseRadius={0.32}
            colorNum={4}
            waveAmplitude={0.3}
            waveFrequency={3}
            waveSpeed={0.05}
          />
        </div>
        <div className="relative z-10">{children}</div>
      </body>
    </html>
  );
}
