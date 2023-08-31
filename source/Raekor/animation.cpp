#include "pch.h"
#include "animation.h"
#include "util.h"

namespace Raekor {

#ifndef DEPRECATE_ASSIMP

void KeyFrames::LoadFromAssimp(const aiNodeAnim* nodeAnim) {
	for (const auto& key : Slice(nodeAnim->mScalingKeys, nodeAnim->mNumScalingKeys))
		m_ScaleKeys.push_back(Vec3Key(key.mTime, Vec3(key.mValue.x, key.mValue.y, key.mValue.z)));

	for (const auto& key : Slice(nodeAnim->mPositionKeys, nodeAnim->mNumPositionKeys))
		m_PositionKeys.push_back(Vec3Key(key.mTime, Vec3(key.mValue.x, key.mValue.y, key.mValue.z)));

	for (const auto& key : Slice(nodeAnim->mRotationKeys, nodeAnim->mNumRotationKeys))
		m_RotationKeys.push_back(QuatKey(key.mTime, Quat(key.mValue.w, key.mValue.x, key.mValue.y, key.mValue.z)));
}

#endif

void KeyFrames::LoadFromGltf(const cgltf_animation* nodeAnim) {

}


Vec3 KeyFrames::GetInterpolatedPosition(float animationTime) const {
	if (m_PositionKeys.size() == 1) {
		// No interpolation necessary for single value
		auto v = m_PositionKeys[0].mValue;
		return { v.x, v.y, v.z };
	}

	auto pos_index = 0u;
	for (auto i = 0u; i < m_PositionKeys.size() - 1; i++) {
		if (animationTime < (float)m_PositionKeys[i + 1].mTime) {
			pos_index = i;
			break;
		}
	}

	auto NextPositionIndex = (pos_index + 1);
	auto delta_time = (float)(m_PositionKeys[NextPositionIndex].mTime - m_PositionKeys[pos_index].mTime);
	
	auto factor = (animationTime - (float)m_PositionKeys[pos_index].mTime) / delta_time;
	if (factor < 0.0f) 
		factor = 0.0f;

	const auto& start = m_PositionKeys[pos_index].mValue;
	const auto& end = m_PositionKeys[NextPositionIndex].mValue;
	auto delta = end - start;
	auto ai_vec = start + factor * delta;

	return { ai_vec.x, ai_vec.y, ai_vec.z };
}
	

Quat KeyFrames::GetInterpolatedRotation(float animationTime) const {
	if (m_RotationKeys.size() == 1) {
		// No interpolation necessary for single value
		auto v = m_RotationKeys[0].mValue;
		return glm::quat(v.w, v.x, v.y, v.z);
	}

	auto rotation_index = 0u;
	for (auto i = 0u; i < m_RotationKeys.size() - 1; i++) {
		if (animationTime < (float)m_RotationKeys[i + 1].mTime) {
			rotation_index = i;
			break;
		}
	}

	auto next_rotation_index = (rotation_index + 1);

	auto delta_time = (float)(m_RotationKeys[next_rotation_index].mTime - m_RotationKeys[rotation_index].mTime);
	auto factor = (animationTime - (float)m_RotationKeys[rotation_index].mTime) / delta_time;
	if (factor < 0.0f) 
		factor = 0.0f;

	const auto& start_rotation_quat = m_RotationKeys[rotation_index].mValue;
	const auto& end_rotation_quat = m_RotationKeys[next_rotation_index].mValue;

	return glm::normalize(glm::lerp(start_rotation_quat, end_rotation_quat, factor));
}


Vec3 KeyFrames::GetInterpolatedScale(float animationTime) const {
	if (m_ScaleKeys.size() == 1) {
		// No interpolation necessary for single value
		auto v = m_ScaleKeys[0].mValue;
		return { v.x, v.y, v.z };
	}

	auto index = 0u;
	for (auto i = 0u; i < m_ScaleKeys.size() - 1; i++) {
		if (animationTime < (float)m_ScaleKeys[i + 1].mTime) {
			index = i;
			break;
		}
	}

	auto next_index = (index + 1);
	auto delta_time = (float)(m_ScaleKeys[next_index].mTime - m_ScaleKeys[index].mTime);
	
	auto factor = (animationTime - (float)m_ScaleKeys[index].mTime) / delta_time;
	if (factor < 0.0f) 
		factor = 0.0f;

	const auto& start = m_ScaleKeys[index].mValue;
	const auto& end = m_ScaleKeys[next_index].mValue;
	auto delta = end - start;
	auto aiVec = start + factor * delta;

	return { aiVec.x, aiVec.y, aiVec.z };
}

#ifndef DEPRECATE_ASSIMP

Animation::Animation(const aiAnimation* inAnimation) {
	m_Name = inAnimation->mName.C_Str();
	m_RunningTime = 0;
	m_TotalDuration = float(inAnimation->mDuration);
}

#endif

Animation::Animation(const cgltf_animation* inAnimation) {
	m_Name = inAnimation->name;
	m_RunningTime = 0;
	m_TotalDuration = 0;
}

#ifndef DEPRECATE_ASSIMP

void Animation::LoadKeyframes(uint32_t inBoneIndex, const aiNodeAnim* inAnimation) {
	m_BoneAnimations[inBoneIndex] = KeyFrames(inBoneIndex);
	m_BoneAnimations[inBoneIndex].LoadFromAssimp(inAnimation);
}

#endif

void Animation::LoadKeyframes(uint32_t inBoneIndex, const cgltf_animation* inAnimation) {
	m_BoneAnimations[inBoneIndex] = KeyFrames(inBoneIndex);
	m_BoneAnimations[inBoneIndex].LoadFromGltf(inAnimation);
}


} // raekor
