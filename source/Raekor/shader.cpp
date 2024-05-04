#include "pch.h"
#include "shader.h"
#include "iter.h"
#include "member.h"

namespace RK {

RTTI_DEFINE_TYPE(ShaderNode) {}

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


void ShaderGraphBuilder::DestroyShaderNode(int inIndex)
{
	const int node_count = m_ShaderNodes.size();
	const int back_index = m_ShaderNodes.size() - 1;

	m_ShaderNodes[inIndex] = m_ShaderNodes.back();
	m_ShaderNodes.pop_back();

	if (node_count > 1)
		ImNodes::SetNodeScreenSpacePos(inIndex, ImNodes::GetNodeScreenSpacePos(back_index));
}


void ShaderGraphBuilder::ConnectPins(int inStartPin, int inEndPin)
{
	m_IncomingLinks[inEndPin] = inStartPin;
	m_OutgoingLinks[inStartPin].push_back(inEndPin);

	m_Links.emplace_back(inStartPin, inEndPin);
}


void ShaderGraphBuilder::DisconnectPins(int inStartPin, int inEndPin)
{
	for (int& pin : m_OutgoingLinks[inStartPin])
	{
		if (pin == inEndPin)
		{
			pin = m_OutgoingLinks[inStartPin].back();
			m_OutgoingLinks[inStartPin].pop_back();
			break;
		}
	}

	m_IncomingLinks.erase(inEndPin);

	for (auto& link : m_Links)
	{
		if (link.first == inStartPin && link.second == inEndPin)
		{
			link = m_Links.back();
			m_Links.pop_back();
			break;
		}
	}
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
	ShaderNodePin* input_pin = m_ShaderNodes[m_NodeIndex]->GetInputPin(++m_InputPinIndex);
	ImNodes::PushColorStyle(ImNodesCol_Pin,input_pin->GetColor());

	input_pin->SetIndex(M_ShaderNodePins.size());
	M_ShaderNodePins.emplace_back(std::make_pair(m_NodeIndex, m_InputPinIndex));
	
	ImNodes::BeginInputAttribute(input_pin->GetIndex(), ImNodesPinShape_CircleFilled);
	return HasIncomingPin(input_pin->GetIndex());
}


bool ShaderGraphBuilder::BeginOutputPin()
{
	ShaderNodePin* output_pin = m_ShaderNodes[m_NodeIndex]->GetOutputPin(++m_OutputPinIndex);
	ImNodes::PushColorStyle(ImNodesCol_Pin, output_pin->GetColor());
	
	output_pin->SetIndex(M_ShaderNodePins.size());
	M_ShaderNodePins.emplace_back(std::make_pair(m_NodeIndex, m_OutputPinIndex));

	ImNodes::BeginOutputAttribute(output_pin->GetIndex(), ImNodesPinShape_QuadFilled);
	return HasOutgoingPins(output_pin->GetIndex());
}


void ShaderGraphBuilder::BeginDraw()
{
	m_NodeIndex = -1;
	m_InputPinIndex = -1;
	m_OutputPinIndex = -1;
	M_ShaderNodePins.clear();
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
	pin_indegree.resize(local_builder.GetShaderNodes().Length());
	pin_outdegree.resize(local_builder.GetShaderNodes().Length());

	for (const auto& [index, shader_node] : gEnumerate(local_builder.GetShaderNodes()))
	{
		for (const ShaderNodePin& output_pin : shader_node->GetOutputPins())
		{
			if (local_builder.HasOutgoingPins(output_pin.GetIndex()))
				pin_indegree[index] += local_builder.GetOutgoingPins(output_pin.GetIndex()).Length();
		}

		for (const ShaderNodePin& input_pin : shader_node->GetInputPins())
		{
			if (local_builder.HasIncomingPin(input_pin.GetIndex()))
				pin_outdegree[index] += 1;
		}
	}

	for (int i = 0; i < pin_indegree.size(); i++)
	{
		std::cout << "Node " << i << " has " << pin_outdegree[i] << "incoming links and " << pin_indegree[i] << " out links\n";

		if (pin_indegree[i] == 0)
		{
			ShaderNode* node = local_builder.GetShaderNode(i);

			if (!node->GetOutputPins().IsEmpty())
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

			for (const auto& input_pin : local_builder.GetShaderNode(node)->GetInputPins())
			{
				if (const ShaderNodePin* pin = local_builder.GetIncomingPin(input_pin))
				{
					int connected_node_index = local_builder.GetShaderNodeIndex(*pin);
					pin_indegree[connected_node_index]--;

					if (pin_indegree[connected_node_index] == 0)
						nodes.push(connected_node_index);
				}
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

	for (const auto& link : m_Links)
	{
		m_IncomingLinks[link.second] = link.first;
		m_OutgoingLinks[link.first].push_back(link.second);
	}
}

} // raekor