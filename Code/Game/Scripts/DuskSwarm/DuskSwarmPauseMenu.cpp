#define RAEKOR_SCRIPT
#include "DuskSwarmScript.h"

namespace RK {

DuskSwarmScript::PauseLayout DuskSwarmScript::GetPauseLayout() const
{
    PauseLayout layout;
    layout.mDisplay = Vec2(m_App->GetViewport().GetDisplaySize());
    layout.mScale = glm::max(layout.mDisplay.y / 1080.0f, 0.5f);

    const float scale = layout.mScale;
    layout.mPanelSize = Vec2(620.0f, 580.0f) * scale;
    layout.mPanelPos = layout.mDisplay * 0.5f - layout.mPanelSize * 0.5f + Vec2(0.0f, 40.0f * scale);

    layout.mTabSize = Vec2(200.0f, 44.0f) * scale;
    const float tab_gap = 12.0f * scale;
    const float tabs_x = layout.mDisplay.x * 0.5f - ( layout.mTabSize.x * float(PAUSE_TAB_COUNT) + tab_gap * float(PAUSE_TAB_COUNT - 1) ) * 0.5f;

    for (int tab = 0; tab < PAUSE_TAB_COUNT; tab++)
        layout.mTabPos[tab] = Vec2(tabs_x + ( layout.mTabSize.x + tab_gap ) * float(tab), layout.mPanelPos.y + 84.0f * scale);

    layout.mSliderSize = Vec2(380.0f, 16.0f) * scale;
    layout.mSliderPos = Vec2(layout.mDisplay.x * 0.5f - layout.mSliderSize.x * 0.5f - 40.0f * scale, layout.mPanelPos.y + 200.0f * scale);
    layout.mSliderPadding = Vec2(16.0f, 18.0f) * scale;
    layout.mSliderStep = 88.0f * scale;
    layout.mCheckboxSize = Vec2(360.0f, 32.0f) * scale;
    layout.mGodModePos = Vec2(layout.mSliderPos.x, layout.mPanelPos.y + 338.0f * scale);
    layout.mSettingsPos = Vec2(layout.mSliderPos.x, layout.mPanelPos.y + 148.0f * scale);
    layout.mSettingStep = 44.0f * scale;
    layout.mButtonSize = Vec2(240.0f, 56.0f) * scale;
    layout.mButtonPos = Vec2(layout.mDisplay.x * 0.5f - layout.mButtonSize.x * 0.5f, layout.mPanelPos.y + 420.0f * scale);
    layout.mQuitPos = layout.mButtonPos + Vec2(0.0f, layout.mButtonSize.y + 16.0f * scale);
    return layout;
}


bool DuskSwarmScript::IsInside(const Vec2& inPoint, const Vec2& inPos, const Vec2& inSize)
{
    return inPoint.x >= inPos.x && inPoint.y >= inPos.y && inPoint.x <= inPos.x + inSize.x && inPoint.y <= inPos.y + inSize.y;
}


float DuskSwarmScript::GetMasterVolume()
{
    Audio* audio = GetAudio();
    return audio ? glm::clamp(audio->GetMasterVolume(), 0.0f, 1.0f) : 0.0f;
}


void DuskSwarmScript::SetMasterVolume(float inVolume)
{
    if (Audio* audio = GetAudio())
        audio->SetMasterVolume(glm::clamp(glm::round(inVolume * 100.0f) / 100.0f, 0.0f, 1.0f));
}


float DuskSwarmScript::GetSensitivityScale() const
{
    return m_SensitivityScale ? glm::clamp(*m_SensitivityScale, cMinSensitivityScale, cMaxSensitivityScale) : 1.0f;
}


void DuskSwarmScript::SetSensitivityScale(float inScale)
{
    if (m_SensitivityScale)
        *m_SensitivityScale = glm::clamp(glm::round(inScale * 10.0f) / 10.0f, cMinSensitivityScale, cMaxSensitivityScale);
}


float DuskSwarmScript::GetSliderValue(int inSlider)
{
    if (inSlider == SLIDER_VOLUME)
        return GetMasterVolume();

    return ( GetSensitivityScale() - cMinSensitivityScale ) / ( cMaxSensitivityScale - cMinSensitivityScale );
}


String DuskSwarmScript::GetSliderText(int inSlider)
{
    if (inSlider == SLIDER_VOLUME)
        return std::format("{}%", int(glm::round(GetMasterVolume() * 100.0f)));

    return std::format("{:.1f}x", GetSensitivityScale());
}


void DuskSwarmScript::SetSliderFromMouse(const PauseLayout& inLayout, int inSlider)
{
    const float value = glm::clamp(( m_MousePosition.x - inLayout.GetSliderPos(inSlider).x ) / inLayout.mSliderSize.x, 0.0f, 1.0f);

    if (inSlider == SLIDER_VOLUME)
        SetMasterVolume(value);
    else
        SetSensitivityScale(glm::mix(cMinSensitivityScale, cMaxSensitivityScale, value));
}


void DuskSwarmScript::ToggleGodMode()
{
    *m_GodMode = *m_GodMode ? 0 : 1;
    PlaySfx(cSelectSound, 0.5f);
}


int* DuskSwarmScript::GetGraphicsSetting(int inSetting)
{
    return g_CVariables->TryGetValue<int>(cGraphicsSettings[inSetting].mCVar);
}


void DuskSwarmScript::ToggleGraphicsSetting(int inSetting)
{
    if (int* value = GetGraphicsSetting(inSetting))
    {
        *value = *value ? 0 : 1;
        PlaySfx(cSelectSound, 0.5f);
    }
}


void DuskSwarmScript::SetPauseTab(int inTab)
{
    if (inTab == m_PauseTab)
        return;

    m_PauseTab = inTab;
    m_DraggingSlider = SLIDER_NONE;
    PlaySfx(cSelectSound, 0.5f);
}


void DuskSwarmScript::Resume()
{
    m_DraggingSlider = SLIDER_NONE;

    if (Game* game = dynamic_cast<Game*>(m_App))
        game->Unpause();
}


void DuskSwarmScript::Quit()
{
    m_DraggingSlider = SLIDER_NONE;

    if (Game* game = dynamic_cast<Game*>(m_App))
        game->Quit();
}


void DuskSwarmScript::DrawPauseMenu()
{
    const PauseLayout layout = GetPauseLayout();
    const float scale = layout.mScale;
    const float alpha = glm::clamp(m_PauseTime / 0.2f, 0.0f, 1.0f);
    const float center = layout.mDisplay.x * 0.5f;
    const Vec2 pos = layout.mPanelPos + Vec2(0.0f, ( 1.0f - EaseOutCubic(alpha) ) * 30.0f * scale);
    const Vec2 offset = pos - layout.mPanelPos;

    g_UIRenderer.SetFont(m_HudFont);
    g_UIRenderer.AddRectFilled(Vec2(0.0f), layout.mDisplay, 0.0f, Vec4(0.0f, 0.0f, 0.0f, 0.55f * alpha));

    DrawWindow(pos, layout.mPanelSize, "Paused", scale, alpha);

    for (int tab = 0; tab < PAUSE_TAB_COUNT; tab++)
    {
        const bool active = tab == m_PauseTab;
        const bool hovered = IsInside(m_MousePosition, layout.mTabPos[tab], layout.mTabSize);
        const float tint = active ? 1.35f : hovered ? 1.1f : 0.7f;
        const Vec2 tab_pos = layout.mTabPos[tab] + offset;

        g_UIRenderer.AddImageSliced(tab_pos, layout.mTabSize, m_ButtonTexture, 10.0f * scale, 12.0f / 64.0f, Vec4(tint, tint, tint, alpha));
        DrawLabel(Vec2(tab_pos.x + layout.mTabSize.x * 0.5f, tab_pos.y + layout.mTabSize.y * 0.5f - 12.0f * scale), cPauseTabNames[tab], 24.0f * scale, Fade(active || hovered ? cRSYellow : cRSOrange, ( active ? 1.0f : 0.75f ) * alpha), UI_TEXT_ALIGN_CENTER);
    }

    if (m_PauseTab == PAUSE_TAB_GENERAL)
        DrawPauseGeneralTab(layout, offset, alpha);
    else
        DrawPauseGraphicsTab(layout, offset, alpha);

    DrawButton(layout.mButtonPos + offset, layout.mButtonSize, "Resume", IsInside(m_MousePosition, layout.mButtonPos, layout.mButtonSize), alpha, scale);
    DrawButton(layout.mQuitPos + offset, layout.mButtonSize, "Quit", IsInside(m_MousePosition, layout.mQuitPos, layout.mButtonSize), alpha, scale);

    const char* hint = m_PauseTab == PAUSE_TAB_GENERAL ? "A / D or drag to adjust  -  G for god mode  -  Tab to switch  -  Esc to resume  -  Q to quit" : "Click to toggle  -  Tab to switch  -  Esc to resume  -  Q to quit";
    DrawLabel(Vec2(center, pos.y + layout.mPanelSize.y + 20.0f * scale), hint, 20.0f * scale, Fade(cRSWhite, 0.85f * alpha), UI_TEXT_ALIGN_CENTER);
}


void DuskSwarmScript::DrawCheckbox(const Vec2& inLayoutPos, const Vec2& inSize, const Vec2& inOffset, StringView inLabel, bool inChecked, float inAlpha, float inScale)
{
    const bool hovered = IsInside(m_MousePosition, inLayoutPos, inSize);
    const Vec2 pos = inLayoutPos + inOffset;
    const Vec2 box_size = Vec2(inSize.y);

    DrawSlotBox(pos, box_size, inScale, inAlpha);

    if (inChecked)
        g_UIRenderer.AddRectFilled(pos + Vec2(7.0f * inScale), box_size - Vec2(14.0f * inScale), 2.0f * inScale, Fade(cRSOrange, 0.9f * inAlpha));

    DrawLabel(pos + Vec2(box_size.x + 14.0f * inScale, 4.0f * inScale), inLabel, 22.0f * inScale, Fade(hovered ? cRSYellow : cRSWhite, inAlpha));
}


void DuskSwarmScript::DrawPauseGraphicsTab(const PauseLayout& inLayout, const Vec2& inOffset, float inAlpha)
{
    for (int setting = 0; setting < int(cGraphicsSettings.size()); setting++)
    {
        const int* value = GetGraphicsSetting(setting);
        DrawCheckbox(inLayout.GetSettingPos(setting), inLayout.mCheckboxSize, inOffset, cGraphicsSettings[setting].mLabel, value && *value != 0, inAlpha, inLayout.mScale);
    }
}


void DuskSwarmScript::DrawPauseGeneralTab(const PauseLayout& inLayout, const Vec2& inOffset, float inAlpha)
{
    const float scale = inLayout.mScale;

    for (int slider = 0; slider < SLIDER_COUNT; slider++)
    {
        const float value = GetSliderValue(slider);
        const Vec2 slider_pos = inLayout.GetSliderPos(slider) + inOffset;
        const Vec2 slider_size = inLayout.mSliderSize;
        const bool slider_hovered = m_DraggingSlider == slider || ( m_DraggingSlider == SLIDER_NONE && IsInside(m_MousePosition, inLayout.GetSliderPos(slider) - inLayout.mSliderPadding, slider_size + inLayout.mSliderPadding * 2.0f) );

        DrawLabel(Vec2(slider_pos.x, slider_pos.y - 44.0f * scale), cSliderNames[slider], 22.0f * scale, Fade(cRSWhite, inAlpha));
        DrawSlotBox(slider_pos - Vec2(4.0f * scale), slider_size + Vec2(8.0f * scale), scale, inAlpha);
        g_UIRenderer.AddRectFilled(slider_pos, Vec2(slider_size.x * value, slider_size.y), 2.0f * scale, Fade(cRSOrange, 0.85f * inAlpha));

        const Vec2 knob_size = Vec2(22.0f, slider_size.y / scale + 22.0f) * scale;
        const float knob_tint = slider_hovered ? 1.35f : 1.0f;
        g_UIRenderer.AddImageSliced(Vec2(slider_pos.x + slider_size.x * value - knob_size.x * 0.5f, slider_pos.y - 11.0f * scale), knob_size, m_ButtonTexture, 8.0f * scale, 12.0f / 64.0f, Vec4(knob_tint, knob_tint, knob_tint, inAlpha));

        DrawLabel(Vec2(slider_pos.x + slider_size.x + 70.0f * scale, slider_pos.y - 6.0f * scale), GetSliderText(slider), 26.0f * scale, Fade(cRSYellow, inAlpha), UI_TEXT_ALIGN_CENTER);
    }

    DrawCheckbox(inLayout.mGodModePos, inLayout.mCheckboxSize, inOffset, "God mode", *m_GodMode != 0, inAlpha, scale);
}

} // RK
