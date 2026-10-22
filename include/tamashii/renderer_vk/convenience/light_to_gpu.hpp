#pragma once
#include <tamashii/engine/scene/render_scene.hpp>
#include <tamashii/engine/scene/light.hpp>
#include <rvk/rvk.hpp>

T_BEGIN_NAMESPACE
class TextureDataVulkan;
class GeometryDataVulkan;
class LightDataVulkan {
public:
												LightDataVulkan(rvk::LogicalDevice* aDevice);
												~LightDataVulkan();

												// aLightBufferUsageFlags = rvk::Buffer::Use::STORAGE
	void										prepare(uint32_t aLightBufferUsageFlags, uint32_t aLightCount = 128);
	void										destroy();

	void										loadScene(rvk::SingleTimeCommand* aStc, scene_s aScene, TextureDataVulkan* aTextureDataVulkan = nullptr, GeometryDataVulkan* aGeometryDataVulkan = nullptr);
	void										loadScene(rvk::SingleTimeCommand* aStc, const std::deque<RefLight_s*>* aRefLights, TextureDataVulkan* aTextureDataVulkan = nullptr, 
													const std::deque<RefModel_s*>* aRefModels = nullptr, GeometryDataVulkan* aGeometryDataVulkan = nullptr);
	void										unloadScene();
	void										update(rvk::SingleTimeCommand* aStc, scene_s aScene, TextureDataVulkan* aTextureDataVulkan = nullptr, GeometryDataVulkan* aGeometryDataVulkan = nullptr);
	void										update(rvk::SingleTimeCommand* aStc, const std::deque<RefLight_s*>* aRefLights, TextureDataVulkan* aTextureDataVulkan = nullptr,
													const std::deque<RefModel_s*>* aRefModels = nullptr, GeometryDataVulkan* aGeometryDataVulkan = nullptr);

	rvk::Buffer*								getLightBuffer();
	int											getIndex(RefLight_s* aRefLight);
	int											getIndex(RefMesh_s* aRefMesh);
	uint32_t									getLightCount() const;
private:
	rvk::LogicalDevice*							mDevice;
	uint32_t									mMaxLightCount;
	std::vector<Light_s>						mLights;
	std::unordered_map<RefLight_s*, uint32_t>	mRefLightToIndex;		// index of the light in the buffer
	std::unordered_map<RefMesh_s*, uint32_t>	mRefMeshToIndex;		// index of the emissive mesh in the buffer
	rvk::Buffer									mLightBuffer;
};

T_END_NAMESPACE