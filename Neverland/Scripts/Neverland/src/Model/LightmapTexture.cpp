#include "LightmapTexture.hpp"

#include <Bamboo/Assets/AssetManagerAPI.hpp>
#include <Bamboo/Assets/MaterialAPI.hpp>
#include <Bamboo/Assets/TextureAPI.hpp>
#include <Bamboo/Logger.hpp>

#include <cmath>

namespace {

constexpr float UPLOAD_COOLDOWN_SECONDS = 0.15f;

} // namespace

void LightmapTexture::init(const LightGrid &lightGrid, MaterialHandle terrainMaterial) {
    shutdown();
    if (!lightGrid.isReady()) { return; }
    // Тайлов в ряду — по числу Y-слоёв (например 64 слоя → 8×8).
    m_tilesPerRow = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(lightGrid.sizeY()))));
    m_atlasSize = m_tilesPerRow * lightGrid.sizeX();
    m_texture = AssetManagerAPI::createTextureRGBA8(
        static_cast<uint32_t>(m_atlasSize), static_cast<uint32_t>(m_atlasSize)
    );
    if (!m_texture.isValid()) {
        LOG_ERROR("LightmapTexture: runtime texture creation failed");
        return;
    }
    m_pixels.assign(static_cast<size_t>(m_atlasSize) * m_atlasSize * 4, 0);
    upload(lightGrid);
    if (terrainMaterial.isValid()) {
        MaterialAPI::setTexture(terrainMaterial, "lightMap", m_texture);
    }
}

void LightmapTexture::shutdown() {
    if (m_texture.isValid()) {
        AssetManagerAPI::deleteTexture(m_texture);
        m_texture = {};
    }
    m_pixels.clear();
    m_uploadedGeneration = 0;
    m_cooldown = 0.f;
}

void LightmapTexture::update(const LightGrid &lightGrid, float deltaTime) {
    if (!m_texture.isValid() || !lightGrid.isReady()) { return; }
    m_cooldown = std::max(0.f, m_cooldown - deltaTime);
    if (m_cooldown > 0.f || lightGrid.generation() == m_uploadedGeneration) { return; }
    upload(lightGrid);
}

void LightmapTexture::upload(const LightGrid &lightGrid) {
    const int cells = lightGrid.sizeX();
    for (int y = 0; y < lightGrid.sizeY(); y++) {
        const int tileX = (y % m_tilesPerRow) * cells;
        const int tileY = (y / m_tilesPerRow) * cells;
        for (int z = 0; z < lightGrid.sizeZ(); z++) {
            const size_t row =
                (static_cast<size_t>(tileY + z) * m_atlasSize + tileX) * 4;
            for (int x = 0; x < cells; x++) {
                const int worldX = lightGrid.minX() + x;
                const int worldY = lightGrid.minY() + y;
                const int worldZ = lightGrid.minZ() + z;
                const size_t index = row + static_cast<size_t>(x) * 4;
                m_pixels[index] = static_cast<unsigned char>(lightGrid.sunAt(worldX, worldY, worldZ) * 17);
                m_pixels[index + 1] =
                    static_cast<unsigned char>(lightGrid.blockAt(worldX, worldY, worldZ) * 17);
                m_pixels[index + 2] = 0;
                m_pixels[index + 3] = 255;
            }
        }
    }
    TextureAPI::updateRGBA8(
        m_texture, m_pixels.data(), static_cast<uint32_t>(m_pixels.size())
    );
    m_uploadedGeneration = lightGrid.generation();
    m_cooldown = UPLOAD_COOLDOWN_SECONDS;
}
