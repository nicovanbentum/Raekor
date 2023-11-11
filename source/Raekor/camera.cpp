#include "pch.h"
#include "camera.h"
#include "input.h"
#include "rmath.h"

namespace Raekor {

Camera::Camera(const Vec3& inPos, const Mat4x4& inProj) :
	m_Position(inPos),
	m_Angle(0.0f, 0.0f)
{
	m_Projection = inProj;
}


void Camera::OnUpdate(float inDeltaTime)
{
	m_PrevView = m_View;
	m_PrevProjection = m_Projection;

	if (SDL_GetRelativeMouseMode())
	{
		if (Input::sIsKeyPressed(SDL_SCANCODE_W))
		{
			Zoom(float(mZoomConstant * inDeltaTime));
		}
		else if (Input::sIsKeyPressed(SDL_SCANCODE_S))
		{
			Zoom(float(-mZoomConstant * inDeltaTime));
		}

		if (Input::sIsKeyPressed(SDL_SCANCODE_A))
		{
			Move({ mMoveConstant * inDeltaTime, 0.0f });
		}
		else if (Input::sIsKeyPressed(SDL_SCANCODE_D))
		{
			Move({ -mMoveConstant * inDeltaTime, 0.0f });
		}
	}

	auto dir = GetForwardVector();
	m_View = glm::lookAtRH(m_Position, m_Position + dir, { 0, 1, 0 });
}



void Camera::Zoom(float amount)
{
	auto dir = GetForwardVector();
	m_Position += dir * ( amount * mZoomSpeed );
}


void Camera::Look(Vec2 amount)
{
	m_Angle.x += float(amount.x * -1);
	m_Angle.y += float(amount.y * -1);
	// clamp to roughly half pi so we dont overshoot
	m_Angle.y = std::clamp(m_Angle.y, -1.57078f, 1.57078f);
}


void Camera::Move(Vec2 amount)
{
	auto dir = GetForwardVector();
	// sideways
	m_Position += glm::normalize(glm::cross(dir, { 0.0f, 1.0f, 0.0f })) * ( amount.x * -mMoveSpeed );
	// up and down
	m_Position.y += float(amount.y * mMoveSpeed);
}


bool Camera::Moved() const
{
	for (uint32_t x = 0; x < 4; x++)
		for (uint32_t y = 0; y < 4; y++)
			if (glm::abs(m_View[x][y] - m_PrevView[x][y]) > 0.000001)
				return true;

	return false;
}


bool Camera::Changed() const
{
	for (uint32_t x = 0; x < 4; x++)
		for (uint32_t y = 0; y < 4; y++)
			if (glm::abs(m_Projection[x][y] - m_PrevProjection[x][y]) > 0.000001)
				return true;

	return Moved();
}


Vec3 Camera::GetForwardVector()
{
	return glm::normalize(Vec3(cos(m_Angle.y) * sin(m_Angle.x),
		sin(m_Angle.y), cos(m_Angle.y) * cos(m_Angle.x)));
}


float Camera::GetFov() const
{
	return 2.0f * atan(1.0f / m_Projection[1][1]) * 180.0f / (float)M_PI;
}


float Camera::GetFar() const
{
	return m_Projection[3][2] / ( m_Projection[2][2] + 1.0f );
}


float Camera::GetNear() const
{
	return m_Projection[3][2] / ( m_Projection[2][2] - 1.0f );
}


float Camera::GetAspectRatio() const
{
	return m_Projection[1][1] / m_Projection[0][0];
}


Frustum Camera::GetFrustum() const
{
	return Frustum(m_Projection * m_View, false);
}


bool CameraController::OnEvent(Camera& inCamera, const SDL_Event& inEvent)
{
	bool camera_changed = false;

	if (inEvent.button.button == 2 || inEvent.button.button == 3)
	{
		if (inEvent.type == SDL_MOUSEBUTTONDOWN)
			SDL_SetRelativeMouseMode(SDL_TRUE);
		else if (inEvent.type == SDL_MOUSEBUTTONUP)
			SDL_SetRelativeMouseMode(SDL_FALSE);
	}

	if (inEvent.type == SDL_KEYDOWN && !inEvent.key.repeat && inEvent.key.keysym.sym == SDLK_LSHIFT)
	{
		inCamera.mZoomConstant *= 20.0f;
		inCamera.mMoveConstant *= 20.0f;
	}

	if (inEvent.type == SDL_KEYUP && !inEvent.key.repeat && inEvent.key.keysym.sym == SDLK_LSHIFT)
	{
		inCamera.mZoomConstant /= 20.0f;
		inCamera.mMoveConstant /= 20.0f;
	}

	if (inEvent.type == SDL_MOUSEMOTION)
	{
		if (SDL_GetRelativeMouseMode() && Input::sIsButtonPressed(3))
		{
			auto formula = glm::radians(0.022f * inCamera.mSensitivity * 2.0f);
			inCamera.Look(glm::vec2(inEvent.motion.xrel * formula, inEvent.motion.yrel * formula));
			camera_changed = true;
		}
		else if (SDL_GetRelativeMouseMode() && Input::sIsButtonPressed(2))
		{
			inCamera.Move(glm::vec2(inEvent.motion.xrel * 0.02f, inEvent.motion.yrel * 0.02f));
			camera_changed = true;
		}
	}
	else if (inEvent.type == SDL_MOUSEWHEEL)
	{
		inCamera.Zoom(float(inEvent.wheel.y));
		camera_changed = true;
	}

	return camera_changed;
}


Viewport::Viewport(Vec2 inSize) :
	m_Camera(Vec3(0, 0.0, 0), glm::perspectiveRH(glm::radians(m_FieldOfView), m_AspectRatio, 0.1f, 1000.0f)),
	size(inSize),
	m_DisplaySize(inSize)
{
}


void Viewport::UpdateProjectionMatrix()
{
	m_Camera.GetProjection() = glm::perspectiveRH(
		glm::radians(m_FieldOfView),
		float(size.x) / float(size.y),
		m_Camera.GetNear(),
		m_Camera.GetFar()
	);
}


static float GetHaltonSequence(uint32_t i, uint32_t b)
{
	float f = 1.0f;
	float r = 0.0f;

	while (i > 0)
	{
		f /= float(b);
		r = r + f * float(i % b);
		i = uint32_t(floorf(float(i) / float(b)));
	}

	return r;
}


void Viewport::OnUpdate(float dt)
{
	m_Camera.OnUpdate(dt);

	m_JitterIndex = m_JitterIndex + 1;

	const auto halton = Vec2(
		2.0f * GetHaltonSequence(m_JitterIndex + 1, 2) - 1.0f,
		2.0f * GetHaltonSequence(m_JitterIndex + 1, 3) - 1.0f
	);

	m_Jitter = halton / Vec2(size);
}


glm::mat4 Viewport::GetJitteredProjMatrix(const Vec2& inJitter) const
{
	auto mat = m_Camera.GetProjection();
	mat[2][0] = m_Jitter[0];
	mat[2][1] = m_Jitter[1];
	return mat;
}

} // namespace::Raekor
