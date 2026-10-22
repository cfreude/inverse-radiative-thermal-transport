#include "tamashii/engine/scene/model.hpp"

T_USE_NAMESPACE

bool aabb_s::intersect(const glm::vec3 aOrigin, const glm::vec3 aDirection, float& aT) const {
	// r.dir is unit direction vector of ray
	const glm::vec3 dirfrac = glm::vec3(1.0f) / aDirection;
	// lb is the corner of AABB with minimal coordinates - left bottom, rt is maximal corner
	// r.org is origin of ray
	const float t1 = (mMin.x - aOrigin.x) * dirfrac.x;
	const float t2 = (mMax.x - aOrigin.x) * dirfrac.x;
	const float t3 = (mMin.y - aOrigin.y) * dirfrac.y;
	const float t4 = (mMax.y - aOrigin.y) * dirfrac.y;
	const float t5 = (mMin.z - aOrigin.z) * dirfrac.z;
	const float t6 = (mMax.z - aOrigin.z) * dirfrac.z;

	const float tmin = std::max(std::max(std::min(t1, t2), std::min(t3, t4)), std::min(t5, t6));
	const float tmax = std::min(std::min(std::max(t1, t2), std::max(t3, t4)), std::max(t5, t6));

	// if tmax < 0, ray (line) is intersecting AABB, but the whole AABB is behind us
	if (tmax < 0)
	{
		aT = tmax;
		return false;
	}

	// if tmin > tmax, ray doesn't intersect AABB
	if (tmin > tmax)
	{
		aT = tmax;
		return false;
	}

	aT = tmin;
	return true;
}
bool aabb_s::inside(const glm::vec3 aPoint) const
{
	return (mMin.x <= aPoint.x && aPoint.x <= mMax.x
		&& mMin.y <= aPoint.y && aPoint.y <= mMax.y
		&& mMin.z <= aPoint.z && aPoint.z <= mMax.z);
}
void aabb_s::set(const aabb_s aAabb)
{
	mMin = glm::min(aAabb.mMin, mMin);
	mMax = glm::max(aAabb.mMax, mMax);
}

void aabb_s::set(const glm::vec3 aVec)
{
	mMin = glm::min(aVec, mMin);
	mMax = glm::max(aVec, mMax);
}

aabb_s aabb_s::transform(const glm::mat4& aMatrix) const
{
	aabb_s aabb = {};
	for (uint8_t i = 0; i < 8; i++)
	{
		glm::vec3 p = aMatrix * glm::vec4(getPoint(i), 1.0f);
		aabb.mMin = glm::min(aabb.mMin, p);
		aabb.mMax = glm::max(aabb.mMax, p);
	}
	return aabb;
}

aabb_s aabb_s::merge(const aabb_s& aAabb) const
{
	return { glm::min(mMin, aAabb.mMin), glm::max(mMax, aAabb.mMax) };
}

// https://dev.theomader.com/transform-bounding-boxes/
//aabb_s aabb_s::transform(const glm::mat4& aMatrix) const
//{
//	aabb_s aabb(glm::vec3(0), glm::vec3(0));
//	for (int i = 0; i < 3; i++)
//	{
//		glm::vec3 m = aMatrix[i] * mMin[i];
//		glm::vec3 n = aMatrix[i] * mMax[i];
//		aabb.mMin += glm::min(m, n);
//		aabb.mMax += glm::max(m, n);
//	}
//	aabb.mMin += glm::vec3(aMatrix[3]);
//	aabb.mMax += glm::vec3(aMatrix[3]);
//	return aabb;
//}

glm::vec3 aabb_s::getPoint(const uint8_t aIdx) const
{
	switch(aIdx) {
	case 0: return mMin;
	case 1: return {mMin.x, mMin.y, mMax.z};
	case 2: return {mMin.x, mMax.y, mMin.z};
	case 3: return {mMax.x, mMin.y, mMin.z};
	case 4: return {mMax.x, mMax.y, mMin.z};
	case 5: return {mMax.x, mMin.y, mMax.z};
	case 6: return {mMin.x, mMax.y, mMax.z};
	case 7: return mMax;
	default: return glm::vec3(0);
	}
}

// https://people.cs.clemson.edu/~dhouse/courses/404/notes/barycentric.pdf
bool triangle_s::intersect(glm::vec3 aOrigin, glm::vec3 aDirection, float& aT, glm::vec2& aBarycentric,
	uint32_t aCull) const
{
	// clockwise
	const glm::vec3 e1 = mVert[1] - mVert[0];
	const glm::vec3 e2 = mVert[2] - mVert[0];
	// counter clockwise
	//const glm::vec3 e1 = mVert[2] - mVert[0];
	//const glm::vec3 e2 = mVert[1] - mVert[0];

	const glm::vec3 h = glm::cross(aDirection, e2);
	const float a = glm::dot(e1, h);

	//if (a > -0.00001f && a < 0.00001f) return false;
	if (/*front*/(aCull == 1 && a > 0.0f) || /*back*/(aCull == 2 && a < 0.0f) || aCull == 3) return false;

	const float f = 1 / a;
	const glm::vec3 s = aOrigin - mVert[0];
	const float u = f * (glm::dot(s, h));

	if (u < 0.0f || u > 1.0f)
		return false;

	const glm::vec3 q = glm::cross(s, e1);
	const float v = f * glm::dot(aDirection, q);

	if (v < 0.0f || u + v > 1.0f)
		return false;

	// at this stage we can compute t to find out where
	// the intersection point is on the line
	aT = f * glm::dot(e2, q);

	// this means that there is a line intersection
	// but not a ray intersection
	if (aT <= 0.00001f) return false;

	// compute the intersection point using equation 1
	const glm::vec3 hitPos = aOrigin + aT * aDirection;
	const glm::vec3 normal = glm::cross(e1, e2); // N 
	const float denom = glm::dot(normal, normal);

	// Step 2: barycentric coordinates
	glm::vec3 C; // vector perpendicular to triangle's plane 

	C = glm::cross(mVert[1] - mVert[0], hitPos - mVert[0]);
	if (glm::dot(normal, C) < 0) return false; // P is on the right side 
	// u
	C = glm::cross(mVert[2] - mVert[1], hitPos - mVert[1]);
	if ((aBarycentric.x = glm::dot(normal, C)) < 0) return false; // P is on the right side 
	// v
	C = glm::cross(mVert[0] - mVert[2], hitPos - mVert[2]);
	if ((aBarycentric.y = glm::dot(normal, C)) < 0) return false; // P is on the right side; 

	aBarycentric /= denom;

	// test
	//glm::vec3 pos = mVert[0] * aBarycentric.x + mVert[1] * aBarycentric.y + mVert[2] * (1.0f - aBarycentric.x - aBarycentric.y);

	return true;
}

bool sphere_s::intersect(const glm::vec3 aOrigin, const glm::vec3 aDirection, float& aT) const
{
	// geometric solution
	const glm::vec3 L = mCenter - aOrigin;
	const float tca = glm::dot(L, aDirection);
	// if (tca < 0) return false;
	const float d2 = glm::dot(L, L) - tca * tca;
	const float radius2 = mRadius * mRadius;
	if (d2 > radius2) return false;
	const float thc = sqrt(radius2 - d2);
	// solutions for t if the ray intersects 
	float t0 = tca - thc;
	float t1 = tca + thc;

	if (t0 > t1) std::swap(t0, t1);

	if (t0 < 0) {
		t0 = t1; // if t0 is negative, let's use t1 instead 
		if (t0 < 0) return false; // both t0 and t1 are negative 
	}

	aT = t0;

	return true;
}

bool disk_s::intersect(const glm::vec3 aOrigin, const glm::vec3 aDirection, float& aT) const
{
	constexpr float epsilon = 1e-6;
	const float denom = glm::dot(mNormal, aDirection);
    if (fabs(denom) > epsilon)
	{
		aT = glm::dot((mCenter - aOrigin), mNormal) / denom;
		if (aT >= 0.0f) {
			const glm::vec3 point = aOrigin + aDirection * aT;
			const float d = glm::length(point - mCenter);
			if (d <= mRadius) return true;
		}
	}
	return false;
}

Mesh::CustomData::CustomData(): mByteSize(0), mData(nullptr) {}

Mesh::CustomData::CustomData(const size_t aBytes): mByteSize(aBytes), mData(new uint8_t[aBytes]) {}

Mesh::CustomData::CustomData(const size_t aBytes, const void* aData): mByteSize(aBytes), mData(new uint8_t[aBytes])
{ std::memcpy(mData, aData, aBytes); }

size_t Mesh::CustomData::bytes() const
{ return mByteSize; }

void Mesh::CustomData::free()
{
	delete[] mData;
	mByteSize = 0;
	mData = nullptr;
}

Mesh::Mesh(const std::string& aName) : Asset(Type::MESH, aName), mTopology(Topology::UNKNOWN), mHasIndices(false), mHasPositions(false), mHasNormals(false),
                                       mHasTangents(false), mHasTextureCoordinates0(false), mHasTextureCoordinates1(false), 
                                       mHasColors0(false), mMaterial(nullptr) {}

Mesh::~Mesh()
{
	for (auto& [id, ed] : mCustomData) ed.free();
}

Mesh::Mesh(const Mesh& aMesh) : Asset(aMesh.getAssetType(), aMesh.getName()), mTopology(aMesh.mTopology), mHasIndices(aMesh.mHasIndices), mHasPositions(aMesh.mHasPositions),
                                mHasNormals(aMesh.mHasNormals), mHasTangents(aMesh.mHasTangents),
                                mHasTextureCoordinates0(aMesh.mHasTextureCoordinates0),
                                mHasTextureCoordinates1(aMesh.mHasTextureCoordinates1), mHasColors0(aMesh.mHasColors0),
                                mIndices(aMesh.mIndices), mVertices(aMesh.mVertices), mAabb(aMesh.mAabb),
                                mMaterial(aMesh.mMaterial) {}

Mesh* Mesh::alloc()
{ return new Mesh(); }

Mesh::Topology Mesh::getTopology() const
{ return mTopology; }

uint32_t* Mesh::getIndicesArray()
{ return mIndices.data(); }

std::vector<uint32_t>* Mesh::getIndicesVector()
{ return &mIndices; }

std::vector<uint32_t>& Mesh::getIndicesVectorRef()
{ return mIndices; }

vertex_s* Mesh::getVerticesArray()
{ return mVertices.data(); }

std::vector<vertex_s>* Mesh::getVerticesVector()
{ return &mVertices; }

std::vector<vertex_s>& Mesh::getVerticesVectorRef()
{ return mVertices; }

Mesh::CustomData* Mesh::addCustomData(const std::string& aKey)
{
	if (getCustomData(aKey)) return nullptr;
	mCustomData.insert({ aKey, {} });
	return getCustomData(aKey);
}

Mesh::CustomData* Mesh::getCustomData(const std::string& aKey)
{
	if (mCustomData.find(aKey) != mCustomData.end()) return &mCustomData[aKey];
	return nullptr;
}

void Mesh::deleteCustomData(const std::string& aKey)
{
	mCustomData.erase(aKey);
}

std::map<std::string, Mesh::CustomData>& Mesh::getCustomDataMapRef()
{ return mCustomData; }

const std::map<std::string, Mesh::CustomData>& Mesh::getCustomDataMapConstRef() const
{ return mCustomData; }

size_t Mesh::getIndexCount() const
{ return mIndices.size(); }

size_t Mesh::getVertexCount() const
{ return mVertices.size(); }

size_t Mesh::getPrimitiveCount() const
{
	switch (mTopology) {
	case Topology::POINT_LIST:
		if (hasIndices()) return getIndexCount();
		return getVertexCount();
	case Topology::LINE_LIST:
	case Topology::LINE_STRIP:
		if (hasIndices()) return getIndexCount() / 2;
		return getVertexCount() / 2;
	case Topology::TRIANGLE_LIST:
	case Topology::TRIANGLE_STRIP:
	case Topology::TRIANGLE_FAN:
		if (hasIndices()) return getIndexCount() / 3;
		return getVertexCount() / 3;
	default:
		return 0;
	}
}

Material* Mesh::getMaterial() const
{ return mMaterial; }

const aabb_s& Mesh::getAABB() const
{ return mAabb; }

bool Mesh::hasIndices() const
{ return mHasIndices; }

bool Mesh::hasPositions() const
{ return mHasPositions; }

bool Mesh::hasNormals() const
{ return mHasNormals; }

bool Mesh::hasTangents() const
{ return mHasTangents; }

bool Mesh::hasTexCoords0() const
{ return mHasTextureCoordinates0; }

bool Mesh::hasTexCoords1() const
{ return mHasTextureCoordinates1; }

bool Mesh::hasColors0() const
{ return mHasColors0; }

void Mesh::setTopology(const Topology aTopology)
{ mTopology = aTopology; }

void Mesh::setIndices(const std::vector<uint32_t>& aIndices)
{ mIndices = aIndices; }

void Mesh::setVertices(const std::vector<vertex_s>& aVertices)
{ mVertices = aVertices; }

void Mesh::setMaterial(Material* aMaterial)
{ mMaterial = aMaterial; }

void Mesh::setAABB(const aabb_s& aAabb)
{ mAabb = aAabb; }

void Mesh::hasIndices(const bool aBool)
{ mHasIndices = aBool; }

void Mesh::hasPositions(const bool aBool)
{ mHasPositions = aBool; }

void Mesh::hasNormals(const bool aBool)
{ mHasNormals = aBool; }

void Mesh::hasTangents(const bool aBool)
{ mHasTangents = aBool; }

void Mesh::hasTexCoords0(const bool aBool)
{ mHasTextureCoordinates0 = aBool; }

void Mesh::hasTexCoords1(const bool aBool)
{ mHasTextureCoordinates1 = aBool; }

void Mesh::hasColors0(const bool aBool)
{ mHasColors0 = aBool; }

triangle_s Mesh::getTriangle(const uint32_t aIndex, const glm::mat4* aModelMatrix) const
{
	const uint32_t idx = aIndex * 3;
	triangle_s triangle = {};
	uint32_t idx0 = idx + 0, idx1 = idx + 1, idx2 = idx + 2;
	if (hasIndices()) {
		idx0 = mIndices[idx0];
		idx1 = mIndices[idx1];
		idx2 = mIndices[idx2];
	}
	if (aModelMatrix) {
		const glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(*aModelMatrix)));
		triangle.mVert[0] = (*aModelMatrix) * mVertices[idx0].position;
		triangle.mVert[1] = (*aModelMatrix) * mVertices[idx1].position;
		triangle.mVert[2] = (*aModelMatrix) * mVertices[idx2].position;
		triangle.mN[0] = normalMat * mVertices[idx0].normal;
		triangle.mN[1] = normalMat * mVertices[idx1].normal;
		triangle.mN[2] = normalMat * mVertices[idx2].normal;
		triangle.mT[0] = normalMat * mVertices[idx0].tangent;
		triangle.mT[1] = normalMat * mVertices[idx1].tangent;
		triangle.mT[2] = normalMat * mVertices[idx2].tangent;
	}
	else {
		triangle.mVert[0] = mVertices[idx0].position;
		triangle.mVert[1] = mVertices[idx1].position;
		triangle.mVert[2] = mVertices[idx2].position;
		triangle.mN[0] = mVertices[idx0].normal;
		triangle.mN[1] = mVertices[idx1].normal;
		triangle.mN[2] = mVertices[idx2].normal;
		triangle.mT[0] = mVertices[idx0].tangent;
		triangle.mT[1] = mVertices[idx1].tangent;
		triangle.mT[2] = mVertices[idx2].tangent;
	}
	triangle.mUV0[0] = mVertices[idx0].texture_coordinates_0;
	triangle.mUV0[1] = mVertices[idx1].texture_coordinates_0;
	triangle.mUV0[2] = mVertices[idx2].texture_coordinates_0;

	const glm::vec3 dir01 = triangle.mVert[1] - triangle.mVert[0];
	const glm::vec3 dir02 = triangle.mVert[2] - triangle.mVert[0];
	const glm::vec3 normal = glm::normalize(glm::cross(dir01, dir02));
	if (!glm::any(glm::isnan(normal))) triangle.mGeoN = normal;

	return triangle;
}

void Mesh::clear()
{
	mTopology = Topology::UNKNOWN;
	mIndices.clear();
	mVertices.clear();
	mHasIndices = false;
	mHasPositions = false;
	mHasNormals = false;
	mHasTangents = false;
	mHasTextureCoordinates0 = false;
	mHasTextureCoordinates1 = false;
	mHasColors0 = false;
	mAabb = {};
}

Model::Model(const std::string& aName) : Asset(Type::MODEL, aName), mAABB()
{ }

Model* Model::alloc(const std::string& aName)
{ return new Model(aName); }

const aabb_s& Model::getAABB() const
{ return mAABB; }

void Model::setAABB(const aabb_s& aAabb)
{ mAABB = aAabb; }

std::list<Mesh*> Model::getMeshList() const
{ return mMeshes; }

uint32_t Model::size() const
{ return static_cast<uint32_t>(mMeshes.size()); }

std::list<Mesh*>::iterator Model::begin()
{ return mMeshes.begin(); }

std::list<Mesh*>::const_iterator Model::begin() const
{ return mMeshes.begin(); }

std::list<Mesh*>::iterator Model::end()
{ return mMeshes.end(); }

std::list<Mesh*>::const_iterator Model::end() const
{ return mMeshes.end(); }

Model::~Model() 
{
	for (const Mesh* m : mMeshes) {
		delete m;
	}
}

Model::Model(const Model& aModel) : Asset(aModel.getAssetType(), aModel.getName()), mAABB(aModel.mAABB)
{
	for(const Mesh *mesh : aModel.getMeshList())
	{
		mMeshes.push_back(new Mesh(*mesh));
	}
}

void Model::addMesh(Mesh* aMesh)
{
	mMeshes.push_back(aMesh);
}

void Model::clear()
{
	for (Mesh* m : mMeshes) {
		m->clear();
		delete m;
	}
	mMeshes.clear();
}

