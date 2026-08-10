#pragma once

namespace evolab::engine {

struct Mat4 {
  float m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
};

Mat4 mat4Identity();
Mat4 mat4Perspective(float fovYRad, float aspect, float nearZ, float farZ);
Mat4 mat4LookAt(float eyeX, float eyeY, float eyeZ, float centerX, float centerY, float centerZ,
                float upX, float upY, float upZ);
Mat4 mat4Multiply(const Mat4& a, const Mat4& b);

struct OrbitCamera {
  float yaw = 0.7f;
  float pitch = 0.45f;
  float distance = 180.0f;
  float targetX = 0.0f;
  float targetY = 0.0f;
  float targetZ = 0.0f;

  void orbit(float deltaYaw, float deltaPitch);
  void zoom(float delta);
  void pan(float deltaRight, float deltaForward);
  Mat4 viewMatrix() const;
  void eyePosition(float& eyeX, float& eyeY, float& eyeZ) const;
};

}  // namespace evolab::engine
