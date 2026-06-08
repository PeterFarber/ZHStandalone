#include "detail/resource_paths.hpp"

#include <raylib.h>

#include <cstdio>

namespace zh::detail {

namespace {

[[nodiscard]] bool try_chdir_if_resources(char const *dir) noexcept {
    if (dir == nullptr || dir[0] == '\0') {
        return false;
    }
    char resources_dir[512];
    int const n = std::snprintf(resources_dir, sizeof(resources_dir), "%sresources", dir);
    if (n <= 0 || static_cast<std::size_t>(n) >= sizeof(resources_dir)) {
        return false;
    }
    if (!DirectoryExists(resources_dir)) {
        return false;
    }
    ChangeDirectory(dir);
    return true;
}

}  // namespace

void ensure_resource_cwd() noexcept {
    if (DirectoryExists("resources")) {
        return;
    }

    // Release build: exe sits in bin/Release/ with resources/ copied beside it.
    // Dev build: sometimes cwd is game/ or repo root — walk up a few levels.
    char const *const app_dir = GetApplicationDirectory();
    if (try_chdir_if_resources(app_dir)) {
        return;
    }

    char candidate[512];
    if (std::snprintf(candidate, sizeof(candidate), "%s..", app_dir) > 0 &&
        try_chdir_if_resources(candidate)) {
        return;
    }
    if (std::snprintf(candidate, sizeof(candidate), "%s../..", app_dir) > 0 &&
        try_chdir_if_resources(candidate)) {
        return;
    }
    if (std::snprintf(candidate, sizeof(candidate), "%s../../..", app_dir) > 0) {
        (void)try_chdir_if_resources(candidate);
    }
}

char const *resolve_resource_file(char const *rel) noexcept {
    static char path[512];
    if (rel == nullptr || rel[0] == '\0') {
        return nullptr;
    }

    auto try_path = [&](char const *fmt, auto... args) -> char const * {
        int const n = std::snprintf(path, sizeof(path), fmt, args...);
        if (n > 0 && static_cast<std::size_t>(n) < sizeof(path) && FileExists(path)) {
            return path;
        }
        return nullptr;
    };

    if (char const *hit = try_path("resources/%s", rel)) {
        return hit;
    }

    char const *const app_dir = GetApplicationDirectory();
    if (char const *hit = try_path("%sresources/%s", app_dir, rel)) {
        return hit;
    }
    if (char const *hit = try_path("%s../resources/%s", app_dir, rel)) {
        return hit;
    }
    if (char const *hit = try_path("%s../../resources/%s", app_dir, rel)) {
        return hit;
    }
    if (char const *hit = try_path("../../resources/%s", rel)) {
        return hit;
    }

    TraceLog(LOG_WARNING,
             "[zh] Resource not found: resources/%s (cwd=%s exe=%s)",
             rel,
             GetWorkingDirectory(),
             app_dir);
    return nullptr;
}

}  // namespace zh::detail
