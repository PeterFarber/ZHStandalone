#pragma once

// WinSock before Windows.h (Vulkan / ENet TUs).

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef NOGDI
#define NOGDI
#endif

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>

// Undef Win32 macros that clash with gfx API names.
#undef CloseWindow
#undef ShowCursor
#undef LoadImage
#undef LoadImageA
#undef LoadImageW
#undef DrawText
#undef DrawTextA
#undef DrawTextW
#undef DrawTextEx
#undef DrawTextExA
#undef DrawTextExW
#undef PlaySoundA
#endif
