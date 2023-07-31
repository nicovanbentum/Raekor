#include "pch.h"
#include "NodeGraphWidget.h"
#include "OS.h"
#include "application.h"

namespace Raekor {

RTTI_CLASS_CPP_NO_FACTORY(NodeGraphWidget) {}

NodeGraphWidget::NodeGraphWidget(Application* inApp) : IWidget(inApp, reinterpret_cast<const char*>(ICON_FA_SITEMAP "  Node Graph ")) {}

void NodeGraphWidget::Draw(Widgets* inWidgets, float dt) {
	m_Visible = ImGui::Begin(m_Title.c_str(), &m_Open);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2.0f, 2.0f));

	if (ImGui::Button(reinterpret_cast<const char*>(ICON_FA_FOLDER_OPEN " Open"))) {
		std::string opened_file_path = OS::sOpenFileDialog("All Files (*.json)\0*.json\0");

		if (!opened_file_path.empty()) {
			auto ifs = std::ifstream(opened_file_path);

			auto buffer = std::stringstream();
			buffer << ifs.rdbuf();

			auto str_buffer = buffer.str();
			m_JSON = JSON::Parser(str_buffer);
			m_JSON.Parse();
			m_OpenFilePath = FileSystem::relative(opened_file_path).string();

		/*	jsmn_parser parser;
			jsmn_init(&parser);
			const auto nr_of_tokens = jsmn_parse(&parser, str_buffer.c_str(), str_buffer.size(), NULL, 0);
			
			jsmn_init(&parser);
			std::vector<jsmntok_t> tokens(nr_of_tokens);
			jsmnerr err = (jsmnerr)jsmn_parse(&parser, str_buffer.c_str(), str_buffer.size(), tokens.data(), tokens.size());

			for (const auto& token : tokens) {
				if (token.parent == -1) {

				}
			}*/
		}
	}

	ImGui::SameLine();

	if (ImGui::Button((const char*)ICON_FA_SAVE " Save")) {
		if (!m_OpenFilePath.empty()) {
			JSON::ObjectBuilder builder;
			
		}
	}

	ImGui::SameLine();

	if (ImGui::Button((const char*)ICON_FA_SAVE " Save As..")) {
		std::string file_path = OS::sSaveFileDialog("JSON File (*.json)\0", "json");

		if (!file_path.empty()) {

		}
	}

	ImGui::SameLine();

	if (ImGui::Button((const char*)ICON_FA_ADJUST " Auto Layout")) {
		auto pin_index = 0u;
		auto link_index = 0u;

		std::queue<uint32_t> queue;
		queue.push(0);

		auto cursor = ImVec2(0, 0);
		auto depth_size = queue.size();

		while (!queue.empty()) {
			auto object_index = queue.front();
			queue.pop();
			depth_size--;

			ImNodes::SetNodeGridSpacePos(object_index, cursor);

			const auto& object = m_JSON.Get(object_index);

			auto child_index = 0u;
			for (const auto& [name, value] : object) {
				if (value.mType == JSON::ValueType::Object)
					queue.push(object_index + ++child_index);
			}

			const auto dimensions = ImNodes::GetNodeDimensions(object_index);
			if (depth_size == 0) {
				depth_size = queue.size();
				cursor.x += dimensions.x + 50.0f;
				cursor.y = 0.0f - ((depth_size * 0.5f) * (dimensions.y + 50.0f));
			}
			else
				cursor.y += dimensions.y + 50.0f;
		}
	}

		ImGui::SameLine();
		ImGui::Text("Current File: \"%s\"", m_OpenFilePath.c_str());

		ImGui::PopStyleVar();

		ImNodes::BeginNodeEditor();
		ImNodes::PushColorStyle(ImNodesCol_TitleBar, ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark)));
		ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered, ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark)));
		ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected, ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark)));
		ImNodes::PushColorStyle(ImNodesCol_NodeBackground, ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(ImGuiCol_WindowBg)));
		ImNodes::PushColorStyle(ImNodesCol_GridBackground, ImGui::ColorConvertFloat4ToU32(ImVec4(0.22f, 0.22f, 0.22f, 1.0f)));
		//ImNodes::PushColorStyle(ImNodesCol_GridLine, ImGui::ColorConvertFloat4ToU32(ImVec4(0.16f, 0.16f, 0.16f, 0.35f)));
		ImNodes::PushColorStyle(ImNodesCol_GridLinePrimary, ImGui::ColorConvertFloat4ToU32(ImVec4(0.56f, 0.16f, 0.16f, 0.35f)));
		ImNodes::PushColorStyle(ImNodesCol_NodeBackgroundHovered, ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(ImGuiCol_WindowBg)));
		ImNodes::PushColorStyle(ImNodesCol_NodeBackgroundSelected, ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(ImGuiCol_WindowBg)));
		ImNodes::PushColorStyle(ImNodesCol_Link, ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)));
		ImNodes::PushColorStyle(ImNodesCol_MiniMapLink, ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)));
		ImNodes::PushColorStyle(ImNodesCol_Pin, ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)));
		ImNodes::PushColorStyle(ImNodesCol_PinHovered, ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 1.0f, 0.0f, 1.0f)));
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

					auto rtti = RTTIFactory::GetRTTI(registered_type.second->GetTypeName());
					auto instance = RTTIFactory::Construct(registered_type.second->GetTypeName());

					for (uint32_t member_index = 0; member_index < rtti->GetMemberCount(); member_index++) {
						auto member = rtti->GetMember(member_index);
						member->ToJSON(object[member->GetCustomName()], instance);
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

		uint32_t pin_index = 0;
		uint32_t link_index = 0;

		std::queue<uint32_t> child_pins;

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
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));

			int input_pin_index = -1;

			if (index > 0) {
				input_pin_index = pin_index;
				ImNodes::BeginInputAttribute(pin_index++);

				ImGui::Text(type_string.c_str());
				ImNodes::EndInputAttribute();
			}
			else {
				ImGui::Text(type_string.c_str());
			}

			ImGui::PopStyleColor();

			ImNodes::EndNodeTitleBar();

			const auto rtti = RTTIFactory::GetRTTI(type_string.c_str());
			
			for (auto member_index = 0u; member_index < rtti->GetMemberCount(); member_index++) {
				auto member = rtti->GetMember(member_index);
				
				if (object.find(member->GetCustomName()) != object.end()) {
					const auto& value = object.at(member->GetCustomName());

					if (pin_index == m_LinkPinDropped) {
						// auto& new_object = m_JSON.AddObject();
					}

					if (value.mType == JSON::ValueType::Object) {
						child_pins.push(pin_index);
						ImNodes::BeginOutputAttribute(pin_index++);
						ImGui::Text(member->GetCustomName());
						ImNodes::EndOutputAttribute();

					}
					else {
						ImNodes::BeginStaticAttribute(pin_index++);
						ImGui::Text(member->GetCustomName());
						ImNodes::EndStaticAttribute();
					}
				}
				else {
					if (m_WasLinkConnected && (pin_index == start_pin_id || pin_index == end_pin_id)) {
						auto& new_value = object[member->GetCustomName()];
						new_value.mType = JSON::ValueType::Object;
					}

					ImNodes::BeginOutputAttribute(pin_index++);
					ImGui::Text(member->GetCustomName());
					ImNodes::EndOutputAttribute();
				}
			}

			ImNodes::EndNode();

			if (!child_pins.empty() && input_pin_index != -1) {
				ImNodes::Link(link_index++, child_pins.front(), input_pin_index);
				child_pins.pop();
			};

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

		m_WasLinkConnected = ImNodes::IsLinkCreated(&start_node_id, &start_pin_id, &end_node_id, &start_node_id);

		ImNodes::IsLinkDropped(&m_LinkPinDropped, false);

		ImGui::End();
}


void NodeGraphWidget::OnEvent(Widgets* inWidgets, const SDL_Event& ev) {
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