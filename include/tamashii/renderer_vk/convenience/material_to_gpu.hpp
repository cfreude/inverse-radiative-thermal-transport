#pragma once
#include <tamashii/engine/scene/render_scene.hpp>
#include <tamashii/engine/scene/material.hpp>
#include <rvk/rvk.hpp>

T_BEGIN_NAMESPACE
class TextureDataVulkan;
class MaterialDataVulkan {
public:
												MaterialDataVulkan(rvk::LogicalDevice* aDevice);
												~MaterialDataVulkan();

												// aMaterialBufferUsageFlags = rvk::Buffer::Use::STORAGE
	void										prepare(uint32_t aMaterialBufferUsageFlags, uint32_t aMaterialCount = 128, bool aAutoResize = false);
	void										destroy();

	void										loadScene(rvk::SingleTimeCommand* aStc, scene_s aScene, TextureDataVulkan* aTextureDataVulkan);
	void										unloadScene();
	void										update(rvk::SingleTimeCommand* aStc, scene_s aScene, TextureDataVulkan* aTextureDataVulkan);
	bool										bufferChanged(bool aReset = true);

	rvk::Buffer*								getMaterialBuffer();
	int											getIndex(Material* aMaterial);
private:
	rvk::LogicalDevice*							mDevice;
	uint32_t									mMaxMaterialCount;
	std::vector<Material_s>						mMaterials;
	std::unordered_map<Material*, uint32_t>		mMaterialToIndex;		// index of the material in the buffer

	bool										mAutoResize;
	bool										mBufferResized;
	rvk::Buffer									mMaterialBuffer;
	uint32_t									mMaterialBufferUsageFlags;
};

T_END_NAMESPACE