#pragma once
#include <tamashii/public.hpp>

T_BEGIN_NAMESPACE
namespace math {
	bool decomposeTransform(const glm::mat4& aTransform, glm::vec3& aTranslation, glm::quat& aRotation, glm::vec3& aScale);
}
namespace color
{
	// http://www.brucelindbloom.com/index.html?Eqn_RGB_to_XYZ.html
	glm::vec3 srgb_to_XYZ(const glm::vec3& asrgb);
	// http://www.brucelindbloom.com/index.html?Eqn_XYZ_to_RGB.html
	glm::vec3 XYZ_to_xyY(const glm::vec3& aXYZ);
	// http://www.brucelindbloom.com/index.html?Eqn_XYZ_to_RGB.html
	glm::vec3 XYZ_to_srgb(const glm::vec3& aXYZ);
	// http://www.brucelindbloom.com/index.html?Eqn_XYZ_to_RGB.html
	glm::vec3 xyY_to_XYZ(const glm::vec3& axyY);

	// https://axonflux.com/handy-rgb-to-hsl-and-rgb-to-hsv-color-model-c
	glm::vec3 rgb_to_hsv(const glm::vec3& argb);
	glm::vec3 hsv_to_rgb(const glm::vec3& ahsv);
}
T_END_NAMESPACE
