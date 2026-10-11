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
    UIFont font;

    if (!BuildFont(inPath, font))
        return false;

    if (m_Fonts.empty())
        m_Fonts.push_back(std::move(font));
    else
        m_Fonts[0] = std::move(font);

    m_FontsVersion++;
    return true;
}


int UIRenderer::AddFont(const Path& inPath)
{
    for (const auto& [index, font] : gEnumerate(m_Fonts))
    {
        if (font.mPath == inPath)
            return int(index);
    }

    UIFont font;

    if (!BuildFont(inPath, font))
        return 0;

    if (m_Fonts.empty())
        m_Fonts.emplace_back();

    m_Fonts.push_back(std::move(font));
    m_FontsVersion++;

    return int(m_Fonts.size()) - 1;
}


bool UIRenderer::BuildFont(const Path& inPath, UIFont& outFont) const
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

    outFont.mPath = inPath;
    outFont.mAtlas = std::move(atlas);
    outFont.mAtlasHeight = atlas_height;
    outFont.mGlyphs = std::move(glyphs);
    outFont.mKerning = std::move(kerning);

    outFont.mAscent = ascent * scale;
    outFont.mDescent = descent * scale;
    outFont.mLineGap = line_gap * scale;

    gLogInfo("UI", "Built a {}x{} SDF font atlas from {}", cFontAtlasWidth, atlas_height, inPath.filename().string());

    return true;
}


void UIRenderer::Reset()
{
    m_Primitives.clear();
    m_CurrentFont = 0;
}


const UIFont* UIRenderer::GetCurrentFont() const
{
    if (m_CurrentFont > 0 && m_CurrentFont < int(m_Fonts.size()) && !m_Fonts[m_CurrentFont].mAtlas.empty())
        return &m_Fonts[m_CurrentFont];

    return HasFont() ? &m_Fonts[0] : nullptr;
}


void UIRenderer::AddImage(Vec2 inPos, Vec2 inSize, uint32_t inTexture, Vec4 inTint, Vec4 inUVRect, float inRadius)
{
    m_Primitives.push_back(UIPrimitive
    {
        .mColor = inTint,
        .mRect = Vec4(inPos, inPos + inSize),
        .mUVRect = inUVRect,
        .mRadius = glm::min(inRadius, glm::min(inSize.x, inSize.y) * 0.5f),
        .mType = UI_PRIMITIVE_IMAGE,
        .mTexture = inTexture
    });
}


void UIRenderer::AddImageRotated(Vec2 inCenter, Vec2 inSize, float inRotation, uint32_t inTexture, Vec4 inTint, Vec4 inUVRect)
{
    m_Primitives.push_back(UIPrimitive
    {
        .mColor = inTint,
        .mRect = Vec4(inCenter - inSize * 0.5f, inCenter + inSize * 0.5f),
        .mUVRect = inUVRect,
        .mType = UI_PRIMITIVE_IMAGE,
        .mRotation = inRotation,
        .mTexture = inTexture
    });
}


void UIRenderer::AddImageSliced(Vec2 inPos, Vec2 inSize, uint32_t inTexture, float inBorder, float inBorderUV, Vec4 inTint, Vec4 inUVRect)
{
    m_Primitives.push_back(UIPrimitive
    {
        .mColor = inTint,
        .mRect = Vec4(inPos, inPos + inSize),
        .mUVRect = inUVRect,
        .mType = UI_PRIMITIVE_IMAGE_SLICED,
        .mTexture = inTexture,
        .mSliceBorder = inBorder,
        .mSliceUV = inBorderUV
    });
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


void UIRenderer::AddRectGradient(Vec2 inPos, Vec2 inSize, float inRadius, Vec4 inColorStart, Vec4 inColorEnd, bool inHorizontal, float inSoftness)
{
    m_Primitives.push_back(UIPrimitive
    {
        .mColor = inColorStart,
        .mRect = Vec4(inPos, inPos + inSize),
        .mUVRect = inColorEnd,
        .mRadius = glm::min(inRadius, glm::min(inSize.x, inSize.y) * 0.5f),
        .mSoftness = inSoftness,
        .mType = uint32_t(inHorizontal ? UI_PRIMITIVE_RECT_GRADIENT_H : UI_PRIMITIVE_RECT_GRADIENT_V)
    });
}


void UIRenderer::AddRectRotated(Vec2 inCenter, Vec2 inSize, float inRotation, float inRadius, Vec4 inColor, float inSoftness)
{
    m_Primitives.push_back(UIPrimitive
    {
        .mColor = inColor,
        .mRect = Vec4(inCenter - inSize * 0.5f, inCenter + inSize * 0.5f),
        .mRadius = glm::min(inRadius, glm::min(inSize.x, inSize.y) * 0.5f),
        .mSoftness = inSoftness,
        .mType = UI_PRIMITIVE_RECT,
        .mRotation = inRotation
    });
}


void UIRenderer::AddArc(Vec2 inCenter, float inRadius, float inThickness, float inStartAngle, float inFill, Vec4 inColor, float inSoftness)
{
    if (inFill <= 0.0f)
        return;

    const float extent = inRadius + inThickness * 0.5f;

    m_Primitives.push_back(UIPrimitive
    {
        .mColor = inColor,
        .mRect = Vec4(inCenter - extent, inCenter + extent),
        .mUVRect = Vec4(inStartAngle, glm::min(inFill, 1.0f), 0.0f, 0.0f),
        .mRadius = inRadius,
        .mThickness = inThickness,
        .mSoftness = inSoftness,
        .mType = UI_PRIMITIVE_ARC
    });
}


const UIGlyph* UIRenderer::GetGlyph(const UIFont& inFont, uint32_t inCodepoint) const
{
    const auto glyph = inFont.mGlyphs.find(inCodepoint);

    if (glyph != inFont.mGlyphs.end())
        return &glyph->second;

    const auto fallback = inFont.mGlyphs.find('?');
    return fallback != inFont.mGlyphs.end() ? &fallback->second : nullptr;
}


float UIRenderer::GetKerning(const UIFont& inFont, uint32_t inFirst, uint32_t inSecond) const
{
    if (inFirst < cFirstCodepoint || inFirst > cLastCodepoint || inSecond < cFirstCodepoint || inSecond > cLastCodepoint)
        return 0.0f;

    return inFont.mKerning[( inFirst - cFirstCodepoint ) * cCodepointCount + ( inSecond - cFirstCodepoint )];
}


Vec2 UIRenderer::MeasureText(StringView inText, float inSize) const
{
    const UIFont* font = GetCurrentFont();

    if (font == nullptr)
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

        if (const UIGlyph* glyph = GetGlyph(*font, codepoint))
            line_width += ( glyph->mAdvance + GetKerning(*font, previous, codepoint) ) * scale;

        previous = codepoint;
    }

    width = glm::max(width, line_width);

    const float line_height = ( font->mAscent - font->mDescent + font->mLineGap ) * scale;
    return Vec2(width, ( font->mAscent - font->mDescent ) * scale + line_height * ( line_count - 1 ));
}


float UIRenderer::AddText(Vec2 inPos, StringView inText, float inSize, Vec4 inColor, EUITextAlign inAlign)
{
    return EmitText(inPos, inText, inSize, inColor, inAlign, 0.0f, 0.0f);
}


float UIRenderer::AddTextOutlined(Vec2 inPos, StringView inText, float inSize, Vec4 inColor, Vec4 inOutlineColor, float inOutlineWidth, EUITextAlign inAlign)
{
    const float max_outline = float(cFontAtlasPadding) * inSize / cFontAtlasPixelHeight - 1.0f;

    EmitText(inPos, inText, inSize, inOutlineColor, inAlign, glm::min(inOutlineWidth, max_outline), 0.0f);
    return EmitText(inPos, inText, inSize, inColor, inAlign, 0.0f, 0.0f);
}


float UIRenderer::AddTextGlow(Vec2 inPos, StringView inText, float inSize, Vec4 inColor, Vec4 inGlowColor, float inGlowRadius, EUITextAlign inAlign)
{
    const float max_glow = float(cFontAtlasPadding) * inSize / cFontAtlasPixelHeight - 1.0f;
    const float glow = glm::min(inGlowRadius, max_glow);

    EmitText(inPos, inText, inSize, inGlowColor, inAlign, glow * 0.25f, glow);
    return EmitText(inPos, inText, inSize, inColor, inAlign, 0.0f, 0.0f);
}


float UIRenderer::EmitText(Vec2 inPos, StringView inText, float inSize, Vec4 inColor, EUITextAlign inAlign, float inDilate, float inSoftness)
{
    const UIFont* font = GetCurrentFont();

    if (font == nullptr || inText.empty())
        return 0.0f;

    const uint32_t font_index = uint32_t(font - m_Fonts.data());
    const float scale = inSize / cFontAtlasPixelHeight;
    const float line_height = ( font->mAscent - font->mDescent + font->mLineGap ) * scale;

    float widest_line = 0.0f;
    float baseline = inPos.y + font->mAscent * scale;

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
            const UIGlyph* glyph = GetGlyph(*font, codepoint);

            if (glyph == nullptr)
                continue;

            pen_x += GetKerning(*font, previous, codepoint) * scale;

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
                    .mThickness = inDilate,
                    .mSoftness = inSoftness,
                    .mType = UI_PRIMITIVE_GLYPH,
                    .mTexture = font_index
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
