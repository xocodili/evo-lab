#pragma once

#include "sim/Energon.hpp"
#include "sim/Organism.hpp"

namespace evolab {

// CAMP axon energon routing: trust/signal-gated feed, η loss per hop, M-only field ingress.
void conveyCampEnergon(Organism& organism, EnergonField& field, std::uint64_t simTick);

}  // namespace evolab
