//
// Created by Bogdan on 29.12.2024.
//

#include "Animation.hpp"
#include <Bamboo/Logger.hpp>

void Animation::start(
    EntityHandle spriteRendererEntity, TextureHandle texture, int cols, int rows, float swapTime
) {
    m_spriteRendererEntity = spriteRendererEntity;
    m_texture = texture;
    m_cols = cols;
    m_rows = rows;
    m_swapTime = swapTime;
    m_index = 0;
    m_time = 0;
}

void Animation::update(double dt) {
    m_time += dt;
    if (m_time >= m_swapTime) {
        m_index++;
        m_index %= (m_cols * m_rows);
        m_time = 0;
    }
    SpriteRendererComponentAPI::setTexture(m_spriteRendererEntity, m_texture);
    SpriteRendererComponentAPI::setCell(m_spriteRendererEntity, m_cols, m_rows, m_index);
}

void Animation::reset() {
    m_time = 0;
    m_index = 0;
}
