#include "PCH.h"
#include "CVars.h"
#include "Member.h"
#include "Archive.h"

namespace RK {

RTTI_DEFINE_TYPE(CVar)
{
	RTTI_DEFINE_MEMBER(CVar, SERIALIZE_ALL, "Type", mType);
	RTTI_DEFINE_MEMBER(CVar, SERIALIZE_ALL, "Int", mIntValue);
	RTTI_DEFINE_MEMBER(CVar, SERIALIZE_ALL, "Float", mFloatValue);
	RTTI_DEFINE_MEMBER(CVar, SERIALIZE_ALL, "String", mStringValue);
}

RTTI_DEFINE_TYPE(CVariables)
{
	RTTI_DEFINE_MEMBER(CVariables, SERIALIZE_ALL, "Variables", m_ConVars);
}


CVariables::CVariables(int argc, char** argv)
{
	std::ifstream stream = std::ifstream("cvars.json");
	if (!stream.is_open() || fs::file_size("cvars.json") == 0)
		return;

	JSON::ReadArchive json_archive = JSON::ReadArchive("cvars.json");
	json_archive >> *this;

	for (int i = 0; i < argc; i++)
	{
		if (argv[i][0] != '-')
			continue;

		const String string = String(argv[i]);
		const size_t equals_pos = string.find('=');

		if (equals_pos == std::string::npos)
			continue;

		const String cvar = string.substr(1, equals_pos - 1);
		const String value = string.substr(equals_pos + 1);

		if (!SetValue(cvar, value))
			std::cout << "[Engine] Failed to set cvar \"" << cvar << "\" to " << value << '\n';
		else
		{
			std::cout << "[Engine] Succesfully set cvar \"" << cvar << "\" to " << value << '\n';
		}
	}
}


CVariables::~CVariables()
{
	JSON::WriteArchive json_archive = JSON::WriteArchive("cvars.json");
	json_archive << *this;
}


String CVar::ToString() const
{
	switch (mType)
	{
		case CVAR_TYPE_INT: 
			return std::to_string(mIntValue);
		case CVAR_TYPE_FLOAT: 
			return std::to_string(mFloatValue);
		case CVAR_TYPE_STRING: 
			return mStringValue;
		default:
			return {};
	}

	return {};
}


bool CVar::SetValue(const String& inValue)
{
	switch (mType)
	{
		case CVAR_TYPE_INT: 
			std::from_chars(inValue.data(), inValue.data() + inValue.size(), mIntValue);
			return true;
		case CVAR_TYPE_FLOAT: 
			std::from_chars(inValue.data(), inValue.data() + inValue.size(), mFloatValue);
			return true;
		case CVAR_TYPE_STRING: 
			mStringValue = inValue;
			return true;
        case CVAR_TYPE_FUNCTION:
            mFunctionValue();
            return true;
		default:
			return false;
	}

	return false;
}


bool CVariables::Exists(const String& inName) const
{
	return m_ConVars.find(inName) != m_ConVars.end();
}


String CVariables::GetValue(const String& inName) const
{
	if (m_ConVars.find(inName) == m_ConVars.end())
		return {};

	return m_ConVars.at(inName).ToString();
}


void CVariables::CreateFn(const String& inName, CVar::Function inFunction)
{
	CVar::Function function = Create(inName, inFunction, true);
}


CVar& CVariables::GetCVar(const String& inName) 
{
	assert(m_ConVars.contains(inName));
	return m_ConVars[inName]; 
}


bool CVariables::SetValue(const String& inName, const String& inValue)
{
	if (m_ConVars.find(inName) == m_ConVars.end())
		return false;

	try
	{
		return m_ConVars[inName].SetValue(inValue);
	}
	catch (...)
	{
		return false;
	}
}

} // Raekor