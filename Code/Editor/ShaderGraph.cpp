#include "pch.h"
#include "ShaderGraph.h"
#include "ShaderGraphNodes.h"
#include "Engine/iter.h"
#include "Engine/member.h"

namespace RK {

RTTI_DEFINE_TYPE(ShaderNode) {}

RTTI_DEFINE_TYPE(ShaderGraphLink)
{
	RTTI_DEFINE_MEMBER(ShaderGraphLink, SERIALIZE_ALL, "From Node", fromNodeIndex);
	RTTI_DEFINE_MEMBER(ShaderGraphLink, SERIALIZE_ALL, "From Pin", fromPinIndex);
	RTTI_DEFINE_MEMBER(ShaderGraphLink, SERIALIZE_ALL, "To Node", toNodeIndex);
	RTTI_DEFINE_MEMBER(ShaderGraphLink, SERIALIZE_ALL, "To Pin", toPinIndex);
}

RTTI_DEFINE_TYPE(ShaderNodePin)
{
	RTTI_DEFINE_MEMBER(ShaderNodePin, SERIALIZE_ALL, "Kind", m_Kind);
	RTTI_DEFINE_MEMBER(ShaderNodePin, SERIALIZE_ALL, "Variable Name", m_VariableName);
}

RTTI_DEFINE_TYPE(ShaderGraphBuilder)
{
	RTTI_DEFINE_MEMBER(ShaderGraphBuilder, SERIALIZE_ALL, "Layout", m_Layout);
	RTTI_DEFINE_MEMBER(ShaderGraphBuilder, SERIALIZE_ALL, "Links", m_Links);
	RTTI_DEFINE_MEMBER(ShaderGraphBuilder, SERIALIZE_ALL, "Template File", m_TemplateFile);
	RTTI_DEFINE_MEMBER(ShaderGraphBuilder, SERIALIZE_ALL, "Output File", m_OutputFile);
}

bool ShaderNodePin::sIsCompatible(EKind inFrom, EKind inTo)
{
	if (inFrom == inTo || inFrom == ShaderNodePin::AUTO || inTo == ShaderNodePin::AUTO)
		return true;

	if (inFrom == ShaderNodePin::FLOAT && inTo == ShaderNodePin::VECTOR)
		return true;

	if (inFrom == ShaderNodePin::VECTOR && inTo == ShaderNodePin::FLOAT)
		return true;

	return false;
}


StringView ShaderNodePin::sGetDefaultValue(EKind inKind)
{
	switch (inKind)
	{
		case BOOL:   return "false";
		case VECTOR: return "float3(0.0, 0.0, 0.0)";
		default:     return "0.0";
	}
}


const ShaderNodePin* ShaderGraphBuilder::GetIncomingPin(const ShaderNodePin& inPin) const
{
	if (!inPin.HasLink() || inPin.GetLinkIndex() >= m_Links.size())
		return nullptr;

	const ShaderGraphLink& link = m_Links[inPin.GetLinkIndex()];
	const ShaderNode* node = GetShaderNode(link.fromNodeIndex);

	if (link.fromPinIndex >= node->GetOutputPins().size())
		return nullptr;

	return &node->GetOutputPin(link.fromPinIndex);
}


String ShaderGraphBuilder::GetIncomingValue(const ShaderNodePin& inPin, StringView inDefault) const
{
	if (const ShaderNodePin* pin = GetIncomingPin(inPin))
		return String(pin->GetOutVariableName());

	return String(inDefault);
}


bool ShaderGraphBuilder::AddLink(int inFromNode, int inFromPin, int inToNode, int inToPin)
{
	if (inFromNode == inToNode)
		return false;

	const ShaderNodePin& from_pin = GetShaderNode(inFromNode)->GetOutputPin(inFromPin);
	const ShaderNodePin& to_pin = GetShaderNode(inToNode)->GetInputPin(inToPin);

	if (!ShaderNodePin::sIsCompatible(from_pin.GetKind(), to_pin.GetKind()))
		return false;

	for (int index = int(m_Links.size()) - 1; index >= 0; index--)
	{
		if (m_Links[index].toNodeIndex == inToNode && m_Links[index].toPinIndex == inToPin)
			m_Links.erase(m_Links.begin() + index);
	}

	m_Links.push_back(ShaderGraphLink { .fromNodeIndex = inFromNode, .fromPinIndex = inFromPin, .toNodeIndex = inToNode, .toPinIndex = inToPin });
	UpdatePinLinks();

	return true;
}


void ShaderGraphBuilder::RemoveLink(int inIndex)
{
	if (inIndex < 0 || inIndex >= m_Links.size())
		return;

	m_Links.erase(m_Links.begin() + inIndex);
	UpdatePinLinks();
}


void ShaderGraphBuilder::UpdatePinLinks()
{
	for (const SharedPtr<ShaderNode>& node : m_ShaderNodes)
	{
		for (ShaderNodePin& pin : node->GetInputPins())
			pin.SetLinkIndex(-1);

		for (ShaderNodePin& pin : node->GetOutputPins())
			pin.SetLinkIndex(-1);
	}

	for (int index = int(m_Links.size()) - 1; index >= 0; index--)
	{
		const ShaderGraphLink& link = m_Links[index];
		const bool valid_nodes = link.fromNodeIndex >= 0 && link.fromNodeIndex < m_ShaderNodes.size() && link.toNodeIndex >= 0 && link.toNodeIndex < m_ShaderNodes.size();

		if (!valid_nodes ||
			link.fromPinIndex < 0 || link.fromPinIndex >= m_ShaderNodes[link.fromNodeIndex]->GetOutputPins().size() ||
			link.toPinIndex < 0 || link.toPinIndex >= m_ShaderNodes[link.toNodeIndex]->GetInputPins().size())
		{
			m_Links.erase(m_Links.begin() + index);
		}
	}

	for (const auto& [index, link] : gEnumerate(m_Links))
	{
		m_ShaderNodes[link.toNodeIndex]->GetInputPin(link.toPinIndex).SetLinkIndex(int(index));
		m_ShaderNodes[link.fromNodeIndex]->GetOutputPin(link.fromPinIndex).SetLinkIndex(int(index));
	}
}


bool ShaderGraphBuilder::FindPin(int inNode, int inGlobalPinIndex, bool& outIsOutput, int& outPinIndex)
{
	if (inNode < 0 || inNode >= m_ShaderNodes.size())
		return false;

	for (const auto& [index, pin] : gEnumerate(m_ShaderNodes[inNode]->GetInputPins()))
	{
		if (pin.GetGlobalIndex() == inGlobalPinIndex)
		{
			outIsOutput = false;
			outPinIndex = int(index);
			return true;
		}
	}

	for (const auto& [index, pin] : gEnumerate(m_ShaderNodes[inNode]->GetOutputPins()))
	{
		if (pin.GetGlobalIndex() == inGlobalPinIndex)
		{
			outIsOutput = true;
			outPinIndex = int(index);
			return true;
		}
	}

	return false;
}


void ShaderGraphBuilder::DestroyShaderNode(int inIndex)
{
	if (inIndex < 0 || inIndex >= m_ShaderNodes.size())
		return;

	const int back_index = int(m_ShaderNodes.size()) - 1;

	for (int index = int(m_Links.size()) - 1; index >= 0; index--)
	{
		if (m_Links[index].fromNodeIndex == inIndex || m_Links[index].toNodeIndex == inIndex)
			m_Links.erase(m_Links.begin() + index);
	}

	for (ShaderGraphLink& link : m_Links)
	{
		if (link.fromNodeIndex == back_index)
			link.fromNodeIndex = inIndex;

		if (link.toNodeIndex == back_index)
			link.toNodeIndex = inIndex;
	}

	if (inIndex != back_index && ImNodes::GetCurrentContext() != nullptr)
		ImNodes::SetNodeGridSpacePos(inIndex, ImNodes::GetNodeGridSpacePos(back_index));

	m_ShaderNodes[inIndex] = m_ShaderNodes.back();
	m_ShaderNodes.pop_back();

	UpdatePinLinks();
}


void ShaderGraphBuilder::Clear()
{
	m_Layout.clear();
	m_Links.clear();
	m_Functions.clear();
	m_ShaderNodes.clear();
	m_TemplateFile.clear();
	m_OutputFile.clear();
	m_ErrorMessage.clear();
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
	m_LineNr = 0;
	m_Functions.clear();
	m_ErrorMessage.clear();

	UpdatePinLinks();

	int output_node = -1;

	for (const auto& [index, shader_node] : gEnumerate(m_ShaderNodes))
	{
		if (!shader_node->IsOutputNode())
			continue;

		if (output_node != -1)
		{
			SetErrorMessage("The graph has more than one shader output node.");
			return false;
		}

		output_node = int(index);
	}

	if (output_node == -1)
	{
		SetErrorMessage("The graph has no shader output node.");
		return false;
	}

	enum EVisitState { UNVISITED, VISITING, VISITED };

	Array<int> sorted_nodes;
	Array<EVisitState> visit_states(m_ShaderNodes.size(), UNVISITED);

	auto Visit = [&](auto&& inVisit, int inNode) -> bool
	{
		if (visit_states[inNode] == VISITED)
			return true;

		if (visit_states[inNode] == VISITING)
			return false;

		visit_states[inNode] = VISITING;

		for (const ShaderGraphLink& link : m_Links)
		{
			if (link.toNodeIndex == inNode && !inVisit(inVisit, link.fromNodeIndex))
				return false;
		}

		visit_states[inNode] = VISITED;
		sorted_nodes.push_back(inNode);

		return true;
	};

	if (!Visit(Visit, output_node))
	{
		SetErrorMessage("The graph contains a cycle.");
		return false;
	}

	String evaluation_order;
	for (int node_index : sorted_nodes)
		evaluation_order += std::format("{} ", node_index);

	gLogDebug("Shader Graph", "Node evaluation order: {}", evaluation_order);

	String main_code;
	for (int node_index : sorted_nodes)
	{
		ShaderNode* shader_node = GetShaderNode(node_index);

		const String generated_code = shader_node->GenerateCode(*this);

		if (generated_code.empty())
			continue;

		main_code += std::format("    // Node {} -> {}\n", node_index, shader_node->GetRTTI().GetTypeName());

		std::istringstream iss(generated_code);

		for (std::string line; std::getline(iss, line); )
			main_code += std::format("    {}\n", line);
	}

	String global_code;
	for (const String& function : GetFunctions())
		global_code += function + "\n\n";

	const size_t global_code_start = ioCode.find("@Global");
	const size_t main_code_start = ioCode.find("@Main");

	if (global_code_start == String::npos || main_code_start == String::npos)
	{
		SetErrorMessage("The template is missing an @Global or @Main marker.");
		return false;
	}

	ioCode.replace(global_code_start, std::strlen("@Global"), global_code);
	ioCode.replace(ioCode.find("@Main"), std::strlen("@Main"), main_code);

	gLogDebug("Shader Graph", "Generated shader code:\n{}\n{}", global_code, main_code);

	return true;
}


bool ShaderGraphBuilder::GenerateCodeFromFiles(const Path& inTemplateFile, const Path& inOutputFile)
{
	if (!fs::exists(inTemplateFile) || !fs::is_regular_file(inTemplateFile))
	{
		SetErrorMessage(std::format("Template file {} does not exist.", inTemplateFile.string()));
		return false;
	}

	std::ifstream ifs(inTemplateFile);
	std::stringstream buffer;
	buffer << ifs.rdbuf();

	String code = buffer.str();

	if (!GenerateCodeFromTemplate(code))
		return false;

	if (inOutputFile.has_parent_path())
		fs::create_directories(inOutputFile.parent_path());

	std::ofstream ofs(inOutputFile);
	ofs << code;

	return true;
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

	back.UpdatePinLinks();
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

	back.UpdatePinLinks();
	return back;
}


void ShaderGraphBuilder::SaveToFileJSON(JSON::WriteArchive& ioArchive)
{
	ioArchive << *this;

	for (const auto& shader_node : m_ShaderNodes)
		ioArchive.WriteNextObject(shader_node->GetRTTI(), shader_node.get());
}


void ShaderGraphBuilder::OpenFromFileJSON(JSON::ReadArchive& ioArchive)
{
	Clear();

	ioArchive >> *this;

	RTTI* rtti = nullptr;
	void* object = ioArchive.ReadNextObject(&rtti);

	while (object)
	{
		m_ShaderNodes.emplace_back((ShaderNode*)object);
		object = ioArchive.ReadNextObject(&rtti);
	}

	UpdatePinLinks();
}


bool ShaderGraphBuilder::sGenerateFromGraphFile(const Path& inGraphFile)
{
	gRegisterShaderNodeTypes();

	if (!fs::exists(inGraphFile))
	{
		gLogError("Shader Graph", "Graph file {} does not exist.", inGraphFile.string());
		return false;
	}

	ShaderGraphBuilder builder;
	JSON::ReadArchive archive(inGraphFile);
	builder.OpenFromFileJSON(archive);

	if (builder.GetShaderNodes().empty())
	{
		gLogError("Shader Graph", "Graph file {} contains no nodes.", inGraphFile.string());
		return false;
	}

	if (!builder.GenerateCodeFromFiles(builder.GetTemplateFile(), builder.GetOutputFile()))
	{
		gLogError("Shader Graph", "Failed to generate {}: {}", inGraphFile.string(), builder.GetErrorMessage());
		return false;
	}

	gLogInfo("Shader Graph", "Generated {} from {} ({} nodes, {} links)", builder.GetOutputFile(), inGraphFile.string(), builder.GetShaderNodes().size(), builder.GetLinks().size());
	return true;
}

} // raekor
