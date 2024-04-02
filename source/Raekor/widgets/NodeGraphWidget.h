#pragma once

#include "widget.h"
#include "json.h"
#include "iter.h"
#include "shader.h"
#include "serial.h"
#include "defines.h"
#include "components.h"

namespace Raekor {

class NodeGraphWidget : public IWidget
{
public:
	RTTI_DECLARE_VIRTUAL_TYPE(NodeGraphWidget);

	NodeGraphWidget(Application* inApp);

	virtual void Draw(Widgets* inWidgets, float dt) override;
	virtual void OnEvent(Widgets* inWidgets, const SDL_Event& ev) override;

private:
	bool m_WasRightClicked = false;
	bool m_WasLinkConnected = false;

	int m_StartNodeID, m_StartPinID;
	int m_EndNodeID = -1, m_EndPinID = -1;
	
	Path m_OpenFilePath;
	Path m_OpenTemplateFilePath;
	ShaderGraphBuilder m_Builder;

};

}