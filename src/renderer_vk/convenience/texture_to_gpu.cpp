#include <tamashii/renderer_vk/convenience/texture_to_gpu.hpp>
#include <tamashii/renderer_vk/convenience/rvk_type_converter.hpp>
#include <tamashii/engine/importer/importer.hpp>

#include <algorithm>

T_USE_NAMESPACE

TextureDataVulkan::TextureDataVulkan(rvk::LogicalDevice* aDevice) : mDevice(aDevice), mDescMaxSamplers(0), mTexDescriptor(aDevice)
{}

TextureDataVulkan::~TextureDataVulkan()
{ destroy(); }

void TextureDataVulkan::prepare(const uint32_t aDescShaderStages, const uint32_t aMaxSamplers)
{
	mDescMaxSamplers = std::min(aMaxSamplers, mDevice->getPhysicalDevice()->getProperties().limits.maxPerStageDescriptorSamplers);

	mImgToRvkimg.reserve(mDescMaxSamplers);
	mTexToDescIdx.reserve(mDescMaxSamplers);
	mImages.reserve(mDescMaxSamplers);

	mTexDescriptor.reserve(1);
	mTexDescriptor.addImageSampler(0, aDescShaderStages, mDescMaxSamplers);
	mTexDescriptor.setBindingFlags(0, (rvk::Descriptor::BindingFlag::PARTIALLY_BOUND | rvk::Descriptor::BindingFlag::VARIABLE_DESCRIPTOR_COUNT));
	mTexDescriptor.finish(false);
}

void TextureDataVulkan::destroy()
{
	unloadScene();
	mTexDescriptor.destroy();
}

void TextureDataVulkan::loadScene(rvk::SingleTimeCommand* aStc, const tamashii::scene_s aScene)
{
	unloadScene();
	// upload all images to gpu
	// order is the same as all images in imageManager (Image.getIndex())
	for (Image *img : aScene.images) {
		const VkFormat format = converter::imageFormatToRvkFormat(img->getFormat());
		uint32_t mipmapLevel = 1;
		if(img->needsMipMaps()) mipmapLevel = static_cast<uint32_t>(std::floor(std::log2(std::max(img->getWidth(), img->getHeight())))) + 1;

		rvk::Image* rvkImg = new rvk::Image(mDevice);
		ASSERT(!rvkImg->isInit(), "rvk::image exist");
		rvkImg->createImage2D(img->getWidth(), img->getHeight(), format,
			rvk::Image::Use::DOWNLOAD | rvk::Image::Use::UPLOAD | rvk::Image::Use::SAMPLED, mipmapLevel);

		rvkImg->STC_UploadData2D(aStc, img->getWidth(), img->getHeight(), img->getPixelSizeInBytes(), img->getData());
		if(mipmapLevel > 1) rvkImg->STC_GenerateMipmaps(aStc);
		rvkImg->STC_TransitionImage(aStc, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		mImages.push_back(rvkImg);
		mImgToRvkimg.insert(std::pair<Image*, rvk::Image*>(img, rvkImg));

		if (mImages.size() > mDescMaxSamplers) spdlog::error("Image count > descriptor size");
	}
	// add image with the correct sampler to the descriptor set
	uint32_t idx = 0;
	for (Texture *tex : aScene.textures) {
		rvk::Image *img = mImgToRvkimg[tex->image];
		img->setSampler(converter::samplerToRvkSampler(tex->sampler));
		mTexDescriptor.setImage(0, img, idx);

		mTexToDescIdx.insert(std::pair<Texture*, uint32_t>(tex, idx));
		idx++;
	}
	// set the descriptor size to the sampler count
	if (!aScene.textures.empty()) {
		mTexDescriptor.setUpdateSize(0, aScene.textures.size());
		mTexDescriptor.update();
	}
}

void TextureDataVulkan::unloadScene()
{
	mImgToRvkimg.clear();
	mTexToDescIdx.clear();

	for (const rvk::Image *img : mImages) {
		delete img;
	}
	mImages.clear();
}

void TextureDataVulkan::update(rvk::SingleTimeCommand* aStc, const tamashii::scene_s aScene)
{
	unloadScene();
	loadScene(aStc, aScene);
}

rvk::Descriptor* TextureDataVulkan::getDescriptor()
{ return &mTexDescriptor; }

rvk::Image* TextureDataVulkan::getImage(Image *aImg)
{
	if (aImg == nullptr || (mImgToRvkimg.find(aImg) == mImgToRvkimg.end())) return nullptr;
	else return mImgToRvkimg[aImg];
}

int TextureDataVulkan::getIndex(Texture *aTex)
{
	if (aTex == nullptr || (mTexToDescIdx.find(aTex) == mTexToDescIdx.end())) return -1;
	else return static_cast<int>(mTexToDescIdx[aTex]);
}

BlueNoiseTextureData_GPU::BlueNoiseTextureData_GPU(rvk::LogicalDevice* aDevice) : mDevice(aDevice), mImg(aDevice)
{}

void BlueNoiseTextureData_GPU::init(rvk::SingleTimeCommand* aStc, const std::vector<std::string>& aImagePaths, const uint32_t aChannels, const glm::uvec2 aResolution, const bool a16Bits)
{
	mImg.destroy();

	VkFormat format;
	int bytesPerPixelChannel;
	if (a16Bits) {
		format = VK_FORMAT_R16_UNORM;
		if (aChannels == 2) format = VK_FORMAT_R16G16_UNORM;
		else if (aChannels == 3) format = VK_FORMAT_R16G16B16_UNORM;
		else if (aChannels == 4) format = VK_FORMAT_R16G16B16A16_UNORM;
		bytesPerPixelChannel = 2;
	}
	else {
		format = VK_FORMAT_R8_UNORM;
		if (aChannels == 2) format = VK_FORMAT_R8G8_UNORM;
		else if (aChannels == 3) format = VK_FORMAT_R8G8B8_UNORM;
		else if (aChannels == 4) format = VK_FORMAT_R8G8B8A8_UNORM;
		bytesPerPixelChannel = 1;
	}

	// each blue noise image has 4 channels, create image array with (#image * channels) blue noise textures
	mImg.createImage2D(aResolution.x, aResolution.y, format, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 1, static_cast<uint32_t>(aImagePaths.size()));
	const size_t imgSize = static_cast<size_t>(aResolution.x) * static_cast<size_t>(aResolution.y);
	const uint32_t bytesPerPixel = aChannels * bytesPerPixelChannel;
	const size_t totalSize = imgSize * bytesPerPixel;
	// tight data of one channel
	std::vector<uint8_t> buffer(totalSize);
	for (uint32_t i = 0; i < aImagePaths.size(); i++) {
		const std::string& path = aImagePaths[i];
		Image* image = nullptr;
		if (a16Bits) image = tamashii::Importer::load_image_16_bit(path);
		else image = tamashii::Importer::load_image_8_bit(path);
		const uint8_t* imgData = image->getData();
		mImg.STC_UploadData2D(aStc, aResolution.x, aResolution.y, bytesPerPixel, &imgData[0], 0, i);
	}
}

void BlueNoiseTextureData_GPU::destroy()
{
	mImg.destroy();
}

rvk::Image* BlueNoiseTextureData_GPU::getImage()
{ return &mImg; }
