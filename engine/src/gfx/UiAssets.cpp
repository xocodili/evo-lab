#include "engine/gfx/UiAssets.hpp"

#include <filesystem>

namespace evolab::engine::gfx {

std::string resolveAssetPath(const std::string& exeBasePath, const char* relativePath) {
  namespace fs = std::filesystem;
  const fs::path fileName = fs::path(relativePath);
  if (!exeBasePath.empty()) {
    const fs::path exeAsset = fs::path(exeBasePath) / fileName;
    if (fs::exists(exeAsset)) {
      return exeAsset.string();
    }
  }
  const fs::path candidates[] = {
      fileName,
      fs::path("..") / fileName,
      fs::path("../..") / fileName,
      fs::path("../../..") / fileName,
  };
  for (const fs::path& candidate : candidates) {
    if (fs::exists(candidate)) {
      return candidate.string();
    }
  }
  return fileName.string();
}

}  // namespace evolab::engine::gfx
