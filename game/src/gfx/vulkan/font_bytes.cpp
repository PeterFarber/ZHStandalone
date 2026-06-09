#include "zh/gfx/vulkan/font_bytes.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>

#if defined(ZH_HAVE_WOFF2)
#include <woff2/decode.h>
#endif

namespace zh::gfx::vk {

namespace {

[[nodiscard]] bool load_file_bytes(char const *path, std::vector<unsigned char> &out) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return false;
    }
    auto const size = file.tellg();
    if (size <= 0) {
        return false;
    }
    out.resize(static_cast<std::size_t>(size));
    file.seekg(0);
    file.read(reinterpret_cast<char *>(out.data()), size);
    return file.good();
}

[[nodiscard]] std::string lower_extension(std::filesystem::path const &path) {
    auto ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

#if defined(ZH_HAVE_WOFF2)
[[nodiscard]] bool decode_woff2(std::vector<unsigned char> const &woff2,
                                std::vector<unsigned char> &ttf_out) {
    if (woff2.empty()) {
        return false;
    }
    size_t const final_size =
        woff2::ComputeWOFF2FinalSize(woff2.data(), woff2.size());
    if (final_size == 0) {
        return false;
    }
    ttf_out.resize(final_size);
    return woff2::ConvertWOFF2ToTTF(ttf_out.data(), final_size, woff2.data(), woff2.size());
}
#endif

}  // namespace

bool load_font_bytes(char const *path, std::vector<unsigned char> &out) {
    if (path == nullptr) {
        return false;
    }

    std::vector<unsigned char> raw;
    if (!load_file_bytes(path, raw)) {
        return false;
    }

    if (lower_extension(std::filesystem::path(path)) == ".woff2") {
#if defined(ZH_HAVE_WOFF2)
        return decode_woff2(raw, out);
#else
        (void)raw;
        return false;
#endif
    }

    out = std::move(raw);
    return !out.empty();
}

}  // namespace zh::gfx::vk
