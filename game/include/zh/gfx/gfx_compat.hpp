#pragma once

#include "zh/gfx/gfx_constants.hpp"
#include "zh/gfx/gfx_types.hpp"

// Global gfx aliases and draw API (Vulkan backend).

using Color = zh::gfx::Color;
using Vector2 = zh::gfx::Vector2;
using Vector3 = zh::gfx::Vector3;
using Rectangle = zh::gfx::Rectangle;
using Camera2D = zh::gfx::Camera2D;
using Camera3D = zh::gfx::Camera3D;
using Ray = zh::gfx::Ray;

#include "zh/gfx/gfx_projection.hpp"

using zh::gfx::GetScreenToWorld2D;
using zh::gfx::GetScreenToWorldRay;
using zh::gfx::GetWorldToScreen;
using zh::gfx::TryGetWorldToScreen;
using zh::gfx::ProjectWorldToScreenForPick;
using zh::gfx::Vector2Distance;
using zh::gfx::Vector3Add;
using zh::gfx::Vector3Scale;
using zh::gfx::Vector3Subtract;

void SetConfigFlags(unsigned flags);
void InitWindow(int width, int height, char const *title);
void CloseWindow();
bool WindowShouldClose();
void SetWindowMinSize(int min_w, int min_h);
void SetTargetFPS(int fps);
void PollInputEvents();

void BeginDrawing();
void EndDrawing();
void ClearBackground(Color color);
void DrawPlayingSkyGradient();
void DrawMenuBackgroundGradient();
void DrawFPS(int posX, int posY);

int GetScreenWidth();
int GetScreenHeight();
float GetFrameTime();
double GetTime();

bool IsKeyDown(int key);
bool IsKeyPressed(int key);
int GetCharPressed();
bool IsMouseButtonDown(int button);
bool IsMouseButtonPressed(int button);
Vector2 GetMousePosition();
float GetMouseWheelMove();
int GetRandomValue(int min, int max);

void DrawRectangle(int x, int y, int width, int height, Color color);
void DrawRectangleRec(Rectangle rec, Color color);
void DrawRectangleGradientV(int x, int y, int width, int height, Color top, Color bottom);
void DrawRectangleLinesEx(Rectangle rec, float line_thick, Color color);
void DrawLineEx(Vector2 start, Vector2 end, float thick, Color color);
void DrawCircle(int centerX, int centerY, float radius, Color color);
void DrawCircleV(Vector2 center, float radius, Color color);
void DrawCircleLines(int centerX, int centerY, float radius, Color color);
void DrawCircleLinesV(Vector2 center, float radius, Color color);
void DrawRing(Vector2 center, float innerRadius, float outerRadius, float startAngle,
              float endAngle, int segments, Color color);
void DrawText(char const *text, int posX, int posY, int fontSize, Color color);
int MeasureText(char const *text, int fontSize);

bool CheckCollisionPointRec(Vector2 point, Rectangle rec);
Color ColorBrightness(Color color, float factor);
Color ColorFromHSV(float hue, float saturation, float value);
Vector3 ColorToHSV(Color color);

void BeginMode3D(Camera3D camera);
void EndMode3D();
void DrawCylinder(Vector3 position, float radiusTop, float radiusBottom, float height,
                  int slices, Color color);
void DrawCube(Vector3 position, float width, float height, float length, Color color);
void DrawTriangle3D(Vector3 v1, Vector3 v2, Vector3 v3, Color color);
void EndShaderMode();
void rlDisableBackfaceCulling();
void rlEnableBackfaceCulling();

bool DirectoryExists(char const *dir);
bool FileExists(char const *fileName);
void ChangeDirectory(char const *path);
char const *GetApplicationDirectory();
char const *GetWorkingDirectory();
void TraceLog(int logLevel, char const *text, ...);
