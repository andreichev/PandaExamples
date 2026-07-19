#pragma once

#include "LightGrid.hpp"

#include <Bamboo/Base.hpp>

#include <vector>

using namespace Bamboo;

// Светокарта рельефа: LightGrid упаковывается в 2D-атлас Y-слоёв (RGBA8: R — солнце,
// G — источники) и загружается в рантайм-текстуру движка; terrain-шейдер семплит её по
// worldPos. Аплоад — с дебаунсом по поколению LightGrid (полный, ~2.4 МБ — редкое событие).
class LightmapTexture final {
public:
    void init(const LightGrid &lightGrid, MaterialHandle terrainMaterial);
    void shutdown();

    // Дебаунс-аплоад: если поколение света изменилось и прошло минимум минимальный интервал.
    void update(const LightGrid &lightGrid, float deltaTime);

private:
    void upload(const LightGrid &lightGrid);

    TextureHandle m_texture;
    std::vector<unsigned char> m_pixels;
    int m_tilesPerRow = 0;
    int m_atlasSize = 0;
    uint32_t m_uploadedGeneration = 0;
    float m_cooldown = 0.f;
};
