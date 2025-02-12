#pragma once

#include "Renderer/Shared.h"

namespace RK {

class UIRenderer
{
public:
    void Reset();

    void AddRectFilled(Vec2 inPos, Vec2 inSize, float inRadius, Vec4 inColor);
    void AddCircleFilled(Vec2 inPos, float inRadius, Vec4 inColor);

    Slice<const Vec4> GetDrawCommands() const { return Slice(m_DrawCommandBuffer); }
    Slice<const DrawCommandHeader> GetDrawCommandHeaders() const { return Slice(m_DrawCommandHeaderBuffer); }

private:
    Array<Vec4> m_DrawCommandBuffer;
    Array<DrawCommandHeader> m_DrawCommandHeaderBuffer;
};

extern UIRenderer g_UIRenderer;

}