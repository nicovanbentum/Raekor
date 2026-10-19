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
	bool IsFocused() const { return m_Focused; }

private:
	EditingContext& GetContext() { return m_Contexts[m_ActiveGraph]; }
	ShaderGraphBuilder& GetBuilder() { return m_Builders[m_ActiveGraph]; }
	ImNodesEditorContext& GetImNodesContext() { return m_ImNodesContexts[m_ActiveGraph]; }

	void OpenGraph(const Path& inFile);
	void SaveGraph(const Path& inFile);
	void GenerateCode();
	void AutoLayout();
	void DeleteSelection();

	void StoreSnapshot();
	void Undo();
	void Redo();

	static String sGetDefaultTemplate(EShaderGraph inShaderGraph);

private:
	String m_GeneratedCode;
	String m_ErrorMessage;
	bool m_ShowGeneratedCode = false;
	bool m_Focused = false;

	int m_DroppedLinkID = -1;
	ImVec2 m_ClickedMousePos = ImVec2(0, 0);
	int m_StartNodeID = -1, m_StartPinID = -1;
	int m_EndNodeID = -1, m_EndPinID = -1;
	

	EShaderGraph m_ActiveGraph = SHADER_GRAPH_PIXEL;
	StaticArray<EditingContext, SHADER_GRAPH_COUNT> m_Contexts;
	StaticArray<ShaderGraphBuilder, SHADER_GRAPH_COUNT> m_Builders;
	StaticArray<ImNodesEditorContext, SHADER_GRAPH_COUNT> m_ImNodesContexts;
	StaticArray<Path, SHADER_GRAPH_COUNT> m_OpenFilePaths;
	StaticArray<Array<ShaderGraphBuilder>, SHADER_GRAPH_COUNT> m_UndoStacks;
	StaticArray<Array<ShaderGraphBuilder>, SHADER_GRAPH_COUNT> m_RedoStacks;
};

}