#pragma once

#include <cstddef>

namespace evolab::engine::gl {

using GLsizeiptr = std::ptrdiff_t;

struct GlContext {
  void (*genVertexArrays)(int, unsigned*) = nullptr;
  void (*bindVertexArray)(unsigned) = nullptr;
  void (*deleteVertexArrays)(int, const unsigned*) = nullptr;
  void (*genBuffers)(int, unsigned*) = nullptr;
  void (*bindBuffer)(unsigned, unsigned) = nullptr;
  void (*bufferData)(unsigned, GLsizeiptr, const void*, unsigned) = nullptr;
  void (*deleteBuffers)(int, const unsigned*) = nullptr;
  void (*enableVertexAttribArray)(unsigned) = nullptr;
  void (*vertexAttribPointer)(unsigned, int, unsigned, unsigned char, int, const void*) = nullptr;
  unsigned (*createShader)(unsigned) = nullptr;
  void (*shaderSource)(unsigned, int, const char* const*, const int*) = nullptr;
  void (*compileShader)(unsigned) = nullptr;
  void (*getShaderiv)(unsigned, unsigned, int*) = nullptr;
  void (*getShaderInfoLog)(unsigned, int, int*, char*) = nullptr;
  void (*deleteShader)(unsigned) = nullptr;
  unsigned (*createProgram)() = nullptr;
  void (*attachShader)(unsigned, unsigned) = nullptr;
  void (*linkProgram)(unsigned) = nullptr;
  void (*getProgramiv)(unsigned, unsigned, int*) = nullptr;
  void (*getProgramInfoLog)(unsigned, int, int*, char*) = nullptr;
  void (*useProgram)(unsigned) = nullptr;
  int (*getUniformLocation)(unsigned, const char*) = nullptr;
  void (*uniformMatrix4fv)(int, int, unsigned char, const float*) = nullptr;
  void (*uniform2f)(int, float, float) = nullptr;
  void (*uniform1i)(int, int) = nullptr;
  void (*deleteProgram)(unsigned) = nullptr;
  void (*drawElements)(unsigned, int, unsigned, const void*) = nullptr;
  void (*drawArrays)(unsigned, int, int) = nullptr;
  void (*viewport)(int, int, int, int) = nullptr;
  void (*clear)(unsigned) = nullptr;
  void (*clearColor)(float, float, float, float) = nullptr;
  void (*enable)(unsigned) = nullptr;
  void (*disable)(unsigned) = nullptr;
  void (*blendFunc)(unsigned, unsigned) = nullptr;
  void (*genTextures)(int, unsigned*) = nullptr;
  void (*bindTexture)(unsigned, unsigned) = nullptr;
  void (*deleteTextures)(int, const unsigned*) = nullptr;
  void (*texImage2D)(unsigned, int, int, int, int, int, unsigned, unsigned, const void*) = nullptr;
  void (*texParameteri)(unsigned, unsigned, int) = nullptr;
  void (*activeTexture)(unsigned) = nullptr;

  bool loaded = false;
};

namespace GlEnum {
constexpr unsigned kArrayBuffer = 0x8892;
constexpr unsigned kElementArrayBuffer = 0x8893;
constexpr unsigned kStaticDraw = 0x88E4;
constexpr unsigned kDynamicDraw = 0x88E8;
constexpr unsigned kVertexShader = 0x8B31;
constexpr unsigned kFragmentShader = 0x8B30;
constexpr unsigned kCompileStatus = 0x8B81;
constexpr unsigned kLinkStatus = 0x8B82;
constexpr unsigned kFloat = 0x1406;
constexpr unsigned kFalse = 0;
constexpr unsigned kTriangles = 0x0004;
constexpr unsigned kTriangleFan = 0x0006;
constexpr unsigned kLines = 0x0001;
constexpr unsigned kPoints = 0x0000;
constexpr unsigned kUnsignedInt = 0x1405;
constexpr unsigned kColorBufferBit = 0x00004000;
constexpr unsigned kDepthBufferBit = 0x00000100;
constexpr unsigned kDepthTest = 0x0B71;
constexpr unsigned kBlend = 0x0BE2;
constexpr unsigned kSrcAlpha = 0x0302;
constexpr unsigned kOneMinusSrcAlpha = 0x0303;
constexpr unsigned kTexture2D = 0x0DE1;
constexpr unsigned kRgba = 0x1908;
constexpr unsigned kUnsignedByte = 0x1401;
constexpr unsigned kTexture0 = 0x84C0;
constexpr unsigned kLinear = 0x2601;
constexpr unsigned kTextureMinFilter = 0x2801;
constexpr unsigned kTextureMagFilter = 0x2800;
constexpr unsigned kProgramPointSize = 0x8642;
}  // namespace GlEnum

bool loadGlContext();
GlContext& gl();

}  // namespace evolab::engine::gl
