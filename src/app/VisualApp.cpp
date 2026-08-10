#include "app/VisualApp.hpp"

#include "sim/SimDiagnostics.hpp"
#include "engine/Camera.hpp"
#include "engine/gl/GlContext.hpp"
#include "engine/gfx/UiAssets.hpp"
#include "engine/gfx/UiFont.hpp"
#include "game/CellPicker.hpp"
#include "game/GameHud.hpp"
#include "game/GameInspector.hpp"
#include "game/GameRenderer.hpp"
#include "game/TerrainMesh.hpp"
#include "platform/InputFrame.hpp"
#include "platform/SdlPlatform.hpp"
#include "sim/BarrenWorld.hpp"
#include "sim/CellPopulation.hpp"
#include "sim/DayCycle.hpp"
#include "sim/Energon.hpp"
#include "sim/EnergonStats.hpp"
#include "sim/WorldConstants.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <random>

namespace evolab {

namespace {

constexpr int kInitialActuatorNomCount = 60;

}  // namespace

int runVisualApp(const CliArgs& args) {
  platform::SdlPlatform platform;
  if (!platform.init(1280, 720, "evo-lab — Phase 2.x Actuator Nom")) {
    return 1;
  }
  if (!engine::gl::loadGlContext()) {
    std::cerr << "Failed to load OpenGL functions\n";
    return 1;
  }

  game::GameRenderer renderer;
  if (!renderer.init()) {
    return 1;
  }

  engine::gfx::UiFont uiFont;
  const std::string fontPath =
      engine::gfx::resolveAssetPath(platform.basePath(), engine::gfx::kDefaultUiFontRelPath);
  if (!uiFont.load(fontPath, engine::gfx::kDefaultUiFontPointSize)) {
    std::cerr << "UI font failed to load: " << fontPath << '\n';
  }

  game::GameHud hud;
  if (!uiFont.loaded() || !hud.init(uiFont)) {
    std::cerr << "Diagnostics overlay disabled (font or HUD init failed).\n";
  }

  game::GameInspector inspector;
  if (!uiFont.loaded() || !inspector.init(uiFont)) {
    std::cerr << "Cell inspector disabled (font init failed).\n";
  }

  std::cout << "Building terrain and hydrology (seed=" << args.seed
            << ", resolution=" << args.resolution << ")...\n";
  std::cout.flush();

  BarrenWorld world(args.seed, args.resolution);
  DayCycle dayCycle(1800.0f);
  EnergonConfig energonConfig;
  energonConfig.spawnRateMax = 14.0f;
  energonConfig.maxBlobs = 2200;
  EnergonField energon(args.seed, energonConfig);
  CellPopulation cells;

  game::TerrainMesh mesh = game::buildTerrainMesh(world.heightmap(), kWorldCellSize);
  renderer.uploadTerrainGeometry(mesh);
  engine::OrbitCamera camera;
  camera.pitch = 0.55f;
  camera.distance = 140.0f;

  cells.seedActuatorOrganisms(world, kWorldCellSize, kTerrainHeightScale, kInitialActuatorNomCount,
                              args.seed);

  bool pauseSim = false;
  bool mouseDown = false;
  std::uint64_t visualSeed = args.seed;
  float fps = 0.0f;

  auto lastTime = std::chrono::steady_clock::now();
  constexpr int kMaxSimStepsPerFrame = 5;

  std::cout << "Phase 2.x — Actuator Noms (1 A, no mouth — crawl until starved)\n";
  std::cout << "Controls: drag=orbit, WASD=pan, scroll=zoom, Space=pause, R=regenerate, Esc=quit\n";
  std::cout << "Hover a cell orb to inspect its genome.\n";

  while (!platform.shouldClose()) {
    platform::InputFrame input;
    platform.poll(input, mouseDown);
    mouseDown = input.mouseLeftDown;

    if (input.keyR) {
      std::random_device rd;
      visualSeed = static_cast<std::uint64_t>(rd());
      world.regenerate(visualSeed);
      energon.setSeed(visualSeed);
      mesh = game::buildTerrainMesh(world.heightmap(), kWorldCellSize);
      renderer.uploadTerrainGeometry(mesh);
      cells.clear();
      cells.seedActuatorOrganisms(world, kWorldCellSize, kTerrainHeightScale,
                                  kInitialActuatorNomCount, visualSeed);
      std::cout << "Regenerated world seed=" << visualSeed << '\n';
    }

    if (input.keySpace) {
      pauseSim = !pauseSim;
      std::cout << (pauseSim ? "Simulation paused\n" : "Simulation running\n");
    }

    if (mouseDown) {
      camera.orbit(static_cast<float>(input.mouseDeltaX) * 0.005f,
                   static_cast<float>(input.mouseDeltaY) * 0.005f);
    }
    if (input.scrollDelta != 0) {
      camera.zoom(static_cast<float>(-input.scrollDelta) * 6.0f);
    }

    const auto now = std::chrono::steady_clock::now();
    const float dt = std::chrono::duration<float>(now - lastTime).count();
    lastTime = now;
    if (input.moveForward != 0.0f || input.moveRight != 0.0f) {
      const float panSpeed = 52.0f;
      camera.pan(input.moveRight * panSpeed * dt, input.moveForward * panSpeed * dt);
    }
    if (dt > 0.0f) {
      const float instantFps = 1.0f / dt;
      fps = fps <= 0.0f ? instantFps : fps * 0.88f + instantFps * 0.12f;
    }

    if (!pauseSim && dt > 0.0f) {
      const int steps =
          std::min(kMaxSimStepsPerFrame, std::max(1, static_cast<int>(dt * 60.0f)));
      for (int i = 0; i < steps; ++i) {
        world.tick();
        const float sun = dayCycle.sunIntensity(world.tickCount());
        energon.tick(world, sun, kWorldCellSize, kTerrainHeightScale);
        cells.tick(world, energon, kWorldCellSize, kTerrainHeightScale);
      }
    }

    const float waterLevel = world.waterLevel();
    game::updateTerrainColors(mesh, world, kWorldCellSize);
    renderer.uploadTerrainColors(mesh);

    int w = 0;
    int h = 0;
    platform.windowSize(w, h);

    float skyR = 0.53f;
    float skyG = 0.75f;
    float skyB = 0.92f;
    dayCycle.skyColor(world.tickCount(), skyR, skyG, skyB);

    const CellPopulationStats cellStats = cells.stats();

    SimDiagnostics diag;
    diag.fps = fps;
    diag.waterLevel = waterLevel;
    diag.tideMin = world.tide().minLevel();
    diag.tideMax = world.tide().maxLevel();
    diag.tidePhase =
        tidePhaseLabel(waterLevel, diag.tideMin, diag.tideMax, world.tickCount(),
                       world.tide().config().periodTicks);
    diag.sunIntensity = dayCycle.sunIntensity(world.tickCount());
    dayCycle.clockTime(world.tickCount(), diag.clockHours, diag.clockMinutes);
    diag.dayNight = dayCycle.dayNightLabel(world.tickCount());
    diag.simTick = world.tickCount();
    diag.seed = visualSeed;
    diag.paused = pauseSim;
    diag.energonCap = energonConfig.maxBlobs;
    diag.energon = computeEnergonStats(energon);
    diag.liveCells = cellStats.liveCells;
    diag.organisms = cellStats.organisms;
    diag.stemCells = cellStats.stemCells;
    diag.mouthOrganisms = cellStats.mouthOrganisms;
    diag.mouthNeurons = cellStats.mouthNeurons;

    renderer.beginFrame(skyR, skyG, skyB);
    renderer.drawTerrain(camera, w, h);
    if (std::abs(world.waterLevelDelta()) <= 0.001f) {
      renderer.drawWaterPlane(mesh, waterLevel * kTerrainHeightScale, camera, w, h);
    }
    renderer.drawEnergon(energon.blobs(), camera, w, h);
    renderer.drawOrganisms(cells.organisms(), camera, w, h);

    const std::uint32_t hoveredId =
        game::pickOrganismAtScreen(cells.organisms(), camera, w, h, input.mouseX, input.mouseY);
    if (hoveredId != 0) {
      if (const Organism* hovered = cells.findById(hoveredId)) {
        diag.hoveredCellSummary = hovered->hoverSummary();
        inspector.draw(hovered->architectureLabel(), w, h);
      }
    } else {
      diag.hoveredCellSummary.clear();
    }

    hud.draw(diag, w, h);

    platform.swap();
  }

  inspector.shutdown();
  hud.shutdown();
  renderer.shutdown();
  platform.shutdown();
  return 0;
}

}  // namespace evolab
