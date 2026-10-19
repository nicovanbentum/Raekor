#include "PCH.h"
#include "NodeGraphWidget.h"
#include "ShaderGraphNodes.h"
#include "Application.h"
#include "Archive.h"
#include "Editor.h"
#include "Member.h"
#include "Iter.h"
#include "OS.h"

namespace RK {

RTTI_DEFINE_TYPE_NO_FACTORY(ShaderGraphWidget) {}


ShaderGraphWidget::ShaderGraphWidget(Editor* inEditor) : 
	IWidget(inEditor, reinterpret_cast<const char*>( ICON_FA_SITEMAP "  Shader Graph " ))
{ 
	auto& ImNodesColors = ImNodes::GetStyle().Colors;

	ImNodesColors[ImNodesCol_TitleBar]			= IM_COL32(0, 0, 0, 255);
	ImNodesColors[ImNodesCol_TitleBarHovered]	= IM_COL32(0, 0, 0, 255);
	ImNodesColors[ImNodesCol_TitleBarSelected]	= IM_COL32(0, 0, 0, 255);

	ImNodesColors[ImNodesCol_GridLine]			= IM_COL32(24, 24, 24, 235);
	ImNodesColors[ImNodesCol_GridBackground]	= IM_COL32(34, 34, 34, 255);
	ImNodesColors[ImNodesCol_GridLinePrimary]	= IM_COL32(2, 2, 2, 85);

	ImNodesColors[ImNodesCol_NodeOutline]			  = IM_COL32(12, 12, 12, 200);
	ImNodesColors[ImNodesCol_NodeBackground]		  = IM_COL32(57, 57, 57, 255);
	ImNodesColors[ImNodesCol_NodeBackgroundHovered]	  = IM_COL32(57, 57, 57, 255);
	ImNodesColors[ImNodesCol_NodeBackgroundSelected]  = IM_COL32(57, 57, 57, 255);

	ImNodesColors[ImNodesCol_Pin]				 = IM_COL32(127, 127, 127, 255);
	ImNodesColors[ImNodesCol_PinHovered]		 = IM_COL32(255, 255, 255, 255);

	ImNodesColors[ImNodesCol_Link]				 = IM_COL32(215, 215, 215, 200);
	ImNodesColors[ImNodesCol_LinkHovered]		 = IM_COL32(235, 235, 235, 200);
	ImNodesColors[ImNodesCol_LinkSelected]		 = IM_COL32(255, 255, 255, 200);

	ImNodesColors[ImNodesCol_MiniMapLink]		 = IM_COL32(255, 255, 255, 255);
	ImNodesColors[ImNodesCol_BoxSelectorOutline] = IM_COL32(235, 235, 235, 235);

	ImNodes::GetStyle().NodeCornerRounding = 0.0f;
	ImNodes::GetStyle().NodeBorderThickness = 1.5f;
	ImNodes::GetStyle().PinCircleRadius = 4.5f;
	ImNodes::GetStyle().PinQuadSideLength = 9.0f;

	ImNodes::EditorContextSet(&GetImNodesContext());
}


void ShaderGraphWidget::Draw(Widgets* inWidgets, float dt)
{
	m_Visible = ImGui::Begin(m_Title.c_str(), &m_Open);
	m_Focused = m_Visible && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

	ImNodes::EditorContextSet(&GetImNodesContext());

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2.0f, 2.0f));

	if (ImGui::Button(reinterpret_cast<const char*>( ICON_FA_FOLDER_OPEN " Open.." )))
	{
		String file_path = OS::sOpenFileDialog("JSON File (*.json)\0*.json\0");

		if (!file_path.empty())
			OpenGraph(file_path);
	}

	ImGui::SameLine();

	if (ImGui::Button((const char*)ICON_FA_SAVE " Save"))
	{
		if (m_OpenFilePaths[m_ActiveGraph].empty())
		{
			String file_path = OS::sSaveFileDialog("JSON File (*.json)\0*.json\0", "json");

			if (!file_path.empty())
				SaveGraph(file_path);
		}
		else
			SaveGraph(m_OpenFilePaths[m_ActiveGraph]);
	}

	ImGui::SameLine();

	if (ImGui::Button((const char*)ICON_FA_SAVE " Save As.."))
	{
		String file_path = OS::sSaveFileDialog("JSON File (*.json)\0*.json\0", "json");

		if (!file_path.empty())
			SaveGraph(file_path);
	}

	ImGui::SameLine();

	if (ImGui::Button((const char*)ICON_FA_SAVE " Generate Code"))
	{
		GenerateCode();

		if (!m_ErrorMessage.empty())
			ImGui::OpenPopup("Error");
	}

	if (ImGui::BeginPopupModal("Error", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("%s", m_ErrorMessage.c_str());

		if (ImGui::Button("OK"))
			ImGui::CloseCurrentPopup();

		ImGui::EndPopup();
	}

	ImGui::SameLine();

	if (ImGui::Button((const char*)ICON_FA_ADJUST " Auto Layout"))
		AutoLayout();

	ImGui::SameLine();

	ImGui::Button((const char*)ICON_FA_COG);

	ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 5.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));

	if (ImGui::BeginPopupContextItem(NULL, ImGuiPopupFlags_MouseButtonLeft))
	{
		ImGui::PushStyleVar(ImGuiStyleVar_SeparatorTextAlign, ImVec2(0.5f, 0.5f));

		bool show_node_indices = GetBuilder().GetShowNodeIndices();
		ImGui::Checkbox("Show Node Indices", &show_node_indices);
		GetBuilder().SetShowNodeIndices(show_node_indices);

		ImGui::Checkbox("Show Generated Code", &m_ShowGeneratedCode);

		ImGui::PopStyleVar();
		ImGui::EndPopup();
	}

	ImGui::PopStyleVar();
	ImGui::PopStyleVar();

	ImGui::SameLine();

	static constexpr std::array shader_graph_names =
	{
		"Vertex Shader",
		"Pixel Shader"
	};

	ImGui::SetNextItemWidth(ImGui::CalcTextSize("Vertex Shader  ").x * 2.0f);

	if (ImGui::BeginCombo("##shaderstage", shader_graph_names[m_ActiveGraph]))
	{
		for (int i = 0; i < SHADER_GRAPH_COUNT; i++)
		{
			if (ImGui::Selectable(shader_graph_names[i], m_ActiveGraph == (EShaderGraph)i))
			{
				SetActiveShaderGraph((EShaderGraph)i);
			}
		}

		ImGui::EndCombo();
	}

	ImGui::SameLine();

	ImGui::SetNextItemWidth(ImGui::CalcTextSize("DeferredPS  ").x * 2.0f);

	if (GetBuilder().GetTemplateFile().empty())
		GetBuilder().SetTemplateFile(sGetDefaultTemplate(m_ActiveGraph));

	const Path template_file = GetBuilder().GetTemplateFile();

	if (ImGui::BeginCombo("##template", template_file.stem().string().c_str()))
	{
		for (const fs::directory_entry& entry : fs::directory_iterator("Assets/Shaders/Backend/Materials"))
		{
			if (entry.path().extension() != ".hlsl")
				continue;

			const String path = entry.path().generic_string();
			const String stem = entry.path().stem().string();

			if (ImGui::Selectable(stem.c_str(), path == template_file.generic_string()))
				GetBuilder().SetTemplateFile(path);
		}

		ImGui::EndCombo();
	}

	ImGui::SameLine();

	ImGui::Text("Current File: \"%s\"", m_OpenFilePaths[m_ActiveGraph].string().c_str());

	ImGui::PopStyleVar();

	ImNodes::BeginNodeEditor();

	// Reset to center of the canvas if we started at 0,0
	const ImVec2 panning = ImNodes::EditorContextGetPanning();

	if (panning.x == 0.0f && panning.y == 0.0f)
		ImNodes::EditorContextResetPanning(ImNodes::GetCurrentContext()->CanvasRectScreenSpace.GetCenter());	
	
	auto ShaderNodeMenuItem = [&]<typename T>(const char* inName)
	{
		if (ImGui::MenuItem(inName))
		{
			StoreSnapshot();
			ShaderNode* node = GetBuilder().CreateShaderNode<T>();
			ImNodes::SetNodeScreenSpacePos(GetBuilder().GetShaderNodes().size() - 1, m_ClickedMousePos);

			return true;
		}

		return false;
	};

	if (ImGui::BeginPopup("Create Node"))
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ShaderNode::sTextureColor);

		ShaderNodeMenuItem.template operator() < RerouteShaderNode > ( "Reroute" );

		if (ImGui::CollapsingHeader("Other", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::PopStyleColor();
			ShaderNodeMenuItem.template operator() < GetTimeShaderNode > ( "Time" );
			ShaderNodeMenuItem.template operator() < GetDeltaTimeShaderNode > ( "Delta Time" );
			ShaderNodeMenuItem.template operator() < ProcedureShaderNode > ( "Procedure" );
			ShaderNodeMenuItem.template operator() < GradientNoiseShaderNode > ( "Gradient Noise" );
		}
		else
			ImGui::PopStyleColor();

		ImGui::PushStyleColor(ImGuiCol_Text, ShaderNode::sScalarColor);

		if (ImGui::CollapsingHeader("Float", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::PopStyleColor();

			ShaderNodeMenuItem.template operator() < FloatValueShaderNode > ( "Value" );
			ShaderNodeMenuItem.template operator() < FloatOpShaderNode > ( "Operator" );
			ShaderNodeMenuItem.template operator() < FloatFunctionShaderNode > ( "Function" );
		}
		else
			ImGui::PopStyleColor();

		ImGui::PushStyleColor(ImGuiCol_Text, ShaderNode::sVectorColor);

		if (ImGui::CollapsingHeader("Vector", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::PopStyleColor();

			ShaderNodeMenuItem.template operator() < VectorValueShaderNode > ( "Value" );
			ShaderNodeMenuItem.template operator() < VectorSplitShaderNode > ( "Split" );
			ShaderNodeMenuItem.template operator() < VectorOpShaderNode > ( "Operator" );
			ShaderNodeMenuItem.template operator() < VectorFunctionShaderNode > ( "Function" );
		}
		else
			ImGui::PopStyleColor();

		if (ImGui::CollapsingHeader("Control Flow", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ShaderNodeMenuItem.template operator() < CompareShaderNode > ( "Compare" );
		}

		if (ImGui::CollapsingHeader("Shader Inputs", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (m_ActiveGraph == SHADER_GRAPH_VERTEX)
			{
				ShaderNodeMenuItem.template operator() < GetPositionShaderNode > ( "Position" );
				ShaderNodeMenuItem.template operator() < GetVertexNormalShaderNode > ( "Normal" );
				ShaderNodeMenuItem.template operator() < GetTangentShaderNode > ( "Tangent" );
				ShaderNodeMenuItem.template operator() < GetTexCoordShaderNode > ( "TexCoord" );
			}

			if (m_ActiveGraph == SHADER_GRAPH_PIXEL)
			{
				ShaderNodeMenuItem.template operator() < GetAlbedoShaderNode > ( "Albedo" );
				ShaderNodeMenuItem.template operator() < GetPixelNormalShaderNode > ( "Normal" );
				ShaderNodeMenuItem.template operator() < GetMetallicShaderNode > ( "Metallic" );
				ShaderNodeMenuItem.template operator() < GetRoughnessShaderNode > ( "Roughness" );
				ShaderNodeMenuItem.template operator() < GetPixelCoordShaderNode > ( "Pixel Coord" );
				ShaderNodeMenuItem.template operator() < GetTexCoordinateShaderNode > ( "Texture Coord" );
			}
		}

		if (ImGui::CollapsingHeader("Shader Outputs", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (m_ActiveGraph == SHADER_GRAPH_VERTEX)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, ShaderNode::sVertexShaderColor);

				ShaderNodeMenuItem.template operator() < VertexShaderOutputShaderNode > ( "Vertex Shader" );
				
				ImGui::PopStyleColor();
			}

			if (m_ActiveGraph == SHADER_GRAPH_PIXEL)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, ShaderNode::sPixelShaderColor);

				ShaderNodeMenuItem.template operator() < PixelShaderOutputShaderNode > ( "Pixel Shader" );

				ImGui::PopStyleColor();
			}
		}

		ImGui::EndPopup();
	}

	if (m_DroppedLinkID >= 0 || (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && ImNodes::IsEditorHovered()))
	{
		m_DroppedLinkID = -1;
		ImGui::OpenPopup("Create Node");
		m_ClickedMousePos = ImGui::GetMousePos();
	}

	int node_index = 0;
	uint32_t link_index = 0;

	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
	ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(38, 38, 38, 255));
	ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(38, 38, 38, 255));
	ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(50, 50, 50, 255));

	GetBuilder().BeginDraw();
	ImNodes::PushAttributeFlag(ImNodesAttributeFlags_EnableLinkDetachWithDragClick);

	for (const auto& [index, shader_node] : gEnumerate(GetBuilder().GetShaderNodes()))
	{
		if (ImNodes::IsNodeSelected(index))
			ImNodes::PushColorStyle(ImNodesCol_NodeOutline, IM_COL32(190, 190, 190, 200));
		else if (GetContext().HoveredNode.first && GetContext().HoveredNode.second == index)
			ImNodes::PushColorStyle(ImNodesCol_NodeOutline, IM_COL32(178, 178, 178, 90));

		shader_node->DrawImNode(GetBuilder());

		if (ImNodes::IsNodeSelected(index))
			ImNodes::PopColorStyle();
		else if (GetContext().HoveredNode.first && GetContext().HoveredNode.second == index)
			ImNodes::PopColorStyle();
	}

	ImNodes::PopAttributeFlag();
	GetBuilder().EndDraw();

	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor(3);

	for (const auto& [index, link] : gEnumerate(GetBuilder().GetLinks()))
	{
		ShaderNode* to_node = GetBuilder().GetShaderNode(link.toNodeIndex);
		ShaderNode* from_node = GetBuilder().GetShaderNode(link.fromNodeIndex);

		const ShaderNodePin& input_pin = to_node->GetInputPin(link.toPinIndex);
		const ShaderNodePin& output_pin = from_node->GetOutputPin(link.fromPinIndex);

		ImU32 link_color = ImNodes::GetStyle().Colors[ImNodesCol_Link];

		if (ShaderNodePin::sIsCompatible(output_pin.GetKind(), input_pin.GetKind()))
			link_color = ImGui::GetColorU32(input_pin.GetColor());

		const ImVec4 color = ImGui::ColorConvertU32ToFloat4(link_color);
		const ImVec4 hovered_color = ImVec4(color.x * 1.5, color.y * 1.5, color.z * 1.5, color.y * 1.5);
		const ImVec4 selected_color = ImVec4(color.x * 2.0, color.y * 2.0, color.z * 2.0, color.y * 2.0);

		const ImU32 link_hovered_color = ImGui::ColorConvertFloat4ToU32(hovered_color);
		const ImU32 link_selected_color = ImGui::ColorConvertFloat4ToU32(selected_color);
		
		ImNodes::PushColorStyle(ImNodesCol_Link, link_color);
		ImNodes::PushColorStyle(ImNodesCol_LinkHovered, link_hovered_color);
		ImNodes::PushColorStyle(ImNodesCol_LinkSelected, link_selected_color);

		ImNodes::Link(int(index), output_pin.GetGlobalIndex(), input_pin.GetGlobalIndex());

		ImNodes::PopColorStyle();
		ImNodes::PopColorStyle();
		ImNodes::PopColorStyle();
	}
	
	ImNodes::MiniMap();
	ImNodes::EndNodeEditor();

	if (m_ShowGeneratedCode)
	{
		ImGui::Begin("Generated Code", &m_ShowGeneratedCode);

		ImGui::InputTextMultiline("##generatedcode", &m_GeneratedCode, ImGui::GetContentRegionAvail(), ImGuiInputTextFlags_ReadOnly);

		ImGui::End();
	}

	GetContext().hoveredLink.first = ImNodes::IsLinkHovered(&GetContext().hoveredLink.second);
	GetContext().HoveredNode.first = ImNodes::IsNodeHovered(&GetContext().HoveredNode.second);

	if (ImNodes::IsLinkCreated(&m_StartNodeID, &m_StartPinID, &m_EndNodeID, &m_EndPinID))
	{
		bool start_is_output = false, end_is_output = false;
		int start_pin = -1, end_pin = -1;

		if (GetBuilder().FindPin(m_StartNodeID, m_StartPinID, start_is_output, start_pin) &&
			GetBuilder().FindPin(m_EndNodeID, m_EndPinID, end_is_output, end_pin) && start_is_output != end_is_output)
		{
			StoreSnapshot();

			if (start_is_output)
				GetBuilder().AddLink(m_StartNodeID, start_pin, m_EndNodeID, end_pin);
			else
				GetBuilder().AddLink(m_EndNodeID, end_pin, m_StartNodeID, start_pin);
		}
	}

	int destroyed_link_id;
	if (ImNodes::IsLinkDestroyed(&destroyed_link_id))
	{
		StoreSnapshot();
		GetBuilder().RemoveLink(destroyed_link_id);
	}

	ImNodes::IsLinkDropped(&m_DroppedLinkID);

	GetContext().SelectedNodes.resize(ImNodes::NumSelectedNodes());
	GetContext().SelectedLinks.resize(ImNodes::NumSelectedLinks());

	if (!GetContext().SelectedNodes.empty())
		ImNodes::GetSelectedNodes(GetContext().SelectedNodes.data());

	if (!GetContext().SelectedLinks.empty())
		ImNodes::GetSelectedLinks(GetContext().SelectedLinks.data());

	ImGui::End();
}


void ShaderGraphWidget::OnEvent(Widgets* inWidgets, const SDL_Event& ev)
{
	if (ev.type != SDL_EVENT_KEY_DOWN || ev.key.repeat || !m_Focused || ImGui::GetIO().WantTextInput)
		return;

	const bool ctrl = ( SDL_GetModState() & SDL_KMOD_CTRL ) != 0;

	switch (ev.key.key)
	{
		case SDLK_DELETE:
		{
			DeleteSelection();
		} break;

		case SDLK_Z:
		{
			if (ctrl)
				Undo();
		} break;

		case SDLK_Y:
		{
			if (ctrl)
				Redo();
		} break;
	}
}

void ShaderGraphWidget::SetActiveShaderGraph(EShaderGraph inShaderGraph)
{
	m_ActiveGraph = inShaderGraph;
	ImNodes::EditorContextSet(&GetImNodesContext());
}


void ShaderGraphWidget::OpenGraph(const Path& inFile)
{
	ShaderGraphBuilder builder;
	JSON::ReadArchive archive(inFile);
	builder.OpenFromFileJSON(archive);

	EShaderGraph shader_graph = SHADER_GRAPH_PIXEL;

	for (const SharedPtr<ShaderNode>& node : builder.GetShaderNodes())
	{
		if (RTTI_OF<VertexShaderOutputShaderNode>() == node->GetRTTI())
			shader_graph = SHADER_GRAPH_VERTEX;
	}

	SetActiveShaderGraph(shader_graph);

	GetBuilder() = builder;
	GetContext() = EditingContext();
	m_OpenFilePaths[shader_graph] = fs::relative(inFile);
	m_UndoStacks[shader_graph].clear();
	m_RedoStacks[shader_graph].clear();

	if (GetBuilder().GetTemplateFile().empty())
		GetBuilder().SetTemplateFile(sGetDefaultTemplate(shader_graph));

	ImNodes::ClearNodeSelection();
	ImNodes::ClearLinkSelection();

	const String layout = String(GetBuilder().GetLayout());
	ImNodes::LoadCurrentEditorStateFromIniString(layout.data(), layout.size());
}


void ShaderGraphWidget::SaveGraph(const Path& inFile)
{
	ShaderGraphBuilder& builder = GetBuilder();

	size_t layout_size = 0;
	const char* layout = ImNodes::SaveCurrentEditorStateToIniString(&layout_size);
	builder.SetLayout(StringView(layout, layout_size));

	JSON::WriteArchive archive(inFile);
	builder.SaveToFileJSON(archive);

	m_OpenFilePaths[m_ActiveGraph] = fs::relative(inFile);
}


void ShaderGraphWidget::GenerateCode()
{
	m_ErrorMessage.clear();
	ShaderGraphBuilder& builder = GetBuilder();

	if (builder.GetTemplateFile().empty())
		builder.SetTemplateFile(sGetDefaultTemplate(m_ActiveGraph));

	if (!fs::exists(builder.GetTemplateFile()) || !fs::is_regular_file(builder.GetTemplateFile()))
	{
		m_ErrorMessage = std::format("Template {} does not exist.", builder.GetTemplateFile());
		return;
	}

	if (builder.GetOutputFile().empty())
	{
		const String file_path = OS::sSaveFileDialog("HLSL File (*.hlsl)\0*.hlsl\0", "hlsl");

		if (file_path.empty())
			return;

		builder.SetOutputFile(fs::relative(file_path).generic_string());
	}

	if (!builder.GenerateCodeFromFiles(builder.GetTemplateFile(), builder.GetOutputFile()))
	{
		m_ErrorMessage = String(builder.GetErrorMessage());
		return;
	}

	std::ifstream ifs(builder.GetOutputFile());
	std::stringstream buffer;
	buffer << ifs.rdbuf();

	m_GeneratedCode = buffer.str();
}


void ShaderGraphWidget::AutoLayout()
{
	ShaderGraphBuilder& builder = GetBuilder();
	const int node_count = int(builder.GetShaderNodes().size());

	Array<int> depths(node_count, -1);
	std::queue<int> queue;

	for (int index = 0; index < node_count; index++)
	{
		if (builder.GetShaderNode(index)->IsOutputNode())
		{
			depths[index] = 0;
			queue.push(index);
		}
	}

	while (!queue.empty())
	{
		const int node = queue.front();
		queue.pop();

		for (const ShaderGraphLink& link : builder.GetLinks())
		{
			if (link.toNodeIndex == node && depths[link.fromNodeIndex] < depths[node] + 1 && depths[node] + 1 < node_count)
			{
				depths[link.fromNodeIndex] = depths[node] + 1;
				queue.push(link.fromNodeIndex);
			}
		}
	}

	int max_depth = 0;
	for (int& depth : depths)
	{
		if (depth < 0)
			depth = 0;

		max_depth = glm::max(max_depth, depth);
	}

	float cursor_x = 0.0f;

	for (int depth = 0; depth <= max_depth; depth++)
	{
		float column_width = 0.0f;
		float cursor_y = 0.0f;

		for (int index = 0; index < node_count; index++)
		{
			if (depths[index] != depth)
				continue;

			const ImVec2 dimensions = ImNodes::GetNodeDimensions(index);
			column_width = glm::max(column_width, dimensions.x);
		}

		for (int index = 0; index < node_count; index++)
		{
			if (depths[index] != depth)
				continue;

			const ImVec2 dimensions = ImNodes::GetNodeDimensions(index);
			ImNodes::SetNodeGridSpacePos(index, ImVec2(cursor_x - column_width, cursor_y));
			cursor_y += dimensions.y + 40.0f;
		}

		cursor_x -= column_width + 80.0f;
	}
}


void ShaderGraphWidget::DeleteSelection()
{
	ShaderGraphBuilder& builder = GetBuilder();
	EditingContext& context = GetContext();

	if (context.SelectedNodes.empty() && context.SelectedLinks.empty())
		return;

	StoreSnapshot();

	std::sort(context.SelectedNodes.begin(), context.SelectedNodes.end(), [](int lhs, int rhs) { return lhs > rhs; });
	std::sort(context.SelectedLinks.begin(), context.SelectedLinks.end(), [](int lhs, int rhs) { return lhs > rhs; });

	for (int selected_link : context.SelectedLinks)
		builder.RemoveLink(selected_link);

	for (int selected_node : context.SelectedNodes)
		builder.DestroyShaderNode(selected_node);

	context.SelectedNodes.clear();
	context.SelectedLinks.clear();
	ImNodes::ClearNodeSelection();
	ImNodes::ClearLinkSelection();
}


void ShaderGraphWidget::StoreSnapshot()
{
	m_UndoStacks[m_ActiveGraph].push_back(GetBuilder());
	m_RedoStacks[m_ActiveGraph].clear();
}


void ShaderGraphWidget::Undo()
{
	Array<ShaderGraphBuilder>& undo_stack = m_UndoStacks[m_ActiveGraph];

	if (undo_stack.empty())
		return;

	m_RedoStacks[m_ActiveGraph].push_back(GetBuilder());
	GetBuilder() = undo_stack.back();
	GetBuilder().UpdatePinLinks();
	undo_stack.pop_back();

	ImNodes::ClearNodeSelection();
	ImNodes::ClearLinkSelection();
}


void ShaderGraphWidget::Redo()
{
	Array<ShaderGraphBuilder>& redo_stack = m_RedoStacks[m_ActiveGraph];

	if (redo_stack.empty())
		return;

	m_UndoStacks[m_ActiveGraph].push_back(GetBuilder());
	GetBuilder() = redo_stack.back();
	GetBuilder().UpdatePinLinks();
	redo_stack.pop_back();

	ImNodes::ClearNodeSelection();
	ImNodes::ClearLinkSelection();
}


String ShaderGraphWidget::sGetDefaultTemplate(EShaderGraph inShaderGraph)
{
	return inShaderGraph == SHADER_GRAPH_VERTEX ? "Assets/Shaders/Backend/Materials/DeferredVS.hlsl" : "Assets/Shaders/Backend/Materials/DeferredPS.hlsl";
}

}