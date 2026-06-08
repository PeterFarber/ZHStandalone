import tailwindcss from "@tailwindcss/vite";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { defineConfig } from "vite";

const here = path.dirname(fileURLToPath(import.meta.url));
const gameResources = path.resolve(here, "../game/resources");

export default defineConfig({
  root: ".",
  plugins: [tailwindcss()],
  server: {
    fs: {
      allow: [here, gameResources],
    },
    proxy: {
      "/resources": {
        target: "http://127.0.0.1",
        bypass(req) {
          const rel = decodeURIComponent((req.url ?? "").replace(/^\/resources\/?/, ""));
          if (!rel) return undefined;
          const file = path.join(gameResources, rel);
          if (fs.existsSync(file) && fs.statSync(file).isFile()) {
            return file;
          }
          return undefined;
        },
      },
    },
  },
});
