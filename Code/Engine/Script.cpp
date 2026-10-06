#include "pch.h"
#include "script.h"
#include "input.h"
#include "scene.h"
#include "assets.h"
#include "application.h"
#include "OS.h"

// this once contained chaiscript, but in the future I'll mainly support native code or integrate Mono for C#

namespace RK {

RTTI_DEFINE_TYPE_NO_FACTORY(INativeScript) {}

Scene* INativeScript::GetScene() { return m_App->GetScene(); }

Assets* INativeScript::GetAssets() { return m_App->GetAssets(); }

Physics* INativeScript::GetPhysics() { return m_App->GetPhysics(); }

IRenderInterface* INativeScript::GetRenderInterface() { return m_App->GetRenderInterface(); }

void INativeScript::Log(const std::string& inText) { gLogInfo("Script", "{}", inText); }



static bool sIsFileReadyToLoad(const Path& inPath)
{
	HANDLE file = CreateFileW(inPath.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

	if (file == INVALID_HANDLE_VALUE)
		return false;

	CloseHandle(file);
	return true;
}



static bool sIsProcessRunning(DWORD inProcessID)
{
	HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, inProcessID);

	if (process == nullptr)
		return GetLastError() == ERROR_ACCESS_DENIED;

	DWORD exit_code = 0;
	const bool is_running = GetExitCodeProcess(process, &exit_code) && exit_code == STILL_ACTIVE;

	CloseHandle(process);
	return is_running;
}



static void sRemoveStaleModuleCopies(const Path& inDirectory)
{
	std::error_code error_code;

	for (const fs::directory_entry& entry : fs::directory_iterator(inDirectory, error_code))
	{
		const String name = entry.path().filename().string();
		const size_t separator = name.find('_');

		DWORD process_id = 0;
		const auto [end, result] = std::from_chars(name.data(), name.data() + ( separator == String::npos ? name.size() : separator ), process_id);

		if (result != std::errc() || process_id == GetCurrentProcessId() || sIsProcessRunning(process_id))
			continue;

		fs::remove_all(entry.path(), error_code);
	}
}



Path ScriptModule::sGetDefaultModulePath()
{
	return OS::sGetExecutablePath().parent_path() / "Scripts.dll";
}



bool ScriptModule::Load(const Path& inModulePath)
{
	Unload();

	m_ModulePath = inModulePath;

	std::error_code error_code;

	if (!fs::exists(m_ModulePath, error_code))
	{
		gLogWarning("Scripts", "Script module {} does not exist, build the Scripts project to create it", m_ModulePath.string());
		return false;
	}

	if (!sIsFileReadyToLoad(m_ModulePath))
	{
		gLogWarning("Scripts", "Script module {} is still being written", m_ModulePath.string());
		return false;
	}

	m_LoadedWriteTime = fs::last_write_time(m_ModulePath, error_code);

	const Path copies_directory = fs::temp_directory_path() / "Raekor" / "Scripts";

	if (m_LoadCount == 0)
		sRemoveStaleModuleCopies(copies_directory);

	m_LoadedDirectory = copies_directory / std::format("{}_{}", GetCurrentProcessId(), m_LoadCount++);
	fs::create_directories(m_LoadedDirectory, error_code);

	const Path loaded_module = m_LoadedDirectory / m_ModulePath.filename();
	fs::copy_file(m_ModulePath, loaded_module, fs::copy_options::overwrite_existing, error_code);

	if (error_code)
	{
		gLogError("Scripts", "Failed to copy {} to {}: {}", m_ModulePath.string(), loaded_module.string(), error_code.message());
		return false;
	}

	Path pdb_path = m_ModulePath;
	pdb_path.replace_extension(".pdb");

	if (fs::exists(pdb_path, error_code))
		fs::copy_file(pdb_path, m_LoadedDirectory / pdb_path.filename(), fs::copy_options::overwrite_existing, error_code);

	HMODULE module = LoadLibraryW(loaded_module.c_str());

	if (module == nullptr)
	{
		gLogError("Scripts", "LoadLibrary failed for {} with error {}", loaded_module.string(), GetLastError());
		fs::remove_all(m_LoadedDirectory, error_code);
		return false;
	}

	const GetTypesFunction GetTypes = (GetTypesFunction)GetProcAddress(module, RK_SCRIPT_TYPES_FUNCTION_STR);

	if (GetTypes == nullptr)
	{
		gLogError("Scripts", "{} does not export {}, add RK_SCRIPT_MODULE() to one of its source files", m_ModulePath.string(), RK_SCRIPT_TYPES_FUNCTION_STR);
		FreeLibrary(module);
		fs::remove_all(m_LoadedDirectory, error_code);
		return false;
	}

	m_Module = module;

	if (const Array<RTTI*>* types = GetTypes())
		m_Types = *types;

	for (RTTI* rtti : m_Types)
	{
		if (g_RTTIFactory.GetRTTI(rtti->GetHash()))
			gLogWarning("Scripts", "Script type {} is already registered, it will be ignored", rtti->GetTypeName());
		else
			g_RTTIFactory.Register(*rtti);
	}

	gLogInfo("Scripts", "Loaded {} with {} script type(s)", m_ModulePath.filename().string(), m_Types.size());

	return true;
}



void ScriptModule::Unload()
{
	if (m_Module == nullptr)
		return;

	for (RTTI* rtti : m_Types)
		g_RTTIFactory.Unregister(*rtti);

	m_Types.clear();

	if (!FreeLibrary((HMODULE)m_Module))
		gLogError("Scripts", "FreeLibrary failed for {} with error {}", m_ModulePath.string(), GetLastError());

	m_Module = nullptr;

	std::error_code error_code;
	fs::remove_all(m_LoadedDirectory, error_code);
}



bool ScriptModule::HasChangedOnDisk() const
{
	std::error_code error_code;

	if (m_ModulePath.empty() || !fs::exists(m_ModulePath, error_code))
		return false;

	const fs::file_time_type write_time = fs::last_write_time(m_ModulePath, error_code);

	if (error_code || write_time == m_LoadedWriteTime)
		return false;

	return sIsFileReadyToLoad(m_ModulePath);
}

} // raekor
