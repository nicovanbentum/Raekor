#pragma once

#include "cvars.h"

namespace Raekor {

struct Frustum;

class Camera
{

public:
	Camera(const Vec3& inPosition, const Mat4x4& inProjMatrix);
	Camera(const Camera&) : Camera(m_Position, m_Projection) {}
	Camera& operator=(const Camera& rhs) { return *this; }

	void OnUpdate(float inDeltaTime);

	void Zoom(float inAmount);
	void Look(Vec2 inAmount);
	void Move(Vec2 inAmount);

	bool Moved() const;
	bool Changed() const;

	Vec3 GetForwardVector();

	void LookAt(Vec3 inPosition);

	const Vec2& GetAngle() const { return m_Angle; }
	void SetAngle(const Vec2& inAngle) { m_Angle = inAngle; }
	
	const Vec3& GetPosition() const { return m_Position; }
	void SetPosition(const Vec3& inPosition) { m_Position = inPosition; }

	Mat4x4& GetView() { return m_View; }
	const Mat4x4& GetView() const { return m_View; }

	Mat4x4& GetProjection() { return m_Projection; }
	const Mat4x4& GetProjection() const { return m_Projection; }

	float GetFov() const;
	float GetFar() const;
	float GetNear() const;
	float GetAspectRatio() const;

	Frustum GetFrustum() const;

private:
	Vec2 m_Angle;
	Vec2 m_PrevAngle;
	Vec3 m_Position;
	Vec3 m_PrevPosition;

	Mat4x4 m_View;
	Mat4x4 m_PrevView;
	Mat4x4 m_Projection;
	Mat4x4 m_PrevProjection;

public:
	float& mSensitivity = g_CVars.Create("sensitivity", 2.0f);
	float mZoomSpeed = 1.0f, mMoveSpeed = 1.0f;
	float mLookConstant = 1.0f, mZoomConstant = 10.0f, mMoveConstant = 10.0f;
};



class CameraController
{
public:
	/* Returns true if the camera moved. */
	static bool OnEvent(Camera& inCamera, const SDL_Event& inEvent);
};



class Viewport
{
public:
	Viewport(glm::vec2 inSize = glm::vec2(0, 0));

	void OnUpdate(float inDeltaTime);

	Camera& GetCamera() { return m_Camera; }
	const Camera& GetCamera() const { return m_Camera; }

	float GetFieldOfView() const { return m_FieldOfView; }
	void SetFieldOfView(float inFieldOfView) { m_FieldOfView = inFieldOfView; UpdateProjectionMatrix(); }

	float GetAspectRatio() const { return m_AspectRatio; }
	void SetAspectRatio(float inAspectRatio) { m_AspectRatio = inAspectRatio; UpdateProjectionMatrix(); }

	Vec2 GetJitter() const { return m_Jitter; }
	void SetJitter(const Vec2& inJitter) { m_Jitter = inJitter; }

	inline const UVec2& GetRenderSize() const { return size; }
	void SetRenderSize(const UVec2& inSize) { size = inSize; UpdateProjectionMatrix(); m_JitterIndex = 0; }

	inline const UVec2 GetDisplaySize() const { return m_DisplaySize; }
	void SetDisplaySize(const UVec2& inSize) { m_DisplaySize = inSize; }

	Mat4x4 GetJitteredProjMatrix(const Vec2& inJitter) const;
	Mat4x4 GetJitteredProjMatrix() const { return GetJitteredProjMatrix(GetJitter()); }

public:
	// These two are public out of convenience
	UVec2 size = UVec2(0u);
	UVec2 offset = UVec2(0u);

private:
	void UpdateProjectionMatrix();

	float m_FieldOfView = 65.0f;
	float m_AspectRatio = 16.0f / 9.0f;

	UVec2 m_DisplaySize = UVec2(0u, 0u);

	uint32_t m_JitterIndex = 0;
	Vec2 m_Jitter = Vec2(0.0f);

	Camera m_Camera;
};


class CameraSequence
{
public:
    RTTI_DECLARE_TYPE(CameraSequence);

    struct KeyFrame
    {
        RTTI_DECLARE_TYPE(CameraSequence::KeyFrame);

        KeyFrame() = default;
        KeyFrame(float inTime, Vec2 inAngle, Vec3 inPos) : mTime(inTime), mAngle(inAngle), mPosition(inPos) {}

        float mTime = 0.0f;
        Vec2 mAngle = Vec2(0.0f, 0.0f);
        Vec3 mPosition = Vec3(0.0f, 0.0f, 0.0f);
    };

    float GetDuration() const { return m_Duration; }
    void  SetDuration(float inValue) { m_Duration = inValue; }

    void AddKeyFrame(const Camera& inCamera, float inTime);
    void AddKeyFrame(const Camera& inCamera, float inTime, Vec3 inPos, Vec2 inAngle);
    void RemoveKeyFrame(uint32_t inIndex);

    uint32_t GetKeyFrameCount() const { return m_KeyFrames.size(); }
    KeyFrame& GetKeyFrame(uint32_t inIndex) { return m_KeyFrames[inIndex]; }
    const KeyFrame& GetKeyFrame(uint32_t inIndex) const { return m_KeyFrames[inIndex]; }

    Vec2 GetAngle(const Camera& inCamera, float inTime) const;
    Vec3 GetPosition(const Camera& inCamera, float inTime) const;

private:
    float m_Duration = 15.0f; // defaul to 15 seconds
    std::vector<KeyFrame> m_KeyFrames;
};

} // Namespace Raekor