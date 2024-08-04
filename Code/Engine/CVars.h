#pragma once

#include "pch.h"
#include "rtti.h"

namespace RK {

enum CVarKind
{
	CVAR_TYPE_NONE,
	CVAR_TYPE_INT,
	CVAR_TYPE_FLOAT,
	CVAR_TYPE_STRING,
	CVAR_TYPE_FUNCTION
};

class CVar
{
	RTTI_DECLARE_TYPE(CVar);
	using Function = std::function<void(void)>;

public:
	CVar() : mType(CVAR_TYPE_NONE) {}
	CVar(int inValue) : mType(CVAR_TYPE_INT), mIntValue(inValue) {}
	CVar(float inValue) : mType(CVAR_TYPE_FLOAT), mFloatValue(inValue) {}
	CVar(const String& inValue) : mType(CVAR_TYPE_STRING), mStringValue(inValue) {}
	CVar(const Function& inValue) : mType(CVAR_TYPE_FUNCTION), mFunctionValue(inValue) {}

	template<typename T> T& GetValue();
	template<> int& GetValue() { return mIntValue; }
	template<> float& GetValue() { return mFloatValue; }
	template<> String& GetValue() { return mStringValue; }
	template<> Function& GetValue() { return mFunctionValue; }

	String ToString() const;
	bool SetValue(const String& inValue);

public:
	CVarKind mType;
	int mIntValue = 0;
	float mFloatValue = 0;
	String mStringValue;
	Function mFunctionValue; // not serialized
};


class CVariables
{
	RTTI_DECLARE_TYPE(CVariables);

public:
	CVariables() = default;
	CVariables(int argc, char** argv);
	~CVariables();

	bool Exists(const String& inName) const;

	String GetValue(const String& inName) const;

	template<typename T>
	inline T& Create(const String& inName, T inValue, bool inForce = false);

	void CreateFn(const String& inName, CVar::Function fn);

	template<typename T>
	inline T& GetValue(const String& inName);

	CVar& GetCVar(const String& inName);

	template<typename T>
	inline T* TryGetValue(const String& inName);

	bool SetValue(const String& inName, const String& inValue);

	size_t GetCount() const { return m_ConVars.size(); }
	const HashMap<String, CVar>& GetCVars() const { return m_ConVars; }

private:
	HashMap<String, CVar> m_ConVars;
};

template<typename T>
inline T& CVariables::Create(const std::string& inName, T value, bool force)
{
	if (m_ConVars.find(inName) == m_ConVars.end() || force)
		m_ConVars[inName] = CVar(value);

	return m_ConVars[inName].GetValue<T>();
}

template<typename T>
inline T& CVariables::GetValue(const std::string& inName)
{
	return m_ConVars[inName].GetValue<T>();
}

template<typename T>
inline T* CVariables::TryGetValue(const std::string& inName)
{
	if (m_ConVars.find(inName) == m_ConVars.end())
		return nullptr;

	return &m_ConVars[inName].GetValue<T>();
}

inline CVariables* g_CVariables = nullptr;

} // raekor
