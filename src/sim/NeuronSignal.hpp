#pragma once

#include "sim/CellConstants.hpp"
#include "sim/CloacaSignal.hpp"
#include "sim/Energon.hpp"
#include "sim/NeuralAxon.hpp"

#include <cstddef>
#include <cstdint>

namespace evolab {

enum class NeuronType : std::uint8_t;

class Organism;
struct SkeletonNode;

// Universal axon analog byte (0–7) shared by P, M, A (and future C).
inline bool isNeuronConfidenceByte(std::uint8_t byte) {
  return byte <= kNeuronConfidenceMax;
}

inline bool isPerceptorConfidenceByte(std::uint8_t byte) {
  return isNeuronConfidenceByte(byte);
}

inline float confidenceToUnit(std::uint8_t confidence) {
  return static_cast<float>(confidence) / static_cast<float>(kNeuronConfidenceMax);
}

std::uint8_t fuelStoreToConfidence(std::size_t storeBytes,
                                   std::uint32_t fullBytes = kNeuronConfidenceFullFuelBytes);

// M outbound: 0 = starving, 7 = full / providing fuel.
std::uint8_t mouthFuelConfidence(const SkeletonNode& mouth);

void reconcileMouthChewFill(SkeletonNode& mouth);

void creditMouthChew(SkeletonNode& mouth, std::uint32_t netBytes);

// Update mouth diet EMA after a successful bite (origin + cloaca band when applicable).
void recordMouthDietBite(SkeletonNode& mouth, EnergonOrigin origin, CloacaBand cloacaBand);

// M→* outbound: fuel/satiation modulated by postingestive diet (gag reflex on distress cloaca).
std::uint8_t mouthOutboundConfidence(const SkeletonNode& mouth);

// A outbound: 0 = idle flagella, 7 = full stroke (scales with stroke bytes paid).
std::uint8_t actuatorActivityConfidence(bool strokePaid, std::uint32_t strokeBytesPaid);

// C hub outbound: 0 = starving hub, 7 = satiated bodyStorage.
std::uint8_t hubFuelConfidence(std::size_t hubBytes,
                               std::uint32_t fullBytes = kComputerHubStoreMaxBytes);

// Outbound confidence for any neuron type at emit time.
std::uint8_t encodeNeuronOutboundConfidence(const Organism& organism, NeuronType type,
                                            const SkeletonNode& node);

void writeAxonConfidence(NeuralAxon& axon, std::uint8_t confidence, std::uint64_t simTick);

// Discretized reward-prediction error on the universal 0–7 gradient (DESIGN-NOTES §8.1.1).
// outcome and expected are signed scores in roughly [-1, 1]; 4 = neutral / no plasticity.
std::uint8_t predictionErrorByte(float outcome, float expected = 0.0f);

// Map RPE byte distance from neutral (4) to believe-trust nudge magnitude/sign.
int trustDeltaFromPredictionError(std::uint8_t rpeByte);

const char* neuronConfidenceRoleLabel(NeuronType source);

}  // namespace evolab
