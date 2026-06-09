#pragma once

// Shared value types for game/UI drawing code.

namespace zh::gfx {

struct Color {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
};

struct Vector2 {
    float x;
    float y;
};

struct Vector3 {
    float x;
    float y;
    float z;
};

struct Rectangle {
    float x;
    float y;
    float width;
    float height;
};

struct Camera2D {
    Vector2 offset{};
    Vector2 target{};
    float rotation = 0.f;
    float zoom = 1.f;
};

struct Camera3D {
    Vector3 position{};
    Vector3 target{};
    Vector3 up{};
    float fovy = 45.f;
    int projection = 0;
};

struct Ray {
    Vector3 position{};
    Vector3 direction{};
};

}  // namespace zh::gfx
