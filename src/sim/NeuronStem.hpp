#pragma once

#include "sim/Energon.hpp"
#include "sim/Organism.hpp"
#include "sim/WorldBinding.hpp"

#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

namespace evolab {

// ---------------------------------------------------------------------------
// Stem-cell substrate (NeuronStem)
//
// Every differentiated neuron (P, M, C, A) extends the same stem layer:
//
//   1. Vital metabolism   — basal + operational costs, fresh energon ingress,
//                           hub vital spend above reserve (morphology alive)
//   2. Surplus metabolism — Black Queen export ramps (equilibrium scales)
//   3. Morphogenesis      — stem bind operator + camp endowment split
//   4. Neuron extensions  — P/M/C/A add registers, senses, dispatch on top
//
// Vital keeps nodes funded; surplus governs axon export above reserve.
// ---------------------------------------------------------------------------

// --- Camp spawn endowment (hub-first storage split) ------------------------

struct CampStorageSplit {
  std::size_t hubBytes = 0;
  std::size_t perceptorBytes = 0;
  std::size_t mouthBytes = 0;
  std::size_t actuatorBytes = 0;
};

CampStorageSplit splitCampStorage(std::size_t totalBytes);

struct EndowCampOptions {
  bool clampToWalletCaps = true;
};

void endowCampNodesFromSplit(Organism& organism, const CampStorageSplit& split,
                             EndowCampOptions options = {});

void endowCampNodes(Organism& organism, std::size_t storageBytes);

// Shared camp body-state (hub reserve, mate gates) gathered once per tick for C and P.
struct CampBodyInteroception {
  bool distress = false;
  bool mateReady = false;
};

void gatherCampBodyInteroception(const Organism& organism, std::uint64_t simTick,
                                 CampBodyInteroception& out);

// Actuator wallet + hub bytes above the vital floor — camp stroke may draw from both.
std::uint32_t campLocomotionFuelBytes(const Organism& organism);

// --- Stem morphogenesis bind (world socket grammar + inherited records) -----

struct StemBindAttempt {
  bool requireEntropy = false;
  bool requirePayment = false;
  float bindRateOverride = -1.0f;
  std::uint32_t bindCostBytes = kStemBindCostBytesDefault;
};

struct StemBindResult {
  bool ok = false;
  StemBindRecord record;
};

bool stemGlueMatches(std::uint8_t glueA, std::uint8_t glueB);

std::uint8_t inferStemHubSlotFromAngle(float heading, float jointAngle);

bool hubSlotTaken(const Organism& organism, std::uint32_t hubNodeId, std::uint8_t slot);

int nextFreeHubSlot(const Organism& organism, std::uint32_t hubNodeId);

StemBindResult tryStemBindPeripheralToHub(Organism& organism, std::uint32_t hubNodeId,
                                          std::uint32_t peripheralNodeId, std::uint8_t hubSlot,
                                          float restLength, float heading,
                                          const StemBindAttempt& attempt, std::mt19937& rng,
                                          Organism* payer = nullptr);

struct StemChainBindResult {
  bool ok = false;
  StemChainRecord record;
};

StemChainBindResult tryStemBindChainSegment(Organism& organism, std::uint32_t parentNodeId,
                                            std::uint32_t childNodeId, float segmentAngleOffset,
                                            float restLength, float heading,
                                            const StemBindAttempt& attempt, std::mt19937& rng,
                                            Organism* payer = nullptr);

// --- Fuel pools (shared by all nodes) --------------------------------------

SkeletonNode* findNeuronNode(Organism& organism, NeuronType type, bool requireAlive = true);
const SkeletonNode* findNeuronNode(const Organism& organism, NeuronType type,
                                   bool requireAlive = true);

std::vector<std::uint8_t>* neuronFuelPool(Organism& organism, SkeletonNode& node);
const std::vector<std::uint8_t>* neuronFuelPool(const Organism& organism,
                                                const SkeletonNode& node);

void consumeFuelBack(std::vector<std::uint8_t>& storage, std::size_t count);

// --- Vital metabolism (stem core; all neuron types) ------------------------

// Bytes the C hub must retain before vital draw (reserve + conservation slack).
std::size_t stemHubVitalSpendFloorBytes();

bool stemHubHasVitalSpendRoom(const Organism& organism, std::size_t bytes);

bool tryConsumeNodeFuel(SkeletonNode& node, std::size_t bytes);

// Draw from C hub only when fill stays above the vital floor (not surplus export).
bool tryConsumeHubVitalFuel(Organism& organism, std::size_t bytes);

// Basal maintenance (1 B/tick); camp coordinator may probabilistically skip payment.
bool tryPayStemBasalCost(Organism& organism, SkeletonNode& node);

// Operational cost (e.g. empty-string mastication tax); node wallet then hub vital.
bool tryPayStemOperationalCost(Organism& organism, SkeletonNode& node, std::size_t bytes);

// Fresh field energon ingress; camp mouths credit hub first, then local overflow.
void creditStemFreshEnergon(Organism& organism, SkeletonNode& node, std::uint8_t byte,
                            std::uint32_t units);

void expelByteAtNode(const SkeletonNode& node, EnergonField& field, std::uint8_t byte,
                     EnergonOrigin origin, float ttlScale, float zOffsetFactor = 0.0f);

void releaseFuelAtNode(const SkeletonNode& node, EnergonField& field,
                       std::vector<std::uint8_t>& storage, EnergonOrigin origin,
                       float ttlScale);

// --- Surplus metabolism (Black Queen export) --------------------------------

struct StemEquilibriumParams {
  std::size_t currentBytes = 0;
  std::size_t priorBytes = 0;
  std::size_t cap = 0;
  std::size_t reserveBytes = 0;
  std::size_t slackBytes = 0;
  std::size_t drainToleranceBytes = kStemEquilibriumDrainToleranceBytes;
  float exportStartUnit = kStemEquilibriumExportStartUnit;
  float exportFullUnit = 1.0f;
};

enum class StemSurplusRefreshPoint : std::uint8_t {
  PostDigest,    // after feed + digest, before coordinator / mouth convey
  PreComputer,   // tickComputerPhase (post-coordinator duty)
  PreConveyance, // conveyCampEnergon (post-metabolism / viability)
};

float stemEquilibriumExportScale(const StemEquilibriumParams& params);
float stemHubDispatchExportScale(const StemEquilibriumParams& params);
float stemNodeEquilibriumExportScale(const Organism& organism, const SkeletonNode& node);

void refreshStemSurplusExportScales(Organism& organism, StemSurplusRefreshPoint point);

bool campMouthAteThisTick(const Organism& organism);

// --- CAMP neuron extensions (signal emit hooks on stem substrate) ------------

void emitCampPreAdvectSignals(Organism& organism, std::uint64_t simTick);
void emitCampActuatorSignals(Organism& organism, std::uint64_t simTick);

}  // namespace evolab
