// Pure checks for server errors on the client: player-facing texts, the error
// behind a WebSocket close code, and what the hint line shows (a fatal error for
// good, a refusal for a while). Run: npx tsx scripts/errors_check.ts
import { closeError, errorText, NOTICE_MS, Notices } from "../src/errors.js";
import { ErrorCode } from "../src/gen/game/v1/protocol_pb.js";

let failures = 0;
const check = (name: string, cond: boolean) => {
  console.log(`${cond ? "ok  " : "FAIL"} ${name}`);
  if (!cond) failures++;
};

// Every code the protocol defines has its own text.
const codes = Object.values(ErrorCode).filter((v): v is ErrorCode => typeof v === "number" && v !== 0);
const texts = codes.map(errorText);
check("every error code has a text", texts.every((t) => t.length > 0 && !t.includes("код")));
check("the texts are all different", new Set(texts).size === texts.length);
check("an unknown code still says something, with its number", errorText(999 as ErrorCode).includes("999"));
check("PROTOCOL_VERSION asks to reload", errorText(ErrorCode.PROTOCOL_VERSION).includes("перезагрузите"));

check("close 4001 is PROTOCOL_VERSION", closeError(4001) === ErrorCode.PROTOCOL_VERSION);
check("close 4007 is INVALID_NAME", closeError(4007) === ErrorCode.INVALID_NAME);
check("a normal close is no error", closeError(1000) === undefined);
check("an abnormal close is no error code", closeError(1006) === undefined);
check("4000 + an unknown code is no error code", closeError(4999) === undefined);

{
  const n = new Notices();
  check("nothing to tell: the usual hint", n.hint("usual", 0) === "usual" && !n.failed);
  n.refuse("refused", 1000);
  check("a refusal shows for a while", n.hint("usual", 1000 + NOTICE_MS - 1) === "refused");
  check("then the usual hint is back", n.hint("usual", 1000 + NOTICE_MS) === "usual");
}
{
  const n = new Notices();
  n.fail("version");
  n.fail("closed 4001"); // the close code follows the error frame: keep the first
  n.refuse("refused", 0);
  check("a fatal error stays and wins", n.hint("usual", 1e9) === "version" && n.failed);
}

console.log(failures === 0 ? "VERDICT: PASS" : `VERDICT: FAIL (${failures})`);
process.exit(failures === 0 ? 0 : 1);
