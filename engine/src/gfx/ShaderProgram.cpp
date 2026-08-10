#include "engine/gfx/ShaderProgram.hpp"

#include "engine/gl/GlContext.hpp"

#include <stdexcept>
#include <string>

namespace evolab::engine::gfx {

namespace {

unsigned compileShader(unsigned type, const char* source) {
  auto& g = gl::gl();
  const unsigned shader = g.createShader(type);
  g.shaderSource(shader, 1, &source, nullptr);
  g.compileShader(shader);
  int ok = 0;
  g.getShaderiv(shader, gl::GlEnum::kCompileStatus, &ok);
  if (!ok) {
    char log[512];
    g.getShaderInfoLog(shader, 512, nullptr, log);
    throw std::runtime_error(std::string("Shader compile failed: ") + log);
  }
  return shader;
}

unsigned linkProgram(unsigned vert, unsigned frag) {
  auto& g = gl::gl();
  const unsigned program = g.createProgram();
  g.attachShader(program, vert);
  g.attachShader(program, frag);
  g.linkProgram(program);
  int ok = 0;
  g.getProgramiv(program, gl::GlEnum::kLinkStatus, &ok);
  if (!ok) {
    char log[512];
    g.getProgramInfoLog(program, 512, nullptr, log);
    throw std::runtime_error(std::string("Program link failed: ") + log);
  }
  return program;
}

}  // namespace

ShaderProgram::~ShaderProgram() {
  if (program_) {
    gl::gl().deleteProgram(program_);
    program_ = 0;
  }
}

ShaderProgram::ShaderProgram(ShaderProgram&& other) noexcept : program_(other.program_) {
  other.program_ = 0;
}

ShaderProgram& ShaderProgram::operator=(ShaderProgram&& other) noexcept {
  if (this != &other) {
    if (program_) {
      gl::gl().deleteProgram(program_);
    }
    program_ = other.program_;
    other.program_ = 0;
  }
  return *this;
}

ShaderProgram ShaderProgram::create(const char* vertexSource, const char* fragmentSource) {
  const unsigned vert = compileShader(gl::GlEnum::kVertexShader, vertexSource);
  const unsigned frag = compileShader(gl::GlEnum::kFragmentShader, fragmentSource);
  ShaderProgram result;
  result.program_ = linkProgram(vert, frag);
  gl::gl().deleteShader(vert);
  gl::gl().deleteShader(frag);
  return result;
}

void ShaderProgram::use() const {
  gl::gl().useProgram(program_);
}

void ShaderProgram::setMat4(const char* name, const Mat4& matrix) const {
  gl::gl().uniformMatrix4fv(gl::gl().getUniformLocation(program_, name), 1, gl::GlEnum::kFalse,
                            matrix.m);
}

void ShaderProgram::setVec2(const char* name, float x, float y) const {
  gl::gl().uniform2f(gl::gl().getUniformLocation(program_, name), x, y);
}

void ShaderProgram::setInt(const char* name, int value) const {
  gl::gl().uniform1i(gl::gl().getUniformLocation(program_, name), value);
}

}  // namespace evolab::engine::gfx
