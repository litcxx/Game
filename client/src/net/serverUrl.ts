// The game server's WebSocket address: `configured` (VITE_SERVER_URL, set at
// build time) when given, else the page's own origin at /ws — where a reverse
// proxy in front of the server takes it (wss for an https page, ws for http).
export function serverUrl(configured: string | undefined, page: { protocol: string; host: string }): string {
  const url = configured?.trim();
  if (url) return url;
  return `${page.protocol === "https:" ? "wss" : "ws"}://${page.host}/ws`;
}
