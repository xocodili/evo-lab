#include "game/GameShaders.hpp"

namespace evolab::game {
const char* kTerrainVert = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
uniform mat4 uMvp;
out vec3 vColor;
void main() {
  vColor = aColor;
  gl_Position = uMvp * vec4(aPos, 1.0);
}
)";

const char* kTerrainFrag = R"(#version 330 core
in vec3 vColor;
out vec4 FragColor;
void main() {
  FragColor = vec4(vColor, 1.0);
}
)";

const char* kWaterVert = R"(#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uMvp;
void main() {
  gl_Position = uMvp * vec4(aPos, 1.0);
}
)";

const char* kWaterFrag = R"(#version 330 core
out vec4 FragColor;
void main() {
  FragColor = vec4(0.08, 0.35, 0.55, 0.45);
}
)";

const char* kEnergonVert = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aColor;
uniform mat4 uMvp;
out vec4 vColor;
void main() {
  vColor = aColor;
  gl_Position = uMvp * vec4(aPos, 1.0);
}
)";

const char* kEnergonFrag = R"(#version 330 core
in vec4 vColor;
out vec4 FragColor;
void main() {
  FragColor = vColor;
}
)";

const char* kCellVert = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aColor;
layout(location = 2) in vec2 aLocal;
uniform mat4 uMvp;
out vec4 vColor;
out vec2 vLocal;
void main() {
  vColor = aColor;
  vLocal = aLocal;
  gl_Position = uMvp * vec4(aPos, 1.0);
}
)";

const char* kCellFrag = R"(#version 330 core
in vec4 vColor;
in vec2 vLocal;
out vec4 FragColor;
void main() {
  if (dot(vLocal, vLocal) > 1.0) {
    discard;
  }
  FragColor = vColor;
}
)";
}

