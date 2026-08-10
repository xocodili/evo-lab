#pragma once

#include "engine/Camera.hpp"
#include "sim/Organism.hpp"

#include <cstdint>
#include <vector>

namespace evolab::game {

std::uint32_t pickOrganismAtScreen(const std::vector<Organism>& organisms,
                                   const engine::OrbitCamera& camera, int viewportW, int viewportH,
                                   int mouseX, int mouseY, float pickRadiusPx = 22.0f);

}  // namespace evolab::game
