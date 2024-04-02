#include "pch.h"
#include "shader.h"
#include "member.h"

namespace Raekor {

RTTI_DEFINE_TYPE(ShaderNode) {}

RTTI_DEFINE_TYPE(ShaderNodePin)
{
	RTTI_DEFINE_MEMBER(ShaderNodePin, SERIALIZE_ALL, "Connected Pin", m_ConnectedPin);
	RTTI_DEFINE_MEMBER(ShaderNodePin, SERIALIZE_ALL, "Connected Node", m_ConnectedNode);
}

RTTI_DEFINE_TYPE(ShaderGraphBuilder)
{
	RTTI_DEFINE_MEMBER(ShaderGraphBuilder, SERIALIZE_ALL, "Links", m_Links);
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
	Pair<int, int> end_pair = M_ShaderNodePins[inEndPin];
	Pair<int, int> start_pair = M_ShaderNodePins[inStartPin];

	ShaderNodePin* input_pin = GetShaderNode(end_pair.first)->GetInputPin(end_pair.second);
	ShaderNodePin* output_pin = GetShaderNode(start_pair.first)->GetOutputPin(start_pair.second);

	input_pin->SetConnectedPin(start_pair.second);
	input_pin->SetConnectedNode(start_pair.first);

	output_pin->SetConnectedPin(end_pair.first);
	output_pin->SetConnectedNode(end_pair.second);

	m_Links.push_back(std::make_pair(inStartPin, inEndPin));
}


void ShaderGraphBuilder::DisconnectPins(int inStartPin, int inEndPin)
{
	Pair<int, int> end_pair = M_ShaderNodePins[inEndPin];
	Pair<int, int> start_pair = M_ShaderNodePins[inStartPin];

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


void ShaderGraphBuilder::BeginNode(StringView inTitle)
{
	m_InputPinIndex = -1;
	m_OutputPinIndex = -1;

	ImNodes::BeginNode(++m_NodeIndex);
	ImNodes::BeginNodeTitleBar();
	ImGui::Text(inTitle.data());
	ImNodes::EndNodeTitleBar();
}


bool ShaderGraphBuilder::BeginInputPin()
{
	M_ShaderNodePins.emplace_back(std::make_pair(m_NodeIndex, ++m_InputPinIndex));
	ImNodes::BeginInputAttribute(M_ShaderNodePins.size() - 1);
	return m_ShaderNodes[m_NodeIndex]->GetInputPin(m_InputPinIndex)->IsConnected();
}


bool ShaderGraphBuilder::BeginOutputPin()
{
	M_ShaderNodePins.emplace_back(std::make_pair(m_NodeIndex, ++m_OutputPinIndex));
	ImNodes::BeginOutputAttribute(M_ShaderNodePins.size() - 1);
	return m_ShaderNodes[m_NodeIndex]->GetOutputPin(m_OutputPinIndex)->IsConnected();
}


void ShaderGraphBuilder::DrawImNodes()
{
	m_NodeIndex = -1;
	m_InputPinIndex = -1;
	m_OutputPinIndex = -1;
	M_ShaderNodePins.clear();

	for (const auto& shader_node : GetShaderNodes())
		shader_node->DrawImNode(*this);
}

} // raekor