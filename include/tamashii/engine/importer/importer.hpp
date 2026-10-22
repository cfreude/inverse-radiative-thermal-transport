#pragma once
#include <tamashii/public.hpp>

#include <filesystem>
#include <deque>

T_BEGIN_NAMESPACE
struct SceneInfo_s;
class Model;
class Mesh;
class Image;
class Light;
class IESLight;
class Importer {
public:
										static Importer& instance()
										{
											static Importer instance;
											return instance;
										}
										Importer(Importer const&) = delete;
	void								operator=(Importer const&) = delete;

										/* SCENE */
										// check file extension and call suited loader implementation
	[[nodiscard]] SceneInfo_s*			load_scene(const std::string& aFile) const
	{
											spdlog::info("Load Scene: {}", aFile);
											std::string ext = std::filesystem::path(aFile).extension().string();
											std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

											for(const auto& t : mSceneLoadFormats) {
												for (const auto& s : std::get<1>(t)) {
													if (ext == std::filesystem::path(s).extension().string()) return std::get<2>(t)(aFile);
												}
											}

											spdlog::warn("...format not supported");
											return nullptr;
										}
	void add_load_scene_format(const std::string& aName, const std::vector<std::string>& aExt, const std::function<SceneInfo_s* (const std::string&)>& aCallback)
										{
											mSceneLoadFormats.emplace_back(aName, aExt, aCallback);
										}

	[[nodiscard]] std::vector<std::pair<std::string, std::vector<std::string>>> load_scene_file_dialog_info() const
										{
											std::vector<std::pair<std::string, std::vector<std::string>>> fdi;
											fdi.reserve(mSceneLoadFormats.size() + 1);
											uint32_t ts = 0;
											for (const auto& t : mSceneLoadFormats) ts += std::get<1>(t).size();
											fdi.emplace_back("Scene", std::vector<std::string>());
											fdi.front().second.reserve(ts);
											for (const auto& t : mSceneLoadFormats) {
												for (const auto& s : std::get<1>(t)) fdi.front().second.emplace_back(s);
												fdi.emplace_back(std::get<0>(t), std::get<1>(t));
											}
											return fdi;
										}
								
										/* MODEL */
    [[nodiscard]] static Model*			load_model(const std::string& aFile)
										{
											spdlog::info("Load Model: {}", aFile);
											std::string ext = std::filesystem::path(aFile).extension().string();
											std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
											if (ext == ".ply") {
												return load_ply(aFile);
											} else if (ext == ".obj") {
												return load_obj(aFile);
											}
											spdlog::warn("...format not supported");
											return nullptr;
										}
    [[nodiscard]] static  Mesh*			load_mesh(const std::string& aFile)
										{
											spdlog::info("Load Mesh: {}", aFile);
											std::string ext = std::filesystem::path(aFile).extension().string();
											std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
											if (ext == ".ply") {
												return load_ply_mesh(aFile);
											}
											spdlog::warn("...format not supported");
											return nullptr;
										}

										/* LIGHT */
    [[nodiscard]] static Light*			load_light(const std::filesystem::path& aFile)
										{
											spdlog::info("Load Light: {}", aFile.string());
											std::string ext = aFile.extension().string();
											std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
											if (ext == ".ies") {
												return (Light*)load_ies(aFile);
											}
											else if (ext == ".ldt") {
												return load_ldt(aFile);
											}
											spdlog::warn("...format not supported");
											return nullptr;
										}

										/* IMAGE */
    [[nodiscard]] static Image*			load_image_8_bit(std::string const& aFile, int aForceNumberOfChannels = 0);
    [[nodiscard]] static Image*			load_image_16_bit(std::string const& aFile, int aForceNumberOfChannels = 0);

										/* FILE */
										// load a text file or binary file
										// to get byte data use .c_str();
	static std::string					load_file(std::string const& aFilename);


private:
										/* Loader Backend */
										/* SCENE */
										// tinygltf URL:https://github.com/syoyo/tinygltf
    [[nodiscard]] static SceneInfo_s*	load_gltf(std::string const& aFile);
										// custom
	[[nodiscard]] static SceneInfo_s*	load_pbrt(std::string const& aFile);
	[[nodiscard]] static SceneInfo_s*	load_bsp(std::string const& aFile);

										/* MODEL */
										// happly URL:https://github.com/nmwsharp/happly
    [[nodiscard]] static Model*			load_ply(const std::string& aFile);
    [[nodiscard]] static Mesh*			load_ply_mesh(const std::string& aFile);
										// tinyobjloader URL:https://github.com/tinyobjloader/tinyobjloader
    [[nodiscard]] static Model*			load_obj(const std::string& aFile);
	[[nodiscard]] static Mesh*			load_obj_mesh(const std::string& aFile);

										/* LIGHT */
										// tinyies URL:https://github.com/fknfilewalker/tinyies.git
    [[nodiscard]] static IESLight*		load_ies(const std::filesystem::path& aFile);
										// tinyldt URL:https://github.com/fknfilewalker/tinyldt.git
    [[nodiscard]] static Light*			load_ldt(const std::filesystem::path& aFile);

										Importer() : mSceneLoadFormats{
										{ "glTF (.gltf/.glb)" , { "*.gltf", "*.glb" }, &load_gltf },
										{ "pbrt-v3 (.pbrt)" , { "*.pbrt" },&load_pbrt },
										{ "Quake III (.bsp)" , { "*.bsp" },&load_bsp }
										} {}

	std::deque<std::tuple<std::string, std::vector<std::string>, std::function<SceneInfo_s* (const std::string&)>>> mSceneLoadFormats;
};
T_END_NAMESPACE
