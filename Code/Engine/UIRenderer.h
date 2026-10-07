#pragma once

#include "Renderer/Shared.h"

namespace RK {

enum EUITextAlign
{
    UI_TEXT_ALIGN_LEFT,
    UI_TEXT_ALIGN_CENTER,
    UI_TEXT_ALIGN_RIGHT
};


struct UIGlyph
{
    Vec2 mOffset = Vec2(0.0f);
    Vec2 mSize = Vec2(0.0f);
    Vec4 mUVRect = Vec4(0.0f);
    float mAdvance = 0.0f;
};


class UIRenderer
{
public:
    static constexpr float cFontAtlasPixelHeight = 48.0f;
    static constexpr int   cFontAtlasPadding = 6;
    static constexpr uint32_t cFontAtlasWidth = 1024;

    bool LoadFont(const Path& inPath);
    bool HasFont() const { return !m_FontAtlas.empty(); }

    void Reset();

    void AddRectFilled(Vec2 inPos, Vec2 inSize, float inRadius, Vec4 inColor, float inSoftness = 0.0f);
    void AddRect(Vec2 inPos, Vec2 inSize, float inRadius, float inThickness, Vec4 inColor);
    void AddCircleFilled(Vec2 inCenter, float inRadius, Vec4 inColor, float inSoftness = 0.0f);

    float AddText(Vec2 inPos, StringView inText, float inSize, Vec4 inColor, EUITextAlign inAlign = UI_TEXT_ALIGN_LEFT);
    Vec2  MeasureText(StringView inText, float inSize) const;

    Slice<const UIPrimitive> GetPrimitives() const { return m_Primitives; }

    uint32_t GetFontAtlasWidth() const { return cFontAtlasWidth; }
    uint32_t GetFontAtlasHeight() const { return m_FontAtlasHeight; }
    uint32_t GetFontAtlasVersion() const { return m_FontAtlasVersion; }
    Slice<const uint8_t> GetFontAtlas() const { return m_FontAtlas; }
    float GetFontDistanceRange() const { return float(cFontAtlasPadding); }

private:
    const UIGlyph* GetGlyph(uint32_t inCodepoint) const;
    float GetKerning(uint32_t inFirst, uint32_t inSecond) const;

    Array<UIPrimitive> m_Primitives;

    Array<uint8_t> m_FontAtlas;
    uint32_t m_FontAtlasHeight = 0;
    uint32_t m_FontAtlasVersion = 0;

    float m_Ascent = 0.0f;
    float m_Descent = 0.0f;
    float m_LineGap = 0.0f;
    Array<float> m_Kerning;
    HashMap<uint32_t, UIGlyph> m_Glyphs;
};

extern RK_API UIRenderer g_UIRenderer;

}
