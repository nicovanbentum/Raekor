#include "pch.h"
#include "NodeGraphWidget.h"
#include "editor.h"
#include "Raekor/OS.h"

namespace Raekor {

RTTI_CLASS_CPP_NO_FACTORY(NodeGraphWidget) {}

NodeGraphWidget::NodeGraphWidget(Editor* editor) : IWidget(editor, "Node Graph") {}

void NodeGraphWidget::draw(float dt) {
	ImGui::Begin(title.c_str());

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2.0f, 2.0f));
	
	if (ImGui::Button(ICON_FA_FOLDER_OPEN " Open")) {
		std::string opened_file_path = OS::sOpenFileDialog("All Files (*.json)\0*.json\0");

		if (!opened_file_path.empty()) {
			auto ofs = std::ifstream(opened_file_path);
			auto archive = cereal::JSONInputArchive(ofs);
			archive(*this);
			for (auto& node : m_Nodes) {
				node.Build();
			}
			m_OpenFilePath = FileSystem::relative(opened_file_path).string();
		}
	} 
	
	ImGui::SameLine();

	if (ImGui::Button(ICON_FA_SAVE " Save")) {
		if (!m_OpenFilePath.empty()) {
			auto ofs = std::ofstream(m_OpenFilePath);
			auto archive = cereal::JSONOutputArchive(ofs);
			archive(*this);
		}
	}
	
	ImGui::SameLine();

	if (ImGui::Button(ICON_FA_SAVE " Save As..")) {
		std::string file_path = OS::sSaveFileDialog("JSON File (*.json)\0", "json");

		if (!file_path.empty()) {
			auto ofs = std::ofstream(file_path);
			auto archive = cereal::JSONOutputArchive(ofs);
			archive(*this);
			m_OpenFilePath = FileSystem::relative(file_path).string();
		}
	} 
	
	ImGui::SameLine();

	if (ImGui::Button(ICON_FA_ADJUST " Auto Layout"))  {
		std::queue<GraphNode*> node_stack;
		for (auto& node : m_Nodes) {
			if (node.IsRootNode()) {
				node_stack.push(&node);
				break;
			}
		}

		auto cursor = ImVec2(0, 0);
		int depth_size = node_stack.size();

		while (!node_stack.empty()) {
			auto current_node = node_stack.front();
			node_stack.pop();
			depth_size--;

			ImNodes::SetNodeGridSpacePos(current_node->id, cursor);
			
			for (const auto& outPin : current_node->outputPins) {
				for (const auto& link : m_Links) {
					if (link.start_id == outPin.id) {
						for (auto& node : m_Nodes) {
							if (node.FindPin(link.end_id)) {
								node_stack.push(&node);
								break;
							}
						}
					}
				}
			}

			const auto dimensions = ImNodes::GetNodeDimensions(current_node->id);
			if (depth_size == 0) {
				depth_size = node_stack.size();
				cursor.x += dimensions.x + 50.0f;
				cursor.y = 0.0f - ((depth_size * 0.5f) * (dimensions.y + 50.0f));
			}
			else {
				cursor.y += dimensions.y + 50.0f;
			}
		}
	}

	ImGui::SameLine();
	ImGui::Text("Current File: \"%s\"", m_OpenFilePath.c_str());

	ImGui::PopStyleVar();

	auto selected_nodes = std::vector<int>(ImNodes::NumSelectedNodes());
	if(!selected_nodes.empty())
		ImNodes::GetSelectedNodes(selected_nodes.data());

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

	if (ImGui::BeginPopup("New Node")) {
		if (ImGui::Selectable("Root Node", false)) {
			auto& node = m_Nodes.emplace_back();
			node.id = GetNextNodeID();
			node.outputPins.emplace_back(Pin::Type::INPUT, GetNextPinID());
			node.name = "Root " + std::to_string(node.id);
			node.Build();
			ImNodes::SetNodeScreenSpacePos(node.id, ImGui::GetMousePos());
			ImGui::CloseCurrentPopup();
		}

		if (ImGui::Selectable("Node", false)) {
			auto& node = m_Nodes.emplace_back();
			node.id = GetNextNodeID();
			node.inputPins.emplace_back(Pin::Type::INPUT, GetNextPinID());
			node.outputPins.emplace_back(Pin::Type::OUTPUT, GetNextPinID());
			node.name = "Node " + std::to_string(node.id);
			node.Build();
			ImNodes::SetNodeScreenSpacePos(node.id, ImGui::GetMousePos());
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	if (m_WasRightClicked && ImNodes::IsEditorHovered()) {
		ImGui::OpenPopup("New Node");
		m_WasRightClicked = false;
	}

	for (auto& node : m_Nodes) {
		const auto is_selected = std::find(selected_nodes.begin(), selected_nodes.end(), node.id) != selected_nodes.end();

		if (is_selected) {
			ImNodes::PushStyleVar(ImNodesStyleVar_NodeBorderThickness, 1.5f);
			ImNodes::PushColorStyle(ImNodesCol_NodeOutline, ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)));
		}

		ImNodes::BeginNode(node.id);

		ImNodes::BeginNodeTitleBar();
		ImGui::SetNextItemWidth(ImGui::CalcTextSize(node.name.c_str()).x + ImGui::GetFontSize());
		ImGui::InputText("##", &node.name);
		ImNodes::EndNodeTitleBar();

		for (auto& pin : node.inputPins) {
			ImNodes::BeginInputAttribute(pin.id);
			ImNodes::EndInputAttribute();
		}

		for (auto& pin : node.outputPins) {
			ImNodes::BeginOutputAttribute(pin.id);
			ImNodes::EndOutputAttribute();
		}

		ImNodes::EndNode();

		if (is_selected) {
			ImNodes::PopStyleVar();
			ImNodes::PopColorStyle();
		}
	}
	
	for (const auto& link : m_Links) {
		ImNodes::Link(link.id, link.start_id, link.end_id);
	}

	ImNodes::MiniMap();
	ImNodes::PopColorStyle();
	ImNodes::PopColorStyle();
	ImNodes::EndNodeEditor();

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
		ImGui::OpenPopup("New Node");
		/*auto& node = m_Nodes.back();
		if (!node.inputPins.empty()) {
			m_Links.emplace_back(GetNextLinkID(), base_attr_id, node.inputPins[0].id);
		}
		ImNodes::SetNodeScreenSpacePos(node.id, ImGui::GetMousePos());*/
	}

	ImGui::End();
}


void NodeGraphWidget::onEvent(const SDL_Event& ev) {
	if (ev.type == SDL_MOUSEBUTTONUP && ev.button.button == SDL_BUTTON_RIGHT)
		m_WasRightClicked = true;}

	if (ev.type == SDL_KEYDOWN && !ev.key.repeat) {
		switch (ev.key.keysym.scancode) {
			case SDL_SCANCODE_DELETE: {
				auto selected_nodes = std::vector<int>(ImNodes::NumSelectedNodes());
				auto selected_links = std::vector<int>(ImNodes::NumSelectedLinks());
				if(!selected_nodes.empty())
					ImNodes::GetSelectedNodes(selected_nodes.data());
				if(!selected_links.empty())
					ImNodes::GetSelectedLinks(selected_links.data());

				for (const auto& node_id : selected_nodes) {
					auto it = std::find_if(m_Nodes.begin(), m_Nodes.end(), [node_id](const auto& node) { return node.id == node_id; });
					if (it != m_Nodes.end()) {
						std::vector<std::vector<Link>::iterator> links_to_delete;
						for (auto link = m_Links.begin(); link != m_Links.end(); link++) {
							if (link->start_id == it->id || link->end_id == it->id)
								links_to_delete.push_back(link);
						}

						for (const auto& link_it : links_to_delete)
							m_Links.erase(link_it);

						m_Nodes.erase(it);
					}
				}

				for (const auto& link_id : selected_links) {
					auto it = std::find_if(m_Links.begin(), m_Links.end(), [link_id](const Link& link) { return link.id == link_id; });
					if (it != m_Links.end())
						m_Links.erase(it);
				}
			} break;
		}
	}
}

const GraphNode* NodeGraphWidget::FindNode(int id) const {
	for (const auto& node : m_Nodes)
		if (node.id == id)
			return &node;

	return nullptr;
}


void GraphNode::Build() {
	for (auto& pin : inputPins) {
		pin.type = Pin::Type::INPUT;
		pin.node = this;
	}

	for (auto& pin : outputPins) {
		pin.type = Pin::Type::OUTPUT;
		pin.node = this;
	}
}

const Pin* GraphNode::FindPin(int id) const {
	for (const auto& pin : inputPins)
		if (pin.id == id)
			return &pin;

	for (const auto& pin : outputPins)
		if (pin.id == id)
			return &pin;

	return nullptr;
}

}