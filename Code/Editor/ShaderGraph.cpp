#include "pch.h"
#include "ShaderGraph.h"
#include "Engine/iter.h"
#include "Engine/member.h"

namespace RK {

RTTI_DEFINE_TYPE(ShaderNode) {}

RTTI_DEFINE_TYPE(ShaderGraphLink) {}

RTTI_DEFINE_TYPE(ShaderNodePin)
{
	RTTI_DEFINE_MEMBER(ShaderNodePin, SERIALIZE_ALL, "Kind", m_Kind);
	RTTI_DEFINE_MEMBER(ShaderNodePin, SERIALIZE_ALL, "Variable Name", m_VariableName);
}

RTTI_DEFINE_TYPE(ShaderGraphBuilder)
{
	RTTI_DEFINE_MEMBER(ShaderGraphBuilder, SERIALIZE_ALL, "Layout", m_Layout);
	RTTI_DEFINE_MEMBER(ShaderGraphBuilder, SERIALIZE_ALL, "Links", m_Links);
}

bool ShaderNodePin::sIsCompatible(EKind inFrom, EKind inTo)
{
	if (inFrom == inTo)
		return true;

	if (inFrom == ShaderNodePin::FLOAT && inTo == ShaderNodePin::VECTOR)
		return true;

	if (inFrom == ShaderNodePin::VECTOR && inTo == ShaderNodePin::FLOAT)
		return true;

	return false;
}

const ShaderNodePin* ShaderGraphBuilder::GetIncomingPin(const ShaderNodePin& inPin)
{
	if (!inPin.HasLink())
		return nullptr;

	// TODO
	return nullptr;
}

void ShaderGraphBuilder::DestroyShaderNode(int inIndex)
{
	const int node_count = m_ShaderNodes.size();
	const int back_index = m_ShaderNodes.size() - 1;

	m_ShaderNodes[inIndex] = m_ShaderNodes.back();
	m_ShaderNodes.pop_back();

	if (node_count > 1)
		ImNodes::SetNodeScreenSpacePos(inIndex, ImNodes::GetNodeScreenSpacePos(back_index));
}


void ShaderGraphBuilder::BeginNode()
{
	m_InputPinIndex = -1;
	m_OutputPinIndex = -1;
	ImNodes::BeginNode(++m_NodeIndex);
}


void ShaderGraphBuilder::BeginNode(StringView inTitle)
{
	m_InputPinIndex = -1;
	m_OutputPinIndex = -1;

	ImNodes::BeginNode(++m_NodeIndex);
	ImNodes::BeginNodeTitleBar();

	if (m_ShowNodeIndices)
		ImGui::Text("%i - %s", m_NodeIndex, inTitle.data());
	else
		ImGui::Text(inTitle.data());

	ImNodes::EndNodeTitleBar();
}


void ShaderGraphBuilder::BeginNode(StringView inTitle, ImU32 inColor)
{
	ImNodes::PushColorStyle(ImNodesCol_TitleBar, inColor);
	ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered, inColor);
	ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected, inColor);

	BeginNode(inTitle);

	ImNodes::PopColorStyle();
	ImNodes::PopColorStyle();
	ImNodes::PopColorStyle();
}


bool ShaderGraphBuilder::BeginInputPin()
{
	ShaderNodePin& input_pin = m_ShaderNodes[m_NodeIndex]->GetInputPin(++m_InputPinIndex);
	ImNodes::PushColorStyle(ImNodesCol_Pin,input_pin.GetColor());

	input_pin.SetGlobalIndex(++m_GlobalPinIndex);
	ImNodes::BeginInputAttribute(input_pin.GetGlobalIndex(), ImNodesPinShape_CircleFilled);
	
	return input_pin.HasLink();
}


bool ShaderGraphBuilder::BeginOutputPin()
{
	ShaderNodePin& output_pin = m_ShaderNodes[m_NodeIndex]->GetOutputPin(++m_OutputPinIndex);
	ImNodes::PushColorStyle(ImNodesCol_Pin, output_pin.GetColor());
	
	output_pin.SetGlobalIndex(++m_GlobalPinIndex);
	ImNodes::BeginOutputAttribute(output_pin.GetGlobalIndex(), ImNodesPinShape_QuadFilled);
	
	return output_pin.HasLink();
}


void ShaderGraphBuilder::BeginDraw()
{
	m_NodeIndex = -1;
	m_InputPinIndex = -1;
	m_OutputPinIndex = -1;
	m_GlobalPinIndex = -1;
}

//////////////////////
// CODEGEN FUNCTIONS

bool ShaderGraphBuilder::GenerateCodeFromTemplate(String& ioCode)
{
	ShaderGraphBuilder local_builder = *this;

	local_builder.m_LineNr = 0;
	local_builder.m_Functions.clear();

	Array<int> pin_indegree;
	Array<int> pin_outdegree;
	pin_indegree.resize(local_builder.GetShaderNodes().size());
	pin_outdegree.resize(local_builder.GetShaderNodes().size());

	for (const auto& [node_index, shader_node] : gEnumerate(local_builder.GetShaderNodes()))
	{
		for (const auto& [pin_index, output_pin ] : gEnumerate(shader_node->GetOutputPins()))
		{
			for (const ShaderGraphLink& link : m_Links)
			{
				if (link.fromNodeIndex == node_index && link.fromPinIndex == pin_index)
				{
					pin_indegree[node_index]++;
				}
			}
		}

		for (const auto& [pin_index, input_pin] : gEnumerate(shader_node->GetInputPins()))
		{
			for (const ShaderGraphLink& link : m_Links)
			{
				if (link.toNodeIndex == node_index && link.toPinIndex == pin_index)
				{
					pin_outdegree[node_index]++;
					break; // input pins can only have 1 link
				}
			}
		}
	}

	for (int i = 0; i < pin_indegree.size(); i++)
	{
		std::cout << "Node " << i << " has " << pin_outdegree[i] << "incoming links and " << pin_indegree[i] << " out links\n";

		if (pin_indegree[i] == 0)
		{
			ShaderNode* node = local_builder.GetShaderNode(i);

			if (!node->GetOutputPins().empty())
				local_builder.DestroyShaderNode(i);
		}
	}

	for (const auto& [index, shader_node] : gEnumerate(local_builder.GetShaderNodes()))
	{
		if (pin_indegree[index] > 0)
			continue;

		Array<int> sorted_nodes;
		std::queue<int> nodes;
		nodes.push(index);

		while (!nodes.empty())
		{
			int node = nodes.front();
			nodes.pop();
			sorted_nodes.push_back(node);

			for (const ShaderNodePin& input_pin : local_builder.GetShaderNode(node)->GetInputPins())
			{
				/*if (const ShaderNodePin* pin = local_builder.GetIncomingPin(input_pin))
				{
					int connected_node_index = local_builder.GetShaderNodeIndex(*pin);
					pin_indegree[connected_node_index]--;

					if (pin_indegree[connected_node_index] == 0)
						nodes.push(connected_node_index);
				}*/
			}
		}

		std::reverse(sorted_nodes.begin(), sorted_nodes.end());

		std::cout << "Node evaluation order: \n";
		for (int node_index : sorted_nodes)
			std::cout << node_index << "  ";

		std::cout << '\n';

		String main_code;
		for (int node_index : sorted_nodes)
		{
			ShaderNode* shader_node = local_builder.GetShaderNode(node_index);

			String generated_code = shader_node->GenerateCode(*this);

			if (!generated_code.empty())
				main_code += std::format("    // Node {} -> {}\n", node_index, shader_node->GetRTTI().GetTypeName());
			
			std::istringstream iss(generated_code);

			for (std::string line; std::getline(iss, line); )
				main_code += std::format("    {}\n", line);
		}

		String global_code;
		for (const String& function : GetFunctions())
			global_code += function + "\n\n";

		auto global_code_start = ioCode.find("@Global");
		auto global_code_end = global_code_start + std::strlen("@Global");
		ioCode = ioCode.substr(0, global_code_start) + global_code + ioCode.substr(global_code_end);

		auto main_code_start = ioCode.find("@Main");
		auto main_code_end = main_code_start + std::strlen("@Main");
		ioCode = ioCode.substr(0, main_code_start) + main_code + ioCode.substr(main_code_end);

		std::cout << "Generated Shader Code:\n";
		std::cout << global_code << '\n';
		std::cout << main_code << '\n';

		return true;
	}

	return false;
}


void ShaderGraphBuilder::StoreSnapshot()
{
	m_Undo.push_back(*this);
	m_Redo.clear();
}


void ShaderGraphBuilder::ResetSnapshots()
{
	m_SnapshotIndex = -1;
}


ShaderGraphBuilder ShaderGraphBuilder::Undo()
{
	if (m_Undo.empty())
		return *this;

	if (m_Redo.empty())
		m_Redo.push_back(*this);
	else
		m_Redo.push_back(m_Undo.back());
	
	ShaderGraphBuilder back = m_Undo.back();
	
	if (m_Undo.size() > 0)
		m_Undo.pop_back();

	return back;
}


ShaderGraphBuilder ShaderGraphBuilder::Redo()
{
	if (m_Redo.empty())
		return *this;
	
	m_Undo.push_back(m_Redo.back());

	ShaderGraphBuilder back = m_Redo.back();

	if (!m_Redo.empty())
		m_Redo.pop_back();

	return back;
}


void ShaderGraphBuilder::SaveToFileJSON(JSON::WriteArchive& ioArchive)
{
	size_t editor_state_size = 0;
	const char* editor_state_str = ImNodes::SaveCurrentEditorStateToIniString(&editor_state_size);

	m_Layout = String(editor_state_str, editor_state_size);

	ioArchive << *this;

	for (const auto& shader_node : m_ShaderNodes)
		ioArchive.WriteNextObject(shader_node->GetRTTI(), shader_node.get());
}


void ShaderGraphBuilder::OpenFromFileJSON(JSON::ReadArchive& ioArchive)
{
	ioArchive >> *this;
	
	ImNodes::LoadCurrentEditorStateFromIniString(m_Layout.data(), m_Layout.size());

	RTTI* rtti = nullptr;
	void* object = ioArchive.ReadNextObject(&rtti);

	while (object)
	{
		m_ShaderNodes.emplace_back((ShaderNode*)object);
		object = ioArchive.ReadNextObject(&rtti);
	}
}

} // raekor