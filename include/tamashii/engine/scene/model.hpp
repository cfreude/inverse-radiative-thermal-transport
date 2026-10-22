#pragma once
#include <tamashii/public.hpp>
#include <tamashii/engine/scene/asset.hpp>

#include <string>
#include <list>
#include <vector>

T_BEGIN_NAMESPACE
class Material;
struct vertex_s {
	glm::vec4	position;
	glm::vec4	normal;						// default flat normals
	glm::vec4	tangent;					// tangents from default MikkTSpace algorithms
											// bitangent = cross(normal, tangent.xyz) * tangent.w
	glm::vec2	texture_coordinates_0;
	glm::vec2	texture_coordinates_1;
	glm::vec4	color_0;					// default (1,1,1,1)
	//glm::vec4	joints_0;
	//glm::vec4	weights_0;
};

struct aabb_s {
                                            aabb_s(const glm::vec3 aMin = glm::vec3(std::numeric_limits<float>::max()),
                                                   const glm::vec3 aMax = glm::vec3(std::numeric_limits<float>::min())) : mMin(aMin), mMax(aMax) {}
	glm::vec3								mMin;
	glm::vec3								mMax;

	bool									intersect(glm::vec3 aOrigin, glm::vec3 aDirection, float& aT) const;
	bool									inside(glm::vec3 aPoint) const;
	void									set(aabb_s aAabb);
	void									set(glm::vec3 aVec);
	aabb_s									transform(const glm::mat4& aMatrix) const;
	aabb_s									merge(const aabb_s& aAabb) const;
											// 0 till 7, get all points
	glm::vec3								getPoint(uint8_t aIdx) const;
};
struct triangle_s {
	glm::vec3								mVert[3];
	glm::vec3								mN[3];
	glm::vec3								mT[3];
	glm::vec2								mUV0[3];
	glm::vec3								mGeoN;

	bool									intersect(glm::vec3 aOrigin, glm::vec3 aDirection, float& aT, glm::vec2& aBarycentric, uint32_t aCull = 0) const;
};
struct sphere_s {
	glm::vec3								mCenter;
	float									mRadius;

	bool									intersect(glm::vec3 aOrigin, glm::vec3 aDirection, float& aT) const;
};
struct disk_s {
	glm::vec3								mCenter;
	float									mRadius;
	glm::vec3								mNormal;

	bool									intersect(glm::vec3 aOrigin, glm::vec3 aDirection, float& aT) const;
};

class Mesh : public Asset {
public:
	enum class Topology {
		UNKNOWN,
		POINT_LIST,
		LINE_LIST,
		LINE_STRIP,
		TRIANGLE_LIST,
		TRIANGLE_STRIP,
		TRIANGLE_FAN
	};
	class CustomData
	{
	public:
		CustomData();
		CustomData(size_t aBytes);
		CustomData(size_t aBytes, const void* aData);

		template <typename T>
		T* alloc(const size_t aCount = 1)
		{
			if (mData) free();
			mByteSize = sizeof(T) * aCount;
			mData = new uint8_t[mByteSize];
			return (T*)mData;
		}
		template <typename T = void>
		T* data() const
		{
			return (T*)mData;
		}
		size_t bytes() const;
		void free();
	private:
		size_t								mByteSize;
		uint8_t*							mData;
	};

											Mesh(const std::string& aName = "");

											~Mesh();
											Mesh(const Mesh& aMesh);

	static Mesh*							alloc();

	// get data
	Topology								getTopology() const;
	uint32_t*								getIndicesArray();
	std::vector<uint32_t>*					getIndicesVector();
	std::vector<uint32_t>&					getIndicesVectorRef();
	vertex_s*								getVerticesArray();
	std::vector<vertex_s>*					getVerticesVector();
	std::vector<vertex_s>&					getVerticesVectorRef();
	CustomData*								addCustomData(const std::string& aKey);
	CustomData*								getCustomData(const std::string& aKey);
	void									deleteCustomData(const std::string& aKey);
	std::map<std::string, CustomData>&		getCustomDataMapRef();
	const std::map<std::string, CustomData>&getCustomDataMapConstRef() const;
	size_t									getIndexCount() const;
	size_t									getVertexCount() const;
	size_t									getPrimitiveCount() const;
	Material*								getMaterial() const;
	const aabb_s &							getAABB() const;
	triangle_s								getTriangle(uint32_t aIndex, const glm::mat4* aModelMatrix = nullptr) const;


	bool									hasIndices() const;
	bool									hasPositions() const;
	bool									hasNormals() const;
	bool									hasTangents() const;
	bool									hasTexCoords0() const;
	bool									hasTexCoords1() const;
	bool									hasColors0() const;

	// set data
	void									setTopology(Topology aTopology );
	void									setIndices(const std::vector<uint32_t>& aIndices);
	void									setVertices(const std::vector<vertex_s>& aVertices);
	void									setMaterial(Material* aMaterial);
	void									setAABB(const aabb_s& aAabb);

	// set info
	void									hasIndices(bool aBool);
	void									hasPositions(bool aBool);
	void									hasNormals(bool aBool);
	void									hasTangents(bool aBool);
	void									hasTexCoords0(bool aBool);
	void									hasTexCoords1(bool aBool);
	void									hasColors0(bool aBool);

	void									clear();

private:
	Topology								mTopology;

	bool									mHasIndices;
	bool									mHasPositions;
	bool									mHasNormals;
	bool									mHasTangents;
	bool									mHasTextureCoordinates0;
	bool									mHasTextureCoordinates1;
	bool									mHasColors0;

	std::vector<uint32_t>					mIndices;
	std::vector<vertex_s>					mVertices;
	std::map<std::string, CustomData>		mCustomData;

											// in object space
	aabb_s									mAabb;
	
	Material*								mMaterial;
};

class Model : public Asset {
public:
											Model(const std::string& aName = "");
											~Model();
											Model(const Model& aModel);


	static Model*							alloc(const std::string& aName = "");

	void									addMesh(Mesh* aMesh);

	const aabb_s&							getAABB() const;
	void									setAABB(const aabb_s& aAabb);

	std::list<Mesh*>						getMeshList() const;
	uint32_t								size() const;

	void									clear();

	// iterator
	std::list<Mesh*>::iterator				begin();
	std::list<Mesh*>::const_iterator		begin() const;
	std::list<Mesh*>::iterator				end();
	std::list<Mesh*>::const_iterator		end() const;
private:
	std::list<Mesh*>						mMeshes;
	aabb_s									mAABB; // in object space
};
T_END_NAMESPACE
