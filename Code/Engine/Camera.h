#pragma once

#include "cvars.h"

namespace RK {

struct Frustum;

class Camera
{
	RTTI_DECLARE_TYPE(Camera);

public:
	static constexpr Vec3 cUp = Vec3(0.0f, 1.0f, 0.0f);
	static constexpr Vec3 cRight = Vec3(1.0f, 0.0f, 0.0f);
	static constexpr Vec3 cForward = Vec3(0.0f, 0.0f, -1.0f);

	static constexpr float cDefaultNearPlane = 0.1f;
	static constexpr float cDefaultFarPlane = 4096.0f;
	static constexpr float cDefaultFieldOfView = 65.0f;
	static constexpr float cDefaultAspectRatio = 16.0f / 9.0f;

public:
	void Zoom(float inAmount);
	void Look(Vec2 inAmount);
	void Move(Vec2 inAmount);
	void Orbit(Vec2 inAmount, Vec3 inTarget, float inRadius);


	Vec3 GetForwardVector() const;

	void LookAt(Vec3 inPosition);

	void SetFov(float inValue) { m_FieldOfView = inValue; }
	void SetFar(float inValue) { m_FarPlane = inValue; }
	void SetNear(float inValue) { m_NearPlane = inValue; }
	void SetAspectRatio(float inValue) { m_AspectRatio = inValue; }

	float GetFov() const { return m_FieldOfView; }
	float GetFar() const { return m_FarPlane; }
	float GetNear() const { return m_NearPlane; }
	float GetAspectRatio() const { return m_AspectRatio; }

	const Vec2& GetAngle() const { return m_Angle; }
	void SetAngle(const Vec2& inAngle) { m_Angle = inAngle; }
	
	const Vec3& GetPosition() const { return m_Position; }
	void SetPosition(const Vec3& inPosition) { m_Position = inPosition; }

	float GetZoomSpeed() const { return mZoomSpeed; }
	float GetMoveSpeed() const { return mMoveSpeed; }

private:
	Vec2 m_Angle = Vec2(0.0f);
	Vec3 m_Position = Vec3(0.0f);
	Vec2 m_PrevAngle = Vec2(0.0f);
	Vec3 m_PrevPosition = Vec3(0.0f);

	float m_FarPlane = cDefaultFarPlane;
	float m_NearPlane = cDefaultNearPlane;
	float m_FieldOfView = cDefaultFieldOfView;
	float m_AspectRatio = cDefaultAspectRatio;

public:
	float mZoomSpeed = 1.0f, mMoveSpeed = 1.0f, mOrbitSpeed = 0.1f;
	float mLookConstant = 1.0f, mZoomConstant = 10.0f, mMoveConstant = 10.0f;
};



class EditorCameraController
{
public:
	/* Returns true if the camera moved. */
	static void OnUpdate(Camera& inCamera, float inDeltaTime);
	/* Returns true if the camera moved. */
	static bool OnEvent(Camera& inCamera, const SDL_Event& inEvent);
};



class Viewport
{
public:
	bool Moved() const;
	bool Changed() const;

	float GetFar() const;
	float GetNear() const;
	Frustum GetFrustum() const;
	float GetFieldOfView() const;
	float GetAspectRatio() const;

	void OnUpdate(const Camera& inCamera);

	Vec3 GetPosition() const { return m_InvView[3]; }

	const Mat4x4& GetView() const { return m_View; }
	const Mat4x4& GetProjection() const { return m_Projection; }

	inline const UVec2& GetRenderSize() const { return size; }
	void SetRenderSize(const UVec2& inSize) { size = inSize; }

	inline const UVec2 GetDisplaySize() const { return m_DisplaySize; }
	void SetDisplaySize(const UVec2& inSize) { m_DisplaySize = inSize; }

public:
	// public out of convenience
	UVec2 size = UVec2(1u, 1u); // m_RenderSize

private:
	UVec2 m_DisplaySize = UVec2(1u, 1u);
		
	Mat4x4 m_View;
	Mat4x4 m_InvView;
	Mat4x4 m_PrevView;
	Mat4x4 m_Projection;
	Mat4x4 m_InvProjection;
	Mat4x4 m_PrevProjection;
};

} // Namespace Raekor