#pragma once

// Runtime path helpers so maps/resources load whether cwd is bin/, game/, or repo root.

namespace zh::detail {

// Find a directory that contains resources/ and chdir there so relative paths work.
void ensure_resource_cwd() noexcept;

// Search cwd, exe dir, and a few parent paths for resources/<rel>.
[[nodiscard]] char const *resolve_resource_file(char const *rel) noexcept;

}  // namespace zh::detail
