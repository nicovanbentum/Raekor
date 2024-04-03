#include "pch.h"
#include "shadernodes.h"
#include "gui.h"
#include "iter.h"
#include "shader.h"
#include "member.h"

namespace Raekor {

RTTI_DEFINE_TYPE(FloatValueShaderNode)
{
	RTTI_DEFINE_TYPE_INHERITANCE(FloatValueShaderNode, ShaderNode);

	RTTI_DEFINE_MEMBER(FloatValueShaderNode, SERIALIZE_ALL, "Float", m_Float);
	RTTI_DEFINE_MEMBER(FloatValueShaderNode, SERIALIZE_ALL, "Output Pin", m_OutputPin);
}

RTTI_DEFINE_TYPE(FloatOpShaderNode) 
{ 
	RTTI_DEFINE_TYPE_INHERITANCE(FloatOpShaderNode, ShaderNode); 

	RTTI_DEFINE_MEMBER(FloatOpShaderNode, SERIALIZE_ALL, "Op", m_Op);
	RTTI_DEFINE_MEMBER(FloatOpShaderNode, SERIALIZE_ALL, "FloatA", m_FloatA);
	RTTI_DEFINE_MEMBER(FloatOpShaderNode, SERIALIZE_ALL, "FloatB", m_FloatB);
	RTTI_DEFINE_MEMBER(FloatOpShaderNode, SERIALIZE_ALL, "Input Pins", m_InputPins);
	RTTI_DEFINE_MEMBER(FloatOpShaderNode, SERIALIZE_ALL, "Output Pins", m_OutputPins);
}

RTTI_DEFINE_TYPE(FloatFunctionShaderNode) 
{ 
	RTTI_DEFINE_TYPE_INHERITANCE(FloatFunctionShaderNode, ShaderNode); 

	RTTI_DEFINE_MEMBER(FloatFunctionShaderNode, SERIALIZE_ALL, "Op", m_Op);
	RTTI_DEFINE_MEMBER(FloatFunctionShaderNode, SERIALIZE_ALL, "Input Pin", m_InputPin);
	RTTI_DEFINE_MEMBER(FloatFunctionShaderNode, SERIALIZE_ALL, "Output Pin", m_OutputPin);
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

RTTI_DEFINE_TYPE(GetPixelCoordShaderNode)
{
	RTTI_DEFINE_TYPE_INHERITANCE(GetPixelCoordShaderNode, SingleOutputShaderNode);
}

RTTI_DEFINE_TYPE(CompareShaderNode)
{
	RTTI_DEFINE_TYPE_INHERITANCE(CompareShaderNode, ShaderNode);

	RTTI_DEFINE_MEMBER(CompareShaderNode, SERIALIZE_ALL, "Op", m_Op);
	RTTI_DEFINE_MEMBER(CompareShaderNode, SERIALIZE_ALL, "Input Pins", m_InputPins);
	RTTI_DEFINE_MEMBER(CompareShaderNode, SERIALIZE_ALL, "Output Pin", m_OutputPin);
}

RTTI_DEFINE_TYPE(ProcedureShaderNode)
{
	RTTI_DEFINE_TYPE_INHERITANCE(ProcedureShaderNode, ShaderNode);

	RTTI_DEFINE_MEMBER(ProcedureShaderNode, SERIALIZE_ALL, "Procedure", m_Procedure);
	RTTI_DEFINE_MEMBER(ProcedureShaderNode, SERIALIZE_ALL, "Input Pins", m_InputPins);
	RTTI_DEFINE_MEMBER(ProcedureShaderNode, SERIALIZE_ALL, "Output Pins", m_OutputPins);
}

RTTI_DEFINE_TYPE(VectorOpShaderNode) 
{ 
	RTTI_DEFINE_TYPE_INHERITANCE(VectorOpShaderNode, ShaderNode); 

	RTTI_DEFINE_MEMBER(VectorOpShaderNode, SERIALIZE_ALL, "Op", m_Op);
	RTTI_DEFINE_MEMBER(VectorOpShaderNode, SERIALIZE_ALL, "Kind", m_Kind);
	RTTI_DEFINE_MEMBER(VectorOpShaderNode, SERIALIZE_ALL, "VectorA", m_VectorA);
	RTTI_DEFINE_MEMBER(VectorOpShaderNode, SERIALIZE_ALL, "VectorB", m_VectorB);
	RTTI_DEFINE_MEMBER(VectorOpShaderNode, SERIALIZE_ALL, "Input Pins", m_InputPins);
	RTTI_DEFINE_MEMBER(VectorOpShaderNode, SERIALIZE_ALL, "Output Pins", m_OutputPins);
}

RTTI_DEFINE_TYPE(VectorValueShaderNode) 
{ 
	RTTI_DEFINE_TYPE_INHERITANCE(VectorValueShaderNode, ShaderNode); 

	RTTI_DEFINE_MEMBER(VectorValueShaderNode, SERIALIZE_ALL, "Kind", m_Kind);
	RTTI_DEFINE_MEMBER(VectorValueShaderNode, SERIALIZE_ALL, "Vector", m_Vector);
	RTTI_DEFINE_MEMBER(VectorValueShaderNode, SERIALIZE_ALL, "Input Pins", m_InputPins);
	RTTI_DEFINE_MEMBER(VectorValueShaderNode, SERIALIZE_ALL, "Output Pins", m_OutputPins);
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

	g_RTTIFactory.Register(RTTI_OF(FloatValueShaderNode));
	g_RTTIFactory.Register(RTTI_OF(FloatOpShaderNode));
	g_RTTIFactory.Register(RTTI_OF(FloatFunctionShaderNode));
	g_RTTIFactory.Register(RTTI_OF(GetTimeShaderNode));
	g_RTTIFactory.Register(RTTI_OF(GetDeltaTimeShaderNode));
	g_RTTIFactory.Register(RTTI_OF(GetPixelCoordShaderNode));
	g_RTTIFactory.Register(RTTI_OF(VectorValueShaderNode));
	g_RTTIFactory.Register(RTTI_OF(VectorOpShaderNode));
	g_RTTIFactory.Register(RTTI_OF(ProcedureShaderNode));
	g_RTTIFactory.Register(RTTI_OF(CompareShaderNode));
	g_RTTIFactory.Register(RTTI_OF(PixelShaderOutputShaderNode));
}


void FloatOpShaderNode::DrawImNode(ShaderGraphBuilder& inBuilder)
{
	inBuilder.BeginNode("FloatOp", ShaderNode::sScalarColor);

	const float node_width = ImGui::CalcTextSize("Multiply").x * 1.5f;

	inBuilder.BeginOutputPin();
	ImGui::Indent(node_width);
	ImGui::Text("Out");
	inBuilder.EndOutputPin();

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


	inBuilder.EndNode();
}


String FloatOpShaderNode::GenerateCode(ShaderGraphBuilder& inBuilder)
{
	String code = ShaderNode::GenerateCode(inBuilder);

	m_OutputPins[0].SetOutVariableName(std::format("MathOp_Out{}", inBuilder.IncrLineNumber()));

	String lhs = m_InputPins[0].IsConnected() ? String(inBuilder.GetConnectedOutputPin(m_InputPins[0])->GetOutVariableName()) : std::format("{:.3f}", m_FloatA);
	String rhs = m_InputPins[1].IsConnected() ? String(inBuilder.GetConnectedOutputPin(m_InputPins[1])->GetOutVariableName()) : std::format("{:.3f}", m_FloatB);

	code += std::format("float {} = float({} {} {});\n", m_OutputPins[0].GetOutVariableName(), lhs, m_OpSymbols[m_Op], rhs);

	return code;
}


void FloatFunctionShaderNode::DrawImNode(ShaderGraphBuilder& ioBuilder)
{
	ioBuilder.BeginNode("FloatFunc", ShaderNode::sScalarColor);

	const float node_width = ImGui::CalcTextSize(m_OpNames[m_Op]).x * 4.0f;

	ioBuilder.BeginInputPin();
	ImGui::Text("In");
	ioBuilder.EndInputPin();

	ImGui::SameLine();

	ioBuilder.BeginOutputPin();
	ImGui::Indent(node_width / 2.0f);
	ImGui::Text("Out");
	ioBuilder.EndOutputPin();

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

	ImNodes::EndNode();
}


String FloatFunctionShaderNode::GenerateCode(ShaderGraphBuilder& inBuilder)
{
	String code = ShaderNode::GenerateCode(inBuilder);

	ShaderNode* prev_out_node = inBuilder.GetShaderNode(m_InputPin.GetConnectedNode());
	ShaderNodePin* prev_out_pin = prev_out_node->GetOutputPin(m_InputPin.GetConnectedPin());

	m_OutputPin.SetOutVariableName(std::format("MathOp_Out{}", inBuilder.IncrLineNumber()));
	code += std::format("float {} = {}({});\n", m_OutputPin.GetOutVariableName(), m_OpNames[m_Op], prev_out_pin->GetOutVariableName());

	return code;
}


void VectorValueShaderNode::DrawImNode(ShaderGraphBuilder& inBuilder)
{
	inBuilder.BeginNode("Vector", ShaderNode::sVectorColor);

	const float node_width = ImGui::CalcTextSize("0.0000, ").x;

	inBuilder.BeginOutputPin();
	ImGui::Indent(node_width);
	ImGui::Text("Out");
	inBuilder.EndOutputPin();

	ImGui::PushItemWidth(node_width * 1.5f);

	if (ImGui::BeginCombo("##vectorshadernodevarianttype", GetVectorName()))
	{
		for (const auto& [index, name] : gEnumerate(m_VectorNames))
		{
			const auto name = ShaderNodePin::sKindNames[ShaderNodePin::EKind(index)];
			if (ImGui::Selectable(name, name == GetVectorName()))
				m_Kind = m_VectorKinds[index];
		}

		ImGui::EndCombo();
	}

	ImGui::PopItemWidth();
	ImGui::PushItemWidth(node_width * 1.25f);

	auto InputFloatFunction = m_VectorInputFunctions[int(m_Kind) - ShaderNodePin::FLOAT2];
	constexpr std::array xyzw_chars = { "X", "Y", "Z", "W" };

	for (int i = 0; i < m_VectorComponents[int(m_Kind) - ShaderNodePin::FLOAT2]; i++)
	{
		if (inBuilder.BeginInputPin())
			ImGui::Text(xyzw_chars[i]);
		else
			ImGui::InputFloat(xyzw_chars[i], &m_Vector[i]);

		inBuilder.EndInputPin();
	}

	ImGui::PopItemWidth();

	inBuilder.EndNode();
}


String VectorValueShaderNode::GenerateCode(ShaderGraphBuilder& inBuilder)
{
	String code = ShaderNode::GenerateCode(inBuilder);

	const int nr_of_floats = GetVectorComponents();

	m_OutputPins[0].SetOutVariableName(std::format("VectorCompose_Out{}", inBuilder.IncrLineNumber()));

	switch (m_Kind)
	{
		case ShaderNodePin::FLOAT2:
			code += std::format("float{} {} = float{}(", nr_of_floats, m_OutputPins[0].GetOutVariableName(), nr_of_floats);
			break;
		case ShaderNodePin::FLOAT3:
			code += std::format("float{} {} = float{}(", nr_of_floats, m_OutputPins[0].GetOutVariableName(), nr_of_floats);
			break;
		case ShaderNodePin::FLOAT4:
			code += std::format("float{} {} = float{}(", nr_of_floats, m_OutputPins[0].GetOutVariableName(), nr_of_floats);
			break;
		default:
			assert(false);
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
	inBuilder.BeginNode("VectorOp", ShaderNode::sVectorColor);

	const float node_width = ImGui::CalcTextSize("0.0000, ").x * GetVectorComponents();

	inBuilder.BeginOutputPin();
	ImGui::Indent(node_width);
	ImGui::Text("Out");
	inBuilder.EndOutputPin();

	ImGui::PushItemWidth(node_width);

	constexpr static std::array operation_names = { "Add", "Minus", "Multiply", "Divide" };

	if (ImGui::BeginCombo("##vectorshadernodevarianttype", GetVectorName()))
	{
		for (const auto& [index, name] : gEnumerate(m_VectorNames))
		{
			const auto name = ShaderNodePin::sKindNames[ShaderNodePin::EKind(index)];
			if (ImGui::Selectable(name, name == GetVectorName()))
				m_Kind = m_VectorKinds[index];
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

	auto InputFloatFunction = GetVectorInputFunction();

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

	inBuilder.EndNode();
}


String VectorOpShaderNode::GenerateCode(ShaderGraphBuilder& inBuilder)
{
	String code = ShaderNode::GenerateCode(inBuilder);

	const int nr_of_floats = GetVectorComponents();

	m_OutputPins[0].SetOutVariableName(std::format("VectorOp_Out{}", inBuilder.IncrLineNumber()));

	String lhs = m_InputPins[0].IsConnected() ? String(inBuilder.GetConnectedOutputPin(m_InputPins[0])->GetOutVariableName()) : "";
	String rhs = m_InputPins[1].IsConnected() ? String(inBuilder.GetConnectedOutputPin(m_InputPins[1])->GetOutVariableName()) : "";

	if (!m_InputPins[0].IsConnected())
	{
		switch (m_Kind)
		{
			case ShaderNodePin::FLOAT2: lhs = std::format("{}({:.3f}, {:.3f})", m_InputPins[0].GetKindName(), m_VectorA[0], m_VectorA[1]); break;
			case ShaderNodePin::FLOAT3: lhs = std::format("{}({:.3f}, {:.3f}, {:.3f})", m_InputPins[0].GetKindName(), m_VectorA[0], m_VectorA[1], m_VectorA[2]); break;
			case ShaderNodePin::FLOAT4: lhs = std::format("{}({:.3f}, {:.3f}, {:.3f}, {:.3f})", m_InputPins[0].GetKindName(), m_VectorA[0], m_VectorA[1], m_VectorA[2], m_VectorA[3]); break;
		}
	}

	if (!m_InputPins[1].IsConnected())
	{
		switch (m_Kind)
		{
			case ShaderNodePin::FLOAT2: rhs = std::format("{}({:.3f}, {:.3f})", m_InputPins[1].GetKindName(), m_VectorB[0], m_VectorB[1]); break;
			case ShaderNodePin::FLOAT3: rhs = std::format("{}({:.3f}, {:.3f}, {:.3f})", m_InputPins[1].GetKindName(), m_VectorB[0], m_VectorB[1], m_VectorB[2]); break;
			case ShaderNodePin::FLOAT4: rhs = std::format("{}({:.3f}, {:.3f}, {:.3f}, {:.3f})", m_InputPins[1].GetKindName(), m_VectorB[0], m_VectorB[1], m_VectorB[2], m_VectorB[3]); break;
		}
	}

	code += std::format("float{} {} = {} {} {};\n", nr_of_floats, m_OutputPins[0].GetOutVariableName(), lhs, m_OpSymbols[m_Op], rhs);

	return code;
}


void SingleOutputShaderNode::DrawImNode(ShaderGraphBuilder& inBuilder)
{
	inBuilder.BeginNode(m_Title, m_OutputPin.GetColor());
	inBuilder.BeginOutputPin();
	ImGui::Indent(ImGui::CalcTextSize(m_Title.c_str()).x);
	ImGui::Text(m_OutputPin.GetKindName().data());
	inBuilder.EndOutputPin();
	inBuilder.EndNode();
}


String SingleOutputShaderNode::GenerateCode(ShaderGraphBuilder& inBuilder)
{
	m_OutputPin.SetOutVariableName(m_OutVariableName);
	return "";
}


void PixelShaderOutputShaderNode::DrawImNode(ShaderGraphBuilder& inBuilder)
{
	inBuilder.BeginNode("Pixel Shader Output", ShaderNode::sPixelShaderColor);

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

	for (const auto& [index, input_pin] : gEnumerate(m_InputPins))
	{
		if (input_pin.IsConnected())
		{
			if (strcmp(m_InputNames[index], "Discard") == 0)
			{
				ShaderNode* shader_node = inBuilder.GetShaderNode(input_pin.GetConnectedNode());
				code += std::format("Discard({});\n", shader_node->GetOutputPin(input_pin.GetConnectedPin())->GetOutVariableName());
			}
			else
			{
				ShaderNode* shader_node = inBuilder.GetShaderNode(input_pin.GetConnectedNode());
				code += std::format("Pack{}({}, packed);\n", m_InputNames[index], shader_node->GetOutputPin(0)->GetOutVariableName());
			}
		}
		else
		{
			// TODO
		}
	}

	return code;
}


void ProcedureShaderNode::DrawImNode(ShaderGraphBuilder& inBuilder)
{
	inBuilder.BeginNode(m_Title, ShaderNode::sTextureColor);

	const float text_box_width = ImGui::GetWindowContentRegionWidth() / 6.0f;

	if (ImGui::Button("Add Input +"))
		m_InputPins.emplace_back();

	ImGui::SameLine(text_box_width - ImGui::CalcTextSize(" Add Input + ").x);

	if (ImGui::Button("Add Output +"))
		m_OutputPins.emplace_back();

	int nr_of_pins = std::max(m_InputPins.size(), m_OutputPins.size());

	for (int i = 0; i < nr_of_pins; i++)
	{
		if (i < m_InputPins.size())
		{
			inBuilder.BeginInputPin();
			ImGui::SetNextItemWidth(text_box_width / 4.0f);
			ImGui::InputText("##expressioninputpin", &m_InputPins[i].m_VariableName);
			inBuilder.EndInputPin();

			if (i < m_OutputPins.size())
				ImGui::SameLine(text_box_width - (text_box_width / 4.0f ));
		}

		if (i < m_OutputPins.size())
		{
			if (i >= m_InputPins.size())
				ImGui::Indent(text_box_width - ( text_box_width / 4.0f ));

			inBuilder.BeginOutputPin();
			//ImGui::Indent(text_box_width - (text_box_width / 4.0f));
			ImGui::SetNextItemWidth(text_box_width / 4.0f);
			ImGui::InputText("##expressionoutputpin", &m_OutputPins[i].m_VariableName);
			inBuilder.EndOutputPin();
		}
	}

	if (m_ShowInputBox)
	{
		ImGui::SetNextItemWidth(text_box_width);
		ImGui::InputTextMultiline("##expression", &m_Procedure);
	}

	ImGui::SetNextItemWidth(text_box_width);

	if (ImGui::Button(m_ShowInputBox ? "hide" : "show", ImVec2(text_box_width, ImGui::GetTextLineHeight())))
		m_ShowInputBox = !m_ShowInputBox;


	inBuilder.EndNode();
}


String ProcedureShaderNode::GenerateCode(ShaderGraphBuilder& inBuilder)
{
	String code = ShaderNode::GenerateCode(inBuilder);

	// build templated function definition
	int param_count = 0;
	String function_params;

	// add 'in' parameters
	for (const ShaderNodePin& node_pin : m_InputPins)
	{
		if (node_pin.IsConnected())
		{
			ShaderNodePin* output_pin = inBuilder.GetConnectedOutputPin(node_pin);
			function_params += std::format("{} {}, ", output_pin->GetKindName(), node_pin.GetOutVariableName());
		}
	}

	// add 'out' parameters
	for (const ShaderNodePin& node_pin : m_OutputPins)
	{
		if (node_pin.IsConnected())
		{
			ShaderNodePin* input_pin = inBuilder.GetConnectedInputPin(node_pin);
			function_params += std::format("out {} {}, ", input_pin->GetKindName(), node_pin.GetOutVariableName());
		}
	}

	// fix trailing comma's
	function_params = function_params.substr(0, std::max(function_params.size() - 2, 0ull));

	String function;
	function += std::format("void Procedure{}({})\n", inBuilder.GetLineNumber(), function_params);
	function += std::format("{{\n {} \n}}", m_Procedure);

	inBuilder.AddFunction(function);

	// declare output variables
	for (const ShaderNodePin& node_pin : m_OutputPins)
	{
		if (node_pin.IsConnected())
		{
			ShaderNodePin* input_pin = inBuilder.GetConnectedInputPin(node_pin);
			code += std::format("{} {};\n", input_pin->GetKindName(), node_pin.GetOutVariableName());
		}
	}

	// build function call arguments
	String function_call_args;

	// add 'in' call args
	for (const ShaderNodePin& node_pin : m_InputPins)
	{
		if (node_pin.IsConnected())
		{
			ShaderNodePin* output_pin = inBuilder.GetConnectedOutputPin(node_pin);
			function_call_args += std::format("{}, ", output_pin->GetOutVariableName());
		}
	}

	// add 'out' call args
	for (const ShaderNodePin& node_pin : m_OutputPins)
	{
		if (node_pin.IsConnected())
			function_call_args += std::format("{}, ", node_pin.GetOutVariableName());
	}

	function_call_args = function_call_args.substr(0, std::max(function_call_args.size() - 2, 0ull));
	
	return code += std::format("Procedure{}({});\n", inBuilder.GetLineNumber(), function_call_args);
}


void CompareShaderNode::DrawImNode(ShaderGraphBuilder& inBuilder)
{
	ImNodes::PushColorStyle(ImNodesCol_TitleBar, IM_COL32_BLACK);
	ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered, IM_COL32_BLACK);
	ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected, IM_COL32_BLACK);

	inBuilder.BeginNode();
	ImNodes::BeginNodeTitleBar();

	const float node_width = ImGui::CalcTextSize("======").x;
	ImGui::PushItemWidth(node_width);

	if (ImGui::BeginCombo("##compareshadernodeop", m_OpSymbols[m_Op]))
	{
		for (const auto& [index, name] : gEnumerate(m_OpSymbols))
		{
			if (ImGui::Selectable(name, int(m_Op) == index))
				m_Op = Op(index);
		}

		ImGui::EndCombo();
	}

	ImNodes::EndNodeTitleBar();

	ImNodes::PopColorStyle();
	ImNodes::PopColorStyle();
	ImNodes::PopColorStyle();

	inBuilder.BeginInputPin();
	ImGui::Text("LHS");
	inBuilder.EndInputPin();

	inBuilder.BeginInputPin();
	ImGui::Text("RHS");
	inBuilder.EndInputPin();

	ImGui::SameLine();

	ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetTextLineHeightWithSpacing() * 0.5f);
	inBuilder.BeginOutputPin();
	ImGui::Indent(node_width * 0.25f);
	ImGui::Text(m_OutputPin.GetKindName().data());
	inBuilder.EndOutputPin();

	inBuilder.EndNode();
}


String CompareShaderNode::GenerateCode(ShaderGraphBuilder& inBuilder)
{
	String code = ShaderNode::GenerateCode(inBuilder);

	m_OutputPin.SetOutVariableName(std::format("Compare_Out{}", inBuilder.IncrLineNumber()));

	StringView name_a = inBuilder.GetConnectedOutputPin(m_InputPins[0])->GetOutVariableName();
	StringView name_b = inBuilder.GetConnectedOutputPin(m_InputPins[1])->GetOutVariableName();

	code += std::format("bool {} = {} {} {};\n", m_OutputPin.GetOutVariableName(), name_a, m_OpSymbols[m_Op], name_b);

	return code;
}


void FloatValueShaderNode::DrawImNode(ShaderGraphBuilder& inBuilder)
{
	inBuilder.BeginNode();

	const float node_width = ImGui::CalcTextSize("0.0000, ").x;
	ImGui::PushItemWidth(node_width * 1.25f);

	inBuilder.BeginOutputPin();
	ImGui::InputFloat("##X", &m_Float);
	ImGui::SameLine();
	ImGui::Text("Out");
	inBuilder.EndOutputPin();

	inBuilder.EndNode();
}



String FloatValueShaderNode::GenerateCode(ShaderGraphBuilder& inBuilder)
{
	String code = ShaderNode::GenerateCode(inBuilder);

	m_OutputPin.SetOutVariableName(std::format("FloatValue_Out{}", inBuilder.IncrLineNumber()));
	code += std::format("float {} = {:.3f};\n", m_OutputPin.GetOutVariableName(), m_Float);

	return code;
}

} // raekor