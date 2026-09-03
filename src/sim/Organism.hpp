#pragma once



#include "sim/Energon.hpp"

#include "engine/kinematics/ArticulatedBodyState.hpp"
#include "engine/kinematics/KinematicSkeleton.hpp"
#include "sim/CellConstants.hpp"
#include "sim/CloacaSignal.hpp"
#include "sim/NeuralAxon.hpp"
#include "sim/OrganismActuator.hpp"
#include "sim/PerceptorFocus.hpp"



#include <array>
#include <cstdint>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>



namespace evolab {



class BarrenWorld;

class EnergonField;



enum class NeuronType : std::uint8_t { None = 0, Mouth, Perceptor, Computer, Actuator };

// Heritable stem assembly (stem BIND replay — docs/WORLD-BINDING-GRAMMAR.md).
enum class StemFace : std::uint8_t { North = 0, East = 1, South = 2, West = 3 };

struct StemBindRecord {
  std::uint32_t hubNodeId = 0;
  std::uint32_t peripheralNodeId = 0;
  std::uint8_t hubSlot = 0;
  std::uint8_t peripheralFace = static_cast<std::uint8_t>(StemFace::North);
  float restLength = 0.0f;
  bool muscleBundle = true;
};

// Colinear chain segment (torpedo): parent → child along heading + segmentAngleOffset.
struct StemChainRecord {
  std::uint32_t parentNodeId = 0;
  std::uint32_t childNodeId = 0;
  float segmentAngleOffset = 0.0f;
  float restLength = 0.0f;
  bool muscleBundle = true;
};

struct StemLocusSpec {
  std::uint32_t nodeId = 0;
  NeuronType type = NeuronType::None;
};

struct StemAssemblyPlan {
  std::vector<StemLocusSpec> loci;
  std::vector<StemBindRecord> binds;
  std::vector<StemChainRecord> chains;
};



// Joint / attachment site. World pose is filled each tick by engine forward kinematics
// (KinematicSkeleton + KinematicLocalPose — see docs/KINEMATICS.md) via Organism::updateKinematics.

struct SkeletonNode {

  std::uint32_t id = 0;

  NeuronType neuron = NeuronType::None;

  bool alive = true;

  float worldX = 0.0f;

  float worldY = 0.0f;

  float worldZ = 0.0f;

  std::vector<std::uint8_t> store;

  // Chew-buffer occupancy from field bites (0–kMouthLocalStoreMaxBytes); conveyance fuel does not
  // raise satiation broadcasts.
  std::uint32_t mouthChewFill = 0;
  // Latched when chew satiation crosses stop band; clears only below resume band (hysteresis).
  bool mouthChewPaused = false;

  bool ateThisTick = false;

  // Rolling diet composition (EMA per bite category) — postingestive confirmation for P→M prediction.
  float mouthDietSunfallEma = 0.0f;
  float mouthDietFragmentEma = 0.0f;
  float mouthDietCloacaDistressEma = 0.0f;
  float mouthDietCloacaBaselineEma = 0.0f;
  float mouthDietCloacaMateEma = 0.0f;

  // Extra-oral taste buds: omnidirectional food sampling + temporal gradient (Berg analogue).
  float mouthTasteSalience = 0.0f;
  float mouthTasteBearing = 0.0f;
  float mouthTastePriorSalience = 0.0f;
  float mouthTasteGradient = 0.0f;
  bool mouthTasteSampleValid = false;
  bool mouthTastePriorValid = false;
  // True when weighted taste vectors cancel (equidistant food ring — Berg / choice bias analogue).
  bool mouthTasteSymmetricAmbiguity = false;
  // Fixated coarse-grid sniff target (steer toward latch; P lock handles exploitation).
  bool mouthTasteLatchValid = false;
  float mouthTasteLatchWorldX = 0.0f;
  float mouthTasteLatchWorldZ = 0.0f;
  float mouthTasteLatchPeakBytes = 0.0f;
  std::uint64_t mouthTasteLatchTick = 0;

  std::uint8_t lastEmittedByte = 0;

  // Local perceptor focus (valid when neuron == Perceptor).
  float gazeHeading = 0.0f;
  PerceptFocusKind focusKind = PerceptFocusKind::None;
  float focusBearing = 0.0f;
  float focusRange = 1.0f;
  float focusSalience = 0.0f;
  bool focusLocked = false;
  std::uint8_t perceptConfidence = 0;
  // Ambient photoreception sample (0–7); outbound on P→M/C axons each paid scan.
  std::uint8_t perceptDiurnalConfidence = 0;
  float perceptSunIntensity = 0.0f;
  // Prior paid-scan best food Go/NoGo score (temporal chemotaxis gradient).
  float perceptPriorFoodSalience = 0.0f;
  bool perceptPriorFoodSalienceValid = false;

  // Consecutive ticks basal cost could not be paid from this neuron's fuel pool.
  std::uint16_t basalArrearsTicks = 0;

  // Prior-tick store bytes for Black Queen equilibrium (stop sharing while draining).
  std::size_t storeBytesPriorTick = 0;
  // Last computed export scale for this node (0 = retain, 1 = full surplus dispatch).
  float equilibriumExportScale = 0.0f;

  // Computer (C) node: pattern template + per-tick match/dispatch (valid when neuron == Computer).
  std::array<std::uint8_t, kComputerRegisterBytes> computerRegister{};
  float lastComputerMatchScore = 0.0f;
  float lastComputerPredictionError = 0.0f;
  float computerFeedGain = 1.0f;

  // Stem-cell Hz coordinator (every node; recursive mini-C inside full C neurons).
  std::array<std::uint8_t, kCoordinatorRegisterBytes> coordinatorRegister{};
  float coordinatorDutyScale = 1.0f;
  float coordinatorPriorExcitation = 0.0f;
  float coordinatorLastExcitation = 0.0f;
  float coordinatorLastDelta = 0.0f;

};



// Coupled mechanical bone (kinematic). Energy on this edge is optional (eta may be 0).
// Kinematic fields mirror engine::kinematics::KinematicBone for FK solvers.

struct SkeletonLink {

  std::uint32_t parentNodeId = 0;

  std::uint32_t childNodeId = 0;

  float restLength = 0.0f;

  // Bind local yaw (radians) relative to parent; used as KinematicBone::jointAngle at FK build.
  float jointAngle = 0.0f;

  float energyEta = 0.88f;

  // When true, joint flex follows the bidirectional believe axon bundle on this bone (evo-lab).
  bool muscleBundle = false;

};



struct ColonyAxon {

  std::uint32_t targetOrganismId = 0;

  float trust = 1.0f;

};



class Organism {

public:

  std::uint32_t id = 0;

  std::uint64_t createdAtTick = 0;

  std::uint32_t rootNodeId = 0;

  std::vector<SkeletonNode> nodes;

  std::vector<SkeletonLink> links;

  std::vector<NeuralAxon> neuralAxons;

  std::vector<ColonyAxon> colonyAxons;

  bool landAdjacent = false;

  bool alive = true;

  float heading = 0.0f;

  // Heritable sensory phenotype (jittered once at spawn).
  float senseRadiusFactor = kPerceptorSenseRadiusFactor;
  // Heritable run-and-tumble phenotype (Berg analogue; jittered at spawn / reproduction).
  float tumbleRateFactor = kDefaultTumbleRateFactor;
  float tumbleTurnFactor = kDefaultTumbleTurnFactor;
  // Signed bias: -1 prefers left tumbles, +1 prefers right (0 = fair coin).
  float tumbleChiralityBias = kDefaultTumbleChiralityBias;
  // Heritable mitochondrial wallet nominal caps (± jitter at spawn / reproduction).
  float peripheralStoreCapFactor = kDefaultPeripheralStoreCapFactor;
  float hubStoreCapFactor = kDefaultHubStoreCapFactor;
  // Heritable Black Queen export threshold (fill unit above which surplus may leave the node).
  float equilibriumExportStartUnit = kStemEquilibriumExportStartUnit;

  // Heritable cloaca palette tiers (cool distress, mid baseline, warm mate; jittered at spawn).
  std::uint8_t cloacaDistressByte = kEnergonPaletteDistress;
  std::uint8_t cloacaBaselineByte = kEnergonPaletteBaseline;
  std::uint8_t cloacaMateByte = kEnergonPaletteMate;

  // Proprioception (A neuron / pre-P): did the last stroke move us?
  float lastDisplacement = 0.0f;
  float lastIntendedThrust = 0.0f;
  float lastMechanicalThrust = 0.0f;
  float lastTranslationEntropyLoss = 0.0f;
  std::uint32_t lastStrokeBytesPaid = 0;
  std::uint32_t lastStrokeBytesFromBody = 0;
  std::uint32_t lastStrokeBytesFromActuatorStore = 0;
  bool lastActuatorInhibited = false;
  float lastActuatorNetDrive = 0.0f;
  float actuatorMouthInboundPriorUnit = 0.0f;
  bool actuatorMouthInboundPriorValid = false;
  ActuatorInteroception lastActuatorInteroception;
  MotorIntent lastMotorIntent;
  float lastMouthBiteDrive = 0.0f;
  bool lastMouthFeedSuppressed = false;
  bool lastMouthHadFoodContact = false;
  float lastMouthTasteSalience = 0.0f;
  float lastMouthTasteGradient = 0.0f;
  float lastMouthTasteBearing = 0.0f;
  bool lastMouthTasteSymmetricAmbiguity = false;
  std::uint8_t lastActuatorOutboundSignal = 0;
  bool lastInWater = false;
  float lastTideDelta = 0.0f;
  bool lastStrokePaid = false;
  bool lastTumbled = false;
  float campAdvectStartX = 0.0f;
  float campAdvectStartZ = 0.0f;
  CampActuatorProprioSnapshot campActuatorProprio;

  // R1 parthenogenesis telemetry (inspector / research).
  std::uint32_t lastParthenogenesisBytesSpent = 0;
  bool lastParthenogenesisSpawned = false;
  std::uint32_t offspringSpawnedCount = 0;
  std::uint64_t parthenogenesisCelebrationStartTick = 0;
  float parthenogenesisBirthHeading = 0.0f;
  bool feedbagOracle = false;
  bool disableTideAdvection = false;
  // Nursery harness: freeze stroke/tumble so babies graze in place (no drift, no crawl).
  bool disableNurseryLocomotion = false;
  // Nursery / chemotaxis harness: skip terrain boundary dry threat scans (scanBlocks).
  bool disableTerrainThreatScan = false;
  std::uint64_t lastParthenogenesisSuccessTick = 0;

  // Heritable stem assembly: locus inventory + hub bind records (see docs/WORLD-BINDING-GRAMMAR.md).
  StemAssemblyPlan stemAssembly;

  // Proprioception (P neuron): mirror of primary perceptor focus (inspector / debug).
  std::uint8_t lastPerceptConfidence = 0;
  std::uint8_t lastPerceptDiurnalConfidence = 0;
  float lastPerceptSunIntensity = 0.0f;
  PerceptFocusKind lastPerceptFocusKind = PerceptFocusKind::None;
  float lastPerceptBearing = 0.0f;
  float lastPerceptRange = 0.0f;
  bool lastPerceptScanPaid = false;
  std::uint32_t lastPerceptBytesPaid = 0;

  // Computer (C) hub — fused match/dispatch telemetry across live C nodes (CAMP only).
  float lastComputerMatchScore = 0.0f;
  float lastComputerPredictionError = 0.0f;
  float computerFeedGain = 1.0f;
  float coordinatorDutyScale = 1.0f;
  float coordinatorMinNodeDuty = 1.0f;
  float coordinatorMaxNodeDuty = 1.0f;
  // Organism-level feast/famine stress (0 = feast/abundance, 1 = famine/torpor).
  float famineUnit = 0.0f;
  std::uint8_t famineConfidence = kNeuronConfidenceNeutral;
  // Prior-tick C hub bytes for equilibrium export gating (Black Queen homeostasis).
  std::size_t hubBytesPriorTick = 0;
  float hubConservationExportScale = 0.0f;
  bool lastHubSignalExpelledThisTick = false;
  CloacaBand lastCloacaBandExpelled = CloacaBand::None;
  std::uint32_t computerNodeId = 0;

  // Articulated-body dynamics (engine E1/E2 — impulse at A, muscle PD flex).
  engine::kinematics::ArticulatedBodyState bodyDynamics;
  float pendingImpulseX = 0.0f;
  float pendingImpulseZ = 0.0f;
  std::uint32_t pendingImpulseNodeId = 0;
  float lastTideVelX = 0.0f;
  float lastTideVelZ = 0.0f;

  // Reused by syncKinematicsPose when it immediately follows updateKinematics (one build/tick).
  engine::kinematics::KinematicSkeleton kinematicsSkeletonScratch_;
  bool kinematicsSkeletonScratchValid_ = false;
  bool kinematicsBirthApplied_ = false;

  SkeletonNode* findNode(std::uint32_t nodeId);

  const SkeletonNode* findNode(std::uint32_t nodeId) const;

  NeuralAxon* findNeuralAxon(std::uint32_t srcNodeId, std::uint32_t dstNodeId);

  const NeuralAxon* findNeuralAxon(std::uint32_t srcNodeId, std::uint32_t dstNodeId) const;



  float rootWorldX() const;

  float rootWorldY() const;

  float rootWorldZ() const;



  void updateKinematics(const BarrenWorld& world, float cellSize, float heightScale);

  // FK-only pass from current bodyDynamics joint state (after clamp / pose edits).
  void syncKinematicsPose(const BarrenWorld& world, float cellSize, float heightScale);

  // Render-only: FK from bodyDynamics without mutating sim node positions.
  using RenderNodePoseMap = std::unordered_map<std::uint32_t, std::array<float, 3>>;
  bool sampleArticulatedRenderPoses(const BarrenWorld& world, float cellSize, float heightScale,
                                    RenderNodePoseMap& outPoses) const;

  // True when updateKinematics runs articulated dynamics (not static FK-only).
  bool usesArticulatedLocomotion() const;

  // Clamp root to world bounds; articulated bodies propagate delta without FK reset.
  void finalizeKinematicsBoundary(const BarrenWorld& world, float cellSize, float heightScale,
                                  float halfExtent);

  void advectRoot(const BarrenWorld& world, const EnergonField& energon, float cellSize,

                  float heightScale, float halfExtent);

  void metabolise(const BarrenWorld& world, float cellSize, float heightScale);

  void tickNeuronViability(EnergonField& field);
  void tickAxonTransitBasal();

  void feed(EnergonField& field, float cellSize, std::uint64_t simTick = 0);

  void perceive(const BarrenWorld& world, const EnergonField& energon, float cellSize,
                float halfExtent, const std::vector<Organism>& population,
                std::uint64_t simTick, float sunIntensity = 1.0f);

  void transferEnergy(EnergonField& field, float cellSize, std::uint64_t simTick = 0);

  void signal(EnergonField& field, std::uint64_t simTick);

  void transferColony();



  int mouthCount() const;
  int perceptorCount() const;
  int actuatorCount() const;

  bool hasMouthNeurons() const;
  bool hasPerceptorNeurons() const;
  bool hasActuatorNeurons() const;
  bool hasLiveActuatorNeurons() const;
  bool hasLiveFunctionalNeurons() const;
  bool isCampNom() const;

  void runDigestAndComputer(EnergonField& field, std::uint64_t simTick);

  void emitPreAdvectSignals(std::uint64_t simTick);

  bool hasNeuralAxons() const;

  // Single spawn hook: developmental axon trust + skeleton jitter (once per parameter).
  void finalizeSpawn(std::mt19937& rng);

  // Remove neural edges whose trust has degraded fully to zero (structural pruning).
  void pruneNeuralAxons();

  bool allLocalStoresEmpty() const;

  std::size_t totalFuelBytes() const;
  std::size_t computerHubFuelBytes() const;

};



Organism makeUndifferentiatedOrganism(std::uint32_t id, float wx, float wz, float wy,

                                      std::size_t storageBytes, std::uint64_t createdAtTick);

Organism makeActuatorOrganism(std::uint32_t id, float wx, float wz, float wy,
                              std::size_t storageBytes, std::uint64_t createdAtTick);

Organism makeCampNomOrganism(std::uint32_t id, float wx, float wz, float wy, std::size_t storageBytes,
                             std::uint64_t createdAtTick, float boneLength,
                             float spawnWorldYaw = 0.0f);

void ensureCampDevelopmentalAxons(Organism& organism);

// PMCCAM chain: two computers sharing one hub, for multi-C pattern/dispatch tests.
Organism makeDualComputerCampOrganism(std::uint32_t id, float wx, float wz, float wy,
                                      std::size_t storageBytes, std::uint64_t createdAtTick,
                                      float boneLength);

// Random chain mutant: 4–8 CAMP-class neurons, neighbour wiring, non-canonical topology.
Organism makeRandomCampMutant(std::uint32_t id, float wx, float wz, float wy,
                              std::size_t storageBytes, std::uint64_t createdAtTick,
                              float boneLength, std::uint64_t mutantSeed);

Organism makeStarMouthOrganism(std::uint32_t id, float wx, float wz, float wy,
                               std::size_t storageBytes, std::uint64_t createdAtTick,
                               int mouthCount, float boneLength);

bool organismLandAdjacent(const BarrenWorld& world, float wx, float wz, float cellSize);



}  // namespace evolab

