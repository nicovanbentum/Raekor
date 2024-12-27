#pragma once

#include "Engine/rtti.h"
#include "Engine/archive.h"

namespace RK {

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
	ShaderNodePin& GetInputPin(int inIndex) { return GetInputPins()[inIndex]; }
	ShaderNodePin& GetOutputPin(int inIndex) { return GetOutputPins()[inIndex]; }

	const ShaderNodePin& GetInputPin(int inIndex) const { return GetInputPins()[inIndex]; }
	const ShaderNodePin& GetOutputPin(int inIndex) const { return GetOutputPins()[inIndex]; }

	virtual Slice<ShaderNodePin> GetInputPins() { return Slice<ShaderNodePin>(); }
	virtual Slice<ShaderNodePin> GetOutputPins() { return Slice<ShaderNodePin>(); }

	virtual Slice<const ShaderNodePin> GetInputPins() const { return Slice<ShaderNodePin>(); }
	virtual Slice<const ShaderNodePin> GetOutputPins() const { return Slice<ShaderNodePin>(); }

	virtual void DrawImNode(ShaderGraphBuilder& inBuilder) {}
	virtual String GenerateCode(ShaderGraphBuilder& inBuilder) { return ""; }

	bool GetIsCollapsed() const { return m_IsCollapsed; }
	void SetIsCollapsed(bool inValue) { m_IsCollapsed = inValue; }

protected:
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

	int GetGlobalIndex() const { return m_Index; }
	void SetGlobalIndex(int inIndex) { m_Index = inIndex; }

	EKind GetKind() const { return m_Kind; }
	void SetKind(EKind inKind) { m_Kind = inKind; }

	bool HasLink() const { return m_LinkIndex > -1; }
	int GetLinkIndex() const { return m_LinkIndex; }

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
	int m_LinkIndex = -1;
	EKind m_Kind = AUTO;
	String m_VariableName;
};


enum EShaderGraph
{
	SHADER_GRAPH_VERTEX,
	SHADER_GRAPH_PIXEL,
	SHADER_GRAPH_COUNT
};

struct ShaderGraphLink
{
	RTTI_DECLARE_TYPE(ShaderGraphLink);

	int fromNodeIndex;
	int fromPinIndex;
	int toNodeIndex;
	int toPinIndex;
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

	ShaderNode* GetShaderNode(int inIndex) { return m_ShaderNodes[inIndex].get(); }
	const ShaderNode* GetShaderNode(int inIndex) const { return m_ShaderNodes[inIndex].get(); }

	const ShaderNodePin* GetIncomingPin(const ShaderNodePin& inPin);

	Slice<const SharedPtr<ShaderNode>> GetShaderNodes() const { return m_ShaderNodes; }

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

	ShaderGraphLink& GetLink(int inIndex) { return m_Links[inIndex]; }
	const ShaderGraphLink& GetLink(int inIndex) const { return m_Links[inIndex]; }

	const Array<ShaderGraphLink>& GetLinks() const { return m_Links; }

	bool GetShowNodeIndices() const { return m_ShowNodeIndices; }
	void SetShowNodeIndices(bool inValue) { m_ShowNodeIndices = inValue; }

	//////////////////////
	// CODEGEN FUNCTIONS
	bool GenerateCodeFromTemplate(String& ioCode);

	uint32_t GetLineNumber() const { return m_LineNr; }
	uint32_t IncrLineNumber() { return m_LineNr++; }

	void SetErrorMessage(StringView inMessage) { m_ErrorMessage = inMessage; }
	StringView GetErrorMessage() const { return m_ErrorMessage; }

	Slice<const String> GetFunctions() const { return m_Functions; }
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
	int m_GlobalPinIndex = -1;
	int m_OutputPinIndex = -1;

	String m_Layout;
	String m_ErrorMessage;
	
	Array<String> m_Functions;
	Array<ShaderGraphLink> m_Links;
	Array<SharedPtr<ShaderNode>> m_ShaderNodes;
};

} // namespace Raekor