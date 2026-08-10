#pragma once

#include "engine/Camera.hpp"

namespace evolab::engine::gfx {

class ShaderProgram {
public:
  ShaderProgram() = default;
  ~ShaderProgram();

  ShaderProgram(const ShaderProgram&) = delete;
  ShaderProgram& operator=(const ShaderProgram&) = delete;
  ShaderProgram(ShaderProgram&& other) noexcept;
  ShaderProgram& operator=(ShaderProgram&& other) noexcept;

  static ShaderProgram create(const char* vertexSource, const char* fragmentSource);

  void use() const;
  void setMat4(const char* name, const Mat4& matrix) const;
  void setVec2(const char* name, float x, float y) const;
  void setInt(const char* name, int value) const;

  unsigned id() const { return program_; }

private:
  unsigned program_ = 0;
};

}  // namespace evolab::engine::gfx
