//
// Created by Bogdan on 29.12.2024.
//

#pragma once

#include <Bamboo/Base.hpp>
#include <Bamboo/Components/SpriteRendererComponentAPI.hpp>

using namespace Bamboo;

class Animation {
public:
    void start(
        EntityHandle spriteRendererEntity,
        TextureHandle texture,
        int cols,
        int rows,
        float swapTime = 0.25
    );

    void update(double dt);

    void reset();

private:
    int m_cols;
    int m_rows;
    float m_swapTime;
    float m_time;
    TextureHandle m_texture;
    int m_index;
    EntityHandle m_spriteRendererEntity;
};
