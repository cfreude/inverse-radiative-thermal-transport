#pragma once
#include <tamashii/public.hpp>
#include <tamashii/engine/scene/asset.hpp>

T_BEGIN_NAMESPACE
struct Texture;
enum class LightType : uint32_t {
	// Bits
	POINT							= 0x00000001,
	SPOT							= 0x00000002,
	DIRECTIONAL						= 0x00000004,
	SQUARE							= 0x00000008,
	RECTANGLE						= 0x00000010,
	CUBE							= 0x00000020,
	CUBOID							= 0x00000040,
	DISK							= 0x00000080,
	ELLIPSE							= 0x00000100,
	SPHERE							= 0x00000200,
	ELLIPSOID						= 0x00000400,
	IES								= 0x00000800,
	TRIANGLE_MESH					= 0x00001000,
	// Mask
	PUNCTUAL						= (POINT | SPOT | DIRECTIONAL),
	HYPERRECTANGULAR				= (SQUARE | RECTANGLE | CUBE | CUBOID),
	ELLIPTICAL						= (DISK | ELLIPSE | SPHERE | ELLIPSOID),
	SURFACE							= (HYPERRECTANGULAR | ELLIPTICAL)
};
// gpu ready struct (aligned to 16 byte)
// !!! change this struct only together with the one in shader/convenience/light_data.glsl/hlsl !!!
struct Light_s {
	// ALL
	glm::vec3						color;
	float							intensity;
	// LIGHT_TYPE_POINT
	// LIGHT_TYPE_SPOT
	// LIGHT_TYPE_SURFACE
	// LIGHT_TYPE_IES
	glm::vec4						pos_ws;
	// LIGHT_TYPE_SPOT
	// LIGHT_TYPE_DIRECTIONAL
	// LIGHT_TYPE_SURFACE
	// LIGHT_TYPE_IES
	glm::vec4						n_ws_norm;
	glm::vec4						t_ws_norm;
	// LIGHT_TYPE_SURFACE
	glm::vec3						dimensions;
	float							____;
	// LIGHT_TYPE_POINT
	// LIGHT_TYPE_SPOT
	float							range;
	// LIGHT_TYPE_PUNCTUAL
	float							light_offset;		// radius for point/spot and angle for directional (for soft shadows)
	// LIGHT_TYPE_SPOT
	float							inner_angle;
	float							outer_angle;
	float							light_angle_scale;
	float							light_angle_offset;
	// LIGHT_TYPE_IES
	float							min_vertical_angle;
	float							max_vertical_angle;
	float							min_horizontal_angle;
	float							max_horizontal_angle;
	// LIGHT_TYPE_IES
	// LIGHT_TYPE_TRIANGLE_MESH
	int								texture_index; // -1 if no texture
	// LIGHT_TYPE_TRIANGLE_MESH
	uint32_t						triangle_count;
	int								id;
	uint32_t						index_buffer_offset;	// -1 if no indices used
	uint32_t						vertex_buffer_offset;
	// LIGHT_TYPE_*
	uint32_t						type;
};

class Light : public Asset {
public:
	enum class Type
	{
		DIRECTIONAL, POINT, SPOT, SURFACE, IES
	};

	Type										getType() const;
	glm::vec3									getColor() const;
	float										getIntensity() const;
	glm::vec4									getDefaultDirection() const;
	glm::vec4									getDefaultTangent() const;

	void										setColor(glm::vec3 aColor);
	void										setIntensity(float aIntensity);
	void										setDefaultDirection(glm::vec3 aDir);
	void										setDefaultTangent(glm::vec3 aDir);

	virtual Light_s								getRawData() const = 0;
protected:
												Light(Type aLightType, glm::vec3 aDirection, glm::vec3 aTangent);
												~Light() = default;

	Type										mLightType;

	glm::vec3									mColor;
												// point/spot :luminous intensity in candela(lm/sr)
												// directional: illuminance in lux(lm/m^2)
	float										mIntensity;
												// default direction (used with model matrix to get ws direction of light)
	glm::vec3									mDirection;
	glm::vec3									mTangent;
};

class PointLight final : public Light {
public:
												PointLight();
												~PointLight() = default;

	Light_s										getRawData() const override;
	float										getRange() const;
	float										getRadius() const;
	void										setRange(float aRange);
	void										setRadius(float aRadius);
private:
	float										mRange;						// 0.0 = inifinite
	float										mRadius;
};

class SpotLight final : public Light {
public:
												SpotLight(float aInnerConeAngle = 0.0f, float aOuterConeAngle = 0.7853981634f/*PI/4*/);
												~SpotLight() = default;

	Light_s										getRawData() const override;
	float										getRange() const;
	float										getRadius() const;
	float										getInnerConeAngle() const;
	float										getOuterConeAngle() const;
	void										setRange(float aRange);
	void										setRadius(float aRadius);
	void										setCone(float aInnerAngle, float aOuterAngle);
private:
	float										mRange;	// 0.0 == inifinite
	float										mRadius;
	struct Cone {								
		float									inner_angle;				
		float									outer_angle; // outer_angle >= innerAngle && outer_angle <= PI/2
	} mCone;
};

class DirectionalLight final : public Light {
public:
												DirectionalLight();
												~DirectionalLight() = default;

	Light_s										getRawData() const override;
	float										getAngle() const;
	void										setAngle(float aAngle);
private:
	float										mAngle; // angular diameter
};

class SurfaceLight final : public Light {
public:
	enum class Shape
	{
		SQUARE, RECTANGLE, CUBE, CUBOID, DISK, ELLIPSE, SPHERE, ELLIPSOID
	};

												SurfaceLight() : Light(Type::SURFACE, { 0,0,-1 }, { 1,0,0 }), mShape(Shape::SQUARE), mDimension(1) {}
												~SurfaceLight() = default;

	Light_s										getRawData() const override;
	glm::vec4									getCenter() const;

	Shape										getShape() const;
	void										setShape(Shape aShape);
	glm::vec3									getDimensions() const;
	void										setDimensions(glm::vec3 aDimension);
	bool										is3D() const;
private:
	Shape										mShape;

	// width, height, depth for HYPERRECTANGULAR, diameter for ELLIPTICAL
	// x in tangent direction, y in bitangent direction, z in normal direction
	glm::vec3									mDimension;
};

class IESLight final : public Light {
public:
												IESLight();
	Light_s										getRawData() const override;

	float										getMinVerticalAngle() const;
	float										getMaxVerticalAngle() const;
	float										getMinHorizontalAngle() const;
	float										getMaxHorizontalAngle() const;
	void										setVerticalAngles(const std::vector<float>& aVerticalAngles);
	void										setHorizontalAngles(const std::vector<float>& aHorizontalAngles);

	Texture*									getCandelaTexture() const;
	void										setCandelaTexture(Texture* aCandelaTexture);
private:
	std::vector<float>							mVerticalAngles;
	std::vector<float>							mHorizontalAngles;

	Texture*									mCandelaTexture;
};
T_END_NAMESPACE
