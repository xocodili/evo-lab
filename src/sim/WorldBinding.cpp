#include "sim/WorldBinding.hpp"

#include <cmath>

namespace evolab {

float hubSocketAngleRad(float organismHeading, std::uint8_t slotIndex) {
  const float slot = static_cast<float>(slotIndex % kWorldHubSocketCount);
  return organismHeading + slot * kWorldHubSocketSeparationRad;
}

}  // namespace evolab
