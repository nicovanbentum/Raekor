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


struct UIFont
{
    Path mPath;
    Array<uint8_t> mAtlas;
    uint32_t mAtlasHeight = 0;
    float mAscent = 0.0f;
    float mDescent = 0.0f;
    float mLineGap = 0.0f;
    Array<float> mKerning;
    HashMap<uint32_t, UIGlyph> mGlyphs;
};


class UIRenderer
{
public:
    static constexpr float cFontAtlasPixelHeight = 48.0f;
    static constexpr int   cFontAtlasPadding = 10;
    static constexpr uint32_t cFontAtlasWidth = 1024;

    bool LoadFont(const Path& inPath);
    bool HasFont() const { return !m_Fonts.empty() && !m_Fonts[0].mAtlas.empty(); }

    int  AddFont(const Path& inPath);
    void SetFont(int inFont) { m_CurrentFont = inFont; }
    int  GetFont() const { return m_CurrentFont; }

    void Reset();

    void AddRectFilled(Vec2 inPos, Vec2 inSize, float inRadius, Vec4 inColor, float inSoftness = 0.0f);
    void AddRect(Vec2 inPos, Vec2 inSize, float inRadius, float inThickness, Vec4 inColor);
    void AddCircleFilled(Vec2 inCenter, float inRadius, Vec4 inColor, float inSoftness = 0.0f);
    void AddRectGradient(Vec2 inPos, Vec2 inSize, float inRadius, Vec4 inColorStart, Vec4 inColorEnd, bool inHorizontal = false, float inSoftness = 0.0f);
    void AddRectRotated(Vec2 inCenter, Vec2 inSize, float inRotation, float inRadius, Vec4 inColor, float inSoftness = 0.0f);
    void AddArc(Vec2 inCenter, float inRadius, float inThickness, float inStartAngle, float inFill, Vec4 inColor, float inSoftness = 0.0f);

    void AddImage(Vec2 inPos, Vec2 inSize, uint32_t inTexture, Vec4 inTint = Vec4(1.0f), Vec4 inUVRect = Vec4(0.0f, 0.0f, 1.0f, 1.0f), float inRadius = 0.0f);
    void AddImageRotated(Vec2 inCenter, Vec2 inSize, float inRotation, uint32_t inTexture, Vec4 inTint = Vec4(1.0f), Vec4 inUVRect = Vec4(0.0f, 0.0f, 1.0f, 1.0f));
    void AddImageSliced(Vec2 inPos, Vec2 inSize, uint32_t inTexture, float inBorder, float inBorderUV, Vec4 inTint = Vec4(1.0f), Vec4 inUVRect = Vec4(0.0f, 0.0f, 1.0f, 1.0f));

    float AddText(Vec2 inPos, StringView inText, float inSize, Vec4 inColor, EUITextAlign inAlign = UI_TEXT_ALIGN_LEFT);
    float AddTextOutlined(Vec2 inPos, StringView inText, float inSize, Vec4 inColor, Vec4 inOutlineColor, float inOutlineWidth, EUITextAlign inAlign = UI_TEXT_ALIGN_LEFT);
    float AddTextGlow(Vec2 inPos, StringView inText, float inSize, Vec4 inColor, Vec4 inGlowColor, float inGlowRadius, EUITextAlign inAlign = UI_TEXT_ALIGN_LEFT);
    Vec2  MeasureText(StringView inText, float inSize) const;

    Slice<const UIPrimitive> GetPrimitives() const { return m_Primitives; }

    uint32_t GetFontCount() const { return uint32_t(m_Fonts.size()); }
    uint32_t GetFontsVersion() const { return m_FontsVersion; }
    uint32_t GetFontAtlasWidth() const { return cFontAtlasWidth; }
    uint32_t GetFontAtlasHeight(uint32_t inFont) const { return m_Fonts[inFont].mAtlasHeight; }
    Slice<const uint8_t> GetFontAtlas(uint32_t inFont) const { return m_Fonts[inFont].mAtlas; }
    float GetFontDistanceRange() const { return float(cFontAtlasPadding); }

private:
    bool BuildFont(const Path& inPath, UIFont& outFont) const;
    const UIFont* GetCurrentFont() const;
    const UIGlyph* GetGlyph(const UIFont& inFont, uint32_t inCodepoint) const;
    float GetKerning(const UIFont& inFont, uint32_t inFirst, uint32_t inSecond) const;
    float EmitText(Vec2 inPos, StringView inText, float inSize, Vec4 inColor, EUITextAlign inAlign, float inDilate, float inSoftness);

    Array<UIPrimitive> m_Primitives;

    Array<UIFont> m_Fonts;
    uint32_t m_FontsVersion = 0;
    int m_CurrentFont = 0;
};

extern RK_API UIRenderer g_UIRenderer;

}
