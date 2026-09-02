#include "sim/NeuronCoordinator.hpp"

#include "sim/CellConstants.hpp"
#include "sim/NeuronFuel.hpp"
#include "sim/NeuronSignal.hpp"
#include "sim/NeuronStem.hpp"
#include "sim/OrganismNeuron.hpp"
#include "sim/PerceptorFocus.hpp"

#include <algorithm>
#include <cmath>

namespace evolab {

namespace {

float nodeFuelUnit(const Organism& organism, const SkeletonNode& node) {
  const std::vector<std::uint8_t>* pool = neuronFuelPool(const_cast<Organism&>(organism), node);
  if (pool == nullptr) {
    return 0.0f;
  }
  const std::size_t maxBytes = nodeStoreNominalCap(organism, node);
  return clamp01(static_cast<float>(pool->size()) /
                 static_cast<float>(std::max<std::size_t>(maxBytes, 1u)));
}

float nodeSenseDrive(const Organism& organism, const SkeletonNode& node) {
  switch (node.neuron) {
    case NeuronType::Mouth:
      return clamp01(node.mouthTasteSalience + std::max(0.0f, node.mouthTasteGradient) * 0.35f);
    case NeuronType::Perceptor:
      return clamp01(node.focusSalience * (node.focusLocked ? 1.0f : 0.65f));
    case NeuronType::Actuator:
      return organism.lastStrokePaid ? 1.0f : kCoordinatorBaselineDutyScale;
    case NeuronType::Computer:
      return clamp01(node.lastComputerMatchScore);
    case NeuronType::None:
      return nodeFuelUnit(organism, node);
    default:
      return 0.5f;
  }
}

std::uint8_t encodeCoordinatorObserved(const CoordinatorInteroception& interoception) {
  const float combined =
      clamp01(interoception.fuelUnit * 0.55f + interoception.senseDrive * 0.45f);
  return static_cast<std::uint8_t>(std::lround(combined * static_cast<float>(kNeuronConfidenceMax)));
}

float inboundSignalStrengthUnit(const Organism& organism, std::uint32_t dstNodeId,
                                std::uint64_t simTick) {
  float best = 0.0f;
  bool any = false;
  forEachInboundAxon(organism, dstNodeId, simTick, true, [&](const InboundAxon& inbound) {
    if (!isNeuronConfidenceByte(inbound.axon.lastReceived.byte)) {
      return;
    }
    any = true;
    best = std::max(best, confidenceToUnit(inbound.axon.lastReceived.byte) * inbound.weight);
  });
  if (!any) {
    return 0.0f;
  }
  return clamp01(best);
}

float organismFieldFoodUnit(const Organism& organism) {
  float best = 0.0f;
  for (const SkeletonNode& node : organism.nodes) {
    if (!node.alive) {
      continue;
    }
    if (node.neuron == NeuronType::Mouth) {
      best = std::max(best, clamp01(node.mouthTasteSalience));
    } else if (node.neuron == NeuronType::Perceptor) {
      if (node.focusKind == PerceptFocusKind::Food) {
        best = std::max(best, clamp01(node.focusSalience));
      }
      if (node.perceptPriorFoodSalienceValid) {
        best = std::max(best, clamp01(node.perceptPriorFoodSalience));
      }
    }
  }
  return best;
}

float hubFuelUnit(const Organism& organism) {
  return clamp01(static_cast<float>(computerHubFuelBytes(organism)) /
                 static_cast<float>(std::max<std::size_t>(hubStoreCapBytes(organism), 1u)));
}

float nodeEffectiveFamine(const Organism& organism, const SkeletonNode& node) {
  float famine = organism.famineUnit;
  if (node.neuron == NeuronType::Mouth) {
    famine *= (1.0f - clamp01(node.mouthTasteSalience));
  } else if (node.neuron == NeuronType::Perceptor && node.focusKind == PerceptFocusKind::Food &&
             node.focusLocked) {
    famine *= (1.0f - clamp01(node.focusSalience));
  }
  return clamp01(famine);
}

float computeNodeDutyScale(SkeletonNode& node, const Organism& organism,
                           CoordinatorInteroception& outInteroception) {
  outInteroception.fuelUnit = nodeFuelUnit(organism, node);
  outInteroception.senseDrive = nodeSenseDrive(organism, node);
  outInteroception.observedByte = encodeCoordinatorObserved(outInteroception);
  outInteroception.patternMatch =
      neuronConfidenceMatchUnit(outInteroception.observedByte, node.coordinatorRegister[0]);

  outInteroception.excitation = clamp01(outInteroception.patternMatch * 0.4f +
                                        outInteroception.senseDrive * kCoordinatorExcitationGain +
                                        outInteroception.fuelUnit * 0.25f);

  outInteroception.delta = outInteroception.excitation - node.coordinatorPriorExcitation;
  node.coordinatorPriorExcitation = outInteroception.excitation;
  node.coordinatorLastExcitation = outInteroception.excitation;
  node.coordinatorLastDelta = outInteroception.delta;

  const float hubUnit = hubFuelUnit(organism);
  const float repleteUnit = hubUnit > 0.01f ? hubUnit : outInteroception.fuelUnit;
  const float satiationBrake = repleteUnit * kCoordinatorSatiationBrake;
  const float famineTorpor = nodeEffectiveFamine(organism, node) * kCoordinatorFamineTorpor;

  float duty = kCoordinatorBaselineDutyScale + outInteroception.excitation * 0.35f +
               outInteroception.delta * kCoordinatorDeltaGain - satiationBrake - famineTorpor;
  duty = std::clamp(duty, coordinatorMinDutyForNeuron(node.neuron), kCoordinatorMaxDutyScale);
  node.coordinatorDutyScale = duty;
  return duty;
}

void syncOrganismCoordinatorTelemetry(Organism& organism) {
  float minDuty = 1.0f;
  float maxDuty = 0.0f;
  float rootDuty = 1.0f;
  bool any = false;

  for (const SkeletonNode& node : organism.nodes) {
    if (!node.alive) {
      continue;
    }
    any = true;
    minDuty = std::min(minDuty, node.coordinatorDutyScale);
    maxDuty = std::max(maxDuty, node.coordinatorDutyScale);
    if (node.id == organism.rootNodeId) {
      rootDuty = node.coordinatorDutyScale;
    }
  }

  if (!any) {
    organism.coordinatorDutyScale = 1.0f;
    organism.coordinatorMinNodeDuty = 1.0f;
    organism.coordinatorMaxNodeDuty = 1.0f;
    return;
  }
  organism.coordinatorDutyScale = rootDuty;
  organism.coordinatorMinNodeDuty = minDuty;
  organism.coordinatorMaxNodeDuty = maxDuty;
}

}  // namespace

float coordinatorMinDutyForNeuron(NeuronType neuron) {
  switch (neuron) {
    case NeuronType::Perceptor:
      return kCoordinatorMinDutyPerceptor;
    case NeuronType::Mouth:
      return kCoordinatorMinDutyMouth;
    case NeuronType::Actuator:
      return kCoordinatorMinDutyActuator;
    case NeuronType::Computer:
      return kCoordinatorMinDutyComputer;
    default:
      return kCoordinatorMinDutyScale;
  }
}

float computeOrganismFamineUnit(const Organism& organism, std::uint64_t simTick) {
  if (!organism.isCampNom()) {
    return 0.0f;
  }

  const SkeletonNode* computer = findNeuronNode(organism, NeuronType::Computer);
  if (computer == nullptr || !computer->alive) {
    return 0.0f;
  }

  const float hubUnit = hubFuelUnit(organism);
  const float hubStress = 1.0f - hubUnit;
  const float inboundStrength = inboundSignalStrengthUnit(organism, computer->id, simTick);
  const float quietStress = 1.0f - inboundStrength;
  const float fieldFood = organismFieldFoodUnit(organism);
  const float fieldStress = 1.0f - fieldFood;

  // Feast: wet food present, or replete hub living on reserves — not famine.
  if (fieldFood >= kCoordinatorFeastFieldFood) {
    return 0.0f;
  }
  if (hubUnit >= kCoordinatorFeastHubUnit) {
    return 0.0f;
  }

  float famine = kCoordinatorFamineHubWeight * hubStress +
                 kCoordinatorFamineQuietWeight * quietStress +
                 kCoordinatorFamineFieldWeight * fieldStress;

  if (fieldFood >= kCoordinatorFamineFieldSuppress) {
    famine *= (1.0f - fieldFood);
  }

  return clamp01(famine);
}

CoordinatorInteroception gatherCoordinatorInteroception(const Organism& organism,
                                                        const SkeletonNode& node) {
  CoordinatorInteroception interoception;
  interoception.fuelUnit = nodeFuelUnit(organism, node);
  interoception.senseDrive = nodeSenseDrive(organism, node);
  interoception.observedByte = encodeCoordinatorObserved(interoception);
  interoception.patternMatch =
      neuronConfidenceMatchUnit(interoception.observedByte, node.coordinatorRegister[0]);
  interoception.excitation =
      clamp01(interoception.patternMatch * 0.4f +
              interoception.senseDrive * kCoordinatorExcitationGain + interoception.fuelUnit * 0.25f);
  interoception.delta = interoception.excitation - node.coordinatorPriorExcitation;
  return interoception;
}

void initCoordinatorNodeRegister(SkeletonNode& node) {
  node.coordinatorRegister = {kNeuronConfidenceNeutral, 0u, 0u, 0u};
  node.coordinatorDutyScale = 1.0f;
  node.coordinatorPriorExcitation = 0.0f;
  node.coordinatorLastExcitation = 0.0f;
  node.coordinatorLastDelta = 0.0f;
}

void tickCoordinatorPhase(Organism& organism, std::uint64_t simTick) {
  if (!organism.alive) {
    return;
  }

  organism.famineUnit = computeOrganismFamineUnit(organism, simTick);
  organism.famineConfidence = famineAbundanceConfidence(organism.famineUnit);

  for (SkeletonNode& node : organism.nodes) {
    if (!node.alive) {
      continue;
    }
    CoordinatorInteroception scratch;
    computeNodeDutyScale(node, organism, scratch);
  }

  syncOrganismCoordinatorTelemetry(organism);
}

float coordinatorDutyScaleForNode(const Organism& organism, std::uint32_t nodeId) {
  const SkeletonNode* node = organism.findNode(nodeId);
  if (node == nullptr || !node->alive) {
    return 1.0f;
  }
  return node->coordinatorDutyScale;
}

float applyMiniCToComputerDispatch(float organDispatchGain, float coordinatorDutyScale,
                                   float hubConservationExportScale) {
  if (organDispatchGain <= 1.0e-4f || hubConservationExportScale <= 1.0e-4f) {
    return 0.0f;
  }
  const float scaled = organDispatchGain * clamp01(coordinatorDutyScale);
  if (scaled <= 1.0e-4f) {
    return 0.0f;
  }
  if (organDispatchGain >= kComputerMinDispatchGain - 1.0e-4f) {
    return std::clamp(scaled, kComputerMinDispatchGain, 1.0f);
  }
  return std::clamp(scaled, 0.0f, 1.0f);
}

}  // namespace evolab
