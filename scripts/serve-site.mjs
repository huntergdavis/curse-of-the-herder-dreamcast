// Serve ./site locally with the cross-origin isolation headers the emulator needs (GitHub Pages gets them from the service worker).
//   node scripts/serve-site.mjs [port]
import { createServer } from "node:http";
import { readFile, stat } from "node:fs/promises";
import { extname, join, normalize } from "node:path";
const root = new URL("../site/", import.meta.url).pathname;
const port = Number(process.argv[2] ?? 4180);
const types = { ".html": "text/html; charset=utf-8", ".js": "text/javascript", ".mjs": "text/javascript", ".wasm": "application/wasm", ".json": "application/json", ".css": "text/css", ".png": "image/png", ".cdi": "application/octet-stream", ".data": "application/octet-stream", ".7z": "application/octet-stream" };
createServer(async (req, res) => {
  let path = normalize(decodeURIComponent(new URL(req.url, "http://x").pathname));
  if (path.endsWith("/")) path += "index.html";
  const file = join(root, path);
  try {
    const st = await stat(file);
    const body = await readFile(st.isDirectory() ? join(file, "index.html") : file);
    res.writeHead(200, {
      "Content-Type": types[extname(file)] ?? "application/octet-stream",
      "Cross-Origin-Opener-Policy": "same-origin",
      "Cross-Origin-Embedder-Policy": "require-corp",
      "Cross-Origin-Resource-Policy": "same-origin",
      "Cache-Control": "no-store",
    });
    res.end(body);
  } catch {
    res.writeHead(404).end("not found");
  }
}).listen(port, () => console.log(`site at http://localhost:${port}/`));
