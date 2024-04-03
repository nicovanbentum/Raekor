#include "pch.h"
#include "shader.h"
#include "member.h"

namespace Raekor {

RTTI_DEFINE_TYPE(ShaderNode) {}

RTTI_DEFINE_TYPE(ShaderNodePin)
{
	RTTI_DEFINE_MEMBER(ShaderNodePin, SERIALIZE_ALL, "Kind", m_Kind);
	RTTI_DEFINE_MEMBER(ShaderNodePin, SERIALIZE_ALL, "Variable Name", m_VariableName);
	RTTI_DEFINE_MEMBER(ShaderNodePin, SERIALIZE_ALL, "Connected Pin", m_ConnectedPin);
	RTTI_DEFINE_MEMBER(ShaderNodePin, SERIALIZE_ALL, "Connected Node", m_ConnectedNode);
}

RTTI_DEFINE_TYPE(ShaderGraphBuilder)
{
	RTTI_DEFINE_MEMBER(ShaderGraphBuilder, SERIALIZE_ALL, "Links", m_Links);
	RTTI_DEFINE_MEMBER(ShaderGraphBuilder, SERIALIZE_ALL, "Layout", m_Layout);
}

String ShaderNode::GenerateCode(ShaderGraphBuilder& inBuilder)
{
	String code;

	for (ShaderNodePin& input_pin : GetInputPins())
	{
		if (input_pin.IsConnected())
			code += inBuilder.GetShaderNode(input_pin.GetConnectedNode())->GenerateCode(inBuilder);
	}

	return code;
}


void ShaderGraphBuilder::ConnectPins(int inStartPin, int inEndPin)
{
	Pair<int, int> end_pair = M_ShaderNodePins.at(inEndPin);
	Pair<int, int> start_pair = M_ShaderNodePins.at(inStartPin);

	ShaderNodePin* input_pin = GetShaderNode(end_pair.first)->GetInputPin(end_pair.second);
	ShaderNodePin* output_pin = GetShaderNode(start_pair.first)->GetOutputPin(start_pair.second);

	input_pin->SetConnectedPin(start_pair.second);
	input_pin->SetConnectedNode(start_pair.first);

	output_pin->SetConnectedPin(end_pair.second);
	output_pin->SetConnectedNode(end_pair.first);

	m_Links.push_back(std::make_pair(inStartPin, inEndPin));
}


void ShaderGraphBuilder::DisconnectPins(int inStartPin, int inEndPin)
{
	Pair<int, int> end_pair = M_ShaderNodePins.at(inEndPin);
	Pair<int, int> start_pair = M_ShaderNodePins.at(inStartPin);

	ShaderNodePin* input_pin = GetShaderNode(end_pair.first)->GetInputPin(end_pair.second);
	ShaderNodePin* output_pin = GetShaderNode(start_pair.first)->GetOutputPin(start_pair.second);

	input_pin->SetConnectedPin(-1);
	input_pin->SetConnectedNode(-1);

	output_pin->SetConnectedPin(-1);
	output_pin->SetConnectedNode(-1);

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
	ImGui::Text("%i - %s", m_NodeIndex, inTitle.data());
	ImNodes::EndNodeTitleBar();
}


void ShaderGraphBuilder::BeginNode(StringView inTitle, ImU32 inColor)
{
	m_InputPinIndex = -1;
	m_OutputPinIndex = -1;

	ImNodes::PushColorStyle(ImNodesCol_TitleBar, inColor);
	ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered, inColor);
	ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected, inColor);

	ImNodes::BeginNode(++m_NodeIndex);
	ImNodes::BeginNodeTitleBar();
	ImGui::Text("%i - %s", m_NodeIndex, inTitle.data());
	ImNodes::EndNodeTitleBar();

	ImNodes::PopColorStyle();
	ImNodes::PopColorStyle();
	ImNodes::PopColorStyle();
}


bool ShaderGraphBuilder::BeginInputPin()
{
	ShaderNodePin* input_pin = m_ShaderNodes[m_NodeIndex]->GetInputPin(++m_InputPinIndex);
	ImNodes::PushColorStyle(ImNodesCol_Pin,input_pin->GetColor());
	M_ShaderNodePins.emplace_back(std::make_pair(m_NodeIndex, m_InputPinIndex));
	ImNodes::BeginInputAttribute(M_ShaderNodePins.size() - 1, ImNodesPinShape_CircleFilled);
	return  input_pin->IsConnected();
}


bool ShaderGraphBuilder::BeginOutputPin()
{
	ShaderNodePin* output_pin = m_ShaderNodes[m_NodeIndex]->GetOutputPin(++m_OutputPinIndex);
	ImNodes::PushColorStyle(ImNodesCol_Pin, output_pin->GetColor());
	M_ShaderNodePins.emplace_back(std::make_pair(m_NodeIndex, m_OutputPinIndex));
	ImNodes::BeginOutputAttribute(M_ShaderNodePins.size() - 1, ImNodesPinShape_QuadFilled);
	return output_pin->IsConnected();
}


void ShaderGraphBuilder::BeginDraw()
{
	m_NodeIndex = -1;
	m_InputPinIndex = -1;
	m_OutputPinIndex = -1;
	M_ShaderNodePins.clear();
}


void ShaderGraphBuilder::EndDraw()
{

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