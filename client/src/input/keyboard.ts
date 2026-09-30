// WASD / arrows -> movement direction; E -> capture. Returns a reader for the
// current intent, sampled once per fixed step by the game loop.
//
// Keys count by their place on the keyboard (KeyboardEvent.code), not by the
// letter they type, so the game plays the same in any layout: in the Russian one
// W types «ц» but is still KeyW. Every key is released when the window loses
// focus (alt-tab, a browser menu): the keyup would never come.
export interface KeyboardState {
  moveX: number;
  moveY: number;
  capturing: boolean;
}

// Whether a key press goes into a text field (the nickname), not to the game.
export function isTyping(target: EventTarget | null): boolean {
  const el = target as { tagName?: string; isContentEditable?: boolean } | null;
  return el?.isContentEditable === true || ["INPUT", "TEXTAREA", "SELECT"].includes(el?.tagName ?? "");
}

export function installInput(view: Window = window): () => KeyboardState {
  const pressed = new Set<string>(); // KeyboardEvent.code
  const axis = (positive: boolean, negative: boolean) => (positive ? 1 : 0) - (negative ? 1 : 0);

  view.addEventListener("keydown", (e) => {
    if (!isTyping(e.target)) pressed.add(e.code);
  });
  view.addEventListener("keyup", (e) => pressed.delete(e.code));
  view.addEventListener("blur", () => pressed.clear());

  return () => ({
    moveX: axis(pressed.has("KeyD") || pressed.has("ArrowRight"), pressed.has("KeyA") || pressed.has("ArrowLeft")),
    moveY: axis(pressed.has("KeyS") || pressed.has("ArrowDown"), pressed.has("KeyW") || pressed.has("ArrowUp")),
    capturing: pressed.has("KeyE"),
  });
}
