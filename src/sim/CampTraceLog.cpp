#include "sim/CampTraceLog.hpp"

#include "sim/CampTopology.hpp"
#include "sim/CellConstants.hpp"
#include "sim/NeuralAxon.hpp"
#include "sim/OrganismNeuron.hpp"
#include "sim/PerceptorFocus.hpp"

#include <cmath>
#include <iomanip>

namespace evolab {

namespace {

const char* focusKindLabel(PerceptFocusKind kind) {
  switch (kind) {
    case PerceptFocusKind::Food:
      return "food";
    case PerceptFocusKind::Mate:
      return "mate";
    case PerceptFocusKind::Threat:
      return "threat";
    case PerceptFocusKind::None:
    default:
      return "none";
  }
}

}  // namespace

bool CampTraceLog::open(const std::string& path) {
  close();
  out_.open(path, std::ios::out | std::ios::trunc);
  return out_.is_open();
}

void CampTraceLog::close() {
  if (out_.is_open()) {
    out_.close();
  }
}

void CampTraceLog::writeHeader(std::uint64_t seed, float targetX, float targetZ) {
  if (!out_) {
    return;
  }
  out_ << "# camp chemotaxis trace seed=" << seed << " target=(" << targetX << ',' << targetZ
       << ")\n";
  out_ << "# axon bytes 0-7 confidence (4=neutral); A stroke cost="
       << kActuatorStrokeCostPerTick << " bytes/tick\n";
  out_.flush();
}

std::uint8_t CampTraceLog::inboundAxonByte(const Organism& organism, std::uint32_t srcId,
                                           std::uint32_t dstId, std::uint64_t simTick) {
  const NeuralAxon* axon = organism.findNeuralAxon(srcId, dstId);
  if (axon == nullptr || !axon->lastReceived.valid || axon->lastReceived.tick != simTick) {
    return 0;
  }
  return axon->lastReceived.byte;
}

void CampTraceLog::recordTick(std::uint64_t simTick, const Organism& organism, float targetX,
                              float targetZ, float sunIntensity) {
  if (!out_) {
    return;
  }

  const float rootX = organism.rootWorldX();
  const float rootZ = organism.rootWorldZ();
  const float dx = targetX - rootX;
  const float dz = targetZ - rootZ;
  const float dist = std::sqrt(dx * dx + dz * dz);
  const float bearing = std::atan2(dx, dz);

  const std::uint8_t pToA = inboundAxonByte(organism, kCampPerceptorId, kCampActuatorId, simTick);
  const std::uint8_t pToM = inboundAxonByte(organism, kCampPerceptorId, kCampMouthId, simTick);
  const std::uint8_t cToA = inboundAxonByte(organism, kCampComputerId, kCampActuatorId, simTick);
  const std::uint8_t mToA = inboundAxonByte(organism, kCampMouthId, kCampActuatorId, simTick);
  const std::uint8_t aToP = inboundAxonByte(organism, kCampActuatorId, kCampPerceptorId, simTick);
  const std::uint8_t mToP = inboundAxonByte(organism, kCampMouthId, kCampPerceptorId, simTick);

  const SkeletonNode* actuatorNode = organism.findNode(kCampActuatorId);
  const std::size_t actuatorFuel =
      actuatorNode != nullptr ? actuatorNode->store.size() : 0;

  const ActuatorInteroception& intero = organism.lastActuatorInteroception;
  const MotorIntent& intent = organism.lastMotorIntent;

  out_ << "tick=" << simTick << std::fixed << std::setprecision(3) << " sun=" << sunIntensity
       << " root=(" << rootX << ',' << rootZ << ") heading=" << organism.heading
       << " dist=" << dist << " bearing=" << bearing
       << " focus=" << focusKindLabel(intero.focusKind)
       << " locked=" << (intero.perceptorLocked ? 1 : 0)
       << " conf=" << static_cast<int>(organism.lastPerceptConfidence) << " P_A="
       << static_cast<int>(pToA) << " P_M=" << static_cast<int>(pToM)
       << " M_A=" << static_cast<int>(mToA)
       << " C_A=" << static_cast<int>(cToA) << " M_P=" << static_cast<int>(mToP)
       << " A_P=" << static_cast<int>(aToP)
       << " A_store=" << actuatorFuel << " appr=" << intero.approach << " flee=" << intero.flee
       << " sat=" << intero.satiation << " hub_sat=" << intero.hubSatiation
       << " m_conf=" << intero.mouthConfidence << " drive=" << intent.netDrive
       << " stroke_req=" << intent.strokeBytes << " inhib=" << (intent.motorSuppressed ? 1 : 0)
       << " turn=" << intent.turnRateScale << " tumble=" << intent.tumbleRateScale
       << " stroke=" << (organism.lastStrokePaid ? 1 : 0)
       << " stroke_paid=" << organism.lastStrokeBytesPaid << " in_water="
       << (organism.lastInWater ? 1 : 0) << " disp=" << organism.lastDisplacement
       << " hub=" << organism.bodyStorage.size()
       << " bite=" << (organism.lastMouthHadFoodContact ? 1 : 0) << '\n';
  out_.flush();
}

}  // namespace evolab
