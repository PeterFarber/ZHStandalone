#include "zh/platform/paths_win32.hpp"

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
#include <Windows.h>
#endif

namespace zh::platform {

char const *application_directory() noexcept {
    static char path[512];
#if defined(_WIN32)
    DWORD const n = GetModuleFileNameA(nullptr, path, static_cast<DWORD>(sizeof(path)));
    if (n > 0 && n < sizeof(path)) {
        for (int i = static_cast<int>(n) - 1; i >= 0; --i) {
            if (path[i] == '\\' || path[i] == '/') {
                path[i + 1] = '\0';
                break;
            }
        }
    }
#else
    path[0] = '/';
    path[1] = '\0';
#endif
    return path;
}

}  // namespace zh::platform
