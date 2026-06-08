export const cn = {
  app: "grid h-full grid-cols-[16rem_minmax(0,1fr)_18rem] grid-rows-[auto_1fr_auto] bg-slate-950 text-slate-200",
  toolbar:
    "col-span-3 flex flex-wrap items-center gap-x-4 gap-y-2 border-b border-slate-800 bg-slate-900/90 px-4 py-2.5",
  toolbarGroup: "flex flex-wrap items-center gap-1.5",
  toolbarLabel:
    "mr-1 text-[10px] font-semibold uppercase tracking-wider text-slate-500",
  sidebar:
    "flex flex-col gap-2 overflow-y-auto border-slate-800 bg-slate-900/40 p-2.5",
  sidebarLeft: "border-r",
  sidebarRight: "border-l",
  statusBar:
    "col-span-3 truncate border-t border-slate-800 bg-slate-900/90 px-4 py-2 text-xs text-slate-400",
  canvasWrap: "relative min-w-0 overflow-hidden bg-[#0b0f16]",
  canvas: "block h-full w-full cursor-crosshair",
  panel:
    "rounded-lg border border-slate-700/80 bg-slate-900/60 shadow-sm shadow-black/20",
  panelSummary:
    "flex cursor-pointer list-none items-center justify-between px-3 py-2 text-[11px] font-semibold uppercase tracking-wide text-slate-400 select-none [&::-webkit-details-marker]:hidden",
  panelBody: "space-y-2 border-t border-slate-700/70 px-3 py-3",
  subheading: "mt-1 text-[10px] font-semibold uppercase tracking-wide text-slate-500",
  toolGrid: "grid grid-cols-2 gap-1.5",
  shapeGrid: "grid grid-cols-2 gap-1.5",
  fieldGrid: "grid grid-cols-2 gap-2",
  field: "flex flex-col gap-1 text-xs text-slate-300",
  fieldLabel: "text-[11px] text-slate-500",
  input:
    "rounded-md border border-slate-600 bg-slate-950 px-2 py-1.5 text-xs text-slate-100 outline-none focus:border-blue-500 focus:ring-1 focus:ring-blue-500/40",
  checkRow: "flex items-center gap-2 text-xs text-slate-300",
  summaryBox:
    "rounded-md border border-slate-700 bg-slate-950/70 px-2.5 py-2 text-xs leading-relaxed text-slate-300",
  hint:
    "rounded-lg border border-slate-800 bg-slate-950/50 px-3 py-2.5 text-[11px] leading-relaxed text-slate-500",
  actionRow: "grid grid-cols-2 gap-1.5",
  tileWrap:
    "rounded-md border border-slate-700 bg-[#0b0f16] p-1",
  tileCanvas: "block cursor-crosshair [image-rendering:pixelated]",
  muted: "text-[11px] leading-snug text-slate-500",
};

const BTN = "me-btn";
const BTN_ACTIVE = "me-btn--active";
const BTN_BLOCK = "me-btn--block";
const BTN_DANGER = "me-btn--danger";

export function setButtonActive(button: HTMLButtonElement, active: boolean) {
  button.classList.toggle(BTN_ACTIVE, active);
  button.setAttribute("aria-pressed", active ? "true" : "false");
}

export function makeButton(
  label: string,
  onClick: () => void,
  opts: { active?: boolean; block?: boolean; danger?: boolean } = {},
): HTMLButtonElement {
  const button = document.createElement("button");
  button.type = "button";
  button.textContent = label;
  button.dataset.block = opts.block ? "1" : "0";
  button.dataset.danger = opts.danger ? "1" : "0";
  button.className = BTN;
  if (opts.block) button.classList.add(BTN_BLOCK);
  if (opts.danger) button.classList.add(BTN_DANGER);
  if (opts.active) button.classList.add(BTN_ACTIVE);
  button.setAttribute("aria-pressed", opts.active ? "true" : "false");
  button.onclick = onClick;
  return button;
}

export function makePanel(
  title: string,
  open = true,
): { el: HTMLDetailsElement; body: HTMLElement } {
  const details = document.createElement("details");
  details.className = `${cn.panel} group`;
  if (open) details.open = true;

  const summary = document.createElement("summary");
  summary.className = cn.panelSummary;
  summary.innerHTML = `${title}<span class="text-slate-600 transition group-open:rotate-180">▾</span>`;

  const body = document.createElement("div");
  body.className = cn.panelBody;
  details.append(summary, body);
  return { el: details, body };
}

export function makeField(label: string, input: HTMLInputElement): HTMLLabelElement {
  const wrap = document.createElement("label");
  wrap.className = cn.field;
  const span = document.createElement("span");
  span.className = cn.fieldLabel;
  span.textContent = label;
  input.className = cn.input;
  wrap.append(span, input);
  return wrap;
}

export function makeNumberField(
  label: string,
  value: number,
  onChange: (v: number) => void,
  step = "any",
): HTMLLabelElement {
  const input = document.createElement("input");
  input.type = "number";
  input.step = step;
  input.value = String(value);
  input.onchange = () => onChange(Number(input.value));
  return makeField(label, input);
}

export function makeToolbarGroup(label: string, children: HTMLElement[]): HTMLElement {
  const wrap = document.createElement("div");
  wrap.className = cn.toolbarGroup;
  const lbl = document.createElement("span");
  lbl.className = cn.toolbarLabel;
  lbl.textContent = label;
  wrap.append(lbl, ...children);
  return wrap;
}
