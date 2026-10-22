#include <tamashii/engine/importer/importer.hpp>
#include <tamashii/engine/scene/light.hpp>
#include <tamashii/engine/scene/image.hpp>
#include <tiny_ies.hpp>
T_USE_NAMESPACE

namespace {
    float findMinSpacing(const std::vector<float>& aAngles)
    {
        float minSpacing = 0.0f;
        float prev = 0.0f;
	    for(const float angle : aAngles)
	    {
		    const float currentSpacing = angle - prev;
            if (currentSpacing < minSpacing || prev == 0.0f) minSpacing = currentSpacing;
            prev = angle;
	    }
        return minSpacing;
    }
}

// https://docs.agi32.com/AGi32/Content/references/IDH_iesna_standard_file_format.htm
IESLight* Importer::load_ies(const std::filesystem::path& aFile) {
    tiny_ies<float>::light ies;
    std::string err;
    std::string warn;
    if (!tiny_ies<float>::load_ies(aFile.string(), err, warn, ies)) {
        spdlog::error("{}", err);
    }
    if (!err.empty()) spdlog::error("{}", err);
    if (!warn.empty()) spdlog::warn("{}", warn);

    //if (!tiny_ies<float>::write_ies("test.ies", ies)) {
    //    spdlog::error("error");
    //}

    if (ies.photometric_type != 1) spdlog::warn("Only IES Type C Lights are supported.");

    const auto light = new IESLight();
    light->setFilepath(aFile.string());

    for (float& v : ies.candela)
    {
        v = v / ies.max_candela;
    }

    /*const float vSpacing = findMinSpacing(ies.vertical_angles);
    const float hSpacing = findMinSpacing(ies.horizontal_angles);
    const float width = ies.max_vertical_angle / vSpacing;
    const float height = ies.max_horizontal_angle / hSpacing;*/

    Image* img = Image::alloc(aFile.filename().string());
    img->init(ies.number_vertical_angles, ies.number_horizontal_angles, Image::Format::R32_FLOAT, ies.candela.data());

    constexpr Sampler sampler = { 
        Sampler::Filter::LINEAR, Sampler::Filter::LINEAR, Sampler::Filter::LINEAR,
        Sampler::Wrap::CLAMP_TO_BORDER, Sampler::Wrap::MIRRORED_REPEAT,
    	Sampler::Wrap::CLAMP_TO_EDGE, 0, 0
    };
    Texture* texture = Texture::alloc();
    texture->image = img;
    texture->sampler = sampler;
    light->setCandelaTexture(texture);
    light->setVerticalAngles(ies.vertical_angles);
    light->setHorizontalAngles(ies.horizontal_angles);
    return light;
}

