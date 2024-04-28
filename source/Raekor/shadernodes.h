#pragma once

#include "rtti.h"
#include "shader.h"

namespace Raekor {

void gRegisterShaderNodeTypes();


class FloatValueShaderNode : public ShaderNode
{
	RTTI_DECLARE_VIRTUAL_TYPE(FloatValueShaderNode);

public:
	void DrawImNode(ShaderGraphBuilder& inBuilder) override;
	String GenerateCode(ShaderGraphBuilder& inBuilder) override;

	MutSlice<ShaderNodePin> GetOutputPins() override { return MutSlice(&m_OutputPin); }

private:
	float m_Float = 0.0f;
	ShaderNodePin m_OutputPin = { ShaderNodePin::FLOAT };
};


class FloatOpShaderNode : public ShaderNode
{
	RTTI_DECLARE_VIRTUAL_TYPE(FloatOpShaderNode);

public:
	enum Op
	{
		MATH_OP_ADD,
		MATH_OP_SUB,
		MATH_OP_DIVIDE,
		MATH_OP_MULTIPLY,
		MATH_OP_COUNT,
	};

	constexpr static std::array m_OpNames = 
	{ 
		"Add", 
		"Subtract", 
		"Divide",
		"Multiply" 
	};

	static constexpr std::array m_OpSymbols =
	{
		"+",
		"-",
		"/",
		"*"
	};

	void DrawImNode(ShaderGraphBuilder& inBuilder) override;
	String GenerateCode(ShaderGraphBuilder& inBuilder) override;

	MutSlice<ShaderNodePin> GetInputPins() { return MutSlice(m_InputPins); }
	MutSlice<ShaderNodePin> GetOutputPins() { return MutSlice(m_OutputPins); }

private:
	StaticArray<ShaderNodePin, 2> m_InputPins = { ShaderNodePin::FLOAT,  ShaderNodePin::FLOAT };
	StaticArray<ShaderNodePin, 1> m_OutputPins = { ShaderNodePin::FLOAT };

private:
	Op m_Op;
	float m_FloatA;
	float m_FloatB;
};


class FloatFunctionShaderNode : public ShaderNode
{
	RTTI_DECLARE_VIRTUAL_TYPE(FloatFunctionShaderNode);

public:
	enum EFunction
	{
		FLOAT_FUNCTION_ABS,
		FLOAT_FUNCTION_SIN,
		FLOAT_FUNCTION_COS,
		FLOAT_FUNCTION_TAN,
		FLOAT_FUNCTION_ROUND,
		FLOAT_FUNCTION_SATURATE,
		FLOAT_FUNCTION_COUNT
	};

	constexpr static std::array m_FunctionNames =
	{
		"Abs",
		"Sin",
		"Cos",
		"Tan",
		"Round",
		"Saturate"
	};

	constexpr static std::array m_FunctionCode =
	{
		"abs",
		"sin",
		"cos",
		"tan",
		"round",
		"saturate"
	};

	void DrawImNode(ShaderGraphBuilder& ioBuilder) override;
	String GenerateCode(ShaderGraphBuilder& inBuilder) override;

	MutSlice<ShaderNodePin> GetInputPins() override { return MutSlice(&m_InputPin, 1); }
	MutSlice<ShaderNodePin> GetOutputPins() override { return MutSlice(&m_OutputPin, 1); }

private:
	EFunction m_Function = FLOAT_FUNCTION_SIN;
	ShaderNodePin m_InputPin = ShaderNodePin::FLOAT;
	ShaderNodePin m_OutputPin = ShaderNodePin::FLOAT;
};


class VectorValueShaderNode : public ShaderNode
{
	RTTI_DECLARE_VIRTUAL_TYPE(VectorValueShaderNode);

public:
	void DrawImNode(ShaderGraphBuilder& inBuilder) override;
	String GenerateCode(ShaderGraphBuilder& inBuilder) override;

	MutSlice<ShaderNodePin> GetInputPins() override { return MutSlice(m_InputPins); }
	MutSlice<ShaderNodePin> GetOutputPins() override { return MutSlice(m_OutputPins); }

private:
	Vec3 m_Vector = Vec3(0.0f, 0.0f, 0.0f);
	StaticArray<ShaderNodePin, 3> m_InputPins = { ShaderNodePin::FLOAT, ShaderNodePin::FLOAT, ShaderNodePin::FLOAT };
	StaticArray<ShaderNodePin, 1> m_OutputPins = { ShaderNodePin::VECTOR };
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

	static constexpr std::array m_OpSymbols =
	{
		"+",
		"-",
		"*",
		"/"
	};

	constexpr static std::array m_FunctionNames = 
	{ 
		"Add", 
		"Minus", 
		"Multiply", 
		"Divide" 
	};


	void DrawImNode(ShaderGraphBuilder& inBuilder) override;
	String GenerateCode(ShaderGraphBuilder& inBuilder) override;

	MutSlice<ShaderNodePin> GetInputPins() { return MutSlice(m_InputPins); }
	MutSlice<ShaderNodePin> GetOutputPins() { return MutSlice(m_OutputPins); }

private:
	StaticArray<ShaderNodePin, 2> m_InputPins = { ShaderNodePin::VECTOR, ShaderNodePin::VECTOR };
	StaticArray<ShaderNodePin, 1> m_OutputPins = { ShaderNodePin::VECTOR };

private:
	Op m_Op = VECTOR_OP_ADD;
	StaticArray<Vec4, 2> m_Vectors;
};


class VectorFunctionShaderNode : public ShaderNode
{
	RTTI_DECLARE_VIRTUAL_TYPE(VectorFunctionShaderNode);

public:
	enum EFunction
	{
		VECTOR_FUNCTION_LENGTH,
		VECTOR_FUNCTION_SATURATE,
		VECTOR_FUNCTION_NORMALIZE,
		VECTOR_FUNCTION_COUNT
	};

	constexpr static std::array m_FunctionNames =
	{
		"Length",
		"Saturate",
		"Normalize"
	};

	constexpr static std::array m_FunctionCode =
	{
		"length",
		"saturate",
		"normalize"
	};

	void DrawImNode(ShaderGraphBuilder& ioBuilder) override;
	String GenerateCode(ShaderGraphBuilder& inBuilder) override;

	MutSlice<ShaderNodePin> GetInputPins() override { return MutSlice(&m_InputPin, 1); }
	MutSlice<ShaderNodePin> GetOutputPins() override { return MutSlice(&m_OutputPin, 1); }

private:
	EFunction m_Function = VECTOR_FUNCTION_SATURATE;
	ShaderNodePin m_InputPin = ShaderNodePin::VECTOR;
	ShaderNodePin m_OutputPin = ShaderNodePin::VECTOR;
};


class VectorSplitShaderNode : public ShaderNode
{
	RTTI_DECLARE_VIRTUAL_TYPE(VectorSplitShaderNode);

public:
	constexpr static std::array m_VariableNames =
	{
		"       X", 
		" Y", 
		"       Z"
	};

	void DrawImNode(ShaderGraphBuilder& ioBuilder) override;
	String GenerateCode(ShaderGraphBuilder& inBuilder) override;

	MutSlice<ShaderNodePin> GetInputPins() override { return MutSlice(&m_InputPin, 1); }
	MutSlice<ShaderNodePin> GetOutputPins() override { return MutSlice(m_OutputPins); }

private:

	ShaderNodePin m_InputPin = ShaderNodePin::VECTOR;
	StaticArray<ShaderNodePin, 3> m_OutputPins = { ShaderNodePin::FLOAT, ShaderNodePin::FLOAT, ShaderNodePin::FLOAT };
};

class SingleOutputShaderNode : public ShaderNode
{
	RTTI_DECLARE_VIRTUAL_TYPE(SingleOutputShaderNode);

public:
	SingleOutputShaderNode() = default;
	SingleOutputShaderNode(StringView inTitle, StringView inOutVar, ShaderNodePin::EKind inKind) :
		m_Title(inTitle), m_OutVariableName(inOutVar), m_OutputPin(inKind) {}

	void DrawImNode(ShaderGraphBuilder& inBuilder) override;
	String GenerateCode(ShaderGraphBuilder& inBuilder) override;

	MutSlice<ShaderNodePin> GetOutputPins() { return MutSlice(&m_OutputPin, 1); }

private:
	String m_Title;
	String m_OutVariableName;
	ShaderNodePin m_OutputPin;
};


class ProcedureShaderNode : public ShaderNode
{
	RTTI_DECLARE_VIRTUAL_TYPE(ProcedureShaderNode);

public:
	void DrawImNode(ShaderGraphBuilder& inBuilder) override;
	String GenerateCode(ShaderGraphBuilder& inBuilder) override;

	void AddInputVariable(const String& inName, ShaderNodePin::EKind inKind) { m_InputNames.push_back(inName); m_InputPins.push_back(inKind); }
	void AddOutputVariable(const String& inName, ShaderNodePin::EKind inKind) { m_OutputNames.push_back(inName); m_OutputPins.push_back(inKind); }

	MutSlice<ShaderNodePin> GetInputPins() override { return MutSlice(m_InputPins); }
	MutSlice<ShaderNodePin> GetOutputPins() override { return MutSlice(m_OutputPins); }

protected:
	String m_Title = "Procedure";
	bool m_ShowInputBox = true;
	String m_Procedure = "";
	Array<String> m_InputNames;
	Array<String> m_OutputNames;
	Array<ShaderNodePin> m_InputPins;
	Array<ShaderNodePin> m_OutputPins;
};


class GradientNoiseShaderNode : public ProcedureShaderNode
{
	RTTI_DECLARE_VIRTUAL_TYPE(GradientNoiseShaderNode);

public:
	GradientNoiseShaderNode();
};


class GetTimeShaderNode : public SingleOutputShaderNode
{
	RTTI_DECLARE_VIRTUAL_TYPE(GetTimeShaderNode);

public:
	GetTimeShaderNode() : SingleOutputShaderNode("Get Time", "fc.mTime", ShaderNodePin::FLOAT) {}
};


class GetPositionShaderNode : public SingleOutputShaderNode
{
	RTTI_DECLARE_VIRTUAL_TYPE(GetPositionShaderNode);

public:
	GetPositionShaderNode() : SingleOutputShaderNode("Get Position", "vertex.mPos", ShaderNodePin::VECTOR) {}
};


class GetTexCoordShaderNode : public SingleOutputShaderNode
{
	RTTI_DECLARE_VIRTUAL_TYPE(GetTexCoordShaderNode);

public:
	GetTexCoordShaderNode() : SingleOutputShaderNode("Get TexCoord", "float3(vertex.mTexCoord, 0.0)", ShaderNodePin::VECTOR) {}
};


class GetNormalShaderNode : public SingleOutputShaderNode
{
	RTTI_DECLARE_VIRTUAL_TYPE(GetNormalShaderNode);

public:
	GetNormalShaderNode() : SingleOutputShaderNode("Get Normal", "vertex.mNormal", ShaderNodePin::VECTOR) {}
};


class GetTangentShaderNode : public SingleOutputShaderNode
{
	RTTI_DECLARE_VIRTUAL_TYPE(GetTangentShaderNode);

public:
	GetTangentShaderNode() : SingleOutputShaderNode("Get Tangent", "vertex.mTangent", ShaderNodePin::VECTOR) {}
};


class GetDeltaTimeShaderNode : public SingleOutputShaderNode
{
	RTTI_DECLARE_VIRTUAL_TYPE(GetDeltaTimeShaderNode);

public:
	GetDeltaTimeShaderNode() : SingleOutputShaderNode("Get Delta Time", "fc.mDeltaTime", ShaderNodePin::FLOAT) {}
};


class GetPixelCoordShaderNode : public SingleOutputShaderNode
{
	RTTI_DECLARE_VIRTUAL_TYPE(GetPixelCoordShaderNode);

public:
	GetPixelCoordShaderNode() : SingleOutputShaderNode("Get Pixel Coord", "input.sv_position.xyz", ShaderNodePin::VECTOR) {}
};


class GetTexCoordinateShaderNode : public SingleOutputShaderNode
{
	RTTI_DECLARE_VIRTUAL_TYPE(GetPixelCoordShaderNode);

public:
	GetTexCoordinateShaderNode() : SingleOutputShaderNode("Get Texture Coord", "float3(input.texcoord.xy, 0.0)", ShaderNodePin::VECTOR) {}
};


class CompareShaderNode : public ShaderNode
{
	RTTI_DECLARE_VIRTUAL_TYPE(CompareShaderNode);

	enum Op
	{
		OP_EQUAL,
		OP_LESS,
		OP_GREATER,
		OP_LESS_EQUAL,
		OP_GREATER_EQUAL,
	};

	static constexpr std::array m_OpSymbols =
	{
		"==",
		"<",
		">",
		"<=",
		">="
	};

	static constexpr std::array m_OpNames =
	{
		"Equal",
		"Less",
		"Greater",
		"Less Equal",
		"Greater Equal"
	};

public:
	void DrawImNode(ShaderGraphBuilder& inBuilder) override;
	String GenerateCode(ShaderGraphBuilder& inBuilder) override;

	MutSlice<ShaderNodePin> GetInputPins() override { return MutSlice(m_InputPins); }
	MutSlice<ShaderNodePin> GetOutputPins() override { return MutSlice(&m_OutputPin); }

private:
	Op m_Op = OP_EQUAL;
	ShaderNodePin m_OutputPin = ShaderNodePin::BOOL;
	StaticArray<ShaderNodePin, 2> m_InputPins = { ShaderNodePin::AUTO, ShaderNodePin::AUTO };
};


class PixelShaderOutputShaderNode : public ShaderNode
{
	RTTI_DECLARE_VIRTUAL_TYPE(PixelShaderOutputShaderNode);

public:
	static constexpr std::array m_InputNames =
	{
		"Albedo",
		"Alpha",
		"Normal",
		"Emissive",
		"Metallic",
		"Roughness",
		"Discard"
	};

	void DrawImNode(ShaderGraphBuilder& inBuilder) override;
	String GenerateCode(ShaderGraphBuilder& inBuilder) override;

	MutSlice<ShaderNodePin> GetInputPins() override { return MutSlice(m_InputPins); }

private:
	StaticArray<ShaderNodePin, 7> m_InputPins
	{
		ShaderNodePin::VECTOR,
		ShaderNodePin::FLOAT,
		ShaderNodePin::VECTOR,
		ShaderNodePin::VECTOR,
		ShaderNodePin::FLOAT,
		ShaderNodePin::FLOAT,
		ShaderNodePin::BOOL
	};

private:
	Vec4 m_Albedo;
	Vec3 m_Normal;
	Vec3 m_Emissive;
	float m_Metallic;
	float m_Roughness;
	bool m_Discard = false;
};


class VertexShaderOutputShaderNode : public ShaderNode
{
	RTTI_DECLARE_VIRTUAL_TYPE(VertexShaderOutputShaderNode);

public:
	static constexpr std::array m_InputNames =
	{
		"Position",
		"Texcoord",
		"Normal",
		"Tangent",
	};

	static constexpr std::array m_MemberNames =
	{
		"mPos",
		"mTexCoord",
		"mNormal",
		"mTangent",
	};

	void DrawImNode(ShaderGraphBuilder& inBuilder) override;
	String GenerateCode(ShaderGraphBuilder& inBuilder) override;

	MutSlice<ShaderNodePin> GetInputPins() override { return MutSlice(m_InputPins); }

private:
	StaticArray<ShaderNodePin, 7> m_InputPins
	{
		ShaderNodePin::VECTOR,
		ShaderNodePin::VECTOR,
		ShaderNodePin::VECTOR,
		ShaderNodePin::VECTOR,
	};

private:
	Vec3 m_Position;
	Vec2 m_Texcoord;
	Vec3 m_Normal;
	Vec3 m_Tangent;
};

} // raekor