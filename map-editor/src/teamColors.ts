/** Team A (west / left) — matches spawn markers in the editor. */
export const TEAM_A_COLOR = "#4dabff";
/** Team B (east / right) — matches spawn markers in the editor. */
export const TEAM_B_COLOR = "#ff6b6b";

export function westGoalColor(): string {
  return TEAM_A_COLOR;
}

export function eastGoalColor(): string {
  return TEAM_B_COLOR;
}

export const DEFAULT_WEST_GOAL_TAG = "Team A goal (west)";
export const DEFAULT_EAST_GOAL_TAG = "Team B goal (east)";
