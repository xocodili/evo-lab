#include "app/VisualApp.hpp"

#include "app/CliArgs.hpp"
#include "app/StartupTrace.hpp"
#include "engine/FixedTimestepClock.hpp"
#include "engine/Viewport.hpp"
#include "engine/Camera.hpp"
#include "engine/gl/GlContext.hpp"
#include "engine/gfx/UiAssets.hpp"
#include "engine/gfx/UiFont.hpp"
#include "game/CellPicker.hpp"
#include "game/GameHud.hpp"
#include "game/GameInspector.hpp"
#include "game/GameRenderer.hpp"
#include "game/OrganismInspector.hpp"
#include "game/TerrainMesh.hpp"
#include "platform/InputFrame.hpp"
#include "platform/SdlPlatform.hpp"
#include "sim/BarrenWorld.hpp"
#include "sim/CellPopulation.hpp"
#include "sim/DayCycle.hpp"
#include "sim/Energon.hpp"
#include "sim/EnergonRain.hpp"
#include "sim/SimConfig.hpp"
#include "sim/SimDiagnostics.hpp"
#include "sim/WorldConstants.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <random>
#include <thread>

namespace evolab {

namespace {

void seedPopulation(CellPopulation& cells, const SimConfig& config, const BarrenWorld& world,
                    float cellSize, float heightScale) {
  switch (config.archetype) {
    case SeedArchetype::StemCell:
      cells.seedStemCells(world, cellSize, heightScale, config.nomCount, config.seed);
      break;
    case SeedArchetype::Actuator:
      cells.seedActuatorOrganisms(world, cellSize, heightScale, config.nomCount, config.seed);
      break;
    case SeedArchetype::Nom:
    default:
      cells.seedNoms(world, cellSize, heightScale, config.nomCount, config.seed);
      break;
  }
}

struct ViewportSize {
  int w = 0;
  int h = 0;
};

ViewportSize resolveViewport(const platform::SdlPlatform& platform, int designW, int designH) {
  ViewportSize out;
  platform.windowSize(out.w, out.h);
  if (out.w <= 0 || out.h <= 0) {
    out.w = designW;
    out.h = designH;
  }
  return out;
}

// Windows marks the process "Not Responding" if SDL events are not pumped during GL init.
class StartupEventPump {
public:
  explicit StartupEventPump(platform::SdlPlatform& platform) : platform_(platform) {
    thread_ = std::thread([this]() {
      while (running_.load(std::memory_order_relaxed)) {
        platform_.pumpEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    });
  }

  ~StartupEventPump() {
    running_.store(false, std::memory_order_relaxed);
    if (thread_.joinable()) {
      thread_.join();
    }
  }

  StartupEventPump(const StartupEventPump&) = delete;
  StartupEventPump& operator=(const StartupEventPump&) = delete;

private:
  platform::SdlPlatform& platform_;
  std::atomic<bool> running_{true};
  std::thread thread_;
};

void presentTerrainFrame(game::GameRenderer& renderer, const game::TerrainMesh& mesh,
                         const BarrenWorld& world, const engine::OrbitCamera& camera, int viewW,
                         int viewH) {
  const float waterLevel = world.waterLevel();
  renderer.beginFrame(0.53f, 0.75f, 0.92f);
  renderer.drawTerrain(camera, viewW, viewH);
  if (std::abs(world.waterLevelDelta()) <= 0.001f) {
    renderer.drawWaterPlane(mesh, waterLevel * kTerrainHeightScale, camera, viewW, viewH);
  }
}

}  // namespace

int runVisualApp(const CliArgs& args) {
  const SimConfig config = simConfigFromCli(args);
  const std::string windowTitle = windowTitleForConfig(config);
  StartupTrace trace;

  trace.step("begin");
  std::cout << "Generating terrain and hydrology (seed=" << config.seed
            << ", resolution=" << config.resolution << ", archetype="
            << seedArchetypeLabel(config.archetype) << ")...\n";
  std::cout.flush();

  BarrenWorld world(config.seed, config.resolution, makeTideFromConfig(config));
  trace.step("world");
  DayCycle dayCycle(kVisualDayCyclePeriodTicks);
  EnergonConfig energonConfig;
  energonConfig.populationScaledRain = true;
  energonConfig.maxBlobs = std::max(2200, config.nomCount * 80);
  EnergonField energon(config.seed, energonConfig);
  CellPopulation cells;
  game::TerrainMesh mesh = game::buildTerrainMesh(world.heightmap(), kWorldCellSize);
  trace.step("mesh");
  seedPopulation(cells, config, world, kWorldCellSize, kTerrainHeightScale);
  trace.step("seed");

  const CellPopulationStats seedStats = cells.stats();
  std::cout << "World ready (" << cells.organisms().size() << " organisms: "
            << seedStats.campNomOrganisms << " CAMP Noms, " << seedStats.stemCells
            << " stem, archetype=" << seedArchetypeLabel(config.archetype) << "). Opening window...\n";
  if (config.archetype == SeedArchetype::Nom && seedStats.campNomOrganisms == 0) {
    std::cerr << "WARNING: --archetype nom but no CAMP Noms were seeded. "
                 "Rebuild evo-lab.exe or check wet spawn sites.\n";
  }
  if (config.archetype == SeedArchetype::StemCell) {
    std::cerr << "NOTE: StemCell dev mode — perceptor/M-P-A wiring is inactive. "
                 "Use --archetype nom to test the CAMP Nom.\n";
  }
  std::cout.flush();

  platform::SdlPlatform platform;
  if (!platform.init(config.designWidth, config.designHeight, windowTitle.c_str())) {
    trace.step("platform_init_failed");
    return 1;
  }
  trace.open(platform.basePath() + "startup.trace");
  std::cout << "Startup trace: " << platform.basePath() << "startup.trace\n";
  std::cout.flush();
  trace.step("begin");
  trace.step("world");
  trace.step("mesh");
  trace.step("seed");
  trace.step("window");

  engine::OrbitCamera camera;
  camera.pitch = 0.55f;
  camera.distance = 140.0f;

  game::GameRenderer renderer;
  {
    StartupEventPump startupPump(platform);

    if (!engine::gl::loadGlContext()) {
      std::cerr << "Failed to load OpenGL functions\n";
      trace.step("gl_load_failed");
      return 1;
    }
    trace.step("gl_load");

    {
      engine::gl::GlContext& g = engine::gl::gl();
      g.clearColor(0.53f, 0.75f, 0.92f, 1.0f);
      g.clear(engine::gl::GlEnum::kColorBufferBit | engine::gl::GlEnum::kDepthBufferBit);
      platform.swap();
      platform.pumpEvents();
    }
    trace.step("first_clear");

    if (!renderer.init([&]() { platform.pumpEvents(); })) {
      trace.step("renderer_init_failed");
      return 1;
    }
    trace.step("renderer");

    renderer.uploadTerrainGeometry(mesh);
    trace.step("terrain_upload");

    const ViewportSize bootViewport = resolveViewport(platform, config.designWidth, config.designHeight);
    const engine::ViewportLayout bootLayout =
        engine::computeLetterbox(bootViewport.w, bootViewport.h, config.designWidth, config.designHeight);
    const int bootW = bootLayout.contentW > 0 ? bootLayout.contentW : bootViewport.w;
    const int bootH = bootLayout.contentH > 0 ? bootLayout.contentH : bootViewport.h;

    presentTerrainFrame(renderer, mesh, world, camera, bootW, bootH);
    platform.swap();
    platform.pumpEvents();
    trace.step("terrain_present");
  }

  engine::gfx::UiFont uiFont;
  game::GameHud hud;
  game::GameInspector inspector;
  bool uiReady = false;

  bool pauseSim = false;
  bool mouseDown = false;
  int frameIndex = 0;
  std::uint64_t visualSeed = config.seed;
  std::uint64_t lastTerrainColorTick = world.tickCount();
  float fps = 0.0f;

  engine::FixedTimestepClock simClock(config.fixedSimHz);
  auto lastTime = std::chrono::steady_clock::now();

  std::cout << "Phase 2.x — " << seedArchetypeLabel(config.archetype) << " Noms\n";
  std::cout << "Controls: drag=orbit, WASD=pan, scroll=zoom, Space=pause, R=regenerate, Esc=quit\n";
  std::cout << "Hover a Nom to inspect architecture.\n";
  std::cout.flush();
  trace.step("ready");
  lastTime = std::chrono::steady_clock::now();

  while (!platform.shouldClose()) {
    platform::InputFrame input;
    platform.poll(input, mouseDown);
    mouseDown = input.mouseLeftDown;

    if (!uiReady && frameIndex >= 2) {
      platform.pumpEvents();
      const std::string fontPath =
          engine::gfx::resolveAssetPath(platform.basePath(), engine::gfx::kDefaultUiFontRelPath);
      if (!uiFont.load(fontPath, engine::gfx::kDefaultUiFontPointSize)) {
        std::cerr << "UI font failed to load: " << fontPath << '\n';
      } else if (hud.init(uiFont) && inspector.init(uiFont)) {
        uiReady = true;
        trace.step("ui_ready");
      } else {
        std::cerr << "Diagnostics overlay disabled (HUD init failed).\n";
        uiReady = true;
      }
      platform.pumpEvents();
    }

    if (input.keyR) {
      std::random_device rd;
      visualSeed = static_cast<std::uint64_t>(rd());
      world.regenerate(visualSeed);
      energon.setSeed(visualSeed);
      mesh = game::buildTerrainMesh(world.heightmap(), kWorldCellSize);
      renderer.uploadTerrainGeometry(mesh);
      lastTerrainColorTick = ~0ULL;
      cells.clear();
      SimConfig regenConfig = config;
      regenConfig.seed = visualSeed;
      seedPopulation(cells, regenConfig, world, kWorldCellSize, kTerrainHeightScale);
      std::cout << "Regenerated world seed=" << visualSeed << '\n';
      std::cout.flush();
    }

    if (input.keySpace) {
      pauseSim = !pauseSim;
      std::cout << (pauseSim ? "Simulation paused\n" : "Simulation running\n");
      std::cout.flush();
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

    if (!pauseSim && dt > 0.0f && frameIndex >= 1) {
      const int steps = std::min(config.maxSimStepsPerFrame, simClock.advance(dt));
      for (int i = 0; i < steps; ++i) {
        world.tick();
        const float sun = dayCycle.sunIntensity(world.tickCount());
        const int rainPopulation = static_cast<int>(cells.organisms().size());
        energon.tick(world, sun, kWorldCellSize, kTerrainHeightScale, rainPopulation);
        cells.tick(world, energon, kWorldCellSize, kTerrainHeightScale, sun);
        if (i + 1 < steps) {
          platform.pumpEvents();
        }
      }
    }

    if (world.tickCount() != lastTerrainColorTick) {
      game::updateTerrainColors(mesh, world, kWorldCellSize);
      renderer.uploadTerrainColors(mesh);
      lastTerrainColorTick = world.tickCount();
    }

    const ViewportSize drawable = resolveViewport(platform, config.designWidth, config.designHeight);
    const engine::ViewportLayout viewport =
        engine::computeLetterbox(drawable.w, drawable.h, config.designWidth, config.designHeight);

    int pickMouseX = input.mouseX;
    int pickMouseY = input.mouseY;
    engine::mapScreenToDesign(input.mouseX, input.mouseY, viewport, pickMouseX, pickMouseY);

    float skyR = 0.53f;
    float skyG = 0.75f;
    float skyB = 0.92f;
    dayCycle.skyColor(world.tickCount(), skyR, skyG, skyB);

    const CellPopulationStats cellStats = cells.stats();

    SimDiagnostics diag;
    diag.fps = fps;
    diag.waterLevel = world.waterLevel();
    diag.tideMin = world.tide().minLevel();
    diag.tideMax = world.tide().maxLevel();
    diag.tidePhase =
        tidePhaseLabel(diag.waterLevel, diag.tideMin, diag.tideMax, world.tickCount(),
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
    diag.campNomOrganisms = cellStats.campNomOrganisms;
    diag.degradedNomOrganisms = cellStats.degradedNomOrganisms;
    diag.mouthOrganisms = cellStats.mouthOrganisms;
    diag.mouthNeurons = cellStats.mouthNeurons;
    diag.archetypeLabel = seedArchetypeLabel(config.archetype);

    const int viewW = viewport.contentW > 0 ? viewport.contentW : drawable.w;
    const int viewH = viewport.contentH > 0 ? viewport.contentH : drawable.h;

    renderer.beginFrame(skyR, skyG, skyB);
    renderer.drawTerrain(camera, viewW, viewH);
    if (std::abs(world.waterLevelDelta()) <= 0.001f) {
      renderer.drawWaterPlane(mesh, diag.waterLevel * kTerrainHeightScale, camera, viewW, viewH);
    }

    if (frameIndex >= 1) {
      renderer.drawEnergon(energon.blobs(), camera, viewW, viewH);
      renderer.drawOrganisms(cells.organisms(), camera, viewW, viewH);

      const std::uint32_t hoveredId =
          game::pickOrganismAtScreen(cells.organisms(), camera, viewW, viewH, pickMouseX, pickMouseY);
      if (hoveredId != 0) {
        if (const Organism* hovered = cells.findById(hoveredId)) {
          diag.hoveredCellSummary = game::formatOrganismHoverSummary(*hovered);
          if (uiReady) {
            inspector.draw(game::formatOrganismArchitectureLabel(*hovered, world.tickCount()), viewW,
                           viewH);
          }
        }
      } else {
        diag.hoveredCellSummary.clear();
      }
    }

    if (uiReady) {
      hud.draw(diag, viewW, viewH);
    }

    platform.swap();
    platform.pumpEvents();
    ++frameIndex;

    if (frameIndex == 1) {
      trace.step("frame1");
    }
  }

  if (uiReady) {
    inspector.shutdown();
    hud.shutdown();
  }
  renderer.shutdown();
  platform.shutdown();
  trace.step("shutdown");
  return 0;
}

}  // namespace evolab
