import type { MapFile, Selection } from "./types";

export type DrawableEntry =
  | { kind: "visual"; index: number; z: number }
  | { kind: "shape"; index: number; z: number };

export type ZCapableSelection = { type: "visual"; index: number } | { type: "shape"; index: number };

export function defaultVisualZ(index: number): number {
  return index;
}

export function defaultShapeZ(map: MapFile, index: number): number {
  return map.visuals.length + index;
}

export function effectiveVisualZ(map: MapFile, index: number): number {
  const piece = map.visuals[index];
  if (!piece) return index;
  return piece.z ?? defaultVisualZ(index);
}

export function effectiveShapeZ(map: MapFile, index: number): number {
  const shape = map.shapes[index];
  if (!shape) return defaultShapeZ(map, index);
  return shape.z ?? defaultShapeZ(map, index);
}

export function effectiveSelectionZ(map: MapFile, sel: Selection): number | null {
  if (sel.type === "visual") return effectiveVisualZ(map, sel.index);
  if (sel.type === "shape") return effectiveShapeZ(map, sel.index);
  return null;
}

export function selectionHasZOrder(selections: Selection[]): boolean {
  return selections.some((s) => s.type === "visual" || s.type === "shape");
}

export function zCapableSelections(selections: Selection[]): ZCapableSelection[] {
  return selections.filter(
    (s): s is ZCapableSelection => s.type === "visual" || s.type === "shape",
  );
}

export function drawableEntryKey(entry: DrawableEntry): string {
  return `${entry.kind}:${entry.index}`;
}

export function selectionToDrawableKey(sel: Selection): string | null {
  if (sel.type === "visual" || sel.type === "shape") {
    return `${sel.type}:${sel.index}`;
  }
  return null;
}

export function buildDrawableOrder(map: MapFile): DrawableEntry[] {
  const entries: DrawableEntry[] = [];
  for (let i = 0; i < map.visuals.length; i++) {
    entries.push({ kind: "visual", index: i, z: effectiveVisualZ(map, i) });
  }
  for (let i = 0; i < map.shapes.length; i++) {
    entries.push({ kind: "shape", index: i, z: effectiveShapeZ(map, i) });
  }
  entries.sort((a, b) => {
    if (a.z !== b.z) return a.z - b.z;
    if (a.kind !== b.kind) return a.kind === "visual" ? -1 : 1;
    return a.index - b.index;
  });
  return entries;
}

export function materializeDrawableZ(map: MapFile): void {
  for (let i = 0; i < map.visuals.length; i++) {
    const piece = map.visuals[i];
    if (piece.z === undefined) piece.z = defaultVisualZ(i);
  }
  for (let i = 0; i < map.shapes.length; i++) {
    const shape = map.shapes[i];
    if (shape.z === undefined) shape.z = defaultShapeZ(map, i);
  }
}

export function setSelectionZValue(map: MapFile, sel: Selection, z: number): void {
  if (sel.type === "visual") {
    map.visuals[sel.index].z = z;
  } else if (sel.type === "shape") {
    map.shapes[sel.index].z = z;
  }
}
