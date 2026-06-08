#pragma once

// Include this before raylib on Windows when a TU also needs WinSock or Windows.h.
//
// Order matters: WinSock2 before Windows.h. We temporarily rename a few Win32
// symbols that clash with raylib (CloseWindow, DrawText, …), then #undef them.
//
// NOGDI avoids fighting GDI's Rectangle type — renaming GDI Rectangle instead
// caused weird presentation glitches on some machines.

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

#define CloseWindow CloseWindowWin32
#define ShowCursor ShowCursorWin32
#define LoadImageA LoadImageAWin32
#define LoadImageW LoadImageWin32
#define DrawTextA DrawTextAWin32
#define DrawTextW DrawTextWin32
#define DrawTextExA DrawTextExAWin32
#define DrawTextExW DrawTextExWin32
#define PlaySoundA PlaySoundAWin32

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>

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
