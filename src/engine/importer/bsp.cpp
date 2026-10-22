#include <tamashii/engine/importer/importer.hpp>

#include <tamashii/public.hpp>
#include <tamashii/engine/scene/image.hpp>
#include <tamashii/engine/scene/material.hpp>
#include <tamashii/engine/scene/model.hpp>
#include <tamashii/engine/scene/scene_graph.hpp>

#include <sstream>
#include <fstream>
#include <string>
#include <array>
#include <filesystem>
#include <glm/glm.hpp>

T_USE_NAMESPACE

constexpr uint32_t CONTENTS_SOLID = 0x1;
constexpr uint32_t CONTENTS_LAVA = 0x8;
constexpr uint32_t CONTENTS_SLIME = 0x10;
constexpr uint32_t CONTENTS_WATER = 0x20;
constexpr uint32_t CONTENTS_FOG = 0x40;
constexpr uint32_t CONTENTS_NOTTEAM1 = 0x80;
constexpr uint32_t CONTENTS_NOTTEAM2 = 0x100;
constexpr uint32_t CONTENTS_NOBOTCLIP = 0x200;
constexpr uint32_t CONTENTS_AREAPORTAL = 0x8000;
constexpr uint32_t CONTENTS_PLAYERCLIP = 0x10000;
constexpr uint32_t CONTENTS_MONSTERCLIP = 0x20000;
constexpr uint32_t CONTENTS_TELEPORTER = 0x40000;
constexpr uint32_t CONTENTS_JUMPPAD = 0x80000;
constexpr uint32_t CONTENTS_CLUSTERPORTAL = 0x100000;
constexpr uint32_t CONTENTS_DONOTENTER = 0x200000;
constexpr uint32_t CONTENTS_BOTCLIP = 0x400000;
constexpr uint32_t CONTENTS_MOVER = 0x800000;
constexpr uint32_t CONTENTS_ORIGIN = 0x1000000;
constexpr uint32_t CONTENTS_BODY = 0x2000000;
constexpr uint32_t CONTENTS_CORPSE = 0x4000000;
constexpr uint32_t CONTENTS_DETAIL = 0x8000000;
constexpr uint32_t CONTENTS_STRUCTURAL = 0x10000000;
constexpr uint32_t CONTENTS_TRANSLUCENT = 0x20000000;
constexpr uint32_t CONTENTS_TRIGGER = 0x40000000;
constexpr uint32_t CONTENTS_NODROP = 0x80000000;

constexpr uint32_t SURF_NODAMAGE = 0x1;
constexpr uint32_t SURF_SLICK = 0x2;
constexpr uint32_t SURF_SKY = 0x4;
constexpr uint32_t SURF_LADDER = 0x8;
constexpr uint32_t SURF_NOIMPACT = 0x10;
constexpr uint32_t SURF_NOMARKS = 0x20;
constexpr uint32_t SURF_FLESH = 0x40;
constexpr uint32_t SURF_NODRAW = 0x80;
constexpr uint32_t SURF_HINT = 0x100;
constexpr uint32_t SURF_SKIP = 0x200;
constexpr uint32_t SURF_NOLIGHTMAP = 0x400;
constexpr uint32_t SURF_POINTLIGHT = 0x800;
constexpr uint32_t SURF_METALSTEPS = 0x1000;
constexpr uint32_t SURF_NOSTEPS = 0x2000;
constexpr uint32_t SURF_NONSOLID = 0x4000;
constexpr uint32_t SURF_LIGHTFILTER = 0x8000;
constexpr uint32_t SURF_ALPHASHADOW = 0x10000;
constexpr uint32_t SURF_NODLIGHT = 0x20000;
constexpr uint32_t SURF_SURFDUST = 0x40000;

constexpr uint32_t LIGHT_MAP_SIZE = 128u * 128u * 4u;

struct BSPLump
{
	int offset;
	int size;
};

struct BSPHeader
{
	char magic[4];
	int version;
	BSPLump lumps[17];
};

struct BSPRawFace
{
	int shader;
	int effect;
	int type;
	int vertexOffset;
	int vertexCount;
	int meshVertexOffset;
	int meshVertexCount;
	int lightMap;
	int lightMapStart[2];
	int lightMapSize[2];
	glm::vec3 lightMapOrigin;
	glm::vec3 lightMapVecs[2];
	glm::vec3 normal;
	int size[2];
};

struct BSPRawTexture
{
	char name[64];
	int surface;
	int contents;
};

struct BSPRawMeshvert {
	unsigned int offset;
};

struct BSPVertex {
	glm::vec3 position;
	glm::vec2 texCoord;
	glm::vec2 lmCoord;
	glm::vec3 normal;
	unsigned char color[4];
};

struct BSPTexture {
	bool render;
	bool solid;
	bool transparent;
	std::string name;
	std::string path;
};

struct BSPLightMap {
	std::array<uint8_t, LIGHT_MAP_SIZE> data;
};

struct BSPMesh {
	std::vector<BSPVertex> vertices;
	std::vector<uint32_t> indices;

	int effect;
	int texture;
	int lightMap;
};

static std::vector<BSPRawFace> loadFaces(std::ifstream& aIn, const int aOffset, const int aSize) {
	const int faceCount = aSize / static_cast<int>(sizeof(BSPRawFace));
	std::vector<BSPRawFace> faces(faceCount);

	aIn.seekg(aOffset);
	aIn.read(reinterpret_cast<char*>(&faces[0]), aSize);

	return faces;
}

static std::vector<BSPVertex> loadVertices(std::ifstream& aIn, const int aOffset, const int aSize) {
	const int vertexCount = aSize / static_cast<int>(sizeof(BSPVertex));
	std::vector<BSPVertex> vertices(vertexCount);

	aIn.seekg(aOffset);
	aIn.read(reinterpret_cast<char*>(&vertices[0]), aSize);

	return vertices;
}

static std::vector<BSPRawMeshvert> loadMeshverts(std::ifstream& aIn, const int aOffset, const int aSize) {
	const int meshvertCount = aSize / static_cast<int>(sizeof(BSPRawMeshvert));
	std::vector<BSPRawMeshvert> meshverts(meshvertCount);

	aIn.seekg(aOffset);
	aIn.read(reinterpret_cast<char*>(&meshverts[0]), aSize);

	return meshverts;
}

static std::vector<BSPRawTexture> loadTextures(std::ifstream& aIn, const int aOffset, const int aSize) {
	const int shaderCount = aSize / static_cast<int>(sizeof(BSPRawTexture));
	std::vector<BSPRawTexture> shaders(shaderCount);

	aIn.seekg(aOffset);
	aIn.read(reinterpret_cast<char*>(&shaders[0]), aSize);

	return shaders;
}

static std::vector<BSPLightMap> loadLightMaps(std::ifstream& aIn, const int aOffset, const int aSize) {
	const int lightMapCount = aSize / (128 * 128 * 3);
	std::vector<BSPLightMap> lightMaps(lightMapCount);

	aIn.seekg(aOffset);

	for (BSPLightMap& lighMap : lightMaps) {
		for (uint32_t i = 0; i < 128u * 128u; i++) {
			aIn.read(reinterpret_cast<char*>(&lighMap.data[i * 4u]), 3);
			lighMap.data[i * 4u + 3u] = 255;
		}
	}

	return lightMaps;
}

static BSPVertex operator+(const BSPVertex& aV1, const BSPVertex& aV2)
{
	BSPVertex temp = {};
	temp.position = aV1.position + aV2.position;
	temp.texCoord = aV1.texCoord + aV2.texCoord;
	temp.lmCoord = aV1.lmCoord + aV2.lmCoord;
	temp.normal = aV1.normal + aV2.normal;
	return temp;
}

static BSPVertex operator*(const BSPVertex& aV1, const float& aD)
{
	BSPVertex temp = {};
	temp.position = aV1.position * aD;
	temp.texCoord = aV1.texCoord * aD;
	temp.lmCoord = aV1.lmCoord * aD;
	temp.normal = aV1.normal * aD;
	return temp;
}

static bool exists(const std::string& aName) {
	const std::ifstream f(aName.c_str());
	return f.good();
}

static std::string getFolder(const std::string& aBspFile) {
	return std::filesystem::path(aBspFile).parent_path().parent_path().string();
}

// TODO: fix duplicate vertices! reuse vertices from previous calls
static void tesselate(const BSPVertex& aV11, const BSPVertex& aV12, const BSPVertex& aV13, const BSPVertex& aV21, const BSPVertex& aV22, const BSPVertex& aV23, const BSPVertex& aV31, const BSPVertex& aV32, const BSPVertex& aV33, std::vector<BSPVertex>& aVerticesOut, std::vector<uint32_t>& aIndicesOut, const int aBezierLevel)
{
	std::vector<BSPVertex> vertices;
	const auto offset = static_cast<uint32_t>(aVerticesOut.size());

	for (int j = 0; j <= aBezierLevel; j++)
	{
		float a = static_cast<float>(j) / static_cast<float>(aBezierLevel);
		float b = 1.f - a;
		vertices.push_back(aV11 * b * b + aV21 * 2 * b * a + aV31 * a * a);
	}

	for (int i = 1; i <= aBezierLevel; i++)
	{
		float a = static_cast<float>(i) / static_cast<float>(aBezierLevel);
		float b = 1.f - a;

		BSPVertex temp[3] = {};

		temp[0] = aV11 * b * b + aV12 * 2 * b * a + aV13 * a * a;
		temp[1] = aV21 * b * b + aV22 * 2 * b * a + aV23 * a * a;
		temp[2] = aV31 * b * b + aV32 * 2 * b * a + aV33 * a * a;

		for (int j = 0; j <= aBezierLevel; j++)
		{
			float aa = static_cast<float>(j) / static_cast<float>(aBezierLevel);
			float bb = 1.f - aa;

			vertices.push_back(temp[0] * bb * bb + temp[1] * 2 * bb * aa + temp[2] * aa * aa);
		}
	}

	aVerticesOut.insert(aVerticesOut.end(), vertices.begin(), vertices.end());

	const int L1 = aBezierLevel + 1;
	for (int i = 0; i < aBezierLevel; i++)
	{
		for (int j = 0; j < aBezierLevel; j++)
		{
			aIndicesOut.push_back(offset + (i) * L1 + (j));
			aIndicesOut.push_back(offset + (i + 1) * L1 + (j + 1));
			aIndicesOut.push_back(offset + (i) * L1 + (j + 1));

			aIndicesOut.push_back(offset + (i + 1) * L1 + (j + 1));
			aIndicesOut.push_back(offset + (i) * L1 + (j));
			aIndicesOut.push_back(offset + (i + 1) * L1 + (j));
		}
	}
}

static std::tuple<std::vector<Model*>, std::vector<Material*>, std::vector<Texture*>, std::vector<Image*>> createModels(
	const std::vector<BSPMesh>& aMeshes, const std::vector<BSPTexture>& aTextures, const std::vector<BSPLightMap>& aLightMaps) {
	std::vector<Model*> models;
	std::vector<Material*> materials;
	std::vector<Texture*> textures;
	std::vector<Image*> images;
	models.reserve(aMeshes.size());
	materials.reserve(aMeshes.size());
	textures.reserve(aTextures.size() + aLightMaps.size());
	images.reserve(aTextures.size() + aLightMaps.size());

	// textures
	std::vector<Texture*> baseColorTextures;
	baseColorTextures.reserve(aTextures.size());
	for (const BSPTexture& bspTexture : aTextures) {
		if (!bspTexture.path.empty()) {
			Texture* baseColorTexture = Texture::alloc();

			Image* image = Importer::load_image_8_bit(bspTexture.path, 4);
			image->needsMipMaps(true);
			baseColorTexture->image = image;
			baseColorTexture->texCoordIndex = 0;

			Sampler sampler = {};
			sampler.min = Sampler::Filter::LINEAR;
			sampler.mag = Sampler::Filter::LINEAR;
			sampler.mipmap = Sampler::Filter::LINEAR;
			sampler.wrapU = Sampler::Wrap::REPEAT;
			sampler.wrapV = Sampler::Wrap::REPEAT;
			sampler.wrapW = Sampler::Wrap::REPEAT;
			baseColorTexture->sampler = sampler;

			images.push_back(image);
			textures.push_back(baseColorTexture);
			baseColorTextures.emplace_back(baseColorTexture);
		}
		else {
			baseColorTextures.emplace_back(nullptr);
		}
	}

	// lightMaps
	std::vector<Texture*> lightTextures;
	lightTextures.reserve(aLightMaps.size());
	for (const BSPLightMap& bspLightMap : aLightMaps) {
		Texture* lightmap = Texture::alloc();

		Image* image = Image::alloc("LightMap");
		image->init(128, 128, Image::Format::RGBA8_UNORM, bspLightMap.data.data());
		lightmap->image = image;
		lightmap->texCoordIndex = 1;

		Sampler sampler = {};
		sampler.min = Sampler::Filter::LINEAR;
		sampler.mag = Sampler::Filter::LINEAR;
		sampler.mipmap = Sampler::Filter::LINEAR;
		sampler.wrapU = Sampler::Wrap::CLAMP_TO_BORDER;
		sampler.wrapV = Sampler::Wrap::CLAMP_TO_BORDER;
		sampler.wrapW = Sampler::Wrap::CLAMP_TO_BORDER;
		lightmap->sampler = sampler;

		images.push_back(image);
		textures.push_back(lightmap);
		lightTextures.push_back(lightmap);
	}

	// meshes
	for (const BSPMesh& bspMesh : aMeshes) {
		Model* model = Model::alloc("bsp");
		aabb_s aabb = aabb_s(glm::vec3(std::numeric_limits<float>::max()), glm::vec3(std::numeric_limits<float>::min()));
		Mesh* mesh = Mesh::alloc();
		Material* material = nullptr;
		Texture* baseColorTexture = bspMesh.texture >= 0 ? baseColorTextures[bspMesh.texture] : nullptr;
		Texture* lightTexture = bspMesh.lightMap >= 0 ? lightTextures[bspMesh.lightMap] : nullptr;

		// check if material already exists
		for (Material* mat : materials) {
			if (mat->getBaseColorTexture() == baseColorTexture && mat->getLightTexture() == lightTexture) {
				material = mat;
			}
		}

		// create new material
		if (material == nullptr) {
			material = Material::alloc(DEFAULT_MATERIAL_NAME);
			material->setRoughnessFactor(1.0f);

			if (lightTexture != nullptr) {
				material->setLightTexture(lightTexture);
				material->setLightFactor({ 1, 1, 1 });
			}
			if (baseColorTexture != nullptr) {
				material->setBaseColorTexture(baseColorTexture);
				material->setBaseColorFactor({ 1, 1, 1, 1 });
			}

			materials.push_back(material);
		}

		mesh->setMaterial(material);

		mesh->setTopology(Mesh::Topology::TRIANGLE_LIST);
		std::vector<vertex_s> vertices;
		for (const BSPVertex& bspVertex : bspMesh.vertices) {
			vertex_s vertex = {};
			vertex.position = { bspVertex.position.x ,bspVertex.position.y ,bspVertex.position.z, 1 };
			vertex.normal = { bspVertex.normal.x ,bspVertex.normal.y ,bspVertex.normal.z, 1 };
			vertex.texture_coordinates_0 = bspVertex.texCoord;
			vertex.texture_coordinates_1 = bspVertex.lmCoord;
			vertex.color_0 = { bspVertex.color[0] / 255.0, bspVertex.color[1] / 255.0, bspVertex.color[2] / 255.0, bspVertex.color[3] / 255.0 };
			vertices.push_back(vertex);

			// min
			if (vertex.position.x < aabb.mMin.x) aabb.mMin.x = vertex.position.x;
			if (vertex.position.y < aabb.mMin.y) aabb.mMin.y = vertex.position.y;
			if (vertex.position.z < aabb.mMin.z) aabb.mMin.z = vertex.position.z;
			// max
			if (vertex.position.x > aabb.mMax.x) aabb.mMax.x = vertex.position.x;
			if (vertex.position.y > aabb.mMax.y) aabb.mMax.y = vertex.position.y;
			if (vertex.position.z > aabb.mMax.z) aabb.mMax.z = vertex.position.z;
		}

		mesh->setIndices(bspMesh.indices);
		mesh->setVertices(vertices);

		mesh->hasColors0(true);
		mesh->hasPositions(true);
		mesh->hasIndices(true);
		mesh->hasNormals(true);
		mesh->hasTexCoords0(true);
		mesh->hasTexCoords1(true);
		mesh->setAABB(aabb);

		model->addMesh(mesh);
		model->setAABB(aabb);

		models.push_back(model);
	}

	return { models, materials, textures, images };
}

static SceneInfo_s* createScene(const std::vector<BSPMesh>& aMeshes, const std::vector<BSPTexture>& aTextures, const std::vector<BSPLightMap>& aLightMaps) {
	SceneInfo_s* si = SceneInfo_s::alloc();

	// Model
	auto [models, materials, textures, images] = createModels(aMeshes, aTextures, aLightMaps);

	// SceneGraph
	spdlog::info("Loading SceneGraph:");
	SceneGraph* sceneGraph = SceneGraph::alloc("SceneGraph");

	Node* rootNode = Node::alloc("RootNode");
	rootNode->setTranslation({ -50, 0, 0 });
	rootNode->setRotation({ -sin(glm::radians(90.0) * 0.5), 0, 0, cos(glm::radians(90.0) * 0.5) });
	rootNode->setScale({ 0.04, 0.04, 0.04 });

	for (Model* model : models) {
		Node* modelNode = Node::alloc("ModelNode");
		modelNode->setModel(model);

		modelNode->setTranslation({ 0, 0, 0 });
		modelNode->setRotation({ 0, 0, 0, 0 });
		modelNode->setScale({ 1, 1, 1 });

		rootNode->addNode(modelNode);
	}

	sceneGraph->addRootNode(rootNode);

	// SceneInfo
	si->mSceneGraphs.push_back(sceneGraph);

	for (Model* model : models) {
		si->mModels.push_back(model);
	}

	for (Material* material : materials) {
		si->mMaterials.push_back(material);
	}

	for (Texture* texture : textures) {
		si->mTextures.push_back(texture);
	}

	for (Image* image : images) {
		si->mImages.push_back(image);
	}

	return si;
}

SceneInfo_s* Importer::load_bsp(std::string const& aFile) {
	spdlog::info("...load bsp");

	std::vector<BSPMesh> meshes;
	std::vector<BSPTexture> textures;
	std::vector<BSPLightMap> lightMaps;

	std::string folder = getFolder(aFile);

	std::ifstream in(aFile, std::ios::in | std::ios::binary);
	if (!in) {
		spdlog::critical("...failed");
		return nullptr;
	}

	in.seekg(0);
	BSPHeader header = {};
	in.read(reinterpret_cast<char*>(&header), sizeof(BSPHeader));

	if (std::string(header.magic, 4) != "IBSP") {
		spdlog::critical("Invalid file '{}'", aFile);
		return nullptr;
	}

	if (header.version != 0x2E) {
		spdlog::critical("File version not supported: version:{}", header.version);
		return nullptr;
	}

	constexpr uint32_t TEXTURES = 1;
	constexpr uint32_t VERTEX = 10;
	constexpr uint32_t MESHVERTEX = 11;
	constexpr uint32_t FACE = 13;
	constexpr uint32_t LIGHTMAP = 14;

	std::vector<BSPRawFace> rawFaces = loadFaces(in, header.lumps[FACE].offset, header.lumps[FACE].size);
	std::vector<BSPVertex> rawVertices = loadVertices(in, header.lumps[VERTEX].offset, header.lumps[VERTEX].size);
	std::vector<BSPRawMeshvert> rawMeshverts = loadMeshverts(in, header.lumps[MESHVERTEX].offset, header.lumps[MESHVERTEX].size);
	std::vector<BSPRawTexture> rawTextures = loadTextures(in, header.lumps[TEXTURES].offset, header.lumps[TEXTURES].size);


	// faces (polygon / mesh / bezier patches) -------------------------------------------------------
	for (const BSPRawFace& rawFace : rawFaces) {

		// Type: Polygon & Mesh
		if (rawFace.type == 1 || rawFace.type == 3) {
			const int vertexOffset = rawFace.vertexOffset;
			const int vertexCount = rawFace.vertexCount;

			const int meshVertexOffset = rawFace.meshVertexOffset;
			const int meshVertexCount = rawFace.meshVertexCount;

			auto mesh = BSPMesh();

			mesh.effect = rawFace.effect;
			mesh.texture = rawFace.shader;
			mesh.lightMap = rawFace.lightMap;

			// vertices
			for (int i = 0; i < vertexCount; i++) {
				mesh.vertices.push_back(rawVertices[vertexOffset + i]);
			}

			// indices
			for (int i = 0; i < meshVertexCount; i += 3) {
				mesh.indices.push_back(rawMeshverts[meshVertexOffset + i].offset);
				mesh.indices.push_back(rawMeshverts[meshVertexOffset + i + 2].offset);
				mesh.indices.push_back(rawMeshverts[meshVertexOffset + i + 1].offset);
			}

			meshes.push_back(mesh);
		}

		// Type: Patch
		if (rawFace.type == 2) {
			auto mesh = BSPMesh();
			mesh.effect = rawFace.effect;
			mesh.texture = rawFace.shader;
			mesh.lightMap = rawFace.lightMap;

			int dimX = (rawFace.size[0] - 1) / 2;
			int dimY = (rawFace.size[1] - 1) / 2;

			for (int x = 0, n = 0; n < dimX; n++, x = 2 * n)
			{
				for (int y = 0, m = 0; m < dimY; m++, y = 2 * m)
				{
					int controlOffset = rawFace.vertexOffset + x + rawFace.size[0] * y;
					tesselate(
						rawVertices[controlOffset + rawFace.size[0] * 0 + 0],
						rawVertices[controlOffset + rawFace.size[0] * 0 + 1],
						rawVertices[controlOffset + rawFace.size[0] * 0 + 2],

						rawVertices[controlOffset + rawFace.size[0] * 1 + 0],
						rawVertices[controlOffset + rawFace.size[0] * 1 + 1],
						rawVertices[controlOffset + rawFace.size[0] * 1 + 2],

						rawVertices[controlOffset + rawFace.size[0] * 2 + 0],
						rawVertices[controlOffset + rawFace.size[0] * 2 + 1],
						rawVertices[controlOffset + rawFace.size[0] * 2 + 2],
						mesh.vertices,
						mesh.indices,
						3
					);
				}
			}
			meshes.push_back(mesh);
		}

		// Type: Billboard
		if (rawFace.type == 4) {
		}
	}

	// textures --------------------------------------------------------------------------------------
	for (const BSPRawTexture& rawTexture : rawTextures) {

		BSPTexture texture;
		texture.render = true;
		texture.transparent = false;
		texture.solid = true;
		texture.name = std::string(rawTexture.name);

		if (rawTexture.surface & SURF_NONSOLID)
			texture.solid = false;
		if (rawTexture.contents & CONTENTS_PLAYERCLIP)
			texture.solid = true;
		if (rawTexture.contents & CONTENTS_TRANSLUCENT)
			texture.transparent = true;
		if (rawTexture.contents & CONTENTS_LAVA)
			texture.render = false;
		if (rawTexture.contents & CONTENTS_SLIME)
			texture.render = false;
		if (rawTexture.contents & CONTENTS_WATER)
			texture.render = false;
		if (rawTexture.contents & CONTENTS_FOG)
			texture.render = false;
		if (texture.name == "noshader")
			texture.render = false;

		if (exists(folder + "/" + texture.name + ".jpg")) {
			texture.path = folder + "/" + texture.name + ".jpg";
		}
		else if (exists(folder + "/" + texture.name + ".tga")) {
			texture.path = folder + "/" + texture.name + ".tga";
		}

		textures.push_back(texture);
	}

	// lightMaps -------------------------------------------------------------------------------------
	lightMaps = loadLightMaps(in, header.lumps[LIGHTMAP].offset, header.lumps[LIGHTMAP].size);

	{
		uint64_t meshCount = meshes.size();
		uint64_t vertexCount = 0;
		for (const auto& mesh : meshes) {
			vertexCount += mesh.vertices.size();
		}
		uint64_t textureCount = textures.size();
		uint64_t lightMapCount = lightMaps.size();

		spdlog::debug("meshes:    {}", meshCount);
		spdlog::debug("vertices:  {}", vertexCount);
		spdlog::debug("textures:  {}", textureCount);
		spdlog::debug("lightMaps: {}", lightMapCount);
	}

	// create SceneInfo
	SceneInfo_s* si = createScene(meshes, textures, lightMaps);

	spdlog::info("...success");

	return si;
}
