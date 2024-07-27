#pragma once

#include "pch.h"
#include "rtti.h"

namespace RK {

enum EConVarType
{
	CVAR_TYPE_ERROR,
	CVAR_TYPE_INT,
	CVAR_TYPE_FLOAT,
	CVAR_TYPE_STRING
};


class ConVar
{
	RTTI_DECLARE_TYPE(ConVar);

    using Func = std::function<void(void)>;

public:
	ConVar() : mType(CVAR_TYPE_ERROR) {}
	ConVar(int inValue) : mType(CVAR_TYPE_INT), mIntValue(inValue) {}
	ConVar(float inValue) : mType(CVAR_TYPE_FLOAT), mFloatValue(inValue) {}
	ConVar(const std::string& inValue) : mType(CVAR_TYPE_STRING), mStringValue(inValue) {}
	ConVar(std::function<void(void)> inValue) : mType(CVAR_TYPE_ERROR), mFuncValue(inValue) {}

	template<typename T> T& GetValue();
	template<> int& GetValue() { return mIntValue; }
	template<> float& GetValue() { return mFloatValue; }
	template<> std::string& GetValue() { return mStringValue; }
	template<> std::function<void(void)>& GetValue() { return mFuncValue; }

	inline EConVarType GetType() const { return mType; }

	std::string ToString() const;
	bool SetValue(const std::string& inValue);

public:
	EConVarType mType;
	int mIntValue = 0;
	float mFloatValue = 0;
	std::string mStringValue;
	std::function<void(void)> mFuncValue; // not serialized
};


class ConVars
{
	RTTI_DECLARE_TYPE(ConVars);

public:
	ConVars();
	~ConVars();

	bool Exists(const std::string& inName) const;

	std::string GetValue(const std::string& inName);

	template<typename T>
	inline T& Create(const std::string& inName, T inValue, bool inForce = false);

	void CreateFn(const std::string& inName, std::function<void()> fn);

	template<typename T>
	inline T& GetValue(const std::string& inName);

	ConVar& GetCVar(const std::string& inName);

	template<typename T>
	inline T* TryGetValue(const std::string& inName);

	bool SetValue(const std::string& inName, const std::string& inValue);

	void ParseCommandLine(int inArgc, char** inArgv);

	size_t GetCount() { return m_ConVars.size(); }

	inline auto end() { return m_ConVars.end(); }
	inline auto begin() { return m_ConVars.begin(); }

private:
	std::unordered_map<std::string, ConVar> m_ConVars;
};


extern ConVars g_CVars;


template<typename T>
inline T& ConVars::Create(const std::string& inName, T value, bool force)
{
	if (m_ConVars.find(inName) == m_ConVars.end() || force)
		m_ConVars[inName] = ConVar(value);

	return m_ConVars[inName].GetValue<T>();
}


template<typename T>
inline T& ConVars::GetValue(const std::string& inName)
{
	return m_ConVars[inName].GetValue<T>();
}


template<typename T>
inline T* ConVars::TryGetValue(const std::string& inName)
{
	if (m_ConVars.find(inName) == m_ConVars.end())
		return nullptr;

	return &m_ConVars[inName].GetValue<T>();
}

} // raekor
