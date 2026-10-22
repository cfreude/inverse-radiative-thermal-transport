#pragma once
#include <tamashii/public.hpp>
#include <tamashii/engine/scene/scene_graph.hpp>

#include <filesystem>

T_BEGIN_NAMESPACE
class Exporter {
public:
	struct SceneExportSettings
	{
		// first 4 bits are format ()
		enum class Format : uint32_t
		{
			glTF = 0x1
		};
		Format mFormat;
		bool mEmbedImages;
		bool mEmbedBuffers;
		bool mWriteBinary;
		bool mExcludeLights;
		bool mExcludeModels;
		bool mExcludeCameras;
		SceneExportSettings(): mFormat(Format::glTF), mEmbedImages(false), mEmbedBuffers(false),
			mWriteBinary(false), mExcludeLights(false), mExcludeModels(false), mExcludeCameras(false) {}

		SceneExportSettings(uint32_t aEncoded) : mFormat(static_cast<Format>(aEncoded & 0xf)), mEmbedImages(aEncoded & 0x10),
		                                         mEmbedBuffers(aEncoded & 0x20), mWriteBinary(aEncoded & 0x40),
		                                         mExcludeLights(aEncoded & 0x80), mExcludeModels(aEncoded & 0x100), mExcludeCameras(aEncoded & 0x200) {}
		uint32_t encode() const
		{
			return static_cast<uint32_t>(mFormat)
			| static_cast<uint32_t>(mEmbedImages << 4)
			| static_cast<uint32_t>(mEmbedBuffers << 5)
			| static_cast<uint32_t>(mWriteBinary << 6)
			| static_cast<uint32_t>(mExcludeLights << 7)
			| static_cast<uint32_t>(mExcludeModels << 8)
			| static_cast<uint32_t>(mExcludeCameras << 9);
		}
	};
	static bool			save_scene(const std::string& aOutputFile, const SceneExportSettings aSettings, const SceneInfo_s& aSceneInfo)
						{
							std::string ext = std::filesystem::path(aOutputFile).extension().string();
							std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
							if (ext == ".gltf" || ext == ".glb") { // glTF
								return save_scene_gltf(aOutputFile, aSettings, aSceneInfo);
							}
							spdlog::warn("...format not supported");
							return false;
						}
	/* IMAGE */
	static void			save_image_png_8_bit(std::string const& aName, int aWidth, int aHeight, int aChannels, const uint8_t* aPixels);
						// stride: floats per pixel in aPixels
						// aOut: offsets for floats in aPixels - must be (A)BGR / (4)210 order, since most of EXR viewers expect this channel order.
	static void			save_image_exr(std::string const& aName, int aWidth, int aHeight, const float* aPixels, uint32_t aStride = 3, const std::vector<uint8_t>& aOut = { 2, 1, 0 } /*B G R*/);
	/* FILE */
	static bool			write_file(std::string const& aFilename, std::string const& aContent);
	static bool			write_file_append(std::string const& aFilename, std::string const& aContent);
private:
	static bool			save_scene_gltf(const std::string& aOutputFile, const SceneExportSettings aSettings, const SceneInfo_s& aSceneInfo);
};
T_END_NAMESPACE
