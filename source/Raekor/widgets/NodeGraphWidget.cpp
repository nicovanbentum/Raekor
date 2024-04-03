#include "pch.h"
#include "NodeGraphWidget.h"

#include "OS.h"
#include "member.h"
#include "archive.h"
#include "shadernodes.h"
#include "application.h"

namespace Raekor {

RTTI_DEFINE_TYPE_NO_FACTORY(NodeGraphWidget) {}


NodeGraphWidget::NodeGraphWidget(Application* inApp) : IWidget(inApp, reinterpret_cast<const char*>( ICON_FA_SITEMAP "  Node Graph " )) 
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

	ImNodesColors[ImNodesCol_Link]				 = IM_COL32(215, 215, 215, 255);
	ImNodesColors[ImNodesCol_LinkHovered]		 = IM_COL32(235, 235, 235, 255);
	ImNodesColors[ImNodesCol_LinkSelected]		 = IM_COL32(255, 255, 255, 255);

	ImNodesColors[ImNodesCol_MiniMapLink]		 = IM_COL32(255, 255, 255, 255);
	ImNodesColors[ImNodesCol_BoxSelectorOutline] = IM_COL32(235, 235, 235, 235);

	ImNodes::GetStyle().NodeCornerRounding = 0.0f;
	ImNodes::GetStyle().NodeBorderThickness = 1.5f;
	ImNodes::GetStyle().PinCircleRadius = 4.5f;
	ImNodes::GetStyle().PinQuadSideLength = 9.0f;
}


void NodeGraphWidget::Draw(Widgets* inWidgets, float dt)
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
			m_Builder.OpenFromFileJSON(archive);
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
			m_Builder.SaveToFileJSON(archive);
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
				String code = buffer.str();

				m_Builder.BeginCodeGen();

				for (const auto& shader_node : m_Builder.GetShaderNodes())
				{
					if (shader_node->GetRTTI() == RTTI_OF(PixelShaderOutputShaderNode))
					{
						String main_code = shader_node->GenerateCode(m_Builder);

						String global_code;
						for (const String& function : m_Builder.GetFunctions())
							global_code += function;

						auto global_code_start = code.find("@Global");
						auto global_code_end = global_code_start + std::strlen("@Global");
						code = code.substr(0, global_code_start) + global_code + code.substr(global_code_end);

						auto main_code_start = code.find("@Main");
						auto main_code_end = main_code_start + std::strlen("@Main");
						code = code.substr(0, main_code_start) + main_code + code.substr(main_code_end);

						std::cout << "Generated Pixel Shader Code:\n";
						std::cout << global_code << '\n';
						std::cout << main_code << '\n';
					}
				}

				m_Builder.EndCodeGen();

				auto ofs = std::ofstream(file_path);
				ofs << code;

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

		for (const auto& [index, shader_node] : gEnumerate(m_Builder.GetShaderNodes()))
		{
			if (shader_node->GetRTTI() == RTTI_OF(PixelShaderOutputShaderNode))
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

			ShaderNode* shader_node = m_Builder.GetShaderNode(object_index);

			if (depth_size == 0)
			{
				cursor.x -= dimensions.x + 50.0f;
				cursor.y = 0.0f - ( ( depth_size * 0.5f ) * ( dimensions.y + 50.0f ) );
			}
			else
				cursor.y += dimensions.x + 50.0f;

			for (const ShaderNodePin& input_pin : m_Builder.GetShaderNode(object_index)->GetInputPins())
			{
				if (input_pin.IsConnected())
					queue.push(input_pin.GetConnectedNode());
			}

			if (depth_size == 0)
				depth_size = queue.size();
		}
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
		ImNodes::EditorContextResetPanning(ImNodes::GetCurrentContext()->CanvasRectScreenSpace.GetCenter());																							   \

	auto ShaderNodeMenuItem = [&]<typename T>(const char* inName)
	{
		if (ImGui::MenuItem(inName))
		{
			m_Builder.CreateShaderNode<T>();
			ImNodes::SetNodeScreenSpacePos(m_Builder.GetShaderNodes().Length() - 1, ImGui::GetMousePos());
		}
	};

	if (ImGui::BeginPopup("Create Node"))
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ShaderNode::sTextureColor);

		if (ImGui::CollapsingHeader("Other", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::PopStyleColor();
			
			ShaderNodeMenuItem.template operator() < ProcedureShaderNode > ( "Procedure" );
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
			ShaderNodeMenuItem.template operator() < VectorOpShaderNode > ( "Operator" );
		}
		else
			ImGui::PopStyleColor();

		if (ImGui::CollapsingHeader("Control Flow", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ShaderNodeMenuItem.template operator() < CompareShaderNode > ( "Compare" );
		}

		if (ImGui::CollapsingHeader("Shader Inputs", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ShaderNodeMenuItem.template operator() < GetTimeShaderNode > ( "Time" );
			ShaderNodeMenuItem.template operator() < GetDeltaTimeShaderNode > ( "Delta Time" );
			ShaderNodeMenuItem.template operator() < GetPixelCoordShaderNode > ( "Pixel Coord" );
		}

		if (ImGui::CollapsingHeader("Shader Outputs", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ShaderNodeMenuItem.template operator() < PixelShaderOutputShaderNode > ( "Vertex Shader" );
			ShaderNodeMenuItem.template operator() < PixelShaderOutputShaderNode > ( "Pixel Shader" );
		}

		ImGui::EndPopup();
	}

	if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && ImNodes::IsEditorHovered())
		ImGui::OpenPopup("Create Node");

	int node_index = 0;
	uint32_t link_index = 0;

	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
	ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(38, 38, 38, 255));
	ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(38, 38, 38, 255));
	ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(50, 50, 50, 255));

	m_Builder.BeginDraw();

	for (const auto& [index, shader_node] : gEnumerate(m_Builder.GetShaderNodes()))
	{
		if (ImNodes::IsNodeSelected(index))
			ImNodes::PushColorStyle(ImNodesCol_NodeOutline, IM_COL32(178, 178, 178, 128));
		else if (m_HoveredNode.first&& m_HoveredNode.second == index)
			ImNodes::PushColorStyle(ImNodesCol_NodeOutline, IM_COL32(178, 178, 178, 90));

		shader_node->DrawImNode(m_Builder);

		if (ImNodes::IsNodeSelected(index))
			ImNodes::PopColorStyle();
		else if (m_HoveredNode.first && m_HoveredNode.second == index)
			ImNodes::PopColorStyle();
	}

	m_Builder.EndDraw();

	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor(3);

	for (const auto& [index, link] : gEnumerate(m_Builder.GetLinks()))
	{
		const ShaderNodePin* input_pin = m_Builder.GetInputPin(link.second);
		const ShaderNodePin* output_pin = m_Builder.GetOutputPin(link.first);

		ImU32 link_color = ImNodes::GetStyle().Colors[ImNodesCol_Link];

		if (input_pin->GetKind() == output_pin->GetKind())
			link_color = ImGui::GetColorU32(input_pin->GetColor());

		ImVec4 color = ImGui::ColorConvertU32ToFloat4(link_color);
		ImVec4 hovered_color = ImVec4(color.x * 1.5, color.y * 1.5, color.z * 1.5, color.y * 1.5);
		ImVec4 selected_color = ImVec4(color.x * 2.0, color.y * 2.0, color.z * 2.0, color.y * 2.0);

		ImU32 link_hovered_color = ImGui::ColorConvertFloat4ToU32(hovered_color);
		ImU32 link_selected_color = ImGui::ColorConvertFloat4ToU32(selected_color);
		
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

	m_hoveredLink.first = ImNodes::IsLinkHovered(&m_hoveredLink.second);
	m_HoveredNode.first = ImNodes::IsNodeHovered(&m_HoveredNode.second);

	if (ImNodes::IsLinkCreated(&m_StartNodeID, &m_StartPinID, &m_EndNodeID, &m_EndPinID))
		m_Builder.ConnectPins(m_StartPinID, m_EndPinID);

	int deleted_node_id;
	if (ImNodes::IsLinkDestroyed(&deleted_node_id))
	{
		const auto& links = m_Builder.GetLinks();
		const auto& link = links[deleted_node_id];
		m_Builder.DisconnectPins(link.first, link.second);
	}

	m_SelectedNodes.resize(ImNodes::NumSelectedNodes());
	m_SelectedLinks.resize(ImNodes::NumSelectedLinks());

	if (!m_SelectedNodes.empty())
		ImNodes::GetSelectedNodes(m_SelectedNodes.data());

	if (!m_SelectedLinks.empty())
		ImNodes::GetSelectedLinks(m_SelectedLinks.data());

	ImGui::End();
}


void NodeGraphWidget::OnEvent(Widgets* inWidgets, const SDL_Event& ev)
{
	if (ev.type == SDL_KEYDOWN && !ev.key.repeat)
	{
		switch (ev.key.keysym.scancode)
		{
			case SDL_SCANCODE_DELETE:
			{
				std::vector<std::pair<int, int>> new_links;

				for (int selected_link : m_SelectedLinks)
				{
					Slice<Pair<int,int>> links = m_Builder.GetLinks();
					const Pair<int, int> link = links[selected_link];
					m_Builder.DisconnectPins(link.first, link.second);
				}

				//m_ShaderNodes = new_nodes;

			} break;
		}
	}
}

}