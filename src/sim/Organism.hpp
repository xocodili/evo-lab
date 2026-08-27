#pragma once



#include "sim/Energon.hpp"

#include "sim/CellConstants.hpp"
#include "sim/CloacaSignal.hpp"
#include "sim/NeuralAxon.hpp"
#include "sim/PerceptorFocus.hpp"



#include <array>
#include <cstdint>

#include <random>

#include <string>

#include <vector>



namespace evolab {



class BarrenWorld;

class EnergonField;



enum class NeuronType : std::uint8_t { None = 0, Mouth, Perceptor, Computer, Actuator };



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

  bool ateThisTick = false;

  std::uint8_t lastEmittedByte = 0;

  // Local perceptor focus (valid when neuron == Perceptor).
  float gazeHeading = 0.0f;
  PerceptFocusKind focusKind = PerceptFocusKind::None;
  float focusBearing = 0.0f;
  float focusRange = 1.0f;
  float focusSalience = 0.0f;
  bool focusLocked = false;
  std::uint8_t perceptConfidence = 0;

  // Consecutive ticks basal cost could not be paid from this neuron's fuel pool.
  std::uint16_t basalArrearsTicks = 0;

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

  std::vector<std::uint8_t> bodyStorage;

  bool landAdjacent = false;

  bool alive = true;

  float heading = 0.0f;

  // Heritable sensory phenotype (jittered once at spawn).
  float senseRadiusFactor = kPerceptorSenseRadiusFactor;

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
  float lastMouthBiteDrive = 0.0f;
  bool lastMouthFeedSuppressed = false;
  bool lastMouthHadFoodContact = false;
  std::uint8_t lastActuatorOutboundSignal = 0;
  bool lastInWater = false;
  float lastTideDelta = 0.0f;
  bool lastStrokePaid = false;
  bool lastTumbled = false;

  // Proprioception (P neuron): mirror of primary perceptor focus (inspector / debug).
  std::uint8_t lastPerceptConfidence = 0;
  PerceptFocusKind lastPerceptFocusKind = PerceptFocusKind::None;
  float lastPerceptBearing = 0.0f;
  float lastPerceptRange = 0.0f;
  bool lastPerceptScanPaid = false;
  std::uint32_t lastPerceptBytesPaid = 0;

  // Computer (C) hub — register template + runtime match/dispatch (CAMP only).
  std::array<std::uint8_t, kComputerRegisterBytes> computerRegister{};
  float lastComputerMatchScore = 0.0f;
  float computerFeedGain = 1.0f;
  bool lastHubSignalExpelledThisTick = false;
  CloacaBand lastCloacaBandExpelled = CloacaBand::None;
  std::uint32_t computerNodeId = 0;
  // Transient stroke flex injected into buildCampMusclePose (CAMP bundle locomotion).
  float lastActuatorStrokeFlexBoost = 0.0f;



  SkeletonNode* findNode(std::uint32_t nodeId);

  const SkeletonNode* findNode(std::uint32_t nodeId) const;

  NeuralAxon* findNeuralAxon(std::uint32_t srcNodeId, std::uint32_t dstNodeId);

  const NeuralAxon* findNeuralAxon(std::uint32_t srcNodeId, std::uint32_t dstNodeId) const;



  float rootWorldX() const;

  float rootWorldY() const;

  float rootWorldZ() const;



  void updateKinematics(const BarrenWorld& world, float cellSize, float heightScale);

  void advectRoot(const BarrenWorld& world, const EnergonField& energon, float cellSize,

                  float heightScale, float halfExtent);

  void metabolise(const BarrenWorld& world, float cellSize, float heightScale);

  void tickNeuronViability(EnergonField& field);

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

};



Organism makeUndifferentiatedOrganism(std::uint32_t id, float wx, float wz, float wy,

                                      std::size_t storageBytes, std::uint64_t createdAtTick);

Organism makeActuatorOrganism(std::uint32_t id, float wx, float wz, float wy,
                              std::size_t storageBytes, std::uint64_t createdAtTick);

Organism makeCampNomOrganism(std::uint32_t id, float wx, float wz, float wy, std::size_t storageBytes,
                             std::uint64_t createdAtTick, float boneLength);

Organism makeStarMouthOrganism(std::uint32_t id, float wx, float wz, float wy,
                               std::size_t storageBytes, std::uint64_t createdAtTick,
                               int mouthCount, float boneLength);

bool organismLandAdjacent(const BarrenWorld& world, float wx, float wz, float cellSize);



}  // namespace evolab

