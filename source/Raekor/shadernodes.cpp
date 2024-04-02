#include "pch.h"
#include "shadernodes.h"
#include "shader.h"
#include "member.h"
#include "iter.h"

namespace Raekor {

RTTI_DEFINE_TYPE(MathOpShaderNode) 
{ 
	RTTI_DEFINE_TYPE_INHERITANCE(MathOpShaderNode, ShaderNode); 

	RTTI_DEFINE_MEMBER(MathOpShaderNode, SERIALIZE_ALL, "Op", m_Op);
	RTTI_DEFINE_MEMBER(MathOpShaderNode, SERIALIZE_ALL, "FloatA", m_FloatA);
	RTTI_DEFINE_MEMBER(MathOpShaderNode, SERIALIZE_ALL, "FloatB", m_FloatB);
	RTTI_DEFINE_MEMBER(MathOpShaderNode, SERIALIZE_ALL, "Input Pins", m_InputPins);
	RTTI_DEFINE_MEMBER(MathOpShaderNode, SERIALIZE_ALL, "Output Pins", m_OutputPins);
}

RTTI_DEFINE_TYPE(MathFuncShaderNode) 
{ 
	RTTI_DEFINE_TYPE_INHERITANCE(MathFuncShaderNode, ShaderNode); 

	RTTI_DEFINE_MEMBER(MathFuncShaderNode, SERIALIZE_ALL, "Op", m_Op);
	RTTI_DEFINE_MEMBER(MathFuncShaderNode, SERIALIZE_ALL, "Input Pin", m_InputPin);
	RTTI_DEFINE_MEMBER(MathFuncShaderNode, SERIALIZE_ALL, "Output Pin", m_OutputPin);
}

RTTI_DEFINE_TYPE(SingleOutputShaderNode)
{
	RTTI_DEFINE_TYPE_INHERITANCE(SingleOutputShaderNode, ShaderNode);
	RTTI_DEFINE_MEMBER(GetTimeShaderNode, SERIALIZE_ALL, "Output Pin", m_OutputPin);
}

RTTI_DEFINE_TYPE(GetTimeShaderNode) 
{ 
	RTTI_DEFINE_TYPE_INHERITANCE(GetTimeShaderNode, SingleOutputShaderNode); 
}

RTTI_DEFINE_TYPE(GetDeltaTimeShaderNode) 
{ 
	RTTI_DEFINE_TYPE_INHERITANCE(GetDeltaTimeShaderNode, SingleOutputShaderNode);
}

RTTI_DEFINE_ENUM(EVectorVariant)
{
	RTTI_DEFINE_ENUM_MEMBER(EVectorVariant, SERIALIZE_ALL, "VEC2", EVectorVariant::VEC2);
	RTTI_DEFINE_ENUM_MEMBER(EVectorVariant, SERIALIZE_ALL, "VEC3", EVectorVariant::VEC3);
	RTTI_DEFINE_ENUM_MEMBER(EVectorVariant, SERIALIZE_ALL, "VEC4", EVectorVariant::VEC4);
}

RTTI_DEFINE_TYPE(VectorOpShaderNode) 
{ 
	RTTI_DEFINE_TYPE_INHERITANCE(VectorOpShaderNode, ShaderNode); 

	RTTI_DEFINE_MEMBER(VectorOpShaderNode, SERIALIZE_ALL, "Op", m_Op);
	RTTI_DEFINE_MEMBER(VectorOpShaderNode, SERIALIZE_ALL, "VectorVariant", m_VectorVariant);
	RTTI_DEFINE_MEMBER(VectorOpShaderNode, SERIALIZE_ALL, "VectorA", m_VectorA);
	RTTI_DEFINE_MEMBER(VectorOpShaderNode, SERIALIZE_ALL, "VectorB", m_VectorB);
	RTTI_DEFINE_MEMBER(VectorOpShaderNode, SERIALIZE_ALL, "Input Pins", m_InputPins);
	RTTI_DEFINE_MEMBER(VectorOpShaderNode, SERIALIZE_ALL, "Output Pins", m_OutputPins);
}

RTTI_DEFINE_TYPE(VectorComposeShaderNode) 
{ 
	RTTI_DEFINE_TYPE_INHERITANCE(VectorComposeShaderNode, ShaderNode); 

	RTTI_DEFINE_MEMBER(VectorComposeShaderNode, SERIALIZE_ALL, "Vector", m_Vector);
	RTTI_DEFINE_MEMBER(VectorComposeShaderNode, SERIALIZE_ALL, "VectorVariant", m_VectorVariant);
	RTTI_DEFINE_MEMBER(VectorComposeShaderNode, SERIALIZE_ALL, "Input Pins", m_InputPins);
	RTTI_DEFINE_MEMBER(VectorComposeShaderNode, SERIALIZE_ALL, "Output Pins", m_OutputPins);
}

RTTI_DEFINE_TYPE(PixelShaderOutputShaderNode) 
{ 
	RTTI_DEFINE_TYPE_INHERITANCE(PixelShaderOutputShaderNode, ShaderNode); 

	RTTI_DEFINE_MEMBER(PixelShaderOutputShaderNode, SERIALIZE_ALL, "Albedo", m_Albedo);
	RTTI_DEFINE_MEMBER(PixelShaderOutputShaderNode, SERIALIZE_ALL, "Normal", m_Normal);
	RTTI_DEFINE_MEMBER(PixelShaderOutputShaderNode, SERIALIZE_ALL, "Emissive", m_Emissive);
	RTTI_DEFINE_MEMBER(PixelShaderOutputShaderNode, SERIALIZE_ALL, "Metallic", m_Metallic);
	RTTI_DEFINE_MEMBER(PixelShaderOutputShaderNode, SERIALIZE_ALL, "Roughness", m_Roughness);
	RTTI_DEFINE_MEMBER(PixelShaderOutputShaderNode, SERIALIZE_ALL, "Input Pins", m_InputPins);
}

void gRegisterShaderNodeTypes()
{
	g_RTTIFactory.Register(RTTI_OF(ShaderNode));
	g_RTTIFactory.Register(RTTI_OF(ShaderNodePin));
	g_RTTIFactory.Register(RTTI_OF(ShaderGraphBuilder));

	g_RTTIFactory.Register(RTTI_OF(MathOpShaderNode));
	g_RTTIFactory.Register(RTTI_OF(MathFuncShaderNode));
	g_RTTIFactory.Register(RTTI_OF(GetTimeShaderNode));
	g_RTTIFactory.Register(RTTI_OF(GetDeltaTimeShaderNode));
	g_RTTIFactory.Register(RTTI_OF(VectorComposeShaderNode));
	g_RTTIFactory.Register(RTTI_OF(PixelShaderOutputShaderNode));
}


void MathOpShaderNode::DrawImNode(ShaderGraphBuilder& inBuilder)
{
	inBuilder.BeginNode("MathOp");

	const float node_width = ImGui::CalcTextSize("Multiply").x * 1.5f;
	ImGui::PushItemWidth(node_width);

	constexpr static std::array operation_names = { "Add", "Minus", "Multiply", "Divide" };

	if (ImGui::BeginCombo("##mathopshadernodevariantop", operation_names[m_Op]))
	{
		for (const auto& [index, name] : gEnumerate(operation_names))
		{
			if (ImGui::Selectable(name, int(m_Op) == index))
				m_Op = Op(index);
		}

		ImGui::EndCombo();
	}

	if (inBuilder.BeginInputPin())
		ImGui::Text("A");
	else
		ImGui::InputFloat("A", &m_FloatA);

	inBuilder.EndInputPin();

	if (inBuilder.BeginInputPin())
		ImGui::Text("B");
	else
		ImGui::InputFloat("B", &m_FloatB);

	inBuilder.EndInputPin();

	ImGui::PopItemWidth();

	inBuilder.BeginOutputPin();
	ImGui::Indent(node_width);
	ImGui::Text("Out");
	inBuilder.EndOutputPin();

	inBuilder.EndNode();
}


String MathOpShaderNode::GenerateCode(ShaderGraphBuilder& inBuilder)
{
	String code = ShaderNode::GenerateCode(inBuilder);

	m_OutputPins[0].SetOutVariableName(std::format("MathOp_Out{}", inBuilder.IncrLineNumber()));

	String lhs = m_InputPins[0].IsConnected() ? String(inBuilder.GetConnectedOutputPin(m_InputPins[0])->GetOutVariableName()) : std::format("{:.3f}", m_FloatA);
	String rhs = m_InputPins[1].IsConnected() ? String(inBuilder.GetConnectedOutputPin(m_InputPins[1])->GetOutVariableName()) : std::format("{:.3f}", m_FloatB);

	code += std::format("float {} = float({} {} {});\n", m_OutputPins[0].GetOutVariableName(), lhs, m_OpSymbols[m_Op], rhs);

	return code;
}


void MathFuncShaderNode::DrawImNode(ShaderGraphBuilder& ioBuilder)
{
	ioBuilder.BeginNode("MathFunc");

	const float node_width = ImGui::CalcTextSize(m_OpNames[m_Op]).x * 4.0f;
	ImGui::PushItemWidth(node_width);

	if (ImGui::BeginCombo("##mathopshadernodevarianttype", m_OpNames[m_Op]))
	{
		for (const auto& [index, name] : gEnumerate(m_OpNames))
		{
			if (ImGui::Selectable(name, int(m_Op) == index))
				m_Op = Op(index);
		}

		ImGui::EndCombo();
	}

	ioBuilder.BeginInputPin();
	ImGui::Text("In");
	ioBuilder.EndInputPin();

	ImGui::SameLine();

	ioBuilder.BeginOutputPin();
	ImGui::Indent(node_width / 2.0f);
	ImGui::Text("Out");
	ioBuilder.EndOutputPin();

	ImNodes::EndNode();
}


String MathFuncShaderNode::GenerateCode(ShaderGraphBuilder& inBuilder)
{
	String code = ShaderNode::GenerateCode(inBuilder);

	ShaderNode* prev_out_node = inBuilder.GetShaderNode(m_InputPin.GetConnectedNode());
	ShaderNodePin* prev_out_pin = prev_out_node->GetOutputPin(m_InputPin.GetConnectedPin());

	m_OutputPin.SetOutVariableName(std::format("MathOp_Out{}", inBuilder.IncrLineNumber()));
	code += std::format("float {} = {}({});\n", m_OutputPin.GetOutVariableName(), m_OpNames[m_Op], prev_out_pin->GetOutVariableName());

	return code;
}


void VectorComposeShaderNode::DrawImNode(ShaderGraphBuilder& inBuilder)
{
	inBuilder.BeginNode("VectorCompose");

	const float node_width = ImGui::CalcTextSize("0.0000, ").x;
	ImGui::PushItemWidth(node_width * 1.5f);

	if (ImGui::BeginCombo("##vectorshadernodevarianttype", m_VectorNames[m_VectorVariant]))
	{
		for (const auto& [index, name] : gEnumerate(m_VectorNames))
		{
			if (ImGui::Selectable(name, int(m_VectorVariant) == index))
				m_VectorVariant = EVectorVariant(index);
		}

		ImGui::EndCombo();
	}

	ImGui::PopItemWidth();
	ImGui::PushItemWidth(node_width * 1.25f);

	auto InputFloatFunction = m_VectorInputFunctions[int(m_VectorVariant)];
	constexpr std::array xyzw_chars = { "X", "Y", "Z", "W" };

	for (int i = 0; i < m_VectorComponents[int(m_VectorVariant)]; i++)
	{
		if (inBuilder.BeginInputPin())
			ImGui::Text(xyzw_chars[i]);
		else
			ImGui::InputFloat(xyzw_chars[i], &m_Vector[i]);

		inBuilder.EndInputPin();
	}

	ImGui::PopItemWidth();

	inBuilder.BeginOutputPin();
	ImGui::Indent(node_width);
	ImGui::Text("Out");
	inBuilder.EndOutputPin();
	inBuilder.EndNode();
}


String VectorComposeShaderNode::GenerateCode(ShaderGraphBuilder& inBuilder)
{
	String code = ShaderNode::GenerateCode(inBuilder);

	const int nr_of_floats = m_VectorComponents[m_VectorVariant];

	m_OutputPins[0].SetOutVariableName(std::format("VectorCompose_Out{}", inBuilder.IncrLineNumber()));

	switch (m_VectorVariant)
	{
		case EVectorVariant::VEC2:
			code += std::format("float{} {} = float{}(", nr_of_floats, m_OutputPins[0].GetOutVariableName(), nr_of_floats);
			break;
		case EVectorVariant::VEC3:
			code += std::format("float{} {} = float{}(", nr_of_floats, m_OutputPins[0].GetOutVariableName(), nr_of_floats);
			break;
		case EVectorVariant::VEC4:
			code += std::format("float{} {} = float{}(", nr_of_floats, m_OutputPins[0].GetOutVariableName(), nr_of_floats);
			break;
	}

	for (int i = 0; i < nr_of_floats; i++)
	{
		if (m_InputPins[i].IsConnected())
		{
			ShaderNode* start_node = inBuilder.GetShaderNode(m_InputPins[i].GetConnectedNode());
			ShaderNodePin* start_pin = start_node->GetOutputPin(m_InputPins[i].GetConnectedPin());

			code += start_pin->GetOutVariableName();
		}
		else
		{
			code += std::format("{:.3f}", m_Vector[i]);
		}

		if (i < nr_of_floats - 1)
			code += ", ";
	}

	code += ");\n";

	return code;
}


void VectorOpShaderNode::DrawImNode(ShaderGraphBuilder& inBuilder)
{
	inBuilder.BeginNode("VectorOp");

	const float node_width = ImGui::CalcTextSize("0.0000, ").x * m_VectorComponents[int(m_VectorVariant)];
	ImGui::PushItemWidth(node_width);

	constexpr static std::array operation_names = { "Add", "Minus", "Multiply", "Divide" };

	if (ImGui::BeginCombo("##vectorshadernodevarianttype", m_VectorNames[m_VectorVariant]))
	{
		for (const auto& [index, name] : gEnumerate(m_VectorNames))
		{
			if (ImGui::Selectable(name, int(m_VectorVariant) == index))
				m_VectorVariant = EVectorVariant(index);
		}

		ImGui::EndCombo();
	}

	if (ImGui::BeginCombo("##vectorshadernodevariantop", operation_names[m_Op]))
	{
		for (const auto& [index, name] : gEnumerate(operation_names))
		{
			if (ImGui::Selectable(name, int(m_Op) == index))
				m_Op = Op(index);
		}

		ImGui::EndCombo();
	}

	auto InputFloatFunction = m_VectorInputFunctions[int(m_VectorVariant)];

	if (inBuilder.BeginInputPin())
		ImGui::Text("A");
	else
		InputFloatFunction("A", &m_VectorA[0], "%.3f", ImGuiInputTextFlags_None);

	inBuilder.EndInputPin();

	if (inBuilder.BeginInputPin())
		ImGui::Text("B");
	else
		InputFloatFunction("B", &m_VectorB[0], "%.3f", ImGuiInputTextFlags_None);

	inBuilder.EndInputPin();

	ImGui::PopItemWidth();

	inBuilder.BeginOutputPin();
	ImGui::Indent(node_width);
	ImGui::Text("Out");
	inBuilder.EndOutputPin();

	inBuilder.EndNode();
}


String VectorOpShaderNode::GenerateCode(ShaderGraphBuilder& inBuilder)
{
	String code = ShaderNode::GenerateCode(inBuilder);

	const int nr_of_floats = m_VectorComponents[m_VectorVariant];

	m_OutputPins[0].SetOutVariableName(std::format("VectorOp_Out{}", inBuilder.IncrLineNumber()));

	code += std::format("float{} {} = float{}({:.3f});", nr_of_floats, m_OutputPins[0].GetOutVariableName(), nr_of_floats, 1.0f);

	return code;
}


void SingleOutputShaderNode::DrawImNode(ShaderGraphBuilder& inBuilder)
{
	inBuilder.BeginNode(GetTitle());
	inBuilder.BeginOutputPin();
	ImGui::Indent(ImGui::CalcTextSize(GetTitle()).x);
	ImGui::Text("Out");
	inBuilder.EndOutputPin();
	inBuilder.EndNode();
}


String SingleOutputShaderNode::GenerateCode(ShaderGraphBuilder& inBuilder)
{
	m_OutputPin.SetOutVariableName(GetOutVariableName());
	return "";
}


void PixelShaderOutputShaderNode::DrawImNode(ShaderGraphBuilder& inBuilder)
{
	inBuilder.BeginNode("Pixel Shader Output");

	const float node_width = ImGui::CalcTextSize("0.0000,  0.0000,  0.0000,  0.0000").x;
	ImGui::PushItemWidth(node_width);

	for (const auto& [index, name] : gEnumerate(m_InputNames))
	{
		inBuilder.BeginInputPin();
		ImGui::Text(name);
		inBuilder.EndInputPin();
	}

	ImGui::PopItemWidth();
	inBuilder.EndNode();
}


String PixelShaderOutputShaderNode::GenerateCode(ShaderGraphBuilder& inBuilder)
{
	String code = ShaderNode::GenerateCode(inBuilder);

	for (const auto& [index, pin] : gEnumerate(m_InputPins))
	{
		if (pin.IsConnected())
		{
			ShaderNode* shader_node = inBuilder.GetShaderNode(pin.GetConnectedNode());

			code += std::format("Pack{}({}, packed);\n", m_InputNames[index], shader_node->GetOutputPin(0)->GetOutVariableName());
		}
		else
		{
			// TODO
		}
	}

	return code;
}

} // raekor