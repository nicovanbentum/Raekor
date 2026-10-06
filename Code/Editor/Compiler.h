#pragma once

#include "application.h"
#include "timer.h"
#include "hash.h"

constexpr std::array sImageFileExtensions = {
	".jpg", ".jpeg", ".tga", ".png", ".dds"
};

constexpr std::array sModelFileExtensions = {
	".obj", ".gltf", ".fbx"
};

constexpr std::array sEmbededFileExtensions = {
	".ttf"
};

enum AssetType
{
	ASSET_TYPE_SCENE,
	ASSET_TYPE_IMAGE,
	ASSET_TYPE_EMBEDDED,
	ASSET_TYPE_NONE
};

constexpr std::array sAssetTypeExtensions = {
	".scene", ".dds", ".bin"
};

namespace RK {

inline AssetType GetCacheFileExtension(const Path& inPath)
{
	const Path extension = inPath.extension();
	for (const char* ext : sModelFileExtensions)
		if (extension == ext)
			return ASSET_TYPE_SCENE;

	for (const char* ext : sImageFileExtensions)
		if (extension == ext)
			return ASSET_TYPE_IMAGE;

	for (const char* ext : sEmbededFileExtensions)
		if (extension == ext)
			return ASSET_TYPE_EMBEDDED;

	return ASSET_TYPE_NONE;
}

struct FileEntry
{
	FileEntry(const Path& inAssetPath)
		: mAssetPath(inAssetPath.string()), mAssetType(GetCacheFileExtension(inAssetPath))
	{
		mCachePath = Asset::GetCachedPath(mAssetPath, sAssetTypeExtensions[(int)mAssetType]);
	}

	void UpdateFileHash()
	{
		std::ifstream file(mAssetPath, std::ios::ate | std::ios::binary);

		const size_t filesize = size_t(file.tellg());
		Array<char> data = Array<char>(filesize);
		
		file.seekg(0);
		file.read((char*)&data[0], filesize);

		mFileHash = gHashFNV1a(data.data(), data.size());
	}

	void UpdateWriteTime()
	{
		String& write_path = mIsCached ? mCachePath : mAssetPath;
		mWriteTime = std::chrono::system_clock::to_time_t(std::chrono::clock_cast<std::chrono::system_clock>( fs::last_write_time(write_path) ));
	}

	void ReadMetadata()
	{
		mIsCached = fs::exists(mCachePath) && fs::is_regular_file(mCachePath);
		UpdateWriteTime();
	}

	bool mIsCached = false;
	AssetType mAssetType = ASSET_TYPE_NONE;
	uint64_t mFileHash = 0;

	String mAssetPath;
	String mCachePath;
	std::time_t mWriteTime;
};

}

namespace RK {

class CompilerApp : public Application
{
public:
	CompilerApp(WindowFlags inFlags);
	~CompilerApp();

	virtual void OnUpdate(float inDeltaTime) override;
	virtual void OnEvent(const SDL_Event& inEvent) override;
	virtual bool OnCloseRequested() override;
	virtual bool IsPausedWhenMinimized() const override { return false; }

	void OpenFromTray();
	void ShowTrayMenu();
	HWND GetWindowHandle();

private:
	void ScheduleCompilation();
	bool IsConversionEnabled(AssetType inType) const;
	void SortFiles(int inColumn, bool inAscending);

	void DrawHeader();
	void DrawToolbar();
	void DrawFileTable();
	void DrawClearCachePopup();

	void OpenFile(const FileEntry& inFile);
	void ShowInExplorer(const FileEntry& inFile);
	void Recompile(uint32_t inIndex);

	uint32_t m_IPCLogSink = 0;
	uint64_t m_StartTicks = 0;
	uint64_t m_FinishedTicks = 0;
	int m_SelectedIndex = -1;
	SDL_Renderer* m_Renderer;
	std::mutex m_FilesInFlightMutex;
	HashSet<uint32_t> m_FilesInFlight;
	Array<FileEntry> m_Files;
	Array<uint32_t> m_SortedFiles;
	String m_Filter;
	int m_TypeFilter = -1;
	bool m_OpenClearCachePopup = false;
	std::atomic<bool> m_CompileScenes = true;
	std::atomic<bool> m_CompileTextures = true;
};


}