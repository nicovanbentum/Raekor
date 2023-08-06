#include "pch.h"
#include "cvars.h"
#include "archive.h"

namespace Raekor {

RTTI_CLASS_CPP(ConVar) {
	RTTI_MEMBER_CPP(ConVar, SERIALIZE_ALL, "Type",   mType);
	RTTI_MEMBER_CPP(ConVar, SERIALIZE_ALL, "Int",    mIntValue);
	RTTI_MEMBER_CPP(ConVar, SERIALIZE_ALL, "Float",  mFloatValue);
	RTTI_MEMBER_CPP(ConVar, SERIALIZE_ALL, "String", mStringValue);
}

RTTI_CLASS_CPP(ConVars) {
	RTTI_MEMBER_CPP(ConVars, SERIALIZE_ALL, "Variables", m_ConVars);
}


ConVars g_CVars;


ConVars::ConVars() {
	auto stream = std::ifstream("cvars.json");
	if (!stream.is_open() || FileSystem::file_size("cvars.json") == 0)
		return;

	auto json_data = JSON::JSONData("cvars.json");
	auto json_archive = JSON::ReadArchive(json_data);
	json_archive >> g_CVars;
}


ConVars::~ConVars() {
	auto json_archive = JSON::WriteArchive("cvars.json");
	json_archive << g_CVars;
}



std::string ConVar::ToString() const {
	switch (mType) {
	case CVAR_TYPE_INT: return std::to_string(mIntValue);
		break;
	case CVAR_TYPE_FLOAT: return std::to_string(mFloatValue);
		break;
	case CVAR_TYPE_STRING: return mStringValue;
		break;
	default:
		return {};
	}

	return {};
}


bool ConVar::SetValue(const std::string& inValue) {
	switch (mType) {
	case CVAR_TYPE_INT: mIntValue = std::stoi(inValue);
		return true;
	case CVAR_TYPE_FLOAT: mFloatValue = std::stof(inValue);
		return true;
	case CVAR_TYPE_STRING: mStringValue = inValue;
		return true;
	default:
		return false;
	}

	return false;
}


bool ConVars::Exists(const std::string& inName) const {
	return m_ConVars.find(inName) != m_ConVars.end();
}


std::string ConVars::GetValue(const std::string& inName) {
	if (m_ConVars.find(inName) == m_ConVars.end())
		return {};

	return m_ConVars[inName].ToString();
}

void ConVars::CreateFn(const std::string& name, std::function<void()> fn) {
	auto ret = Create(name, fn, true);
}


bool ConVars::SetValue(const std::string& inName, const std::string& inValue) {
	if (m_ConVars.find(inName) == m_ConVars.end())
		return false;

	try {
		return m_ConVars[inName].SetValue(inValue);
	}
	catch (...) {
		return false;
	}
}


void ConVars::ParseCommandLine(int argc, char** argv) {
	for (int i = 0; i < argc; i++) {
		if (argv[i][0] != '-')
			continue;

		const auto string = std::string(argv[i]);
		const auto equals_pos = string.find('=');

		if (equals_pos == std::string::npos)
			continue;
		
		const auto cvar = string.substr(1, equals_pos - 1);
		const auto value = string.substr(equals_pos + 1);

		if (!SetValue(cvar, value))
			std::cout << "Failed to set cvar \"" << cvar << "\" to " << value << '\n';
		else {
			std::cout << "Succesfully set cvar \"" << cvar << "\" to " << value << '\n';
		}
	}
}

} // Raekor