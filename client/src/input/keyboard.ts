// WASD / arrows -> movement direction; 'e' -> capture. Calls `onChange` only
// when the intent changes (the server holds it until the next input).
export function installInput(
  onChange: (moveX: number, moveY: number, capturing: boolean) => void,
): void {
  const pressed = new Set<string>();
  let last = { moveX: 0, moveY: 0, capturing: false };

  const axis = (positive: boolean, negative: boolean) => (positive ? 1 : 0) - (negative ? 1 : 0);

  const update = () => {
    const moveX = axis(
      pressed.has("d") || pressed.has("arrowright"),
      pressed.has("a") || pressed.has("arrowleft"),
    );
    const moveY = axis(
      pressed.has("s") || pressed.has("arrowdown"),
      pressed.has("w") || pressed.has("arrowup"),
    );
    const capturing = pressed.has("e");
    if (moveX !== last.moveX || moveY !== last.moveY || capturing !== last.capturing) {
      last = { moveX, moveY, capturing };
      onChange(moveX, moveY, capturing);
    }
  };

  window.addEventListener("keydown", (e) => {
    pressed.add(e.key.toLowerCase());
    update();
  });
  window.addEventListener("keyup", (e) => {
    pressed.delete(e.key.toLowerCase());
    update();
  });
}
