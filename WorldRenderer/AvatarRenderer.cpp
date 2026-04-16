#include "AvatarRenderer.h"

AvatarRenderer::AvatarRenderer(const Renderer2D& renderer)
: m_renderer(const_cast<Renderer2D&>(renderer))
{
    for(uint8 i = 0; i < BODY_PART_SIZE; ++i) {
        m_clothes[i] = ITEM_ID_BLANK;
    }

    m_skinColor = Color(255, 255, 255, 255);
}
