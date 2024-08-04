#pragma once

#include "ecs.h"
#include "rtti.h"

namespace RK {

template<typename T>
struct TKey
{
	double mTime = 0.0f;
	T mValue = {};

	TKey() = default;
	TKey(double inTime, const T& inValue) : mTime(inTime), mValue(inValue) {}

	inline bool operator==(const T& inOther) const { return inOther.mValue == mValue; }
	inline bool operator!=(const T& inOther) const { return inOther.mValue != mValue; }

	inline bool operator<(const T& inOther) const { return mTime < inOther.mTime; }
	inline bool operator>(const T& inOther) const { return mTime > inOther.mTime; }
};

struct Vec3Key : public TKey<Vec3>
{
	RTTI_DECLARE_TYPE(Vec3Key);
	using TKey::TKey;
};

struct QuatKey : public TKey<Quat>
{
	RTTI_DECLARE_TYPE(QuatKey);
	using TKey::TKey;
};

class KeyFrames
{
	RTTI_DECLARE_TYPE(KeyFrames);

	friend class GltfImporter;
	friend class AssimpImporter;

#ifndef DEPRECATE_ASSIMP
	void LoadFromAssimp(const aiNodeAnim* inNodeAnim);
#endif
	void LoadFromGltf(const cgltf_animation_channel* inNodeAnim);

	void AddScaleKey(const Vec3Key& inKey) { m_ScaleKeys.push_back(inKey); }
	void AddPositionKey(const Vec3Key& inKey) { m_PositionKeys.push_back(inKey); }
	void AddRotationKey(const QuatKey& inKey) { m_RotationKeys.push_back(inKey); }

	Vec3 GetInterpolatedScale(float animationTime) const;
	Quat GetInterpolatedRotation(float animationTime) const;
	Vec3 GetInterpolatedPosition(float animationTime) const;

	bool CanInterpolateScale() const { return m_ScaleKeys.size() > 1; }
	bool CanInterpolatePosition() const { return m_PositionKeys.size() > 1; }
	bool CanInterpolateRotation() const { return m_RotationKeys.size() > 1; }

private:
	Array<Vec3Key> m_ScaleKeys;
	Array<Vec3Key> m_PositionKeys;
	Array<QuatKey> m_RotationKeys;
};

class Animation : public Component
{
	RTTI_DECLARE_TYPE(Animation);

	friend struct Skeleton;
	friend class GltfImporter;
	friend class AssimpImporter;

public:
	explicit Animation() = default;
#ifndef DEPRECATE_ASSIMP
	Animation(const aiAnimation* inAnimation);
#endif
	Animation(const cgltf_animation* inAnimation);

	void OnUpdate(float inDeltaTime);

	const String& GetName() const { return m_Name; }
	uint32_t GetBoneCount() const { return m_KeyFrames.size(); }
	float GetTotalDuration() const { return m_TotalDuration; }
	
	auto GetBoneNames() const { return std::views::keys(m_KeyFrames); }
	auto GetKeyFrames() const { return std::views::values(m_KeyFrames); }

	bool IsPlaying() const { return m_IsPlaying; }
	void SetIsPlaying(bool inPlaying) { m_IsPlaying = inPlaying; }

	float GetRunningTime() const { return m_RunningTime; }
	void  SetRunningTime(float inTime) { m_RunningTime = inTime; }

#ifndef DEPRECATE_ASSIMP
	void LoadKeyframes(uint32_t inBoneIndex, const aiNodeAnim* inAnimation);
#endif
	void LoadKeyframes(const std::string& inBoneName, const cgltf_animation_channel* inAnimation);
	bool HasKeyFrames(const String& inBoneName) const { return m_KeyFrames.contains(inBoneName); }


	KeyFrames& GetKeyFrames(const String& inBoneName) { return m_KeyFrames.at(inBoneName); }
	const KeyFrames& GetKeyFrames(const String& inBoneName) const { return m_KeyFrames.at(inBoneName); }

private:
	/* Name of the animation. */
	String m_Name;
	/* Runtime state of the animation. */
	bool m_IsPlaying = false;
	/* Elapsed time in milliseconds. */
	float m_RunningTime = 0.0f;
	/* Total duration of the animation.*/
	float m_TotalDuration = 0.0f;
	/* Arrays of keyframes mapped to bone/joint indices. */
	std::unordered_map<String, KeyFrames> m_KeyFrames;
};

} // raekor
