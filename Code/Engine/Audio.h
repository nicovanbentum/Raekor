#pragma once

#include "ECS.h"
#include "CVars.h"
#include "Defines.h"

struct ma_sound;
struct ma_engine;

namespace RK {

class Scene;
class Viewport;

struct SoundSource;


class Audio
{
public:
	NO_COPY_NO_MOVE(Audio);

	static constexpr uint32_t cMaxOneShots = 64;
	static constexpr float cMaxDopplerSpeed = 100.0f;

	Audio();
	~Audio();

	bool IsInitialized() const { return m_Engine != nullptr; }

	void UpdateListener(Scene& inScene, const Viewport& inViewport, float inDeltaTime);
	void OnUpdate(Scene& inScene, float inDeltaTime, bool inGameRunning);

	void OnStop();

	void Pause();
	void Unpause();
	bool IsPaused() const { return m_Paused; }

	bool Play(Scene& inScene, Entity inEntity);
	void Stop(Scene& inScene, Entity inEntity);
	void Stop(Scene& inScene, Entity inEntity, float inFadeTime);
	bool IsPlaying(const Scene& inScene, Entity inEntity) const;

	void StopAll();

	bool Preload(const String& inFile);

	void PlayOneShot(const String& inFile, float inVolume = 1.0f, float inPitch = 1.0f);
	void PlayOneShot(const String& inFile, const Vec3& inPosition, float inVolume = 1.0f, float inPitch = 1.0f, float inMinDistance = 1.0f);

	const Vec3& GetListenerPosition() const { return m_ListenerPosition; }

	static bool sIsSupportedFile(const Path& inFile);
	static const char* sGetFileDialogFilters();

private:
	struct Voice
	{
		Entity mEntity = Entity::Null;
		String mFile;
		bool mStream = false;
		uint32_t mCategory = 0;
		ma_sound* mSound = nullptr;
		Vec3 mPrevPosition = Vec3(0.0f);
		bool mHasPrevPosition = false;

		bool Matches(const SoundSource& inSource) const;
	};

	Voice* FindVoice(const Scene& inScene, Entity inEntity);
	const Voice* FindVoice(const Scene& inScene, Entity inEntity) const;

	ma_sound* CreateSound(const String& inFile, uint32_t inFlags, ma_sound* inGroup);
	void DestroySound(ma_sound* inSound);

	void ApplySettings(ma_sound* inSound, const SoundSource& inSource);
	void UpdateVoices(Scene& inScene, float inDeltaTime);
	void UpdateTriggeredSources(Scene& inScene);
	void UpdateOneShots();
	void UpdateVolumes();

	void ReleaseVoice(uint32_t inVoiceID);
	ma_sound* GetGroup(uint32_t inCategory) const;

private:
	struct
	{
		float& masterVolume = g_CVariables->Create("audio_volume", 1.0f, true);
		float& effectsVolume = g_CVariables->Create("audio_volume_effects", 1.0f, true);
		float& ambienceVolume = g_CVariables->Create("audio_volume_ambience", 1.0f, true);
		float& musicVolume = g_CVariables->Create("audio_volume_music", 1.0f, true);
	} m_Settings;

	ma_engine* m_Engine = nullptr;
	Array<ma_sound*> m_Groups;

	uint32_t m_NextVoiceID = 1;
	HashMap<uint32_t, Voice> m_Voices;
	Array<ma_sound*> m_OneShots;
	HashSet<String> m_PreloadedFiles;

	bool m_Paused = false;
	bool m_HasPrevListenerPosition = false;
	Vec3 m_ListenerPosition = Vec3(0.0f);
};

} // namespace RK
