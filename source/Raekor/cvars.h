#pragma once

#include "pch.h"

namespace cereal {

template<class Archive>
void load(Archive& archive, std::function<void()>& fn) {}


template<class Archive>
void save(Archive& archive, const std::function<void()>& fn) {}

} // namespace cereal

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
	CVars() {
		auto stream = std::ifstream("cvars.json");
		if (!stream.is_open())
			return;

		auto archive = cereal::JSONInputArchive(stream);
		archive(m_CVars);
	}


	~CVars() {
		auto stream = std::ofstream("cvars.json");
		auto archive = cereal::JSONOutputArchive(stream);
		archive(m_CVars);
	}


	std::string GetValue(const std::string& name) {
		if (m_CVars.find(name) == m_CVars.end())
			return {};

		try { return std::visit(GetVisitor(), m_CVars[name]); }
		catch (std::exception& e) {
			UNREFERENCED_PARAMETER(e);
			return {};
		}
	}

	template<typename T>
	[[nodiscard]] T& Create(const std::string& name, T value, bool force = false);
	void CreateFn(const std::string& name, std::function<void()> fn);
	
	template<typename T>
	[[nodiscard]] T& GetValue(const std::string& name);

	bool Exists(const std::string& inName) const { return m_CVars.find(inName) != m_CVars.end(); }

	template<typename T>
	[[nodiscard]] T* TryGetValue(const std::string& name);
	
	bool SetValue(const std::string& inName, const std::string& value);

	void ParseCommandLine(int argc, char** argv);

	inline auto end()    { return m_CVars.end();   }
	inline auto begin()  { return m_CVars.begin(); }

	size_t GetCount() { return m_CVars.size(); }

private:
	std::unordered_map<std::string, CVar> m_CVars;
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
inline T& CVars::Create(const std::string& name, T value, bool force) {
	if (m_CVars.find(name) == m_CVars.end() || force)
		m_CVars[name] = value;

	return std::get<T>(m_CVars[name]);
}


inline void CVars::CreateFn(const std::string& name, std::function<void()> fn) {
	auto ret = Create(name, fn, true);
}


template<typename T>
inline T& CVars::GetValue(const std::string& name) {
	return std::get<T>(m_CVars[name]);
}

template<typename T>
inline T* CVars::TryGetValue(const std::string& name) {
	if (m_CVars.find(name) == m_CVars.end())
		return nullptr;

	return &std::get<T>(m_CVars[name]);
}


inline bool CVars::SetValue(const std::string& inName, const std::string& inValue) {
	if (m_CVars.find(inName) == m_CVars.end())
		return false;

	try {
		SetVisitor visitor(inValue);
		return std::visit(visitor, m_CVars[inName]);
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

				if (!SetValue(cvar, value))
					std::cout << "Failed to set cvar \"" << cvar << "\" to " << value << '\n';
			}
		}
	}
}

extern CVars g_CVars;

} // raekor
