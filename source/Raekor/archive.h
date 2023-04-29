#pragma once

#include "json.h"

namespace Raekor::JSON {

class ReadArchive {
public:
	ReadArchive(const Path& inPath);

	template<typename T>
	ReadArchive& operator>> (T& rhs) {
		if (m_ObjectIndex >= m_Parser.Count())
			return *this;

		const auto& object = m_Parser.Get(0);
		if (!m_Parser.Contains(object, "Type"))
			return *this;

		const auto& type_value = m_Parser.GetValue(object, "Type");
		const auto& type_string = type_value.As<JSON::String>().ToString();
		auto rtti = RTTIFactory::GetRTTI(type_string.c_str());

		if (!rtti || *rtti != RTTI_OF(T)) 
			return *this;

		for (uint32_t i = 0; i < rtti->GetMemberCount(); i++) {
			auto member = rtti->GetMember(i);
			assert(member);

			if (object.find(member->GetCustomName()) != object.end())
				member->FromJSON(object.at(member->GetCustomName()), &rhs);
		}

		return *this;
	}

private:
	uint32_t m_ObjectIndex = 0;
	JSON::Parser m_Parser;
};



class WriteArchive {
public:
	WriteArchive(const Path& inPath);

	template<typename T>
	WriteArchive& operator<< (T& rhs) {
		auto obj = JSON::ObjectBuilder();
		auto& rtti = RTTI_OF(T);

		obj.WritePair("Type", rtti.GetTypeName());

		for (int i = 0; i < rtti.GetMemberCount(); i++) {
			auto member = rtti.GetMember(i);

			if (member) {
				JSON::Value value;
				member->ToJSON(value, &rhs);
				obj.WritePair(member->GetCustomName(), value);
			}
		}

		m_Ofs << obj.AsString() << "\n\n";

		return *this;
	}

private:
	std::ofstream m_Ofs;
};



} // Raekor::JSON