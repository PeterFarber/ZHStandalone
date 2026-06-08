import type { MapFile } from "./types";

export function cloneMap(map: MapFile): MapFile {
  return JSON.parse(JSON.stringify(map)) as MapFile;
}

export class MapHistory {
  private past: MapFile[] = [];
  private future: MapFile[] = [];

  constructor(private readonly maxDepth = 50) {}

  clear() {
    this.past = [];
    this.future = [];
  }

  record(map: MapFile) {
    this.past.push(cloneMap(map));
    if (this.past.length > this.maxDepth) {
      this.past.shift();
    }
    this.future = [];
  }

  canUndo(): boolean {
    return this.past.length > 0;
  }

  canRedo(): boolean {
    return this.future.length > 0;
  }

  undo(current: MapFile): MapFile | null {
    if (this.past.length === 0) return null;
    this.future.push(cloneMap(current));
    return cloneMap(this.past.pop()!);
  }

  redo(current: MapFile): MapFile | null {
    if (this.future.length === 0) return null;
    this.past.push(cloneMap(current));
    return cloneMap(this.future.pop()!);
  }
}
