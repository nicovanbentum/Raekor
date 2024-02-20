#pragma once

#include "rtti.h"

namespace Raekor {

template<typename T>
struct Key
{
	double mTime = 0.0f;
	T mValue = {};

	Key() = default;
	Key(double inTime, const T& inValue) : mTime(inTime), mValue(inValue) {}

	inline bool operator==(const T& inOther) const { return inOther.mValue == mValue; }
	inline bool operator!=(const T& inOther) const { return inOther.mValue != mValue; }

	inline bool operator<(const T& inOther) const { return mTime < inOther.mTime; }
	inline bool operator>(const T& inOther) const { return mTime > inOther.mTime; }
};

struct Vec3Key : public Key<Vec3>
{
	RTTI_DECLARE_TYPE(Vec3Key);
	using Key::Key;
};

struct QuatKey : public Key<Quat>
{
	RTTI_DECLARE_TYPE(QuatKey);
	using Key::Key;
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

private:
	std::vector<Vec3Key> m_ScaleKeys;
	std::vector<Vec3Key> m_PositionKeys;
	std::vector<QuatKey> m_RotationKeys;
};

class Animation
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

	const std::string& GetName() const { return m_Name; }

	uint32_t GetBoneCount() const { return m_KeyFrames.size(); }
	
	float GetTotalDuration() const { return m_TotalDuration; }
	
	auto GetBoneNames() { return std::views::keys(m_KeyFrames); }
	auto GetKeyFrames() { return std::views::values(m_KeyFrames); }

	bool IsPlaying() const { return m_IsPlaying; }
	void SetIsPlaying(bool inPlaying) { m_IsPlaying = inPlaying; }

	float GetRunningTime() const { return m_RunningTime; }
	void  SetRunningTime(float inTime) { m_RunningTime = inTime; }

#ifndef DEPRECATE_ASSIMP
	void LoadKeyframes(uint32_t inBoneIndex, const aiNodeAnim* inAnimation);
#endif
	void LoadKeyframes(const std::string& inBoneName, const cgltf_animation_channel* inAnimation);

	KeyFrames& GetKeyFrames(const std::string& inBoneName) { return m_KeyFrames[inBoneName]; }

private:
	/* Name of the animation. */
	std::string m_Name;
	/* Runtime state of the animation. */
	bool m_IsPlaying = false;
	/* Elapsed time in milliseconds. */
	float m_RunningTime = 0.0f;
	/* Total duration of the animation.*/
	float m_TotalDuration = 0.0f;
	/* Arrays of keyframes mapped to bone/joint indices. */
	std::unordered_map<std::string, KeyFrames> m_KeyFrames;
};

} // raekor
