#include "pch.h"
#include "shadernodes.h"
#include "gui.h"
#include "iter.h"
#include "shader.h"
#include "member.h"

namespace RK {

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

	RTTI_DEFINE_MEMBER(FloatFunctionShaderNode, SERIALIZE_ALL, "Function", m_Function);
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

RTTI_DEFINE_TYPE(GetPositionShaderNode)
{
	RTTI_DEFINE_TYPE_INHERITANCE(GetPositionShaderNode, SingleOutputShaderNode);
}

RTTI_DEFINE_TYPE(GetNormalShaderNode)
{
	RTTI_DEFINE_TYPE_INHERITANCE(GetNormalShaderNode, SingleOutputShaderNode);
}

RTTI_DEFINE_TYPE(GetTexCoordShaderNode)
{
	RTTI_DEFINE_TYPE_INHERITANCE(GetTexCoordShaderNode, SingleOutputShaderNode);
}

RTTI_DEFINE_TYPE(GetTangentShaderNode)
{
	RTTI_DEFINE_TYPE_INHERITANCE(GetTangentShaderNode, SingleOutputShaderNode);
}

RTTI_DEFINE_TYPE(GetDeltaTimeShaderNode) 
{ 
	RTTI_DEFINE_TYPE_INHERITANCE(GetDeltaTimeShaderNode, SingleOutputShaderNode);
}

RTTI_DEFINE_TYPE(GetPixelCoordShaderNode)
{
	RTTI_DEFINE_TYPE_INHERITANCE(GetPixelCoordShaderNode, SingleOutputShaderNode);
}

RTTI_DEFINE_TYPE(GetTexCoordinateShaderNode)
{
	RTTI_DEFINE_TYPE_INHERITANCE(GetTexCoordinateShaderNode, SingleOutputShaderNode);
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
	RTTI_DEFINE_MEMBER(ProcedureShaderNode, SERIALIZE_ALL, "Show Input Box", m_ShowInputBox);
	RTTI_DEFINE_MEMBER(ProcedureShaderNode, SERIALIZE_ALL, "Input Names", m_InputNames);
	RTTI_DEFINE_MEMBER(ProcedureShaderNode, SERIALIZE_ALL, "Output Names", m_OutputNames);
	RTTI_DEFINE_MEMBER(ProcedureShaderNode, SERIALIZE_ALL, "Input Pins", m_InputPins);
	RTTI_DEFINE_MEMBER(ProcedureShaderNode, SERIALIZE_ALL, "Output Pins", m_OutputPins);
}

RTTI_DEFINE_TYPE(GradientNoiseShaderNode)
{
	RTTI_DEFINE_TYPE_INHERITANCE(GradientNoiseShaderNode, ProcedureShaderNode);
}

RTTI_DEFINE_TYPE(VectorOpShaderNode) 
{ 
	RTTI_DEFINE_TYPE_INHERITANCE(VectorOpShaderNode, ShaderNode); 

	RTTI_DEFINE_MEMBER(VectorOpShaderNode, SERIALIZE_ALL, "Op", m_Op);
	RTTI_DEFINE_MEMBER(VectorOpShaderNode, SERIALIZE_ALL, "Vectors", m_Vectors);
	RTTI_DEFINE_MEMBER(VectorOpShaderNode, SERIALIZE_ALL, "Input Pins", m_InputPins);
	RTTI_DEFINE_MEMBER(VectorOpShaderNode, SERIALIZE_ALL, "Output Pins", m_OutputPins);
}

RTTI_DEFINE_TYPE(VectorValueShaderNode) 
{ 
	RTTI_DEFINE_TYPE_INHERITANCE(VectorValueShaderNode, ShaderNode); 

	RTTI_DEFINE_MEMBER(VectorValueShaderNode, SERIALIZE_ALL, "Vector", m_Vector);
	RTTI_DEFINE_MEMBER(VectorValueShaderNode, SERIALIZE_ALL, "Input Pins", m_InputPins);
	RTTI_DEFINE_MEMBER(VectorValueShaderNode, SERIALIZE_ALL, "Output Pins", m_OutputPins);
}

RTTI_DEFINE_TYPE(VectorFunctionShaderNode)
{
	RTTI_DEFINE_TYPE_INHERITANCE(VectorFunctionShaderNode, ShaderNode);

	RTTI_DEFINE_MEMBER(VectorFunctionShaderNode, SERIALIZE_ALL, "Function", m_Function);
	RTTI_DEFINE_MEMBER(VectorFunctionShaderNode, SERIALIZE_ALL, "Input Pin", m_InputPin);
	RTTI_DEFINE_MEMBER(VectorFunctionShaderNode, SERIALIZE_ALL, "Output Pin", m_OutputPin);
}

RTTI_DEFINE_TYPE(VectorSplitShaderNode)
{
	RTTI_DEFINE_TYPE_INHERITANCE(VectorSplitShaderNode, ShaderNode);

	RTTI_DEFINE_MEMBER(VectorSplitShaderNode, SERIALIZE_ALL, "Input Pin", m_InputPin);
	RTTI_DEFINE_MEMBER(VectorSplitShaderNode, SERIALIZE_ALL, "Output Pins", m_OutputPins);
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

RTTI_DEFINE_TYPE(VertexShaderOutputShaderNode)
{
	RTTI_DEFINE_TYPE_INHERITANCE(VertexShaderOutputShaderNode, ShaderNode);

	RTTI_DEFINE_MEMBER(VertexShaderOutputShaderNode, SERIALIZE_ALL, "Position", m_Position);
	RTTI_DEFINE_MEMBER(VertexShaderOutputShaderNode, SERIALIZE_ALL, "Texcoord", m_Texcoord);
	RTTI_DEFINE_MEMBER(VertexShaderOutputShaderNode, SERIALIZE_ALL, "Normal", m_Normal);
	RTTI_DEFINE_MEMBER(VertexShaderOutputShaderNode, SERIALIZE_ALL, "Tangent", m_Tangent);
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
	g_RTTIFactory.Register(RTTI_OF(GetPositionShaderNode));
	g_RTTIFactory.Register(RTTI_OF(GetTexCoordShaderNode));
	g_RTTIFactory.Register(RTTI_OF(GetNormalShaderNode));
	g_RTTIFactory.Register(RTTI_OF(GetTangentShaderNode));
	g_RTTIFactory.Register(RTTI_OF(GetDeltaTimeShaderNode));
	g_RTTIFactory.Register(RTTI_OF(GetPixelCoordShaderNode));
	g_RTTIFactory.Register(RTTI_OF(GetTexCoordinateShaderNode));
	g_RTTIFactory.Register(RTTI_OF(VectorValueShaderNode));
	g_RTTIFactory.Register(RTTI_OF(VectorFunctionShaderNode));
	g_RTTIFactory.Register(RTTI_OF(VectorSplitShaderNode));
	g_RTTIFactory.Register(RTTI_OF(VectorOpShaderNode));
	g_RTTIFactory.Register(RTTI_OF(ProcedureShaderNode));
	g_RTTIFactory.Register(RTTI_OF(GradientNoiseShaderNode));
	g_RTTIFactory.Register(RTTI_OF(CompareShaderNode));
	g_RTTIFactory.Register(RTTI_OF(PixelShaderOutputShaderNode));
	g_RTTIFactory.Register(RTTI_OF(VertexShaderOutputShaderNode));
}


void FloatOpShaderNode::DrawImNode(ShaderGraphBuilder& inBuilder)
{
	inBuilder.BeginNode(m_OpNames[m_Op], ShaderNode::sScalarColor);

	const float node_width = ImGui::CalcTextSize("Multiply").x * 1.5f;

	inBuilder.BeginOutputPin();
	ImGui::Indent(node_width);
	ImGui::Text("Out");
	inBuilder.EndOutputPin();

	ImGui::PushItemWidth(node_width);

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

	ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgActive));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));

	if (ImGui::BeginCombo("##mathopshadernodevariantop", m_OpNames[m_Op]))
	{
		for (const auto& [index, name] : gEnumerate(m_OpNames))
		{
			if (ImGui::Selectable(name, int(m_Op) == index))
				m_Op = Op(index);
		}

		ImGui::EndCombo();
	}

	ImGui::PopStyleColor(3);

	ImGui::PopItemWidth();

	inBuilder.EndNode();
}


String FloatOpShaderNode::GenerateCode(ShaderGraphBuilder& inBuilder)
{
	m_OutputPins[0].SetOutVariableName(std::format("FloatOp_Out{}", inBuilder.IncrLineNumber()));

	String lhs = std::format("{:.3f}", m_FloatA);
	String rhs = std::format("{:.3f}", m_FloatB);

	if (const ShaderNodePin* pin = inBuilder.GetIncomingPin(m_InputPins[0]))
		lhs = pin->GetOutVariableName();

	if (const ShaderNodePin* pin = inBuilder.GetIncomingPin(m_InputPins[1]))
		rhs = pin->GetOutVariableName();

	return std::format("float {} = float({} {} {});\n", m_OutputPins[0].GetOutVariableName(), lhs, m_OpSymbols[m_Op], rhs);
}


void FloatFunctionShaderNode::DrawImNode(ShaderGraphBuilder& ioBuilder)
{
	ioBuilder.BeginNode(m_FunctionNames[m_Function], ShaderNode::sScalarColor);

	const float node_width = ImGui::CalcTextSize("Saturate").x * 1.35f;

	ioBuilder.BeginInputPin();
	ImGui::Text("In");
	ioBuilder.EndInputPin();

	ImGui::SameLine();

	ioBuilder.BeginOutputPin();
	ImGui::Indent(node_width / 2.0f);
	ImGui::Text("Out");
	ioBuilder.EndOutputPin();

	ImGui::PushItemWidth(node_width);

	ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgActive));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));

	if (ImGui::BeginCombo("##FloatFunctions", m_FunctionNames[m_Function]))
	{
		for (const auto& [index, name] : gEnumerate(m_FunctionNames))
		{
			if (ImGui::Selectable(name, int(m_Function) == index))
				m_Function = EFunction(index);
		}

		ImGui::EndCombo();
	}

	ImGui::PopStyleColor(3);

	ImNodes::EndNode();
}


String FloatFunctionShaderNode::GenerateCode(ShaderGraphBuilder& inBuilder)
{
	const ShaderNodePin* prev_out_pin = inBuilder.GetIncomingPin(m_InputPin);

	String out_var = prev_out_pin ? String(prev_out_pin->GetOutVariableName()) : "1.0";

	m_OutputPin.SetOutVariableName(std::format("FloatFunction_Out{}", inBuilder.IncrLineNumber()));
	return std::format("float {} = {}({});\n", m_OutputPin.GetOutVariableName(), m_FunctionCode[m_Function], out_var);
}


void VectorValueShaderNode::DrawImNode(ShaderGraphBuilder& inBuilder)
{
	inBuilder.BeginNode("Vector", ShaderNode::sVectorColor);

	const float node_width = ImGui::CalcTextSize("0.0000, ").x;

	inBuilder.BeginOutputPin();
	ImGui::Indent(node_width);
	ImGui::Text("Out");
	inBuilder.EndOutputPin();

	ImGui::PushItemWidth(node_width * 1.25f);

	constexpr std::array xyzw_chars = { "X", "Y", "Z" };

	for (int i = 0; i < 3; i++)
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

	m_OutputPins[0].SetOutVariableName(std::format("VectorValue_Out{}", inBuilder.IncrLineNumber()));

	code += std::format("float3 {} = float3(", m_OutputPins[0].GetOutVariableName());

	for (int i = 0; i < 3; i++)
	{
		if (const ShaderNodePin* start_pin = inBuilder.GetIncomingPin(m_InputPins[i]))
		{
			code += start_pin->GetOutVariableName();
		}
		else
		{
			code += std::format("{:.3f}", m_Vector[i]);
		}

		if (i < 2)
			code += ", ";
	}

	code += ");\n";

	return code;
}


void VectorOpShaderNode::DrawImNode(ShaderGraphBuilder& inBuilder)
{
	inBuilder.BeginNode(m_FunctionNames[m_Op], ShaderNode::sVectorColor);

	const float node_width = ImGui::CalcTextSize("0.0000, ").x * 3;

	inBuilder.BeginOutputPin();
	ImGui::Indent(node_width);
	ImGui::Text("Out");
	inBuilder.EndOutputPin();

	ImGui::PushItemWidth(node_width);

	ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgActive));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));

	if (ImGui::BeginCombo("##vectorshadernodevariantop", m_FunctionNames[m_Op]))
	{
		for (const auto& [index, name] : gEnumerate(m_FunctionNames))
		{
			if (ImGui::Selectable(name, int(m_Op) == index))
				m_Op = Op(index);
		}

		ImGui::EndCombo();
	}

	ImGui::PopStyleColor(3);

	if (inBuilder.BeginInputPin())
		ImGui::Text("A");
	else
		ImGui::InputFloat3("A", glm::value_ptr(m_Vectors[0]), "%.3f", ImGuiInputTextFlags_None);

	inBuilder.EndInputPin();

	if (inBuilder.BeginInputPin())
		ImGui::Text("B");
	else
		ImGui::InputFloat3("B", glm::value_ptr(m_Vectors[1]), "%.3f", ImGuiInputTextFlags_None);

	inBuilder.EndInputPin();

	ImGui::PopItemWidth();

	inBuilder.EndNode();
}


String VectorOpShaderNode::GenerateCode(ShaderGraphBuilder& inBuilder)
{
	String code = ShaderNode::GenerateCode(inBuilder);

	m_OutputPins[0].SetOutVariableName(std::format("VectorOp_Out{}", inBuilder.IncrLineNumber()));

	StaticArray<String, 2> variables_code;

	for (int index = 0; index < variables_code.size(); index++)
	{
		Vec4& var_vec = m_Vectors[index];
		String& var_code = variables_code[index];
		ShaderNodePin& var_pin = m_InputPins[index];

		if (const ShaderNodePin* pin = inBuilder.GetIncomingPin(var_pin))
		{
			var_code = pin->GetOutVariableName();
		}
		else
		{
			var_code = std::format("{}({:.3f}, {:.3f}, {:.3f})", m_InputPins[0].GetKindTypeName(), var_vec[0], var_vec[1], var_vec[2]); 
		}
	}

	code += std::format("float3 {} = {} {} {};\n", m_OutputPins[0].GetOutVariableName(), variables_code[0], m_OpSymbols[m_Op], variables_code[1]);

	return code;
}


void VectorFunctionShaderNode::DrawImNode(ShaderGraphBuilder& ioBuilder)
{
	ioBuilder.BeginNode(m_FunctionNames[m_Function], ShaderNode::sVectorColor);

	const float node_width = ImGui::CalcTextSize("Normalize").x * 1.35f;

	ioBuilder.BeginInputPin();
	ImGui::Text("In");
	ioBuilder.EndInputPin();

	ImGui::SameLine();

	ioBuilder.BeginOutputPin();
	ImGui::Indent(node_width / 2.0f);
	ImGui::Text("Out");
	ioBuilder.EndOutputPin();

	ImGui::PushItemWidth(node_width);

	ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgActive));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));

	if (ImGui::BeginCombo("##VectorFunctions", m_FunctionNames[m_Function]))
	{
		for (const auto& [index, name] : gEnumerate(m_FunctionNames))
		{
			if (ImGui::Selectable(name, int(m_Function) == index))
				m_Function = EFunction(index);
		}

		ImGui::EndCombo();
	}

	ImGui::PopStyleColor(3);

	ImNodes::EndNode();
}


String VectorFunctionShaderNode::GenerateCode(ShaderGraphBuilder& inBuilder)
{
	String code = ShaderNode::GenerateCode(inBuilder);

	const ShaderNodePin* prev_out_pin = inBuilder.GetIncomingPin(m_InputPin);

	m_OutputPin.SetOutVariableName(std::format("VectorFunction_Out{}", inBuilder.IncrLineNumber()));
	code += std::format("{} {} = {}({});\n", prev_out_pin->GetKindTypeName(), m_OutputPin.GetOutVariableName(), m_FunctionCode[m_Function], prev_out_pin->GetOutVariableName());

	return code;
}


void VectorSplitShaderNode::DrawImNode(ShaderGraphBuilder& ioBuilder)
{
	ioBuilder.BeginNode("Split", ShaderNode::sVectorColor);

	const float node_width = ImGui::CalcTextSize("Split").x * 1.35f;

	for (int i = 0; i < 3; i++)
	{
		if (i == 1)
		{
			ioBuilder.BeginInputPin();
			ImGui::Text("In");
			ioBuilder.EndInputPin();

			ImGui::SameLine();
		}

		ioBuilder.BeginOutputPin();
		ImGui::Text(m_VariableNames[i]);
		ioBuilder.EndOutputPin();
	}

	ioBuilder.EndNode();
}


String VectorSplitShaderNode::GenerateCode(ShaderGraphBuilder& ioBuilder)
{
	if (const ShaderNodePin* incoming_pin = ioBuilder.GetIncomingPin(m_InputPin))
	{
		m_OutputPins[0].SetOutVariableName(std::format("{}.x", incoming_pin->GetOutVariableName()));
		m_OutputPins[1].SetOutVariableName(std::format("{}.y", incoming_pin->GetOutVariableName()));
		m_OutputPins[2].SetOutVariableName(std::format("{}.z", incoming_pin->GetOutVariableName()));
	}

	return ""; // does not emit any new code directly
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
		if (const ShaderNodePin* incoming_pin = inBuilder.GetIncomingPin(input_pin))
		{
			if (strcmp(m_InputNames[index], "Discard") == 0)
			{
				code += std::format("Discard({});\n", incoming_pin->GetOutVariableName());
			}
			else
			{
				code += std::format("Pack{}({}, packed);\n", m_InputNames[index], incoming_pin->GetOutVariableName());
			}
		}
		else
		{
			// TODO
		}
	}

	return code;
}


void VertexShaderOutputShaderNode::DrawImNode(ShaderGraphBuilder& inBuilder)
{
	inBuilder.BeginNode("Vertex Shader Output", ShaderNode::sVertexShaderColor);

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


String VertexShaderOutputShaderNode::GenerateCode(ShaderGraphBuilder& inBuilder)
{
	String code = ShaderNode::GenerateCode(inBuilder);

	for (const auto& [index, input_pin] : gEnumerate(m_InputPins))
	{
		if (const ShaderNodePin* incoming_pin = inBuilder.GetIncomingPin(input_pin))
		{
			code += std::format("vertex.{} = {};\n", m_MemberNames[index], incoming_pin->GetOutVariableName());
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
	const float combo_box_width = ImGui::CalcTextSize("float4").x * 1.75f;
	const float input_text_width = text_box_width / 4.0f;

	if (ImGui::Button("Add Input +"))
	{
		m_InputPins.emplace_back();
		m_InputNames.emplace_back();
	}

	ImGui::SameLine(text_box_width - ImGui::CalcTextSize(" Add Input + ").x - ImNodes::GetStyle().NodePadding.x);

	if (ImGui::Button("Add Output +"))
	{
		m_OutputPins.emplace_back();
		m_OutputNames.emplace_back();
	}

	ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgActive));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));

	int nr_of_pins = std::max(m_InputPins.size(), m_OutputPins.size());

	for (int i = 0; i < nr_of_pins; i++)
	{
		if (i < m_InputPins.size())
		{
			ShaderNodePin& input_pin = m_InputPins[i];

			inBuilder.BeginInputPin();
			
			ImGui::SetNextItemWidth(combo_box_width);

			if (ImGui::BeginCombo("##procinputpinkindcombo", input_pin.GetKindName().data()))
			{
				for (int i = 0; i < ShaderNodePin::EKind::COUNT; i++)
				{
					const char* kind_name = ShaderNodePin::sKindNames[i];
					ShaderNodePin::EKind kind = ShaderNodePin::EKind(i);

					if (ImGui::Selectable(kind_name, kind == input_pin.GetKind()))
						input_pin.m_Kind = kind;
				}

				ImGui::EndCombo();
			}

			ImGui::SameLine();

			ImGui::SetNextItemWidth(input_text_width);
			ImGui::InputText("##procinputtextinputpin", &m_InputNames[i]);
			
			inBuilder.EndInputPin();

			if (i < m_OutputPins.size())
				ImGui::SameLine();
		}

		if (i < m_OutputPins.size())
		{
			ShaderNodePin& output_pin = m_OutputPins[i];

			inBuilder.BeginOutputPin();

			if (i < m_InputPins.size())
				ImGui::Indent(text_box_width - combo_box_width - input_text_width - combo_box_width - input_text_width - ImNodes::GetStyle().NodePadding.x * 4.0f);
			else
				ImGui::Indent(text_box_width - combo_box_width - input_text_width - ImNodes::GetStyle().NodePadding.x * 2.0f);

			ImGui::SetNextItemWidth(input_text_width);
			ImGui::InputText("##procinputtextoutputpin", &m_OutputNames[i]);

			ImGui::SameLine();
			
			ImGui::SetNextItemWidth(combo_box_width);

			if (ImGui::BeginCombo("##procinputpinkindcombo", output_pin.GetKindName().data()))
			{
				for (int i = 0; i < ShaderNodePin::EKind::COUNT; i++)
				{
					const char* kind_name = ShaderNodePin::sKindNames[i];
					ShaderNodePin::EKind kind = ShaderNodePin::EKind(i);

					if (ImGui::Selectable(kind_name, kind == output_pin.GetKind()))
						output_pin.m_Kind = kind;
				}

				ImGui::EndCombo();
			}

			inBuilder.EndOutputPin();
		}
	}

	ImGui::PopStyleColor(3);

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
	// build templated function definition
	int param_count = 0;
	String function_params;

	// add 'in' parameters
	for (const auto& [index, node_pin] : gEnumerate(m_InputPins))
	{
		function_params += std::format("{} {}, ", node_pin.GetKindTypeName(), m_InputNames[index]);
	}

	// add 'out' parameters
	for (const auto& [index, node_pin] : gEnumerate(m_OutputPins))
	{
		function_params += std::format("out {} {}, ", node_pin.GetKindTypeName(), m_OutputNames[index]);
	}

	// fix trailing comma's
	function_params = function_params.substr(0, std::max(function_params.size() - 2, 0ull));

	String function;
	uint32_t function_line_nr = inBuilder.GetLineNumber();
	function += std::format("void Procedure{}({})\n", function_line_nr, function_params);
	function += std::format("{{\n {} \n}}", m_Procedure);

	inBuilder.AddFunction(function);

	String code;

	// declare output variables
	for (const auto& [index, node_pin] : gEnumerate(m_OutputPins))
	{
		node_pin.SetOutVariableName(std::format("{}{}", m_OutputNames[index], inBuilder.IncrLineNumber()));
		code += std::format("{} {};\n", node_pin.GetKindTypeName(), node_pin.GetOutVariableName());
	}

	// build function call arguments
	String function_call_args;

	// add 'in' call args
	for (const ShaderNodePin& node_pin : m_InputPins)
	{
		if (const ShaderNodePin* output_pin = inBuilder.GetIncomingPin(node_pin))
		{
			function_call_args += std::format("{}, ", output_pin->GetOutVariableName());
		}
	}

	// add 'out' call args
	for (const ShaderNodePin& node_pin : m_OutputPins)
	{
		function_call_args += std::format("{}, ", node_pin.GetOutVariableName());
	}

	function_call_args = function_call_args.substr(0, std::max(function_call_args.size() - 2, 0ull));
	
	return code += std::format("Procedure{}({});\n", function_line_nr, function_call_args);
}


GradientNoiseShaderNode::GradientNoiseShaderNode()
{
	m_Title = "Gradient Noise";
	AddInputVariable("inUV", ShaderNodePin::VECTOR);
	AddOutputVariable("outNoise", ShaderNodePin::FLOAT);
	m_Procedure = std::format("{} = GradientNoise({}.xy);", m_OutputNames[0], m_InputNames[0]);
	m_ShowInputBox = false;
}


void CompareShaderNode::DrawImNode(ShaderGraphBuilder& inBuilder)
{
	if (m_InputPins[0].GetKind() == m_InputPins[1].GetKind())
		inBuilder.BeginNode(m_OpNames[m_Op], m_InputPins[0].GetColor());
	else
		inBuilder.BeginNode(m_OpNames[m_Op]);

	const float node_width = ImGui::CalcTextSize("Greater Equal").x * 1.5f;

	inBuilder.BeginOutputPin();
	ImGui::Indent(node_width);
	ImGui::Text("Out");
	inBuilder.EndOutputPin();

	ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgActive));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));

	ImGui::PushItemWidth(node_width);

	if (ImGui::BeginCombo("##compareshadernodeop", m_OpNames[m_Op]))
	{
		for (const auto& [index, name] : gEnumerate(m_OpSymbols))
		{
			if (ImGui::Selectable(name, int(m_Op) == index))
				m_Op = Op(index);
		}

		ImGui::EndCombo();
	}

	ImGui::PopStyleColor(3);

	inBuilder.BeginInputPin();
	ImGui::Text("A");
	inBuilder.EndInputPin();

	inBuilder.BeginInputPin();
	ImGui::Text("B");
	inBuilder.EndInputPin();

	inBuilder.EndNode();
}


String CompareShaderNode::GenerateCode(ShaderGraphBuilder& inBuilder)
{
	m_OutputPin.SetOutVariableName(std::format("Compare_Out{}", inBuilder.IncrLineNumber()));

	StringView name_a = inBuilder.GetIncomingPin(m_InputPins[0])->GetOutVariableName();
	StringView name_b = inBuilder.GetIncomingPin(m_InputPins[1])->GetOutVariableName();

	return std::format("bool {} = {} {} {};\n", m_OutputPin.GetOutVariableName(), name_a, m_OpSymbols[m_Op], name_b);
}


void FloatValueShaderNode::DrawImNode(ShaderGraphBuilder& inBuilder)
{
	/*if (inBuilder.GetShowNodeIndices())
		inBuilder.BeginNode("Float");
	else*/
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