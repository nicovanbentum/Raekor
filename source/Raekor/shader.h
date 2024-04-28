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
	static constexpr ImU32 sOtherColor = IM_COL32(200, 200, 200, 255);
	static constexpr ImU32 sScalarColor = IM_COL32(36, 98, 131, 255);
	static constexpr ImU32 sVectorColor = IM_COL32(99, 99, 199, 255);
	static constexpr ImU32 sTextureColor = IM_COL32(131, 49, 74, 255);
	static constexpr ImU32 sPixelShaderColor = IM_COL32(73, 131, 73, 255);
	static constexpr ImU32 sVertexShaderColor = IM_COL32(201, 181, 15, 255);

public:
	ShaderNodePin* GetInputPin(int inIndex) { return inIndex >= GetInputPins().Length() ? nullptr : &GetInputPins()[inIndex]; }
	ShaderNodePin* GetOutputPin(int inIndex) { return inIndex >= GetOutputPins().Length() ? nullptr : &GetOutputPins()[inIndex]; }

	virtual MutSlice<ShaderNodePin> GetInputPins() { return MutSlice<ShaderNodePin>(); }
	virtual MutSlice<ShaderNodePin> GetOutputPins() { return MutSlice<ShaderNodePin>(); }

	virtual void DrawImNode(ShaderGraphBuilder& inBuilder) {}
	virtual String GenerateCode(ShaderGraphBuilder& inBuilder) { return ""; }

	int GetIndex() const { return m_Index; }
	void SetIndex(int inIndex) { m_Index = inIndex; }

	bool GetIsCollapsed() const { return m_IsCollapsed; }
	void SetIsCollapsed(bool inValue) { m_IsCollapsed = inValue; }

protected:
	int m_Index = -1;
	bool m_IsCollapsed = false;
};


class ShaderNodePin
{
	RTTI_DECLARE_TYPE(ShaderNodePin);
	friend class ProcedureShaderNode; 

public:
	enum EKind 
	{ 
		AUTO,
		BOOL,
		FLOAT,
		VECTOR,
		COUNT
	};

	static constexpr std::array sKindNames =
	{
		"Auto",
		"Bool",
		"Float",
		"Vector"
	};

	static constexpr std::array sKindTypeNames =
	{
		"auto",
		"bool",
		"float",
		"float3"
	};

public:
	ShaderNodePin() = default;
	ShaderNodePin(EKind inKind) : m_Kind(inKind) {}

	static bool sIsCompatible(EKind inFrom, EKind inTo);

	int GetIndex() const { return m_Index; }
	void SetIndex(int inIndex) { m_Index = inIndex; }

	EKind GetKind() const { return m_Kind; }
	void SetKind(EKind inKind) { m_Kind = inKind; }

	ImU32 GetColor() const { return m_KindColors[m_Kind]; }
	StringView GetKindName() const { return sKindNames[m_Kind]; }
	StringView GetKindTypeName() const { return sKindTypeNames[m_Kind]; }

	StringView GetOutVariableName() const { return m_VariableName; }
	void SetOutVariableName(StringView inName) { m_VariableName = inName; }

private:
	static constexpr std::array m_KindColors =
	{
		ShaderNode::sOtherColor,  // AUTO
		ShaderNode::sOtherColor,  // BOOL
		ShaderNode::sScalarColor, // SCALAR
		ShaderNode::sVectorColor, // VECTOR
		ShaderNode::sVectorColor, // VECTOR
		ShaderNode::sVectorColor  // VECTOR
	};

private:
	int m_Index = -1;
	EKind m_Kind = AUTO;
	String m_VariableName;
};


enum EShaderGraph
{
	SHADER_GRAPH_VERTEX,
	SHADER_GRAPH_PIXEL,
	SHADER_GRAPH_COUNT
};

class ShaderGraphBuilder
{
	RTTI_DECLARE_TYPE(ShaderGraphBuilder);

public:
	//////////////////////
	// GRAPH FUNCTIONS
	template<typename T>
	ShaderNode* CreateShaderNode() { return m_ShaderNodes.emplace_back(std::make_shared<T>()).get(); }

	void DestroyShaderNode(int inIndex);

	int GetShaderNodeIndex(const ShaderNodePin& inPin) const { return M_ShaderNodePins[inPin.GetIndex()].first; }
	int GetShaderNodePinIndex(const ShaderNodePin& inPin) const { return M_ShaderNodePins[inPin.GetIndex()].second; }

	ShaderNode* GetShaderNode(int inIndex) { return m_ShaderNodes[inIndex].get(); }
	const ShaderNode* GetShaderNode(int inIndex) const { return m_ShaderNodes[inIndex].get(); }

	ShaderNode* GetShaderNode(const ShaderNodePin& inPin) { return GetShaderNode(M_ShaderNodePins[inPin.GetIndex()].second); }
	const ShaderNode* GetShaderNode(const ShaderNodePin& inPin) const { return GetShaderNode(M_ShaderNodePins[inPin.GetIndex()].second); }

	const ShaderNodePin* GetInputPin(int inIndex) { return GetShaderNode(M_ShaderNodePins[inIndex].first)->GetInputPin(M_ShaderNodePins[inIndex].second); }
	const ShaderNodePin* GetOutputPin(int inIndex) { return GetShaderNode(M_ShaderNodePins[inIndex].first)->GetOutputPin(M_ShaderNodePins[inIndex].second); }

	int GetIncomingPin(int inIndex) const { return m_IncomingLinks.at(inIndex); }
	Slice<int> GetOutgoingPins(int inIndex) const { return m_OutgoingLinks.at(inIndex); }

	bool HasIncomingPin(int inIndex) const { return m_IncomingLinks.contains(inIndex); }
	bool HasOutgoingPins(int inIndex) const { return m_OutgoingLinks.contains(inIndex) && !m_OutgoingLinks.at(inIndex).empty(); }

	void ConnectPins(int inStartPin, int inEndPin);
	void DisconnectPins(int inStartPin, int inEndPin);

	const ShaderNodePin* GetIncomingPin(const ShaderNodePin& inPin) { return HasIncomingPin(inPin.GetIndex()) ? GetOutputPin(GetIncomingPin(inPin.GetIndex())) : nullptr; }

	Slice<std::shared_ptr<ShaderNode>> GetShaderNodes() const { return m_ShaderNodes; }

	void RequestPassConstants() { m_RequiresPassConstants = true; }
	void RequestFrameConstants() { m_RequiresFrameConstants = true; }

	//////////////////////
	// IMGUI/IMNODES FUNCTIONS
	void BeginNode();
	void BeginNode(StringView inTitle);
	void BeginNode(StringView inTitle, ImU32 inColor);
	void EndNode() { ImNodes::EndNode(); }

	bool BeginInputPin();
	bool BeginOutputPin();

	void EndInputPin() { ImNodes::PopColorStyle();  ImNodes::EndInputAttribute(); }
	void EndOutputPin() { ImNodes::PopColorStyle(); ImNodes::EndOutputAttribute(); }

	uint32_t GetCurrentNodeIndex() const { return m_NodeIndex; }
	uint32_t GetCurrentInputPinIndex() const { return m_InputPinIndex; }
	uint32_t GetCurrentOutputPinIndex() const { return m_OutputPinIndex; }

	void BeginDraw();
	void EndDraw() {}

	Slice<Pair<int, int>> GetLinks() const { return Slice(m_Links); }

	bool GetShowNodeIndices() const { return m_ShowNodeIndices; }
	void SetShowNodeIndices(bool inValue) { m_ShowNodeIndices = inValue; }

	//////////////////////
	// CODEGEN FUNCTIONS
	bool GenerateCodeFromTemplate(String& ioCode);

	uint32_t GetLineNumber() const { return m_LineNr; }
	uint32_t IncrLineNumber() { return m_LineNr++; }

	void SetErrorMessage(StringView inMessage) { m_ErrorMessage = inMessage; }
	StringView GetErrorMessage() const { return m_ErrorMessage; }

	Slice<String> GetFunctions() const { return m_Functions; }
	void AddFunction(const String& inFunction) { m_Functions.push_back(inFunction); }

	void StoreSnapshot();
	void ResetSnapshots();
	ShaderGraphBuilder Undo();
	ShaderGraphBuilder Redo();

	//////////////////////
	// SAVE/RESTORE FUNCTIONS
	void SaveToFileJSON(JSON::WriteArchive& ioArchive);
	void OpenFromFileJSON(JSON::ReadArchive& ioArchive);

private:
	static inline int m_SnapshotIndex = -1;
	static inline Array<ShaderGraphBuilder> m_Undo;
	static inline Array<ShaderGraphBuilder> m_Redo;

private:
	bool m_ShowNodeIndices = false;
	bool m_RequiresPassConstants = false;
	bool m_RequiresFrameConstants = false;

	int m_LineNr = 0;
	int m_NodeIndex = -1;
	int m_InputPinIndex = -1;
	int m_OutputPinIndex = -1;


	String m_Layout;
	String m_ErrorMessage;
	Array<String> m_Functions;
	Array<Pair<int, int>> m_Links; // global pin index <-> global pin index
	Array<Pair<int, int>> M_ShaderNodePins; // global pin index -> <node index, local pin index>

	HashMap<int, int> m_IncomingLinks; // global pin index -> global pin index
	HashMap<int, Array<int>> m_OutgoingLinks; // global pin index -> global pin indices
	Array<SharedPtr<ShaderNode>> m_ShaderNodes;
};

} // namespace Raekor