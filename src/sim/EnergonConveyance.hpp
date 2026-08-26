#pragma once

#include <cstdint>

namespace evolab {

class EnergonField;
class Organism;

// PMA axon energon routing: trust/signal-gated feed, η loss per hop, waste-only cloaca at M.
void conveyPmaEnergon(Organism& organism, EnergonField& field, std::uint64_t simTick);

}  // namespace evolab
