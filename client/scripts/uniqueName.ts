// A player name that is most likely free: `base`, a dash and four random
// base-36 characters (keep `base` to 11 characters: names are 1–16). The server
// keeps every name taken (ignoring case) until it restarts, so a fixed name is
// refused (INVALID_NAME) on its second join. The smoke scripts name their
// players this way.
export function uniqueName(base: string): string {
  const suffix = Math.random().toString(36).slice(2, 6).padEnd(4, "0");
  return `${base}-${suffix}`;
}
