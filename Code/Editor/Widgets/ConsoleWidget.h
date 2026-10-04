#pragma once

#include "Widget.h"

namespace RK {

class Editor;

class ConsoleWidget : public IWidget
{
public:
	RTTI_DECLARE_VIRTUAL_TYPE(ConsoleWidget);

	static constexpr size_t cMaxItems = 8192;

	ConsoleWidget(Editor* inEditor);
	~ConsoleWidget();

	virtual void Draw(Widgets* inWidgets, float inDeltaTime) override;
	virtual void OnEvent(Widgets* inWidgets, const SDL_Event& inEvent) override {}

private:
	void ExecuteCommand(const String& inCommand);

	static int sEditCallback(ImGuiInputTextCallbackData* data);

public:
	int m_ActiveItem = 0;
	bool m_ShouldScrollToBottom = false;

	String m_InputBuffer = "";
	Array<String> m_CommandHistory;

	ImGuiTextFilter m_Filter;
	StaticArray<bool, LOG_LEVEL_COUNT> m_ShowLevel = { true, true, true, true };

	uint32_t m_LogSink = 0;
	std::mutex m_ItemsMutex;
	Array<LogMessage> m_Items;
};

} // raekor