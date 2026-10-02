// The borders of a set of cells, each once: two neighbours in the set share one
// line, not two. A border is [x0, y0, x1, y1] in cells, on the gridlines: (0, 0)
// is the map's top-left corner, (cols, rows) its bottom-right one.
export type Border = [number, number, number, number];

export function cellBorders(cells: readonly number[], cols: number): Border[] {
  const across = new Set<number>(); // a cell's top: row * cols + col, row up to rows
  const down = new Set<number>(); // a cell's left: row * (cols + 1) + col, col up to cols
  for (const i of cells) {
    const col = i % cols;
    const row = Math.floor(i / cols);
    across.add(row * cols + col).add((row + 1) * cols + col);
    down.add(row * (cols + 1) + col).add(row * (cols + 1) + col + 1);
  }
  const borders: Border[] = [];
  for (const k of across) {
    const [x, y] = [k % cols, Math.floor(k / cols)];
    borders.push([x, y, x + 1, y]);
  }
  for (const k of down) {
    const [x, y] = [k % (cols + 1), Math.floor(k / (cols + 1))];
    borders.push([x, y, x, y + 1]);
  }
  return borders;
}
