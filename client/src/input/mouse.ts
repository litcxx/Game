// Left mouse button = use the active ability while held; a min-hold keeps a quick
// click's attack alive long enough to be sampled by a fixed step. Also tracks
// the cursor (canvas pixels) for aiming. A right click on the canvas opens no
// browser menu: the menu would take the focus, and with it the keys held.
const MIN_ATTACK_HOLD_MS = 60;

export interface MouseState {
  attacking(): boolean;
  cursor(): { x: number; y: number } | undefined; // canvas pixels; undefined until it moves
}

export function installMouse(canvas: HTMLCanvasElement, canAttack: () => boolean): MouseState {
  let attacking = false;
  let downAt = 0;
  let releaseTimer: ReturnType<typeof setTimeout> | undefined;
  let cursor: { x: number; y: number } | undefined;

  const track = (e: MouseEvent) => {
    const rect = canvas.getBoundingClientRect();
    cursor = { x: e.clientX - rect.left, y: e.clientY - rect.top };
  };
  canvas.addEventListener("mousemove", track);

  canvas.addEventListener("mousedown", (e) => {
    track(e);
    if (e.button !== 0 || !canAttack()) return;
    if (releaseTimer !== undefined) {
      clearTimeout(releaseTimer);
      releaseTimer = undefined;
    }
    downAt = performance.now();
    attacking = true;
  });
  canvas.addEventListener("contextmenu", (e) => e.preventDefault());
  // A release anywhere in the window ends the attack (the canvas's own window).
  canvas.ownerDocument.defaultView!.addEventListener("mouseup", (e) => {
    if (e.button !== 0 || !attacking || releaseTimer !== undefined) return;
    const held = performance.now() - downAt;
    const release = () => {
      releaseTimer = undefined;
      attacking = false;
    };
    if (held >= MIN_ATTACK_HOLD_MS) release();
    else releaseTimer = setTimeout(release, MIN_ATTACK_HOLD_MS - held);
  });

  return { attacking: () => attacking, cursor: () => cursor };
}
