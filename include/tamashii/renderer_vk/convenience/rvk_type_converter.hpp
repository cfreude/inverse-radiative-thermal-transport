#pragma once
#include <tamashii/engine/scene/image.hpp>
#include <rvk/rvk.hpp>

T_BEGIN_NAMESPACE
namespace converter {
	rvk::SamplerConfig samplerToRvkSampler(const Sampler& aSampler);
	VkFormat imageFormatToRvkFormat(Image::Format aFormat);

	// create a descriptor containing img, the descriptor can then be used for ImGui::Image((ImTextureID)desc...
	void createImguiDescriptor(rvk::Descriptor* aDesc, rvk::Image* aImg);
}
T_END_NAMESPACE
