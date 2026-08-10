#pragma once

#include <string>
#include <vector>

namespace evolab::engine::gfx {

class UiFont {
public:
  UiFont();
  ~UiFont();

  UiFont(const UiFont&) = delete;
  UiFont& operator=(const UiFont&) = delete;

  bool load(const std::string& path, float pointSize);
  bool loaded() const;

  bool renderTextBitmap(const std::string& text, std::vector<unsigned char>& rgba, int& outW,
                        int& outH, int minW = 0, int minH = 0) const;

private:
  struct State;
  State* state_ = nullptr;
};

}  // namespace evolab::engine::gfx
