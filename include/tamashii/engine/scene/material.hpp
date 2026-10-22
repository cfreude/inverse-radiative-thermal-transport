#pragma once
#include <tamashii/engine/scene/asset.hpp>

T_BEGIN_NAMESPACE
struct Texture;
// gpu ready struct (aligned to 16 byte)
// !!! change this struct only together with the one in shader/convenience/material_data.glsl/hlsl !!!
//#if defined(_MSC_VER)
//__declspec(align(16)) struct Material_s {
//#else
//struct Material_s {
//#endif
struct Material_s {
	// texture indices depend on gpu implementation
	int			baseColorTexIdx;
	int			metallicTexIdx;
	int			roughnessTexIdx;
	int			occlusionTexIdx;
	/* -- 16 byte -- */
	int			normalTexIdx;
	int			emissionTexIdx;
	int			specularTexIdx;
	int			specularColorTexIdx;
	/* -- 16 byte -- */
	int			transmissionTexIdx;
	int			clearcoatTexIdx;
	int			clearcoatRoughnessTexIdx;
	int			clearcoatNormalTexIdx;
	/* -- 16 byte -- */
	int			sheenColorTexIdx;
	int			sheenRoughnessTexIdx;
	int			lightTexIdx;
	int			customTexIdx;
	/* -- 16 byte -- */
	// tex coord idx
	int			baseColorTexCoordIdx;
	int			metallicTexCoordIdx;
	int			roughnessTexCoordIdx;
	int			occlusionTexCoordIdx;
	/* -- 16 byte -- */
	int			normalTexCoordIdx;
	int			emissionTexCoordIdx;
	int			specularTexCoordIdx;
	int			specularColorTexCoordIdx;
	/* -- 16 byte -- */
	int			transmissionTexCoordIdx;
	int			customTexCoordIdx;
	int			clearcoatTexCoordIdx;
	int			clearcoatRoughnessTexCoordIdx;
	/* -- 16 byte -- */
	int			clearcoatNormalTexCoordIdx;
	int			sheenColorTexCoordIdx;
	int			sheenRoughnessTexCoordIdx;
	int			lightTexCoordIdx;
	// factors
	/* -- 16 byte -- */
	glm::vec4	baseColorFactor;
	/* -- 16 byte -- */
	float		metallicFactor;
	float		roughnessFactor;
	float		occlusionStrength;
	float		normalScale;
	/* -- 16 byte -- */
	glm::vec3	emissionFactor;
	float		emissionStrength;
	/* -- 16 byte -- */
	glm::vec3	specularColorFactor;
	float		specularFactor;
	/* -- 16 byte -- */
	float		ior;
	float		transmissionFactor;
	float		clearcoatFactor;
	float		clearcoatRoughnessFactor;
	/* -- 16 byte -- */
	glm::vec3	sheenColorFactor;
	float		sheenRoughnessFactor;
	/* -- 16 byte -- */
	glm::vec3	lightFactor;
	int			_;		/*not set*/
	/* -- 16 byte -- */
	float		alphaDiscardValue;
	uint32_t	alphaDiscard;
	uint32_t	isEmissive;
	uint32_t	isDoubleSided;
};
//#if defined(__clang__) || defined(__GNUC__) || defined(__GNUG__)
//}__attribute__((aligned(16)));
//#else
//};
//#endif

// class
class Material : public Asset {
public:
	enum class BlendMode {
		_OPAQUE,								// fully opaque // for some reason it is not possible to use OPAQUE
		MASK,									// use the alpha discard value to discard writes
		BLEND									// blend
	};
	enum class Type {
		DIFFUSE,
		MIRROR
	};

												Material(const std::string& aName = "");
												~Material();
												Material(const Material& aMaterial);

	static Material*							alloc(const std::string& aName = "");

	Material_s									getRawData() const;
	void										setCullBackface(bool aCull);
	void										setBlendMode(BlendMode aBlendMode);
	void										setAlphaDiscardValue(float aAlphaDiscardValue);
	bool										getCullBackface() const;
	BlendMode									getBlendMode() const;
	float										getAlphaDiscardValue() const;

	bool										hasBaseColorTexture() const;
	bool										hasMetallicTexture() const;
	bool										hasRoughnessTexture() const;
	bool										hasEmissionTexture() const;
	bool										hasNormalTexture() const;
	bool										hasOcclusionTexture() const;
	bool										hasSpecularTexture() const;
	bool										hasSpecularColorTexture() const;

	// base color
	glm::vec4									getBaseColorFactor() const;
	Texture*									getBaseColorTexture() const;
	void										setBaseColorFactor(const glm::vec4& aVec);
	void										setBaseColorTexture(Texture* aTexture);
	// metallic
	float										getMetallicFactor() const;
	Texture*									getMetallicTexture() const;
	void										setMetallicFactor(const float& aFloat);
	void										setMetallicTexture(Texture* aTexture);
	// roughness
	float										getRoughnessFactor() const;
	Texture*									getRoughnessTexture() const;
	void										setRoughnessFactor(const float& aFloat);
	void										setRoughnessTexture(Texture* aTexture);
	// emission
	bool										isLight() const;
	float										getEmissionStrength() const;
	glm::vec3									getEmissionFactor() const;
	Texture*									getEmissionTexture() const;
	void										setEmissionStrength(const float& aFloat);
	void										setEmissionFactor(const glm::vec3& aVec);
	void										setEmissionTexture(Texture* aTexture);
	// normal
	float										getNormalScale() const;
	Texture*									getNormalTexture() const;
	void										setNormalScale(const float& aFloat);
	void										setNormalTexture(Texture* aTexture);
	// occlusion
	float										getOcclusionStrength() const;
	Texture*									getOcclusionTexture() const;
	void										setOcclusionStrength(const float& aFloat);
	void										setOcclusionTexture(Texture* aTexture);
	// specular
	float										getSpecularFactor() const;
	glm::vec3									getSpecularColorFactor() const;
	Texture*									getSpecularTexture() const;
	Texture*									getSpecularColorTexture() const;
	void										setSpecularFactor(const float& aFloat);
	void										setSpecularColorFactor(const glm::vec3& aVec);
	void										setSpecularTexture(Texture* aTexture);
	void										setSpecularColorTexture(Texture* aTexture);
	// transmission
	float										getTransmissionFactor() const;
	Texture*									getTransmissionTexture() const;
	void										setTransmissionFactor(const float& aFloat);
	void										setTransmissionTexture(Texture* aTexture);
	// volume
	float										getThicknessFactor() const;
	Texture*									getThicknessTexture() const;
	float										getAttenuationDistance() const;
	glm::vec3									getAttenuationColor() const;
	void										setThicknessFactor(const float& aFloat);
	void										setThicknessTexture(Texture* aTexture);
	void										setAttenuationDistance(const float& aFloat);
	void										setAttenuationColor(const glm::vec3& aVec);
	// index of refraction
	float										getIOR() const;
	void										setIOR(const float& aFloat);

	glm::vec3									getLightFactor() const;
	Texture*									getLightTexture() const;
	void										setLightFactor(const glm::vec3& aVec);
	void										setLightTexture(Texture* aTexture);
	Texture*									getCustomTexture() const;
	void										setCustomTexture(Texture* aTexture);


private:
	BlendMode									mBlendMode;
	float										mAlphaDiscardValue;
	bool										mCullBackface;

	// factors
	glm::vec4									mBaseColorFactor;
	float										mMetallicFactor;
	float										mRoughnessFactor;
	glm::vec3									mEmissionFactor;
	float										mEmissionStrength;
	float										mNormalScale;
	float										mOcclusionStrength;

	float										mSpecularFactor;
	glm::vec3									mSpecularColorFactor; 
	float										mTransmissionFactor;
	float										mIor;
	float										mThicknessFactor;
	float										mAttenuationDistance;
	glm::vec3									mAttenuationColor;

	glm::vec3									mLightFactor;

	// textures
	Texture*									mBaseColorTexture;
	Texture*									mMetallicTexture;
	Texture*									mRoughnessTexture;
	Texture*									mEmissionTexture;
	Texture*									mNormalTexture;
	Texture*									mOcclusionTexture;
	Texture*									mSpecularTexture;
	Texture*									mSpecularColorTexture;
	Texture*									mTransmissionTexture;
	Texture*									mThicknessTexture;

	Texture*									mLightTexture;
	Texture*									mCustomTexture;
};
T_END_NAMESPACE
