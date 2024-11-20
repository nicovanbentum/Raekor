#include "pch.h"
#include "Camera.h"
#include "Member.h"
#include "Input.h"
#include "Maths.h"

namespace RK {

RTTI_DEFINE_TYPE(Camera)
{
	RTTI_DEFINE_MEMBER(Camera, SERIALIZE_ALL, "Angle", m_Angle);
	RTTI_DEFINE_MEMBER(Camera, SERIALIZE_ALL, "Position", m_Position);
	RTTI_DEFINE_MEMBER(Camera, SERIALIZE_ALL, "Far Plane", m_FarPlane);
	RTTI_DEFINE_MEMBER(Camera, SERIALIZE_ALL, "Near Plane", m_NearPlane);
	RTTI_DEFINE_MEMBER(Camera, SERIALIZE_ALL, "Aspect Ratio", m_AspectRatio);
	RTTI_DEFINE_MEMBER(Camera, SERIALIZE_ALL, "Field of View", m_FieldOfView);
}


void Camera::Zoom(float amount)
{
	Vec3 dir = GetForwardVector();
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
	Vec3 dir = GetForwardVector();
	// sideways
	m_Position += glm::normalize(glm::cross(dir, { 0.0f, 1.0f, 0.0f })) * ( amount.x * -mMoveSpeed );
	// up and down
	m_Position.y += float(amount.y * mMoveSpeed);
}


bool Viewport::Moved() const
{
	for (uint32_t x = 0; x < 4; x++)
		for (uint32_t y = 0; y < 4; y++)
			if (glm::abs(m_View[x][y] - m_PrevView[x][y]) > 0.000001)
				return true;

	return false;
}


bool Viewport::Changed() const
{
	for (uint32_t x = 0; x < 4; x++)
		for (uint32_t y = 0; y < 4; y++)
			if (glm::abs(m_Projection[x][y] - m_PrevProjection[x][y]) > 0.000001)
				return true;

	return Moved();
}


Vec3 Camera::GetForwardVector() const
{
	return glm::normalize(Vec3(cos(m_Angle.y) * sin(m_Angle.x),
		sin(m_Angle.y), cos(m_Angle.y) * cos(m_Angle.x)));
}


void Camera::LookAt(Vec3 inPosition)
{
	m_Angle.x = glm::atan(inPosition.x, inPosition.z);
	m_Angle.y = std::clamp(glm::asin(inPosition.y), -1.57078f, 1.57078f);
}


float Viewport::GetFieldOfView() const
{
	return 2.0f * atan(1.0f / m_Projection[1][1]) * 180.0f / (float)M_PI;
}


float Viewport::GetFar() const
{
	return m_Projection[3][2] / ( m_Projection[2][2] + 1.0f );
}


float Viewport::GetNear() const
{
	return m_Projection[3][2] / ( m_Projection[2][2] - 1.0f );
}


float Viewport::GetAspectRatio() const
{
	return m_Projection[1][1] / m_Projection[0][0];
}


Frustum Viewport::GetFrustum() const
{
	return Frustum(m_Projection * m_View, false);
}


void EditorCameraController::OnUpdate(Camera& inCamera, float inDeltaTime)
{
	if (g_Input->IsRelativeMouseMode())
	{
		if (g_Input->IsKeyDown(Key::W))
		{
			inCamera.Zoom(float(inCamera.mZoomConstant * inDeltaTime));
		}
		else if (g_Input->IsKeyDown(Key::S))
		{
			inCamera.Zoom(float(-inCamera.mZoomConstant * inDeltaTime));
		}

		if (g_Input->IsKeyDown(Key::A))
		{
			inCamera.Move({ inCamera.mMoveConstant * inDeltaTime, 0.0f });
		}
		else if (g_Input->IsKeyDown(Key::D))
		{
			inCamera.Move({ -inCamera.mMoveConstant * inDeltaTime, 0.0f });
		}
	}

	if (g_Input->HasController())
	{
		SDL_GameController* controller = g_Input->GetController();

		const Sint16 x_axis = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX);
		const Sint16 y_axis = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);

		float sensitivity = g_CVariables->GetValue<float>("sensitivity");

		float move_x = ( x_axis / 32767.0f ) * sensitivity;
		float move_z = ( y_axis / 32767.0f ) * sensitivity;

		const Sint16 lt_axis = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT);
		const Sint16 rt_axis = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);

		move_x = move_x * glm::lerp(1.0f, 12.3333f, lt_axis / 32767.0f);
		move_z = move_z * glm::lerp(1.0f, 12.3333f, lt_axis / 32767.0f);

		move_x = move_x * glm::lerp(1.0f / 12.3333f, 1.0f, 1.0f - (rt_axis / 32767.0f));
		move_z = move_z * glm::lerp(1.0f / 12.3333f, 1.0f, 1.0f - (rt_axis / 32767.0f));

		const Sint16 right_x = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTX);
		const Sint16 right_y = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTY);

		const float delta_yaw = ( right_x / 32767.0f ) * sensitivity;
		const float delta_pitch = ( right_y / 32767.0f ) * sensitivity;

		inCamera.Look(Vec2(delta_yaw * inDeltaTime, delta_pitch * inDeltaTime));

		float move_y = 0.0f;

		if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_LEFTSHOULDER))
			move_y = -sensitivity;

		if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER))
			move_y = sensitivity;

		inCamera.Move(Vec2(-move_x * inDeltaTime, move_y * inDeltaTime));
		inCamera.Zoom(-move_z * inDeltaTime);
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
		if (g_Input->IsRelativeMouseMode() && g_Input->IsButtonDown(3))
		{
			const CVar& sens_cvar = g_CVariables->GetCVar("sensitivity");
			const float formula = glm::radians(0.022f * sens_cvar.mFloatValue * 2.0f);
			
			inCamera.Look(glm::vec2(inEvent.motion.xrel * formula, inEvent.motion.yrel * formula));
			
			camera_changed = true;
		}
		else if (g_Input->IsRelativeMouseMode() && g_Input->IsButtonDown(2))
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


void Viewport::OnUpdate(const Camera& inCamera)
{
	m_PrevView = m_View;
	m_PrevProjection = m_Projection;
	
	m_View = glm::lookAtRH(
		inCamera.GetPosition(), 
		inCamera.GetPosition() + inCamera.GetForwardVector(), 
		{0, 1, 0}
	);

	m_Projection = glm::perspectiveRH(
		glm::radians(inCamera.GetFov()),
		inCamera.GetAspectRatio(),
		inCamera.GetNear(),
		inCamera.GetFar()
	);

	m_InvView = glm::inverse(m_View);
	m_InvProjection = glm::inverse(m_Projection);
}

} // namespace::Raekor
