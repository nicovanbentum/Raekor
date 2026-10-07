#include "PCH.h"
#include "UIRenderer.h"
#include "Iter.h"
#include "Renderer/Shared.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

namespace RK {

UIRenderer g_UIRenderer;

static constexpr uint32_t cFirstCodepoint = 32;
static constexpr uint32_t cLastCodepoint = 126;
static constexpr uint32_t cCodepointCount = cLastCodepoint - cFirstCodepoint + 1;


bool UIRenderer::LoadFont(const Path& inPath)
{
    File file = File(inPath, std::ios::binary | std::ios::in);

    if (!file.is_open())
    {
        gLogError("UI", "Failed to open font {}", inPath.string());
        return false;
    }

    Array<uint8_t> font_data = Array<uint8_t>(std::istreambuf_iterator<char>(file), {});

    stbtt_fontinfo font_info;

    if (!stbtt_InitFont(&font_info, font_data.data(), stbtt_GetFontOffsetForIndex(font_data.data(), 0)))
    {
        gLogError("UI", "Failed to parse font {}", inPath.string());
        return false;
    }

    const float scale = stbtt_ScaleForPixelHeight(&font_info, cFontAtlasPixelHeight);

    int ascent = 0, descent = 0, line_gap = 0;
    stbtt_GetFontVMetrics(&font_info, &ascent, &descent, &line_gap);

    struct GlyphBitmap
    {
        uint32_t mCodepoint;
        unsigned char* mPixels;
        int mWidth, mHeight, mOffsetX, mOffsetY;
        float mAdvance;
    };

    Array<GlyphBitmap> bitmaps;
    bitmaps.reserve(cCodepointCount);

    constexpr unsigned char cOnEdgeValue = 128;
    constexpr float cPixelDistanceScale = float(cOnEdgeValue) / float(cFontAtlasPadding);

    for (uint32_t codepoint = cFirstCodepoint; codepoint <= cLastCodepoint; codepoint++)
    {
        GlyphBitmap& bitmap = bitmaps.emplace_back();
        bitmap.mCodepoint = codepoint;
        bitmap.mPixels = stbtt_GetCodepointSDF(&font_info, scale, codepoint, cFontAtlasPadding, cOnEdgeValue, cPixelDistanceScale, &bitmap.mWidth, &bitmap.mHeight, &bitmap.mOffsetX, &bitmap.mOffsetY);

        int advance = 0, left_side_bearing = 0;
        stbtt_GetCodepointHMetrics(&font_info, codepoint, &advance, &left_side_bearing);
        bitmap.mAdvance = advance * scale;
    }

    uint32_t cursor_x = 1, cursor_y = 1, row_height = 0;

    struct Placement { uint32_t mX, mY; };
    Array<Placement> placements(bitmaps.size());

    for (const auto& [index, bitmap] : gEnumerate(bitmaps))
    {
        if (bitmap.mPixels == nullptr)
            continue;

        if (cursor_x + bitmap.mWidth + 1 > cFontAtlasWidth)
        {
            cursor_x = 1;
            cursor_y += row_height + 1;
            row_height = 0;
        }

        placements[index] = Placement { cursor_x, cursor_y };

        cursor_x += bitmap.mWidth + 1;
        row_height = glm::max(row_height, uint32_t(bitmap.mHeight));
    }

    const uint32_t atlas_height = std::bit_ceil(cursor_y + row_height + 1);

    Array<uint8_t> atlas(size_t(cFontAtlasWidth) * atlas_height, 0);
    HashMap<uint32_t, UIGlyph> glyphs;

    for (const auto& [index, bitmap] : gEnumerate(bitmaps))
    {
        UIGlyph& glyph = glyphs[bitmap.mCodepoint];
        glyph.mAdvance = bitmap.mAdvance;

        if (bitmap.mPixels == nullptr)
            continue;

        const Placement& placement = placements[index];

        for (int y = 0; y < bitmap.mHeight; y++)
            std::memcpy(&atlas[( placement.mY + y ) * cFontAtlasWidth + placement.mX], &bitmap.mPixels[y * bitmap.mWidth], bitmap.mWidth);

        glyph.mOffset = Vec2(float(bitmap.mOffsetX), float(bitmap.mOffsetY));
        glyph.mSize = Vec2(float(bitmap.mWidth), float(bitmap.mHeight));
        glyph.mUVRect = Vec4(
            float(placement.mX) / cFontAtlasWidth,
            float(placement.mY) / atlas_height,
            float(placement.mX + bitmap.mWidth) / cFontAtlasWidth,
            float(placement.mY + bitmap.mHeight) / atlas_height
        );

        stbtt_FreeSDF(bitmap.mPixels, nullptr);
    }

    Array<float> kerning(cCodepointCount * cCodepointCount, 0.0f);

    for (uint32_t first = 0; first < cCodepointCount; first++)
    {
        for (uint32_t second = 0; second < cCodepointCount; second++)
            kerning[first * cCodepointCount + second] = stbtt_GetCodepointKernAdvance(&font_info, first + cFirstCodepoint, second + cFirstCodepoint) * scale;
    }

    m_FontAtlas = std::move(atlas);
    m_FontAtlasHeight = atlas_height;
    m_FontAtlasVersion++;
    m_Glyphs = std::move(glyphs);
    m_Kerning = std::move(kerning);

    m_Ascent = ascent * scale;
    m_Descent = descent * scale;
    m_LineGap = line_gap * scale;

    gLogInfo("UI", "Built a {}x{} SDF font atlas from {}", cFontAtlasWidth, atlas_height, inPath.filename().string());

    return true;
}


void UIRenderer::Reset()
{
    m_Primitives.clear();
}


void UIRenderer::AddRectFilled(Vec2 inPos, Vec2 inSize, float inRadius, Vec4 inColor, float inSoftness)
{
    m_Primitives.push_back(UIPrimitive
    {
        .mColor = inColor,
        .mRect = Vec4(inPos, inPos + inSize),
        .mRadius = glm::min(inRadius, glm::min(inSize.x, inSize.y) * 0.5f),
        .mSoftness = inSoftness,
        .mType = UI_PRIMITIVE_RECT
    });
}


void UIRenderer::AddRect(Vec2 inPos, Vec2 inSize, float inRadius, float inThickness, Vec4 inColor)
{
    m_Primitives.push_back(UIPrimitive
    {
        .mColor = inColor,
        .mRect = Vec4(inPos, inPos + inSize),
        .mRadius = glm::min(inRadius, glm::min(inSize.x, inSize.y) * 0.5f),
        .mThickness = inThickness,
        .mType = UI_PRIMITIVE_RECT
    });
}


void UIRenderer::AddCircleFilled(Vec2 inCenter, float inRadius, Vec4 inColor, float inSoftness)
{
    m_Primitives.push_back(UIPrimitive
    {
        .mColor = inColor,
        .mRect = Vec4(inCenter - inRadius, inCenter + inRadius),
        .mRadius = inRadius,
        .mSoftness = inSoftness,
        .mType = UI_PRIMITIVE_CIRCLE
    });
}


const UIGlyph* UIRenderer::GetGlyph(uint32_t inCodepoint) const
{
    const auto glyph = m_Glyphs.find(inCodepoint);

    if (glyph != m_Glyphs.end())
        return &glyph->second;

    const auto fallback = m_Glyphs.find('?');
    return fallback != m_Glyphs.end() ? &fallback->second : nullptr;
}


float UIRenderer::GetKerning(uint32_t inFirst, uint32_t inSecond) const
{
    if (inFirst < cFirstCodepoint || inFirst > cLastCodepoint || inSecond < cFirstCodepoint || inSecond > cLastCodepoint)
        return 0.0f;

    return m_Kerning[( inFirst - cFirstCodepoint ) * cCodepointCount + ( inSecond - cFirstCodepoint )];
}


Vec2 UIRenderer::MeasureText(StringView inText, float inSize) const
{
    if (!HasFont())
        return Vec2(0.0f);

    const float scale = inSize / cFontAtlasPixelHeight;

    float width = 0.0f;
    float line_width = 0.0f;
    uint32_t line_count = 1;
    uint32_t previous = 0;

    for (const char character : inText)
    {
        const uint32_t codepoint = uint8_t(character);

        if (codepoint == '\n')
        {
            width = glm::max(width, line_width);
            line_width = 0.0f;
            previous = 0;
            line_count++;
            continue;
        }

        if (const UIGlyph* glyph = GetGlyph(codepoint))
            line_width += ( glyph->mAdvance + GetKerning(previous, codepoint) ) * scale;

        previous = codepoint;
    }

    width = glm::max(width, line_width);

    const float line_height = ( m_Ascent - m_Descent + m_LineGap ) * scale;
    return Vec2(width, ( m_Ascent - m_Descent ) * scale + line_height * ( line_count - 1 ));
}


float UIRenderer::AddText(Vec2 inPos, StringView inText, float inSize, Vec4 inColor, EUITextAlign inAlign)
{
    if (!HasFont() || inText.empty())
        return 0.0f;

    const float scale = inSize / cFontAtlasPixelHeight;
    const float line_height = ( m_Ascent - m_Descent + m_LineGap ) * scale;

    float widest_line = 0.0f;
    float baseline = inPos.y + m_Ascent * scale;

    size_t line_start = 0;

    while (line_start <= inText.size())
    {
        size_t line_end = inText.find('\n', line_start);

        if (line_end == StringView::npos)
            line_end = inText.size();

        const StringView line = inText.substr(line_start, line_end - line_start);
        const float line_width = MeasureText(line, inSize).x;

        float pen_x = inPos.x;

        if (inAlign == UI_TEXT_ALIGN_CENTER)
            pen_x -= line_width * 0.5f;
        else if (inAlign == UI_TEXT_ALIGN_RIGHT)
            pen_x -= line_width;

        uint32_t previous = 0;

        for (const char character : line)
        {
            const uint32_t codepoint = uint8_t(character);
            const UIGlyph* glyph = GetGlyph(codepoint);

            if (glyph == nullptr)
                continue;

            pen_x += GetKerning(previous, codepoint) * scale;

            if (glyph->mSize.x > 0.0f && glyph->mSize.y > 0.0f)
            {
                const Vec2 glyph_min = Vec2(pen_x, baseline) + glyph->mOffset * scale;
                const Vec2 glyph_max = glyph_min + glyph->mSize * scale;

                m_Primitives.push_back(UIPrimitive
                {
                    .mColor = inColor,
                    .mRect = Vec4(glyph_min, glyph_max),
                    .mUVRect = glyph->mUVRect,
                    .mRadius = scale,
                    .mType = UI_PRIMITIVE_GLYPH
                });
            }

            pen_x += glyph->mAdvance * scale;
            previous = codepoint;
        }

        widest_line = glm::max(widest_line, line_width);
        baseline += line_height;
        line_start = line_end + 1;
    }

    return widest_line;
}

}
