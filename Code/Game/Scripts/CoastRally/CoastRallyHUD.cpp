#define RAEKOR_SCRIPT
#include "CoastRallyScript.h"

namespace RK {

Vec4 CoastRallyScript::Fade(const Vec4& inColor, float inAlpha)
{
    return Vec4(Vec3(inColor), inColor.a * inAlpha);
}


void CoastRallyScript::AddPopup(const String& inText, const Vec4& inColor)
{
    m_Popups.push_back(Popup { .mText = inText, .mTime = 0.0f, .mColor = inColor });
}


void CoastRallyScript::DrawText(Vec2 inPos, StringView inText, float inSize, Vec4 inColor, EUITextAlign inAlign)
{
    const float outline = inSize * 0.08f;
    g_UIRenderer.AddTextOutlined(inPos + Vec2(0.0f, inSize * 0.06f), inText, inSize, Fade(cInk, 0.5f * inColor.a), Fade(cInk, 0.5f * inColor.a), outline, inAlign);
    g_UIRenderer.AddTextOutlined(inPos, inText, inSize, inColor, Fade(cInk, inColor.a), outline, inAlign);
}


float CoastRallyScript::DrawKey(Vec2 inPos, StringView inLabel, float inScale)
{
    const float size = 17.0f * inScale;
    const float width = glm::max(g_UIRenderer.MeasureText(inLabel, size).x + 18.0f * inScale, 30.0f * inScale);

    g_UIRenderer.AddRectFilled(inPos + Vec2(0.0f, 3.0f * inScale), Vec2(width, 28.0f * inScale), 6.0f * inScale, Fade(cInk, 0.6f));
    g_UIRenderer.AddRectFilled(inPos, Vec2(width, 28.0f * inScale), 6.0f * inScale, cCream);
    g_UIRenderer.AddText(inPos + Vec2(width * 0.5f, 4.0f * inScale), inLabel, size, cInk, UI_TEXT_ALIGN_CENTER);
    return width;
}


void CoastRallyScript::DrawHUD(float inDeltaTime)
{
    const Vec2 display = Vec2(m_App->GetViewport().GetDisplaySize());
    const float scale = glm::max(display.y / 1080.0f, 0.5f);
    const float margin = 32.0f * scale;

    DrawSpeedometer(display, scale, margin);
    DrawTripInfo(scale, margin);

    if (*m_GodMode)
    {
        const Vec2 pos = Vec2(margin, margin + 232.0f * scale);
        const Vec2 size = Vec2(150.0f, 34.0f) * scale;

        g_UIRenderer.AddRectFilled(pos, size, size.y * 0.5f, Fade(cSea, 0.9f));
        g_UIRenderer.SetFont(m_HudFont);
        g_UIRenderer.AddText(pos + Vec2(size.x * 0.5f, 3.0f * scale), "GOD MODE", 26.0f * scale, cInk, UI_TEXT_ALIGN_CENTER);
    }

    if (m_ShowControls)
        DrawControls(display, scale, margin);

    DrawTitle(display, scale);
    DrawPopups(display, scale, inDeltaTime);
}


void CoastRallyScript::DrawSpeedometer(const Vec2& inDisplay, float inScale, float inMargin)
{
    const JPH::WheeledVehicleController* controller = GetController();

    if (controller == nullptr)
        return;

    const float radius = 118.0f * inScale;
    const Vec2 center = inDisplay - Vec2(inMargin + radius + 20.0f * inScale, inMargin + radius);
    const float start = glm::pi<float>() * 0.75f;
    const float sweep = 0.75f;
    const float max_speed = 220.0f;

    const float speed = glm::abs(GetForwardSpeed()) * 3.6f;
    const float fraction = glm::clamp(speed / max_speed, 0.0f, 1.0f);
    const float rpm = glm::clamp(( controller->GetEngine().GetCurrentRPM() - cEngineIdleRPM ) / ( cEngineMaxRPM - cEngineIdleRPM ), 0.0f, 1.0f);

    Vec4 color = glm::mix(cSea, cSun, glm::smoothstep(0.25f, 0.55f, fraction));
    color = glm::mix(color, cCoral, glm::smoothstep(0.7f, 0.9f, fraction));

    g_UIRenderer.AddCircleFilled(center, radius + 26.0f * inScale, Vec4(0.01f, 0.03f, 0.07f, 0.6f), 10.0f * inScale);
    g_UIRenderer.AddArc(center, radius, 16.0f * inScale, start, sweep, Vec4(1.0f, 1.0f, 1.0f, 0.12f));
    g_UIRenderer.AddArc(center, radius, 16.0f * inScale, start, sweep * fraction, color);
    g_UIRenderer.AddArc(center, radius + 16.0f * inScale, 4.0f * inScale, start, sweep, Vec4(1.0f, 1.0f, 1.0f, 0.08f));
    g_UIRenderer.AddArc(center, radius + 16.0f * inScale, 4.0f * inScale, start, sweep * rpm, rpm > 0.85f ? cCoral : Fade(cCream, 0.7f));

    g_UIRenderer.SetFont(m_LabelFont);

    for (int tick = 0; tick <= 22; tick++)
    {
        const float angle = start + glm::two_pi<float>() * sweep * float(tick) / 22.0f;
        const Vec2 direction = Vec2(glm::cos(angle), glm::sin(angle));
        const bool major = tick % 2 == 0;
        const Vec2 size = Vec2(major ? 14.0f : 7.0f, major ? 3.0f : 2.0f) * inScale;

        g_UIRenderer.AddRectRotated(center + direction * ( radius - 22.0f * inScale ), size, angle, 1.0f * inScale, Fade(cCream, major ? 0.8f : 0.4f));

        if (tick % 4 == 0)
            g_UIRenderer.AddText(center + direction * ( radius - 44.0f * inScale ) - Vec2(0.0f, 8.0f * inScale), std::format("{}", tick * 10), 14.0f * inScale, Fade(cCream, 0.7f), UI_TEXT_ALIGN_CENTER);
    }

    g_UIRenderer.SetFont(m_HudFont);
    DrawText(center - Vec2(0.0f, 56.0f * inScale), std::format("{}", int(speed)), 92.0f * inScale, cCream, UI_TEXT_ALIGN_CENTER);

    g_UIRenderer.SetFont(m_LabelFont);
    g_UIRenderer.AddText(center + Vec2(0.0f, 36.0f * inScale), "KM/H", 17.0f * inScale, Fade(cCream, 0.7f), UI_TEXT_ALIGN_CENTER);

    const int gear = controller->GetTransmission().GetCurrentGear();
    const String gear_text = gear < 0 ? String("R") : gear == 0 ? String("N") : std::format("{}", gear);
    const Vec2 gear_size = Vec2(46.0f, 40.0f) * inScale;
    const Vec2 gear_pos = center + Vec2(-gear_size.x * 0.5f, 62.0f * inScale);

    g_UIRenderer.AddRectFilled(gear_pos, gear_size, 8.0f * inScale, Fade(cCream, 0.92f));
    g_UIRenderer.SetFont(m_HudFont);
    g_UIRenderer.AddText(gear_pos + Vec2(gear_size.x * 0.5f, 2.0f * inScale), gear_text, 34.0f * inScale, gear < 0 ? cCoral : cInk, UI_TEXT_ALIGN_CENTER);

    if (m_Boosting)
    {
        const float pulse = 0.75f + 0.25f * glm::sin(m_Time * 18.0f);
        g_UIRenderer.AddTextGlow(center - Vec2(0.0f, radius + 78.0f * inScale), "BOOST", 46.0f * inScale, Fade(cSun, pulse), Fade(cCoral, 0.8f * pulse), 12.0f * inScale, UI_TEXT_ALIGN_CENTER);
    }
}


void CoastRallyScript::DrawTripInfo(float inScale, float inMargin)
{
    const int minutes = int(m_TimeOfDay * 24.0f * 60.0f) % ( 24 * 60 );

    const StaticArray<std::pair<const char*, String>, 5> rows =
    {
        std::pair { "TIME",          std::format("{:02}:{:02}", minutes / 60, minutes % 60) },
        std::pair { "DISTANCE",     std::format("{:.2f} KM", m_Odometer / 1000.0f) },
        std::pair { "TOP SPEED",     std::format("{} KM/H", int(m_TopSpeed * 3.6f)) },
        std::pair { "BEST AIR",      std::format("{:.1f} S", m_BestAirTime) },
        std::pair { "PROPS SMASHED", std::format("{} / {}", m_PropsKnocked, m_Props.size()) },
    };

    const float line_height = 30.0f * inScale;
    const Vec2 pos = Vec2(inMargin);
    const Vec2 size = Vec2(330.0f * inScale, 70.0f * inScale + line_height * rows.size());

    g_UIRenderer.AddRectFilled(pos, size, 14.0f * inScale, Vec4(0.01f, 0.03f, 0.07f, 0.55f), 2.0f * inScale);
    g_UIRenderer.AddRectFilled(pos + Vec2(0.0f, 54.0f * inScale), Vec2(size.x, 3.0f * inScale), 0.0f, Fade(cSun, 0.8f));

    g_UIRenderer.SetFont(m_HudFont);
    DrawText(pos + Vec2(20.0f, 8.0f) * inScale, "COAST RALLY", 40.0f * inScale, cSun);

    g_UIRenderer.SetFont(m_LabelFont);

    for (size_t index = 0; index < rows.size(); index++)
    {
        const Vec2 row = pos + Vec2(20.0f * inScale, 68.0f * inScale + line_height * float(index));
        g_UIRenderer.AddText(row, rows[index].first, 17.0f * inScale, Fade(cCream, 0.65f));
        g_UIRenderer.AddText(row + Vec2(size.x - 40.0f * inScale, 0.0f), rows[index].second, 19.0f * inScale, cCream, UI_TEXT_ALIGN_RIGHT);
    }
}


void CoastRallyScript::DrawControls(const Vec2& inDisplay, float inScale, float inMargin)
{
    static constexpr StaticArray<std::pair<const char*, const char*>, 10> cControls =
    {
        std::pair { "W / S",     "Throttle, brake and reverse" },
        std::pair { "A / D",     "Steer, or spin in the air" },
        std::pair { "SPACE",     "Handbrake" },
        std::pair { "SHIFT",     "Boost" },
        std::pair { "R",         "Back on the road" },
        std::pair { "C",         "Switch camera" },
        std::pair { "MOUSE",     "Look around" },
        std::pair { "BACKSPACE", "Restart and reset props" },
        std::pair { "G",         "God mode, drive through things" },
        std::pair { "H",         "Hide controls" },
    };

    const float line_height = 36.0f * inScale;
    const Vec2 size = Vec2(430.0f * inScale, line_height * cControls.size() + 28.0f * inScale);
    const Vec2 pos = Vec2(inMargin, inDisplay.y - inMargin - size.y);

    g_UIRenderer.AddRectFilled(pos, size, 14.0f * inScale, Vec4(0.01f, 0.03f, 0.07f, 0.5f), 2.0f * inScale);
    g_UIRenderer.SetFont(m_LabelFont);

    for (size_t index = 0; index < cControls.size(); index++)
    {
        const Vec2 row = pos + Vec2(18.0f * inScale, 16.0f * inScale + line_height * float(index));
        float x = 0.0f;

        for (const auto key : std::views::split(StringView(cControls[index].first), StringView(" / ")))
        {
            if (x > 0.0f)
            {
                g_UIRenderer.AddText(row + Vec2(x + 3.0f * inScale, 3.0f * inScale), "/", 18.0f * inScale, Fade(cCream, 0.6f));
                x += 16.0f * inScale;
            }

            x += DrawKey(row + Vec2(x, 0.0f), StringView(key.begin(), key.end()), inScale) + 4.0f * inScale;
        }

        g_UIRenderer.AddText(row + Vec2(150.0f * inScale, 3.0f * inScale), cControls[index].second, 19.0f * inScale, cCream);
    }
}


void CoastRallyScript::DrawTitle(const Vec2& inDisplay, float inScale)
{
    const float alpha = glm::clamp(( 7.0f - m_DriveTime ) / 1.5f, 0.0f, 1.0f) * glm::clamp(m_DriveTime / 0.4f, 0.0f, 1.0f);

    if (alpha <= 0.0f || m_PauseTime > 0.0f)
        return;

    const float center = inDisplay.x * 0.5f;
    const float rise = ( 1.0f - glm::clamp(m_DriveTime / 0.6f, 0.0f, 1.0f) ) * 40.0f * inScale;
    const float title_y = inDisplay.y * 0.16f + rise;

    g_UIRenderer.SetFont(m_HudFont);
    g_UIRenderer.AddTextGlow(Vec2(center, title_y), "COAST RALLY", 150.0f * inScale, Fade(cSun, alpha), Fade(cCoral, 0.5f * alpha), 22.0f * inScale, UI_TEXT_ALIGN_CENTER);
    DrawText(Vec2(center, title_y), "COAST RALLY", 150.0f * inScale, Fade(cSun, alpha), UI_TEXT_ALIGN_CENTER);

    g_UIRenderer.SetFont(m_LabelFont);
    g_UIRenderer.AddText(Vec2(center, title_y + 170.0f * inScale), "Explore the island, hit the jumps, smash the props", 28.0f * inScale, Fade(cCream, alpha), UI_TEXT_ALIGN_CENTER);
}


void CoastRallyScript::DrawPopups(const Vec2& inDisplay, float inScale, float inDeltaTime)
{
    float popup_y = inDisplay.y * 0.3f;

    g_UIRenderer.SetFont(m_HudFont);

    for (Popup& popup : m_Popups)
    {
        popup.mTime += inDeltaTime;

        const float fade_in = glm::clamp(popup.mTime / 0.12f, 0.0f, 1.0f);
        const float fade_out = glm::clamp(( 1.8f - popup.mTime ) / 0.4f, 0.0f, 1.0f);
        const float pop = 1.0f + 0.3f * glm::max(0.0f, 1.0f - popup.mTime * 6.0f);

        DrawText(Vec2(inDisplay.x * 0.5f, popup_y), popup.mText, 58.0f * inScale * pop, Fade(popup.mColor, fade_in * fade_out), UI_TEXT_ALIGN_CENTER);
        popup_y += 72.0f * inScale;
    }

    std::erase_if(m_Popups, [](const Popup& inPopup) { return inPopup.mTime >= 1.8f; });
}

} // RK
