#pragma once

#include "ECS.h"
#include "RTTI.h"

namespace RK {

class Editor;
class Scene;
class Assets;
class Physics;
class Widgets;
class IRenderInterface;
class Application;

class IWidget
{
	RTTI_DECLARE_VIRTUAL_TYPE(IWidget);

public:
	IWidget(Editor* inEditor, const String& inTitle);
	virtual void Draw(Widgets* inWidgets, float inDeltaTime) = 0;
	virtual void OnEvent(Widgets* inWidgets, const SDL_Event& inEvent) = 0;

	void Show() { m_PrevOpen = m_Open; m_Open = true; }
	void Hide() { m_PrevOpen = m_Open; m_Open = false; }
	void Restore() { m_Open = m_PrevOpen; }

	bool IsOpen() { return m_Open; }
	bool IsVisible() { return m_Visible; }
	bool IsFocused() { return m_Focused; }

	const String& GetTitle() { return m_Title; }

protected:
	// Need to be defined in the cpp to avoid circular dependencies
	Scene& GetScene();
	Assets& GetAssets();
	Physics& GetPhysics();
	IRenderInterface& GetRenderInterface();

	Entity GetActiveEntity();
	void   SetActiveEntity(Entity inEntity);

	Editor* m_Editor;
	String m_Title;
	bool m_Open = true;
	bool m_PrevOpen = true;
	bool m_Visible = false;
	bool m_Focused = false;
};


class Widgets
{
public:

	template<typename T>
	T* Register(Editor* inEditor)
	{
		static_assert( std::is_base_of<IWidget, T>() );
		m_Widgets.emplace_back(std::make_shared<T>(inEditor));
        return std::static_pointer_cast<T>( m_Widgets.back() ).get();
	}

	template<typename T>
	T* GetWidget()
	{
		for (const auto& widget : m_Widgets)
			if (widget->GetRTTI() == RTTI_OF<T>())
				return std::static_pointer_cast<T>( widget ).get();

		return nullptr;
	}

	void Draw(float inDeltaTime);
	void OnEvent(const SDL_Event& inEvent);

	auto begin() { return m_Widgets.begin(); }
	auto end() { return m_Widgets.end(); }

private:
	std::vector<std::shared_ptr<IWidget>> m_Widgets;
};

}
