// WASD / arrows -> movement direction; 'e' -> capture. Returns a reader for the
// current intent, sampled once per fixed step by the game loop.
export interface KeyboardState {
  moveX: number;
  moveY: number;
  capturing: boolean;
}

export function installInput(): () => KeyboardState {
  const pressed = new Set<string>();
  const axis = (positive: boolean, negative: boolean) => (positive ? 1 : 0) - (negative ? 1 : 0);

  window.addEventListener("keydown", (e) => pressed.add(e.key.toLowerCase()));
  window.addEventListener("keyup", (e) => pressed.delete(e.key.toLowerCase()));

  return () => ({
    moveX: axis(pressed.has("d") || pressed.has("arrowright"), pressed.has("a") || pressed.has("arrowleft")),
    moveY: axis(pressed.has("s") || pressed.has("arrowdown"), pressed.has("w") || pressed.has("arrowup")),
    capturing: pressed.has("e"),
  });
}
