#pragma once

#include "json.h"
#include "rtti.h"

namespace Raekor::JSON {

class ReadArchive {
public:
	ReadArchive(const Path& inPath);

	template<typename T>
	ReadArchive& operator>> (T& rhs) {
		if (m_ObjectIndex >= m_Parser.Count())
			return *this;

		if (!ParseObject(m_Parser.Get(m_ObjectIndex), &rhs))
			return *this; // something went wrong

		return *this;
	}


	bool ParseObject(const JSON::Object& inObject, void* inInstance) {
		if (m_ObjectIndex >= m_Parser.Count())
			return false;

		if (!m_Parser.Contains(inObject, "Type"))
			return false;

		const auto& type_value = m_Parser.GetValue(inObject, "Type");
		const auto& type_string = type_value.As<JSON::String>().ToString();
		auto rtti = RTTIFactory::GetRTTI(type_string.c_str());

		if (!rtti)
			return false;

		for (uint32_t i = 0; i < rtti->GetMemberCount(); i++) {
			auto member = rtti->GetMember(i);
			assert(member);

			if (inObject.find(member->GetCustomName()) != inObject.end()) {
				auto& member_value = inObject.at(member->GetCustomName());
				
				// recursive call, will blow up eventually TODO: FIXME
				if (member_value.mType == JSON::ValueType::Object) {
					if (!ParseObject(m_Parser.Get(++m_ObjectIndex), member->GetMember(inInstance)))
						return false;
				} 
				else {
					member->FromJSON(member_value, inInstance);
				}

			}
		}

		return true;
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