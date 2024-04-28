#include "pch.h"
#include "camera.h"
#include "input.h"
#include "rmath.h"
#include "member.h"

namespace Raekor {

RTTI_DEFINE_TYPE(Camera)
{
	RTTI_DEFINE_MEMBER(Camera, SERIALIZE_ALL, "Angle", m_Angle);
	RTTI_DEFINE_MEMBER(Camera, SERIALIZE_ALL, "Position", m_Position);
	RTTI_DEFINE_MEMBER(Camera, SERIALIZE_ALL, "Projection", m_Projection);
}

RTTI_DEFINE_TYPE(CameraSequence::KeyFrame)
{
    RTTI_DEFINE_MEMBER(CameraSequence::KeyFrame, SERIALIZE_ALL, "Time", mTime);
    RTTI_DEFINE_MEMBER(CameraSequence::KeyFrame, SERIALIZE_ALL, "Angle", mAngle);
    RTTI_DEFINE_MEMBER(CameraSequence::KeyFrame, SERIALIZE_ALL, "Position", mPosition);
}

RTTI_DEFINE_TYPE(CameraSequence)
{
    RTTI_DEFINE_MEMBER(CameraSequence, SERIALIZE_ALL, "Duration", m_Duration);
    RTTI_DEFINE_MEMBER(CameraSequence, SERIALIZE_ALL, "Key Frames", m_KeyFrames);
}

Camera::Camera(const Vec3& inPos, const Mat4x4& inProj) :
	m_Position(inPos),
	m_Angle(0.0f, 0.0f)
{
	m_Projection = inProj;
}


void Camera::OnUpdate()
{
	m_PrevView = m_View;
	m_PrevProjection = m_Projection;

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


void Camera::LookAt(Vec3 inPosition)
{
	m_Angle.x = glm::atan(inPosition.x, inPosition.z);
	m_Angle.y = std::clamp(glm::asin(inPosition.y), -1.57078f, 1.57078f);
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


void EditorCameraController::OnUpdate(Camera& inCamera, float inDeltaTime)
{
	if (SDL_GetRelativeMouseMode())
	{
		if (g_Input->IsKeyPressed(SDL_SCANCODE_W))
		{
			inCamera.Zoom(float(inCamera.mZoomConstant * inDeltaTime));
		}
		else if (g_Input->IsKeyPressed(SDL_SCANCODE_S))
		{
			inCamera.Zoom(float(-inCamera.mZoomConstant * inDeltaTime));
		}

		if (g_Input->IsKeyPressed(SDL_SCANCODE_A))
		{
			inCamera.Move({ inCamera.mMoveConstant * inDeltaTime, 0.0f });
		}
		else if (g_Input->IsKeyPressed(SDL_SCANCODE_D))
		{
			inCamera.Move({ -inCamera.mMoveConstant * inDeltaTime, 0.0f });
		}
	}
}


bool EditorCameraController::OnEvent(Camera& inCamera, const SDL_Event& inEvent)
{
	bool camera_changed = false;

	if (inEvent.button.button == 2 || inEvent.button.button == 3)
	{
		if (inEvent.type == SDL_MOUSEBUTTONDOWN)
			g_Input->SetRelativeMouseMode(true);
		else if (inEvent.type == SDL_MOUSEBUTTONUP)
			g_Input->SetRelativeMouseMode(false);
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
		if (SDL_GetRelativeMouseMode() && g_Input->IsButtonPressed(3))
		{
			const float formula = glm::radians(0.022f * inCamera.mSensitivity * 2.0f);
			inCamera.Look(glm::vec2(inEvent.motion.xrel * formula, inEvent.motion.yrel * formula));
			camera_changed = true;
		}
		else if (SDL_GetRelativeMouseMode() && g_Input->IsButtonPressed(2))
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
	m_Camera(Vec3(0, 1.0, 0), glm::perspectiveRH(glm::radians(m_FieldOfView), m_AspectRatio, 0.1f, 4096.0f)),
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
	m_Camera.OnUpdate();

	m_JitterIndex = m_JitterIndex + 1;

	const Vec2 halton = Vec2
	(
		2.0f * GetHaltonSequence(m_JitterIndex + 1, 2) - 1.0f,
		2.0f * GetHaltonSequence(m_JitterIndex + 1, 3) - 1.0f
	);

	m_Jitter = halton / Vec2(size);
}


glm::mat4 Viewport::GetJitteredProjMatrix(const Vec2& inJitter) const
{
	Mat4x4 proj = m_Camera.GetProjection();
	proj[2][0] = m_Jitter[0];
	proj[2][1] = m_Jitter[1];
	return proj;
}


void CameraSequence::AddKeyFrame(const Camera& inCamera, float inTime)
{
    AddKeyFrame(inCamera, inTime, inCamera.GetPosition(), inCamera.GetAngle());
}


void CameraSequence::AddKeyFrame(const Camera& inCamera, float inTime, Vec3 inPosition, Vec2 inAngle)
{
    m_KeyFrames.emplace_back(inTime, inAngle, inPosition);

    std::sort(m_KeyFrames.begin(), m_KeyFrames.end(), [](const KeyFrame& inKey1, const KeyFrame& inKey2) { return inKey1.mTime < inKey2.mTime; });
}


void CameraSequence::RemoveKeyFrame(uint32_t inIndex)
{
    m_KeyFrames.erase(m_KeyFrames.begin() + inIndex);

    std::sort(m_KeyFrames.begin(), m_KeyFrames.end(), [](const KeyFrame& inKey1, const KeyFrame& inKey2) { return inKey1.mTime < inKey2.mTime; });
}


Vec2 CameraSequence::GetAngle(const Camera& inCamera, float inTime) const
{
    const auto nr_of_key_frames = m_KeyFrames.size();
    if (nr_of_key_frames == 0)
    {
        return inCamera.GetAngle();
    }
    else if (nr_of_key_frames == 1)
    {
        return m_KeyFrames[0].mAngle;
    }
    else
    {
        auto start_index = 0u;
        
        for (; start_index < nr_of_key_frames - 1; start_index++)
        {
            if (inTime < m_KeyFrames[start_index + 1].mTime)
                break;
        }

        assert(start_index < nr_of_key_frames);
        auto final_index = start_index + 1;

        if (final_index == nr_of_key_frames)
            return m_KeyFrames[start_index].mAngle;

        auto start_time = m_KeyFrames[start_index].mTime;
        auto final_time = m_KeyFrames[final_index].mTime;
        
        auto delta_time = m_KeyFrames[final_index].mTime - m_KeyFrames[start_index].mTime;
        auto factor = glm::max((inTime - start_time) / delta_time, 0.0f);

        const auto& start_angle = m_KeyFrames[start_index].mAngle;
        const auto& final_angle = m_KeyFrames[final_index].mAngle;

        return start_angle + factor * (final_angle - start_angle);
    }
}


Vec3 CameraSequence::GetPosition(const Camera& inCamera, float inTime) const
{
    const int nr_of_key_frames = m_KeyFrames.size();
    if (nr_of_key_frames == 0)
    {
        return inCamera.GetPosition();
    }
    else if (nr_of_key_frames == 1)
    {
        return m_KeyFrames[0].mPosition;
    }

    int start_index = 0;
    
    for (; start_index < nr_of_key_frames - 1; start_index++)
    {
        if (inTime < m_KeyFrames[start_index + 1].mTime)
            break;
    }

    assert(start_index < nr_of_key_frames);
    const int final_index = start_index + 1;

    if (final_index == nr_of_key_frames)
        return m_KeyFrames[start_index].mPosition;

    const float start_time = m_KeyFrames[start_index].mTime;
	const float final_time = m_KeyFrames[final_index].mTime;

    const Vec3& start_position = m_KeyFrames[start_index].mPosition;
    const Vec3& final_position = m_KeyFrames[final_index].mPosition;

    const float a = (inTime - start_time) / (final_time - start_time);

    //return glm::catmullRom(
    //    m_KeyFrames[glm::clamp(start_index - 1, 0, nr_of_key_frames - 1)].mPosition, 
    //    m_KeyFrames[glm::clamp(start_index + 0, 0, nr_of_key_frames - 1)].mPosition, 
    //    m_KeyFrames[glm::clamp(start_index + 1, 0, nr_of_key_frames - 1)].mPosition, 
    //    m_KeyFrames[glm::clamp(start_index + 2, 0, nr_of_key_frames - 1)].mPosition, 
    //    glm::clamp(a, 0.0f, 1.0f)
    //);
    
    return glm::lerp(start_position, final_position, glm::clamp(a, 0.0f, 1.0f));
}

} // namespace::Raekor
