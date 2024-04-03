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
	if (!OS::sCheckCommandLineOption("-shader_editor"))
		Hide(); 
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
	ImNodes::PushColorStyle(ImNodesCol_TitleBar, ImGui::ColorConvertFloat4ToU32(ImVec4(0, 0, 0, 1)));
	ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered, ImGui::ColorConvertFloat4ToU32(ImVec4(0, 0, 0, 1)));
	ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected, ImGui::ColorConvertFloat4ToU32(ImVec4(0, 0, 0, 1)));
	ImNodes::PushColorStyle(ImNodesCol_GridBackground, ImGui::ColorConvertFloat4ToU32(ImVec4(35.0f / 255.0f, 35.0f / 255.0f, 35.0f / 255.0f, 1.0f)));
	ImNodes::PushColorStyle(ImNodesCol_GridLine, ImGui::ColorConvertFloat4ToU32(ImVec4(2.0f / 255.0f, 2.0f / 255.0f, 2.0f / 255.0f, 0.35f)));
	ImNodes::PushColorStyle(ImNodesCol_GridLinePrimary, ImGui::ColorConvertFloat4ToU32(ImVec4(2.0f / 255.0f, 2.0f / 255.0f, 2.0f / 255.0f, 0.35f)));
	ImNodes::PushColorStyle(ImNodesCol_NodeOutline, ImGui::ColorConvertFloat4ToU32(ImVec4(2.0f / 255.0f, 2.0f / 255.0f, 2.0f / 255.0f, 1.0f)));
	ImNodes::PushColorStyle(ImNodesCol_NodeBackground, ImGui::ColorConvertFloat4ToU32(ImVec4(52.0f / 255.0f, 52.0f / 255.0f, 52.0f / 255.0f, 1.0f)));
	ImNodes::PushColorStyle(ImNodesCol_NodeBackgroundHovered, ImGui::ColorConvertFloat4ToU32(ImVec4(52.0f / 255.0f, 52.0f / 255.0f, 52.0f / 255.0f, 1.0f)));
	ImNodes::PushColorStyle(ImNodesCol_NodeBackgroundSelected, ImGui::ColorConvertFloat4ToU32(ImVec4(52.0f / 255.0f, 52.0f / 255.0f, 52.0f / 255.0f, 1.0f)));
	ImNodes::PushColorStyle(ImNodesCol_Link, ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)));
	ImNodes::PushColorStyle(ImNodesCol_MiniMapLink, ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)));
	ImNodes::PushColorStyle(ImNodesCol_Pin, ImGui::ColorConvertFloat4ToU32(ImVec4(0.5f, 0.5f, 0.5f, 1.0f)));
	ImNodes::PushColorStyle(ImNodesCol_PinHovered, ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)));
	ImNodes::PushColorStyle(ImNodesCol_LinkHovered, ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 1.0f, 0.0f, 1.0f)));
	ImNodes::PushColorStyle(ImNodesCol_LinkSelected, ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 1.0f, 0.0f, 1.0f)));
	ImNodes::PushColorStyle(ImNodesCol_BoxSelectorOutline, ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 1.0f, 0.0f, 1.0f)));

	ImNodes::PushStyleVar(ImNodesStyleVar_NodeCornerRounding, 0);
	//ImNodes::PushStyleVar(ImNodesStyleVar_NodeBorderThickness, 0);

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
		if (ImGui::CollapsingHeader("Other", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ShaderNodeMenuItem.template operator() < ProcedureShaderNode > ( "Procedure" );
		}

		ImGui::PushStyleColor(ImGuiCol_Text, ShaderNodePin::sBlue);

		if (ImGui::CollapsingHeader("Float", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::PopStyleColor();

			ShaderNodeMenuItem.template operator() < FloatValueShaderNode > ( "Value" );
			ShaderNodeMenuItem.template operator() < FloatOpShaderNode > ( "Operator" );
			ShaderNodeMenuItem.template operator() < FloatFunctionShaderNode > ( "Function" );
		}
		else
			ImGui::PopStyleColor();

		ImGui::PushStyleColor(ImGuiCol_Text, ShaderNodePin::sOrange);

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
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(38.0f / 255.0f, 38.0f / 255.0f, 38.0f / 255.0f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(38.0f / 255.0f, 38.0f / 255.0f, 38.0f / 255.0f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(38.0f / 255.0f, 38.0f / 255.0f, 38.0f / 255.0f, 1.0f));

	m_Builder.BeginDraw();

	for (const auto& [index, shader_node] : gEnumerate(m_Builder.GetShaderNodes()))
	{
		bool selected = false;

		for (int selected_node_index : m_SelectedNodes)
		{
			if (selected_node_index == index)
			{
				selected = true;
				break;
			}
		}

		if (selected)
		{
			ImNodes::PushColorStyle(ImNodesCol_NodeOutline, ImGui::GetColorU32(ImVec4(0.7f, 0.7f, 0.9f, 0.5f)));
			ImNodes::PushStyleVar(ImNodesStyleVar_NodeBorderThickness, 2.0f);
		}

		shader_node->DrawImNode(m_Builder);

		if (selected)
		{
			ImNodes::PopColorStyle();
			ImNodes::PopStyleVar();
		}
	}

	m_Builder.EndDraw();

	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor(3);

	for (const auto& [index, link] : gEnumerate(m_Builder.GetLinks()))
		ImNodes::Link(index, link.first, link.second);
	
	ImNodes::MiniMap();
	ImNodes::EndNodeEditor();

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