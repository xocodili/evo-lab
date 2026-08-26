#pragma once

#include "sim/NeuronSignal.hpp"

#include <cstdint>

namespace evolab {

enum class PerceptFocusKind : std::uint8_t { None = 0, Food = 1, Mate = 2, Threat = 3 };

const char* perceptFocusKindLabel(PerceptFocusKind kind);

}  // namespace evolab
