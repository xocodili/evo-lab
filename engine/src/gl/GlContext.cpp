#include "engine/gl/GlContext.hpp"

#include <SDL.h>

#include <stdexcept>

namespace evolab::engine::gl {

namespace {

GlContext gGl;

template <typename T>
T loadFn(const char* name) {
  return reinterpret_cast<T>(SDL_GL_GetProcAddress(name));
}

}  // namespace

bool loadGlContext() {
  if (gGl.loaded) {
    return true;
  }

  gGl.genVertexArrays = loadFn<decltype(gGl.genVertexArrays)>("glGenVertexArrays");
  gGl.bindVertexArray = loadFn<decltype(gGl.bindVertexArray)>("glBindVertexArray");
  gGl.deleteVertexArrays = loadFn<decltype(gGl.deleteVertexArrays)>("glDeleteVertexArrays");
  gGl.genBuffers = loadFn<decltype(gGl.genBuffers)>("glGenBuffers");
  gGl.bindBuffer = loadFn<decltype(gGl.bindBuffer)>("glBindBuffer");
  gGl.bufferData = loadFn<decltype(gGl.bufferData)>("glBufferData");
  gGl.deleteBuffers = loadFn<decltype(gGl.deleteBuffers)>("glDeleteBuffers");
  gGl.enableVertexAttribArray = loadFn<decltype(gGl.enableVertexAttribArray)>("glEnableVertexAttribArray");
  gGl.vertexAttribPointer = loadFn<decltype(gGl.vertexAttribPointer)>("glVertexAttribPointer");
  gGl.createShader = loadFn<decltype(gGl.createShader)>("glCreateShader");
  gGl.shaderSource = loadFn<decltype(gGl.shaderSource)>("glShaderSource");
  gGl.compileShader = loadFn<decltype(gGl.compileShader)>("glCompileShader");
  gGl.getShaderiv = loadFn<decltype(gGl.getShaderiv)>("glGetShaderiv");
  gGl.getShaderInfoLog = loadFn<decltype(gGl.getShaderInfoLog)>("glGetShaderInfoLog");
  gGl.deleteShader = loadFn<decltype(gGl.deleteShader)>("glDeleteShader");
  gGl.createProgram = loadFn<decltype(gGl.createProgram)>("glCreateProgram");
  gGl.attachShader = loadFn<decltype(gGl.attachShader)>("glAttachShader");
  gGl.linkProgram = loadFn<decltype(gGl.linkProgram)>("glLinkProgram");
  gGl.getProgramiv = loadFn<decltype(gGl.getProgramiv)>("glGetProgramiv");
  gGl.getProgramInfoLog = loadFn<decltype(gGl.getProgramInfoLog)>("glGetProgramInfoLog");
  gGl.useProgram = loadFn<decltype(gGl.useProgram)>("glUseProgram");
  gGl.getUniformLocation = loadFn<decltype(gGl.getUniformLocation)>("glGetUniformLocation");
  gGl.uniformMatrix4fv = loadFn<decltype(gGl.uniformMatrix4fv)>("glUniformMatrix4fv");
  gGl.uniform2f = loadFn<decltype(gGl.uniform2f)>("glUniform2f");
  gGl.uniform1i = loadFn<decltype(gGl.uniform1i)>("glUniform1i");
  gGl.deleteProgram = loadFn<decltype(gGl.deleteProgram)>("glDeleteProgram");
  gGl.drawElements = loadFn<decltype(gGl.drawElements)>("glDrawElements");
  gGl.drawArrays = loadFn<decltype(gGl.drawArrays)>("glDrawArrays");
  gGl.viewport = loadFn<decltype(gGl.viewport)>("glViewport");
  gGl.clear = loadFn<decltype(gGl.clear)>("glClear");
  gGl.clearColor = loadFn<decltype(gGl.clearColor)>("glClearColor");
  gGl.enable = loadFn<decltype(gGl.enable)>("glEnable");
  gGl.disable = loadFn<decltype(gGl.disable)>("glDisable");
  gGl.blendFunc = loadFn<decltype(gGl.blendFunc)>("glBlendFunc");
  gGl.genTextures = loadFn<decltype(gGl.genTextures)>("glGenTextures");
  gGl.bindTexture = loadFn<decltype(gGl.bindTexture)>("glBindTexture");
  gGl.deleteTextures = loadFn<decltype(gGl.deleteTextures)>("glDeleteTextures");
  gGl.texImage2D = loadFn<decltype(gGl.texImage2D)>("glTexImage2D");
  gGl.texParameteri = loadFn<decltype(gGl.texParameteri)>("glTexParameteri");
  gGl.activeTexture = loadFn<decltype(gGl.activeTexture)>("glActiveTexture");

  if (!gGl.genVertexArrays || !gGl.createShader || !gGl.drawElements || !gGl.clear ||
      !gGl.deleteProgram) {
    return false;
  }

  gGl.loaded = true;
  return true;
}

GlContext& gl() {
  if (!gGl.loaded) {
    throw std::runtime_error("OpenGL context not loaded");
  }
  return gGl;
}

}  // namespace evolab::engine::gl
