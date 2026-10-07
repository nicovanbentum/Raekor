#include "PCH.h"
#include "RenderInterface.h"
#include "Components.h"

namespace RK {

void IRenderInterface::UploadMaterialTextures(Entity inEntity, Material& inMaterial, Assets& inAssets)
{
	assert(Material::Default.IsLoaded() && "Default material not loaded, did the programmer forget to initialize its gpu maps before opening a scene?");

	auto UploadTexture = [&](const String& inFile, bool inIsSRGB, uint8_t inSwizzle, uint32_t inDefaultMap, uint32_t& ioGpuMap)
	{
		if (TextureAsset::Ptr asset = inAssets.GetAsset<TextureAsset>(inFile))
			ioGpuMap = UploadTextureFromAsset(asset, inIsSRGB, inSwizzle);
		else
			ioGpuMap = inDefaultMap;
	};

	UploadTexture(inMaterial.albedoFile, true, inMaterial.gpuAlbedoMapSwizzle, Material::Default.gpuAlbedoMap, inMaterial.gpuAlbedoMap);
	UploadTexture(inMaterial.normalFile, false, inMaterial.gpuNormalMapSwizzle, Material::Default.gpuNormalMap, inMaterial.gpuNormalMap);
	UploadTexture(inMaterial.emissiveFile, false, inMaterial.gpuEmissiveMapSwizzle, Material::Default.gpuEmissiveMap, inMaterial.gpuEmissiveMap);
	UploadTexture(inMaterial.metallicFile, false, inMaterial.gpuMetallicMapSwizzle, Material::Default.gpuMetallicMap, inMaterial.gpuMetallicMap);
	UploadTexture(inMaterial.roughnessFile, false, inMaterial.gpuRoughnessMapSwizzle, Material::Default.gpuRoughnessMap, inMaterial.gpuRoughnessMap);
}

} // namespace RK
