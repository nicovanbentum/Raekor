#include "pch.h"
#include "NodeGraphWidget.h"

#include "OS.h"
#include "member.h"
#include "archive.h"
#include "shadernodes.h"
#include "application.h"

namespace Raekor {

RTTI_DEFINE_TYPE_NO_FACTORY(NodeGraphWidget) {}


NodeGraphWidget::NodeGraphWidget(Application* inApp) : IWidget(inApp, reinterpret_cast<const char*>( ICON_FA_SITEMAP "  Node Graph " )) {}


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
		ImGui::Text("No HLSL template selected. See console output log.\n");

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
						String generated_code = shader_node->GenerateCode(m_Builder);

						auto gen_code_start = code.find("@Code");
						auto gen_code_end = gen_code_start + std::strlen("@Code");
						code = code.substr(0, gen_code_start) + generated_code + code.substr(gen_code_end);

						std::cout << "Generated Pixel Shader Code:\n";
						std::cout << generated_code << '\n';
					}
				}

				m_Builder.EndCodeGen();

				auto ofs = std::ofstream(file_path);
				ofs << code;
			}
		}
	}

	ImGui::SameLine();

	if (ImGui::Button((const char*)ICON_FA_ADJUST " Auto Layout"))
	{
		auto pin_index = 0u;
		auto link_index = 0u;

		std::queue<uint32_t> queue;
		queue.push(0);

		auto cursor = ImVec2(0, 0);
		auto depth_size = queue.size();

		while (!queue.empty())
		{
			auto object_index = queue.front();
			queue.pop();
			depth_size--;

			ImNodes::SetNodeGridSpacePos(object_index, cursor);

			const auto dimensions = ImNodes::GetNodeDimensions(object_index);
			if (depth_size == 0)
			{
				depth_size = queue.size();
				cursor.x += dimensions.x + 50.0f;
				cursor.y = 0.0f - ( ( depth_size * 0.5f ) * ( dimensions.y + 50.0f ) );
			}
			else
				cursor.y += dimensions.y + 50.0f;
		}
	}

	ImGui::SameLine();

	ImGui::SetNextItemWidth(ImGui::CalcTextSize("DeferredPS").x * 2.0f);

	if (ImGui::BeginCombo(" Template", m_OpenTemplateFilePath.stem().string().c_str()))
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
		if (ImGui::CollapsingHeader("Float", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ShaderNodeMenuItem.template operator() < MathOpShaderNode > ( "Operator" );
			ShaderNodeMenuItem.template operator() < MathFuncShaderNode > ( "Function" );
		}

		if (ImGui::CollapsingHeader("Vector", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ShaderNodeMenuItem.template operator() < VectorComposeShaderNode > ( "Value" );
			ShaderNodeMenuItem.template operator() < VectorOpShaderNode > ( "Operator" );
		}

		if (ImGui::CollapsingHeader("Shader Inputs", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ShaderNodeMenuItem.template operator() < GetTimeShaderNode > ( "Time" );
			ShaderNodeMenuItem.template operator() < GetDeltaTimeShaderNode > ( "Delta Time" );
		}

		if (ImGui::CollapsingHeader("Shader Outputs", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ShaderNodeMenuItem.template operator() < PixelShaderOutputShaderNode > ( "Vertex Shader" );
			ShaderNodeMenuItem.template operator() < PixelShaderOutputShaderNode > ( "Pixel Shader" );
		}

		ImGui::EndPopup();
	}

	if (m_WasRightClicked && ImNodes::IsEditorHovered())
	{
		ImGui::OpenPopup("Create Node");
		m_WasRightClicked = false;
	}

	int node_index = 0;
	uint32_t link_index = 0;

	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(38.0f / 255.0f, 38.0f / 255.0f, 38.0f / 255.0f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(38.0f / 255.0f, 38.0f / 255.0f, 38.0f / 255.0f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(38.0f / 255.0f, 38.0f / 255.0f, 38.0f / 255.0f, 1.0f));

	m_Builder.DrawImNodes();

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

	ImGui::End();
}


void NodeGraphWidget::OnEvent(Widgets* inWidgets, const SDL_Event& ev)
{
	if (ev.type == SDL_MOUSEBUTTONUP && ev.button.button == SDL_BUTTON_RIGHT)
		m_WasRightClicked = true;

	if (ev.type == SDL_KEYDOWN && !ev.key.repeat)
	{
		switch (ev.key.keysym.scancode)
		{
			case SDL_SCANCODE_DELETE:
			{
				auto selected_nodes = std::vector<int>(ImNodes::NumSelectedNodes());
				auto selected_links = std::vector<int>(ImNodes::NumSelectedLinks());

				if (!selected_nodes.empty())
					ImNodes::GetSelectedNodes(selected_nodes.data());

				if (!selected_links.empty())
					ImNodes::GetSelectedLinks(selected_links.data());

				std::vector<std::pair<int, int>> new_links;

				for (int selected_link : selected_links)
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