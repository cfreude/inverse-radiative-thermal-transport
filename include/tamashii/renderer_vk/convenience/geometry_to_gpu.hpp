#pragma once
#include <tamashii/engine/scene/render_scene.hpp>
#include <rvk/rvk.hpp>

T_BEGIN_NAMESPACE
class Mesh;
class Model;
class MaterialDataVulkan;
class GeometryDataVulkan {
public:
															GeometryDataVulkan(rvk::LogicalDevice* aDevice);
															~GeometryDataVulkan();

	void													prepare(uint32_t aMaxIndices = 2097152,
	                                                                uint32_t aMaxVertices = 524288,
	                                                                uint32_t aIndexBufferUsageFlags = rvk::Buffer::Use::INDEX,
	                                                                uint32_t aVertexBufferUsageFlags = rvk::Buffer::Use::VERTEX);
	void													destroy();

	void													loadScene(rvk::SingleTimeCommand* aStc, tamashii::scene_s aScene);
	void													update(rvk::SingleTimeCommand* aStc, tamashii::scene_s aScene);
	void													unloadScene();

	struct primitveBufferOffset_s {
		uint32_t											mIndexOffset = 0;
		uint32_t											mIndexByteOffset = 0;
		uint32_t											mVertexOffset = 0;
		uint32_t											mVertexByteOffset = 0;
	};

	rvk::Buffer*											getIndexBuffer();
	rvk::Buffer*											getVertexBuffer();
															// get total/mesh/model offsets for vertex/index buffer
	primitveBufferOffset_s									getOffset() const;
	primitveBufferOffset_s									getOffset(Mesh *aMesh);
	primitveBufferOffset_s									getOffset(Model *aModel);

	struct SceneInfo_s {
		uint32_t											mIndexCount;
		uint32_t											mVertexCount;
		uint32_t											mMeshCount;
		uint32_t											mInstanceCount;
		uint32_t											mGeometryCount;
	};
	static SceneInfo_s										getSceneGeometryInfo(tamashii::scene_s aScene);

protected:
	rvk::LogicalDevice*										mDevice;
	uint32_t												mStages;

	uint32_t												mMaxIndexCount;
	uint32_t												mMaxVertexCount;

	rvk::Buffer												mIndexBuffer;
	rvk::Buffer												mVertexBuffer;
	primitveBufferOffset_s									mBufferOffset;			// offset 
	std::unordered_map<Model*, primitveBufferOffset_s>		mModelToBOffset;		// offset for this model in indexBuffer and vertexBuffer
	std::unordered_map<Mesh*, primitveBufferOffset_s>		mMeshToBOffset;			// offset for this mesh in indexBuffer and vertexBuffer
};

class GeometryDataBlasVulkan : public GeometryDataVulkan {
public:
															GeometryDataBlasVulkan(rvk::LogicalDevice* aDevice);
															~GeometryDataBlasVulkan();

															// stages = rvk::Shader::Stage
	void													prepare(uint32_t aMaxIndices = 2097152,
																	uint32_t aMaxVertices = 524288,
																	uint32_t aIndexBufferUsageFlags = rvk::Buffer::Use::STORAGE | rvk::Buffer::Use::AS_INPUT,
																	uint32_t aVertexBufferUsageFlags = rvk::Buffer::Use::STORAGE | rvk::Buffer::Use::AS_INPUT);
	void													destroy();

	void													loadScene(rvk::SingleTimeCommand* aStc, tamashii::scene_s aScene);
	void													unloadScene();

	int														getGeometryIndex(Mesh *aMesh);

	rvk::BottomLevelAS*										getBlas(Model *aModel);

private:
	rvk::Buffer												mAsBuffer;
	std::vector<rvk::BottomLevelAS*>						mBottomAs;
	std::unordered_map<Model*, rvk::BottomLevelAS*>			mModelToBlas;
	std::unordered_map<Mesh*, uint32_t>						mMeshToGeometryIndex; // geometry index in the blas
};

class GeometryDataTlasVulkan {
public:
															GeometryDataTlasVulkan(rvk::LogicalDevice* aDevice);
															~GeometryDataTlasVulkan();

	void													prepare(uint32_t aInstanceCount, uint32_t aGeometryDataBufferFlags = 0, uint32_t aGeometryDataCount = 0);
	void													destroy();

	void													loadScene(rvk::SingleTimeCommand* aStc, tamashii::scene_s aScene, GeometryDataBlasVulkan* blas_gpu, MaterialDataVulkan* md_gpu = nullptr);
	void													unloadScene();
	void													update(const rvk::CommandBuffer* aCmdBuffer, rvk::SingleTimeCommand* aStc, tamashii::scene_s aScene, GeometryDataBlasVulkan* blas_gpu, MaterialDataVulkan* md_gpu = nullptr);

															// if already added, replace
	void													add(RefModel_s *aRefModel, const rvk::BottomLevelAS *aBlas);
	void													build(const rvk::CommandBuffer* aCmdBuffer);
	uint32_t												size() const;
	void													clear();

	rvk::TopLevelAS*										getTlas();
	rvk::Buffer*											getGeometryDataBuffer();
	int														getInstanceIndex(RefModel_s* aRefModel);
	int														getCustomIndex(RefModel_s* aRefModel);
	int														getGeometryOffset(RefModel_s* aRefModel);

	// iterate over all added instances
	std::deque<RefModel_s*>::iterator						begin();
	std::deque<RefModel_s*>::const_iterator					begin() const;
	std::deque<RefModel_s*>::iterator						end();
	std::deque<RefModel_s*>::const_iterator					end() const;
private:
	rvk::LogicalDevice*										mDevice;
	/*!!! change this struct only in combination with the one in shader/convenience/as_data.glsl/hlsl !!!*/
	struct GeometryData_s {
		glm::mat4 mModelMatrix;
		uint32_t mIndexBufferOffset;
		uint32_t mVertexBufferOffset;
		uint32_t mHasIndices;
		uint32_t mMaterialIndex;
	};

	uint32_t												mMaxInstanceCount;
	uint32_t												mGeometryOffset;
	std::unordered_map<RefModel_s*, uint32_t>				mRefModelToInstanceIndex;
	std::unordered_map<RefModel_s*, uint32_t>				mRefModelToCustomIndex;
	std::unordered_map<RefModel_s*, uint32_t>				mRefModelToGeometryOffset;
	rvk::TopLevelAS											mTlas;
	std::deque<RefModel_s*>									mModels;

	uint32_t												mMaxGeometryDataCount;
	std::vector<GeometryData_s>								mGeometryData;
	rvk::Buffer												mGeometryDataBuffer;
};
T_END_NAMESPACE