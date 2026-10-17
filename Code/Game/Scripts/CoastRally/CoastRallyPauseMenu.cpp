#define RAEKOR_SCRIPT
#include "CoastRallyScript.h"

namespace RK {

CoastRallyScript::PauseLayout CoastRallyScript::GetPauseLayout() const
{
    PauseLayout layout;
    layout.mDisplay = Vec2(m_App->GetViewport().GetDisplaySize());
    layout.mScale = glm::max(layout.mDisplay.y / 1080.0f, 0.5f);

    const float scale = layout.mScale;
    const float center = layout.mDisplay.x * 0.5f;

    layout.mPanelSize = Vec2(640.0f, 560.0f) * scale;
    layout.mPanelPos = layout.mDisplay * 0.5f - layout.mPanelSize * 0.5f + Vec2(0.0f, 20.0f * scale);

    layout.mTabSize = Vec2(180.0f, 44.0f) * scale;
    const float tab_gap = 12.0f * scale;
    const float tabs_x = center - ( layout.mTabSize.x * float(RALLY_TAB_COUNT) + tab_gap * float(RALLY_TAB_COUNT - 1) ) * 0.5f;

    for (int tab = 0; tab < RALLY_TAB_COUNT; tab++)
        layout.mTabPos[tab] = Vec2(tabs_x + ( layout.mTabSize.x + tab_gap ) * float(tab), layout.mPanelPos.y + 100.0f * scale);

    layout.mSliderSize = Vec2(380.0f, 10.0f) * scale;
    layout.mSliderPos = Vec2(center - layout.mSliderSize.x * 0.5f - 50.0f * scale, layout.mPanelPos.y + 236.0f * scale);
    layout.mSliderPadding = Vec2(16.0f, 20.0f) * scale;
    layout.mSliderStep = 92.0f * scale;
    layout.mToggleSize = Vec2(420.0f, 32.0f) * scale;
    layout.mGodModePos = Vec2(layout.mSliderPos.x, layout.mPanelPos.y + 372.0f * scale);
    layout.mAutoDrivePos = layout.mGodModePos + Vec2(0.0f, 44.0f * scale);
    layout.mSettingsPos = Vec2(layout.mSliderPos.x, layout.mPanelPos.y + 176.0f * scale);
    layout.mSettingStep = 50.0f * scale;
    layout.mButtonSize = Vec2(220.0f, 56.0f) * scale;
    layout.mResumePos = Vec2(center - layout.mButtonSize.x - 10.0f * scale, layout.mPanelPos.y + 470.0f * scale);
    layout.mQuitPos = Vec2(center + 10.0f * scale, layout.mResumePos.y);
    return layout;
}


bool CoastRallyScript::IsInside(const Vec2& inPoint, const Vec2& inPos, const Vec2& inSize)
{
    return inPoint.x >= inPos.x && inPoint.y >= inPos.y && inPoint.x <= inPos.x + inSize.x && inPoint.y <= inPos.y + inSize.y;
}


float CoastRallyScript::GetMasterVolume()
{
    Audio* audio = GetAudio();
    return audio ? glm::clamp(audio->GetMasterVolume(), 0.0f, 1.0f) : 0.0f;
}


void CoastRallyScript::SetMasterVolume(float inVolume)
{
    if (Audio* audio = GetAudio())
        audio->SetMasterVolume(glm::clamp(glm::round(inVolume * 100.0f) / 100.0f, 0.0f, 1.0f));
}


float CoastRallyScript::GetSensitivityScale() const
{
    return m_SensitivityScale ? glm::clamp(*m_SensitivityScale, cMinSensitivityScale, cMaxSensitivityScale) : 1.0f;
}


void CoastRallyScript::SetSensitivityScale(float inScale)
{
    if (m_SensitivityScale)
        *m_SensitivityScale = glm::clamp(glm::round(inScale * 10.0f) / 10.0f, cMinSensitivityScale, cMaxSensitivityScale);
}


float CoastRallyScript::GetSliderValue(int inSlider)
{
    if (inSlider == RALLY_SLIDER_VOLUME)
        return GetMasterVolume();

    return ( GetSensitivityScale() - cMinSensitivityScale ) / ( cMaxSensitivityScale - cMinSensitivityScale );
}


String CoastRallyScript::GetSliderText(int inSlider)
{
    if (inSlider == RALLY_SLIDER_VOLUME)
        return std::format("{}%", int(glm::round(GetMasterVolume() * 100.0f)));

    return std::format("{:.1f}X", GetSensitivityScale());
}


void CoastRallyScript::SetSliderFromMouse(const PauseLayout& inLayout, int inSlider)
{
    const float value = glm::clamp(( m_MousePosition.x - inLayout.GetSliderPos(inSlider).x ) / inLayout.mSliderSize.x, 0.0f, 1.0f);

    if (inSlider == RALLY_SLIDER_VOLUME)
        SetMasterVolume(value);
    else
        SetSensitivityScale(glm::mix(cMinSensitivityScale, cMaxSensitivityScale, value));
}


void CoastRallyScript::ToggleAutoDrive()
{
    *m_AutoDrive = *m_AutoDrive ? 0 : 1;
    PlaySfx(cSelectSound, 0.5f);
}


int* CoastRallyScript::GetGraphicsSetting(int inSetting)
{
    return g_CVariables->TryGetValue<int>(cRallyGraphicsSettings[inSetting].mCVar);
}


void CoastRallyScript::ToggleGraphicsSetting(int inSetting)
{
    if (int* value = GetGraphicsSetting(inSetting))
    {
        *value = *value ? 0 : 1;
        PlaySfx(cSelectSound, 0.5f);
    }
}


void CoastRallyScript::SetPauseTab(int inTab)
{
    if (inTab == m_PauseTab)
        return;

    m_PauseTab = inTab;
    m_DraggingSlider = RALLY_SLIDER_NONE;
    PlaySfx(cSelectSound, 0.5f);
}


void CoastRallyScript::Resume()
{
    m_DraggingSlider = RALLY_SLIDER_NONE;

    if (Game* game = dynamic_cast<Game*>(m_App))
        game->Unpause();
}


void CoastRallyScript::Quit()
{
    m_DraggingSlider = RALLY_SLIDER_NONE;

    if (Game* game = dynamic_cast<Game*>(m_App))
        game->Quit();
}


void CoastRallyScript::DrawPauseMenu()
{
    const PauseLayout layout = GetPauseLayout();
    const float scale = layout.mScale;
    const float alpha = glm::clamp(m_PauseTime / 0.2f, 0.0f, 1.0f);
    const float ease = 1.0f - ( 1.0f - alpha ) * ( 1.0f - alpha ) * ( 1.0f - alpha );
    const float center = layout.mDisplay.x * 0.5f;
    const Vec2 pos = layout.mPanelPos + Vec2(0.0f, ( 1.0f - ease ) * 30.0f * scale);
    const Vec2 offset = pos - layout.mPanelPos;
    const Vec2 size = layout.mPanelSize;

    g_UIRenderer.AddRectFilled(Vec2(0.0f), layout.mDisplay, 0.0f, Vec4(0.0f, 0.02f, 0.05f, 0.5f * alpha));

    g_UIRenderer.AddRectFilled(pos + Vec2(0.0f, 10.0f * scale), size, 20.0f * scale, Fade(cInk, 0.6f * alpha), 18.0f * scale);
    g_UIRenderer.AddRectFilled(pos, size, 20.0f * scale, Vec4(0.01f, 0.03f, 0.07f, 0.88f * alpha), 2.0f * scale);
    g_UIRenderer.AddRect(pos, size, 20.0f * scale, 2.0f * scale, Fade(cSea, 0.25f * alpha));
    g_UIRenderer.AddRectFilled(pos + Vec2(0.0f, 76.0f * scale), Vec2(size.x, 3.0f * scale), 0.0f, Fade(cSun, 0.8f * alpha));

    g_UIRenderer.SetFont(m_HudFont);
    DrawText(Vec2(center, pos.y + 10.0f * scale), "PIT STOP", 62.0f * scale, Fade(cSun, alpha), UI_TEXT_ALIGN_CENTER);

    for (int tab = 0; tab < RALLY_TAB_COUNT; tab++)
    {
        const bool active = tab == m_PauseTab;
        const bool hovered = IsInside(m_MousePosition, layout.mTabPos[tab], layout.mTabSize);
        const Vec2 tab_pos = layout.mTabPos[tab] + offset;
        const Vec4 fill = active ? cSun : Fade(cCream, hovered ? 0.18f : 0.08f);

        g_UIRenderer.AddRectFilled(tab_pos, layout.mTabSize, layout.mTabSize.y * 0.5f, Fade(fill, alpha));
        g_UIRenderer.AddText(Vec2(tab_pos.x + layout.mTabSize.x * 0.5f, tab_pos.y + 7.0f * scale), cRallyTabNames[tab], 28.0f * scale, Fade(active ? cInk : hovered ? cSun : cCream, alpha), UI_TEXT_ALIGN_CENTER);
    }

    if (m_PauseTab == RALLY_TAB_GENERAL)
        DrawPauseGeneralTab(layout, offset, alpha);
    else
        DrawPauseGraphicsTab(layout, offset, alpha);

    DrawMenuButton(layout.mResumePos, layout.mButtonSize, offset, "RESUME", true, alpha, scale);
    DrawMenuButton(layout.mQuitPos, layout.mButtonSize, offset, "QUIT", false, alpha, scale);

    g_UIRenderer.SetFont(m_LabelFont);
    const char* hint = m_PauseTab == RALLY_TAB_GENERAL ? "A / D or drag to adjust  -  G for god mode  -  Tab to switch  -  Esc to resume  -  Q to quit" : "Click to toggle  -  Tab to switch  -  Esc to resume  -  Q to quit";
    g_UIRenderer.AddText(Vec2(center, pos.y + size.y + 22.0f * scale), hint, 19.0f * scale, Fade(cCream, 0.8f * alpha), UI_TEXT_ALIGN_CENTER);
}


void CoastRallyScript::DrawToggle(const Vec2& inLayoutPos, const Vec2& inSize, const Vec2& inOffset, StringView inLabel, bool inChecked, float inAlpha, float inScale)
{
    const bool hovered = IsInside(m_MousePosition, inLayoutPos, inSize);
    const Vec2 pos = inLayoutPos + inOffset;
    const Vec2 track_size = Vec2(inSize.y * 1.8f, inSize.y);
    const float knob_radius = inSize.y * 0.5f - 4.0f * inScale;
    const float knob_x = inChecked ? pos.x + track_size.x - inSize.y * 0.5f : pos.x + inSize.y * 0.5f;

    g_UIRenderer.AddRectFilled(pos, track_size, track_size.y * 0.5f, Fade(inChecked ? cSun : Fade(cCream, 0.12f), inAlpha));
    g_UIRenderer.AddCircleFilled(Vec2(knob_x, pos.y + inSize.y * 0.5f), knob_radius, Fade(inChecked ? cInk : cCream, inAlpha), 1.0f * inScale);

    g_UIRenderer.SetFont(m_LabelFont);
    g_UIRenderer.AddText(pos + Vec2(track_size.x + 16.0f * inScale, 3.0f * inScale), inLabel, 22.0f * inScale, Fade(hovered ? cSun : cCream, inAlpha));
}


void CoastRallyScript::DrawMenuButton(const Vec2& inLayoutPos, const Vec2& inSize, const Vec2& inOffset, StringView inLabel, bool inPrimary, float inAlpha, float inScale)
{
    const bool hovered = IsInside(m_MousePosition, inLayoutPos, inSize);
    const Vec2 pos = inLayoutPos + inOffset - Vec2(0.0f, hovered ? 2.0f * inScale : 0.0f);
    const float radius = 14.0f * inScale;

    g_UIRenderer.AddRectFilled(inLayoutPos + inOffset + Vec2(0.0f, 5.0f * inScale), inSize, radius, Fade(cInk, 0.7f * inAlpha));

    if (inPrimary)
        g_UIRenderer.AddRectFilled(pos, inSize, radius, Fade(hovered ? glm::mix(cSun, cCream, 0.3f) : cSun, inAlpha));
    else
        g_UIRenderer.AddRectFilled(pos, inSize, radius, Fade(cCream, ( hovered ? 0.22f : 0.1f ) * inAlpha));

    if (!inPrimary)
        g_UIRenderer.AddRect(pos, inSize, radius, 2.0f * inScale, Fade(hovered ? cCoral : cCream, 0.6f * inAlpha));

    g_UIRenderer.SetFont(m_HudFont);
    g_UIRenderer.AddText(Vec2(pos.x + inSize.x * 0.5f, pos.y + 9.0f * inScale), inLabel, 34.0f * inScale, Fade(inPrimary ? cInk : hovered ? cCoral : cCream, inAlpha), UI_TEXT_ALIGN_CENTER);
}


void CoastRallyScript::DrawPauseGraphicsTab(const PauseLayout& inLayout, const Vec2& inOffset, float inAlpha)
{
    for (int setting = 0; setting < int(cRallyGraphicsSettings.size()); setting++)
    {
        const int* value = GetGraphicsSetting(setting);
        DrawToggle(inLayout.GetSettingPos(setting), inLayout.mToggleSize, inOffset, cRallyGraphicsSettings[setting].mLabel, value && *value != 0, inAlpha, inLayout.mScale);
    }
}


void CoastRallyScript::DrawPauseGeneralTab(const PauseLayout& inLayout, const Vec2& inOffset, float inAlpha)
{
    const float scale = inLayout.mScale;

    for (int slider = 0; slider < RALLY_SLIDER_COUNT; slider++)
    {
        const float value = GetSliderValue(slider);
        const Vec2 slider_pos = inLayout.GetSliderPos(slider) + inOffset;
        const Vec2 slider_size = inLayout.mSliderSize;
        const bool hovered = m_DraggingSlider == slider || ( m_DraggingSlider == RALLY_SLIDER_NONE && IsInside(m_MousePosition, inLayout.GetSliderPos(slider) - inLayout.mSliderPadding, slider_size + inLayout.mSliderPadding * 2.0f) );
        const float radius = slider_size.y * 0.5f;

        g_UIRenderer.SetFont(m_LabelFont);
        g_UIRenderer.AddText(Vec2(slider_pos.x, slider_pos.y - 40.0f * scale), cRallySliderNames[slider], 21.0f * scale, Fade(cCream, 0.8f * inAlpha));

        g_UIRenderer.AddRectFilled(slider_pos, slider_size, radius, Fade(cCream, 0.12f * inAlpha));
        g_UIRenderer.AddRectGradient(slider_pos, Vec2(glm::max(slider_size.x * value, slider_size.y), slider_size.y), radius, Fade(cSea, inAlpha), Fade(cSun, inAlpha), true);

        const Vec2 knob = Vec2(slider_pos.x + slider_size.x * value, slider_pos.y + radius);
        const float knob_radius = ( hovered ? 15.0f : 13.0f ) * scale;
        g_UIRenderer.AddCircleFilled(knob, knob_radius + 3.0f * scale, Fade(cInk, inAlpha), 1.0f * scale);
        g_UIRenderer.AddCircleFilled(knob, knob_radius, Fade(hovered ? cSun : cCream, inAlpha), 1.0f * scale);

        g_UIRenderer.SetFont(m_HudFont);
        DrawText(Vec2(slider_pos.x + slider_size.x + 70.0f * scale, slider_pos.y - 18.0f * scale), GetSliderText(slider), 36.0f * scale, Fade(cSun, inAlpha), UI_TEXT_ALIGN_CENTER);
    }

    DrawToggle(inLayout.mGodModePos, inLayout.mToggleSize, inOffset, "God mode", *m_GodMode != 0, inAlpha, scale);
    DrawToggle(inLayout.mAutoDrivePos,inLayout.mToggleSize, inOffset, "Auto drive", *m_AutoDrive != 0, inAlpha, scale);
}

} // RK
