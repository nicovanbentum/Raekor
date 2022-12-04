#pragma once

namespace Raekor {

class RTTI;

class ArchiveOut {
public:
	template<typename T>
	void SaveClass(void* inClass) {

	}
	
private:
	virtual void Save(void* inClass, const RTTI& inRTTI);
};



class ArchiveIn {

};



} // Raekor