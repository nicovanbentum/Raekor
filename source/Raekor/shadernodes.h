#pragma once

#include "rtti.h"
#include "shader.h"

namespace Raekor {

void gRegisterShaderNodeTypes();

class MathOpShaderNode : public ShaderNode
{
	RTTI_DECLARE_VIRTUAL_TYPE(MathOpShaderNode);

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
	StaticArray<ShaderNodePin, 2> m_InputPins;
	StaticArray<ShaderNodePin, 1> m_OutputPins;

private:
	Op m_Op;
	float m_FloatA;
	float m_FloatB;
};

class MathFuncShaderNode : public ShaderNode
{
	RTTI_DECLARE_VIRTUAL_TYPE(MathFuncShaderNode);

public:
	enum Op
	{
		MATH_OP_SIN,
		MATH_OP_COS,
		MATH_OP_TAN,
		MATH_OP_COUNT
	};

	constexpr static std::array m_OpNames =
	{
		"sin",
		"cos",
		"tan"
	};

	void DrawImNode(ShaderGraphBuilder& ioBuilder) override;
	String GenerateCode(ShaderGraphBuilder& inBuilder) override;

	MutSlice<ShaderNodePin> GetInputPins() override { return MutSlice(&m_InputPin, 1); }
	MutSlice<ShaderNodePin> GetOutputPins() override { return MutSlice(&m_OutputPin, 1); }

private:
	Op m_Op = MATH_OP_SIN;
	ShaderNodePin m_InputPin;
	ShaderNodePin m_OutputPin;
};


enum EVectorVariant
{
	VEC2, VEC3, VEC4, COUNT
};
RTTI_DECLARE_ENUM(EVectorVariant);

class VectorShaderNode : public ShaderNode
{
protected:

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
};


class VectorComposeShaderNode : public VectorShaderNode
{
	RTTI_DECLARE_VIRTUAL_TYPE(VectorComposeShaderNode);

public:
	void DrawImNode(ShaderGraphBuilder& inBuilder) override;

	String GenerateCode(ShaderGraphBuilder& inBuilder) override;

	MutSlice<ShaderNodePin> GetInputPins() override { return MutSlice(m_InputPins); }
	MutSlice<ShaderNodePin> GetOutputPins() override { return MutSlice(m_OutputPins); }

private:
	Vec4 m_Vector = Vec4(0, 0, 0, 0);
	EVectorVariant m_VectorVariant = VEC4;

	StaticArray<ShaderNodePin, 4> m_InputPins;
	StaticArray<ShaderNodePin, 1> m_OutputPins;
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
	StaticArray<ShaderNodePin, 2> m_InputPins;
	StaticArray<ShaderNodePin, 1> m_OutputPins;

private:
	Op m_Op;
	EVectorVariant m_VectorVariant;
	Vec4 m_VectorA;
	Vec4 m_VectorB;
};


class SingleOutputShaderNode : public ShaderNode
{
	RTTI_DECLARE_VIRTUAL_TYPE(SingleOutputShaderNode);

public:
	virtual const char* GetTitle() { return "@fixme"; };
	virtual const char* GetOutVariableName() { return "@fixme"; };

	void DrawImNode(ShaderGraphBuilder& inBuilder) override;
	String GenerateCode(ShaderGraphBuilder& inBuilder) override;

	MutSlice<ShaderNodePin> GetOutputPins() { return MutSlice(&m_OutputPin, 1); }

private:
	ShaderNodePin m_OutputPin;
};


class GetTimeShaderNode : public SingleOutputShaderNode
{
	RTTI_DECLARE_VIRTUAL_TYPE(GetTimeShaderNode);

public:
	virtual const char* GetTitle() { return "Get Time"; };
	virtual const char* GetOutVariableName() { return "fc.mTime"; };
};


class GetDeltaTimeShaderNode : public SingleOutputShaderNode
{
	RTTI_DECLARE_VIRTUAL_TYPE(GetDeltaTimeShaderNode);

public:
	virtual const char* GetTitle() { return "Get Delta Time"; };
	virtual const char* GetOutVariableName() { return "fc.mDeltaTime"; };
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
		"Roughness"
	};

	void DrawImNode(ShaderGraphBuilder& inBuilder) override;
	String GenerateCode(ShaderGraphBuilder& inBuilder) override;

	MutSlice<ShaderNodePin> GetInputPins() override { return MutSlice(m_InputPins); }

private:
	StaticArray<ShaderNodePin, 5> m_InputPins;

private:
	Vec4 m_Albedo;
	Vec3 m_Normal;
	Vec3 m_Emissive;
	float m_Metallic;
	float m_Roughness;
};

} // raekor