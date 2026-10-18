#define RAEKOR_SCRIPT
#include "CoastRallyScript.h"

namespace RK {

void CoastRallyScript::CreateAudio()
{
    const auto CreateLoop = [this](StringView inName, const char* inFile, ESoundCategory inCategory, bool inStream)
    {
        const Entity entity = CreateEntity(inName);

        SoundSource& source = m_Scene->Add<SoundSource>(entity);
        source.file = inFile;
        source.category = inCategory;
        source.volume = 0.0f;
        source.loop = true;
        source.stream = inStream;
        source.spatial = false;
        source.fadeInTime = 0.3f;
        source.fadeOutTime = 0.3f;

        return entity;
    };

    m_EngineEntity = CreateLoop("Engine", cEngineSound, SOUND_CATEGORY_EFFECTS, false);
    m_SkidEntity = CreateLoop("Skid", cSkidSound, SOUND_CATEGORY_EFFECTS, false);
    m_WindEntity = CreateLoop("Wind", cWindSound, SOUND_CATEGORY_EFFECTS, false);
    m_AmbienceEntity = CreateLoop("Coast Ambience", cAmbienceSound, SOUND_CATEGORY_AMBIENCE, true);

    m_MusicEntity = CreateLoop("Music", cMusicSound, SOUND_CATEGORY_MUSIC, true);

    if (SoundSource* ambience = FindComponent<SoundSource>(m_AmbienceEntity))
    {
        ambience->volume = 0.55f;
        ambience->fadeInTime = 2.0f;
    }

    if (SoundSource* music = FindComponent<SoundSource>(m_MusicEntity))
    {
        music->volume = cMusicVolume;
        music->fadeInTime = 1.5f;
        music->fadeOutTime = 1.2f;
    }

    PlayEntitySound(m_MusicEntity);

    if (Audio* audio = GetAudio())
    {
        for (const char* sound : cSoundEffects)
            audio->Preload(sound);
    }
}


void CoastRallyScript::PlaySfx(const char* inFile, float inVolume, float inPitch)
{
    if (Audio* audio = GetAudio())
        audio->PlayOneShot(inFile, inVolume, inPitch);
}


void CoastRallyScript::PlaySfxAt(const char* inFile, const Vec3& inPosition, float inVolume, float inPitch)
{
    if (Audio* audio = GetAudio())
        audio->PlayOneShot(inFile, inPosition, inVolume, inPitch, 6.0f);
}


void CoastRallyScript::PlayEntitySound(Entity inEntity)
{
    if (Audio* audio = GetAudio())
        audio->Play(*m_Scene, inEntity);
}


void CoastRallyScript::StopEntitySound(Entity inEntity)
{
    if (Audio* audio = GetAudio())
        audio->Stop(*m_Scene, inEntity);
}


void CoastRallyScript::UpdateSounds(float inDeltaTime)
{
    const JPH::WheeledVehicleController* controller = GetController();
    const float smoothing = 1.0f - glm::exp(-inDeltaTime * 10.0f);

    if (SoundSource* engine = FindComponent<SoundSource>(m_EngineEntity); engine && controller)
    {
        const float rpm = controller->GetEngine().GetCurrentRPM();
        const float target_volume = 0.32f + 0.3f * m_Throttle + ( m_Boosting ? 0.08f : 0.0f );

        engine->pitch = glm::mix(engine->pitch, glm::clamp(rpm / cEngineSoundRPM, 0.35f, 2.4f), smoothing);
        engine->volume = glm::mix(engine->volume, target_volume, smoothing);
    }

    if (SoundSource* skid = FindComponent<SoundSource>(m_SkidEntity))
    {
        const float amount = GetSkidAmount();

        skid->volume = glm::mix(skid->volume, 0.55f * amount, smoothing);
        skid->pitch = 0.9f + 0.2f * amount;
    }

    if (SoundSource* wind = FindComponent<SoundSource>(m_WindEntity))
    {
        const float amount = glm::clamp(( glm::length(m_CarVelocity) - 8.0f ) / 40.0f, 0.0f, 1.0f);

        wind->volume = glm::mix(wind->volume, 0.45f * amount, smoothing);
        wind->pitch = 0.8f + 0.4f * amount;
    }
}

} // RK
