#include <tamashii/engine/importer/importer.hpp>

#include <tamashii/public.hpp>
#include <tamashii/engine/scene/image.hpp>
#include <tamashii/engine/scene/material.hpp>
#include <tamashii/engine/scene/model.hpp>
#include <tamashii/engine/scene/scene_graph.hpp>
#include <tamashii/engine/scene/camera.hpp>
#include <tamashii/engine/scene/light.hpp>
#include <tamashii/engine/topology/topology.hpp>

// Define these only in *one* .cc file.
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define JSON_HAS_CPP_17
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_INCLUDE_JSON
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE
#define TINYGLTF_NOEXCEPTION
#define TINYGLTF_USE_CPP14
#include <stb_image.h>
#include <stb_image_write.h>
#include <nlohmann/json.hpp>
#include <tiny_gltf.h>

#include <sstream>
#include <set>
#include <filesystem>
#include <glm/glm.hpp>

T_USE_NAMESPACE
// If defined ies/ldt lights are parsed from *.gltf files.
constexpr bool ENABLE_IES_LIGHTS = true;
constexpr bool ENABLE_LDT_LIGHTS = false;

#define RI_C(typ, data) reinterpret_cast<typ>(data)
// dereferenced interpret cast
#define D_RI_C(typ, data) *RI_C(typ, data)


namespace {
	float animation_cycle_time = 0;
	std::vector<std::vector<Image*>> imageToStorageDirectory = {};
	std::vector<Material*> materialToStorageDirectory = {};
	std::vector<Model*> meshToStorageDirectory = {};
	std::vector<Camera*> cameraToStorageDirectory = {};
	std::vector<Light*> lightToStorageDirectory = {};

	// store image indices that consist of occlusion/rougness/metallic textures
	std::set<int> roughMetalImageIndices;
	std::set<int> occlImageIndices;

	std::vector<Node*> nodeList = {};

	// CONVERTERS
	// IMAGE
	std::string mimeTypeToExtension(const std::string& aMimeType);
	Image::Format tinygltfImageToTextureFormat(const tinygltf::Image& aImg);
	// MATERIAL
	// sampler
	Sampler::Wrap tinygltfTextureWrapTypeTotTextureWrap(const uint32_t aTextureWrapType);
	Sampler::Filter tinygltfTextureFilterTotTextureFilter(const uint32_t textureFilterType);
	Sampler::Filter tinygltfTextureMipMapFilterTotTextureMipMapFilter(const uint32_t aTextureMipMapFilter);
	bool tinygltfTextureHasMipmaps(const uint32_t aTextureFilterType);
	// MODEL
	Mesh::Topology tinygltfModeToTopology(const uint32_t aMode);


	// load the custom properties set in the extra field of the gltf file
	tamashii::Value loadValues(tinygltf::Value& aSrc) {
		if (aSrc.IsInt()) return Value(aSrc.GetNumberAsInt());
		if (aSrc.IsBool()) return Value(aSrc.Get<bool>());
		if (aSrc.IsReal()) return Value(static_cast<float>(aSrc.GetNumberAsDouble()));
		if (aSrc.IsString()) return Value(aSrc.Get<std::string>());
		if (aSrc.IsBinary()) return Value(aSrc.Get<std::vector<unsigned char>>());
		if (aSrc.IsArray()) {
			auto srcValues = aSrc.Get<tinygltf::Value::Array>();
			std::vector<tamashii::Value> dstValues;
			dstValues.reserve(srcValues.size());
			for (tinygltf::Value& srcValue : srcValues) {
				dstValues.emplace_back(loadValues(srcValue));
			}
			return tamashii::Value(dstValues);
		}
		if (aSrc.IsObject()) {
			auto srcValues = aSrc.Get<tinygltf::Value::Object>();
			std::map<std::string, tamashii::Value> dstValues;
			for (auto& srcValue : srcValues) {
				dstValues.emplace(srcValue.first, loadValues(srcValue.second));
			}
			return tamashii::Value(dstValues);
		}
		return {};
	}
	void loadCustomProperties(const tinygltf::Value& aSrc, Asset* aDst) {
		if (aSrc.IsObject()) {
			for (const std::string& key : aSrc.Keys()) {
				tinygltf::Value gltfValue = aSrc.Get(key);
				aDst->addCustomProperty(key, loadValues(gltfValue));
			}
		}
	}


	/**
	* Material
	**/
	void loadTexture(Texture &aTex, const int aTextureIdx, const tinygltf::Model& aModel, const int aImgOffset = 0) {
		const tinygltf::Texture& textureGLTF = aModel.textures[aTextureIdx];
		aTex.image = imageToStorageDirectory[textureGLTF.source][aImgOffset];

		// if no sampler defined use default
		if (textureGLTF.sampler != -1) {
			const tinygltf::Sampler& samplerGLTF = aModel.samplers[textureGLTF.sampler];

			const bool needsMipmaps = aTex.image->needsMipMaps() | tinygltfTextureHasMipmaps(samplerGLTF.minFilter);
			aTex.image->needsMipMaps(needsMipmaps);
			aTex.sampler.min = tinygltfTextureFilterTotTextureFilter(samplerGLTF.minFilter);
			aTex.sampler.mag = tinygltfTextureFilterTotTextureFilter(samplerGLTF.magFilter);
			aTex.sampler.mipmap = tinygltfTextureMipMapFilterTotTextureMipMapFilter(samplerGLTF.minFilter);
			aTex.sampler.wrapU = tinygltfTextureWrapTypeTotTextureWrap(samplerGLTF.wrapS);
			aTex.sampler.wrapV = tinygltfTextureWrapTypeTotTextureWrap(samplerGLTF.wrapT);
			aTex.sampler.wrapW = Sampler::Wrap::REPEAT; // legacy tinygltf wrapR extension removed
			aTex.sampler.minLod = 0;
			aTex.sampler.maxLod = std::numeric_limits<float>::max();
		}
		else {
			aTex.image->needsMipMaps(true);
			aTex.sampler = { Sampler::Filter::LINEAR, Sampler::Filter::LINEAR, Sampler::Filter::LINEAR,
				Sampler::Wrap::REPEAT, Sampler::Wrap::REPEAT, Sampler::Wrap::REPEAT, 0, std::numeric_limits<float>::max() };
		}
	}

	void loadMaterial(Material *m_dst, tinygltf::Material &m_gltf, tinygltf::Model& model, std::deque<Texture*> &textures) {
		glm::vec4 baseColorFactor = glm::make_vec4(m_gltf.pbrMetallicRoughness.baseColorFactor.data());
		glm::vec3 emissionFactor = glm::make_vec3(m_gltf.emissiveFactor.data());
		float metallicFactor = static_cast<float>(m_gltf.pbrMetallicRoughness.metallicFactor);
		float roughnessFactor = static_cast<float>(m_gltf.pbrMetallicRoughness.roughnessFactor);
		float normalScale = static_cast<float>(m_gltf.normalTexture.scale);
		float occlusionStrength = static_cast<float>(m_gltf.occlusionTexture.strength);
		m_dst->setBaseColorFactor(baseColorFactor);
		m_dst->setEmissionFactor(emissionFactor);
		m_dst->setMetallicFactor(metallicFactor);
		m_dst->setRoughnessFactor(roughnessFactor);
		m_dst->setNormalScale(normalScale);
		m_dst->setOcclusionStrength(occlusionStrength);

		m_dst->setCullBackface(!m_gltf.doubleSided);
		if (m_gltf.alphaMode == "OPAQUE") {
			m_dst->setBlendMode(Material::BlendMode::_OPAQUE);
		} else if (m_gltf.alphaMode == "MASK") {
			m_dst->setBlendMode(Material::BlendMode::MASK);
		} else if (m_gltf.alphaMode == "BLEND") {
			m_dst->setBlendMode(Material::BlendMode::BLEND);
		}
		m_dst->setAlphaDiscardValue(static_cast<float>(m_gltf.alphaCutoff));

		if (m_gltf.pbrMetallicRoughness.baseColorTexture.index != -1) {
			Texture* t = Texture::alloc();
			loadTexture(*t, m_gltf.pbrMetallicRoughness.baseColorTexture.index, model);
			t->texCoordIndex = m_gltf.pbrMetallicRoughness.baseColorTexture.texCoord;
			t->image->setSRGB(true);
			m_dst->setBaseColorTexture(t);	
			textures.push_back(t);
		}
		if (m_gltf.pbrMetallicRoughness.metallicRoughnessTexture.index != -1) {
			// roughness
			Texture* t = Texture::alloc();
			loadTexture(*t, m_gltf.pbrMetallicRoughness.metallicRoughnessTexture.index, model, 1);
			t->texCoordIndex = m_gltf.pbrMetallicRoughness.metallicRoughnessTexture.texCoord;
			m_dst->setRoughnessTexture(t);
			textures.push_back(t);
			// metallic
			t = Texture::alloc();
			loadTexture(*t, m_gltf.pbrMetallicRoughness.metallicRoughnessTexture.index, model, 2);
			t->texCoordIndex = m_gltf.pbrMetallicRoughness.metallicRoughnessTexture.texCoord;
			m_dst->setMetallicTexture(t);
			textures.push_back(t);
		}
		if (m_gltf.emissiveTexture.index != -1) {
			Texture* t = Texture::alloc();
			loadTexture(*t, m_gltf.emissiveTexture.index, model);
			t->texCoordIndex = m_gltf.emissiveTexture.texCoord;
			m_dst->setEmissionTexture(t);
			textures.push_back(t);
		}
		if (m_gltf.normalTexture.index != -1) {
			Texture* t = Texture::alloc();
			loadTexture(*t, m_gltf.normalTexture.index, model);
			t->texCoordIndex = m_gltf.normalTexture.texCoord;
			m_dst->setNormalTexture(t);
			textures.push_back(t);
		}
		if (m_gltf.occlusionTexture.index != -1) {
			Texture* t = Texture::alloc();
			loadTexture(*t, m_gltf.occlusionTexture.index, model);
			t->texCoordIndex = m_gltf.occlusionTexture.texCoord;
			m_dst->setOcclusionTexture(t);
			textures.push_back(t);
		}

		loadCustomProperties(m_gltf.extras, m_dst);

		// extensions
		if (!m_gltf.extensions.empty()) {
			tinygltf::Value ext;
			if ((ext = m_gltf.extensions["KHR_materials_ior"]).IsObject()) {
				if (ext.Has("ior")) m_dst->setIOR(static_cast<float>(ext.Get("ior").GetNumberAsDouble()));
			}
			if ((ext = m_gltf.extensions["KHR_materials_transmission"]).IsObject()) {
				if (ext.Has("transmissionFactor")) m_dst->setTransmissionFactor(static_cast<float>(ext.Get("transmissionFactor").GetNumberAsDouble()));

				if (ext.Has("transmissionTexture"))
				{
					Texture* t = Texture::alloc();
					loadTexture(*t, ext.Get("transmissionTexture").Get("index").GetNumberAsInt(), model);
					m_dst->setTransmissionTexture(t);
					textures.push_back(t);
				}
			}
			if ((ext = m_gltf.extensions["KHR_materials_volume"]).IsObject()) {
				tinygltf::Value value;

				if (ext.Has("thicknessFactor")) m_dst->setThicknessFactor(static_cast<float>(ext.Get("thicknessFactor").GetNumberAsDouble()));
				if (ext.Has("attenuationDistance")) m_dst->setAttenuationDistance(static_cast<float>(ext.Get("attenuationDistance").GetNumberAsDouble()));

				if (ext.Has("attenuationColor")) {
					const auto& v = ext.Get("attenuationColor");
					if (v.IsArray()) m_dst->setAttenuationColor({ v.Get(0).GetNumberAsDouble(), v.Get(1).GetNumberAsDouble(), v.Get(2).GetNumberAsDouble() });
				}
			}
		}
	}
	/**
	* Model
	**/
	void loadIndices(Mesh& tmesh, const tinygltf::Model& aModel, const int accessorsIndex) {
		if (accessorsIndex == -1) return;
		const tinygltf::Accessor& accessor = aModel.accessors[accessorsIndex];
		const tinygltf::BufferView& bufferView = aModel.bufferViews[accessor.bufferView];
		const tinygltf::Buffer& buffer = aModel.buffers[bufferView.buffer];
		const int byteStride = accessor.ByteStride(bufferView);

		assert((accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE || accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT || accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
			&& accessor.type == TINYGLTF_TYPE_SCALAR);

		std::vector<uint32_t> &indices = tmesh.getIndicesVectorRef();
		indices.resize(accessor.count);
		const uint8_t* data = (const uint8_t*)&buffer.data[bufferView.byteOffset + accessor.byteOffset];
		for (size_t i = 0; i < accessor.count; i++) {
			if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
				const uint8_t* idx = (uint8_t*)&data[i * byteStride];
				indices[i] = static_cast<uint32_t>(idx[0]);
			}
			else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
				const uint16_t* idx = (uint16_t*)&data[i * byteStride];
				indices[i] = static_cast<uint32_t>(idx[0]);
			}
			else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
				const uint32_t* idx = (uint32_t*)&data[i * byteStride];
				indices[i] = static_cast<uint32_t>(idx[0]);
			}
		}
		tmesh.hasIndices(true);
		assert(indices.size() == accessor.count);
	}

	void loadPositions(std::vector<vertex_s> &aVertices, const tinygltf::Model& model, const int accessorsIndex) {
		if (accessorsIndex == -1) return;
		const tinygltf::Accessor& accessor = model.accessors[accessorsIndex];
		const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
		const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
		const int byteStride = accessor.ByteStride(bufferView);

		assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && accessor.type == TINYGLTF_TYPE_VEC3);
		const uint8_t *data = (const uint8_t*) &buffer.data[bufferView.byteOffset + accessor.byteOffset];
		for (size_t i = 0; i < accessor.count; i++) {
			const glm::vec3* pos = (glm::vec3*)&data[i * byteStride];
			aVertices[i].position = glm::vec4(pos[0], 1);
		}
		assert(aVertices.size() == accessor.count);
	}
	void loadNormals(std::vector<vertex_s>& aVertices, const tinygltf::Model& model, const int accessorsIndex) {
		if (accessorsIndex == -1) return;
		const tinygltf::Accessor& accessor = model.accessors[accessorsIndex];
		const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
		const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
		const int byteStride = accessor.ByteStride(bufferView);

		assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && accessor.type == TINYGLTF_TYPE_VEC3);
		const uint8_t* data = (const uint8_t*)&buffer.data[bufferView.byteOffset + accessor.byteOffset];
		for (size_t i = 0; i < accessor.count; i++) {
			const glm::vec3* normal = (glm::vec3*)&data[i * byteStride];
			aVertices[i].normal = glm::vec4(normal[0], 0);
		}
		assert(aVertices.size() == accessor.count);
	}
	void loadTangents(std::vector<vertex_s>& aVertices, const tinygltf::Model& model, const int accessorsIndex) {
		if (accessorsIndex == -1) return;
		const tinygltf::Accessor& accessor = model.accessors[accessorsIndex];
		const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
		const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
		const int byteStride = accessor.ByteStride(bufferView);

		assert((accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && accessor.type == TINYGLTF_TYPE_VEC4));
		const uint8_t* data = (const uint8_t*)&buffer.data[bufferView.byteOffset + accessor.byteOffset];
		for (size_t i = 0; i < accessor.count; i++) {
			const glm::vec4* tangent = (glm::vec4*)&data[i * byteStride];
			aVertices[i].tangent = tangent[0];
			aVertices[i].tangent[3] = 0;
		}
		assert(aVertices.size() == accessor.count);
	}
	void loadTexCoords0(std::vector<vertex_s>& aVertices, const tinygltf::Model& model, const int accessorsIndex) {
		if (accessorsIndex == -1) return;
		const tinygltf::Accessor& accessor = model.accessors[accessorsIndex];
		const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
		const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
		const int byteStride = accessor.ByteStride(bufferView);
		assert((accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT || accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT || accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
			&& accessor.type == TINYGLTF_TYPE_VEC2);
		const uint8_t* data = (const uint8_t*)&buffer.data[bufferView.byteOffset + accessor.byteOffset];
		for (size_t i = 0; i < accessor.count; i++) {
			if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
				const glm::vec2* uv = (glm::vec2*)&data[i * byteStride];
				aVertices[i].texture_coordinates_0 = uv[0];
			} else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
				const glm::u16vec2* uv = (glm::u16vec2*)&data[i * byteStride];
				aVertices[i].texture_coordinates_0 = glm::vec2(uv[0]) / glm::vec2(65535);
			} else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
				const glm::u8vec2* uv = (glm::u8vec2*)&data[i * byteStride];
				aVertices[i].texture_coordinates_0 = glm::vec2(uv[0]) / glm::vec2(255);
			}
		}
		assert(aVertices.size() == accessor.count);
	}
	void loadTexCoords1(std::vector<vertex_s>& aVertices, const tinygltf::Model& aModel, const int accessorsIndex) {
		if (accessorsIndex == -1) return;
		const tinygltf::Accessor& accessor = aModel.accessors[accessorsIndex];
		const tinygltf::BufferView& bufferView = aModel.bufferViews[accessor.bufferView];
		const tinygltf::Buffer& buffer = aModel.buffers[bufferView.buffer];
		const int byteStride = accessor.ByteStride(bufferView);
		assert((accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT || accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT || accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
			&& accessor.type == TINYGLTF_TYPE_VEC2);
		const uint8_t* data = (const uint8_t*)&buffer.data[bufferView.byteOffset + accessor.byteOffset];
		for (size_t i = 0; i < accessor.count; i++) {
			if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
				const glm::vec2* uv = (glm::vec2*)&data[i * byteStride];
				aVertices[i].texture_coordinates_1 = uv[0];
			}
			else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
				const glm::u16vec2* uv = (glm::u16vec2*)&data[i * byteStride];
				aVertices[i].texture_coordinates_1 = glm::vec2(uv[0]) / glm::vec2(65535);
			}
			else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
				const glm::u8vec2* uv = (glm::u8vec2*)&data[i * byteStride];
				aVertices[i].texture_coordinates_1 = glm::vec2(uv[0]) / glm::vec2(255);
			}
		}
		assert(aVertices.size() == accessor.count);
	}
	void loadColors0(std::vector<vertex_s>& aVertices, const tinygltf::Model& aModel, const int accessorsIndex) {
		if (accessorsIndex == -1) return;
		const tinygltf::Accessor& accessor = aModel.accessors[accessorsIndex];
		const tinygltf::BufferView& bufferView = aModel.bufferViews[accessor.bufferView];
		const tinygltf::Buffer& buffer = aModel.buffers[bufferView.buffer];
		const int byteStride = accessor.ByteStride(bufferView);

		assert((accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT || accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT || 
			accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) && (accessor.type == TINYGLTF_TYPE_VEC3 || accessor.type == TINYGLTF_TYPE_VEC4));
		const uint8_t* data = (const uint8_t*)&buffer.data[bufferView.byteOffset + accessor.byteOffset];
		for (size_t i = 0; i < accessor.count; i++) {
			if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
				const float* color = (float*)&data[i * byteStride];
				if(accessor.type == TINYGLTF_TYPE_VEC3) aVertices[i].color_0 = glm::vec4(color[0], color[1], color[2], 1);
				else if (accessor.type == TINYGLTF_TYPE_VEC4) aVertices[i].color_0 = glm::vec4(color[0], color[1], color[2], color[3]);
			} else if(accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
			{
				const uint16_t* color = (uint16_t*)&data[i * byteStride];
				if (accessor.type == TINYGLTF_TYPE_VEC3) aVertices[i].color_0 = glm::vec4(color[0], color[1], color[2], 65535) / glm::vec4(65535);
				else if (accessor.type == TINYGLTF_TYPE_VEC4) aVertices[i].color_0 = glm::vec4(color[0], color[1], color[2], color[3]) / glm::vec4(65535);
			} else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
			{
				const uint8_t* color = (uint8_t*)&data[i * byteStride];
				if (accessor.type == TINYGLTF_TYPE_VEC3) aVertices[i].color_0 = glm::vec4(color[0], color[1], color[2], 255) / glm::vec4(255);
				else if (accessor.type == TINYGLTF_TYPE_VEC4) aVertices[i].color_0 = glm::vec4(color[0], color[1], color[2], color[3]) / glm::vec4(255);
			}
		}
		assert(aVertices.size() == accessor.count);
	}
	void loadCustomData(const std::string& aKey, std::map<std::string, tamashii::Mesh::CustomData>& aExtraData, const tinygltf::Model& aModel, const int aAccessorsIndex) {
		if (aAccessorsIndex == -1) return;
		const tinygltf::Accessor& accessor = aModel.accessors[aAccessorsIndex];
		const tinygltf::BufferView& bufferView = aModel.bufferViews[accessor.bufferView];
		const tinygltf::Buffer& buffer = aModel.buffers[bufferView.buffer];
		const int byteStride = accessor.ByteStride(bufferView);
		const size_t byteSize = static_cast<size_t>(byteStride) * accessor.count;

		const tamashii::Mesh::CustomData ed(byteSize);
		std::memcpy(ed.data(), &buffer.data[bufferView.byteOffset + accessor.byteOffset], byteSize);
		aExtraData.insert({ aKey, ed });
	}

	void loadVertices(Mesh& tmesh, const tinygltf::Model& aModel, tinygltf::Primitive& aPrimitive) {
		int idxPosition = -1;
		int idxNormal = -1;
		int idxTangent = -1;
		int idxTexcoord0 = -1;
		int idxTexcoord1 = -1;
		int idxTexcoord2 = -1;
		int idxColor0 = -1;
		int idxJoints0 = -1;
		int idxWeights0 = -1;

		for (std::pair<std::string const, int>& attrib : aPrimitive.attributes) {
			if (attrib.first == "POSITION") idxPosition = attrib.second;
			else if (attrib.first == "NORMAL") idxNormal = attrib.second;
			else if (attrib.first == "TANGENT") idxTangent = attrib.second;
			else if (attrib.first == "TEXCOORD_0") idxTexcoord0 = attrib.second;
			else if (attrib.first == "TEXCOORD_1") idxTexcoord1 = attrib.second;
			else if (attrib.first == "TEXCOORD_2") idxTexcoord2 = attrib.second;
			else if (attrib.first == "COLOR_0") idxColor0 = attrib.second;
			else if (attrib.first == "JOINTS_0") idxJoints0 = attrib.second;
			else if (attrib.first == "WEIGHTS_0") idxWeights0 = attrib.second;
			else loadCustomData(attrib.first, tmesh.getCustomDataMapRef(), aModel, attrib.second);
		}

		const bool hasPosition = (idxPosition != -1);
		const bool hasNormal = (idxNormal != -1);
		const bool hasTangent = (idxTangent != -1);
		const bool hasTexcoord0 = (idxTexcoord0 != -1);
		const bool hasTexcoord1 = (idxTexcoord1 != -1);
		const bool hasColor0 = (idxColor0 != -1);
		tmesh.hasPositions(hasPosition);
		tmesh.hasNormals(hasNormal);
		tmesh.hasTangents(hasTangent);
		tmesh.hasTexCoords0(hasTexcoord0);
		tmesh.hasTexCoords1(hasTexcoord1);
		tmesh.hasColors0(hasColor0);

		const size_t vertexCount = aModel.accessors[idxPosition].count;
		if (hasNormal) assert(vertexCount == aModel.accessors[idxNormal].count);
		if (hasTangent) assert(vertexCount == aModel.accessors[idxTangent].count);
		if (hasTexcoord0) assert(vertexCount == aModel.accessors[idxTexcoord0].count);
		if (hasTexcoord1) assert(vertexCount == aModel.accessors[idxTexcoord1].count);
		if (hasColor0) assert(vertexCount == aModel.accessors[idxColor0].count);

		std::vector<vertex_s> &vertices = tmesh.getVerticesVectorRef();
		vertices.resize(vertexCount);
		loadPositions(vertices, aModel, idxPosition);
		loadNormals(vertices, aModel, idxNormal);
		loadTangents(vertices, aModel, idxTangent);
		loadTexCoords0(vertices, aModel, idxTexcoord0);
		loadTexCoords1(vertices, aModel, idxTexcoord1);
		loadColors0(vertices, aModel, idxColor0);

		if (hasPosition) {
			aabb_s aabb;
			aabb.mMin = glm::vec3(aModel.accessors[idxPosition].minValues[0], aModel.accessors[idxPosition].minValues[1], aModel.accessors[idxPosition].minValues[2]);
			aabb.mMax = glm::vec3(aModel.accessors[idxPosition].maxValues[0], aModel.accessors[idxPosition].maxValues[1], aModel.accessors[idxPosition].maxValues[2]);
			tmesh.setAABB(aabb);
		}
	}


	void loadModel(Model* m_dst, tinygltf::Mesh& m_gltf, const tinygltf::Model& model) {
		
		aabb_s aabb;
		for (tinygltf::Primitive& primitive : m_gltf.primitives) {
			Mesh* tmesh = Mesh::alloc();
			const int indicesIdx = primitive.indices;

			tmesh->setTopology(tinygltfModeToTopology(primitive.mode));
			// indices
			loadIndices(*tmesh, model, indicesIdx);
			// vertices
			loadVertices(*tmesh, model, primitive);
			// calculate tangents if not present
			if (!tmesh->hasNormals() && tmesh->getTopology() == Mesh::Topology::TRIANGLE_LIST) topology::calcNormals(tmesh);
			if (!tmesh->hasTangents() && tmesh->getTopology() == Mesh::Topology::TRIANGLE_LIST && tmesh->hasTexCoords0()) topology::calcMikkTSpaceTangents(tmesh);
			if (!tmesh->hasTangents() && tmesh->getTopology() == Mesh::Topology::TRIANGLE_LIST) topology::calcStarkTangent(tmesh);
			
			if (primitive.material != -1)  tmesh->setMaterial(materialToStorageDirectory[primitive.material]);

			if (m_dst->size() == 0) aabb = tmesh->getAABB();
			else aabb = aabb.merge(tmesh->getAABB());
			loadCustomProperties(primitive.extras, tmesh);
			m_dst->addMesh(tmesh);
		}
		m_dst->setAABB(aabb);

		loadCustomProperties(m_gltf.extras, m_dst);
	}

	// CAMERA
	void loadCamera(Camera* c_dst, const tinygltf::Camera& c_gltf, tinygltf::Model& model) {
		if (c_gltf.type == "perspective") {
			const tinygltf::PerspectiveCamera& pc = c_gltf.perspective;
			c_dst->initPerspectiveCamera(pc.yfov, pc.aspectRatio, pc.znear, pc.zfar);
		} else if (c_gltf.type == "orthographic") {
			const tinygltf::OrthographicCamera& oc = c_gltf.orthographic;
			c_dst->initOrthographicCamera(oc.xmag, oc.ymag, oc.znear, oc.zfar);
		}
	}

	// LIGHT
	void loadPointLight(PointLight* l_dst, tinygltf::Light& l_gltf, tinygltf::Model& model) {
		if (l_gltf.color.size() == 3) l_dst->setColor({ l_gltf.color[0], l_gltf.color[1], l_gltf.color[2] });
		// Kv(683) - maximum spectral luminous efficacy of radiation for photoscopic vision,
		// for 555 nm wavelength, that is most perceptually efficient for human vision
		// theta e - radiant flux[watts]
		// theta v - luminous flux[lumens]
		// Iv - luminous intensity[lm / sr or candela], what KHR_punctual_lights mandates for point lights

		// lumen = kv * watts
		const float lumens = 683 * l_gltf.intensity;
		// lm/sr or candela = lumens / 4 pi
		const float candela = lumens / (4 * 3.14159265358979323846);
		l_dst->setIntensity(static_cast<float>(l_gltf.intensity));
		l_dst->setRange(static_cast<float>(l_gltf.range));
	}
	void loadSpotLight(SpotLight* l_dst, tinygltf::Light& l_gltf, tinygltf::Model& model) {
		if (l_gltf.color.size() == 3) l_dst->setColor({ l_gltf.color[0], l_gltf.color[1], l_gltf.color[2] });
		l_dst->setIntensity(static_cast<float>(l_gltf.intensity));
		l_dst->setRange(static_cast<float>(l_gltf.range));
		l_dst->setCone(static_cast<float>(l_gltf.spot.innerConeAngle), static_cast<float>(l_gltf.spot.outerConeAngle));
		l_dst->setDefaultDirection({ 0,0,-1 });
		l_dst->setDefaultTangent({ 1,0,0 });
	}
	void loadDirectionalLight(DirectionalLight* l_dst, tinygltf::Light& l_gltf, tinygltf::Model& model) {
		if (l_gltf.color.size() == 3) l_dst->setColor({ l_gltf.color[0], l_gltf.color[1], l_gltf.color[2] });
		l_dst->setIntensity(static_cast<float>(l_gltf.intensity));
		l_dst->setDefaultDirection({ 0,0,-1 });
		l_dst->setDefaultTangent({ 1,0,0 });
	}
	void loadSurfaceLight(SurfaceLight* l_dst, tinygltf::Light& l_gltf, tinygltf::Model& model) {
		if (l_gltf.color.size() == 3) l_dst->setColor({ l_gltf.color[0], l_gltf.color[1], l_gltf.color[2] });
		l_dst->setIntensity(static_cast<float>(l_gltf.intensity));
		l_dst->setDefaultDirection({ 0,0,-1 });
		l_dst->setDefaultTangent({ 1,0,0 });
		std::string shape = l_gltf.extras.Get("shape").Get<std::string>();
		glm::vec3 dim(0);
		if (shape == "rect") {
			dim.x = static_cast<float>(l_gltf.extras.Get("width").Get<double>());
			dim.y = static_cast<float>(l_gltf.extras.Get("height").Get<double>());
			l_dst->setShape(SurfaceLight::Shape::RECTANGLE);
		} else if (shape == "disk") {
			dim.x = dim.y = static_cast<float>(l_gltf.extras.Get("radius").Get<double>());
			l_dst->setShape(SurfaceLight::Shape::DISK);
		}
		l_dst->setDimensions(dim);
	}
	void loadIESLight(IESLight* l_dst, tinygltf::Light& l_gltf, tinygltf::Model& model) {
		if (l_gltf.color.size() == 3) l_dst->setColor({ l_gltf.color[0], l_gltf.color[1], l_gltf.color[2] });
		l_dst->setIntensity(static_cast<float>(l_gltf.intensity));
		l_dst->setDefaultDirection({ 0,0,-1 });
		l_dst->setDefaultTangent({ 1,0,0 });

		// Kv(683) - maximum spectral luminous efficacy of radiation for photoscopic vision,
		// for 555 nm wavelength, that is most perceptually efficient for human vision
		// theta e - radiant flux[watts]
		// theta v - luminous flux[lumens]
		// Iv - luminous intensity[lm / sr or candela], what KHR_punctual_lights mandates for point lights
		// lumen = kv * watts
		//const float lumens = 683.0f * static_cast<float>(l_gltf.intensity);
		// lm/sr or candela = lumens / 4 pi
		//const float candela = lumens / (4 * 3.14159265358979323846);
	}
}


// calculation 
// translationMatrix = createTranslationMatrix(node.translation);
// rotationMatrix = createRotationMatrix(node.rotation);
// scaleMatrix = createScaleMatrix(node.scale);
// localTransform = translationMatrix * rotationMatrix * scaleMatrix;
#include <glm/gtx/string_cast.hpp>
void addNode(Node* tnode, tinygltf::Node* node, const int node_index, tinygltf::Model& model) {
	spdlog::info("   {}", node->name);
	nodeList[node_index] = tnode;
	// set model or camera for node
	if (node->mesh != -1) {
		Model* m = meshToStorageDirectory[node->mesh];
		tnode->setModel(m);
	}
	if (node->camera != -1) {
		Camera* c = cameraToStorageDirectory[node->camera];
		tnode->setCamera(c);
	}
	if (!node->extensions.empty()) {
		if (node->extensions.count("KHR_lights_punctual")) {
			tinygltf::Value ext = node->extensions["KHR_lights_punctual"];
			if (ext.IsObject()) {
				tinygltf::Value light = ext.Get("light");
				if (light.IsInt()) {
					tnode->setLight(lightToStorageDirectory[light.GetNumberAsInt()]);
				}
			}
		}
	}
	
	// set transformation
	if (node->matrix.size() == 16) {
		glm::dmat4 matrix = glm::make_mat4(node->matrix.data());
		glm::dvec3 scale;
		glm::dquat rotation;
		glm::dvec3 translation;
        glm::dvec3 skew;
        glm::dvec4 perspective;
        ASSERT(glm::decompose(matrix, scale, rotation, translation, skew, perspective), "decompose error");

		tnode->setRotation(glm::vec4(rotation.x, rotation.y, rotation.z, rotation.w));
		tnode->setScale(glm::vec3(scale.x, scale.y, scale.z));
		tnode->setTranslation(glm::vec3(translation.x, translation.y, translation.z));
	}
	else {
		if (node->rotation.size() == 4) tnode->setRotation(glm::vec4(node->rotation[0], node->rotation[1], node->rotation[2], node->rotation[3]));
		if (node->scale.size() == 3) tnode->setScale(glm::vec3(node->scale[0], node->scale[1], node->scale[2]));
		if (node->translation.size() == 3) tnode->setTranslation(glm::vec3(node->translation[0], node->translation[1], node->translation[2]));
	}

	loadCustomProperties(node->extras, tnode);

	// process children
	for (int nodeIndex : node->children) {
		tinygltf::Node *n = &model.nodes[nodeIndex];
		Node* tn = Node::alloc(n->name);
		addNode(tn, n, nodeIndex, model);
		tnode->addNode(tn);
	}
}

void loadAnimation(tinygltf::Animation& a_gltf, const tinygltf::Model& aModel) {
	for (tinygltf::AnimationChannel& ac_gltf : a_gltf.channels) {
		if (ac_gltf.target_node == -1) continue;
		Node* tnode = nodeList[ac_gltf.target_node];
		tinygltf::AnimationSampler& as_gltf = a_gltf.samplers[ac_gltf.sampler];

		TRS::Interpolation interp = TRS::Interpolation::NONE;
		if (as_gltf.interpolation == "LINEAR") interp = TRS::Interpolation::LINEAR;
		else if (as_gltf.interpolation == "STEP")  interp = TRS::Interpolation::STEP;
		else if (as_gltf.interpolation == "CUBICSPLINE")  interp = TRS::Interpolation::CUBICSPLINE;

		// time steps
		std::vector<float> time_steps;
		{
			const tinygltf::Accessor& accessor = aModel.accessors[as_gltf.input];
			const tinygltf::BufferView& bufferView = aModel.bufferViews[accessor.bufferView];
			const tinygltf::Buffer& buffer = aModel.buffers[bufferView.buffer];
			const int byteStride = accessor.ByteStride(bufferView);
			assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && accessor.type == TINYGLTF_TYPE_SCALAR && byteStride == 4);
			time_steps.resize(accessor.count);
			animation_cycle_time = std::max(animation_cycle_time, static_cast<float>(accessor.maxValues[0]));

			for (size_t i = 0; i < accessor.count; i++) {
				uint8_t* data = (uint8_t*)&buffer.data[bufferView.byteOffset + accessor.byteOffset + (i * byteStride)];
				if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) time_steps[i] = D_RI_C(float*, data);
				else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_BYTE) time_steps[i] = std::fmax(D_RI_C(int8_t*, data) / 127.0f, -1.0f);
				else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) time_steps[i] = D_RI_C(uint8_t*, data) / 255.0f;
				else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_SHORT) time_steps[i] = std::fmax(D_RI_C(int16_t*, data) / 32767.0f, -1.0f);
				else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) time_steps[i] = D_RI_C(uint16_t*, data) / 65535.0f;
			}
		}
		if (time_steps.size() == 1) continue;
		{
			const tinygltf::Accessor& accessor = aModel.accessors[as_gltf.output];
			const tinygltf::BufferView& bufferView = aModel.bufferViews[accessor.bufferView];
			const tinygltf::Buffer& buffer = aModel.buffers[bufferView.buffer];
			const int byteStride = accessor.ByteStride(bufferView);

			if (ac_gltf.target_path == "translation") {
				std::vector<glm::vec3> translation_steps;
				translation_steps.resize(accessor.count);
				assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && accessor.type == TINYGLTF_TYPE_VEC3 && byteStride == 12);
				const glm::vec3* data = (const glm::vec3*)&buffer.data[bufferView.byteOffset + accessor.byteOffset];
				for (uint32_t i = 0; i < accessor.count; i++) translation_steps[i] = glm::vec3(data[i]);
				tnode->setTranslationAnimation(interp, time_steps, translation_steps);
			}
			else if (ac_gltf.target_path == "rotation") {
				std::vector<glm::vec4> rotation_steps;
				rotation_steps.resize(accessor.count);
				assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && accessor.type == TINYGLTF_TYPE_VEC4 && byteStride == 16);
				for (uint32_t i = 0; i < accessor.count; i++) {
					uint8_t* data = (uint8_t*)&buffer.data[bufferView.byteOffset + accessor.byteOffset + (i * byteStride)];
					if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) rotation_steps[i] = (D_RI_C(glm::vec4*, data));
					else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_BYTE) rotation_steps[i] = glm::max(glm::vec4(D_RI_C(glm::i8vec4*, data)) / 127.0f, glm::vec4(-1.0f));
					else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) rotation_steps[i] = (glm::vec4(D_RI_C(glm::u8vec4*, data)) / 255.0f);
					else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_SHORT) rotation_steps[i] = glm::max(glm::vec4(D_RI_C(glm::i16vec4*, data)) / 32767.0f, glm::vec4(-1.0f));
					else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) rotation_steps[i] = (glm::vec4(D_RI_C(glm::u16vec4*, data)) / 65535.0f);
				}
				tnode->setRotationAnimation(interp, time_steps, rotation_steps);
			}
			else if (ac_gltf.target_path == "scale") {
				std::vector<glm::vec3> scale_steps;
				scale_steps.resize(accessor.count);
				assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && accessor.type == TINYGLTF_TYPE_VEC3 && byteStride == 12);
				uint8_t* data;
				for (uint32_t i = 0; i < accessor.count; i++) {
					data = (uint8_t*)&buffer.data[bufferView.byteOffset + accessor.byteOffset + (i * byteStride)];
					if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) scale_steps[i] = (D_RI_C(glm::vec3*, data));
					else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_BYTE) scale_steps[i] = glm::max(glm::vec3(D_RI_C(glm::i8vec3*, data)) / 127.0f, glm::vec3(-1.0f));
					else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) scale_steps[i] = (glm::vec3(D_RI_C(glm::u8vec3*, data)) / 255.0f);
					else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_SHORT) scale_steps[i] = glm::max(glm::vec3(D_RI_C(glm::i16vec3*, data)) / 32767.0f, glm::vec3(-1.0f));
					else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) scale_steps[i] = (glm::vec3(D_RI_C(glm::u16vec3*, data)) / 65535.0f);
				}
				tnode->setScaleAnimation(interp, time_steps, scale_steps);
			}
			else if (ac_gltf.target_path == "weights") {
				assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && accessor.type == TINYGLTF_TYPE_SCALAR && byteStride == 4);
			}
		}	
	}
}

void loadScene(SceneInfo_s* si, tinygltf::Model& aModel, const std::string& aDirectory) {

	// check for metallic roughness occlusion texture index so we can split it
	for (const tinygltf::Material& gltfMat : aModel.materials) {
		if (gltfMat.pbrMetallicRoughness.metallicRoughnessTexture.index != -1) {
			const int metRouSource = aModel.textures[gltfMat.pbrMetallicRoughness.metallicRoughnessTexture.index].source;
			roughMetalImageIndices.insert(metRouSource);
			if (gltfMat.occlusionTexture.index != -1) {
				const int occlSource = aModel.textures[gltfMat.occlusionTexture.index].source;
				if(occlSource == metRouSource) occlImageIndices.insert(aModel.textures[gltfMat.occlusionTexture.index].source);
			}
		}
	}

	// IMAGE
	if (!aModel.images.empty()) spdlog::info("Loading Images:");
	int idx = 0;
	for (tinygltf::Image& img_gltf : aModel.images) {
		std::ostringstream oss;
		oss << aDirectory << std::filesystem::path("/").make_preferred().string() << img_gltf.uri;
		img_gltf.uri = oss.str();
		if (!std::filesystem::path(img_gltf.uri).has_extension()) {
			std::string ext = mimeTypeToExtension(img_gltf.mimeType);
			if(!img_gltf.name.empty()) img_gltf.uri += img_gltf.name + ext;
			else img_gltf.uri += "image_" + std::to_string(idx) + ext;
		}
		// non metallic roughness images, just use the image
		if (roughMetalImageIndices.find(idx) == roughMetalImageIndices.end()) {
			Image* img = Image::alloc(img_gltf.name);
			img->setFilepath(img_gltf.uri);
			img->init(img_gltf.width, img_gltf.height,
				tinygltfImageToTextureFormat(img_gltf), &img_gltf.image[0]);

			imageToStorageDirectory.push_back({ img });
			si->mImages.push_back(img);
			spdlog::info("   {} {}", img->getName(), img->getFilepath());
		}
		else {
			// Red: Ambient Occlusion
			// Green : Roughness
			// Blue : Metallic
			imageToStorageDirectory.emplace_back();
			std::vector<Image*>& img_vec = imageToStorageDirectory.back();
			img_vec.reserve(3);

			// split metallic roughness and occlusion
			std::vector<unsigned char> image_data(img_gltf.width * img_gltf.height);
			// occlusion - offset 0
			// if no occlusion texture -> null
			if (occlImageIndices.find(idx) != occlImageIndices.end()) {
				for (size_t i = 0, j = 0; j < img_gltf.image.size(); i++, j += 4) {
					image_data[i] = (img_gltf.image[j]);
				}
				Image* img = Image::alloc(img_gltf.name + "_occlusion");
				img->setFilepath(img_gltf.uri);
				img->init(img_gltf.width, img_gltf.height, Image::Format::R8_UNORM, &image_data[0]);
				img_vec.push_back(img);
				si->mImages.push_back(img);
				spdlog::info("   {} {}", img->getName(), img->getFilepath());
			} else img_vec.push_back(nullptr);
			// roughness - offset 1
			{
				for (size_t i = 0, j = 0; j < img_gltf.image.size(); i++, j += 4) {
					image_data[i] = (img_gltf.image[j + 1]);
				}
				Image* img = Image::alloc(img_gltf.name + "_roughness");
				img->setFilepath(img_gltf.uri);
				img->init(img_gltf.width, img_gltf.height, Image::Format::R8_UNORM, &image_data[0]);
				img_vec.push_back(img);
				si->mImages.push_back(img);
				spdlog::info("   {} {}", img->getName(), img->getFilepath());
			}
			// metallic - offset 2
			{
				for (size_t i = 0, j = 0; j < img_gltf.image.size(); i++, j += 4) {
					image_data[i] = (img_gltf.image[j + 2]);
				}
				Image* img = Image::alloc(img_gltf.name + "_metallic");
				img->setFilepath(img_gltf.uri);
				img->init(img_gltf.width, img_gltf.height, Image::Format::R8_UNORM, &image_data[0]);
				img_vec.push_back(img);
				si->mImages.push_back(img);
				spdlog::info("   {} {}", img->getName(), img->getFilepath());
			}
		}
		idx++;
	}
	// MATERIAL
	if (!aModel.materials.empty()) spdlog::info("Loading Materials:");
	for (tinygltf::Material& m_gltf : aModel.materials) {
		Material* m = Material::alloc(m_gltf.name);
		loadMaterial(m, m_gltf, aModel, si->mTextures);
		materialToStorageDirectory.push_back(m);
		si->mMaterials.push_back(m);
		spdlog::info("   {} {}", m->getName(), m->getFilepath());
	}
	// MODEL
	if (!aModel.meshes.empty()) spdlog::info("Loading Models:");
	for (tinygltf::Mesh& m_gltf : aModel.meshes) {
		Model* m = Model::alloc(m_gltf.name);
		loadModel(m, m_gltf, aModel);
		meshToStorageDirectory.push_back(m);
		si->mModels.push_back(m);

		// if mesh has no material -> add default
		for (Mesh* mesh : *m) {
			if (mesh->getMaterial() == nullptr) {
				Material* mat = Material::alloc(DEFAULT_MATERIAL_NAME);
				mesh->setMaterial(mat);
				si->mMaterials.push_back(mat);
			}
		}
		spdlog::info("   {} {}", m->getName(), m->getFilepath());
	}
	// CAMERA
	if (!aModel.cameras.empty()) spdlog::info("Loading Cameras:");
	for (tinygltf::Camera& c_gltf : aModel.cameras) {
		Camera* c = Camera::alloc();
		c->setName(c_gltf.name);
		loadCamera(c, c_gltf, aModel);
		cameraToStorageDirectory.push_back(c);
		si->mCameras.push_back(c);
		spdlog::info("   {} {}", c->getName(), c->getFilepath());
	}

	// LIGHT
	if(!aModel.lights.empty()) spdlog::info("Loading Lights:");
	for (tinygltf::Light& l_gltf : aModel.lights) {
		Light* l = nullptr;
		if (l_gltf.type == "directional") {
			l = new DirectionalLight();
			loadDirectionalLight(static_cast<DirectionalLight*>(l), l_gltf, aModel);
		}
		else if (l_gltf.type == "point") {
			l = new PointLight();
			loadPointLight(static_cast<PointLight*>(l), l_gltf, aModel);
		}
		else if (l_gltf.type == "spot") {
			// check if spotlight has a property with a ies/ldt light path
			const bool ies = ENABLE_IES_LIGHTS && l_gltf.extras.Has("ies");
			const bool ldt = ENABLE_LDT_LIGHTS && l_gltf.extras.Has("ldt");

			if (!ies && !ldt) {
				l = new SpotLight();
				loadSpotLight(static_cast<SpotLight*>(l), l_gltf, aModel);
			} else if(ies) {
				const std::string filename = l_gltf.extras.Get("ies").Get<std::string>();
				const std::string filepath = std::filesystem::path(aDirectory + "/" + filename).make_preferred().string();
				l = Importer::load_light(filepath);
				if (l->getType() == Light::Type::IES) {
					loadIESLight(static_cast<IESLight*>(l), l_gltf, aModel);
					l->setFilepath(filepath);
					si->mImages.push_back(((IESLight*)l)->getCandelaTexture()->image);
					si->mTextures.push_back(((IESLight*)l)->getCandelaTexture());
				}
				else spdlog::error("glTF: ies light could not be loaded");
			}
			else if (ldt) {
			}
		} else if(l_gltf.type == "KHR_lights_area")
		{
			l = new SurfaceLight();
			loadSurfaceLight(static_cast<SurfaceLight*>(l), l_gltf, aModel);
		}

		if (l) {
			l->setName(l_gltf.name);
			loadCustomProperties(l_gltf.extras, l);
			lightToStorageDirectory.push_back(l);
			si->mLights.push_back(l);
			spdlog::info("   {} {}", l->getName(), l->getFilepath());
		}
	}

	// SCENE
	if (!aModel.scenes.empty()) {
		spdlog::info("Loading SceneGraph:");
		SceneGraph* tscene = SceneGraph::alloc(aModel.scenes[0].name);
		for (const int root_index : aModel.scenes[0].nodes) {
			tinygltf::Node* rootNode = &aModel.nodes[root_index];
			Node* tRootNode = Node::alloc(rootNode->name);
			addNode(tRootNode, rootNode, root_index, aModel);
			tscene->addRootNode(tRootNode);
		}
		si->mSceneGraphs.push_back(tscene);
	}

	// ANIMATION
	for (tinygltf::Animation& a_gltf : aModel.animations) {
		loadAnimation(a_gltf, aModel);
	}
}

SceneInfo_s* Importer::load_gltf(std::string const& aFile) {
	tinygltf::Model model;
	tinygltf::TinyGLTF loader;
	std::string err;
	std::string warn;

	spdlog::info("...using tiny glTF");
	bool ret = false;
	if (strstr(aFile.c_str(), ".gltf") != nullptr) ret = loader.LoadASCIIFromFile(&model, &err, &warn, aFile);
	else if (strstr(aFile.c_str(), ".glb") != nullptr) ret = loader.LoadBinaryFromFile(&model, &err, &warn, aFile); // for binary glTF(.glb)

	if (!warn.empty()) {
		spdlog::warn("Load glTF {}", warn.c_str());
		return nullptr;
	}

	if (!err.empty()) {
		spdlog::error("Load glTF {}", err.c_str());
		return nullptr;
	}

	if (!ret) {
		spdlog::critical("...failed");
		return nullptr;
	}
	spdlog::info("...success");

	//dbgModel(model);

	animation_cycle_time = 0;

	imageToStorageDirectory.reserve(model.images.size());
	materialToStorageDirectory.reserve(model.materials.size());
	meshToStorageDirectory.reserve(model.meshes.size());
	cameraToStorageDirectory.reserve(model.cameras.size());
	lightToStorageDirectory.reserve(model.lights.size());
	nodeList.resize(model.nodes.size());

	SceneInfo_s* si = SceneInfo_s::alloc();
	loadScene(si, model, std::filesystem::path(aFile).parent_path().string());
	si->mCycleTime = animation_cycle_time;

	imageToStorageDirectory.clear();
	materialToStorageDirectory.clear();
	meshToStorageDirectory.clear();
	cameraToStorageDirectory.clear();
	lightToStorageDirectory.clear();
	roughMetalImageIndices.clear();
	occlImageIndices.clear();
	nodeList.clear();

	return si;
}

namespace {
	std::string mimeTypeToExtension(const std::string& aMimeType)
	{
		if (aMimeType == "image/png") return ".png";
		if (aMimeType == "image/jpeg") return ".jpg";
		if (aMimeType == "image/bmp") return ".bmp";
		if (aMimeType == "image/gif") return ".gif";
		return "";
	}
	// IMAGE
	Image::Format tinygltfImageToTextureFormat(const tinygltf::Image& aImg) {
		if (aImg.bits == 8 && aImg.component == 3) return Image::Format::RGB8_UNORM;
		if (aImg.bits == 8 && aImg.component == 4) return Image::Format::RGBA8_UNORM;
		if (aImg.bits == 16 && aImg.component == 3) return Image::Format::RGB16_UNORM;
		if (aImg.bits == 16 && aImg.component == 4) return Image::Format::RGBA16_UNORM;
		return Image::Format::UNKNOWN;
	}
	// MATERIAL
	Sampler::Wrap tinygltfTextureWrapTypeTotTextureWrap(const uint32_t aTextureWrapType) {
		switch (aTextureWrapType)
		{
		case 10497: return Sampler::Wrap::REPEAT;				// REPEAT
		case 33071: return Sampler::Wrap::CLAMP_TO_EDGE;		// CLAMP_TO_EDGE
		case 33648: return Sampler::Wrap::MIRRORED_REPEAT;	// MIRRORED_REPEAT
		default: return Sampler::Wrap::REPEAT;
		}
	}
	Sampler::Filter tinygltfTextureFilterTotTextureFilter(const uint32_t textureFilterType) {
		switch (textureFilterType)
		{
		case 9728: return Sampler::Filter::NEAREST;	// NEAREST
		case 9729: return Sampler::Filter::LINEAR;	// LINEAR
		case 9984: return Sampler::Filter::NEAREST;	// NEAREST_MIPMAP_NEAREST
		case 9985: return Sampler::Filter::LINEAR;	// LINEAR_MIPMAP_NEAREST
		case 9986: return Sampler::Filter::NEAREST;	// NEAREST_MIPMAP_LINEAR
		case 9987: return Sampler::Filter::LINEAR;	// LINEAR_MIPMAP_LINEAR
		default: return Sampler::Filter::LINEAR;
		}
	}
	// use with min filter in case of gltf
	Sampler::Filter tinygltfTextureMipMapFilterTotTextureMipMapFilter(const uint32_t aTextureMipMapFilter) {
		switch (aTextureMipMapFilter)
		{
		case 9728: return Sampler::Filter::NEAREST;		// NEAREST
		case 9729: return Sampler::Filter::NEAREST;		// LINEAR
		case 9984: return Sampler::Filter::NEAREST;		// NEAREST_MIPMAP_NEAREST
		case 9985: return Sampler::Filter::NEAREST;		// LINEAR_MIPMAP_NEAREST
		case 9986: return Sampler::Filter::LINEAR;		// NEAREST_MIPMAP_LINEAR
		case 9987: return Sampler::Filter::LINEAR;		// LINEAR_MIPMAP_LINEAR
		default: return Sampler::Filter::LINEAR;
		}
	}
	bool tinygltfTextureHasMipmaps(const uint32_t aTextureFilterType) {
		switch (aTextureFilterType)
		{
		case 9728: // NEAREST
		case 9729: // LINEAR
			return false;
		case 9984: // NEAREST_MIPMAP_NEAREST
		case 9985: // LINEAR_MIPMAP_NEAREST
		case 9986: // NEAREST_MIPMAP_LINEAR
		case 9987: // LINEAR_MIPMAP_LINEAR
			return true;
		default:
			return false;
		}
	}

	// MODEL
	Mesh::Topology tinygltfModeToTopology(const uint32_t aMode) {
		switch (aMode)
		{
		case 0: return Mesh::Topology::POINT_LIST;
		case 1: return Mesh::Topology::LINE_LIST;
		case 3: return Mesh::Topology::LINE_STRIP;
		case 4: return Mesh::Topology::TRIANGLE_LIST;
		case 5: return Mesh::Topology::TRIANGLE_STRIP;
		case 6: return Mesh::Topology::TRIANGLE_FAN;
		default: return Mesh::Topology::UNKNOWN;
		}
	}
}
