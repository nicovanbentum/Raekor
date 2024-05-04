#pragma once

#include "slice.h"

namespace RK {

class DebugRenderer
{
public:
    static constexpr auto cDefaultLineColor = Vec4(0.85, 0.3f, 0.1f, 1.0f);
    static constexpr auto cDefaultFillColor = Vec4(0.85, 0.3f, 0.1f, 1.0f);

    // line shapes
    void AddLine(const Vec3& inStart, const Vec3& inEnd, const Vec4& inColor = cDefaultLineColor);
    
    void AddLineCube(const Vec3& inMin, const Vec3& inMax, const Mat4x4& inTransform = Mat4x4(1.0f), const Vec4& inColor = cDefaultLineColor);
    void AddLineCone(const Vec3& inPos, const Vec3& inDir, float inLength, float inAngle, const Vec4& inColor = cDefaultLineColor);
    
    void AddLineSphere(const Vec3& inPos, float inRadius, const Vec4& inColor = cDefaultLineColor);
    void AddLineCircle(const Vec3& inPos, float inRadius, const Vec3& inDir, const Vec3& inAxis, const Vec4& inColor = cDefaultLineColor);
    void AddLineArrow(const Vec3& inPos, const Vec3& inDir, float inLength, float inExtent, const Vec4& inColor = cDefaultLineColor);
    
    // filled shapes
    void AddQuadColored(const Vec3& inV0, const Vec3& inV1, const Vec3& inV2, const Vec3& inV3, const Vec4& inColor = cDefaultFillColor);
    void AddQuadTextured(const Vec3& inV0, const Vec3& inV1, const Vec3& inV2, const Vec3& inV3, uint64_t inTexture);
    
    void AddCubeColored(const Vec3& inMin, const Vec3& inMax, const Vec4& inColor = cDefaultFillColor);

    void Reset(); // call once per frame as early as possible, will clear data!!

    Slice<Vec4> GetLinesToRender() const { return Slice(m_RenderLines); }
    Slice<Vec4> GetTrianglesToRender() const { return Slice(m_RenderTriangles); }

private:
    std::vector<Vec4> m_RenderLines; // xyz = pos, w = packed color
    std::vector<Vec4> m_RenderTriangles; // xyz = pos, w = packed color or texture index
};

extern DebugRenderer g_DebugRenderer;

}