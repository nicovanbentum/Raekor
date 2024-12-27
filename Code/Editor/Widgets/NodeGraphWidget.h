#pragma once

#include "Widget.h"
#include "ShaderGraph.h"

namespace RK {

class Editor;

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

	ShaderGraphWidget(Editor* inEditor);

	virtual void Draw(Widgets* inWidgets, float dt) override;
	virtual void OnEvent(Widgets* inWidgets, const SDL_Event& ev) override;

	void SetActiveShaderGraph(EShaderGraph inShaderGraph);

private:
	String m_GeneratedCode;
	bool m_ShowGeneratedCode = false;

	int m_DroppedLinkID = -1;
	ImVec2 m_ClickedMousePos = ImVec2(0, 0);
	int m_StartNodeID = -1, m_StartPinID = -1;
	int m_EndNodeID = -1, m_EndPinID = -1;
	
	Path m_OpenFilePath;
	Path m_OpenTemplateFilePath;
	Path m_OpenGeneratedFilePath;

	EShaderGraph m_ActiveGraph = SHADER_GRAPH_PIXEL;
	StaticArray<EditingContext, SHADER_GRAPH_COUNT> m_Contexts;
	StaticArray<ShaderGraphBuilder, SHADER_GRAPH_COUNT> m_Builders;
	StaticArray<ImNodesEditorContext, SHADER_GRAPH_COUNT> m_ImNodesContexts;
	
	EditingContext& m_Context = m_Contexts[m_ActiveGraph];
	ShaderGraphBuilder& m_Builder = m_Builders[m_ActiveGraph];
	ImNodesEditorContext& m_ImNodesContext = m_ImNodesContexts[m_ActiveGraph];
};

}