// WASD / arrow keys -> movement direction. Calls `onChange` only when the
// direction actually changes (the server holds the intent until the next input).
export function installMovementKeys(onChange: (moveX: number, moveY: number) => void): void {
  const pressed = new Set<string>();
  let last: [number, number] = [0, 0];

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
    if (moveX !== last[0] || moveY !== last[1]) {
      last = [moveX, moveY];
      onChange(moveX, moveY);
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
