#include "pch.h"
#include "script.h"
#include "input.h"
#include "scene.h"
#include "assets.h"
#include "application.h"

// this once contained chaiscript, but in the future I'll mainly support native code or integrate Mono for C#

namespace Raekor {

RTTI_DEFINE_TYPE_NO_FACTORY(INativeScript) {}

Scene* INativeScript::GetScene() { return m_App->GetScene(); }

Assets* INativeScript::GetAssets() { return m_App->GetAssets(); }

Physics* INativeScript::GetPhysics() { return m_App->GetPhysics(); }

IRenderInterface* INativeScript::GetRenderInterface() { return m_App->GetRenderInterface(); }

void INativeScript::Log(const std::string& inText) { m_App->LogMessage(inText.data()); }

} // raekor
