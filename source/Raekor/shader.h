#pragma once

#include "rtti.h"
#include "slice.h"
#include "archive.h"

namespace Raekor {

class ShaderNode;
class ShaderNodePin;
class ShaderGraphBuilder;


class ShaderNode
{
	RTTI_DECLARE_VIRTUAL_TYPE(ShaderNode);

public:
	ShaderNodePin* GetInputPin(int inIndex) { return inIndex >= GetInputPins().Length() ? nullptr : &GetInputPins()[inIndex]; }
	ShaderNodePin* GetOutputPin(int inIndex) { return inIndex >= GetOutputPins().Length() ? nullptr : &GetOutputPins()[inIndex]; }

	virtual MutSlice<ShaderNodePin> GetInputPins() { return MutSlice<ShaderNodePin>(); }
	virtual MutSlice<ShaderNodePin> GetOutputPins() { return MutSlice<ShaderNodePin>(); }

	virtual void DrawImNode(ShaderGraphBuilder& inBuilder) {}
	virtual String GenerateCode(ShaderGraphBuilder& inBuilder);
};


struct ShaderNodePin
{
	RTTI_DECLARE_TYPE(ShaderNodePin);
	friend class ProcedureShaderNode; 

public:
	enum EKind 
	{ 
		AUTO,
		BOOL,
		FLOAT,
		FLOAT2,
		FLOAT3,
		FLOAT4
	};

	static constexpr std::array sKindNames =
	{
		"auto",
		"bool",
		"float",
		"float2",
		"float3",
		"float4"
	};

public:
	static constexpr ImVec4 sBlue  = ImVec4(0.3f, 0.3f, 8.0f, 1.0f);
	static constexpr ImVec4 sWhite = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
	static constexpr ImVec4 sOrange = ImVec4(1.0f, 0.3f, 0.3, 1.0f);

public:
	ShaderNodePin() = default;
	ShaderNodePin(EKind inKind) : m_Kind(inKind) {}

	EKind GetKind() const { return m_Kind; }
	ImVec4 GetColor() const { return m_KindColors[m_Kind]; }
	StringView GetKindName() const { return sKindNames[m_Kind]; }

	int GetConnectedPin() const { return m_ConnectedPin; }
	int GetConnectedNode() const { return m_ConnectedNode; }

	void SetConnectedPin(int inValue) { m_ConnectedPin = inValue; }
	void SetConnectedNode(int inValue) { m_ConnectedNode = inValue; }

	bool IsConnected() const { return m_ConnectedPin != -1 && m_ConnectedNode != -1; }

	StringView GetOutVariableName() const { return m_VariableName; }
	void SetOutVariableName(StringView inName) { m_VariableName = inName; }

private:
	static constexpr std::array m_KindColors =
	{
		sWhite,  // AUTO
		sWhite,  // BOOL
		sBlue,   // SCALAR
		sOrange, // VECTOR
		sOrange, // VECTOR
		sOrange  // VECTOR
	};

private:
	int m_ConnectedPin = -1;
	int m_ConnectedNode = -1;

private:
	EKind m_Kind = AUTO;
	String m_VariableName;
};


class ShaderGraphBuilder
{
	RTTI_DECLARE_TYPE(ShaderGraphBuilder);

public:
	//////////////////////
	// GRAPH FUNCTIONS
	template<typename T>
	ShaderNode* CreateShaderNode() { return m_ShaderNodes.emplace_back(std::make_shared<T>()).get(); }

	ShaderNode* GetShaderNode(int inIndex) { return m_ShaderNodes[inIndex].get(); }
	const ShaderNode* GetShaderNode(int inIndex) const { return m_ShaderNodes[inIndex].get(); }

	ShaderNodePin* GetConnectedInputPin(const ShaderNodePin& inPin) { return GetShaderNode(inPin.GetConnectedNode())->GetInputPin(inPin.GetConnectedPin()); }
	ShaderNodePin* GetConnectedOutputPin(const ShaderNodePin& inPin) { return GetShaderNode(inPin.GetConnectedNode())->GetOutputPin(inPin.GetConnectedPin()); }

	void ConnectPins(int inStartPin, int inEndPin);
	void DisconnectPins(int inStartPin, int inEndPin);

	Slice<Pair<int, int>> GetLinks() const { return m_Links; }
	Slice<std::shared_ptr<ShaderNode>> GetShaderNodes() const { return m_ShaderNodes; }

	void RequestPassConstants() { m_RequiresPassConstants = true; }
	void RequestFrameConstants() { m_RequiresFrameConstants = true; }

	//////////////////////
	// IMGUI/IMNODES FUNCTIONS
	void BeginNode();
	void BeginNode(StringView inTitle);
	void EndNode() { ImNodes::EndNode(); }

	bool BeginInputPin();
	bool BeginOutputPin();

	void EndInputPin() { ImNodes::PopColorStyle();  ImNodes::EndInputAttribute(); }
	void EndOutputPin() { ImNodes::PopColorStyle(); ImNodes::EndOutputAttribute(); }

	uint32_t GetCurrentNodeIndex() const { return m_NodeIndex; }
	uint32_t GetCurrentInputPinIndex() const { return m_InputPinIndex; }
	uint32_t GetCurrentOutputPinIndex() const { return m_OutputPinIndex; }

	void BeginDraw();
	void EndDraw();

	//////////////////////
	// CODEGEN FUNCTIONS
	void BeginCodeGen() 
	{ 
		m_LineNr = 0; 
		m_Functions.clear();
	}

	void EndCodeGen() {}

	uint32_t GetLineNumber() const { return m_LineNr; }
	uint32_t IncrLineNumber() { return m_LineNr++; }

	Slice<String> GetFunctions() const { return m_Functions; }
	void AddFunction(const String& inFunction) { m_Functions.push_back(inFunction); }

	//////////////////////
	// SAVE/RESTORE FUNCTIONS
	void SaveToFileJSON(JSON::WriteArchive& ioArchive);
	void OpenFromFileJSON(JSON::ReadArchive& ioArchive);

private:
	bool m_RequiresPassConstants = false;
	bool m_RequiresFrameConstants = false;

	uint32_t m_LineNr = 0;
	uint32_t m_NodeIndex = -1;
	uint32_t m_InputPinIndex = -1;
	uint32_t m_OutputPinIndex = -1;
	Array<String> m_Functions;
	Array<Pair<int, int>> m_Links; // <global pin index, global pin index>
	Array<Pair<int, int>> M_ShaderNodePins; // global pin index -> <node index, local pin index>
	Array<std::shared_ptr<ShaderNode>> m_ShaderNodes;
};

} // namespace Raekor