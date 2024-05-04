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

constexpr std::array sCppFileExtensions = {
	".cpp"
};

enum AssetType
{
	ASSET_TYPE_SCENE,
	ASSET_TYPE_IMAGE,
	ASSET_TYPE_EMBEDDED,
	ASSET_TYPE_CPP_SCRIPT,
	ASSET_TYPE_NONE
};

constexpr std::array sAssetTypeExtensions = {
	".scene", ".dds", ".bin", ".dll"
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

	for (const char* ext : sCppFileExtensions)
		if (extension == ext)
			return ASSET_TYPE_CPP_SCRIPT;

	return ASSET_TYPE_NONE;
}

struct FileEntry
{
	FileEntry(const Path& inAssetPath)
		: mAssetPath(inAssetPath.string()), mAssetType(GetCacheFileExtension(inAssetPath))
	{
		mCachePath = fs::relative(inAssetPath).replace_extension(sAssetTypeExtensions[(int)mAssetType]).string().replace(0, 6, "cached");
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
	virtual void LogMessage(const std::string& inMessage) override;

	void OpenFromTray();
	HWND GetWindowHandle();

private:
	uint64_t m_StartTicks = 0;
	uint64_t m_FinishedTicks = 0;
	Path m_CurrentPath;
	bool m_WasClosed = false;
	int m_ResizeCounter = 0;
	int m_SelectedIndex = -1;
	SDL_Renderer* m_Renderer;
	uint32_t m_NrOfFilesInFlight = 0;
	std::mutex m_FilesInFlightMutex;
	std::set<uint32_t> m_FilesInFlight;
	std::vector<FileEntry> m_Files;
	std::set<fs::path> m_CachedFiles;
	std::atomic<bool> m_CompileScenes = true;
	std::atomic<bool> m_CompileScripts = true;
	std::atomic<bool> m_CompileTextures = true;
};


}