#include "pch.h"
#include "debug.h"

namespace RK {

DebugRenderer g_DebugRenderer;


uint32_t Float4ToRGBA8(Vec4 val)
{
    uint32_t packed = 0;
    packed += uint32_t(val.r * 255);
    packed += uint32_t(val.g * 255) << 8;
    packed += uint32_t(val.b * 255) << 16;
    packed += uint32_t(val.a * 255) << 24;
    return packed;
}


void DebugRenderer::AddLine(const Vec3& inStart, const Vec3& inEnd, const Vec4& inColor)
{
    const auto packed_color = std::bit_cast<float>( Float4ToRGBA8(inColor) );
    
    m_RenderLines.push_back(Vec4(inStart, packed_color));
    m_RenderLines.push_back(Vec4(inEnd, packed_color));
}


void DebugRenderer::AddLineCube(const Vec3& min, const Vec3& max, const Mat4x4& m, const Vec4& inColor)
{
    AddLine(Vec3(m * Vec4(min.x, min.y, min.z, 1.0)), Vec3(m * Vec4(max.x, min.y, min.z, 1.0f)), inColor);
    AddLine(Vec3(m * Vec4(max.x, min.y, min.z, 1.0)), Vec3(m * Vec4(max.x, max.y, min.z, 1.0f)), inColor);
    AddLine(Vec3(m * Vec4(max.x, max.y, min.z, 1.0)), Vec3(m * Vec4(min.x, max.y, min.z, 1.0f)), inColor);
    AddLine(Vec3(m * Vec4(min.x, max.y, min.z, 1.0)), Vec3(m * Vec4(min.x, min.y, min.z, 1.0f)), inColor);
    AddLine(Vec3(m * Vec4(min.x, min.y, min.z, 1.0)), Vec3(m * Vec4(min.x, min.y, max.z, 1.0f)), inColor);
    AddLine(Vec3(m * Vec4(max.x, min.y, min.z, 1.0)), Vec3(m * Vec4(max.x, min.y, max.z, 1.0f)), inColor);
    AddLine(Vec3(m * Vec4(max.x, max.y, min.z, 1.0)), Vec3(m * Vec4(max.x, max.y, max.z, 1.0f)), inColor);
    AddLine(Vec3(m * Vec4(min.x, max.y, min.z, 1.0)), Vec3(m * Vec4(min.x, max.y, max.z, 1.0f)), inColor);
    AddLine(Vec3(m * Vec4(min.x, min.y, max.z, 1.0)), Vec3(m * Vec4(max.x, min.y, max.z, 1.0f)), inColor);
    AddLine(Vec3(m * Vec4(max.x, min.y, max.z, 1.0)), Vec3(m * Vec4(max.x, max.y, max.z, 1.0f)), inColor);
    AddLine(Vec3(m * Vec4(max.x, max.y, max.z, 1.0)), Vec3(m * Vec4(min.x, max.y, max.z, 1.0f)), inColor);
    AddLine(Vec3(m * Vec4(min.x, max.y, max.z, 1.0)), Vec3(m * Vec4(min.x, min.y, max.z, 1.0f)), inColor);
}


void DebugRenderer::AddLineCone(const Vec3& inPos, const Vec3& inDir, float inLength, float inAngle, const Vec4& inColor)
{
    Vec3 target = inPos + inDir * inLength;

    Vec3 right = glm::normalize(glm::cross(inDir, glm::vec3(0.0f, 1.0f, 0.0f)));
    Vec3 up = glm::cross(right, inDir);

    // Calculate cone radius and step angle
    float half_angle = inAngle / 2.0f;
    float cone_radius = inLength * tan(half_angle);

    constexpr auto steps = 16u;
    constexpr auto step_angle = glm::radians(360.0f / float(steps));

    Vec3 last_point;

    for (int i = 0; i <= steps; i++)
    {
        float angle = step_angle * i;
        Vec3 circle_point = target + cone_radius * ( cos(angle) * right + sin(angle) * up );

        AddLine(inPos, circle_point, inColor);

        if (i > 0)
            AddLine(last_point, circle_point, inColor);

        last_point = circle_point;
    }
}


void DebugRenderer::AddLineCircle(const Vec3& inPos, float inRadius, const Vec3& inDir, const Vec3& inAxis, const Vec4& inColor)
{
    Vec3 point = inDir;

    constexpr float step_size = M_PI * 2.0f / 32.0f;

    for (float angle = 0.0f; angle < M_PI * 2.0f; angle += step_size)
    {
        Vec3 new_point = glm::toMat3(glm::angleAxis(step_size, inAxis)) * point;

        Vec3 start_point = inPos + point * inRadius;

        Vec3 end_point = inPos + new_point * inRadius;

        AddLine(start_point, end_point, inColor);

        point = new_point;
    }
}


void DebugRenderer::AddLineArrow(const Vec3& inPos, const Vec3& inDir, float inWidth, float inLength, const Vec4& inColor)
{
    Vec3 start_point = inPos;
    Vec3 end_point = start_point + inDir*inLength + inDir* 0.3f;

    for (int i = 0; i < 4; i++)
    {
        constexpr float up_dirs[4] = { 1.0f, -1.0f, 0.0f, 0.0f };
        constexpr float right_dirs[4] = { 0.0f, 0.0f, -1.0f, 1.0f };

        Vec3 right = glm::normalize(glm::cross(inDir, glm::vec3(0.0f, 1.0f, 0.0f)));
        Vec3 up = glm::normalize(glm::cross(right, inDir));

        up = up * up_dirs[i];
        right = right * right_dirs[i];

        Vec3 curr_point = start_point;
        Vec3 next_point = start_point + up * inWidth + right * inWidth;
        AddLine(curr_point, next_point, inColor);

        curr_point = next_point;
        next_point = curr_point + inDir * inLength;
        AddLine(curr_point, next_point, inColor);


        curr_point = next_point;
        next_point = curr_point + up * inWidth + right * inWidth;
        AddLine(curr_point, next_point, inColor);

        AddLine(next_point, end_point, inColor);
    }
}


void DebugRenderer::AddLineSphere(const Vec3& inPos, float inRadius, const Vec4& inColor)
{
    AddLineCircle(inPos, inRadius, Vec3(1, 0, 0), Vec3(0, 1, 0), inColor);
    AddLineCircle(inPos, inRadius, Vec3(0, 1, 0), Vec3(0, 0, 1), inColor);
    AddLineCircle(inPos, inRadius, Vec3(0, 0, 1), Vec3(1, 0, 0), inColor);
}


void DebugRenderer::Reset()
{
    m_RenderLines.clear();
    m_RenderTriangles.clear();
}


} // namespace Raekor