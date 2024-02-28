#pragma once

#include "application.h"
#include "timer.h"
#include "hash.h"

constexpr auto sImageFileExtensions = std::array {
	".jpg", ".jpeg", ".tga", ".png", ".dds"
};

constexpr auto sModelFileExtensions = std::array {
	".obj", ".gltf", ".fbx"
};

constexpr auto sEmbededFileExtensions = std::array {
	".ttf"
};

constexpr auto sCppFileExtensions = std::array {
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

constexpr auto sAssetTypeExtensions = std::array {
	".scene", ".dds", ".bin", ".dll"
};

namespace Raekor {

inline AssetType GetCacheFileExtension(const Path& inPath)
{
	const auto extension = inPath.extension();
	for (const auto& ext : sModelFileExtensions)
		if (extension == ext)
			return ASSET_TYPE_SCENE;

	for (const auto& ext : sImageFileExtensions)
		if (extension == ext)
			return ASSET_TYPE_IMAGE;

	for (const auto& ext : sEmbededFileExtensions)
		if (extension == ext)
			return ASSET_TYPE_EMBEDDED;

	for (const auto& ext : sCppFileExtensions)
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
		std::vector<char> data;
		const size_t filesize = size_t(file.tellg());
		data.resize(filesize);
		file.seekg(0);
		file.read((char*)&data[0], filesize);

		mFileHash = gHashFNV1a(data.data(), data.size());
	}

	void UpdateWriteTime()
	{
		auto& write_path = mIsCached ? mCachePath : mAssetPath;
		mWriteTime = std::chrono::system_clock::to_time_t(std::chrono::clock_cast<std::chrono::system_clock>( fs::last_write_time(write_path) ));
	}

	void ReadMetadata()
	{
		mIsCached = fs::exists(mCachePath) && fs::is_regular_file(mCachePath);
		UpdateWriteTime();
	}

	bool mIsCached = false;
	uint64_t mFileHash = 0;
	AssetType mAssetType;
	std::string mAssetPath;
	std::string mCachePath;
	std::time_t mWriteTime;
};

}

namespace Raekor {

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