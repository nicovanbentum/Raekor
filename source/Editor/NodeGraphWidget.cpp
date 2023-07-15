#include "pch.h"
#include "NodeGraphWidget.h"
#include "Raekor/OS.h"
#include "Raekor/application.h"

namespace Raekor {

RTTI_CLASS_CPP_NO_FACTORY(NodeGraphWidget) {}

NodeGraphWidget::NodeGraphWidget(Application* inApp) : IWidget(inApp, reinterpret_cast<const char*>(ICON_FA_SITEMAP "  Node Graph ")) {}

void NodeGraphWidget::Draw(float dt) {
	m_Visible = ImGui::Begin(m_Title.c_str(), &m_Open);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2.0f, 2.0f));
	
	if (ImGui::Button(reinterpret_cast<const char*>(ICON_FA_FOLDER_OPEN " Open"))) {
		std::string opened_file_path = OS::sOpenFileDialog("All Files (*.json)\0*.json\0");

		if (!opened_file_path.empty()) {
			auto ifs = std::ifstream(opened_file_path);

			auto buffer = std::stringstream();
			buffer << ifs.rdbuf();

			auto str_buffer = buffer.str();
			/*m_JSON = JSON::Parser(str_buffer);
			m_JSON.Parse();*/
			m_OpenFilePath = FileSystem::relative(opened_file_path).string();

			jsmn_parser parser;
			jsmn_init(&parser);
			const auto nr_of_tokens = jsmn_parse(&parser, str_buffer.c_str(), str_buffer.size(), NULL, 0);
			
			jsmn_init(&parser);
			std::vector<jsmntok_t> tokens(nr_of_tokens);
			jsmnerr err = (jsmnerr)jsmn_parse(&parser, str_buffer.c_str(), str_buffer.size(), tokens.data(), tokens.size());

			for (const auto& token : tokens) {
				if (token.parent == -1) {

				}
			}
		}
	} 
	
	ImGui::SameLine();

	if (ImGui::Button((const char*)ICON_FA_SAVE " Save")) {
		if (!m_OpenFilePath.empty()) {
			auto ofs = std::ofstream(m_OpenFilePath);
			auto archive = cereal::JSONOutputArchive(ofs);
			archive(*this);
		}
	}
	
	ImGui::SameLine();

	if (ImGui::Button((const char*)ICON_FA_SAVE " Save As..")) {
		std::string file_path = OS::sSaveFileDialog("JSON File (*.json)\0", "json");

		if (!file_path.empty()) {
			auto ofs = std::ofstream(file_path);
			auto archive = cereal::JSONOutputArchive(ofs);
			archive(*this);
			m_OpenFilePath = FileSystem::relative(file_path).string();
		}
	} 
	
	ImGui::SameLine();

	if (ImGui::Button((const char*)ICON_FA_ADJUST " Auto Layout"))  {
		auto node_fifo = std::queue<GraphNode*>();
		for (auto& node : m_Nodes)
			if (node.IsRootNode())
				node_fifo.push(&node);

		auto cursor = ImVec2(0, 0);
		int depth_size = node_fifo.size();

		while (!node_fifo.empty()) {
			auto current_node = node_fifo.front();
			node_fifo.pop();
			depth_size--;

			//ImNodes::SetNodeGridSpacePos(current_node->id, cursor);
			
			for (const auto& outPin : current_node->outputPins) {
				for (const auto& link : m_Links) {
					if (link.start_id == outPin.id) {
						for (auto& node : m_Nodes) {
							if (node.FindPin(link.end_id)) {
								node_fifo.push(&node);
								break;
							}
						}
					}
				}
			}

			//const auto dimensions = ImNodes::GetNodeDimensions(current_node->id);
			if (depth_size == 0) {
				depth_size = node_fifo.size();
				//cursor.x += dimensions.x + 50.0f;
				//cursor.y = 0.0f - ((depth_size * 0.5f) * (dimensions.y + 50.0f));
			}
			//else
				//cursor.y += dimensions.y + 50.0f;
		}
	}

	ImGui::SameLine();
	ImGui::Text("Current File: \"%s\"", m_OpenFilePath.c_str());

	ImGui::PopStyleVar();

	ImNodes::BeginNodeEditor();
	ImNodes::PushColorStyle(ImNodesCol_TitleBar, ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(ImGuiCol_Button)));
	ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered, ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(ImGuiCol_Button)));
	ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected, ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(ImGuiCol_Button)));
	ImNodes::PushColorStyle(ImNodesCol_NodeBackground, ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(ImGuiCol_WindowBg)));
	ImNodes::PushColorStyle(ImNodesCol_NodeBackgroundHovered, ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(ImGuiCol_WindowBg)));
	ImNodes::PushColorStyle(ImNodesCol_NodeBackgroundSelected, ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(ImGuiCol_WindowBg)));
	ImNodes::PushColorStyle(ImNodesCol_Link, ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)));
	ImNodes::PushColorStyle(ImNodesCol_MiniMapLink, ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)));
	ImNodes::PushColorStyle(ImNodesCol_Pin, ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 0.0f, 1.0f)));
	ImNodes::PushColorStyle(ImNodesCol_PinHovered, ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)));
	ImNodes::PushColorStyle(ImNodesCol_LinkHovered, ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 1.0f, 0.0f, 1.0f)));
	ImNodes::PushColorStyle(ImNodesCol_LinkSelected, ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 1.0f, 0.0f, 1.0f)));
	ImNodes::PushColorStyle(ImNodesCol_BoxSelectorOutline, ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 1.0f, 0.0f, 1.0f)));

	// Reset to center of the canvas if we started at 0,0
	const auto panning = ImNodes::EditorContextGetPanning();

	if (panning.x == 0.0f && panning.y == 0.0f) 
		ImNodes::EditorContextResetPanning(ImNodes::GetCurrentContext()->CanvasRectScreenSpace.GetCenter());



	if (ImGui::BeginPopup("New Type")) {
		for (const auto& registered_type : RTTIFactory::GetAllTypesIter()) {
			if (ImGui::Selectable(registered_type.second->GetTypeName(), false)) {

				auto& object = m_JSON.AddObject();
				auto& type_field = object["Type"];
				type_field.mType = JSON::ValueType::String;
				type_field.mData.mString.mPtr = registered_type.second->GetTypeName();
				type_field.mData.mString.mLength = strlen(registered_type.second->GetTypeName());
				
				std::string temp;
				JSON::ToJSONValue(object["Name"], temp);
				
				auto rtti = RTTIFactory::GetRTTI(registered_type.second->GetTypeName());
				auto instance = RTTIFactory::Construct(registered_type.second->GetTypeName());
				
				for (uint32_t member_index = 0; member_index < rtti->GetMemberCount(); member_index++) {
					auto member = rtti->GetMember(member_index);
					member->ToJSON(object[member->GetName()], instance);
				}

				ImGui::CloseCurrentPopup();
			}
		}

		ImGui::EndPopup();
	}

	if (m_WasRightClicked && ImNodes::IsEditorHovered()) {
		ImGui::OpenPopup("New Type");
		m_WasRightClicked = false;
	}

	for (uint32_t index = 0; index < m_JSON.Count(); index++) {
		auto& object = m_JSON.Get(index);
		if (!m_JSON.Contains(object, "Type"))
			continue;

		const auto& type_value = m_JSON.GetValue(object, "Type");
		const auto& type_string = type_value.As<JSON::String>().ToString();

		if (index == m_SelectedObject) {
			ImNodes::PushStyleVar(ImNodesStyleVar_NodeBorderThickness, 1.5f);
			ImNodes::PushColorStyle(ImNodesCol_NodeOutline, ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)));
		}

		ImNodes::BeginNode(index);

		ImNodes::BeginNodeTitleBar();
		ImGui::Text(type_string.c_str());
		ImNodes::EndNodeTitleBar();

		uint32_t pin_index = 0;
		for (const auto& [name, value] : object) {
			if (name == "Type" || name == "Name")
				continue;

			ImNodes::BeginOutputAttribute(pin_index++);
			ImGui::Text(name.c_str());
			ImNodes::EndOutputAttribute();
		}

		ImNodes::EndNode();

		if (index == m_SelectedObject) {
			ImNodes::PopStyleVar();
			ImNodes::PopColorStyle();
		}

	}

	ImNodes::MiniMap();
	ImNodes::PopColorStyle();
	ImNodes::PopColorStyle();
	ImNodes::EndNodeEditor();

	if (ImNodes::NumSelectedNodes() > 0) {
		auto selected_nodes = std::vector<int>(ImNodes::NumSelectedNodes());
		ImNodes::GetSelectedNodes(selected_nodes.data());
		m_SelectedObject = selected_nodes[0];
	}
	else {
		m_SelectedObject = -1;
	}

	int start_id, end_id;
	if (ImNodes::IsLinkCreated(&start_id, &end_id)) {
		m_Links.emplace_back(GetNextLinkID(), start_id, end_id);

		const GraphNode* start_node = nullptr;
		const GraphNode* out_node = nullptr;

		for (const auto& node : m_Nodes) {
			if (node.FindPin(start_id))
				start_node = &node;
			if (node.FindPin(end_id))
				out_node = &node;
		}
	}

	int base_attr_id = 0;
	if (ImNodes::IsLinkDropped(&base_attr_id, false)) {
		ImGui::OpenPopup("New Type");
		/*auto& node = m_Nodes.back();
		if (!node.inputPins.empty()) {
			m_Links.emplace_back(GetNextLinkID(), base_attr_id, node.inputPins[0].id);
		}
		ImNodes::SetNodeScreenSpacePos(node.id, ImGui::GetMousePos());*/
	}

	ImGui::End();
}


void NodeGraphWidget::OnEvent(const SDL_Event& ev) {
	if (ev.type == SDL_MOUSEBUTTONUP && ev.button.button == SDL_BUTTON_RIGHT)
		m_WasRightClicked = true;

	if (ev.type == SDL_KEYDOWN && !ev.key.repeat) {
		switch (ev.key.keysym.scancode) {
			case SDL_SCANCODE_DELETE: {
				auto selected_nodes = std::vector<int>(ImNodes::NumSelectedNodes());
				auto selected_links = std::vector<int>(ImNodes::NumSelectedLinks());
				if(!selected_nodes.empty())
					ImNodes::GetSelectedNodes(selected_nodes.data());
				if(!selected_links.empty())
					ImNodes::GetSelectedLinks(selected_links.data());
			} break;
		}
	}
}

}