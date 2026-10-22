#include <tamashii/renderer_vk/convenience/geometry_to_gpu.hpp>
#include <tamashii/renderer_vk/convenience/material_to_gpu.hpp>
#include <tamashii/engine/scene/ref_entities.hpp>
#include <tamashii/engine/scene/model.hpp>
T_USE_NAMESPACE

GeometryDataVulkan::GeometryDataVulkan(rvk::LogicalDevice* aDevice) : mDevice(aDevice), mStages(rvk::Shader::Stage::ALL),
mMaxIndexCount(0), mMaxVertexCount(0), mIndexBuffer(aDevice), mVertexBuffer(aDevice)
{}

GeometryDataVulkan::~GeometryDataVulkan()
{ destroy(); }

void GeometryDataVulkan::prepare(const uint32_t aMaxIndices,
                               const uint32_t aMaxVertices,
                               const uint32_t aIndexBufferUsageFlags,
                               const uint32_t aVertexBufferUsageFlags)
{
	unloadScene();
	mMaxIndexCount = std::max(1u, aMaxIndices);
	mMaxVertexCount = std::max(1u, aMaxVertices);
	mIndexBuffer.create(aIndexBufferUsageFlags, mMaxIndexCount * sizeof(uint32_t), rvk::Buffer::Location::DEVICE);
	mVertexBuffer.create(aVertexBufferUsageFlags, mMaxVertexCount * sizeof(vertex_s), rvk::Buffer::Location::DEVICE);
}
void GeometryDataVulkan::destroy()
{
	mIndexBuffer.destroy();
	mVertexBuffer.destroy();
	mMaxIndexCount = 0;
	mMaxVertexCount = 0;
	unloadScene();
}
void GeometryDataVulkan::loadScene(rvk::SingleTimeCommand* aStc, const tamashii::scene_s aScene)
{
	unloadScene();
	// upload geometrie data to the index/vertex buffers
	{
		// count
		uint32_t mcount = 0;
		uint32_t icount = 0;
		uint32_t vcount = 0;
		for (Model *model : aScene.models) {
			for (const Mesh *mesh : *model) {
				mcount++;
				if (mesh->hasIndices()) icount += mesh->getIndexCount();
				vcount += mesh->getVertexCount();
			}
		}
		mModelToBOffset.reserve(aScene.models.size());
		mMeshToBOffset.reserve(mcount);
		if (vcount) {
			// upload
			primitveBufferOffset_s offsets = {};
			for (Model *model : aScene.models) {
				mModelToBOffset.insert(std::pair(model, offsets));
				for (Mesh *mesh : *model) {
					mMeshToBOffset.insert(std::pair(mesh, offsets));
					// indices
					if (mesh->hasIndices()) {
						mIndexBuffer.STC_UploadData(aStc, mesh->getIndicesArray(), mesh->getIndexCount() * sizeof(uint32_t), offsets.mIndexByteOffset);
						offsets.mIndexOffset += mesh->getIndexCount();
						offsets.mIndexByteOffset += mesh->getIndexCount() * sizeof(uint32_t);
						if (offsets.mIndexOffset > mMaxIndexCount) spdlog::error("Indices count > buffer size");
					}
					// vertices
					mVertexBuffer.STC_UploadData(aStc, mesh->getVerticesArray(), mesh->getVertexCount() * sizeof(vertex_s), offsets.mVertexByteOffset);
					offsets.mVertexOffset += mesh->getVertexCount();
					offsets.mVertexByteOffset += mesh->getVertexCount() * sizeof(vertex_s);
					if (offsets.mVertexOffset > mMaxVertexCount) spdlog::error("Vertices count > buffer size");
				}
			}
			mBufferOffset = offsets;
		}
	}
}

void GeometryDataVulkan::update(rvk::SingleTimeCommand* aStc, const scene_s aScene)
{
	unloadScene();
	loadScene(aStc, aScene);
}

void GeometryDataVulkan::unloadScene()
{
	mModelToBOffset.clear();
	mMeshToBOffset.clear();
	mBufferOffset = {};
}

rvk::Buffer* GeometryDataVulkan::getIndexBuffer()
{ return &mIndexBuffer; }

rvk::Buffer* GeometryDataVulkan::getVertexBuffer()
{ return &mVertexBuffer; }

GeometryDataVulkan::primitveBufferOffset_s GeometryDataVulkan::getOffset() const
{ return mBufferOffset; }

GeometryDataVulkan::primitveBufferOffset_s GeometryDataVulkan::getOffset(Mesh* aMesh)
{ return mMeshToBOffset[aMesh]; }

GeometryDataVulkan::primitveBufferOffset_s GeometryDataVulkan::getOffset(Model* aModel)
{ return mModelToBOffset[aModel]; }

GeometryDataVulkan::SceneInfo_s GeometryDataVulkan::getSceneGeometryInfo(const tamashii::scene_s aScene)
{
	SceneInfo_s info{};
	for (Model *model : aScene.models) {
		for (const Mesh *mesh : *model) {
			info.mMeshCount++;
			if (mesh->hasIndices()) info.mIndexCount += mesh->getIndexCount();
			info.mVertexCount += mesh->getVertexCount();
		}
	}
	info.mInstanceCount = static_cast<uint32_t>(aScene.refModels.size());

	for (const RefModel_s* refModel : aScene.refModels) {
		info.mGeometryCount += refModel->refMeshes.size();
	}
	return info;
}

GeometryDataBlasVulkan::GeometryDataBlasVulkan(rvk::LogicalDevice* aDevice) : GeometryDataVulkan(aDevice), mAsBuffer(aDevice) {}

GeometryDataBlasVulkan::~GeometryDataBlasVulkan()
{ destroy(); }

void GeometryDataBlasVulkan::prepare(const uint32_t aMaxIndices, 
                                   const uint32_t aMaxVertices, 
                                   const uint32_t aIndexBufferUsageFlags, 
                                   const uint32_t aVertexBufferUsageFlags)
{
	GeometryDataVulkan::prepare(aMaxIndices, aMaxVertices, aIndexBufferUsageFlags, aVertexBufferUsageFlags);
}

void GeometryDataBlasVulkan::destroy()
{
	unloadScene();
	GeometryDataVulkan::destroy();
}

void GeometryDataBlasVulkan::loadScene(rvk::SingleTimeCommand* aStc, const tamashii::scene_s aScene)
{
	unloadScene();
	GeometryDataVulkan::loadScene(aStc, aScene);

	uint32_t geometry_count = 0;
	for (const Model *model : aScene.models) {
		geometry_count += model->getMeshList().size();
	}
    if(!geometry_count) return;

	mBottomAs.reserve(aScene.models.size());
	mModelToBlas.reserve(aScene.models.size());
	mMeshToGeometryIndex.reserve(geometry_count);

	uint32_t scratch_buffer_size = 0;
	uint32_t as_buffer_size = 0;
	uint32_t geometry_index = 0;
	// build bottom as
	for (Model *model : aScene.models) {
		// create one blas for each model and reserve memory for its geometries
		auto blas = new rvk::BottomLevelAS(mDevice);
		blas->reserve(static_cast<uint32_t>(model->getMeshList().size()));
		geometry_index = 0;
		for (Mesh *mesh : *model) {
			mMeshToGeometryIndex.insert(std::pair(mesh, geometry_index));
			geometry_index++;

			// add this geometry/mesh to the bottom as of the current model
			rvk::ASTriangleGeometry astri;
			if (mesh->hasIndices()) astri.setIndicesFromDevice(VK_INDEX_TYPE_UINT32, static_cast<uint32_t>(mesh->getIndexCount()), &mIndexBuffer, mMeshToBOffset[mesh].mIndexByteOffset);
			astri.setVerticesFromDevice(VK_FORMAT_R32G32B32_SFLOAT, sizeof(vertex_s), static_cast<uint32_t>(mesh->getVertexCount()), &mVertexBuffer, mMeshToBOffset[mesh].mVertexByteOffset);

			blas->addGeometry(astri, mesh->getMaterial()->getBlendMode() == Material::BlendMode::_OPAQUE ? VK_GEOMETRY_OPAQUE_BIT_KHR : 0);
		}
		// build the bottom level acceleration structure
		blas->preprepare(VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
		mBottomAs.push_back(blas);
		mModelToBlas.insert(std::pair(model, blas));

		as_buffer_size += rountUpToMultipleOf<uint32_t>(blas->getASSize(), 256);
		scratch_buffer_size += blas->getBuildScratchSize();
	}

	mAsBuffer.create(rvk::Buffer::AS_STORE, as_buffer_size, rvk::Buffer::Location::DEVICE);
	rvk::Buffer scratchBuffer(mDevice);
	scratchBuffer.create(rvk::Buffer::AS_SCRATCH, scratch_buffer_size, rvk::Buffer::Location::DEVICE);

	uint32_t scratchBufferOffset = 0;
	uint32_t asBufferOffset = 0;
	aStc->begin();
	for (Model *model : aScene.models) {
		mModelToBlas[model]->setASBuffer(&mAsBuffer, asBufferOffset);
		mModelToBlas[model]->setScratchBuffer(&scratchBuffer, scratchBufferOffset);
		mModelToBlas[model]->prepare(VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
		mModelToBlas[model]->CMD_Build(aStc->buffer());
		asBufferOffset += rountUpToMultipleOf<uint32_t>(mModelToBlas[model]->getASSize(), 256);
		scratchBufferOffset += mModelToBlas[model]->getBuildScratchSize();
	}
	aStc->end();

	scratchBuffer.destroy();
}

void GeometryDataBlasVulkan::unloadScene()
{
	for (const rvk::BottomLevelAS *blas : mBottomAs) {
		delete blas;
	}
	mBottomAs.clear();
	mModelToBlas.clear();
	mMeshToGeometryIndex.clear();
	mAsBuffer.destroy();
}

int GeometryDataBlasVulkan::getGeometryIndex(Mesh *aMesh)
{
	if (aMesh == nullptr || (mMeshToGeometryIndex.find(aMesh) == mMeshToGeometryIndex.end())) return -1;
	return static_cast<int>(mMeshToGeometryIndex[aMesh]);
}

rvk::BottomLevelAS* GeometryDataBlasVulkan::getBlas(Model *aModel)
{
	if (aModel == nullptr || (mModelToBlas.find(aModel) == mModelToBlas.end())) return nullptr;
	return mModelToBlas[aModel];
}

GeometryDataTlasVulkan::GeometryDataTlasVulkan(rvk::LogicalDevice* aDevice) : mDevice(aDevice),
	mMaxInstanceCount(0), mGeometryOffset(0), mTlas(aDevice), mMaxGeometryDataCount(0),
    mGeometryDataBuffer(aDevice)
{
}

GeometryDataTlasVulkan::~GeometryDataTlasVulkan()
{ destroy(); }

void GeometryDataTlasVulkan::prepare(const uint32_t aInstanceCount, const uint32_t aGeometryDataBufferFlags, const uint32_t aGeometryDataCount)
{
	mRefModelToInstanceIndex.reserve(aGeometryDataCount);
	mRefModelToGeometryOffset.reserve(aGeometryDataCount);
	mRefModelToCustomIndex.reserve(aGeometryDataCount);
	mTlas.reserve(aInstanceCount);
	mGeometryOffset = 0;
	mMaxInstanceCount = aInstanceCount;

	if (aGeometryDataCount) {
		mGeometryData.reserve(aGeometryDataCount);
		mGeometryDataBuffer.create(aGeometryDataBufferFlags, aGeometryDataCount * sizeof(GeometryData_s), rvk::Buffer::Location::DEVICE);
		mMaxGeometryDataCount = aGeometryDataCount;
	}
}


void GeometryDataTlasVulkan::destroy()
{
	unloadScene();
	mTlas.destroy();
	mMaxInstanceCount = 0;

	mMaxGeometryDataCount = 0;
	mGeometryDataBuffer.destroy();
}


void GeometryDataTlasVulkan::loadScene(rvk::SingleTimeCommand* aStc, const tamashii::scene_s aScene, GeometryDataBlasVulkan* blas_gpu, MaterialDataVulkan* md_gpu)
{
	unloadScene();
	if (aScene.refModels.empty()) return;
	for (RefModel_s* refModel : aScene.refModels) {
		add(refModel, blas_gpu->getBlas(refModel->model));
		if (mMaxGeometryDataCount && md_gpu) {
			GeometryData_s data{};
			data.mModelMatrix = refModel->model_matrix;
			for (const RefMesh_s* refMesh : refModel->refMeshes) {
				const GeometryDataVulkan::primitveBufferOffset_s offset = blas_gpu->getOffset(refMesh->mesh);
				data.mIndexBufferOffset = offset.mIndexOffset;
				data.mVertexBufferOffset = offset.mVertexOffset;
				data.mHasIndices = refMesh->mesh->hasIndices();
				data.mMaterialIndex = md_gpu->getIndex(refMesh->mesh->getMaterial());
				mGeometryData.push_back(data);
			}
		}
	}
	if(!mGeometryData.empty()) mGeometryDataBuffer.STC_UploadData(aStc, mGeometryData.data(), mGeometryData.size() * sizeof(GeometryData_s));

	aStc->begin();
	mTlas.prepare(VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR);
	mTlas.CMD_Build(aStc->buffer(), nullptr);
	aStc->end();
}

void GeometryDataTlasVulkan::unloadScene()
{
	clear();
	mTlas.destroy();
}

void GeometryDataTlasVulkan::update(const rvk::CommandBuffer* aCmdBuffer, rvk::SingleTimeCommand* aStc, const tamashii::scene_s aScene, GeometryDataBlasVulkan* blas_gpu, MaterialDataVulkan* md_gpu)
{
	clear();
	if (aScene.refModels.empty()) return;
	for (RefModel_s* refModel : aScene.refModels) {
		add(refModel, blas_gpu->getBlas(refModel->model));
		if (mMaxGeometryDataCount && md_gpu) {
			GeometryData_s data{};
			data.mModelMatrix = refModel->model_matrix;
			for (const RefMesh_s* refMesh : refModel->refMeshes) {
				const GeometryDataVulkan::primitveBufferOffset_s offset = blas_gpu->getOffset(refMesh->mesh);
				data.mIndexBufferOffset = offset.mIndexOffset;
				data.mVertexBufferOffset = offset.mVertexOffset;
				data.mHasIndices = refMesh->mesh->hasIndices();
				data.mMaterialIndex = md_gpu->getIndex(refMesh->mesh->getMaterial());
				mGeometryData.push_back(data);
				if (mGeometryData.size() > mMaxGeometryDataCount) spdlog::error("Geometry count > buffer size");
			}
		}
	}
	if (!mGeometryData.empty()) mGeometryDataBuffer.STC_UploadData(aStc, mGeometryData.data(), mGeometryData.size() * sizeof(GeometryData_s));

	mTlas.prepare(VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR);
	mTlas.CMD_Build(aCmdBuffer, nullptr);
}

void GeometryDataTlasVulkan::add(RefModel_s* aRefModel, const rvk::BottomLevelAS* aBlas)
{
	mModels.push_back(aRefModel);
	// glm stores matrices in a column-major order but vulkan acceleration structures requiere a row-major order
	glm::mat4 modelMatrix = glm::transpose(aRefModel->model_matrix);
	// create an instance for the blas and add it to the tlas
	rvk::ASInstance asInstance(aBlas);
	asInstance.setTransform(&modelMatrix[0][0]);
	asInstance.setCustomIndex(static_cast<uint32_t>(mGeometryData.size()));
	asInstance.setMask(aRefModel->mask);

	bool cull = true;
	for(const RefMesh_s* mesh : aRefModel->refMeshes) cull &= mesh->mesh->getMaterial()->getCullBackface();
	if (!cull) asInstance.setFlags(VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR);

	if (mRefModelToInstanceIndex.find(aRefModel) == mRefModelToInstanceIndex.end()) {
		mRefModelToInstanceIndex.insert(std::pair(aRefModel, mTlas.size()));
		mRefModelToCustomIndex.insert(std::pair(aRefModel, static_cast<uint32_t>(mGeometryData.size())));
		mRefModelToGeometryOffset.insert(std::pair(aRefModel, mGeometryOffset));

		mTlas.addInstance(asInstance);
	} else {
		mTlas.replaceInstance(mRefModelToInstanceIndex[aRefModel], asInstance);
	}
	mGeometryOffset += aBlas->size();
	if (mTlas.size() > mMaxInstanceCount) spdlog::error("Instance count > tlas size");
}

void GeometryDataTlasVulkan::build(const rvk::CommandBuffer* aCmdBuffer)
{
	if (mTlas.isBuild()) {
		mTlas.CMD_Build(aCmdBuffer, mTlas.isBuild() ? &mTlas : nullptr);
	}
	else {
		mTlas.prepare(VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR);
		mTlas.CMD_Build(aCmdBuffer, nullptr);
	}
}

uint32_t GeometryDataTlasVulkan::size() const
{ return mTlas.size(); }

rvk::TopLevelAS* GeometryDataTlasVulkan::getTlas()
{ return &mTlas; }

rvk::Buffer* GeometryDataTlasVulkan::getGeometryDataBuffer()
{ return &mGeometryDataBuffer; }

void GeometryDataTlasVulkan::clear()
{
	mTlas.clear();
	mRefModelToInstanceIndex.clear();
	mRefModelToGeometryOffset.clear();
	mRefModelToCustomIndex.clear();
	mGeometryOffset = 0;
	mModels.clear();
	mGeometryData.clear();
}

int GeometryDataTlasVulkan::getInstanceIndex(RefModel_s* aRefModel)
{
	if (aRefModel == nullptr || (mRefModelToInstanceIndex.find(aRefModel) == mRefModelToInstanceIndex.end())) return -1;
	return static_cast<int>(mRefModelToInstanceIndex[aRefModel]);
}

int GeometryDataTlasVulkan::getCustomIndex(RefModel_s* aRefModel)
{
	if (aRefModel == nullptr || (mRefModelToCustomIndex.find(aRefModel) == mRefModelToCustomIndex.end())) return -1;
	return static_cast<int>(mRefModelToCustomIndex[aRefModel]);
}

int GeometryDataTlasVulkan::getGeometryOffset(RefModel_s* aRefModel)
{
	if (aRefModel == nullptr || (mRefModelToGeometryOffset.find(aRefModel) == mRefModelToGeometryOffset.end())) return -1;
	return static_cast<int>(mRefModelToGeometryOffset[aRefModel]);
}

std::deque<RefModel_s*>::iterator GeometryDataTlasVulkan::begin()
{ return mModels.begin(); }

std::deque<RefModel_s*>::const_iterator GeometryDataTlasVulkan::begin() const
{ return mModels.begin(); }

std::deque<RefModel_s*>::iterator GeometryDataTlasVulkan::end()
{ return mModels.end(); }

std::deque<RefModel_s*>::const_iterator GeometryDataTlasVulkan::end() const
{ return mModels.end(); }
