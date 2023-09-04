#pragma once

#include "widget.h"
#include "json.h"
#include "serial.h"

namespace Raekor {

class NodeGraphWidget : public IWidget
{
public:
	RTTI_DECLARE_TYPE(NodeGraphWidget);

	NodeGraphWidget(Application* inApp);

	virtual void Draw(Widgets* inWidgets, float dt) override;
	virtual void OnEvent(Widgets* inWidgets, const SDL_Event& ev) override;

	int GetSelectedObjectIndex() { return m_SelectedObject; }
	void SelectObject(int inIndex) { m_SelectedObject = inIndex; ImNodes::ClearNodeSelection(); ImNodes::SelectNode(m_SelectedObject); }

private:
	int m_SelectedObject = -1;
	int m_LinkPinDropped = -1;

	bool m_WasRightClicked = false;
	bool m_WasLinkConnected = false;

	int m_StartNodeID, m_StartPinID;
	int m_EndNodeID, m_EndPinID;

	Path m_OpenFilePath;
	JSON::JSONData m_JSON;
	std::unordered_map<const char*, void*> m_Objects;
};

}