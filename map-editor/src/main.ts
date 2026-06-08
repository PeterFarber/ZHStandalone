import "./style.css";
import { MapEditor } from "./editor";
import { DEFAULT_TILE_CELL, clampSrcRect, pickCellContaining, pickRegion, type SheetKey } from "./tileSheets";
import type { DrawShape, Rect, Tool } from "./types";
import { DRAW_SHAPES } from "./types";
import {
  cn,
  makeButton,
  makeField,
  makeNumberField,
  makePanel,
  makeToolbarGroup,
  setButtonActive,
} from "./ui";

const canvas = document.getElementById("canvas") as HTMLCanvasElement;
const toolbar = document.getElementById("toolbar")!;
const sidebarLeft = document.getElementById("sidebar-left")!;
const sidebarRight = document.getElementById("sidebar-right")!;
const status = document.getElementById("status")!;

const editor = new MapEditor(canvas, status);

function shapeButton(label: string, shape: DrawShape): HTMLButtonElement {
  const button = makeButton(label, () => {
    editor.setDrawShape(shape);
  }, { block: true });
  button.classList.add("shape-btn");
  button.dataset.shape = shape;
  return button;
}

function toolButton(label: string, tool: Tool): HTMLButtonElement {
  const button = makeButton(label, () => {
    editor.setTool(tool);
  }, { block: true });
  button.classList.add("tool-btn");
  button.dataset.tool = tool;
  return button;
}

const fileInput = document.createElement("input");
fileInput.type = "file";
fileInput.accept = "application/json,.json";
fileInput.onchange = async () => {
  const file = fileInput.files?.[0];
  if (!file) return;
  editor.importJson(await file.text());
  fileInput.value = "";
};

const gridToggle = makeButton("Grid", () => {
  editor.showGrid = !editor.showGrid;
  refreshViewToggles();
}, { active: true });

const logicToggle = makeButton("Logic", () => {
  editor.showLogic = !editor.showLogic;
  refreshViewToggles();
}, { active: true });

toolbar.className = cn.toolbar;
toolbar.append(
  makeToolbarGroup("File", [
    makeButton("New", () => editor.resetDefault()),
    makeButton("Open", () => fileInput.click()),
    makeButton("Download", () => editor.download(`${editor.map.name || "map"}.json`)),
    fileInput,
  ]),
  makeToolbarGroup("Edit", [
    makeButton("Undo", () => editor.undo()),
    makeButton("Redo", () => editor.redo()),
  ]),
  makeToolbarGroup("View", [gridToggle, logicToggle]),
);

const toolsPanel = makePanel("Tools", true);
for (const [title, items] of [
  ["General", [["Select", "select"]]],
  ["Draw", [["Color", "color_shape"]]],
  [
    "Visuals",
    [
      ["Ice", "ice_tile"],
      ["Walls", "wall_frame"],
      ["Circle", "circle_mark"],
      ["Goal art", "goal_sprite"],
    ],
  ],
  [
    "Zones",
    [
      ["West goal · Team A", "goal_west"],
      ["East goal · Team B", "goal_east"],
      ["Blocker", "collider"],
    ],
  ],
  [
    "Spawns · Team A",
    [
      ["GK", "spawn_team_a_gk"],
      ["SK 1", "spawn_team_a_sk0"],
      ["SK 2", "spawn_team_a_sk1"],
      ["SK 3", "spawn_team_a_sk2"],
    ],
  ],
  [
    "Spawns · Team B",
    [
      ["GK", "spawn_team_b_gk"],
      ["SK 1", "spawn_team_b_sk0"],
      ["SK 2", "spawn_team_b_sk1"],
      ["SK 3", "spawn_team_b_sk2"],
    ],
  ],
  [
    "Spawns · Puck",
    [["Puck", "spawn_puck"]],
  ],
] as [string, [string, Tool][]][]) {
  const group = document.createElement("div");
  group.className = "space-y-1.5";
  const heading = document.createElement("h4");
  heading.className = cn.subheading;
  heading.textContent = title;
  const grid = document.createElement("div");
  grid.className = cn.toolGrid;
  for (const [label, tool] of items) {
    grid.append(toolButton(label, tool));
  }
  group.append(heading, grid);
  toolsPanel.body.append(group);
}

const shapePanel = makePanel("Draw shape", true);
const shapeGrid = document.createElement("div");
shapeGrid.className = cn.shapeGrid;
for (const shape of DRAW_SHAPES) {
  shapeGrid.append(shapeButton(shape[0].toUpperCase() + shape.slice(1), shape));
}
shapePanel.body.append(shapeGrid);

const cornerRadiusWrap = document.createElement("div");
cornerRadiusWrap.className = "mt-3 space-y-1.5";
const cornerRadiusLabel = document.createElement("div");
cornerRadiusLabel.className = cn.subheading;
cornerRadiusLabel.textContent = "Corner radius";
const cornerRadiusInput = document.createElement("input");
cornerRadiusInput.type = "range";
cornerRadiusInput.min = "0";
cornerRadiusInput.max = "256";
cornerRadiusInput.step = "1";
cornerRadiusInput.value = String(editor.cornerRadius);
cornerRadiusInput.className = "w-full";
cornerRadiusInput.addEventListener("input", () => {
  editor.cornerRadius = Math.max(0, Number(cornerRadiusInput.value) || 0);
});
cornerRadiusWrap.append(cornerRadiusLabel, cornerRadiusInput);
shapePanel.body.append(cornerRadiusWrap);

const colorPanel = makePanel("Draw colors", true);
const colorForm = document.createElement("div");
colorForm.className = "space-y-3";

const fillRow = document.createElement("label");
fillRow.className = "flex items-center gap-2 text-xs text-slate-300";
const fillEnabled = document.createElement("input");
fillEnabled.type = "checkbox";
fillEnabled.checked = true;
const fillPicker = document.createElement("input");
fillPicker.type = "color";
fillPicker.value = "#4dabff";
fillPicker.className = "h-8 w-12 cursor-pointer rounded border border-slate-600 bg-slate-900";
const fillOpacity = document.createElement("input");
fillOpacity.type = "range";
fillOpacity.min = "0";
fillOpacity.max = "100";
fillOpacity.value = "33";
fillOpacity.className = "min-w-0 flex-1";
fillRow.append(fillEnabled, document.createTextNode("Fill"), fillPicker, fillOpacity);

const strokeRow = document.createElement("label");
strokeRow.className = "flex items-center gap-2 text-xs text-slate-300";
const strokeEnabled = document.createElement("input");
strokeEnabled.type = "checkbox";
strokeEnabled.checked = true;
const strokePicker = document.createElement("input");
strokePicker.type = "color";
strokePicker.value = "#ffffff";
strokePicker.className = "h-8 w-12 cursor-pointer rounded border border-slate-600 bg-slate-900";
strokeRow.append(strokeEnabled, document.createTextNode("Stroke"), strokePicker);

const strokeWidthInput = document.createElement("input");
strokeWidthInput.type = "number";
strokeWidthInput.min = "1";
strokeWidthInput.max = "32";
strokeWidthInput.step = "1";
strokeWidthInput.value = "2";
strokeWidthInput.className = cn.input;

function rgbaFromHex(hex: string, alpha: number): string {
  const c = hex.replace("#", "");
  const r = parseInt(c.slice(0, 2), 16);
  const g = parseInt(c.slice(2, 4), 16);
  const b = parseInt(c.slice(4, 6), 16);
  return `rgba(${r}, ${g}, ${b}, ${alpha})`;
}

function syncDrawColorsFromEditor() {
  fillEnabled.checked = editor.drawFillEnabled;
  strokeEnabled.checked = editor.drawStrokeEnabled;
  strokeWidthInput.value = String(editor.drawStrokeWidth);
  strokePicker.value = editor.drawStroke.slice(0, 7);
  const fillMatch = editor.drawFill.match(/rgba?\(([^)]+)\)/);
  if (fillMatch) {
    const parts = fillMatch[1].split(",").map((p) => p.trim());
    if (parts.length >= 4) {
      const r = Number(parts[0]);
      const g = Number(parts[1]);
      const b = Number(parts[2]);
      const a = Number(parts[3]);
      fillPicker.value = `#${r.toString(16).padStart(2, "0")}${g.toString(16).padStart(2, "0")}${b.toString(16).padStart(2, "0")}`;
      fillOpacity.value = String(Math.round(a * 100));
      return;
    }
  }
  if (editor.drawFill.startsWith("#")) {
    if (editor.drawFill.length === 9) {
      fillPicker.value = editor.drawFill.slice(0, 7);
      fillOpacity.value = String(
        Math.round((parseInt(editor.drawFill.slice(7, 9), 16) / 255) * 100),
      );
      return;
    }
    if (editor.drawFill.length >= 7) {
      fillPicker.value = editor.drawFill.slice(0, 7);
      fillOpacity.value = "100";
    }
  }
}

function applyDrawColorsToEditor() {
  editor.drawFillEnabled = fillEnabled.checked;
  editor.drawStrokeEnabled = strokeEnabled.checked;
  editor.drawStrokeWidth = Math.max(1, Number(strokeWidthInput.value) || 2);
  editor.drawStroke = strokePicker.value;
  editor.drawFill = rgbaFromHex(fillPicker.value, Number(fillOpacity.value) / 100);
}

for (const el of [fillEnabled, fillPicker, fillOpacity, strokeEnabled, strokePicker, strokeWidthInput]) {
  el.addEventListener("input", () => {
    applyDrawColorsToEditor();
    refreshColorPanel();
  });
}

colorForm.append(
  fillRow,
  makeField("Stroke width", strokeWidthInput),
  strokeRow,
);
colorPanel.body.append(colorForm);

const selectionPanel = makePanel("Selection", true);
const selectionSummary = document.createElement("div");
selectionSummary.className = cn.summaryBox;
selectionSummary.textContent = "Nothing selected";
const selectionHint = document.createElement("p");
selectionHint.className = cn.muted;
selectionHint.textContent =
  "Left-click selects top object. Right-click cycles stacked objects. Ctrl+click toggles; Shift+click adds. Locked objects can be selected but not moved.";
const lockButton = makeButton("Lock", () => editor.toggleSelectionLock(), { block: true });
const selectionActions = document.createElement("div");
selectionActions.className = cn.actionRow;
selectionActions.append(
  lockButton,
  makeButton("Clear", () => editor.clearSelection(), { block: true }),
  makeButton("Delete", () => editor.deleteSelected(), { block: true, danger: true }),
);
selectionPanel.body.append(selectionSummary, selectionHint, selectionActions);

const zOrderSection = document.createElement("div");
zOrderSection.className = "mt-3 space-y-2";
const zOrderHeading = document.createElement("div");
zOrderHeading.className = cn.subheading;
zOrderHeading.textContent = "Layer (z-index)";
const zOrderHint = document.createElement("p");
zOrderHint.className = cn.muted;
zOrderHint.textContent = "[ / ] step · Ctrl+[ / Ctrl+] back/front";
const zInput = document.createElement("input");
zInput.type = "number";
zInput.step = "1";
zInput.className = cn.input;
const zButtonRow = document.createElement("div");
zButtonRow.className = cn.actionRow;
zButtonRow.append(
  makeButton("Back", () => editor.nudgeSelectionZ(-1), { block: true }),
  makeButton("Fwd", () => editor.nudgeSelectionZ(1), { block: true }),
);
const zExtButtonRow = document.createElement("div");
zExtButtonRow.className = cn.actionRow;
zExtButtonRow.append(
  makeButton("To back", () => editor.sendSelectionToBack(), { block: true }),
  makeButton("To front", () => editor.bringSelectionToFront(), { block: true }),
);
zOrderSection.append(
  zOrderHeading,
  zOrderHint,
  makeField("Z index", zInput),
  zButtonRow,
  zExtButtonRow,
  makeButton("Apply z", () => {
    const value = Number(zInput.value);
    if (Number.isFinite(value)) editor.setSelectionZ(value);
  }, { block: true }),
);
zOrderSection.hidden = true;
selectionPanel.body.append(zOrderSection);

const objectListPanel = makePanel("Objects", true);
const objectListHint = document.createElement("p");
objectListHint.className = cn.muted;
objectListHint.textContent =
  "Pick by name. Ctrl+click toggles selection; Shift+click adds to selection.";
const objectListBody = document.createElement("div");
objectListBody.className = "max-h-72 space-y-1 overflow-y-auto";
objectListPanel.body.append(objectListHint, objectListBody);

const leftHint = document.createElement("p");
leftHint.className = cn.hint;
leftHint.innerHTML =
  '<span class="font-medium text-slate-400">Canvas:</span> green = play area · purple = camera view · <span class="font-medium text-slate-400">Right-click</span> cycles stacked objects (Select tool) · Logic overlay for tags';

sidebarLeft.append(toolsPanel.el, shapePanel.el, colorPanel.el, selectionPanel.el, objectListPanel.el, leftHint);

const settingsPanel = makePanel("Map settings", true);
const settingsForm = document.createElement("div");
settingsForm.className = "space-y-3";
const playWidthInput = document.createElement("input");
playWidthInput.type = "number";
playWidthInput.min = "320";
playWidthInput.step = "8";
playWidthInput.className = cn.input;
const playHeightInput = document.createElement("input");
playHeightInput.type = "number";
playHeightInput.min = "180";
playHeightInput.step = "8";
playHeightInput.className = cn.input;
const lockAspectLabel = document.createElement("label");
lockAspectLabel.className = cn.checkRow;
const lockAspectCheck = document.createElement("input");
lockAspectCheck.type = "checkbox";
lockAspectCheck.checked = true;
lockAspectLabel.append(lockAspectCheck, document.createTextNode("Lock 16:9 aspect"));
const playAreaHint = document.createElement("p");
playAreaHint.className = cn.muted;
playAreaHint.textContent =
  "Green outline = playable ice. Resizing expands around the current center (faceoff stays put).";
const gridSizeInput = document.createElement("input");
gridSizeInput.type = "number";
gridSizeInput.min = "1";
gridSizeInput.step = "1";
gridSizeInput.className = cn.input;
const gridHint = document.createElement("p");
gridHint.className = cn.muted;
gridHint.textContent =
  "Minor lines every N units. Snap uses N × 8 major cells (9 anchors each: corners, edge midpoints, center).";
settingsForm.append(
  makeField("Play area width", playWidthInput),
  makeField("Play area height", playHeightInput),
  lockAspectLabel,
  makeButton("Apply play area", () => {
    let width = Math.max(320, Number(playWidthInput.value) || 1920);
    let height = Math.max(180, Number(playHeightInput.value) || 1080);
    if (lockAspectCheck.checked) {
      height = Math.round(width * (9 / 16));
      playHeightInput.value = String(height);
    }
    editor.setPlayAreaSize(width, height);
  }, { block: true }),
  playAreaHint,
  makeField("Minor grid size", gridSizeInput),
  gridHint,
  makeButton("Apply grid", () => {
    editor.applyMapSettings({
      gridSize: Math.max(1, Number(gridSizeInput.value) || 8),
    });
  }, { block: true }),
);
settingsPanel.body.append(settingsForm);

const tilePanel = makePanel("Tile picker", true);
const tileControls = document.createElement("div");
tileControls.className = "space-y-2";

const useTileLabel = document.createElement("label");
useTileLabel.className = cn.checkRow;
const useTileCheck = document.createElement("input");
useTileCheck.type = "checkbox";
useTileCheck.checked = editor.useTileSrc;
useTileCheck.onchange = () => {
  editor.useTileSrc = useTileCheck.checked;
};
useTileLabel.append(useTileCheck, document.createTextNode("Use tile region when placing"));

const snapSheetLabel = document.createElement("label");
snapSheetLabel.className = cn.checkRow;
const snapSheetCheck = document.createElement("input");
snapSheetCheck.type = "checkbox";
snapSheetCheck.checked = true;
snapSheetLabel.append(snapSheetCheck, document.createTextNode("Snap drag to sheet grid"));

const cellSizeField = makeNumberField("Cell size", 64, (v) => {
  const sheet = editor.getActiveSheet();
  if (sheet) editor.tileCellSizes[sheet] = Math.max(1, v);
  drawTilePicker();
});

const srcFieldsWrap = document.createElement("div");
srcFieldsWrap.className = cn.fieldGrid;
const srcFields: Record<string, HTMLInputElement> = {};
for (const [key, label] of [
  ["sx", "Src X"],
  ["sy", "Src Y"],
  ["sw", "Src W"],
  ["sh", "Src H"],
] as const) {
  const input = document.createElement("input");
  input.type = "number";
  input.step = "1";
  srcFields[key] = input;
  srcFieldsWrap.append(makeField(label, input));
}

tileControls.append(
  useTileLabel,
  snapSheetLabel,
  cellSizeField,
  srcFieldsWrap,
  makeButton("Apply src rect", () => {
    const sheet = editor.getActiveSheet();
    const img = sheet ? editor.getAsset(sheet) : null;
    if (!sheet || !img) return;
    editor.setActiveSrc(
      sheet,
      clampSrcRect(img.width, img.height, {
        x: Number(srcFields.sx.value) || 0,
        y: Number(srcFields.sy.value) || 0,
        w: Number(srcFields.sw.value) || 1,
        h: Number(srcFields.sh.value) || 1,
      }),
    );
    drawTilePicker();
  }, { block: true }),
);

const tilePickerWrap = document.createElement("div");
tilePickerWrap.className = cn.tileWrap;
const tileCanvas = document.createElement("canvas");
tileCanvas.id = "tile-picker";
tileCanvas.className = cn.tileCanvas;
tilePickerWrap.append(tileCanvas);
const srcInfo = document.createElement("div");
srcInfo.className = cn.muted;
srcInfo.textContent = "Click a cell or drag a region on the sheet.";
tilePanel.body.append(tileControls, tilePickerWrap, srcInfo);

const propertiesPanel = makePanel("Selection properties", false);
const propertiesBody = document.createElement("div");
propertiesBody.className = "space-y-2";
propertiesPanel.body.append(propertiesBody);

const rightHint = document.createElement("p");
rightHint.className = cn.hint;
rightHint.innerHTML =
  'Save JSON to <code class="rounded bg-slate-800 px-1 py-0.5 text-slate-300">game/maps/default.json</code>, then rebuild the game.';

sidebarRight.append(settingsPanel.el, tilePanel.el, propertiesPanel.el, rightHint);

const backgroundPanel = makePanel("Background", true);
const backgroundForm = document.createElement("div");
backgroundForm.className = "space-y-3";

const bgColorEnabled = document.createElement("input");
bgColorEnabled.type = "checkbox";
bgColorEnabled.checked = false;
const bgColorRow = document.createElement("label");
bgColorRow.className = cn.checkRow;
const bgColorPicker = document.createElement("input");
bgColorPicker.type = "color";
bgColorPicker.value = "#0b0f16";
bgColorPicker.className = "h-8 w-12 cursor-pointer rounded border border-slate-600 bg-slate-900";
bgColorRow.append(bgColorEnabled, document.createTextNode("Fill color"), bgColorPicker);

const bgImagePath = document.createElement("input");
bgImagePath.type = "text";
bgImagePath.className = cn.input;
bgImagePath.placeholder = "maps/backgrounds/arena.png";

const bgRepeat = document.createElement("input");
bgRepeat.type = "checkbox";
const bgRepeatRow = document.createElement("label");
bgRepeatRow.className = cn.checkRow;
bgRepeatRow.append(bgRepeat, document.createTextNode("Tile image to fill area"));

const bgFileInput = document.createElement("input");
bgFileInput.type = "file";
bgFileInput.accept = "image/*";
bgFileInput.className = "hidden";
bgFileInput.onchange = () => {
  const file = bgFileInput.files?.[0];
  if (!file) return;
  editor.setBackgroundImageFile(file);
  bgImagePath.value = `maps/backgrounds/${file.name}`;
  refreshBackgroundForm();
  bgFileInput.value = "";
};

const bgHint = document.createElement("p");
bgHint.className = cn.muted;
bgHint.textContent =
  "Image path is under game resources/ (copy uploads to resources/maps/backgrounds/ for in-game use). Default area = camera bounds.";

backgroundForm.append(
  bgColorRow,
  makeField("Image path", bgImagePath),
  bgRepeatRow,
  makeButton("Choose image…", () => bgFileInput.click(), { block: true }),
  bgFileInput,
  makeButton("Fit to camera bounds", () => editor.fitBackgroundToCamera(), { block: true }),
  makeButton("Apply background", () => {
    const bg = {
      ...(bgColorEnabled.checked ? { color: bgColorPicker.value } : {}),
      ...(bgImagePath.value.trim() ? { image: bgImagePath.value.trim() } : {}),
      repeat: bgRepeat.checked,
      dest: editor.getBackground()?.dest ?? undefined,
    };
    if (!bg.color && !bg.image) {
      editor.applyBackground(null);
    } else {
      editor.applyBackground(bg);
    }
    refreshBackgroundForm();
  }, { block: true }),
  makeButton("Clear background", () => {
    editor.applyBackground(null);
    refreshBackgroundForm();
  }, { block: true, danger: true }),
  bgHint,
);
backgroundPanel.body.append(backgroundForm);

function refreshBackgroundForm() {
  const bg = editor.getBackground();
  bgColorEnabled.checked = Boolean(bg?.color);
  bgColorPicker.value = bg?.color?.startsWith("#") ? bg.color : bgColorPicker.value;
  bgImagePath.value = bg?.image ?? "";
  bgRepeat.checked = bg?.repeat === true;
}

sidebarRight.insertBefore(backgroundPanel.el, settingsPanel.el);

function refreshSettingsForm() {
  const pb = editor.map.player_bounds;
  playWidthInput.value = String(Math.round(pb.w));
  playHeightInput.value = String(Math.round(pb.h));
  gridSizeInput.value = String(editor.gridSize);
}

function appendTagEditor() {
  if (!editor.selectionSupportsTag()) return;
  const tagInput = document.createElement("input");
  tagInput.type = "text";
  tagInput.className = cn.input;
  tagInput.value = editor.getSelectionTag() ?? "";
  tagInput.placeholder = "Label shown on canvas";
  propertiesBody.append(
    makeField("Tag", tagInput),
    makeButton(
      "Apply tag",
      () => {
        editor.setSelectionTag(tagInput.value);
      },
      { block: true },
    ),
  );
}

function refreshLockButton() {
  if (!editor.selectionSupportsLock()) {
    lockButton.disabled = true;
    lockButton.textContent = "Lock";
    return;
  }
  lockButton.disabled = false;
  const state = editor.getSelectionLockState();
  if (state === "all") lockButton.textContent = "Unlock";
  else if (state === "mixed") lockButton.textContent = "Lock all";
  else lockButton.textContent = "Lock";
}

function refreshObjectList() {
  objectListBody.replaceChildren();
  const entries = editor.listMapObjects();
  if (entries.length === 0) {
    const empty = document.createElement("p");
    empty.className = cn.muted;
    empty.textContent = "No objects on this map yet.";
    objectListBody.append(empty);
    return;
  }

  let lastCategory = "";
  for (const entry of entries) {
    if (entry.category !== lastCategory) {
      const heading = document.createElement("div");
      heading.className = cn.subheading;
      heading.textContent = entry.category;
      objectListBody.append(heading);
      lastCategory = entry.category;
    }
    const btn = makeButton(
      `${entry.locked ? "🔒 " : ""}${entry.name}`,
      () => {},
      { block: true, active: editor.isSelected(entry.selection) },
    );
    btn.addEventListener("click", (e) => {
      editor.selectFromList(entry.selection, {
        toggle: e.ctrlKey || e.metaKey,
        add: e.shiftKey && !e.ctrlKey && !e.metaKey,
      });
    });
    if (entry.locked) btn.classList.add("me-btn--locked");
    objectListBody.append(btn);
  }
}

function refreshSelectionPanels() {
  selectionSummary.textContent = editor.selectionSummary();
  refreshLockButton();
  refreshObjectList();
  propertiesBody.innerHTML = "";

  const showZ = editor.selectionHasZOrder();
  zOrderSection.hidden = !showZ;
  if (showZ) {
    const z = editor.selectionZ();
    zInput.value = z === null ? "" : String(z);
    zInput.placeholder = z === null ? "mixed" : "";
  }

  appendTagEditor();

  const colorShape = editor.getSelectedShape();
  if (colorShape) {
    propertiesPanel.el.open = true;
    selectionHint.textContent = `Editing color ${colorShape.kind}`;
    const fillInput = document.createElement("input");
    fillInput.type = "text";
    fillInput.className = cn.input;
    fillInput.value = colorShape.fill ?? "";
    const strokeInput = document.createElement("input");
    strokeInput.type = "text";
    strokeInput.className = cn.input;
    strokeInput.value = colorShape.stroke ?? "";
    const widthInput = document.createElement("input");
    widthInput.type = "number";
    widthInput.min = "1";
    widthInput.max = "32";
    widthInput.className = cn.input;
    widthInput.value = String(colorShape.strokeWidth ?? 2);
    const fields = [
      makeField("Fill (CSS color)", fillInput),
      makeField("Stroke (CSS color)", strokeInput),
      makeField("Stroke width", widthInput),
    ];
    if (colorShape.kind === "rect") {
      const cornerInput = document.createElement("input");
      cornerInput.type = "number";
      cornerInput.min = "0";
      cornerInput.step = "1";
      cornerInput.className = cn.input;
      cornerInput.value = String(colorShape.cornerRadius ?? 0);
      fields.push(makeField("Corner radius", cornerInput));
      propertiesBody.append(...fields);
      propertiesBody.append(
        makeButton(
          "Apply",
          () => {
            editor.updateSelectedShape({
              fill: fillInput.value.trim() || undefined,
              stroke: strokeInput.value.trim() || undefined,
              strokeWidth: Math.max(1, Number(widthInput.value) || 2),
              cornerRadius: Math.max(0, Number(cornerInput.value) || 0),
            });
          },
          { block: true },
        ),
      );
    } else {
      propertiesBody.append(...fields);
      propertiesBody.append(
        makeButton(
          "Apply colors",
          () => {
            editor.updateSelectedShape({
              fill: fillInput.value.trim() || undefined,
              stroke: strokeInput.value.trim() || undefined,
              strokeWidth: Math.max(1, Number(widthInput.value) || 2),
            });
          },
          { block: true },
        ),
      );
    }
    return;
  }

  const collider = editor.getSelectedCollider();
  if (collider?.kind === "rect") {
    propertiesPanel.el.open = true;
    selectionHint.textContent = "Editing blocker rect — drag magenta handle for roundness";
    const cornerInput = document.createElement("input");
    cornerInput.type = "number";
    cornerInput.min = "0";
    cornerInput.step = "1";
    cornerInput.className = cn.input;
    cornerInput.value = String(collider.cornerRadius ?? 0);
    propertiesBody.append(
      makeField("Corner radius", cornerInput),
      makeButton(
        "Apply",
        () => {
          editor.updateSelectedCollider({
            cornerRadius: Math.max(0, Number(cornerInput.value) || 0),
          });
        },
        { block: true },
      ),
    );
    return;
  }

  const piece = editor.getSelectedVisual();
  if (!piece) {
    if (editor.selectionSupportsTag()) {
      propertiesPanel.el.open = true;
      selectionHint.textContent = "Edit the tag label shown on the canvas (Logic overlay).";
    } else {
      propertiesPanel.el.open = false;
      if (editor.selections.length > 1) {
        selectionHint.textContent = "Multiple items selected — drag to move together.";
      } else {
        selectionHint.textContent =
          "Drag to move (snaps to grid). Single selection: drag green handles to edit points.";
      }
    }
    return;
  }

  propertiesPanel.el.open = true;
  selectionHint.textContent = `Editing visual: ${piece.id}`;

  const src = piece.src ?? { x: 0, y: 0, w: 0, h: 0 };
  const draft = { ...src };
  for (const [label, key] of [
    ["Src X", "x"],
    ["Src Y", "y"],
    ["Src W", "w"],
    ["Src H", "h"],
  ] as [string, keyof Rect][]) {
    propertiesBody.append(
      makeNumberField(label, src[key], (v) => {
        draft[key] = v;
      }, "1"),
    );
  }
  propertiesBody.append(
    makeButton("Apply source", () => {
      editor.updateSelectedVisual({ use_src: true, src: draft });
      drawTilePicker();
    }, { block: true }),
    makeButton("Flip X", () => {
      editor.updateSelectedVisual({ flip_x: !piece.flip_x });
    }, { block: true }),
  );
}

let tileDrag: { x0: number; y0: number; x1: number; y1: number } | null = null;
let tileHover: Rect | null = null;

const TILE_PICKER_MAX_W = 220;

/** Max preview height when space allows; otherwise we shrink to fit the viewport. */
const TILE_PICKER_PREFERRED_MAX_H = 280;

function tilePickerLimits(): { maxW: number; maxH: number } {
  const sidebarPad = 24;
  const maxW = Math.min(
    TILE_PICKER_MAX_W,
    Math.max(120, sidebarRight.clientWidth - sidebarPad),
  );

  const wrapTop = tilePickerWrap.getBoundingClientRect().top;
  const statusTop = status.getBoundingClientRect().top;
  const srcInfoH = srcInfo.offsetHeight || 32;
  const gap = 12;
  const available = statusTop - wrapTop - srcInfoH - gap;
  const maxH = Math.max(96, Math.min(TILE_PICKER_PREFERRED_MAX_H, available));

  return { maxW, maxH };
}

function tilePickerScale(img: HTMLImageElement): number {
  const { maxW, maxH } = tilePickerLimits();
  return Math.min(maxW / img.width, maxH / img.height);
}

function texFromCanvasEvent(e: MouseEvent): { x: number; y: number } | null {
  const sheet = editor.getActiveSheet();
  const img = sheet ? editor.getAsset(sheet) : null;
  if (!sheet || !img) return null;
  const rect = tileCanvas.getBoundingClientRect();
  if (rect.width <= 0 || rect.height <= 0) return null;
  const inside =
    e.clientX >= rect.left &&
    e.clientX <= rect.right &&
    e.clientY >= rect.top &&
    e.clientY <= rect.bottom;
  if (!inside) return null;
  const u = (e.clientX - rect.left) / rect.width;
  const v = (e.clientY - rect.top) / rect.height;
  // Stay inside the last cell when touching the bottom/right edge.
  const x = Math.min(img.width - 1e-6, Math.max(0, u * img.width));
  const y = Math.min(img.height - 1e-6, Math.max(0, v * img.height));
  return { x, y };
}

function syncSrcFields(sheet: SheetKey, src: Rect | undefined) {
  if (!src) return;
  srcFields.sx.value = String(Math.round(src.x));
  srcFields.sy.value = String(Math.round(src.y));
  srcFields.sw.value = String(Math.round(src.w));
  srcFields.sh.value = String(Math.round(src.h));
}

function drawTilePicker() {
  const sheet = editor.getActiveSheet();
  const ctx = tileCanvas.getContext("2d")!;
  ctx.clearRect(0, 0, tileCanvas.width, tileCanvas.height);

  if (!sheet) {
    srcInfo.textContent = "Choose a visual tool to pick tiles.";
    tilePickerWrap.classList.add("hidden");
    tilePanel.el.open = false;
    return;
  }

  tilePanel.el.open = true;
  tilePickerWrap.classList.remove("hidden");
  const img = editor.getAsset(sheet);
  if (!img) {
    srcInfo.textContent = `Loading ${sheet}.png…`;
    return;
  }

  const cell = editor.tileCellSizes[sheet] ?? DEFAULT_TILE_CELL[sheet];
  const cellInput = cellSizeField.querySelector("input") as HTMLInputElement;
  cellInput.value = String(cell);

  const scale = tilePickerScale(img);
  const dispW = Math.max(1, Math.round(img.width * scale));
  const dispH = Math.max(1, Math.round(img.height * scale));
  tileCanvas.width = dispW;
  tileCanvas.height = dispH;
  tileCanvas.style.width = `${dispW}px`;
  tileCanvas.style.height = `${dispH}px`;

  ctx.drawImage(img, 0, 0, dispW, dispH);
  ctx.strokeStyle = "rgba(255,255,255,0.12)";
  ctx.lineWidth = 1;
  const cellDisp = cell * scale;
  for (let x = 0; x <= dispW; x += cellDisp) {
    ctx.beginPath();
    ctx.moveTo(x, 0);
    ctx.lineTo(x, dispH);
    ctx.stroke();
  }
  for (let y = 0; y <= dispH; y += cellDisp) {
    ctx.beginPath();
    ctx.moveTo(0, y);
    ctx.lineTo(dispW, y);
    ctx.stroke();
  }

  const preview =
    tileDrag != null
      ? pickRegion(
          img.width,
          img.height,
          tileDrag.x0,
          tileDrag.y0,
          tileDrag.x1,
          tileDrag.y1,
          cell,
          snapSheetCheck.checked,
        )
      : null;

  if (tileHover && !preview) {
    ctx.fillStyle = "rgba(110,161,255,0.14)";
    ctx.fillRect(tileHover.x * scale, tileHover.y * scale, tileHover.w * scale, tileHover.h * scale);
    ctx.strokeStyle = "#6ea1ff";
    ctx.lineWidth = 2;
    ctx.strokeRect(tileHover.x * scale, tileHover.y * scale, tileHover.w * scale, tileHover.h * scale);
    const hx = (tileHover.x + tileHover.w * 0.5) * scale;
    const hy = (tileHover.y + tileHover.h * 0.5) * scale;
    ctx.fillStyle = "#6ea1ff";
    ctx.beginPath();
    ctx.arc(hx, hy, 3, 0, Math.PI * 2);
    ctx.fill();
  }

  const active = preview ?? editor.activeSrc[sheet];
  if (active) {
    ctx.fillStyle = preview ? "rgba(255,209,102,0.2)" : "rgba(255,209,102,0.12)";
    ctx.fillRect(active.x * scale, active.y * scale, active.w * scale, active.h * scale);
    ctx.strokeStyle = preview ? "#6ea1ff" : "#ffd166";
    ctx.lineWidth = 2;
    ctx.strokeRect(
      active.x * scale,
      active.y * scale,
      active.w * scale,
      active.h * scale,
    );
    srcInfo.textContent = preview
      ? `${sheet}: selecting ${active.w}×${active.h}px`
      : `${sheet}: src (${active.x}, ${active.y}) ${active.w}×${active.h}px · sheet ${img.width}×${img.height}`;
    syncSrcFields(sheet, active);
  } else {
    srcInfo.textContent = `Click a cell or drag a region · sheet ${img.width}×${img.height}px`;
  }
}

function refreshTilePicker() {
  drawTilePicker();
  requestAnimationFrame(() => drawTilePicker());
  refreshSelectionPanels();
}

tileCanvas.addEventListener("mousedown", (e) => {
  if (e.button !== 0) return;
  const pt = texFromCanvasEvent(e);
  if (!pt) return;
  tileDrag = { x0: pt.x, y0: pt.y, x1: pt.x, y1: pt.y };
  drawTilePicker();
});

tileCanvas.addEventListener("mousemove", (e) => {
  const pt = texFromCanvasEvent(e);
  const sheet = editor.getActiveSheet();
  const img = sheet ? editor.getAsset(sheet) : null;
  if (tileDrag) {
    if (!pt) return;
    tileDrag.x1 = pt.x;
    tileDrag.y1 = pt.y;
    drawTilePicker();
    return;
  }
  if (!pt || !sheet || !img) {
    tileHover = null;
    drawTilePicker();
    return;
  }
  const cell = editor.tileCellSizes[sheet] ?? DEFAULT_TILE_CELL[sheet];
  tileHover = pickCellContaining(img.width, img.height, pt.x, pt.y, cell);
  drawTilePicker();
});

tileCanvas.addEventListener("mouseleave", () => {
  tileHover = null;
  drawTilePicker();
});

function commitTileDrag() {
  if (!tileDrag) return;
  const sheet = editor.getActiveSheet();
  const img = sheet ? editor.getAsset(sheet) : null;
  if (!sheet || !img) {
    tileDrag = null;
    return;
  }
  const cell = editor.tileCellSizes[sheet] ?? DEFAULT_TILE_CELL[sheet];
  const rect = tileCanvas.getBoundingClientRect();
  const dragPx = Math.hypot(
    ((tileDrag.x1 - tileDrag.x0) / img.width) * rect.width,
    ((tileDrag.y1 - tileDrag.y0) / img.height) * rect.height,
  );
  const src =
    dragPx < 4
      ? pickCellContaining(img.width, img.height, tileDrag.x0, tileDrag.y0, cell)
      : pickRegion(
          img.width,
          img.height,
          tileDrag.x0,
          tileDrag.y0,
          tileDrag.x1,
          tileDrag.y1,
          cell,
          snapSheetCheck.checked,
        );
  editor.setActiveSrc(sheet, src);
  tileDrag = null;
  drawTilePicker();
}

tileCanvas.addEventListener("mouseup", commitTileDrag);
window.addEventListener("mouseup", commitTileDrag);

function refreshViewToggles() {
  setButtonActive(gridToggle, editor.showGrid);
  setButtonActive(logicToggle, editor.showLogic);
}

function refreshCornerRadiusPanel() {
  const show =
    editor.drawShape === "square" &&
    (editor.tool === "collider" || editor.tool === "color_shape");
  cornerRadiusWrap.classList.toggle("hidden", !show);
  if (show) cornerRadiusInput.value = String(editor.cornerRadius);
}

function refreshColorPanel() {
  const active = editor.tool === "color_shape";
  colorPanel.el.open = active;
  colorForm.querySelectorAll("input").forEach((input) => {
    input.disabled = !active;
  });
  if (active) syncDrawColorsFromEditor();
}

function refreshToolPicker() {
  document.querySelectorAll("#sidebar-left .tool-btn").forEach((el) => {
    const button = el as HTMLButtonElement;
    setButtonActive(button, button.dataset.tool === editor.tool);
  });
}

function refreshShapePicker() {
  document.querySelectorAll("#sidebar-left .shape-btn").forEach((el) => {
    const button = el as HTMLButtonElement;
    const shape = button.dataset.shape as DrawShape | undefined;
    if (!shape) return;
    const disabled = editor.tool === "select" || !editor.shapeSupported(shape);
    button.disabled = disabled;
    button.title =
      disabled && editor.tool !== "select" ? `${shape} not available for ${editor.tool}` : "";
    setButtonActive(button, !disabled && editor.drawShape === shape);
  });
}

editor.onMapChange = () => {
  refreshSettingsForm();
  refreshBackgroundForm();
  refreshViewToggles();
  refreshToolPicker();
  refreshCornerRadiusPanel();
  refreshColorPanel();
  refreshTilePicker();
  refreshShapePicker();
  refreshSelectionPanels();
};

refreshSettingsForm();
refreshBackgroundForm();
refreshViewToggles();
applyDrawColorsToEditor();
refreshToolPicker();
refreshCornerRadiusPanel();
refreshColorPanel();
refreshShapePicker();
refreshTilePicker();

const tilePickerResizeObserver = new ResizeObserver(() => {
  if (editor.getActiveSheet()) drawTilePicker();
});
tilePickerResizeObserver.observe(sidebarRight);
window.addEventListener("resize", () => {
  if (editor.getActiveSheet()) drawTilePicker();
});
