#include <tamashii/engine/importer/importer.hpp>
#include <tamashii/engine/scene/light.hpp>
#include <tiny_ldt.hpp>
T_USE_NAMESPACE

Light* Importer::load_ldt(const std::filesystem::path& aFile) {
    tiny_ldt<float>::light ldt;
    std::string err;
    std::string warn;
    if (!tiny_ldt<float>::load_ldt(aFile.string(),err, warn, ldt)) {
        spdlog::error("{}", err);
    }
    if (!err.empty()) spdlog::error("{}", err);
    if (!warn.empty()) spdlog::warn("{}", warn);

	//if (!tiny_ldt<float>::write_ldt("test.ldt", ldt)) {
	//    spdlog::error("error");
	//}

    return new SpotLight;
}