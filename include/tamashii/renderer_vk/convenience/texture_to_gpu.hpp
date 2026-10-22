#pragma once
#include <tamashii/engine/scene/render_scene.hpp>
#include <rvk/rvk.hpp>

T_BEGIN_NAMESPACE
class Image;
class Texture;
class TextureDataVulkan {
public:
														TextureDataVulkan(rvk::LogicalDevice* aDevice);
														~TextureDataVulkan();

	void												prepare(uint32_t aDescShaderStages = rvk::Shader::Stage::ALL, uint32_t aMaxSamplers = 256);
	void												destroy();

	void												loadScene(rvk::SingleTimeCommand* aStc, tamashii::scene_s aScene);
	void												unloadScene();
	void												update(rvk::SingleTimeCommand* aStc, tamashii::scene_s aScene);

														// get the descriptor
	rvk::Descriptor*									getDescriptor();
														// get corresponding img on gpu, nullptr if not found
	rvk::Image*											getImage(Image *aImg);
														// get index of sampler from texture, -1 if not found
	int													getIndex(Texture *aTex);

private:
	rvk::LogicalDevice*									mDevice;
	uint32_t											mDescMaxSamplers;

	std::vector<rvk::Image*>							mImages;
	std::unordered_map<Image*, rvk::Image*>				mImgToRvkimg;		// tamashii::Image to rvk::Image
	std::unordered_map<Texture*, uint32_t>				mTexToDescIdx;	// tamashii::Texture to index in descriptor set (tamashii::Texture = Image + Sampler)
	rvk::Descriptor										mTexDescriptor;
};
class BlueNoiseTextureData_GPU {
public:
														BlueNoiseTextureData_GPU(rvk::LogicalDevice* aDevice);
														~BlueNoiseTextureData_GPU() = default;
														// load a blue noise texture array of aImagePaths.size()
														// aChannels: number of channels each input image of aImagePaths has (e.g. r = 1 ... rgba = 4)
														// a16Bits: images have 16 bits per pixel channel, default is 8 bits
	void												init(rvk::SingleTimeCommand* aStc, const std::vector<std::string>& aImagePaths, uint32_t aChannels, glm::uvec2 aResolution, bool a16Bits = false);
	void												destroy();
	rvk::Image*											getImage();
private:
	rvk::LogicalDevice*									mDevice;
	rvk::Image											mImg;
};
T_END_NAMESPACE