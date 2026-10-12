#include "PCH.h"
#include "Audio.h"
#include "Scene.h"
#include "Camera.h"
#include "Profiler.h"
#include "Components.h"

#include "miniaudio/miniaudio.h"

namespace RK {

static_assert(SOUND_CATEGORY_COUNT == 3);


Audio::Audio()
{
	ma_engine_config config = ma_engine_config_init();
	config.listenerCount = 1;

	m_Engine = new ma_engine();

	if (ma_engine_init(&config, m_Engine) != MA_SUCCESS)
	{
		gLogError("Audio", "Failed to initialize the audio engine, sound is disabled");

		delete m_Engine;
		m_Engine = nullptr;
		return;
	}

	ma_engine_listener_set_world_up(m_Engine, 0, Camera::cUp.x, Camera::cUp.y, Camera::cUp.z);

	for (uint32_t category = 0; category < SOUND_CATEGORY_COUNT; category++)
	{
		ma_sound_group* group = new ma_sound_group();

		if (ma_sound_group_init(m_Engine, 0, nullptr, group) != MA_SUCCESS)
		{
			delete group;
			group = nullptr;
		}

		m_Groups.push_back(group);
	}

	UpdateVolumes();

	gLogInfo("Audio", "Initialized audio engine at {} Hz with {} channels", ma_engine_get_sample_rate(m_Engine), ma_engine_get_channels(m_Engine));
}


Audio::~Audio()
{
	if (!m_Engine)
		return;

	StopAll();

	for (ma_sound* sound : m_OneShots)
		DestroySound(sound);

	m_OneShots.clear();

	for (const String& file : m_PreloadedFiles)
		ma_resource_manager_unregister_file(ma_engine_get_resource_manager(m_Engine), file.c_str());

	for (ma_sound_group* group : m_Groups)
	{
		if (group)
		{
			ma_sound_group_uninit(group);
			delete group;
		}
	}

	ma_engine_uninit(m_Engine);
	delete m_Engine;
}


bool Audio::sIsSupportedFile(const Path& inFile)
{
	const String extension = inFile.extension().string();

	for (const char* supported : { ".wav", ".mp3", ".flac", ".ogg" })
	{
		if (_stricmp(extension.c_str(), supported) == 0)
			return true;
	}

	return false;
}


const char* Audio::sGetFileDialogFilters()
{
	return "Audio Files (*.wav, *.mp3, *.flac, *.ogg)\0*.wav;*.mp3;*.flac;*.ogg\0";
}


bool Audio::Voice::Matches(const SoundSource& inSource) const
{
	return mFile == inSource.file && mStream == inSource.stream && mCategory == uint32_t(inSource.category);
}


ma_sound_group* Audio::GetGroup(uint32_t inCategory) const
{
	return inCategory < m_Groups.size() ? m_Groups[inCategory] : nullptr;
}


ma_sound* Audio::CreateSound(const String& inFile, uint32_t inFlags, ma_sound_group* inGroup)
{
	if (!m_Engine || inFile.empty())
		return nullptr;

	if (!fs::exists(inFile))
	{
		gLogWarning("Audio", "Sound file {} does not exist", inFile);
		return nullptr;
	}

	ma_sound* sound = new ma_sound();

	if (ma_sound_init_from_file(m_Engine, inFile.c_str(), inFlags, inGroup, nullptr, sound) != MA_SUCCESS)
	{
		gLogError("Audio", "Failed to load sound file {}", inFile);

		delete sound;
		return nullptr;
	}

	return sound;
}


void Audio::DestroySound(ma_sound* inSound)
{
	if (inSound)
	{
		ma_sound_uninit(inSound);
		delete inSound;
	}
}


void Audio::ApplySettings(ma_sound* inSound, const SoundSource& inSource)
{
	ma_sound_set_volume(inSound, glm::max(inSource.volume, 0.0f));
	ma_sound_set_pitch(inSound, glm::max(inSource.pitch, 0.01f));
	ma_sound_set_looping(inSound, inSource.loop);
	ma_sound_set_spatialization_enabled(inSound, inSource.spatial);

	if (inSource.spatial)
	{
		ma_sound_set_attenuation_model(inSound, ma_attenuation_model_inverse);
		ma_sound_set_min_distance(inSound, glm::max(inSource.minDistance, 0.01f));
		ma_sound_set_max_distance(inSound, glm::max(inSource.maxDistance, inSource.minDistance));
		ma_sound_set_rolloff(inSound, glm::max(inSource.rolloff, 0.0f));
		ma_sound_set_doppler_factor(inSound, glm::max(inSource.dopplerFactor, 0.0f));
	}
}


Audio::Voice* Audio::FindVoice(const Scene& inScene, Entity inEntity)
{
	const SoundSource* source = inScene.GetPtr<SoundSource>(inEntity);

	if (!source || source->voice == 0)
		return nullptr;

	const auto voice = m_Voices.find(source->voice);

	if (voice == m_Voices.end() || voice->second.mEntity != inEntity)
		return nullptr;

	return &voice->second;
}


const Audio::Voice* Audio::FindVoice(const Scene& inScene, Entity inEntity) const
{
	return const_cast<Audio*>( this )->FindVoice(inScene, inEntity);
}


bool Audio::Play(Scene& inScene, Entity inEntity)
{
	if (!m_Engine)
		return false;

	SoundSource* source = inScene.GetPtr<SoundSource>(inEntity);

	if (!source)
		return false;

	Voice* voice = FindVoice(inScene, inEntity);

	if (voice && !voice->Matches(*source))
	{
		ReleaseVoice(source->voice);
		voice = nullptr;
	}

	if (!voice)
	{
		const uint32_t flags = MA_SOUND_FLAG_ASYNC | ( source->stream ? MA_SOUND_FLAG_STREAM : MA_SOUND_FLAG_DECODE );
		const uint32_t voice_id = m_NextVoiceID++;

		voice = &m_Voices[voice_id];
		voice->mEntity = inEntity;
		voice->mFile = source->file;
		voice->mStream = source->stream;
		voice->mCategory = source->category;
		voice->mSound = CreateSound(source->file, flags, GetGroup(source->category));

		source->voice = voice_id;
	}

	ma_sound* sound = voice->mSound;

	if (!sound)
		return false;

	ApplySettings(sound, *source);

	if (const Transform* transform = inScene.GetPtr<Transform>(inEntity))
	{
		const Vec3 position = transform->GetPositionWorldSpace();
		ma_sound_set_position(sound, position.x, position.y, position.z);
		ma_sound_set_velocity(sound, 0.0f, 0.0f, 0.0f);

		voice->mPrevPosition = position;
		voice->mHasPrevPosition = true;
	}

	const bool was_playing = ma_sound_is_playing(sound);

	ma_sound_reset_stop_time_and_fade(sound);

	if (!was_playing || !source->loop)
		ma_sound_seek_to_pcm_frame(sound, 0);

	if (source->fadeInTime > 0.0f)
		ma_sound_set_fade_in_milliseconds(sound, was_playing ? -1.0f : 0.0f, 1.0f, ma_uint64(source->fadeInTime * 1000.0f));

	return ma_sound_start(sound) == MA_SUCCESS;
}


void Audio::Stop(Scene& inScene, Entity inEntity)
{
	if (const SoundSource* source = inScene.GetPtr<SoundSource>(inEntity))
		Stop(inScene, inEntity, source->fadeOutTime);
}


void Audio::Stop(Scene& inScene, Entity inEntity, float inFadeTime)
{
	Voice* voice = FindVoice(inScene, inEntity);

	if (!voice || !voice->mSound || !ma_sound_is_playing(voice->mSound))
		return;

	if (inFadeTime > 0.0f)
		ma_sound_stop_with_fade_in_milliseconds(voice->mSound, ma_uint64(inFadeTime * 1000.0f));
	else
		ma_sound_stop(voice->mSound);
}


bool Audio::IsPlaying(const Scene& inScene, Entity inEntity) const
{
	const Voice* voice = FindVoice(inScene, inEntity);
	return voice && voice->mSound && ma_sound_is_playing(voice->mSound);
}


void Audio::ReleaseVoice(uint32_t inVoiceID)
{
	const auto voice = m_Voices.find(inVoiceID);

	if (voice == m_Voices.end())
		return;

	DestroySound(voice->second.mSound);
	m_Voices.erase(voice);
}


void Audio::StopAll()
{
	for (auto& [id, voice] : m_Voices)
		DestroySound(voice.mSound);

	m_Voices.clear();

	for (ma_sound* sound : m_OneShots)
		DestroySound(sound);

	m_OneShots.clear();
}


void Audio::OnStop()
{
	StopAll();
	Unpause();
}


void Audio::Pause()
{
	if (m_Engine && !m_Paused)
	{
		ma_engine_stop(m_Engine);
		m_Paused = true;
	}
}


void Audio::Unpause()
{
	if (m_Engine && m_Paused)
	{
		ma_engine_start(m_Engine);
		m_Paused = false;
	}
}


bool Audio::Preload(const String& inFile)
{
	if (!m_Engine || inFile.empty())
		return false;

	if (m_PreloadedFiles.contains(inFile))
		return true;

	if (!fs::exists(inFile))
	{
		gLogWarning("Audio", "Sound file {} does not exist", inFile);
		return false;
	}

	const ma_uint32 flags = MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_DECODE | MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_ASYNC;

	if (ma_resource_manager_register_file(ma_engine_get_resource_manager(m_Engine), inFile.c_str(), flags) != MA_SUCCESS)
	{
		gLogError("Audio", "Failed to preload sound file {}", inFile);
		return false;
	}

	m_PreloadedFiles.insert(inFile);
	return true;
}


void Audio::PlayOneShot(const String& inFile, float inVolume, float inPitch)
{
	if (!m_Engine || m_OneShots.size() >= cMaxOneShots || !Preload(inFile))
		return;

	const uint32_t flags = MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_ASYNC | MA_SOUND_FLAG_NO_SPATIALIZATION;

	if (ma_sound* sound = CreateSound(inFile, flags, GetGroup(SOUND_CATEGORY_EFFECTS)))
	{
		ma_sound_set_volume(sound, glm::max(inVolume, 0.0f));
		ma_sound_set_pitch(sound, glm::max(inPitch, 0.01f));
		ma_sound_start(sound);

		m_OneShots.push_back(sound);
	}
}


void Audio::PlayOneShot(const String& inFile, const Vec3& inPosition, float inVolume, float inPitch, float inMinDistance)
{
	if (!m_Engine || m_OneShots.size() >= cMaxOneShots || !Preload(inFile))
		return;

	const uint32_t flags = MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_ASYNC;

	if (ma_sound* sound = CreateSound(inFile, flags, GetGroup(SOUND_CATEGORY_EFFECTS)))
	{
		ma_sound_set_volume(sound, glm::max(inVolume, 0.0f));
		ma_sound_set_pitch(sound, glm::max(inPitch, 0.01f));
		ma_sound_set_attenuation_model(sound, ma_attenuation_model_inverse);
		ma_sound_set_min_distance(sound, glm::max(inMinDistance, 0.01f));
		ma_sound_set_position(sound, inPosition.x, inPosition.y, inPosition.z);
		ma_sound_start(sound);

		m_OneShots.push_back(sound);
	}
}


void Audio::UpdateListener(Scene& inScene, const Viewport& inViewport, float inDeltaTime)
{
	if (!m_Engine)
		return;

	Vec3 position = inViewport.GetPosition();
	Vec3 forward = -Vec3(glm::inverse(inViewport.GetView())[2]);

	for (const auto& [entity, listener, transform] : inScene.Each<AudioListener, Transform>())
	{
		position = transform.GetPositionWorldSpace();
		forward = transform.GetRotationWorldSpace() * Camera::cForward;
		break;
	}

	Vec3 velocity = Vec3(0.0f);

	if (m_HasPrevListenerPosition && inDeltaTime > 0.0f)
	{
		velocity = ( position - m_ListenerPosition ) / inDeltaTime;

		if (glm::length(velocity) > cMaxDopplerSpeed)
			velocity = Vec3(0.0f);
	}

	if (glm::length(forward) < 1e-4f)
		forward = Camera::cForward;

	forward = glm::normalize(forward);

	ma_engine_listener_set_position(m_Engine, 0, position.x, position.y, position.z);
	ma_engine_listener_set_direction(m_Engine, 0, forward.x, forward.y, forward.z);
	ma_engine_listener_set_velocity(m_Engine, 0, velocity.x, velocity.y, velocity.z);

	m_ListenerPosition = position;
	m_HasPrevListenerPosition = true;
}


void Audio::UpdateVoices(Scene& inScene, float inDeltaTime)
{
	for (auto voice = m_Voices.begin(); voice != m_Voices.end();)
	{
		const SoundSource* source = inScene.GetPtr<SoundSource>(voice->second.mEntity);

		if (!source || source->voice != voice->first || !voice->second.Matches(*source))
		{
			DestroySound(voice->second.mSound);
			voice = m_Voices.erase(voice);
		}
		else
			voice++;
	}

	for (const auto& [entity, source] : inScene.Each<SoundSource>())
	{
		if (source.voice == 0)
			continue;

		const auto voice_entry = m_Voices.find(source.voice);

		if (voice_entry == m_Voices.end() || voice_entry->second.mEntity != entity)
		{
			source.voice = 0;
			continue;
		}

		Voice& voice = voice_entry->second;

		if (!voice.mSound)
			continue;

		ApplySettings(voice.mSound, source);

		const Transform* transform = inScene.GetPtr<Transform>(entity);

		if (!source.spatial || !transform)
			continue;

		const Vec3 position = transform->GetPositionWorldSpace();

		Vec3 velocity = Vec3(0.0f);

		if (voice.mHasPrevPosition && inDeltaTime > 0.0f)
		{
			velocity = ( position - voice.mPrevPosition ) / inDeltaTime;

			if (glm::length(velocity) > cMaxDopplerSpeed)
				velocity = Vec3(0.0f);
		}

		ma_sound_set_position(voice.mSound, position.x, position.y, position.z);
		ma_sound_set_velocity(voice.mSound, velocity.x, velocity.y, velocity.z);

		voice.mPrevPosition = position;
		voice.mHasPrevPosition = true;
	}
}


void Audio::UpdateTriggeredSources(Scene& inScene)
{
	for (const auto& [entity, source] : inScene.Each<SoundSource>())
	{
		if (source.playOnStart && source.voice == 0)
			Play(inScene, entity);

		const TriggerVolume* trigger = inScene.GetPtr<TriggerVolume>(entity);

		if (!trigger)
			continue;

		if (trigger->entered && source.playOnTriggerEnter)
			Play(inScene, entity);

		if (trigger->exited && source.stopOnTriggerExit)
			Stop(inScene, entity);
	}
}


void Audio::UpdateOneShots()
{
	for (size_t index = 0; index < m_OneShots.size();)
	{
		ma_sound* sound = m_OneShots[index];

		if (ma_sound_at_end(sound))
		{
			DestroySound(sound);
			m_OneShots[index] = m_OneShots.back();
			m_OneShots.pop_back();
		}
		else
			index++;
	}
}


void Audio::UpdateVolumes()
{
	if (!m_Engine)
		return;

	ma_engine_set_volume(m_Engine, glm::clamp(m_Settings.masterVolume, 0.0f, 4.0f));

	const StaticArray<float, SOUND_CATEGORY_COUNT> volumes =
	{
		m_Settings.effectsVolume,
		m_Settings.ambienceVolume,
		m_Settings.musicVolume
	};

	for (uint32_t category = 0; category < SOUND_CATEGORY_COUNT; category++)
	{
		if (ma_sound_group* group = GetGroup(category))
			ma_sound_group_set_volume(group, glm::clamp(volumes[category], 0.0f, 4.0f));
	}
}


void Audio::OnUpdate(Scene& inScene, float inDeltaTime, bool inGameRunning)
{
	PROFILE_FUNCTION_CPU();

	if (!m_Engine)
		return;

	UpdateVolumes();

	UpdateVoices(inScene, inDeltaTime);

	if (inGameRunning && !m_Paused)
		UpdateTriggeredSources(inScene);

	UpdateOneShots();
}

} // namespace RK
