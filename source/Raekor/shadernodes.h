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
		MATH_OP_MINUS,
		MATH_OP_MULTIPLY,
		MATH_OP_DIVIDE,
		MATH_OP_COUNT,
	};

	static constexpr std::array m_OpSymbols =
	{
		"+",
		"-",
		"*",
		"/"
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
	enum Op
	{
		MATH_OP_SIN,
		MATH_OP_COS,
		MATH_OP_TAN,
		MATH_OP_SATURATE,
		MATH_OP_COUNT
	};

	constexpr static std::array m_OpNames =
	{
		"sin",
		"cos",
		"tan",
		"saturate"
	};

	void DrawImNode(ShaderGraphBuilder& ioBuilder) override;
	String GenerateCode(ShaderGraphBuilder& inBuilder) override;

	MutSlice<ShaderNodePin> GetInputPins() override { return MutSlice(&m_InputPin, 1); }
	MutSlice<ShaderNodePin> GetOutputPins() override { return MutSlice(&m_OutputPin, 1); }

private:
	Op m_Op = MATH_OP_SIN;
	ShaderNodePin m_InputPin = ShaderNodePin::FLOAT;
	ShaderNodePin m_OutputPin = ShaderNodePin::FLOAT;
};


class VectorShaderNode : public ShaderNode
{
protected:

	constexpr static std::array m_VectorKinds =
	{
		ShaderNodePin::FLOAT2, ShaderNodePin::FLOAT3, ShaderNodePin::FLOAT4,
	};

	constexpr static std::array m_VectorNames =
	{
		"Vec2", "Vec3", "Vec4"
	};

	constexpr static std::array m_VectorComponents =
	{
		2, 3, 4
	};

	constexpr static std::array m_VectorInputFunctions =
	{
		&ImGui::InputFloat2, &ImGui::InputFloat3, &ImGui::InputFloat4
	};

	int GetKindIndex() const { return int(m_Kind) - ShaderNodePin::FLOAT2; }

	auto GetVectorName() const { return m_VectorNames[GetKindIndex()]; }
	auto GetVectorComponents() const { return m_VectorComponents[GetKindIndex()]; }
	auto GetVectorInputFunction() const { return m_VectorInputFunctions[GetKindIndex()]; }

protected:
	ShaderNodePin::EKind m_Kind = ShaderNodePin::FLOAT4;
};


class VectorValueShaderNode : public VectorShaderNode
{
	RTTI_DECLARE_VIRTUAL_TYPE(VectorValueShaderNode);

public:
	void DrawImNode(ShaderGraphBuilder& inBuilder) override;
	String GenerateCode(ShaderGraphBuilder& inBuilder) override;

	MutSlice<ShaderNodePin> GetInputPins() override { return MutSlice(m_InputPins); }
	MutSlice<ShaderNodePin> GetOutputPins() override { return MutSlice(m_OutputPins); }

private:
	Vec4 m_Vector = Vec4(0.0f, 0.0f, 0.0f, 1.0f);
	StaticArray<ShaderNodePin, 4> m_InputPins = { ShaderNodePin::FLOAT, ShaderNodePin::FLOAT, ShaderNodePin::FLOAT, ShaderNodePin::FLOAT };
	StaticArray<ShaderNodePin, 1> m_OutputPins = { ShaderNodePin::FLOAT4 };
};


class VectorOpShaderNode : public VectorShaderNode
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

	void DrawImNode(ShaderGraphBuilder& inBuilder) override;
	String GenerateCode(ShaderGraphBuilder& inBuilder) override;

	MutSlice<ShaderNodePin> GetInputPins() { return MutSlice(m_InputPins); }
	MutSlice<ShaderNodePin> GetOutputPins() { return MutSlice(m_OutputPins); }

private:
	StaticArray<ShaderNodePin, 2> m_InputPins = { ShaderNodePin::FLOAT4, ShaderNodePin::FLOAT4 };
	StaticArray<ShaderNodePin, 1> m_OutputPins = { ShaderNodePin::FLOAT4 };

private:
	Op m_Op = VECTOR_OP_ADD;
	Vec4 m_VectorA = Vec4(0.0f, 0.0f, 0.0f, 1.0f);
	Vec4 m_VectorB = Vec4(0.0f, 0.0f, 0.0f, 1.0f);
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

	MutSlice<ShaderNodePin> GetInputPins() override { return MutSlice(m_InputPins); }
	MutSlice<ShaderNodePin> GetOutputPins() override { return MutSlice(m_OutputPins); }

private:
	String m_Title = "Procedure";
	bool m_ShowInputBox = true;
	String m_Procedure = "";
	std::vector<ShaderNodePin> m_InputPins;
	std::vector<ShaderNodePin> m_OutputPins;
};


class GetTimeShaderNode : public SingleOutputShaderNode
{
	RTTI_DECLARE_VIRTUAL_TYPE(GetTimeShaderNode);

public:
	GetTimeShaderNode() : SingleOutputShaderNode("Get Time", "fc.mTime", ShaderNodePin::FLOAT) {}
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
	GetPixelCoordShaderNode() : SingleOutputShaderNode("Get Pixel Coord", "input.sv_position.xy", ShaderNodePin::FLOAT2) {}
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
	StaticArray<ShaderNodePin, 6> m_InputPins
	{
		ShaderNodePin::FLOAT4,
		ShaderNodePin::FLOAT3,
		ShaderNodePin::FLOAT3,
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

} // raekor