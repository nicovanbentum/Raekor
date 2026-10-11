#include "PCH.h"
#include "Components.h"
#include "OBJ.h"
#include "FBX.h"
#include "GLTF.h"
#include "Iter.h"
#include "Scene.h"
#include "Timer.h"
#include "Threading.h"
#include "Serialization.h"
#include "RenderInterface.h"

namespace RK {

RTTI_DEFINE_TYPE(MeshAsset) { RTTI_DEFINE_TYPE_INHERITANCE(MeshAsset, Asset); }

template SharedPtr<MeshAsset> Assets::GetAsset<MeshAsset>(const String& inPath);


struct MeshPackHeader
{
	uint32_t mMagicNumber = MeshAsset::cMagicNumber;
	uint32_t mVersion = MeshAsset::cVersion;
	uint32_t mMeshCount = 0;
	uint32_t mPadding = 0;
};


struct MeshPackEntry
{
	uint64_t mStart = 0;
	uint64_t mSize = 0;
};


MeshAsset::~MeshAsset()
{
	if (m_Renderer)
		m_Renderer->DestroyMeshBuffers(*this);
}


bool MeshAsset::Load()
{
	File file = File(m_Path, std::ios::binary | std::ios::in);

	if (!file.is_open())
		return false;

	MeshPackHeader header;
	ReadFileBinary(file, header);

	if (header.mMagicNumber != cMagicNumber || header.mVersion != cVersion)
	{
		gLogError("Assets", "{} is not a mesh pack of version {}", m_Path.string(), cVersion);
		return false;
	}

	ReadFileBinary(file, m_Source);

	Array<MeshPackEntry> entries(header.mMeshCount);
	ReadFileSlice(file, Slice(entries));

	if (!file)
		return false;

	m_Meshes.resize(header.mMeshCount);

	Atomic<bool> success = true;

	g_JobSystem.ParallelFor(header.mMeshCount, 1, [&](uint32_t inIndex)
	{
		File mesh_file = File(m_Path, std::ios::binary | std::ios::in);
		mesh_file.seekg(entries[inIndex].mStart);

		MeshGeometry& geometry = m_Meshes[inIndex];
		ReadFileBinary(mesh_file, geometry.name);
		ReadFileBinary(mesh_file, geometry.bbox);
		ReadFileBinary(mesh_file, geometry.positions);
		ReadFileBinary(mesh_file, geometry.uvs);
		ReadFileBinary(mesh_file, geometry.normals);
		ReadFileBinary(mesh_file, geometry.tangents);
		ReadFileBinary(mesh_file, geometry.indices);

		if (!mesh_file || uint64_t(mesh_file.tellg()) != entries[inIndex].mStart + entries[inIndex].mSize)
		{
			success = false;
			return;
		}

		geometry.CalculateVertices();
	});

	if (!success)
	{
		gLogError("Assets", "Failed to read the meshes in {}", m_Path.string());
		m_Meshes.clear();
		return false;
	}

	return true;
}


bool MeshAsset::Save(const String& inPath) const
{
	std::error_code error_code;
	fs::create_directories(Path(inPath).parent_path(), error_code);

	File file = File(inPath, std::ios::binary | std::ios::out | std::ios::trunc);

	if (!file.is_open())
	{
		gLogError("Assets", "Failed to open {} for writing", inPath);
		return false;
	}

	MeshPackHeader header;
	header.mMeshCount = uint32_t(m_Meshes.size());

	WriteFileBinary(file, header);
	WriteFileBinary(file, m_Source);

	const uint64_t entries_start = uint64_t(file.tellp());

	Array<MeshPackEntry> entries(m_Meshes.size());
	WriteFileSlice(file, Slice<const MeshPackEntry>(entries));

	for (const auto& [index, geometry] : gEnumerate(m_Meshes))
	{
		entries[index].mStart = uint64_t(file.tellp());

		WriteFileBinary(file, geometry.name);
		WriteFileBinary(file, geometry.bbox);
		WriteFileBinary(file, geometry.positions);
		WriteFileBinary(file, geometry.uvs);
		WriteFileBinary(file, geometry.normals);
		WriteFileBinary(file, geometry.tangents);
		WriteFileBinary(file, geometry.indices);

		entries[index].mSize = uint64_t(file.tellp()) - entries[index].mStart;
	}

	file.seekp(entries_start);
	WriteFileSlice(file, Slice<const MeshPackEntry>(entries));

	if (!file)
	{
		gLogError("Assets", "Failed to write {}", inPath);
		return false;
	}

	return true;
}


String MeshAsset::GetSourcePath(const Path& inFile)
{
	std::error_code error_code;
	const Path relative_path = fs::relative(inFile, error_code);

	return ( error_code || relative_path.empty() ? inFile : relative_path ).generic_string();
}


MeshAsset::Ptr MeshAsset::Create(const String& inSource)
{
	MeshAsset::Ptr asset = std::make_shared<MeshAsset>();
	asset->m_Source = inSource;

	if (IsBuiltin(inSource))
		asset->m_Path = inSource;
	else if (!inSource.empty())
		asset->m_Path = GetCachedPath(inSource);

	return asset;
}


MeshAsset::Ptr MeshAsset::CreateBuiltin(const String& inSource)
{
	MeshAsset::Ptr asset = Create(inSource);
	MeshGeometry& geometry = asset->m_Meshes.emplace_back();

	if (inSource == cBuiltinCube)
		MeshGeometry::CreateCube(geometry, 1.0f);
	else if (inSource == cBuiltinPlane)
		MeshGeometry::CreatePlane(geometry, 1.0f);
	else if (inSource == cBuiltinSphere)
		MeshGeometry::CreateSphere(geometry, 0.5f, 32, 32);
	else
		return nullptr;

	geometry.name = Path(inSource).filename().string();
	return asset;
}


MeshAsset::Ptr MeshAsset::Import(const String& inSource)
{
	Timer timer;

	Scene scene(nullptr);
	const Path extension = Path(inSource).extension();

	SharedPtr<MeshAsset> asset;

	if (extension == ".gltf" || extension == ".glb")
	{
		GltfImporter importer(scene, nullptr);

		if (importer.LoadFromFile(inSource, nullptr))
			asset = importer.GetMeshAsset();
	}
	else if (extension == ".fbx")
	{
		FBXImporter importer(scene, nullptr);

		if (importer.LoadFromFile(inSource, nullptr))
			asset = importer.GetMeshAsset();
	}
	else if (extension == ".obj")
	{
		OBJImporter importer(scene, nullptr);

		if (importer.LoadFromFile(inSource, nullptr))
			asset = importer.GetMeshAsset();
	}
	else
	{
		gLogError("Assets", "Can't import meshes from {}, the file type is not supported", inSource);
		return nullptr;
	}

	if (asset)
		gLogInfo("Assets", "Imported {} meshes from {} in {:.2f} ms", asset->GetMeshCount(), inSource, Timer::sToMilliseconds(timer.GetElapsedTime()));

	return asset;
}


MeshAsset::Ptr MeshAsset::Find(Assets& ioAssets, const String& inSource)
{
	if (inSource.empty())
		return nullptr;

	if (IsBuiltin(inSource))
	{
		if (MeshAsset::Ptr asset = ioAssets.FindAsset<MeshAsset>(inSource))
			return asset;

		MeshAsset::Ptr asset = CreateBuiltin(inSource);
		return asset ? ioAssets.AddAsset(inSource, asset) : nullptr;
	}

	const String cached_path = GetCachedPath(inSource);

	std::error_code error_code;
	const bool has_source = fs::exists(inSource, error_code);
	const bool has_cache = fs::exists(cached_path, error_code);
	const bool is_stale = !has_cache || ( has_source && fs::last_write_time(cached_path, error_code) < fs::last_write_time(inSource, error_code) );

	if (!is_stale)
	{
		if (MeshAsset::Ptr asset = ioAssets.GetAsset<MeshAsset>(cached_path))
			return asset;
	}

	if (!has_source)
	{
		gLogError("Assets", "Can't load meshes from {}, the source file and its cached meshes are missing", inSource);
		return nullptr;
	}

	MeshAsset::Ptr asset = Import(inSource);

	if (asset == nullptr)
		return nullptr;

	ioAssets.ReleaseAsset(cached_path);
	return ioAssets.AddAsset(cached_path, asset);
}

} // RK
