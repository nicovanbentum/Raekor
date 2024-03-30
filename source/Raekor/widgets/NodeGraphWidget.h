#pragma once

#include "widget.h"
#include "json.h"
#include "serial.h"
#include "defines.h"
#include "iter.h"

namespace Raekor {

enum EVectorType 
{ 
	VECTOR_TYPE_VEC2,
	VECTOR_TYPE_VEC3,
	VECTOR_TYPE_VEC4,
	VECTOR_TYPE_COUNT
};

class ShaderNodePin
{

};


class ShaderNode
{
	RTTI_DECLARE_VIRTUAL_TYPE(ShaderNode);

public:
	virtual void DrawImNode(int& inNodeCounter, int& inPinCounter) {}
};


struct ShaderNodeLink
{
	uint32_t mStartNodeIndex;
	uint32_t mStartPinIndex;
	uint32_t mEndNodeIndex;
	uint32_t mEndPinIndex;
};


class VectorOpShaderNode : public ShaderNode
{
	RTTI_DECLARE_VIRTUAL_TYPE(VectorOpShaderNode);

public:
	enum Op
	{
		VECTOR_OP_ADD,
		VECTOR_OP_MINUS,
		VECTOR_OP_MULTIPLY,
		VECTOR_OP_DIVIDE,
		VECTOR_OP_COUNT,
	};

	void DrawImNode(int& inNodeCounter, int& inPinCounter) override
	{
		ImNodes::BeginNode(inNodeCounter++);

		ImNodes::BeginNodeTitleBar();
		ImGui::Text("VectorOp");
		ImNodes::EndNodeTitleBar();

		constexpr std::array type_sizes = { 2, 3, 4 };
		constexpr std::array type_names = { "Vec2", "Vec3", "Vec4" };
		constexpr std::array type_functions = { &ImGui::InputFloat2, &ImGui::InputFloat3, &ImGui::InputFloat4 };
		constexpr std::array operation_names = { "Add", "Minus", "Multiply", "Divide" };

		const float node_width = ImGui::CalcTextSize("0.0000, ").x * type_sizes[int(m_VectorType)];
		ImGui::PushItemWidth(node_width);

		if (ImGui::BeginCombo("##vectorshadernodevarianttype", type_names[m_VectorType]))
		{
			for (const auto& [index, name] : gEnumerate(type_names))
			{
				if (ImGui::Selectable(name, int(m_VectorType) == index))
					m_VectorType = EVectorType(index);
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

		auto InputFloatFunction = type_functions[int(m_VectorType)];

		ImNodes::BeginInputAttribute(inPinCounter++);
		InputFloatFunction("A", &m_VectorA[0], "%.3f", ImGuiInputTextFlags_None);
		ImNodes::EndInputAttribute();

		ImNodes::BeginInputAttribute(inPinCounter++);
		InputFloatFunction("B", &m_VectorB[0], "%.3f", ImGuiInputTextFlags_None);
		ImNodes::EndInputAttribute();

		ImGui::PopItemWidth();

		ImNodes::BeginOutputAttribute(inPinCounter++);
		ImGui::Indent(node_width);
		ImGui::Text("Out");
		ImNodes::EndOutputAttribute();

		ImNodes::EndNode();
	}

private:
	Op m_Op;
	EVectorType m_VectorType;
	Vec4 m_VectorA;
	Vec4 m_VectorB;
};


class PixelShaderOutputShaderNode : public ShaderNode
{
	RTTI_DECLARE_VIRTUAL_TYPE(PixelShaderOutputShaderNode);

public:
	void DrawImNode(int& inNodeCounter, int& inPinCounter) override
	{
		ImNodes::BeginNode(inNodeCounter++);

		ImNodes::BeginNodeTitleBar();
		ImGui::Text("Pixel Shader Output");
		ImNodes::EndNodeTitleBar();

		const float node_width = ImGui::CalcTextSize("0.0000,  0.0000,  0.0000,  0.0000").x;
		ImGui::PushItemWidth(node_width);

		ImNodes::BeginInputAttribute(inPinCounter++);
		ImGui::InputFloat4("Albedo", &m_Albedo[0]);
		ImNodes::EndInputAttribute();

		ImNodes::BeginInputAttribute(inPinCounter++);
		ImGui::InputFloat3("Normal", &m_Normal[0]);
		ImNodes::EndInputAttribute();

		ImNodes::BeginInputAttribute(inPinCounter++);
		ImGui::InputFloat3("Emissive", &m_Emissive[0]);
		ImNodes::EndInputAttribute();

		ImNodes::BeginInputAttribute(inPinCounter++);
		ImGui::InputFloat("Metallic", &m_Metallic);
		ImNodes::EndInputAttribute();

		ImNodes::BeginInputAttribute(inPinCounter++);
		ImGui::InputFloat("Roughness", &m_Roughness);
		ImNodes::EndInputAttribute();

		ImGui::PopItemWidth();

		ImNodes::EndNode();
	}

private:
	Vec4 m_Albedo;
	Vec3 m_Normal;
	Vec3 m_Emissive;
	float m_Metallic;
	float m_Roughness;
};


class ShaderGraphBuilder
{
public:
	template<typename T>
	ShaderNode* CreateShaderNode() { return m_ShaderNodes.emplace_back(new T()); }
	Slice<ShaderNode*> GetShaderNodes() const { return Slice(m_ShaderNodes); }

private:
	Array<ShaderNode*> m_ShaderNodes;
	Array<ShaderNodePin> m_ShaderNodePins;
	Array<ShaderNodeLink> m_ShaderNodeLinks;
};

class NodeGraphWidget : public IWidget
{
public:
	RTTI_DECLARE_VIRTUAL_TYPE(NodeGraphWidget);

	NodeGraphWidget(Application* inApp);

	virtual void Draw(Widgets* inWidgets, float dt) override;
	virtual void OnEvent(Widgets* inWidgets, const SDL_Event& ev) override;

	int GetSelectedObjectIndex() { return m_SelectedObject; }
	void SelectObject(int inIndex) { m_SelectedObject = inIndex; ImNodes::ClearNodeSelection(); ImNodes::SelectNode(m_SelectedObject); }

private:
	int m_SelectedObject = -1;
	int m_LinkPinDropped = -1;

	bool m_WasRightClicked = false;
	bool m_WasLinkConnected = false;

	int m_StartNodeID, m_StartPinID;
	int m_EndNodeID, m_EndPinID;
	
	Path m_OpenFilePath;
	ShaderGraphBuilder m_ShaderGraphBuilder;
};

}