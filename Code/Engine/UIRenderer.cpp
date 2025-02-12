#include "PCH.h"
#include "UIRenderer.h"
#include "Renderer/Shared.h"

namespace RK {

UIRenderer g_UIRenderer;

void UIRenderer::Reset()
{
    m_DrawCommandBuffer.clear();
    m_DrawCommandHeaderBuffer.clear();
}

void UIRenderer::AddRectFilled(Vec2 inPos, Vec2 inSize, float inRadius, Vec4 inColor)
{
    DrawCommandHeader header = {};
    header.type = DRAW_COMMAND_RECT;
    header.startOffset = m_DrawCommandBuffer.size();
    m_DrawCommandHeaderBuffer.push_back(header);

    m_DrawCommandBuffer.push_back(inColor);
    m_DrawCommandBuffer.push_back(Vec4(inPos.x, inPos.y, inSize.x, inSize.y));
    m_DrawCommandBuffer.push_back(Vec4(inRadius, inRadius, inRadius, inRadius));
}

void UIRenderer::AddCircleFilled(Vec2 inPos, float inRadius, Vec4 inColor)
{
    DrawCommandHeader header = {};
    header.type = DRAW_COMMAND_CIRCLE_FILLED;
    header.startOffset = m_DrawCommandBuffer.size();
    m_DrawCommandHeaderBuffer.push_back(header);

    m_DrawCommandBuffer.push_back(inColor);
    m_DrawCommandBuffer.push_back(Vec4(inPos.x, inPos.y, inRadius, 0.0f));
}

}