#define RAEKOR_SCRIPT
#include "DuskSwarmScript.h"

namespace RK {

Entity DuskSwarmScript::CreateMusic(StringView inName, const char* inFile, float inVolume)
{
    const Entity entity = CreateEntity(inName);

    SoundSource& music = m_Scene->Add<SoundSource>(entity);
    music.file = inFile;
    music.category = SOUND_CATEGORY_MUSIC;
    music.volume = inVolume;
    music.loop = true;
    music.stream = true;
    music.spatial = false;
    music.fadeInTime = 2.0f;
    music.fadeOutTime = 1.5f;

    return entity;
}


void DuskSwarmScript::CreateAudio()
{
    m_ListenerEntity = CreateEntity("Listener");
    m_Scene->Add<AudioListener>(m_ListenerEntity);

    m_MusicEntity = CreateMusic("Music", cMusicSound, 0.5f);
    m_BossMusicEntity = CreateMusic("Boss Music", cBossMusicSound, 0.55f);

    const Entity ambience_entity = CreateEntity("Night Ambience");

    SoundSource& ambience = m_Scene->Add<SoundSource>(ambience_entity);
    ambience.file = cAmbienceSound;
    ambience.category = SOUND_CATEGORY_AMBIENCE;
    ambience.volume = 0.7f;
    ambience.loop = true;
    ambience.stream = true;
    ambience.spatial = false;
    ambience.playOnStart = true;
    ambience.fadeInTime = 2.0f;

    if (Audio* audio = GetAudio())
    {
        for (const char* sound : cSoundEffects)
            audio->Preload(sound);
    }
}


bool DuskSwarmScript::CanPlaySfx(const char* inFile, float inMinInterval)
{
    if (inMinInterval <= 0.0f)
        return true;

    const auto [entry, inserted] = m_SoundTimes.try_emplace(inFile, m_Time);

    if (!inserted)
    {
        if (m_Time - entry->second < inMinInterval)
            return false;

        entry->second = m_Time;
    }

    return true;
}


void DuskSwarmScript::PlaySfx(const char* inFile, float inVolume, float inPitch, float inMinInterval)
{
    Audio* audio = GetAudio();

    if (audio && CanPlaySfx(inFile, inMinInterval))
        audio->PlayOneShot(inFile, inVolume, inPitch);
}


void DuskSwarmScript::PlaySfxAt(const char* inFile, const Vec3& inPosition, float inVolume, float inPitch, float inMinInterval)
{
    Audio* audio = GetAudio();

    if (audio && CanPlaySfx(inFile, inMinInterval))
        audio->PlayOneShot(inFile, inPosition, inVolume, inPitch, cSoundMinDistance);
}


void DuskSwarmScript::PlayEntitySound(Entity inEntity)
{
    if (Audio* audio = GetAudio())
        audio->Play(*m_Scene, inEntity);
}


void DuskSwarmScript::StopEntitySound(Entity inEntity)
{
    if (Audio* audio = GetAudio())
        audio->Stop(*m_Scene, inEntity);
}


void DuskSwarmScript::PlayMusic(bool inBoss)
{
    StopEntitySound(inBoss ? m_MusicEntity : m_BossMusicEntity);
    PlayEntitySound(inBoss ? m_BossMusicEntity : m_MusicEntity);
}


void DuskSwarmScript::StopMusic()
{
    StopEntitySound(m_MusicEntity);
    StopEntitySound(m_BossMusicEntity);
}


void DuskSwarmScript::UpdateSounds()
{
    PROFILE_FUNCTION_CPU();

    SetTransform(m_ListenerEntity, ToWorld(m_PlayerPosition, cListenerHeight), Vec3(1.0f));

    if (m_State == STATE_GAME_OVER && !m_EndJinglePlayed && m_PlayerDeathTimer >= cGameOverJingleDelay)
    {
        m_EndJinglePlayed = true;
        PlaySfx(cGameOverSound, 0.8f);
    }
}

} // RK
