#include "pch.h"
#include "NodeGraphWidget.h"

#include "OS.h"
#include "member.h"
#include "archive.h"
#include "shadernodes.h"
#include "application.h"

namespace Raekor {

RTTI_DEFINE_TYPE_NO_FACTORY(ShaderGraphWidget) {}


ShaderGraphWidget::ShaderGraphWidget(Application* inApp) : 
	IWidget(inApp, reinterpret_cast<const char*>( ICON_FA_SITEMAP "  Shader Graph " ))
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

	GetBuilder().ResetSnapshots();
	GetBuilder().StoreSnapshot();

	ImNodes::EditorContextSet(&GetImNodesContext());
}


void ShaderGraphWidget::Draw(Widgets* inWidgets, float dt)
{
	m_Visible = ImGui::Begin(m_Title.c_str(), &m_Open);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2.0f, 2.0f));

	if (ImGui::Button(reinterpret_cast<const char*>( ICON_FA_FOLDER_OPEN " Open.." )))
	{
		std::string file_path = OS::sOpenFileDialog("JSON File (*.json)\0");

		if (!file_path.empty())
		{
			m_OpenFilePath = fs::relative(file_path);
			JSON::ReadArchive archive(file_path);

			GetBuilder().OpenFromFileJSON(archive);
			GetBuilder().ResetSnapshots();
			GetBuilder().StoreSnapshot();
		}
	}

	ImGui::SameLine();

	if (ImGui::BeginPopupModal("Error", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("No ShaderNode backend selected.\n");

		if (ImGui::Button("OK"))
			ImGui::CloseCurrentPopup();

		ImGui::EndPopup();
	}

	if (ImGui::Button((const char*)ICON_FA_SAVE " Save As.."))
	{
		std::string file_path = OS::sSaveFileDialog("JSON File (*.json)\0", "json");

		if (!file_path.empty())
		{
			JSON::WriteArchive archive(file_path);
			GetBuilder().SaveToFileJSON(archive);
		}
	}

	ImGui::SameLine();

	if (ImGui::Button((const char*)ICON_FA_SAVE " Generate Code.."))
	{
		if (!fs::exists(m_OpenTemplateFilePath) || !fs::is_regular_file(m_OpenTemplateFilePath))
		{
			ImGui::OpenPopup("Error");
		}
		else
		{
			std::string file_path = OS::sSaveFileDialog("HLSL File (*.hlsl)\0", "hlsl");

			if (!file_path.empty())
			{
				auto ifs = std::ifstream(m_OpenTemplateFilePath);
				auto buffer = std::stringstream();
				
				buffer << ifs.rdbuf();
				m_GeneratedCode = buffer.str();
				GetBuilder().GenerateCodeFromTemplate(m_GeneratedCode);

				auto ofs = std::ofstream(file_path);
				ofs << m_GeneratedCode;

				m_OpenGeneratedFilePath = file_path;
			}
		}
	}

	ImGui::SameLine();

	if (ImGui::Button((const char*)ICON_FA_ADJUST " Auto Layout"))
	{
		auto pin_index = 0u;
		auto link_index = 0u;

		std::queue<uint32_t> queue;

		for (const auto& [index, shader_node] : gEnumerate(GetBuilder().GetShaderNodes()))
		{
			if (shader_node->GetRTTI() == RTTI_OF(PixelShaderOutputShaderNode) || shader_node->GetRTTI() == RTTI_OF(VertexShaderOutputShaderNode))
			{
				queue.push(index);
			}
		}

		auto cursor = ImVec2(0, 0);
		auto depth_size = queue.size();

		while (!queue.empty())
		{
			auto object_index = queue.front();
			queue.pop();
			depth_size--;

			ImNodes::SetNodeGridSpacePos(object_index, cursor);

			const auto dimensions = ImNodes::GetNodeDimensions(object_index);

			ShaderNode* shader_node = GetBuilder().GetShaderNode(object_index);

			if (depth_size == 0)
			{
				cursor.x -= dimensions.x + 50.0f;
				cursor.y = 0.0f - ( ( depth_size * 0.5f ) * ( dimensions.y + 50.0f ) );
			}
			else
				cursor.y += dimensions.x + 50.0f;

			for (const ShaderNodePin& input_pin : GetBuilder().GetShaderNode(object_index)->GetInputPins())
			{
				/*if (m_Builder.HasIncomingPin(input_pin.GetIndex()))
					queue.push(input_pin.GetConnectedNode());*/
			}

			if (depth_size == 0)
				depth_size = queue.size();
		}
	}

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
				m_ActiveGraph = (EShaderGraph)i;
				ImNodes::EditorContextSet(&GetImNodesContext());
			}
		}

		ImGui::EndCombo();
	}

	ImGui::SameLine();

	ImGui::SetNextItemWidth(ImGui::CalcTextSize("DeferredPS  ").x * 2.0f);

	if (ImGui::BeginCombo("##template", m_OpenTemplateFilePath.stem().string().c_str()))
	{
		for (fs::directory_entry entry : fs::directory_iterator("assets\\system\\shaders\\DirectX\\ShaderNodes"))
		{
			Path path = entry.path();
			String stem = path.stem().string();
			if (ImGui::Selectable(stem.c_str(), path == m_OpenTemplateFilePath))
				m_OpenTemplateFilePath = entry.path();
		}

		ImGui::EndCombo();
	}

	ImGui::SameLine();

	ImGui::Text("Current File: \"%s\"", m_OpenFilePath.string().c_str());

	ImGui::PopStyleVar();

	ImNodes::BeginNodeEditor();

	// Reset to center of the canvas if we started at 0,0
	const auto panning = ImNodes::EditorContextGetPanning();

	if (panning.x == 0.0f && panning.y == 0.0f)
		ImNodes::EditorContextResetPanning(ImNodes::GetCurrentContext()->CanvasRectScreenSpace.GetCenter());	
	
	auto ShaderNodeMenuItem = [&]<typename T>(const char* inName)
	{
		if (ImGui::MenuItem(inName))
		{
			GetBuilder().CreateShaderNode<T>();
			ImNodes::SetNodeScreenSpacePos(GetBuilder().GetShaderNodes().Length() - 1, m_ClickedMousePos);

			return true;
		}

		return false;
	};

	if (ImGui::BeginPopup("Create Node"))
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ShaderNode::sTextureColor);

		if (ImGui::CollapsingHeader("Other", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::PopStyleColor();
			ShaderNodeMenuItem.template operator() < GetTimeShaderNode > ( "Time" );
			ShaderNodeMenuItem.template operator() < GetDeltaTimeShaderNode > ( "Delta Time" );
			ShaderNodeMenuItem.template operator() < ProcedureShaderNode > ( "Procedure" );
			ShaderNodeMenuItem.template operator() < GradientNoiseShaderNode > ( "Gradient Noise" );
		}

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
				ShaderNodeMenuItem.template operator() < GetNormalShaderNode > ( "Normal" );
				ShaderNodeMenuItem.template operator() < GetTangentShaderNode > ( "Tangent" );
				ShaderNodeMenuItem.template operator() < GetTexCoordShaderNode > ( "TexCoord" );
			}

			if (m_ActiveGraph == SHADER_GRAPH_PIXEL)
			{
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

	if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && ImNodes::IsEditorHovered())
	{
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

	for (const auto& [index, shader_node] : gEnumerate(GetBuilder().GetShaderNodes()))
	{
		if (ImNodes::IsNodeSelected(index))
			ImNodes::PushColorStyle(ImNodesCol_NodeOutline, IM_COL32(190, 190, 190, 200));
		else if (GetContext().HoveredNode.first&& GetContext().HoveredNode.second == index)
			ImNodes::PushColorStyle(ImNodesCol_NodeOutline, IM_COL32(178, 178, 178, 90));

		shader_node->DrawImNode(GetBuilder());

		if (ImNodes::IsNodeSelected(index))
			ImNodes::PopColorStyle();
		else if (GetContext().HoveredNode.first && GetContext().HoveredNode.second == index)
			ImNodes::PopColorStyle();
	}

	GetBuilder().EndDraw();

	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor(3);

	for (const auto& [index, link] : gEnumerate(GetBuilder().GetLinks()))
	{
		const ShaderNodePin* input_pin = GetBuilder().GetInputPin(link.second);
		const ShaderNodePin* output_pin = GetBuilder().GetOutputPin(link.first);

		ImU32 link_color = ImNodes::GetStyle().Colors[ImNodesCol_Link];

		if (ShaderNodePin::sIsCompatible(output_pin->GetKind(), input_pin->GetKind()))
			link_color = ImGui::GetColorU32(input_pin->GetColor());

		const ImVec4 color = ImGui::ColorConvertU32ToFloat4(link_color);
		const ImVec4 hovered_color = ImVec4(color.x * 1.5, color.y * 1.5, color.z * 1.5, color.y * 1.5);
		const ImVec4 selected_color = ImVec4(color.x * 2.0, color.y * 2.0, color.z * 2.0, color.y * 2.0);

		const ImU32 link_hovered_color = ImGui::ColorConvertFloat4ToU32(hovered_color);
		const ImU32 link_selected_color = ImGui::ColorConvertFloat4ToU32(selected_color);
		
		ImNodes::PushColorStyle(ImNodesCol_Link, link_color);
		ImNodes::PushColorStyle(ImNodesCol_LinkHovered, link_hovered_color);
		ImNodes::PushColorStyle(ImNodesCol_LinkSelected, link_selected_color);

		ImNodes::Link(index, link.first, link.second);

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
		GetBuilder().StoreSnapshot();
		GetBuilder().ConnectPins(m_StartPinID, m_EndPinID);
	}

	int deleted_node_id;
	if (ImNodes::IsLinkDestroyed(&deleted_node_id))
	{
		const auto& links = GetBuilder().GetLinks();
		const auto& link = links[deleted_node_id];
		GetBuilder().DisconnectPins(link.first, link.second);
	}

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
	if (ev.type == SDL_KEYDOWN && !ev.key.repeat)
	{
		switch (ev.key.keysym.sym)
		{
			case SDLK_DELETE:
			{
				std::sort(GetContext().SelectedNodes.begin(), GetContext().SelectedNodes.end(), [](int lhs, int rhs) { return lhs > rhs; });
				std::sort(GetContext().SelectedLinks.begin(), GetContext().SelectedLinks.end(), [](int lhs, int rhs) { return lhs > rhs; });

				for (int selected_link : GetContext().SelectedLinks)
				{
					Slice<Pair<int,int>> links = GetBuilder().GetLinks();
					const Pair<int, int> link = links[selected_link];
					GetBuilder().DisconnectPins(link.first, link.second);
				}

				for (int selected_node : GetContext().SelectedNodes)
				{
					for (auto link : GetBuilder().GetLinks())
					{
						const ShaderNodePin* input_pin = GetBuilder().GetInputPin(link.second);
						const ShaderNodePin* output_pin = GetBuilder().GetOutputPin(link.first);

						int input_node_index = GetBuilder().GetShaderNodeIndex(*input_pin);
						int output_node_index = GetBuilder().GetShaderNodeIndex(*output_pin);

						if (input_node_index == selected_node || output_node_index == selected_node)
							GetBuilder().DisconnectPins(link.first, link.second);
					}

					GetBuilder().DestroyShaderNode(selected_node);
				}

				//m_ShaderNodes = new_nodes;

			} break;

			case SDL_SCANCODE_Z:
			{
				if (SDL_GetModState() & KMOD_LCTRL)
				{
					GetBuilder() = GetBuilder().Undo();
				}
			} break;

			case SDL_SCANCODE_Y:
			{
				if (SDL_GetModState() & KMOD_LCTRL)
				{
					GetBuilder() = GetBuilder().Redo();
				}
			} break;
		}
	}
}

}