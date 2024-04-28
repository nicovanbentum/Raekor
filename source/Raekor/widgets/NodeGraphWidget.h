#pragma once

#include "widget.h"
#include "json.h"
#include "iter.h"
#include "shader.h"
#include "serial.h"
#include "defines.h"
#include "components.h"

namespace Raekor {

class ShaderGraphWidget : public IWidget
{
public:
	RTTI_DECLARE_VIRTUAL_TYPE(ShaderGraphWidget);

	struct EditingContext
	{
		Array<int> SelectedNodes;
		Array<int> SelectedLinks;
		Pair<bool, int> HoveredNode = std::make_pair(false, -1);
		Pair<bool, int> hoveredLink = std::make_pair(false, -1);
	};

	ShaderGraphWidget(Application* inApp);

	virtual void Draw(Widgets* inWidgets, float dt) override;
	virtual void OnEvent(Widgets* inWidgets, const SDL_Event& ev) override;

	EditingContext& GetContext() { return m_Contexts[m_ActiveGraph]; }
	ShaderGraphBuilder& GetBuilder() { return m_Builders[m_ActiveGraph]; }
	ImNodesEditorContext& GetImNodesContext() { return m_ImNodesContexts[m_ActiveGraph]; }

private:
	String m_GeneratedCode;
	bool m_ShowGeneratedCode = false;

	ImVec2 m_ClickedMousePos;
	int m_StartNodeID, m_StartPinID;
	int m_EndNodeID = -1, m_EndPinID = -1;
	
	Path m_OpenFilePath;
	Path m_OpenTemplateFilePath;
	Path m_OpenGeneratedFilePath;

	EShaderGraph m_ActiveGraph = SHADER_GRAPH_VERTEX;
	StaticArray<EditingContext, SHADER_GRAPH_COUNT> m_Contexts;
	StaticArray<ShaderGraphBuilder, SHADER_GRAPH_COUNT> m_Builders;
	StaticArray<ImNodesEditorContext, SHADER_GRAPH_COUNT> m_ImNodesContexts;
};

}