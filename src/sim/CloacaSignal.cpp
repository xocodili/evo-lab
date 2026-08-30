#include "sim/CloacaSignal.hpp"

#include "sim/CellConstants.hpp"
#include "sim/Chaos.hpp"
#include "sim/NeuronFuel.hpp"
#include "sim/NeuronSignal.hpp"
#include "sim/NeuronStem.hpp"
#include "sim/EnergonString.hpp"
#include "sim/Organism.hpp"

namespace evolab {

namespace {

std::uint64_t packCloacaTagBytes(std::uint8_t tag, std::uint32_t count) {
  std::uint64_t data = 0;
  const std::uint32_t packed = std::min(count, 8u);
  for (std::uint32_t i = 0; i < packed; ++i) {
    data |= static_cast<std::uint64_t>(tag) << (8 * i);
  }
  return data;
}

}  // namespace

CloacaBand cloacaBandFromTag(std::uint8_t tag) {
  if (tag == 0) {
    return CloacaBand::None;
  }
  if (tag < kCloacaPaletteDistressCeiling) {
    return CloacaBand::Distress;
  }
  if (tag >= kCloacaPaletteMateFloor) {
    return CloacaBand::Mate;
  }
  if (tag >= kSignalTagReservedMin) {
    return CloacaBand::Baseline;
  }
  return CloacaBand::Baseline;
}

std::uint8_t cloacaBandTag(CloacaBand band) {
  switch (band) {
    case CloacaBand::Distress:
      return kCloacaTagDistress;
    case CloacaBand::Baseline:
      return kCloacaTagBaseline;
    case CloacaBand::Mate:
      return kCloacaTagMate;
    default:
      return 0;
  }
}

std::uint8_t cloacaBandTag(const Organism& organism, CloacaBand band) {
  switch (band) {
    case CloacaBand::Distress:
      return organism.cloacaDistressByte;
    case CloacaBand::Baseline:
      return organism.cloacaBaselineByte;
    case CloacaBand::Mate:
      return organism.cloacaMateByte;
    default:
      return 0;
  }
}

void jitterCloacaPaletteBytes(Organism& organism, std::mt19937& rng) {
  if (!organism.isCampNom()) {
    return;
  }

  auto jitterByte = [&](std::uint8_t baseline) {
    const float jittered = chaosJitterFloat(static_cast<float>(baseline), rng);
    return static_cast<std::uint8_t>(
        std::clamp(static_cast<int>(jittered + 0.5f), 1, 255));
  };

  std::uint8_t distress = jitterByte(organism.cloacaDistressByte);
  std::uint8_t baseline = jitterByte(organism.cloacaBaselineByte);
  std::uint8_t mate = jitterByte(organism.cloacaMateByte);

  baseline = std::max(baseline,
                      static_cast<std::uint8_t>(distress + kCloacaPaletteMinTierGap));
  mate = std::max(mate, static_cast<std::uint8_t>(baseline + kCloacaPaletteMinTierGap));

  organism.cloacaDistressByte = distress;
  organism.cloacaBaselineByte = baseline;
  organism.cloacaMateByte = mate;
}

std::uint32_t cloacaVentByteCost(CloacaBand band) {
  switch (band) {
    case CloacaBand::Distress:
      return kCloacaVentCostDistress;
    case CloacaBand::Baseline:
      return kCloacaVentCostBaseline;
    case CloacaBand::Mate:
      return kCloacaVentCostMate;
    default:
      return 0;
  }
}

CloacaBand cloacaBandFromBlob(const EnergonBlob& blob) {
  if (blob.origin == EnergonOrigin::Cloaca) {
    return cloacaBandFromTag(static_cast<std::uint8_t>(blob.data & 0xFF));
  }
  if (blob.origin == EnergonOrigin::Signal) {
    return CloacaBand::Baseline;
  }
  return CloacaBand::None;
}

bool campDistressPredicate(const Organism& organism) {
  if (computerHubFuelBytes(organism) < kComputerHubReserveBytes) {
    return true;
  }
  for (const SkeletonNode& node : organism.nodes) {
    if (node.alive && node.basalArrearsTicks > 0) {
      return true;
    }
  }
  return hubFuelConfidence(computerHubFuelBytes(organism)) < 3u;
}

bool campMateReadyPredicate(const Organism& organism, std::uint64_t simTick) {
  if (!organism.isCampNom() || !organism.alive) {
    return false;
  }
  if (hubFuelConfidence(computerHubFuelBytes(organism)) < kComputerSatiationConfidence) {
    return false;
  }
  const SkeletonNode* mouth = findNeuronNode(organism, NeuronType::Mouth);
  if (mouth == nullptr ||
      mouth->store.size() < static_cast<std::size_t>(kMouthInhibitActuatorConfidence)) {
    return false;
  }
  const SkeletonNode* perceptor = findNeuronNode(organism, NeuronType::Perceptor);
  const SkeletonNode* actuator = findNeuronNode(organism, NeuronType::Actuator);
  if (perceptor == nullptr || actuator == nullptr) {
    return false;
  }
  if (perceptor->store.size() < kPerceptorScanCostPerTick) {
    return false;
  }
  if (actuator->store.size() < kActuatorStrokeCostPerTick) {
    return false;
  }
  if (organism.lastPerceptFocusKind == PerceptFocusKind::Threat &&
      organism.lastPerceptConfidence < kNeuronConfidenceNeutral) {
    return false;
  }
  const std::uint64_t age =
      simTick > organism.createdAtTick ? simTick - organism.createdAtTick : 0u;
  if (age < kMateMinAgeTicks) {
    return false;
  }
  return computerHubFuelBytes(organism) >= kComputerHubReserveBytes + kCloacaVentCostMate;
}

CloacaBand chooseCloacaBand(const Organism& organism, std::uint64_t simTick) {
  if (campDistressPredicate(organism) &&
      computerHubFuelBytes(organism) >= kCloacaVentCostDistress) {
    return CloacaBand::Distress;
  }
  if (campMateReadyPredicate(organism, simTick)) {
    return CloacaBand::Mate;
  }
  const float satiation =
      confidenceToUnit(hubFuelConfidence(computerHubFuelBytes(organism)));
  if (satiation >= confidenceToUnit(kComputerSatiationConfidence) &&
      computerHubFuelBytes(organism) >= kComputerHubReserveBytes + kCloacaVentCostBaseline) {
    return CloacaBand::Baseline;
  }
  return CloacaBand::None;
}

bool expelCloacaVent(Organism& organism, EnergonField& field, SkeletonNode& computer,
                     CloacaBand band) {
  const std::uint32_t cost = cloacaVentByteCost(band);
  if (cost == 0 || !hubStoreConsumeBack(organism, cost)) {
    return false;
  }
  const std::uint8_t tag = cloacaBandTag(organism, band);
  EnergonBlob blob;
  blob.data = packCloacaTagBytes(tag, cost);
  blob.remaining = static_cast<std::uint16_t>(cost);
  blob.initialBytes = static_cast<std::uint8_t>(cost);
  blob.origin = EnergonOrigin::Cloaca;
  blob.x = computer.worldX;
  blob.z = computer.worldZ;
  blob.y = computer.worldY;
  blob.grounded = true;
  blob.onWet = true;
  energonAssignGroundedTtl(blob, field.config(), true);
  energonBlobInitPoint(blob);
  field.injectBlob(blob);
  return true;
}

}  // namespace evolab
