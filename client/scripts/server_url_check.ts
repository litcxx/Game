// Pure checks for the server address the client connects to: VITE_SERVER_URL
// when the build sets it, else the page's own origin at /ws (wss on https).
// Run: npx tsx scripts/server_url_check.ts
import { serverUrl } from "../src/net/serverUrl.js";

let failures = 0;
const check = (name: string, cond: boolean) => {
  console.log(`${cond ? "ok  " : "FAIL"} ${name}`);
  if (!cond) failures++;
};

const https = { protocol: "https:", host: "game.example" };
const http = { protocol: "http:", host: "127.0.0.1:8080" };

check("a configured address wins", serverUrl("ws://localhost:27998/", https) === "ws://localhost:27998/");
check("an https page: wss on its own origin", serverUrl(undefined, https) === "wss://game.example/ws");
check("an http page: ws on its own origin, port kept", serverUrl(undefined, http) === "ws://127.0.0.1:8080/ws");
check("an empty setting means the default", serverUrl("", https) === "wss://game.example/ws");
check("a blank setting means the default", serverUrl("   ", https) === "wss://game.example/ws");
check("a configured address is trimmed", serverUrl(" wss://a.example/ws ", http) === "wss://a.example/ws");

console.log("VERDICT:", failures === 0 ? "PASS" : `FAIL (${failures})`);
process.exit(failures === 0 ? 0 : 1);
