#include "pch.h"
#include "animation.h"
#include "member.h"
#include "timer.h"

namespace RK {

RTTI_DEFINE_TYPE(Vec3Key)
{
	RTTI_DEFINE_MEMBER(Vec3Key, SERIALIZE_ALL, "Time", mTime);
	RTTI_DEFINE_MEMBER(Vec3Key, SERIALIZE_ALL, "Value", mValue);
}


RTTI_DEFINE_TYPE(QuatKey)
{
	RTTI_DEFINE_MEMBER(QuatKey, SERIALIZE_ALL, "Time", mTime);
	RTTI_DEFINE_MEMBER(QuatKey, SERIALIZE_ALL, "Value", mValue);
}


RTTI_DEFINE_TYPE(KeyFrames)
{
	RTTI_DEFINE_MEMBER(KeyFrames, SERIALIZE_ALL, "Scale Keys", m_ScaleKeys);
	RTTI_DEFINE_MEMBER(KeyFrames, SERIALIZE_ALL, "Position Keys", m_PositionKeys);
	RTTI_DEFINE_MEMBER(KeyFrames, SERIALIZE_ALL, "Rotation Keys", m_RotationKeys);
}


RTTI_DEFINE_TYPE(Animation)
{
	RTTI_DEFINE_TYPE_INHERITANCE(Animation, Component);
	RTTI_DEFINE_MEMBER(Animation, SERIALIZE_ALL, "Name", m_Name);
	RTTI_DEFINE_MEMBER(Animation, SERIALIZE_ALL, "Duration", m_TotalDuration);
	RTTI_DEFINE_MEMBER(Animation, SERIALIZE_ALL, "Bone Keyframe Map", m_KeyFrames);
}


#ifndef DEPRECATE_ASSIMP

void KeyFrames::LoadFromAssimp(const aiNodeAnim* nodeAnim)
{
	for (const auto& key : Slice(nodeAnim->mScalingKeys, nodeAnim->mNumScalingKeys))
		m_ScaleKeys.push_back(Vec3Key(key.mTime, Vec3(key.mValue.x, key.mValue.y, key.mValue.z)));

	for (const auto& key : Slice(nodeAnim->mPositionKeys, nodeAnim->mNumPositionKeys))
		m_PositionKeys.push_back(Vec3Key(key.mTime, Vec3(key.mValue.x, key.mValue.y, key.mValue.z)));

	for (const auto& key : Slice(nodeAnim->mRotationKeys, nodeAnim->mNumRotationKeys))
		m_RotationKeys.push_back(QuatKey(key.mTime, Quat(key.mValue.w, key.mValue.x, key.mValue.y, key.mValue.z)));
}

#endif

void KeyFrames::LoadFromGltf(const cgltf_animation_channel* channel)
{
	assert(channel->sampler->interpolation == cgltf_interpolation_type_linear);
	float buffer[4]; // covers time (scalar float values), pos and scale (vec3) and rotation (quat)

	switch (channel->target_path)
	{
		case cgltf_animation_path_type_translation:
		{
			for (uint32_t index = 0; index < channel->sampler->input->count; index++)
			{
				Vec3Key& key = m_PositionKeys.emplace_back();

				const cgltf_size num_components = cgltf_num_components(channel->sampler->input->type);
				assert(num_components == 1);

				cgltf_accessor_read_float(channel->sampler->input, index, buffer, num_components);
				key.mTime = Timer::sToMilliseconds(buffer[0]);
			}

			for (uint32_t index = 0; index < channel->sampler->output->count; index++)
			{
				assert(index < m_PositionKeys.size());
				Vec3Key& key = m_PositionKeys[index];

				const cgltf_size num_components = cgltf_num_components(channel->sampler->output->type);
				assert(num_components == 3);

				cgltf_accessor_read_float(channel->sampler->output, index, buffer, num_components);
				key.mValue = Vec3(buffer[0], buffer[1], buffer[2]);
			}

			break;
		}
		case cgltf_animation_path_type_rotation:
		{
			for (uint32_t index = 0; index < channel->sampler->input->count; index++)
			{
				QuatKey& key = m_RotationKeys.emplace_back();

				const int num_components = cgltf_num_components(channel->sampler->input->type);
				assert(num_components == 1);

				cgltf_accessor_read_float(channel->sampler->input, index, buffer, num_components);
				key.mTime = Timer::sToMilliseconds(buffer[0]);
			}

			for (uint32_t index = 0; index < channel->sampler->output->count; index++)
			{
				assert(index < m_RotationKeys.size());
				QuatKey& key = m_RotationKeys[index];

				const int num_components = cgltf_num_components(channel->sampler->output->type);
				assert(num_components == 4);

				cgltf_accessor_read_float(channel->sampler->output, index, buffer, num_components);
				key.mValue = Quat(buffer[3], buffer[0], buffer[1], buffer[2]);
			}

			break;
		}
		case cgltf_animation_path_type_scale:
		{
			for (uint32_t index = 0; index < channel->sampler->input->count; index++)
			{
				Vec3Key& key = m_ScaleKeys.emplace_back();

				const int num_components = cgltf_num_components(channel->sampler->input->type);
				assert(num_components == 1);

				cgltf_accessor_read_float(channel->sampler->input, index, buffer, num_components);
				key.mTime = Timer::sToMilliseconds(buffer[0]);
			}

			for (uint32_t index = 0; index < channel->sampler->output->count; index++)
			{
				assert(index < m_ScaleKeys.size());
				Vec3Key& key = m_ScaleKeys[index];

				const int num_components = cgltf_num_components(channel->sampler->output->type);
				assert(num_components == 3);

				cgltf_accessor_read_float(channel->sampler->output, index, buffer, num_components);
				key.mValue = Vec3(buffer[0], buffer[1], buffer[2]);
			}

			break;
		}
	}
}


Vec3 KeyFrames::GetInterpolatedPosition(float animationTime) const
{
	if (m_PositionKeys.size() == 1)
		return m_PositionKeys[0].mValue;

	int pos_index = 0;
	for (int i = 0; i < m_PositionKeys.size() - 1; i++)
	{
		if (animationTime < (float)m_PositionKeys[i + 1].mTime)
		{
			pos_index = i;
			break;
		}
	}

	int NextPositionIndex = ( pos_index + 1 );
	float delta_time = m_PositionKeys[NextPositionIndex].mTime - m_PositionKeys[pos_index].mTime;

	float factor = ( animationTime - (float)m_PositionKeys[pos_index].mTime ) / delta_time;
	if (factor < 0.0f)
		factor = 0.0f;

	const Vec3& start = m_PositionKeys[pos_index].mValue;
	const Vec3& end = m_PositionKeys[NextPositionIndex].mValue;
	Vec3 delta = end - start;
	Vec3 ai_vec = start + factor * delta;

	return { ai_vec.x, ai_vec.y, ai_vec.z };
}


Quat KeyFrames::GetInterpolatedRotation(float animationTime) const
{
	if (m_RotationKeys.size() == 1)
		return m_RotationKeys[0].mValue;

	int rotation_index = 0;
	for (int i = 0; i < m_RotationKeys.size() - 1; i++)
	{
		if (animationTime < (float)m_RotationKeys[i + 1].mTime)
		{
			rotation_index = i;
			break;
		}
	}

	int next_rotation_index = ( rotation_index + 1 );

	float delta_time = (float)( m_RotationKeys[next_rotation_index].mTime - m_RotationKeys[rotation_index].mTime );
	float factor = ( animationTime - (float)m_RotationKeys[rotation_index].mTime ) / delta_time;
	if (factor < 0.0f)
		factor = 0.0f;

	const Quat& start_rotation_quat = m_RotationKeys[rotation_index].mValue;
	const Quat& end_rotation_quat = m_RotationKeys[next_rotation_index].mValue;

	return glm::normalize(glm::slerp(start_rotation_quat, end_rotation_quat, factor));
}


Vec3 KeyFrames::GetInterpolatedScale(float animationTime) const
{
	if (m_ScaleKeys.size() == 1)
		return m_ScaleKeys[0].mValue;

	int index = 0;
	for (int i = 0; i < m_ScaleKeys.size() - 1; i++)
	{
		if (animationTime < (float)m_ScaleKeys[i + 1].mTime)
		{
			index = i;
			break;
		}
	}

	int next_index = ( index + 1 );
	float delta_time = (float)( m_ScaleKeys[next_index].mTime - m_ScaleKeys[index].mTime );

	float factor = ( animationTime - (float)m_ScaleKeys[index].mTime ) / delta_time;
	if (factor < 0.0f)
		factor = 0.0f;

	const Vec3& start = m_ScaleKeys[index].mValue;
	const Vec3& end = m_ScaleKeys[next_index].mValue;
	Vec3 delta = end - start;
	Vec3 aiVec = start + factor * delta;

	return { aiVec.x, aiVec.y, aiVec.z };
}

#ifndef DEPRECATE_ASSIMP

Animation::Animation(const aiAnimation* inAnimation)
{
	m_Name = inAnimation->mName.C_Str();
	m_RunningTime = 0;
	m_TotalDuration = float(inAnimation->mDuration);
}

#endif

Animation::Animation(const cgltf_animation* inAnimation) : m_Name(inAnimation->name ? inAnimation->name : "Animation")
{
	for (const cgltf_animation_channel& channel : Slice(inAnimation->channels, inAnimation->channels_count))
	{
		m_TotalDuration = glm::max(m_TotalDuration, Timer::sToMilliseconds(channel.sampler->input->max[0]));
	}
}

#ifndef DEPRECATE_ASSIMP

void Animation::LoadKeyframes(uint32_t inBoneIndex, const aiNodeAnim* inAnimation)
{
	m_KeyFrames.insert({ inBoneIndex, KeyFrames(inBoneIndex) });
	m_KeyFrames[inBoneIndex].LoadFromAssimp(inAnimation);
}

#endif


void Animation::OnUpdate(float inDeltaTime)
{
	if (m_IsPlaying)
	{
		/*
			This is bugged, Assimp docs say totalDuration is in ticks, but the actual value is real world time in milliseconds
			see https://github.com/assimp/assimp/issues/2662
		*/
		m_RunningTime += Timer::sToMilliseconds(inDeltaTime);

		if (m_RunningTime > m_TotalDuration)
			m_RunningTime = 0;
	}
}


void Animation::LoadKeyframes(const std::string& inBoneName, const cgltf_animation_channel* inAnimation)
{
	// use insert here to protect against overwriting the same bone index for different channels
	m_KeyFrames.insert({ inBoneName, KeyFrames() });
	m_KeyFrames[inBoneName].LoadFromGltf(inAnimation);
}


} // raekor
