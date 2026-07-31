#include "BaseScript.hpp"
#include "Model/GameContext.hpp"
#include "Model/TerrainAccess.hpp"
#include "Model/WorldSave.hpp"

#include <Bamboo/Input.hpp>
#include <Bamboo/Logger.hpp>

#include <algorithm>
#include <chrono>

void BaseScript::start() {
    GameContext::init();
    GameContext::s_blocksMaterial = material;
    GameContext::s_terrainMaterial = terrainMaterial;
    GameContext::s_markerMaterial = markerMaterial;

    if (!TerrainAccess::init()) {
        LOG_ERROR("Neverland: engine terrain is missing — add a 'Terrain' entity with Terrain3D");
        return;
    }
    // Постройки покрывают тот же участок, что и рельеф.
    GameContext::s_buildingGrid->init(
        TerrainAccess::worldMinX(), 0, TerrainAccess::worldMinZ(),
        TerrainAccess::worldMaxX() - TerrainAccess::worldMinX(), TerrainAccess::worldMaxY(),
        TerrainAccess::worldMaxZ() - TerrainAccess::worldMinZ(), material
    );

    // Загрузка мира одним проходом на фазу: рестор сейва раскладывает объекты БЕЗ меша
    // (чанки копят dirty), затем свет целиком, затем единый ремеш уже с готовым светом.
    using Clock = std::chrono::steady_clock;
    const auto phaseMs = [](Clock::time_point &phase) {
        const auto now = Clock::now();
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - phase).count();
        phase = now;
        return static_cast<long long>(ms);
    };
    Clock::time_point phase = Clock::now();

    // Восстановление сейва: правки рельефа поверх ассета + архитектурные объекты
    // (сейвы v2 несут одиночные кубы — мигрируются в Block-объекты).
    if (WorldSave *save = GameContext::s_worldSave) {
        TerrainAccess::restoreEdits(save->terrainEdits);
        GameContext::s_buildingGrid->restoreObjects(save->objects);
    }
    const long long restoreMs = phaseMs(phase);

    // Воксельный свет: прозрачность рельефа одним окном + полный пересчёт (старт мира —
    // единственное место полного прохода).
    const int minX = TerrainAccess::worldMinX();
    const int minZ = TerrainAccess::worldMinZ();
    const int sizeX = TerrainAccess::worldMaxX() - minX;
    const int sizeZ = TerrainAccess::worldMaxZ() - minZ;
    const int sizeY = TerrainAccess::worldMaxY();
    GameContext::s_lightGrid->init(minX, 0, minZ, sizeX, sizeY, sizeZ);
    const TerrainAccess::Window window =
        TerrainAccess::readWindow(minX, 0, minZ, minX + sizeX, sizeY, minZ + sizeZ);
    for (int y = 0; y < sizeY; y++) {
        for (int z = minZ; z < minZ + sizeZ; z++) {
            for (int x = minX; x < minX + sizeX; x++) {
                if (window.layerAt(x, y, z) != 0) {
                    GameContext::s_lightGrid->setTerrainOpaque(x, y, z, true);
                }
            }
        }
    }
    GameContext::s_lightGrid->recomputeAll(*GameContext::s_buildingGrid);
    const long long lightMs = phaseMs(phase);

    GameContext::s_buildingGrid->rebuildDirtyChunks();
    const long long meshMs = phaseMs(phase);

    GameContext::s_lightmap->init(*GameContext::s_lightGrid, terrainMaterial);
    const long long lightmapMs = phaseMs(phase);
    LOG_INFO(
        "Neverland: world load — restore %lld ms, light %lld ms, mesh %lld ms, lightmap %lld ms",
        restoreMs, lightMs, meshMs, lightmapMs
    );
}

void BaseScript::update(float dt) {
    updateAutosave(dt);
    if (GameContext::s_lightmap != nullptr && GameContext::s_lightGrid != nullptr) {
        GameContext::s_lightmap->update(*GameContext::s_lightGrid, dt);
    }
    updatePerfLog(dt);
    if (Input::isKeyPressed(Key::L)) { LOG_INFO("Hello Panda! var: %d", var); }
}

// Диагностика «тряски»: раз в 5 с — пиковый кадр и фоновая работа (ремеши, светокарта).
// В покое remesh и uploads должны быть 0; ненулевые в покое = источник дёрганий.
void BaseScript::updatePerfLog(float dt) {
    m_perfTimer += dt;
    m_perfFrames++;
    m_perfMaxDt = std::max(m_perfMaxDt, dt);
    if (m_perfTimer < 5.0f) { return; }
    const uint32_t remeshes =
        GameContext::s_buildingGrid != nullptr ? GameContext::s_buildingGrid->takeRemeshCount() : 0;
    const uint32_t uploads =
        GameContext::s_lightmap != nullptr ? GameContext::s_lightmap->takeUploadCount() : 0;
    const float frames = static_cast<float>(m_perfFrames);
    LOG_INFO(
        "Neverland perf: avg %.1f ms, max %.1f ms, remesh %u, lm %u, dPos %.4f | scripts/frame: "
        "player %.2f ms, blocks %.2f ms, hud %.2f ms",
        m_perfTimer / frames * 1000.f, m_perfMaxDt * 1000.f, remeshes, uploads,
        GameContext::s_camPosDeltaMax, GameContext::s_scriptMsPlayer / frames,
        GameContext::s_scriptMsBlocks / frames, GameContext::s_scriptMsHud / frames
    );
    GameContext::s_camPosDeltaMax = 0.f;
    GameContext::s_camLookDeltaMax = 0.f;
    GameContext::s_scriptMsPlayer = 0.0;
    GameContext::s_scriptMsBlocks = 0.0;
    GameContext::s_scriptMsHud = 0.0;
    m_perfTimer = 0.f;
    m_perfFrames = 0;
    m_perfMaxDt = 0.f;
}

void BaseScript::updateAutosave(float dt) {
    constexpr float AUTOSAVE_INTERVAL_SECONDS = 30.0f;
    m_autosaveTimer += dt;
    if (m_autosaveTimer < AUTOSAVE_INTERVAL_SECONDS) { return; }
    m_autosaveTimer = 0.0f;
    saveWorld();
}

void BaseScript::saveWorld() {
    WorldSave *save = GameContext::s_worldSave;
    if (save == nullptr || GameContext::s_buildingGrid == nullptr) { return; }
    const auto started = std::chrono::steady_clock::now();
    save->terrainEdits = TerrainAccess::editAccumulator();
    GameContext::s_buildingGrid->collectObjects(save->objects);
    save->saveToDisk();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - started)
                        .count();
    if (ms > 50) { LOG_WARN("Neverland: world save took %lld ms (frame spike)", (long long)ms); }
}

void BaseScript::shutdown() {
    saveWorld();
    TerrainAccess::deinit();
    GameContext::deinit();
}
