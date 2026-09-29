// Build-time settings Vite puts in import.meta.env (from .env files or the
// environment of `npm run build` / `npm run dev`).
interface ImportMetaEnv {
  // The game server's WebSocket address; unset: the page's origin at /ws.
  readonly VITE_SERVER_URL?: string;
}

interface ImportMeta {
  readonly env: ImportMetaEnv;
}
