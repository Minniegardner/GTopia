#pragma once

#include "Precompiled.h"
#include "ItemUtils.h"
#include "Math/Color.h"
#include "Renderer2D.h"

class AvatarRenderer {
public:
    AvatarRenderer(const Renderer2D& renderer);
    ~AvatarRenderer();

private:
    uint16 m_clothes[BODY_PART_SIZE];
    Color m_skinColor;

    Renderer2D& m_renderer;
};