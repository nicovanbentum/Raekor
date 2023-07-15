#pragma once

#include "pch.h"
#include "serial.h"

#define DECLARE_CVAR(type, name, value) type& name = CVars::sCreate(#name, value)

namespace Raekor {

typedef std::variant<int, float, std::string, std::function<void()>> CVar;

template<class Archive>
void serialize(Archive& archive, CVar& cvar) {
	archive(cvar);
}



class CVars {
private:
	struct GetVisitor {
		template<typename T> 
		inline std::string operator()(T& value) { return std::to_string(value); }
		template<> std::string operator()(std::string& value) { return value; }
		template<> std::string operator()(std::function<void()>& value) { return {}; }
	};


	struct SetVisitor {
		inline SetVisitor(const std::string& value) : m_Value(value) {}

		template<typename T> bool operator()(T& cvar) { return false; }

		template<> bool operator()(int& cvar);
		template<> bool operator()(float& cvar);
		template<> bool operator()(std::string& cvar);
		template<> bool operator()(std::function<void()>& cvar);

		const std::string& m_Value;
	};

public:
	CVars();
	~CVars();

	template<typename T>
	[[nodiscard]] static T& sCreate(const std::string& name, T value, bool force = false);
	static void sCreateFn(const std::string& name, std::function<void()> fn);
	
	template<typename T>
	[[nodiscard]] static T& sGetValue(const std::string& name);

	template<typename T>
	[[nodiscard]] static T* sTryGetValue(const std::string& name);
	
	static std::string sGetValue(const std::string& name);
	static bool sSetValue(std::string name, const std::string& value);

	static void ParseCommandLine(int argc, char** argv);

	inline auto end()    { return cvars.end();   }
	inline auto begin()  { return cvars.begin(); }

	static CVars& sGet() { return *global.get(); }
	static size_t sGetCount() { return global->cvars.size(); }

private:
	std::unordered_map<std::string, CVar> cvars;

private:
	inline static std::unique_ptr<CVars> global = std::make_unique<CVars>();
};



template<> inline bool CVars::SetVisitor::operator()(std::function<void()>& cvar) {
	cvar();
	return true;
}

template<> inline bool CVars::SetVisitor::operator()(std::string& cvar) {
	cvar = m_Value;
	return true;
}

template<> inline bool CVars::SetVisitor::operator()(int& cvar) {
	cvar = std::stoi(m_Value);
	return true;
}

template<> inline bool CVars::SetVisitor::operator()(float& cvar) {
	cvar = std::stof(m_Value);
	return true;
}



template<typename T>
inline T& CVars::sCreate(const std::string& name, T value, bool force) {
	if (global->cvars.find(name) == global->cvars.end() || force)
		global->cvars[name] = value;

	return std::get<T>(global->cvars[name]);
}


inline void CVars::sCreateFn(const std::string& name, std::function<void()> fn) {
	auto ret = sCreate(name, fn, true);
}


template<typename T>
inline T& CVars::sGetValue(const std::string& name) {
	return std::get<T>(global->cvars[name]);
}

template<typename T>
inline T* CVars::sTryGetValue(const std::string& name) {
	if (global->cvars.find(name) == global->cvars.end())
		return nullptr;

	return &std::get<T>(global->cvars[name]);
}


inline bool CVars::sSetValue(std::string name, const std::string& value) {
	if (global->cvars.find(name) == global->cvars.end())
		return false;

	try {
		SetVisitor visitor(value);
		return std::visit(visitor, global->cvars[name]);
	}
	catch (...) {
		return false;
	}
}


inline void CVars::ParseCommandLine(int argc, char** argv) {
	for (int i = 0; i < argc; i++) {
		if (argv[i][0] == '-') {
			const auto string = std::string(argv[i]);
			const auto equals_pos = string.find('=');

			if (equals_pos != std::string::npos) {
				const auto cvar = string.substr(1, equals_pos - 1);
				const auto value = string.substr(equals_pos + 1);

				if (!CVars::sSetValue(cvar, value))
					std::cout << "Failed to set cvar \"" << cvar << "\" to " << value << '\n';
			}
		}
	}
}

} // raekor
