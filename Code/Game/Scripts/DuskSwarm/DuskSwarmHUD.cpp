#define RAEKOR_SCRIPT
#include "DuskSwarmScript.h"

namespace RK {

void DuskSwarmScript::AddDamageNumber(const Vec3& inPosition, int inValue, bool inCrit)
{
    if (m_DamageNumbers.size() >= cMaxDamageNumbers)
        m_DamageNumbers.erase(m_DamageNumbers.begin());

    m_DamageNumbers.push_back(DamageNumber { .mPosition = inPosition + Vec3(RandomFloat(-0.3f, 0.3f), 0.0f, RandomFloat(-0.3f, 0.3f)), .mTime = 0.0f, .mValue = inValue, .mCrit = inCrit });
}


void DuskSwarmScript::AddChat(const String& inText, const Vec4& inColor)
{
    if (m_ChatMessages.size() >= 32)
        m_ChatMessages.erase(m_ChatMessages.begin());

    m_ChatMessages.push_back(ChatMessage { .mText = inText, .mColor = inColor });
}


void DuskSwarmScript::AddPopup(const String& inText, const String& inSubText, const Vec4& inColor)
{
    if (m_Popups.size() >= 4)
        m_Popups.erase(m_Popups.begin());

    m_Popups.push_back(Popup { .mText = inText, .mSubText = inSubText, .mTime = 0.0f, .mDuration = 3.0f, .mColor = inColor });
}


bool DuskSwarmScript::ProjectToScreen(const Vec3& inPosition, Vec2& outScreen) const
{
    const Viewport& viewport = m_App->GetViewport();
    const Vec4 clip = viewport.GetProjection() * viewport.GetView() * Vec4(inPosition, 1.0f);

    if (clip.w <= 0.0f)
        return false;

    const Vec2 ndc = Vec2(clip) / clip.w;
    outScreen = ( Vec2(ndc.x, -ndc.y) * 0.5f + 0.5f ) * Vec2(viewport.GetDisplaySize());
    return true;
}


Vec4 DuskSwarmScript::Fade(const Vec4& inColor, float inAlpha)
{
    return Vec4(Vec3(inColor), inColor.a * inAlpha);
}


void DuskSwarmScript::DrawLabel(Vec2 inPos, StringView inText, float inSize, Vec4 inColor, EUITextAlign inAlign)
{
    g_UIRenderer.AddText(inPos + Vec2(glm::max(1.0f, inSize * 0.08f)), inText, inSize, Vec4(0.0f, 0.0f, 0.0f, inColor.a), inAlign);
    g_UIRenderer.AddText(inPos, inText, inSize, inColor, inAlign);
}


void DuskSwarmScript::DrawStonePanel(Vec2 inPos, Vec2 inSize, float inScale, float inAlpha)
{
    g_UIRenderer.AddRectFilled(inPos + Vec2(0.0f, 6.0f) * inScale, inSize, 8.0f * inScale, Vec4(0.0f, 0.0f, 0.0f, 0.4f * inAlpha), 12.0f * inScale);
    g_UIRenderer.AddImageSliced(inPos, inSize, m_PanelTexture, 18.0f * inScale, 28.0f / 256.0f, Vec4(1.0f, 1.0f, 1.0f, inAlpha));
}


void DuskSwarmScript::DrawDivider(Vec2 inPos, float inWidth, float inScale, float inAlpha)
{
    g_UIRenderer.AddRectFilled(inPos, Vec2(inWidth, glm::max(1.0f, inScale)), 0.0f, Vec4(0.0f, 0.0f, 0.0f, 0.7f * inAlpha));
    g_UIRenderer.AddRectFilled(inPos + Vec2(0.0f, glm::max(1.0f, inScale)), Vec2(inWidth, glm::max(1.0f, inScale)), 0.0f, Vec4(0.55f, 0.48f, 0.38f, 0.6f * inAlpha));
}


void DuskSwarmScript::DrawWindow(Vec2 inPos, Vec2 inSize, StringView inTitle, float inScale, float inAlpha)
{
    DrawStonePanel(inPos, inSize, inScale, inAlpha);
    DrawLabel(Vec2(inPos.x + inSize.x * 0.5f, inPos.y + 24.0f * inScale), inTitle, 30.0f * inScale, Fade(cRSOrange, inAlpha), UI_TEXT_ALIGN_CENTER);
    DrawDivider(Vec2(inPos.x + 26.0f * inScale, inPos.y + 64.0f * inScale), inSize.x - 52.0f * inScale, inScale, inAlpha);
}


void DuskSwarmScript::DrawFlatBar(Vec2 inPos, Vec2 inSize, float inFill, Vec4 inFillColor, Vec4 inBackColor, float inScale)
{
    g_UIRenderer.AddRectFilled(inPos, inSize, 0.0f, inBackColor);
    g_UIRenderer.AddRectFilled(inPos, Vec2(inSize.x * glm::clamp(inFill, 0.0f, 1.0f), inSize.y), 0.0f, inFillColor);
    g_UIRenderer.AddRect(inPos, inSize, 0.0f, glm::max(1.0f, inScale), Vec4(0.0f, 0.0f, 0.0f, inBackColor.a));
}


void DuskSwarmScript::DrawSlotBox(Vec2 inPos, Vec2 inSize, float inScale, float inAlpha)
{
    g_UIRenderer.AddImageSliced(inPos, inSize, m_SlotTexture, 8.0f * inScale, 10.0f / 64.0f, Vec4(1.0f, 1.0f, 1.0f, inAlpha));
}


void DuskSwarmScript::DrawButton(Vec2 inPos, Vec2 inSize, StringView inLabel, bool inHovered, float inAlpha, float inScale)
{
    const float tint = inHovered ? 1.35f : 1.0f;
    g_UIRenderer.AddImageSliced(inPos, inSize, m_ButtonTexture, 10.0f * inScale, 12.0f / 64.0f, Vec4(tint, tint, tint, inAlpha));
    DrawLabel(Vec2(inPos.x + inSize.x * 0.5f, inPos.y + inSize.y * 0.5f - 14.0f * inScale), inLabel, 28.0f * inScale, Fade(inHovered ? cRSYellow : cRSOrange, inAlpha), UI_TEXT_ALIGN_CENTER);
}


void DuskSwarmScript::DrawGem(Vec2 inCenter, float inSize, Vec4 inColor)
{
    g_UIRenderer.AddImage(inCenter - Vec2(inSize * 0.5f), Vec2(inSize), m_GemTexture, inColor);
}


void DuskSwarmScript::DrawUpgradeIcon(int inUpgrade, Vec2 inCenter, float inSize, float inAlpha)
{
    g_UIRenderer.AddImage(inCenter - Vec2(inSize * 1.25f), Vec2(inSize * 2.5f), m_UpgradeIconTextures[inUpgrade], Vec4(1.0f, 1.0f, 1.0f, inAlpha));
}


Vec4 DuskSwarmScript::GetOrbValueColor(float inFraction)
{
    const float fraction = glm::clamp(inFraction, 0.0f, 1.0f);
    return fraction > 0.5f ? glm::mix(cRSYellow, cRSGreen, ( fraction - 0.5f ) * 2.0f) : glm::mix(cRSRed, cRSYellow, fraction * 2.0f);
}


void DuskSwarmScript::DrawOrb(Vec2 inPos, float inScale, float inFill, const Vec3& inColor, uint32_t inIcon, StringView inValue, float inValueFraction)
{
    const float k = 0.8f * inScale;
    const Vec2 center = inPos + Vec2(88.0f, 40.0f) * k;
    const float radius = 27.0f * k;
    const float fill = glm::clamp(inFill, 0.0f, 1.0f);

    g_UIRenderer.AddCircleFilled(center, radius, Vec4(0.06f, 0.05f, 0.04f, 1.0f));

    if (fill > 0.0f)
        g_UIRenderer.AddImage(Vec2(center.x - radius, center.y + radius - 2.0f * radius * fill), Vec2(2.0f * radius, 2.0f * radius * fill), m_OrbFillTexture, Vec4(inColor, 1.0f), Vec4(0.0f, 1.0f - fill, 1.0f, 1.0f));

    g_UIRenderer.AddImage(inPos - Vec2(0.0f, 24.0f * k), Vec2(128.0f * k), m_OrbFrameTexture);
    g_UIRenderer.AddImage(center - Vec2(15.0f * k), Vec2(30.0f * k), inIcon);
    DrawLabel(inPos + Vec2(34.0f, 29.0f) * k, inValue, 22.0f * k, GetOrbValueColor(inValueFraction), UI_TEXT_ALIGN_CENTER);
}


void DuskSwarmScript::DrawHUD(float inDeltaTime)
{
    PROFILE_FUNCTION_CPU();

    const Vec2 display = Vec2(m_App->GetViewport().GetDisplaySize());
    const float scale = glm::max(display.y / 1080.0f, 0.5f);
    const float margin = 20.0f * scale;

    g_UIRenderer.SetFont(m_HudFont);

    const float health_fraction = m_Health / GetMaxHealth();

    if (m_HitFlash > 0.0f)
        g_UIRenderer.AddRect(Vec2(0.0f), display, 0.0f, 120.0f * scale, Vec4(0.8f, 0.05f, 0.05f, 0.4f * m_HitFlash));

    if (health_fraction < 0.3f && m_State == STATE_PLAYING)
    {
        const float pulse = 0.5f + 0.5f * glm::sin(m_Time * 6.0f);
        g_UIRenderer.AddRect(Vec2(0.0f), display, 0.0f, 90.0f * scale, Vec4(0.7f, 0.0f, 0.05f, ( 0.12f + 0.12f * pulse ) * ( 1.0f - health_fraction / 0.3f )));
    }

    if (m_State == STATE_PLAYING)
    {
        DrawHitsplats(inDeltaTime, scale);
        DrawHintArrows(display, scale);

        if (m_FirstPerson)
            DrawCrosshair(display, scale);
    }

    Vec2 player_screen;
    if (m_State == STATE_PLAYING && !m_FirstPerson && health_fraction < 1.0f && ProjectToScreen(ToWorld(m_PlayerPosition, 2.2f), player_screen))
    {
        const Vec2 size = Vec2(64.0f, 8.0f) * scale;
        DrawFlatBar(player_screen - Vec2(size.x * 0.5f, size.y), size, health_fraction, cHealthGreen, cHealthRed, scale);
    }

    DrawInfoBox(scale, margin);
    DrawExperience(display, inDeltaTime, scale, margin);
    DrawBossBar(display, scale, margin);
    DrawInventory(display, scale, margin);
    DrawChatbox(display, scale, margin);
    DrawPopups(display, inDeltaTime, scale);

    if (m_RunTime < 4.5f && m_State == STATE_PLAYING)
        DrawIntro(display, scale);

    if (m_ScreenFlash > 0.0f)
        g_UIRenderer.AddRectFilled(Vec2(0.0f), display, 0.0f, Vec4(1.0f, 0.97f, 0.9f, m_ScreenFlash * 0.7f));

    if (m_State == STATE_LEVEL_UP)
        DrawLevelUp(display, scale);
    else if (( m_State == STATE_GAME_OVER || m_State == STATE_VICTORY ) && m_PlayerDeathTimer > 0.6f)
        DrawEndScreen(display, scale);
}


void DuskSwarmScript::DrawCrosshair(const Vec2& inDisplay, float inScale)
{
    const Vec2 center = glm::floor(inDisplay * 0.5f);
    const float thickness = glm::max(2.0f, glm::round(2.0f * inScale));
    const float length = glm::round(7.0f * inScale);
    const float gap = glm::round(4.0f * inScale);
    const float outline = glm::max(1.0f, glm::round(inScale));

    const StaticArray<std::pair<Vec2, Vec2>, 4> arms =
    {
        std::pair { Vec2(-gap - length, -thickness * 0.5f), Vec2(length, thickness) },
        std::pair { Vec2(gap, -thickness * 0.5f),           Vec2(length, thickness) },
        std::pair { Vec2(-thickness * 0.5f, -gap - length), Vec2(thickness, length) },
        std::pair { Vec2(-thickness * 0.5f, gap),           Vec2(thickness, length) },
    };

    for (const auto& [offset, size] : arms)
        g_UIRenderer.AddRectFilled(center + offset - outline, size + outline * 2.0f, 0.0f, Vec4(0.0f, 0.0f, 0.0f, 0.6f));

    for (const auto& [offset, size] : arms)
        g_UIRenderer.AddRectFilled(center + offset, size, 0.0f, Fade(cRSWhite, 0.9f));
}


void DuskSwarmScript::DrawSkyDial(const Vec2& inPos, const Vec2& inSize, float inScale)
{
    static constexpr StaticArray<Vec2, 10> cStars =
    {
        Vec2(0.08f, 0.18f), Vec2(0.17f, 0.52f), Vec2(0.27f, 0.12f), Vec2(0.38f, 0.38f), Vec2(0.47f, 0.08f),
        Vec2(0.58f, 0.3f), Vec2(0.66f, 0.1f), Vec2(0.76f, 0.46f), Vec2(0.85f, 0.2f), Vec2(0.93f, 0.55f),
    };

    const float angle = GetSkyAngle();
    const float sun_height = glm::sin(angle);
    const float day = glm::smoothstep(-0.1f, 0.35f, sun_height);
    const float glow = 1.0f - glm::smoothstep(0.0f, 0.3f, glm::abs(sun_height));

    const Vec4 top = glm::mix(Vec4(0.03f, 0.04f, 0.1f, 1.0f), Vec4(0.24f, 0.42f, 0.7f, 1.0f), day);
    const Vec4 bottom = glm::mix(glm::mix(Vec4(0.08f, 0.1f, 0.2f, 1.0f), Vec4(0.62f, 0.72f, 0.85f, 1.0f), day), Vec4(0.95f, 0.5f, 0.25f, 1.0f), glow * 0.85f);

    DrawSlotBox(inPos - Vec2(4.0f * inScale), inSize + Vec2(8.0f * inScale), inScale);

    const float ground = 12.0f * inScale;
    const float horizon = inPos.y + inSize.y - ground;

    g_UIRenderer.AddRectGradient(inPos, Vec2(inSize.x, inSize.y - ground), 2.0f * inScale, top, bottom);

    for (size_t index = 0; index < cStars.size(); index++)
    {
        const float twinkle = 0.65f + 0.35f * glm::sin(m_Time * 2.3f + float(index) * 1.7f);
        const Vec2 star = inPos + cStars[index] * Vec2(inSize.x, inSize.y - ground);
        g_UIRenderer.AddCircleFilled(star, 1.2f * inScale, Vec4(1.0f, 1.0f, 0.9f, ( 1.0f - day ) * twinkle * 0.9f));
    }

    const Vec2 center = Vec2(inPos.x + inSize.x * 0.5f, horizon);
    const Vec2 radius = Vec2(inSize.x * 0.5f - 14.0f * inScale, inSize.y - ground - 10.0f * inScale);
    const float body_radius = 6.0f * inScale;

    for (int dot = 0; dot <= 24; dot++)
    {
        const float dot_angle = glm::pi<float>() * float(dot) / 24.0f;
        g_UIRenderer.AddCircleFilled(center + Vec2(-glm::cos(dot_angle), -glm::sin(dot_angle)) * radius, 0.9f * inScale, Vec4(1.0f, 1.0f, 1.0f, 0.22f));
    }

    for (const bool sun : { true, false })
    {
        const float body_angle = sun ? angle : angle + glm::pi<float>();
        const float body_height = glm::sin(body_angle);

        if (body_height * radius.y < -( ground - body_radius ))
            continue;

        const Vec2 body = center + Vec2(-glm::cos(body_angle), -body_height) * radius;
        const float halo = glm::smoothstep(-0.05f, 0.25f, body_height);

        if (sun)
        {
            const Vec4 color = glm::mix(Vec4(1.0f, 0.55f, 0.2f, 1.0f), Vec4(1.0f, 0.9f, 0.5f, 1.0f), glm::smoothstep(0.0f, 0.5f, sun_height));
            g_UIRenderer.AddCircleFilled(body, body_radius * 2.4f, Fade(color, 0.35f * halo), body_radius * 2.0f);
            g_UIRenderer.AddCircleFilled(body, body_radius, color);
        }
        else
        {
            const float t = glm::clamp(( body.y - inPos.y ) / ( horizon - inPos.y ), 0.0f, 1.0f);
            g_UIRenderer.AddCircleFilled(body, body_radius * 2.2f, Vec4(0.7f, 0.8f, 1.0f, 0.18f * halo), body_radius * 2.0f);
            g_UIRenderer.AddCircleFilled(body, body_radius, Vec4(0.88f, 0.92f, 1.0f, 1.0f));
            g_UIRenderer.AddCircleFilled(body + Vec2(body_radius * 0.55f, -body_radius * 0.3f), body_radius * 0.85f, glm::mix(top, bottom, t));
        }
    }

    g_UIRenderer.AddRectFilled(Vec2(inPos.x, horizon), Vec2(inSize.x, ground), 2.0f * inScale, Vec4(0.07f, 0.09f, 0.06f, 1.0f));
    g_UIRenderer.AddRectFilled(Vec2(inPos.x, horizon), Vec2(inSize.x, glm::max(1.0f, inScale)), 0.0f, Fade(bottom, 0.6f));
}


void DuskSwarmScript::DrawInfoBox(float inScale, float inMargin)
{
    Array<std::pair<String, String>> rows =
    {
        { "Time:", FormatTime(m_RunTime) },
        { "Final dawn:", FormatTime(glm::max(0.0f, cVictoryTime - m_RunTime)) },
        { "Kills:", std::format("{}", m_Kills) },
        { "Damage:", std::format("{:.1f}k", m_DamageDealt / 1000.0f) },
    };

    const bool streak = m_Streak >= 10 && m_State == STATE_PLAYING;

    if (streak)
        rows.push_back({ "Streak:", std::format("x{}", m_Streak) });

    const float row_height = 26.0f * inScale;
    const Vec2 pos = Vec2(inMargin);
    const Vec2 size = Vec2(260.0f * inScale, 146.0f * inScale + float(rows.size()) * row_height);

    DrawStonePanel(pos, size, inScale, 0.94f);
    DrawLabel(Vec2(pos.x + size.x * 0.5f, pos.y + 18.0f * inScale), "Dusk Swarm", 24.0f * inScale, cRSOrange, UI_TEXT_ALIGN_CENTER);
    DrawSkyDial(pos + Vec2(24.0f, 46.0f) * inScale, Vec2(size.x - 48.0f * inScale, 66.0f * inScale), inScale);

    for (size_t index = 0; index < rows.size(); index++)
    {
        const float y = pos.y + 126.0f * inScale + float(index) * row_height;
        const bool is_streak = streak && index == rows.size() - 1;
        const Vec4 value_color = is_streak ? Fade(cRSOrange, 0.4f + 0.6f * glm::clamp(m_StreakTimer / cStreakWindow, 0.0f, 1.0f)) : cRSYellow;

        DrawLabel(Vec2(pos.x + 24.0f * inScale, y), rows[index].first, 20.0f * inScale, cRSWhite);
        DrawLabel(Vec2(pos.x + size.x - 24.0f * inScale, y), rows[index].second, 20.0f * inScale, value_color, UI_TEXT_ALIGN_RIGHT);
    }
}


void DuskSwarmScript::DrawExperience(const Vec2& inDisplay, float inDeltaTime, float inScale, float inMargin)
{
    const Vec2 size = Vec2(240.0f, 104.0f) * inScale;
    const Vec2 pos = Vec2(inDisplay.x - inMargin - size.x, inMargin);
    const int needed = GetExperienceToNextLevel();

    DrawStonePanel(pos, size, inScale, 0.94f);
    DrawLabel(pos + Vec2(24.0f, 20.0f) * inScale, "Level:", 20.0f * inScale, cRSWhite);
    DrawLabel(Vec2(pos.x + size.x - 24.0f * inScale, pos.y + 20.0f * inScale), std::format("{}", m_Level), 20.0f * inScale, cRSYellow, UI_TEXT_ALIGN_RIGHT);
    DrawLabel(pos + Vec2(24.0f, 46.0f) * inScale, "XP:", 20.0f * inScale, cRSWhite);
    DrawLabel(Vec2(pos.x + size.x - 24.0f * inScale, pos.y + 46.0f * inScale), std::format("{} / {}", m_Experience, needed), 20.0f * inScale, cRSYellow, UI_TEXT_ALIGN_RIGHT);
    DrawFlatBar(pos + Vec2(24.0f, 76.0f) * inScale, Vec2(size.x - 48.0f * inScale, 10.0f * inScale), float(m_Experience) / float(needed), Vec4(0.2f, 0.65f, 1.0f, 1.0f), Vec4(0.05f, 0.06f, 0.12f, 1.0f), inScale);

    const float orb_width = 128.0f * 0.8f * inScale;
    const Vec2 orb_pos = Vec2(pos.x + size.x - orb_width, pos.y + size.y + 6.0f * inScale);
    const float health_fraction = m_Health / GetMaxHealth();
    const float dash_ready = glm::clamp(1.0f - m_DashCooldownTimer / cDashCooldown, 0.0f, 1.0f);

    DrawOrb(orb_pos, inScale, health_fraction, Vec3(0.78f, 0.06f, 0.05f), m_HeartTexture, std::format("{}", int(glm::ceil(m_Health))), health_fraction);
    DrawOrb(orb_pos + Vec2(0.0f, 68.0f * inScale), inScale, dash_ready, Vec3(0.95f, 0.78f, 0.2f), m_DashIconTexture, std::format("{}", int(dash_ready * 100.0f)), dash_ready);

    const float drop_x = orb_pos.x - 14.0f * inScale;

    for (XPDrop& drop : m_XPDrops)
    {
        drop.mTime += inDeltaTime;

        const float alpha = glm::clamp(( cXPDropDuration - drop.mTime ) / 0.4f, 0.0f, 1.0f);
        const float y = orb_pos.y + 110.0f * inScale - drop.mTime * 80.0f * inScale;
        const String text = std::format("+{}", drop.mValue);
        const float width = g_UIRenderer.MeasureText(text, 20.0f * inScale).x;

        DrawGem(Vec2(drop_x - width - 14.0f * inScale, y + 10.0f * inScale), 20.0f * inScale, Vec4(0.45f, 0.85f, 1.0f, alpha));
        DrawLabel(Vec2(drop_x, y), text, 20.0f * inScale, Fade(cRSWhite, alpha), UI_TEXT_ALIGN_RIGHT);
    }

    std::erase_if(m_XPDrops, [](const XPDrop& inDrop) { return inDrop.mTime >= cXPDropDuration; });
}


void DuskSwarmScript::DrawBossBar(const Vec2& inDisplay, float inScale, float inMargin)
{
    if (m_BossEnemy < 0)
        return;

    const Enemy& boss = m_Enemies[m_BossEnemy];

    if (!boss.mActive)
        return;

    const float health = glm::max(boss.mHealth, 0.0f);
    const bool enraged = boss.mHealth < boss.mMaxHealth * 0.5f && !boss.mDying;
    const float appear = glm::clamp(( m_RunTime - cBossTimes[glm::max(m_NextBoss - 1, 0)] ) / 0.8f, 0.0f, 1.0f);
    const Vec2 size = Vec2(480.0f, 90.0f) * inScale;
    const Vec2 pos = Vec2(inDisplay.x * 0.5f - size.x * 0.5f, inMargin);
    const Vec2 bar_pos = pos + Vec2(26.0f, 50.0f) * inScale;
    const Vec2 bar_size = Vec2(size.x - 52.0f * inScale, 20.0f * inScale);

    DrawStonePanel(pos, size, inScale, 0.94f * appear);
    DrawLabel(Vec2(inDisplay.x * 0.5f, pos.y + 18.0f * inScale), enraged ? "The Lich (enraged)" : "The Lich", 24.0f * inScale, Fade(enraged ? cRSRed : cRSYellow, appear), UI_TEXT_ALIGN_CENTER);
    DrawFlatBar(bar_pos, bar_size, health / boss.mMaxHealth, Fade(cHealthGreen, appear), Fade(cHealthRed, appear), inScale);
    DrawLabel(Vec2(inDisplay.x * 0.5f, bar_pos.y + 1.0f * inScale), std::format("{} / {}", int(glm::ceil(health)), int(boss.mMaxHealth)), 17.0f * inScale, Fade(cRSWhite, appear), UI_TEXT_ALIGN_CENTER);
}


float DuskSwarmScript::GetWeaponCooldownFraction(int inUpgrade) const
{
    const int level = m_UpgradeLevels[inUpgrade];
    const float haste = GetCooldownMultiplier();

    switch (inUpgrade)
    {
        case UPGRADE_BOLT:      return 1.0f - m_BoltTimer / ( ( 0.95f - 0.1f * ( level - 1 ) ) * haste );
        case UPGRADE_NOVA:      return 1.0f - m_NovaTimer / ( ( 3.6f - 0.35f * ( level - 1 ) ) * haste );
        case UPGRADE_FIREBALL:  return 1.0f - m_FireballTimer / ( ( 2.6f - 0.25f * ( level - 1 ) ) * haste );
        case UPGRADE_LIGHTNING: return 1.0f - m_LightningTimer / ( ( 2.2f - 0.2f * ( level - 1 ) ) * haste );
        case UPGRADE_METEOR:    return 1.0f - m_MeteorTimer / ( ( 5.5f - 0.5f * ( level - 1 ) ) * haste );
        default:                return 1.0f;
    }
}


void DuskSwarmScript::DrawInventory(const Vec2& inDisplay, float inScale, float inMargin)
{
    Array<int> items;

    for (int pass = 0; pass < 2; pass++)
    {
        for (int upgrade = 0; upgrade < UPGRADE_COUNT; upgrade++)
        {
            if (m_UpgradeLevels[upgrade] > 0 && cUpgrades[upgrade].mIsWeapon == ( pass == 0 ))
                items.push_back(upgrade);
        }
    }

    if (items.empty())
        return;

    constexpr int cColumns = 4;
    const float slot = 58.0f * inScale;
    const float gap = 6.0f * inScale;
    const float padding = 20.0f * inScale;
    const int rows = ( int(items.size()) + cColumns - 1 ) / cColumns;
    const Vec2 size = Vec2(padding * 2.0f + cColumns * slot + ( cColumns - 1 ) * gap, padding * 2.0f + rows * slot + ( rows - 1 ) * gap);
    const Vec2 pos = inDisplay - Vec2(inMargin) - size;

    DrawStonePanel(pos, size, inScale, 0.94f);

    for (size_t index = 0; index < items.size(); index++)
    {
        const int upgrade = items[index];
        const int level = m_UpgradeLevels[upgrade];
        const Vec2 slot_pos = pos + Vec2(padding) + Vec2(float(index % cColumns), float(index / cColumns)) * ( slot + gap );

        DrawSlotBox(slot_pos, Vec2(slot), inScale);
        DrawUpgradeIcon(upgrade, slot_pos + Vec2(slot * 0.5f), slot * 0.3f, 1.0f);

        const float ready = glm::clamp(GetWeaponCooldownFraction(upgrade), 0.0f, 1.0f);

        if (ready < 1.0f)
            g_UIRenderer.AddRectFilled(slot_pos + Vec2(3.0f * inScale), Vec2(slot - 6.0f * inScale, ( slot - 6.0f * inScale ) * ( 1.0f - ready )), 2.0f * inScale, Vec4(0.0f, 0.0f, 0.0f, 0.55f));

        DrawLabel(slot_pos + Vec2(6.0f, 3.0f) * inScale, std::format("{}", level), 19.0f * inScale, level >= cMaxUpgradeLevel ? cRSGreen : cRSYellow);
    }
}


void DuskSwarmScript::DrawChatbox(const Vec2& inDisplay, float inScale, float inMargin)
{
    const Vec2 size = Vec2(540.0f, 184.0f) * inScale;
    const Vec2 pos = Vec2(inMargin, inDisplay.y - inMargin - size.y);
    const float text_size = 18.0f * inScale;
    const float line = 22.0f * inScale;
    const float input_y = pos.y + size.y - 34.0f * inScale;
    const float left = pos.x + 16.0f * inScale;

    g_UIRenderer.AddImageSliced(pos, size, m_ParchmentTexture, 14.0f * inScale, 16.0f / 256.0f, Vec4(1.0f, 1.0f, 1.0f, 0.9f));
    g_UIRenderer.AddRectFilled(Vec2(left, input_y - 6.0f * inScale), Vec2(size.x - 32.0f * inScale, glm::max(1.0f, inScale)), 0.0f, Vec4(0.23f, 0.17f, 0.1f, 0.6f));

    const float name_width = g_UIRenderer.AddText(Vec2(left, input_y), "Survivor: ", text_size, cChatBlack);

    if (glm::fract(m_Time * 0.8f) < 0.5f)
        g_UIRenderer.AddText(Vec2(left + name_width, input_y), "*", text_size, cChatBlue);

    int shown = 0;

    for (auto message = m_ChatMessages.rbegin(); message != m_ChatMessages.rend() && shown < cChatLines; ++message, ++shown)
        g_UIRenderer.AddText(Vec2(left, input_y - 10.0f * inScale - line * float(shown + 1)), message->mText, text_size, message->mColor);
}


void DuskSwarmScript::DrawHintArrows(const Vec2& inDisplay, float inScale)
{
    const auto DrawHintArrow = [&](const Vec3& inWorld)
    {
        Vec2 screen;
        const bool visible = ProjectToScreen(inWorld, screen);
        const float edge = 50.0f * inScale;

        if (visible && screen.x > edge && screen.y > edge && screen.x < inDisplay.x - edge && screen.y < inDisplay.y - edge)
            return;

        Vec2 player_screen;
        if (!m_FirstPerson && !ProjectToScreen(GetPlayerCenter(), player_screen))
            return;

        Vec2 direction = Vec2(inWorld.x - m_PlayerPosition.x, inWorld.z - m_PlayerPosition.y);

        if (m_FirstPerson)
            direction = Vec2(glm::dot(direction, GetLookRight()), -glm::dot(direction, GetLookForward()));

        direction =glm::length(direction) > 0.001f ? glm::normalize(direction) : Vec2(1.0f, 0.0f);

        const Vec2 center = inDisplay * 0.5f;
        const Vec2 extent = inDisplay * 0.5f - Vec2(edge);
        const float t = glm::min(extent.x / glm::max(glm::abs(direction.x), 0.001f), extent.y / glm::max(glm::abs(direction.y), 0.001f));
        const Vec2 position = center + direction * ( t - 6.0f * inScale * ( 1.0f + glm::sin(m_Time * 7.0f) ) );

        g_UIRenderer.AddImageRotated(position, Vec2(48.0f * inScale), glm::atan(direction.y, direction.x), m_HintArrowTexture);
    };

    for (const Pickup& pickup : m_Pickups)
    {
        if (pickup.mActive && pickup.mType == PICKUP_CHEST)
            DrawHintArrow(ToWorld(pickup.mPosition, 0.5f));
    }

    if (m_BossEnemy >= 0 && m_Enemies[m_BossEnemy].mActive)
        DrawHintArrow(ToWorld(m_Enemies[m_BossEnemy].mPosition, 1.5f));
}


String DuskSwarmScript::FormatTime(float inSeconds)
{
    const int seconds = int(inSeconds);
    return std::format("{:02}:{:02}", seconds / 60, seconds % 60);
}


void DuskSwarmScript::DrawHitsplats(float inDeltaTime, float inScale)
{
    for (DamageNumber& number : m_DamageNumbers)
    {
        number.mTime += inDeltaTime;

        Vec2 screen;
        if (!ProjectToScreen(number.mPosition + Vec3(0.0f, 0.6f + number.mTime * 0.4f, 0.0f), screen))
            continue;

        const float alpha = glm::clamp(( 0.75f - number.mTime ) / 0.2f, 0.0f, 1.0f);
        const float pop = 1.0f + 0.35f * glm::max(0.0f, 1.0f - number.mTime * 10.0f);
        const float size = ( number.mCrit ? 44.0f : 34.0f ) * inScale * pop;
        const float text_size = 19.0f * inScale * pop;

        g_UIRenderer.AddImage(screen - Vec2(size * 0.5f), Vec2(size), number.mCrit ? m_HitsplatMaxTexture : m_HitsplatTexture, Vec4(1.0f, 1.0f, 1.0f, alpha));
        DrawLabel(screen - Vec2(0.0f, text_size * 0.55f), std::format("{}", number.mValue), text_size, Fade(cRSWhite, alpha), UI_TEXT_ALIGN_CENTER);
    }

    std::erase_if(m_DamageNumbers, [](const DamageNumber& inNumber) { return inNumber.mTime >= 0.75f; });
}


void DuskSwarmScript::DrawPopups(const Vec2& inDisplay, float inDeltaTime, float inScale)
{
    float popup_y = inDisplay.y * 0.24f;

    for (Popup& popup : m_Popups)
    {
        popup.mTime += inDeltaTime;

        const float fade_in = glm::clamp(popup.mTime / 0.15f, 0.0f, 1.0f);
        const float fade_out = glm::clamp(( popup.mDuration - popup.mTime ) / 0.4f, 0.0f, 1.0f);
        const float alpha = fade_in * fade_out;
        const float size = glm::mix(34.0f, 44.0f, EaseOutBack(fade_in)) * inScale;
        const float center = inDisplay.x * 0.5f;

        DrawLabel(Vec2(center, popup_y), popup.mText, size, Fade(popup.mColor, alpha), UI_TEXT_ALIGN_CENTER);
        popup_y += size * 1.15f;

        if (!popup.mSubText.empty())
        {
            DrawLabel(Vec2(center, popup_y), popup.mSubText, 22.0f * inScale, Fade(cRSYellow, alpha), UI_TEXT_ALIGN_CENTER);
            popup_y += 28.0f * inScale;
        }

        popup_y += 18.0f * inScale;
    }

    std::erase_if(m_Popups, [](const Popup& inPopup) { return inPopup.mTime >= inPopup.mDuration; });
}


void DuskSwarmScript::DrawIntro(const Vec2& inDisplay, float inScale)
{
    const float alpha = glm::clamp(m_RunTime / 0.5f, 0.0f, 1.0f) * glm::clamp(( 4.5f - m_RunTime ) / 1.0f, 0.0f, 1.0f);
    const float center = inDisplay.x * 0.5f;
    const float y = inDisplay.y * 0.2f;

    g_UIRenderer.SetFont(m_TitleFont);
    DrawLabel(Vec2(center, y), "Dusk Swarm", 100.0f * inScale, Fade(cRSOrange, alpha), UI_TEXT_ALIGN_CENTER);
    g_UIRenderer.SetFont(m_HudFont);
    DrawLabel(Vec2(center, y + 124.0f * inScale), "Survive three nights", 32.0f * inScale, Fade(cRSYellow, alpha), UI_TEXT_ALIGN_CENTER);
}


void DuskSwarmScript::DrawLevelUp(const Vec2& inDisplay, float inScale)
{
    const float time = m_Time - m_LevelUpTime;
    const float alpha = glm::clamp(time / 0.25f, 0.0f, 1.0f);

    g_UIRenderer.AddRectFilled(Vec2(0.0f), inDisplay, 0.0f, Vec4(0.0f, 0.0f, 0.0f, 0.55f * alpha));

    const Vec2 size = Vec2(900.0f, 520.0f) * inScale;
    const Vec2 pos = inDisplay * 0.5f - size * 0.5f + Vec2(0.0f, ( 1.0f - EaseOutCubic(alpha) ) * 30.0f * inScale);
    const float center = inDisplay.x * 0.5f;

    DrawWindow(pos, size, "Level Up!", inScale, alpha);
    DrawLabel(Vec2(center, pos.y + 80.0f * inScale), std::format("Congratulations, you are now level {}. Choose a power:", m_Level), 21.0f * inScale, Fade(cRSYellow, alpha), UI_TEXT_ALIGN_CENTER);

    const Vec2 box_size = Vec2(264.0f, 334.0f) * inScale;
    const float gap = 22.0f * inScale;
    const float total_width = m_ChoiceCount * box_size.x + ( m_ChoiceCount - 1 ) * gap;

    for (int choice = 0; choice < m_ChoiceCount; choice++)
    {
        const int upgrade = m_Choices[choice];
        const UpgradeInfo& info = cUpgrades[upgrade];
        const int level = m_UpgradeLevels[upgrade];
        const bool selected = choice == m_SelectedChoice;
        const float card_alpha = glm::clamp(( time - 0.06f * choice ) / 0.2f, 0.0f, 1.0f) * alpha;
        const Vec2 box_pos = Vec2(center - total_width * 0.5f + choice * ( box_size.x + gap ), pos.y + 120.0f * inScale - ( selected ? 6.0f * inScale : 0.0f ));
        const float box_center = box_pos.x + box_size.x * 0.5f;

        DrawSlotBox(box_pos, box_size, inScale, card_alpha);

        if (selected)
        {
            const float pulse = 0.5f + 0.5f * glm::sin(m_Time * 5.0f);
            g_UIRenderer.AddRectFilled(box_pos, box_size, 4.0f * inScale, Vec4(1.0f, 1.0f, 0.8f, 0.06f * card_alpha));
            g_UIRenderer.AddRect(box_pos - Vec2(2.0f * inScale), box_size + Vec2(4.0f * inScale), 4.0f * inScale, 2.0f * inScale, Fade(cRSYellow, ( 0.55f + 0.45f * pulse ) * card_alpha));
        }

        DrawLabel(box_pos + Vec2(14.0f, 10.0f) * inScale, std::format("{}", choice + 1), 22.0f * inScale, Fade(cRSYellow, card_alpha));
        DrawUpgradeIcon(upgrade, Vec2(box_center, box_pos.y + 78.0f * inScale), 32.0f * inScale, card_alpha);
        DrawLabel(Vec2(box_center, box_pos.y + 140.0f * inScale), info.mName, 25.0f * inScale, Fade(cRSOrange, card_alpha), UI_TEXT_ALIGN_CENTER);

        const bool is_new = upgrade != UPGRADE_FEAST && level == 0;
        const String chip = upgrade == UPGRADE_FEAST ? String("Restores health") : is_new ? String(info.mIsWeapon ? "New weapon!" : "New!") : std::format("Level {} -> {}", level, level + 1);
        DrawLabel(Vec2(box_center, box_pos.y + 174.0f * inScale), chip, 19.0f * inScale, Fade(is_new ? cRSGreen : cRSYellow, card_alpha), UI_TEXT_ALIGN_CENTER);
        DrawLabel(Vec2(box_center, box_pos.y + 212.0f * inScale), info.mDescription, 19.0f * inScale, Fade(cRSWhite, card_alpha), UI_TEXT_ALIGN_CENTER);

        if (upgrade == UPGRADE_FEAST)
            continue;

        for (int pip = 0; pip < cMaxUpgradeLevel; pip++)
        {
            const Vec2 pip_size = Vec2(16.0f * inScale);
            const Vec2 pip_pos = Vec2(box_center + ( float(pip) - 2.5f ) * 24.0f * inScale + 4.0f * inScale, box_pos.y + box_size.y - 40.0f * inScale);
            const bool next = pip == level;
            const float blink = 0.5f + 0.5f * glm::sin(m_Time * 6.0f);
            const Vec4 color = pip < level ? cRSOrange : next ? Fade(cRSYellow, blink) : Vec4(0.12f, 0.1f, 0.08f, 1.0f);

            g_UIRenderer.AddRectFilled(pip_pos, pip_size, 2.0f * inScale, Fade(color, card_alpha));
            g_UIRenderer.AddRect(pip_pos, pip_size, 2.0f * inScale, glm::max(1.0f, inScale), Vec4(0.0f, 0.0f, 0.0f, card_alpha));
        }
    }

    DrawLabel(Vec2(center, pos.y + size.y - 50.0f * inScale), "Press 1, 2 or 3 to choose, or A / D and Enter", 19.0f * inScale, Fade(cRSWhite, 0.85f * alpha), UI_TEXT_ALIGN_CENTER);
}


void DuskSwarmScript::DrawEndScreen(const Vec2& inDisplay, float inScale)
{
    const bool victory = m_State == STATE_VICTORY;
    const float alpha = glm::clamp(( m_PlayerDeathTimer - 0.6f ) / 0.6f, 0.0f, 1.0f);

    g_UIRenderer.AddRectFilled(Vec2(0.0f), inDisplay, 0.0f, Vec4(0.0f, 0.0f, 0.0f, 0.55f * alpha));

    const Vec2 size = Vec2(620.0f, 430.0f) * inScale;
    const Vec2 pos = inDisplay * 0.5f - size * 0.5f + Vec2(0.0f, ( 1.0f - EaseOutCubic(alpha) ) * 40.0f * inScale);
    const float center = inDisplay.x * 0.5f;

    DrawWindow(pos, size, victory ? "Congratulations!" : "Oh dear, you are dead!", inScale, alpha);
    DrawLabel(Vec2(center, pos.y + 80.0f * inScale), victory ? "You survived the third night. The swarm retreats." : "The night claims another soul.", 21.0f * inScale, Fade(cRSYellow, alpha), UI_TEXT_ALIGN_CENTER);

    const StaticArray<std::pair<String, String>, 6> stats =
    {
        std::pair { String("Survived:"), FormatTime(m_RunTime) },
        std::pair { String("Level:"), std::format("{}", m_Level) },
        std::pair { String("Kills:"), std::format("{}", m_Kills) },
        std::pair { String("Best streak:"), std::format("{}", m_BestStreak) },
        std::pair { String("Elites slain:"), std::format("{}", m_EliteKills) },
        std::pair { String("Damage:"), std::format("{:.1f}k", m_DamageDealt / 1000.0f) },
    };

    const float column_width = ( size.x - 100.0f * inScale ) * 0.5f;

    for (size_t index = 0; index < stats.size(); index++)
    {
        const float x = pos.x + 40.0f * inScale + float(index % 2) * ( column_width + 20.0f * inScale );
        const float y = pos.y + 128.0f * inScale + float(index / 2) * 40.0f * inScale;

        DrawLabel(Vec2(x, y), stats[index].first, 22.0f * inScale, Fade(cRSWhite, alpha));
        DrawLabel(Vec2(x + column_width, y), stats[index].second, 22.0f * inScale, Fade(cRSYellow, alpha), UI_TEXT_ALIGN_RIGHT);
    }

    DrawDivider(Vec2(pos.x + 26.0f * inScale, pos.y + 262.0f * inScale), size.x - 52.0f * inScale, inScale, alpha);
    DrawLabel(Vec2(center, pos.y + 282.0f * inScale), std::format("Personal best: {}", FormatTime(m_BestTime)), 22.0f * inScale, Fade(cRSOrange, alpha), UI_TEXT_ALIGN_CENTER);

    const float blink = 0.6f + 0.4f * glm::sin(m_Time * 4.0f);
    DrawLabel(Vec2(center, pos.y + size.y - 70.0f * inScale), "Press Enter to play again", 26.0f * inScale, Fade(cRSYellow, alpha * blink), UI_TEXT_ALIGN_CENTER);
}

} // RK
