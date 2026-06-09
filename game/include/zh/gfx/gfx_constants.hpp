#pragma once

#include "zh/gfx/gfx_types.hpp"

// Color and key constants for the gfx layer.

inline constexpr zh::gfx::Color WHITE{255, 255, 255, 255};
inline constexpr zh::gfx::Color BLACK{0, 0, 0, 255};
inline constexpr zh::gfx::Color OFF_WHITE{245, 245, 245, 255};
inline constexpr zh::gfx::Color LIGHTGRAY{200, 200, 200, 255};
inline constexpr zh::gfx::Color GRAY{130, 130, 130, 255};
inline constexpr zh::gfx::Color DARKGRAY{80, 80, 80, 255};
inline constexpr zh::gfx::Color MAROON{190, 33, 55, 255};
inline constexpr zh::gfx::Color DARKBLUE{0, 82, 172, 255};
inline constexpr zh::gfx::Color DARKGREEN{0, 117, 44, 255};
inline constexpr zh::gfx::Color GOLD{255, 203, 0, 255};
inline constexpr zh::gfx::Color SKYBLUE{102, 191, 255, 255};

inline constexpr int KEY_NULL = 0;
inline constexpr int KEY_APOSTROPHE = 39;
inline constexpr int KEY_COMMA = 44;
inline constexpr int KEY_MINUS = 45;
inline constexpr int KEY_PERIOD = 46;
inline constexpr int KEY_SLASH = 47;
inline constexpr int KEY_ZERO = 48;
inline constexpr int KEY_NINE = 57;
inline constexpr int KEY_SEMICOLON = 59;
inline constexpr int KEY_EQUAL = 61;
inline constexpr int KEY_A = 65;
inline constexpr int KEY_B = 66;
inline constexpr int KEY_C = 67;
inline constexpr int KEY_D = 68;
inline constexpr int KEY_G = 71;
inline constexpr int KEY_Q = 81;
inline constexpr int KEY_E = 69;
inline constexpr int KEY_R = 82;
inline constexpr int KEY_F3 = 292;
inline constexpr int KEY_S = 83;
inline constexpr int KEY_W = 87;
inline constexpr int KEY_X = 88;
inline constexpr int KEY_Z = 90;
inline constexpr int KEY_SPACE = 32;
inline constexpr int KEY_LEFT_SHIFT = 340;
inline constexpr int KEY_LEFT_CONTROL = 341;
inline constexpr int KEY_ESCAPE = 256;
inline constexpr int KEY_BACKSPACE = 259;
inline constexpr int KEY_TAB = 258;
inline constexpr int KEY_UP = 265;
inline constexpr int KEY_DOWN = 262;
inline constexpr int KEY_LEFT = 263;
inline constexpr int KEY_RIGHT = 264;
inline constexpr int KEY_LEFT_BRACKET = 91;
inline constexpr int KEY_RIGHT_BRACKET = 93;

inline constexpr int MOUSE_BUTTON_LEFT = 0;
inline constexpr int MOUSE_BUTTON_RIGHT = 1;
inline constexpr int MOUSE_BUTTON_MIDDLE = 2;

inline constexpr int LOG_INFO = 3;
inline constexpr int LOG_WARNING = 4;

inline constexpr int CAMERA_PERSPECTIVE = 0;
inline constexpr int CAMERA_ORTHOGRAPHIC = 1;

inline constexpr float PI = 3.14159265358979323846f;
inline constexpr float DEG2RAD = PI / 180.f;
inline constexpr float RAD2DEG = 180.f / PI;

inline constexpr unsigned FLAG_WINDOW_RESIZABLE = 0x00000004U;
inline constexpr unsigned FLAG_MSAA_4X_HINT = 0x00000020U;
inline constexpr unsigned FLAG_VSYNC_HINT = 0x00000040U;
inline constexpr unsigned FLAG_WINDOW_HIGHDPI = 0x00002000U;
