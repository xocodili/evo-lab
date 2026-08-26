#include "game/OrganismInspector.hpp"

#include "sim/CellConstants.hpp"
#include "sim/NeuralAxon.hpp"
#include "sim/NeuronSignal.hpp"
#include "sim/OrganismNeuron.hpp"
#include "sim/PerceptorFocus.hpp"
#include "sim/WorldConstants.hpp"

#include <cstdio>

namespace evolab::game {

namespace {

int trustDisplayPercent(std::uint16_t trust) {
  return static_cast<int>((static_cast<unsigned>(trust) * 100u + kTrustBaseline / 2u) /
                          kTrustBaseline);
}

const char* signalTagLabel(std::uint8_t tag) {
  if (isNeuronConfidenceByte(tag)) {
    return "CONF";
  }
  return "—";
}

const SkeletonNode* findPerceptorNode(const Organism& organism) {
  return findFirstNeuronNode(organism, NeuronType::Perceptor, false);
}

const SkeletonNode* findMouthNode(const Organism& organism) {
  return findFirstNeuronNode(organism, NeuronType::Mouth, false);
}

const SkeletonNode* findActuatorNode(const Organism& organism) {
  return findFirstNeuronNode(organism, NeuronType::Actuator, false);
}

float daysOfEnergon(std::size_t bytes) {
  return static_cast<float>(bytes) / static_cast<float>(kTicksPerStemCellDay);
}

std::size_t totalNodeFuel(const Organism& organism) {
  std::size_t total = 0;
  for (const SkeletonNode& node : organism.nodes) {
    total += node.store.size();
  }
  return total;
}

const char* neuronAliveFlag(const Organism& organism, NeuronType type) {
  for (const SkeletonNode& node : organism.nodes) {
    if (node.alive && node.neuron == type) {
      return "alive";
    }
  }
  return "dead";
}

void formatAxonConfidence(char* buffer, std::size_t bufferSize, std::uint8_t byte,
                          NeuronType source) {
  if (isNeuronConfidenceByte(byte)) {
    std::snprintf(buffer, bufferSize, "%u/7 (%s)", static_cast<unsigned>(byte),
                  neuronConfidenceRoleLabel(source));
  } else {
    std::snprintf(buffer, bufferSize, "%s", signalTagLabel(byte));
  }
}

}  // namespace

std::string formatOrganismArchitectureLabel(const Organism& organism, std::uint64_t simTick) {
  char buffer[2048];
  const std::size_t nodeFuel = totalNodeFuel(organism);
  const float daysRemaining = daysOfEnergon(nodeFuel + organism.bodyStorage.size());

  if (!organism.hasMouthNeurons() && !organism.hasActuatorNeurons()) {
    if (organism.hasPerceptorNeurons()) {
      const SkeletonNode* perceptorNode = findPerceptorNode(organism);
      const SkeletonNode* mouthNode = findMouthNode(organism);
      const SkeletonNode* actuatorNode = findActuatorNode(organism);
      std::snprintf(buffer, sizeof(buffer),
                    "Nom #%u (degraded P-M-A)\n"
                    "Type: partial chain — M/A lost, not a stem cell\n"
                    "Nodes: %zu  Links: %zu  Axons: %zu\n"
                    "Fuel: P %zu (%s)  M %zu (%s)  A %zu (%s)\n"
                    "Body storage: %zu bytes (unused on intact Nom)\n"
                    "Land-adjacent: %s  tick %llu  %s",
                    organism.id, organism.nodes.size(), organism.links.size(),
                    organism.neuralAxons.size(),
                    perceptorNode != nullptr ? perceptorNode->store.size() : 0,
                    neuronAliveFlag(organism, NeuronType::Perceptor),
                    mouthNode != nullptr ? mouthNode->store.size() : 0,
                    neuronAliveFlag(organism, NeuronType::Mouth),
                    actuatorNode != nullptr ? actuatorNode->store.size() : 0,
                    neuronAliveFlag(organism, NeuronType::Actuator),
                    organism.bodyStorage.size(), organism.landAdjacent ? "yes" : "no",
                    static_cast<unsigned long long>(simTick), organism.alive ? "alive" : "dead");
      return buffer;
    }

    std::snprintf(buffer, sizeof(buffer),
                  "StemCell #%u\n"
                  "Type: undifferentiated\n"
                  "Nodes: %zu  Links: %zu\n"
                  "Storage: %zu bytes (%.2f d)\n"
                  "Land-adjacent: %s\n"
                  "Created: tick %llu\n"
                  "Status: %s",
                  organism.id, organism.nodes.size(), organism.links.size(),
                  organism.bodyStorage.size(), daysRemaining,
                  organism.landAdjacent ? "yes" : "no",
                  static_cast<unsigned long long>(organism.createdAtTick),
                  organism.alive ? "alive" : "dead");
    return buffer;
  }

  if (organism.isPmaNom()) {
    const SkeletonNode* perceptorNode = findPerceptorNode(organism);
    const SkeletonNode* mouthNode = findMouthNode(organism);
    const SkeletonNode* actuatorNode = findActuatorNode(organism);
    const NeuralAxon* pToM = organism.findNeuralAxon(1, 2);
    const NeuralAxon* pToA = organism.findNeuralAxon(1, 3);
    const NeuralAxon* mToA = organism.findNeuralAxon(2, 3);
    const NeuralAxon* aToM = organism.findNeuralAxon(3, 2);
    const NeuralAxon* mToP = organism.findNeuralAxon(2, 1);
    const NeuralAxon* aToP = organism.findNeuralAxon(3, 1);
    char pToMRecv[24] = "—";
    char pToARecv[24] = "—";
    char mToPRecv[24] = "—";
    char aToPRecv[24] = "—";
    char mToARecv[24] = "—";
    char aToMRecv[24] = "—";
    if (pToM != nullptr && pToM->lastReceived.valid) {
      formatAxonConfidence(pToMRecv, sizeof(pToMRecv), pToM->lastReceived.byte,
                           NeuronType::Perceptor);
    }
    if (pToA != nullptr && pToA->lastReceived.valid) {
      formatAxonConfidence(pToARecv, sizeof(pToARecv), pToA->lastReceived.byte,
                           NeuronType::Perceptor);
    }
    if (mToP != nullptr && mToP->lastReceived.valid) {
      formatAxonConfidence(mToPRecv, sizeof(mToPRecv), mToP->lastReceived.byte, NeuronType::Mouth);
    }
    if (aToP != nullptr && aToP->lastReceived.valid) {
      formatAxonConfidence(aToPRecv, sizeof(aToPRecv), aToP->lastReceived.byte,
                           NeuronType::Actuator);
    }
    if (mToA != nullptr && mToA->lastReceived.valid) {
      formatAxonConfidence(mToARecv, sizeof(mToARecv), mToA->lastReceived.byte, NeuronType::Mouth);
    }
    if (aToM != nullptr && aToM->lastReceived.valid) {
      formatAxonConfidence(aToMRecv, sizeof(aToMRecv), aToM->lastReceived.byte,
                           NeuronType::Actuator);
    }
    std::snprintf(buffer, sizeof(buffer),
                  "Nom #%u\n"
                  "Type: P-M-A Nom\n"
                  "Nodes: 3  Links: %zu  Axons: %zu\n"
                  "Heading: %.0f deg  senseR: %.2f cells\n"
                  "Energon (tick %llu):\n"
                  "  P [sense]:  %zu B  %s  scan: %s (%u B)\n"
                  "  M [mouth]:  %zu B  %s  ate: %s  drive: %.0f%%  inhibit: %s\n"
                  "  A [motor]:  %zu B  %s\n"
                  "Focus (last tick):\n"
                  "  kind: %s  confidence: %u/7  bearing: %+.0f deg  range: %.0f%%\n"
                  "Signals (analog 0-7, last tick):\n"
                  "  P->M: %s  P->A: %s\n"
                  "  M->P: %s  A->P: %s  M->A: %s  A->M: %s\n"
                  "  stroke: %s (%u B)  drive: %.0f%%  inhibit: %s\n"
                  "Land-adjacent: %s  %s",
                  organism.id, organism.links.size(), organism.neuralAxons.size(),
                  organism.heading * 180.0f / 3.14159265f, organism.senseRadiusFactor,
                  static_cast<unsigned long long>(simTick != 0 ? simTick : organism.createdAtTick),
                  perceptorNode != nullptr ? perceptorNode->store.size() : 0,
                  perceptorNode != nullptr && perceptorNode->alive ? "alive" : "dead",
                  organism.lastPerceptScanPaid ? "paid" : "skipped", organism.lastPerceptBytesPaid,
                  mouthNode != nullptr ? mouthNode->store.size() : 0,
                  mouthNode != nullptr && mouthNode->alive ? "alive" : "dead",
                  mouthNode != nullptr && mouthNode->alive && mouthNode->ateThisTick ? "yes" : "no",
                  organism.lastMouthBiteDrive * 100.0f,
                  organism.lastMouthFeedSuppressed ? "yes (interoception)" : "no",
                  actuatorNode != nullptr ? actuatorNode->store.size() : 0,
                  actuatorNode != nullptr && actuatorNode->alive ? "alive" : "dead",
                  perceptFocusKindLabel(organism.lastPerceptFocusKind),
                  static_cast<unsigned>(organism.lastPerceptConfidence),
                  organism.lastPerceptBearing * 180.0f / 3.14159265f,
                  organism.lastPerceptRange * 100.0f, pToMRecv, pToARecv, mToPRecv, aToPRecv,
                  mToARecv, aToMRecv,
                  organism.lastStrokePaid ? "paid" : "skipped", organism.lastStrokeBytesPaid,
                  organism.lastActuatorNetDrive * 100.0f,
                  organism.lastActuatorInhibited ? "yes (interoception)" : "no",
                  organism.landAdjacent ? "yes" : "no", organism.alive ? "alive" : "dead");
    return buffer;
  }

  if (organism.hasActuatorNeurons() && !organism.hasMouthNeurons()) {
    const float strokeEfficiency = organism.lastIntendedThrust > 0.0f
                                       ? organism.lastMechanicalThrust / organism.lastIntendedThrust
                                       : 0.0f;
    const float displacementEfficiency = organism.lastIntendedThrust > 0.0f
                                             ? organism.lastDisplacement / organism.lastIntendedThrust
                                             : 0.0f;
    std::snprintf(buffer, sizeof(buffer),
                  "Nom #%u\n"
                  "Type: actuator (1 A, no mouth)\n"
                  "Storage: %zu bytes (%.2f d)\n"
                  "Heading: %.0f deg\n"
                  "Proprioception (last tick):\n"
                  "  displacement: %.4f\n"
                  "  gross thrust intent: %.4f\n"
                  "  mechanical thrust: %.4f (eta %.0f%%)\n"
                  "  stroke paid: %u B (%s)\n"
                  "  of stroke -> heat: %.2f B (not extra charge)\n"
                  "  stroke efficiency: %.2f  path/tide: %.2f\n"
                  "  tide delta: %+.5f\n"
                  "  crawl burn/tick: %u B (basal %u + stroke %u)\n"
                  "  in water: %s\n"
                  "  tumbled: %s\n"
                  "Land-adjacent: %s\n"
                  "Created: tick %llu\n"
                  "Status: %s",
                  organism.id, organism.bodyStorage.size(), daysRemaining,
                  organism.heading * 180.0f / 3.14159265f, organism.lastDisplacement,
                  organism.lastIntendedThrust, organism.lastMechanicalThrust,
                  kActuatorTranslationEta * 100.0f, organism.lastStrokeBytesPaid,
                  organism.lastStrokePaid ? "this tick" : "skipped",
                  organism.lastTranslationEntropyLoss, strokeEfficiency, displacementEfficiency,
                  organism.lastTideDelta, kActuatorCrawlCostPerTick, kStemCellBasalCostPerTick,
                  kActuatorStrokeCostPerTick, organism.lastInWater ? "yes" : "no — stranded",
                  organism.lastTumbled ? "yes" : "no", organism.landAdjacent ? "yes" : "no",
                  static_cast<unsigned long long>(organism.createdAtTick),
                  organism.alive ? "alive" : "dead");
    return buffer;
  }

  std::size_t localBytes = 0;
  for (const SkeletonNode& node : organism.nodes) {
    localBytes += node.store.size();
  }

  if (organism.hasNeuralAxons() && organism.mouthCount() == 2 && organism.nodes.size() == 2) {
    // Legacy star-mouth hub (Computer prototype) — not twin-mouth.
    const NeuralAxon* axon12 = organism.findNeuralAxon(1, 2);
    const NeuralAxon* axon21 = organism.findNeuralAxon(2, 1);
    char recv12Byte[8] = "—";
    if (axon12 != nullptr && axon12->lastReceived.valid) {
      std::snprintf(recv12Byte, sizeof(recv12Byte), "0x%02X", axon12->lastReceived.byte);
    }
    char recv21Byte[8] = "—";
    if (axon21 != nullptr && axon21->lastReceived.valid) {
      std::snprintf(recv21Byte, sizeof(recv21Byte), "0x%02X", axon21->lastReceived.byte);
    }
    std::snprintf(buffer, sizeof(buffer),
                  "Organism #%u\n"
                  "Type: star mouth hub (C prototype)\n"
                  "Nodes: 2  Bone: 1  Heading: %.0f deg\n"
                  "Body: %zu bytes (%.2f d)  Node stores: %zu\n"
                  "Axon M1->M2 feed:%d%% believe:%d%% last:0x%02X recv:%s\n"
                  "Axon M2->M1 feed:%d%% believe:%d%% last:0x%02X recv:%s\n"
                  "Land-adjacent: %s  tick %llu  %s",
                  organism.id, organism.heading * 180.0f / 3.14159265f, organism.bodyStorage.size(),
                  daysRemaining, localBytes,
                  axon12 != nullptr ? trustDisplayPercent(axon12->trustFeed) : 0,
                  axon12 != nullptr ? trustDisplayPercent(evolab::axonMaxBelieveTrust(*axon12)) : 0,
                  axon12 != nullptr ? axon12->lastSentByte : 0, recv12Byte,
                  axon21 != nullptr ? trustDisplayPercent(axon21->trustFeed) : 0,
                  axon21 != nullptr ? trustDisplayPercent(evolab::axonMaxBelieveTrust(*axon21)) : 0,
                  axon21 != nullptr ? axon21->lastSentByte : 0, recv21Byte,
                  organism.landAdjacent ? "yes" : "no",
                  static_cast<unsigned long long>(organism.createdAtTick),
                  organism.alive ? "alive" : "dead");
    return buffer;
  }

  std::snprintf(buffer, sizeof(buffer),
                "Organism #%u\n"
                "Type: kinetic mouth (%d M)\n"
                "Nodes: %zu  Links: %zu  Heading: %.0f deg\n"
                "Body storage: %zu bytes (%.2f d)\n"
                "Node stores: %zu bytes\n"
                "Land-adjacent: %s\n"
                "Created: tick %llu\n"
                "Status: %s",
                organism.id, organism.mouthCount(), organism.nodes.size(), organism.links.size(),
                organism.heading * 180.0f / 3.14159265f, organism.bodyStorage.size(),
                daysRemaining, localBytes, organism.landAdjacent ? "yes" : "no",
                static_cast<unsigned long long>(organism.createdAtTick),
                organism.alive ? "alive" : "dead");
  return buffer;
}

std::string formatOrganismHoverSummary(const Organism& organism) {
  char buffer[160];
  if (organism.isPmaNom()) {
    const SkeletonNode* perceptorNode = findPerceptorNode(organism);
    const SkeletonNode* mouthNode = findMouthNode(organism);
    const SkeletonNode* actuatorNode = findActuatorNode(organism);
    char senseSummary[16] = "—";
    if (organism.lastPerceptScanPaid) {
      std::snprintf(senseSummary, sizeof(senseSummary), "%u/7",
                    static_cast<unsigned>(organism.lastPerceptConfidence));
    }
    std::snprintf(buffer, sizeof(buffer),
                  "Hover: Nom #%u P-M-A P %zu M %zu A %zu sense %s",
                  organism.id,
                  perceptorNode != nullptr ? perceptorNode->store.size() : 0,
                  mouthNode != nullptr ? mouthNode->store.size() : 0,
                  actuatorNode != nullptr ? actuatorNode->store.size() : 0, senseSummary);
  } else if (organism.hasActuatorNeurons() && !organism.hasMouthNeurons()) {
    std::snprintf(buffer, sizeof(buffer), "Hover: Nom #%u actuator (d=%.3f, stroke %s)",
                  organism.id, organism.lastDisplacement,
                  organism.lastStrokePaid ? "paid" : "skipped");
  } else if (!organism.hasMouthNeurons() && !organism.hasActuatorNeurons()) {
    if (organism.hasPerceptorNeurons()) {
      std::snprintf(buffer, sizeof(buffer), "Hover: Nom #%u degraded P-M-A", organism.id);
    } else {
      std::snprintf(buffer, sizeof(buffer), "Hover: StemCell #%u (undifferentiated)", organism.id);
    }
  } else if (organism.hasNeuralAxons() && organism.mouthCount() == 2) {
    std::snprintf(buffer, sizeof(buffer), "Hover: Organism #%u star mouth hub", organism.id);
  } else {
    std::snprintf(buffer, sizeof(buffer), "Hover: Organism #%u kinetic mouth (%d M, %zu links)",
                  organism.id, organism.mouthCount(), organism.links.size());
  }
  return buffer;
}

}  // namespace evolab::game
