#pragma once

#include <string>

namespace evolab::engine::gfx {

inline constexpr const char* kDefaultUiFontRelPath = "assets/fonts/LexendDeca-Regular.ttf";
inline constexpr float kDefaultUiFontPointSize = 11.0f;

// Resolve a path relative to assets/ from exe dir or common build-tree locations.
std::string resolveAssetPath(const std::string& exeBasePath, const char* relativePath);

}  // namespace evolab::engine::gfx
