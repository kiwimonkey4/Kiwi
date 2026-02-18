import type { Config } from "tailwindcss";

const config: Config = {
  content: ["./src/pages/**/*.{js,ts,jsx,tsx,mdx}", "./src/components/**/*.{js,ts,jsx,tsx,mdx}", "./src/app/**/*.{js,ts,jsx,tsx,mdx}"],
  theme: {
    extend: {
      colors: {
        kiwi: {
          green: {
            50: '#f0f9f4',
            100: '#dcf2e4',
            200: '#bae5cd',
            300: '#8bd3ad',
            400: '#5aba87',
            500: '#3a9f6a',
            600: '#2a8054',
            700: '#226645',
            800: '#1d5138',
            900: '#18432f',
          },
          brown: {
            50: '#faf8f5',
            100: '#f2ede5',
            200: '#e4d9c9',
            300: '#d1bea5',
            400: '#ba9d7c',
            500: '#a8855f',
            600: '#8f6e4d',
            700: '#765841',
            800: '#614938',
            900: '#513d30',
          },
        },
      },
    },
  },
  plugins: []
};

export default config;
