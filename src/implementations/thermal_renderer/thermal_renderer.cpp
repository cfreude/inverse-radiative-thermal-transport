#include "thermal_renderer.hpp"

#include <imgui.h>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <string>
#include <limits>
#include <math.h>
#include <map>
#include <fstream>

#include <spdlog/stopwatch.h>

/* engine */
#include <tamashii/engine/common/common.hpp>
#include <tamashii/engine/common/input.hpp>
#include <tamashii/engine/common/vars.hpp>
#include <tamashii/engine/platform/filewatcher.hpp>
#include <thermal_common.hpp>
#include <thermal_export.hpp>
#include <thermal_objective.hpp>
#include <thermal_params.hpp>

RVK_USE_NAMESPACE
using namespace tamashii;

#define VK_INDEX_DATA_SIZE 1024 * 1024 * 4
#define VK_VERTEX_DATA_SIZE 512 * 1024 * 4
#define VK_GLOBAL_IMAGE_SIZE 128
#define VK_INSTANCE_SIZE 256 * 4
#define VK_GEOMETRY_SIZE 256 * 4

/*
TODOs:
- FIX solver NaN
- solve FEM math
- think about switching to triangle based matrix?
- check if there is bug in raygen shader ThreadID > 1000
- add conduction
- FIX transport recomputationlogic/button
- make object properties editable
*/

void ThermalRenderer::windowSizeChanged(const int aWidth, const int aHeight) {
	SingleTimeCommand stc = mGetStcBuffer();
	stc.begin();
	for (uint32_t si_idx = 0; si_idx < mFrameCount; si_idx++) {
		VkFrameData& frameData = mVkFrameData[si_idx];

		frameData.rtImage.destroy();
		// rtImage
		frameData.rtImage.createImage2D(aWidth, aHeight, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
		frameData.rtImage.CMD_TransitionImage(stc.buffer(), VK_IMAGE_LAYOUT_GENERAL);
		frameData.globalDescriptor.setImage(GLSL_GLOBAL_OUT_IMAGE_BINDING, &frameData.rtImage);
		if (frameData.top.size()) frameData.globalDescriptor.update();
	}
	stc.end();
}

// implementation preparation
void ThermalRenderer::prepare(renderInfo_s* aRenderInfo) {

	//spdlog::set_level(spdlog::level::level_enum::info);
	//spdlog::set_level(spdlog::level::level_enum::debug);
	//spdlog::set_pattern("[%H:%M:%S %z] [%n] [%^---%L---%$] [thread %t] %v");

	printVramSize(mDevice);

	prepareData(aRenderInfo);

	mThermalTransport.prepare(mVkData->geometryDataBuffer, this->blas_gpu);

	createSunAndSky = false;
	sunDirections.push_back(Vector3f(1, 1, 1));
}

void ThermalRenderer::prepareData(renderInfo_s* aRenderInfo)
{
	printVramSize(mDevice);

	mVkData.emplace(mDevice);

	blas_gpu.prepare(VK_INDEX_DATA_SIZE, VK_VERTEX_DATA_SIZE);

	// geometry
	mVkData->geometryDataBuffer.create(rvk::Buffer::Use::STORAGE, VK_GEOMETRY_SIZE * sizeof(GeometrySSBO), rvk::Buffer::Location::HOST_COHERENT);
	mVkData->geometryDataBuffer.mapBuffer();

#ifndef DISABLE_GUI

	mVkFrameData.resize(mFrameCount, VkFrameData(mDevice));
	td_gpu.prepare(rvk::Shader::Stage::RAYGEN | rvk::Shader::Stage::ANY_HIT, VK_GLOBAL_IMAGE_SIZE);

	// lights
	mVkData->pointLightBuffer.create(rvk::Buffer::Use::STORAGE, sizeof(LightsSSBO), rvk::Buffer::Location::HOST_COHERENT);
	mVkData->pointLightBuffer.mapBuffer();

	SingleTimeCommand stc = mGetStcBuffer();
	stc.begin();
	// data for individual frames
	for (uint32_t si_idx = 0; si_idx < mFrameCount; si_idx++) {
		VkFrameData& frameData = mVkFrameData[si_idx];

		// instance
		frameData.instanceDataBuffer.create(rvk::Buffer::Use::STORAGE, VK_INSTANCE_SIZE * sizeof(InstanceSSBO), rvk::Buffer::Location::HOST_COHERENT);
		frameData.instanceDataBuffer.mapBuffer();
		// rtImage
		frameData.rtImage.createImage2D(aRenderInfo->target_size.x, aRenderInfo->target_size.y, VK_FORMAT_B8G8R8A8_UNORM, rvk::Image::Use::DOWNLOAD | rvk::Image::Use::UPLOAD | rvk::Image::Use::STORAGE);
		frameData.rtImage.CMD_TransitionImage(stc.buffer(), VK_IMAGE_LAYOUT_GENERAL);

		// global uniform buffer
		frameData.globalUniformBuffer.create(rvk::Buffer::Use::UNIFORM, sizeof(GlobalUbo), rvk::Buffer::Location::HOST_COHERENT);
		frameData.globalUniformBuffer.mapBuffer();

		// global uniform aux buffer
		frameData.globalAuxUniformBuffer.create(rvk::Buffer::Use::UNIFORM, sizeof(AuxiliaryUbo), rvk::Buffer::Location::HOST_COHERENT);
		frameData.globalAuxUniformBuffer.mapBuffer();

		// descriptors
		// global descriptor
		frameData.globalDescriptor.reserve(10);
		frameData.globalDescriptor.addUniformBuffer(GLSL_GLOBAL_UBO_BINDING, rvk::Shader::Stage::RAYGEN);
		frameData.globalDescriptor.addUniformBuffer(GLSL_GLOBAL_AUX_UBO_BINDING, rvk::Shader::Stage::RAYGEN);
		frameData.globalDescriptor.addStorageBuffer(GLSL_GLOBAL_INSTANCE_DATA_BINDING, rvk::Shader::Stage::RAYGEN | rvk::Shader::Stage::ANY_HIT);
		frameData.globalDescriptor.addStorageBuffer(GLSL_GLOBAL_GEOMETRY_DATA_BINDING, rvk::Shader::Stage::RAYGEN | rvk::Shader::Stage::ANY_HIT);
		frameData.globalDescriptor.addStorageBuffer(GLSL_GLOBAL_INDEX_BUFFER_BINDING, rvk::Shader::Stage::RAYGEN | rvk::Shader::Stage::ANY_HIT);
		frameData.globalDescriptor.addStorageBuffer(GLSL_GLOBAL_VERTEX_BUFFER_BINDING, rvk::Shader::Stage::RAYGEN | rvk::Shader::Stage::ANY_HIT);
		frameData.globalDescriptor.addStorageBuffer(GLSL_GLOBAL_TRANSPORT_DATA_BINDING, rvk::Shader::Stage::RAYGEN);
		frameData.globalDescriptor.addStorageBuffer(GLSL_GLOBAL_KELVIN_DATA_BINDING, rvk::Shader::Stage::RAYGEN);
		frameData.globalDescriptor.addStorageBuffer(GLSL_GLOBAL_LIGHT_DATA_BINDING, rvk::Shader::Stage::RAYGEN);
		frameData.globalDescriptor.addAccelerationStructureKHR(GLSL_GLOBAL_AS_BINDING, rvk::Shader::Stage::RAYGEN);
		frameData.globalDescriptor.addStorageImage(GLSL_GLOBAL_OUT_IMAGE_BINDING, rvk::Shader::Stage::RAYGEN);
		// set
		frameData.globalDescriptor.setBuffer(GLSL_GLOBAL_UBO_BINDING, &frameData.globalUniformBuffer);
		frameData.globalDescriptor.setBuffer(GLSL_GLOBAL_AUX_UBO_BINDING, &frameData.globalAuxUniformBuffer);
		frameData.globalDescriptor.setBuffer(GLSL_GLOBAL_INSTANCE_DATA_BINDING, &frameData.instanceDataBuffer);
		frameData.globalDescriptor.setBuffer(GLSL_GLOBAL_GEOMETRY_DATA_BINDING, &mVkData->geometryDataBuffer);
		frameData.globalDescriptor.setBuffer(GLSL_GLOBAL_INDEX_BUFFER_BINDING, this->blas_gpu.getIndexBuffer());
		frameData.globalDescriptor.setBuffer(GLSL_GLOBAL_VERTEX_BUFFER_BINDING, this->blas_gpu.getVertexBuffer());
		frameData.globalDescriptor.setBuffer(GLSL_GLOBAL_LIGHT_DATA_BINDING, &mVkData->pointLightBuffer);
		frameData.globalDescriptor.setImage(GLSL_GLOBAL_OUT_IMAGE_BINDING, &frameData.rtImage);
		frameData.globalDescriptor.finish(false);
	}
	stc.end();

	mVkData->rtshader.addStage(rvk::Shader::Source::GLSL, rvk::Shader::Stage::RAYGEN, "./assets/shader/raytracing_thermal/simple_ray.rgen", { "GLSL" });
	mVkData->rtshader.addStage(rvk::Shader::Source::GLSL, rvk::Shader::Stage::MISS, "./assets/shader/raytracing_thermal/simple_ray.rmiss", { "GLSL" });
	mVkData->rtshader.addStage(rvk::Shader::Source::GLSL, rvk::Shader::Stage::CLOSEST_HIT, "./assets/shader/raytracing_thermal/simple_ray.rchit", { "GLSL" });
	mVkData->rtshader.addStage(rvk::Shader::Source::GLSL, rvk::Shader::Stage::ANY_HIT, "./assets/shader/raytracing_thermal/simple_ray.rahit", { "GLSL" });
	mVkData->rtshader.addGeneralShaderGroup("./assets/shader/raytracing_thermal/simple_ray.rmiss");
	mVkData->rtshader.addGeneralShaderGroup("./assets/shader/raytracing_thermal/simple_ray.rgen");
	mVkData->rtshader.addHitShaderGroup("./assets/shader/raytracing_thermal/simple_ray.rchit", "./assets/shader/raytracing_thermal/simple_ray.rahit");
	mVkData->rtshader.finish();

	mVkData->rtpipeline.setShader(&mVkData->rtshader);
	mVkData->rtpipeline.addDescriptorSet({ &mVkFrameData[0].globalDescriptor, td_gpu.getDescriptor() });
	mVkData->rtpipeline.finish();

#endif // DISABLE_GUI
}

void ThermalRenderer::destroy() {
	FileWatcher::getInstance().removeFile("assets/shader/raytracing_thermal/simple_ray.rgen");

	td_gpu.destroy();
	blas_gpu.destroy();
	mVkFrameData.clear();
	mVkData.reset();
	mThermalTransport.unload();
}

void ThermalRenderer::computeSceneAABB()
{
	RenderScene* _scene = Common::getInstance().getRenderSystem()->getMainScene();
	scene_s scene = _scene->getSceneData();

	std::tie(min_aabb, max_aabb) = computeSceneAABB(scene);
	spdlog::debug("AABB min: {}, {}, {}", min_aabb.x, min_aabb.y, min_aabb.z);
	spdlog::debug("AABB max: {}, {}, {}", max_aabb.x, max_aabb.y, max_aabb.z);

	std::tie(boundingSphereCenter, boundingSphereRadius) = computeSceneBoundingSphere(min_aabb, max_aabb);
	spdlog::debug("boundingSphereCenter: {}, {}, {}", boundingSphereCenter.x, boundingSphereCenter.y, boundingSphereCenter.z);
	spdlog::debug("boundingSphereRadius: {}", boundingSphereRadius);
}

std::tuple<glm::vec3, glm::vec3> ThermalRenderer::computeSceneAABB(scene_s _scene)
{
	if (_scene.refModels.size() != 0) {
		glm::vec3 min_aabb = glm::vec3(std::numeric_limits<float>::max());
		glm::vec3 max_aabb = glm::vec3(std::numeric_limits<float>::min());

		for (RefModel_s* refModel : _scene.refModels) {
			if (refModel->model->getName() == "Sky")
				continue;

			if (!((bool)refModel->model->getCustomProperty("traceable").getInt()))
				continue;

			glm::mat4 model_matrix = glm::transpose(refModel->model_matrix);
			for (RefMesh_s* refMesh : refModel->refMeshes) {
				Mesh* m = refMesh->mesh;
				for (int vi = 0; vi < m->getVertexCount(); vi++) {
					glm::vec4 v0_ = refModel->model_matrix * m->getVerticesVector()->at(vi).position;
					glm::vec3 v0 = glm::vec3(v0_.x, v0_.y, v0_.z) / v0_.w;
					min_aabb = glm::min(min_aabb, v0);
					max_aabb = glm::max(max_aabb, v0);
				}
			}
		}

		return std::make_tuple(min_aabb, max_aabb);
	}
	else
	{
		spdlog::warn("Empty scene. Can't compute valid AABB.");
		return std::make_tuple(glm::vec3(), glm::vec3());
	}
}

std::tuple<glm::vec3, float> ThermalRenderer::computeSceneBoundingSphere(glm::vec3 _min_aabb, glm::vec3 _max_aabb)
{
	glm::vec3 half_diagonal = (_max_aabb - _min_aabb) * 0.5f;
	float half_diagonal_length = glm::length(half_diagonal);

	if (half_diagonal_length > 0)
	{
		return std::make_tuple(_min_aabb + half_diagonal, half_diagonal_length);
	}
	else
	{
		spdlog::warn("Invalid AABB. Can't compute valid bounding sphere.");
		return std::make_tuple(glm::vec3(), 0.f);
	}
}

glm::vec4 ThermalRenderer::getBoundingSphere()
{
	return glm::vec4(boundingSphereCenter.x, boundingSphereCenter.y, boundingSphereCenter.z, boundingSphereRadius);
}

// scene
void ThermalRenderer::sceneLoad(scene_s scene) {

	//scene_s s = _load_scene();
	_sceneLoad();
}

void ThermalRenderer::_sceneLoad() {
	
	computeSceneAABB();

	RenderScene* _scene = Common::getInstance().getRenderSystem()->getMainScene();
	scene_s scene = _scene->getSceneData();

	// CONTINUE HERE
	// mThermalSky.loadGeometry(_scene);

	CHECK_EMPTY_SCENE(scene);

	loadData(scene);

	mThermalScene.load(scene);
	ThermalObjects& termObj = mThermalScene.getObjects();
	ThermalScene::SceneProperties_s& sceneProp = mThermalScene.getProperties();

	mThermalData.init(scene, termObj, sceneProp.vertexCount, sceneProp.triangleCount );
	mThermalData.load(scene, termObj, sky_min_kelvin, solver.compute_steady_state ? 1 : mThermalVars.timeSteps);

	SingleTimeCommand stc = mGetStcBuffer();
	mThermalTransport.load(stc, mDevice, blas_gpu, mVkData->geometryDataBuffer, scene, mThermalScene, mThermalData, mThermalVars, true);
	//computeTransportMatrix();

#ifndef DISABLE_GUI
	for (uint32_t si_idx = 0; si_idx < mFrameCount; si_idx++) {
		VkFrameData& frameData = mVkFrameData[si_idx];
		frameData.globalDescriptor.setBuffer(GLSL_GLOBAL_TRANSPORT_DATA_BINDING, &mThermalTransport.getTransportBuffer());
		frameData.globalDescriptor.setBuffer(GLSL_GLOBAL_KELVIN_DATA_BINDING, &mThermalTransport.getKelvinBuffer());
		frameData.globalDescriptor.update();
	}
#endif !DISABLE_GUI

	thermalInit(scene);

#ifdef DEBUG
	printVramSize(mDevice);
#endif // DEBUG


#ifndef DISABLE_GUI
	//mThermalVars.remainingTimeSteps = 0;
	mThermalGui.plotObjComboString = "";
	for (string name : termObj.name)
		mThermalGui.plotObjComboString += name + '\0';
	spdlog::info(mThermalGui.plotObjComboString);

	bool tmp=mThermalGui.enableAutoAdjust;
	mThermalGui.enableAutoAdjust=true;
	mThermalGui.autoAdjustDisplayRange(mThermalData);		
	mThermalGui.enableAutoAdjust=tmp;

	mThermalGui.reset();
#endif !DISABLE_GUI
}

void ThermalRenderer::loadData(scene_s scene)
{
	SingleTimeCommand stc = mGetStcBuffer();

	int count = 0;

#ifndef DISABLE_GUI
	td_gpu.loadScene(&stc, scene);
#endif !DISABLE_GUI

	// geometry lookup buffer to find the correct vertex informations in shader during ray tracing
	GeometrySSBO geometry[VK_GEOMETRY_SIZE] = {};
	int geometry_count = 0;
	int vertex_count = 0;
	int triangle_count = 0;

	blas_gpu.loadScene(&stc, scene);
	// no need to build any tlas if there are no models in the current scene
	spdlog::debug("model count = {}", scene.refModels.size());
	if (scene.refModels.size() != 0) {
		count = 0;
		for (RefModel_s* refModel : scene.refModels) {
			// each mesh in our model will be a geometry of this models blas
			for (RefMesh_s* refMesh : refModel->refMeshes) {
				Mesh* m = refMesh->mesh;

				GeometryDataVulkan::primitveBufferOffset_s offsets = blas_gpu.getOffset(m);
				// add geometry infos to our geometry lookup buffer
				geometry[geometry_count].index_buffer_offset = offsets.mIndexOffset;
				geometry[geometry_count].vertex_buffer_offset = offsets.mVertexOffset;
				geometry[geometry_count].has_indices = m->hasIndices();
				std::memcpy(&geometry[geometry_count].baseColorFactor, &refMesh->mesh->getMaterial()->getBaseColorFactor()[0], sizeof(glm::vec4));
				geometry[geometry_count].baseColorTexIdx = -1;
				if (refMesh->mesh->getMaterial()->hasBaseColorTexture()) geometry[geometry_count].baseColorTexIdx = refMesh->mesh->getMaterial()->getBaseColorTexture()->index;
				geometry[geometry_count].alphaCutoff = refMesh->mesh->getMaterial()->getAlphaDiscardValue();
				geometry[geometry_count].triangleCount = m->getVertexCount() / 3;

				// index
				if (m->hasIndices()) {
					triangle_count += m->getIndexCount() / 3;
					geometry[geometry_count].triangleCount = m->getIndexCount() / 3;
				}
				else {
					triangle_count += m->getVertexCount() / 3;
				}
				geometry_count++;

				// vertex
				vertex_count += m->getVertexCount();
			}
		}
		mVkData->geometryDataBuffer.STC_UploadData(&stc, &geometry, geometry_count * sizeof(GeometrySSBO));

#ifndef DISABLE_GUI

		for (uint32_t si_idx = 0; si_idx < mFrameCount; si_idx++) {
			VkFrameData& frameData = mVkFrameData[si_idx];

			frameData.top.reserve(scene.refModels.size());
			for (RefModel_s* refModel : scene.refModels) {
				rvk::ASInstance as_instance(blas_gpu.getBlas(refModel->model));
				glm::mat4 model_matrix = glm::transpose(refModel->model_matrix);
				as_instance.setTransform(&model_matrix[0][0]);
				as_instance.setMask(refModel->mask);
				frameData.top.addInstance(as_instance);
			}
			frameData.top.prepare(VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR);
			stc.begin();
			frameData.top.CMD_Build(stc.buffer());
			stc.end();
			// add the tlas to the descriptor and update it
			frameData.globalDescriptor.setAccelerationStructureKHR(GLSL_GLOBAL_AS_BINDING, &frameData.top);
		}


#endif !DISABLE_GUI

	}
}

void ThermalRenderer::computeTransportMatrix()
{
	if (!mDisableCompute)
	{
		SingleTimeCommand stc = mGetStcBuffer();
		mThermalTransport.compute(stc, mDevice, mThermalScene, mThermalData, mThermalVars, solver.mode);
	}
	mDisableCompute = false;
}

void ThermalRenderer::thermalInit(scene_s scene)
{			
	resetSimulation();
	spdlog::info("thermalInit: done");
}

void ThermalRenderer::getKelvin(float* _arr, unsigned int _count)
{
	const VectorXf tmp = mThermalData.currentValueVector.cast<FLOAT>();
	for (int i = 0; i < _count; i += 3)
	{
		float val = tmp[i / 3] / kelvinUnitFactor;
		_arr[i] = val;
		_arr[i + 1] = val;
		_arr[i + 2] = val;
	}
}

void ThermalRenderer::getValue(float* _arr, unsigned int _count, unsigned int _type)
{
	const VectorXf tmp = mThermalData.currentValueVector.cast<FLOAT>();
	Vec& vertexAreaVector = mThermalData.vertexAreaVector;
	for (int i = 0; i < _count; i += 3)
	{
		float val = tmp[i / 3] / kelvinUnitFactor;
		if (_type == 1)
			val = STEFAN_BOLTZMANN_CONST_RAW * vertexAreaVector[i / 3] * pow(val, 4.0);
		_arr[i] = val;
		_arr[i + 1] = val;
		_arr[i + 2] = val;
	}
}

void ThermalRenderer::setKelvin(float* _arr, unsigned int _arr_size, unsigned int _vertex_offset, bool _sky_values)
{
	Vec& currentValueVector = mThermalData.currentValueVector;

	if (_sky_values)
	{		
		RenderScene* _scene = Common::getInstance().getRenderSystem()->getMainScene();
		scene_s scene = _scene->getSceneData();

		Vec& vertexAreaVector = mThermalData.vertexAreaVector;
		SCALAR avg_sky_value = 0.0;
		for (RefModel_s* refModel : scene.refModels) {

			if (refModel->model->getName() == "Sky")
			{
				for (RefMesh_s* refMesh : refModel->refMeshes) {
					Mesh* m = refMesh->mesh;
					std::vector<uint32_t> vis = *m->getIndicesVector();
					unsigned int index_offset = 0;
					for (int i = 0; i < _arr_size; i++)
					{
						float sky_value = _arr[i]; // sky value in kWh/m2(/h)
						avg_sky_value += sky_value;

						{
							unsigned int vertex_ind_0 = sky_vertex_offset + vis[index_offset++];
							mThermalData.initialValueVector(vertex_ind_0) = sky_value_to_kelvin(sky_value, sky_min_kelvin);

							unsigned int vertex_ind_1 = sky_vertex_offset + vis[index_offset++];
							mThermalData.initialValueVector(vertex_ind_1) = sky_value_to_kelvin(sky_value, sky_min_kelvin);

							unsigned int vertex_ind_2 = sky_vertex_offset + vis[index_offset++];
							mThermalData.initialValueVector(vertex_ind_2) = sky_value_to_kelvin(sky_value, sky_min_kelvin);
						}

						{
							unsigned int vertex_ind_0 = sky_vertex_offset + vis[index_offset++];
							mThermalData.initialValueVector(vertex_ind_0) = sky_value_to_kelvin(sky_value, sky_min_kelvin);

							unsigned int vertex_ind_1 = sky_vertex_offset + vis[index_offset++];
							mThermalData.initialValueVector(vertex_ind_1) = sky_value_to_kelvin(sky_value, sky_min_kelvin);

							unsigned int vertex_ind_2 = sky_vertex_offset + vis[index_offset++];
							mThermalData.initialValueVector(vertex_ind_2) = sky_value_to_kelvin(sky_value, sky_min_kelvin);
						}
					}

					avg_sky_value /= ((float)_arr_size);
				}

				mThermalScene.getObjects().kelvin[0] = avg_sky_value;

				Vec sky_kelvin = mThermalData.initialValueVector.segment(sky_vertex_offset, sky_vertex_count);
				currentValueVector.segment(sky_vertex_offset, sky_vertex_count) = sky_kelvin;

#ifndef RUNTIME_OPTIMIZED
				spdlog::debug("sky_kelvin MIN: {}", sky_kelvin.minCoeff());
				spdlog::debug("sky_kelvin MEAN: {}", sky_kelvin.mean());
				spdlog::debug("sky_kelvin MAX: {}", sky_kelvin.maxCoeff());
#endif // !RUNTIME_OPTIMIZED
			}
		}
	}
	else
	{
		for (int i = 0; i < _arr_size; i++)
			currentValueVector[_vertex_offset+i] = _arr[i] * kelvinUnitFactor;
	}
}

void ThermalRenderer::setTimeStep(float _hours)
{
	solver.step_size = _hours * 3600.0 * secondsUnitFactor;
}

void ThermalRenderer::setTime(float _hours)
{
	mThermalVars.simulationTime = _hours * 3600.0 * secondsUnitFactor;
}

float ThermalRenderer::getTimeHours()
{
	return mThermalData.sky.current_hour + mThermalVars.simulationTime / (3600.0 * secondsUnitFactor);
}

void ThermalRenderer::updateSky(float _hour)
{
	mThermalData.setSkyHour(_hour);
	SingleTimeCommand stc = mGetStcBuffer();
	mThermalTransport.uploadValueVector(stc, mThermalData.currentValueVector);
	ThermalObjects& objects = mThermalScene.getObjects();
	mThermalData.setObjectStatistics(objects, mThermalData.currentValueVector);
}

void ThermalRenderer::resetSimulation()
{
	mThermalData.reset();
	mThermalVars.simulationTime = 0.0;
	mThermalVars.outputFileCount = 0;

#ifndef DISABLE_GUI
	SingleTimeCommand stc = mGetStcBuffer();
	mThermalTransport.uploadValueVector(stc, mThermalData.currentValueVector);
	mThermalVars.simulationTime = 0.0;
	//mThermalVars.remainingTimeSteps = 0;
	
	ThermalObjects& objects = mThermalScene.getObjects();
	mThermalData.setObjectStatistics(objects, mThermalData.currentValueVector);

	mThermalGui.autoAdjustDisplayRange(mThermalData);
#endif !DISABLE_GUI
}

void ThermalRenderer::thermalTimestep()
{
	Vec x = mThermalData.currentValueVector;
	x = (mThermalData.fixedVarsVector.array() > 0.0).select(mThermalData.initialValueVector, x);

	if ((solver.mode == 0) && solver.compute_steady_state)
	{
		//SCALAR mean = x.mean();
		x = (mThermalData.fixedVarsVector.array() < 1.0 && x.array() == 0.0).select(1.0, x);
	}

	if (!solver.compute_steady_state && mThermalVars.simulationTime <= 0.0)
	{
		spdlog::info("time: {} h | step dur.: {} s", 0, 0);
		logEigenBase("x", x.transpose() / kelvinUnitFactor);
	}

	spdlog::stopwatch sw;
	spdlog::stopwatch sw_acaTests;

	logEigenBase("thermalVars.initialValueVector", mThermalData.initialValueVector.transpose() / kelvinUnitFactor);
	logEigenBase("solve(x)", x.transpose() / kelvinUnitFactor);

#ifndef RUNTIME_OPTIMIZED
	//SCALAR val = solver.residual(x).array().pow(2.0).sum();
	//spdlog::debug("value(x) = {}", val);
#endif // !RUNTIME_OPTIMIZED	

	solver.solve(x, mThermalData.currentValueVector, mThermalData.lumpedMassMatrix, mThermalTransport.getTransportMatrix(), mThermalData.conductiveLaplacian, mThermalData.fixedVarsVector, mThermalTransport.getACATransportMatrix(), mThermalTransport.isCompressed());

	if (solver.mode == 1)
	{		
		x = (mThermalData.fixedVarsVector.array() < 1.0).select(x, mThermalData.initialValueVector);
	}

	mThermalData.currentValueVector = x;

	auto comp_time = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(sw_acaTests.elapsed()).count() / 1000.0);
	if (mThermalTransport.isCompressed()) {
		std::vector<SCALAR>& aca_comp_times = solver.getACACompTime();
		aca_comp_times.push_back(std::stof(comp_time));
		Vec& T_aca = solver.getTACA();
		T_aca = x;
		SCALAR& compressionDuration = solver.getCompressionDuration();
		compressionDuration = mThermalTransport.getCompressionDuration();
		SCALAR& compressionErrorFrob = solver.getTFrobError();
		SCALAR& compressionErrorInf = solver.getTInfError();
		SCALAR& compressionErrorFrobRel = solver.getTFrobErrorRel();
		SCALAR& compressionErrorInfRel = solver.getTInfErrorRel();
		compressionErrorFrob = mThermalTransport.getTFrobError();
		compressionErrorInf = mThermalTransport.getTInfError();
		compressionErrorFrobRel = mThermalTransport.getTFrobErrorRel();
		compressionErrorInfRel = mThermalTransport.getTInfErrorRel();
	}
	else {
		std::vector<SCALAR>& reg_comp_times = solver.getRegCompTime();
		reg_comp_times.push_back(std::stof(comp_time));
		Vec& T = solver.getT();
		T = x;
	}

	//mThermalGui.totalTime += sw.elapsed();
	//mThermalGui.computedStepCount += 1;

	if (solver.compute_steady_state) {
		spdlog::info("thermalTimestep() - steady state | dur. {:.3} s", sw);
	}		
	else {
		mThermalVars.simulationTime += solver.step_size;
		spdlog::info("thermalTimestep() - time: {} h | dur. {:.3} s", mThermalVars.simulationTime / (secondsUnitFactor * 3600.0), sw);

		// update sky
		if (mThermalData.sky.hours.size() > 0)
		{
			float hour = getTimeHours();
			updateSky(hour);
		}

		if (mThermalVars.exportEnabled)
		{
			// export after time step
			if (mThermalVars.outputFileCount >= 0) { // set negative to disable output
				RenderScene* _scene = Common::getInstance().getRenderSystem()->getMainScene();
				std::filesystem::path sceneFile(_scene->getSceneFileName());
				std::string fnameNoExt = sceneFile.filename().string();
				if (fnameNoExt.find('.') != fnameNoExt.npos) fnameNoExt = fnameNoExt.substr(0, fnameNoExt.find_first_of('.'));
				std::stringstream sstr;
				sstr << sceneFile.parent_path().string() << std::string(1, sceneFile.preferred_separator) << fnameNoExt << "_t";
				sstr.width(5); sstr.fill('0'); sstr << (mThermalVars.outputFileCount++) << ".vtk";
				export_vtk(sstr.str(), mThermalData.currentValueVector);
				spdlog::info("Exported {}", sstr.str());
			}
		}
	}	
	logEigenBase("x (kelvin)", x.transpose() / kelvinUnitFactor);

#ifndef DISABLE_GUI

	SingleTimeCommand stc = mGetStcBuffer();
	mThermalTransport.uploadValueVector(stc, mThermalData.currentValueVector);

	if (mThermalGui.showStatistics)
	{
		ThermalObjects& objects = mThermalScene.getObjects();
		mThermalData.setObjectStatistics(objects, mThermalData.currentValueVector);

		int offset = mThermalScene.getObjects().vertexOffset[mThermalGui.plotObjComboIndex];
		int count = mThermalScene.getObjects().vertexCount[mThermalGui.plotObjComboIndex];

		float avgKelvin = mThermalData.currentValueVector.segment(offset, count).mean();
		mThermalGui.avgKelvin = avgKelvin;
		mThermalGui.avgKelvinMin = std::min(mThermalGui.avgKelvinMin, avgKelvin);
		mThermalGui.avgKelvinMax = std::max(mThermalGui.avgKelvinMax, avgKelvin);
		mThermalGui.avgKelvins.emplace_back(avgKelvin);
	}

	mThermalGui.autoAdjustDisplayRange(mThermalData);
#endif !DISABLE_GUI
}

void ThermalRenderer::sceneUnload(scene_s scene) {

	blas_gpu.unloadScene();

#ifndef DISABLE_GUI

	td_gpu.unloadScene();
	for (int i = 0; i < mFrameCount; i++) {
		mVkFrameData[i].top.destroy();
	}

#endif // !DISABLE_GUI

	solver.reset();
	mThermalTransport.unload();
	mThermalScene.unload();
	mThermalData.unload();
}

void ThermalRenderer::_sceneUnload() {
	RenderScene* scene = Common::getInstance().getRenderSystem()->getMainScene();
	scene->readyToRender(false);
	sceneUnload(scene->getSceneData());
	for (auto m : scene->getSceneData().refModels)
		scene->removeModel(m);
	for (auto m : scene->getSceneData().models)
		delete m;
	scene->getSceneData().models.clear();
}

// frame
void ThermalRenderer::drawView(viewDef_s* aViewDef) {

#ifdef DISABLE_GUI
	assert(false);// , "drawView called!");
#endif // DISABLE_GUI

	if (InputSystem::getInstance().wasReleased(Input::KEY_ESCAPE))
	{
		mThermalVars.runOptim = false;
	}

	SingleTimeCommand stc = mGetStcBuffer();

	if (mThermalVars.reset)
	{
		resetSimulation();
		mThermalVars.reset = false;
	}

	if( mThermalVars.resetGeometryFromParams ){
		updateGeometryWithParameters( mThermalData.params->getLastParams() );
		mThermalVars.resetGeometryFromParams = false;
	}

	if (mThermalVars.updateSky)
	{
		updateSky(mThermalData.sky.current_hour);
		mThermalVars.updateSky = false;
	}

	if (mThermalVars.recomputeTransport)
	{
		//resetSimulation();
		RenderScene* render_scene = tamashii::Common::getInstance().getRenderSystem()->getMainScene();
		mThermalTransport.load(stc, mDevice, blas_gpu, mVkData->geometryDataBuffer, render_scene->getSceneData(), mThermalScene, mThermalData, mThermalVars);
		computeTransportMatrix();
		mThermalTransport.uploadValueVector(stc, mThermalData.currentValueVector);
		mThermalVars.recomputeTransport = false;
	}

	if (mThermalVars.remainingTimeSteps > 0)
	{
		// if this is the first call to thermalTimestep and we're supposed to start with steady-state initial conditions temporarily switch to steady state mode ...
		bool tmpSteadyState = false;
		if( mThermalVars.steadyStateInitialCondition && solver.compute_steady_state==false && mThermalVars.remainingTimeSteps==mThermalVars.timeSteps && mThermalVars.simulationTime==0.0 ){
			tmpSteadyState = true;
			solver.compute_steady_state = true;
		}
		
		thermalTimestep();
		
		if( tmpSteadyState ) solver.compute_steady_state = false;

		if (solver.compute_steady_state)
			mThermalVars.remainingTimeSteps = 0;
		else
			mThermalVars.remainingTimeSteps -= 1;

		if (mThermalGui.saveScreenshotForIteration)
		{
			RenderScene* _scene = Common::getInstance().getRenderSystem()->getMainScene();
			std::filesystem::path sceneFile(_scene->getSceneFileName());
			std::string fnameNoExt = sceneFile.filename().string();
			if (fnameNoExt.find('.') != fnameNoExt.npos) fnameNoExt = fnameNoExt.substr(0, fnameNoExt.find_first_of('.'));
			std::string path = sceneFile.parent_path().string() + std::string(1, sceneFile.preferred_separator) + fnameNoExt + fmt::format("-{:0>4}", mThermalGui.screenshotCounter) + ".png";
			Common::getInstance().takeScreenshot(path, false);
			mThermalGui.screenshotCounter++;
		}
	}

	if( mThermalVars.runOptim ){
		runGeomOptim();

		if (mThermalGui.saveScreenshotForIteration)
		{
			RenderScene* _scene = Common::getInstance().getRenderSystem()->getMainScene();
			std::filesystem::path sceneFile(_scene->getSceneFileName());
			std::string fnameNoExt = sceneFile.filename().string();
			if (fnameNoExt.find('.') != fnameNoExt.npos) fnameNoExt = fnameNoExt.substr(0, fnameNoExt.find_first_of('.'));
			std::string path = sceneFile.parent_path().string() + std::string(1, sceneFile.preferred_separator) + fnameNoExt + fmt::format("-{:0>4}", mThermalGui.screenshotCounter) + ".png";
			Common::getInstance().takeScreenshot(path, false);
			mThermalGui.screenshotCounter++;
		}
	}
	if( mThermalVars.runSingleFwd ){
		mThermalVars.runSingleFwd = false;
		updateGeometryWithParameters(mThermalData.params->getLastParams());
		runForwardSim();
	}
	if( mThermalVars.runSingleAdj ){
		mThermalVars.runSingleAdj = false;
		Vec grad; grad.resize( mThermalData.params->getNumberOfParams() );
		runAdjointSim(grad);
	}
	if( mThermalVars.runSingleFD ){
		mThermalVars.runSingleFD = false;
		runGeomOptim(true/*only FD check*/);
	}
	if( mThermalVars.doPrepT2 ){
		mThermalVars.doPrepT2 = false;
		runForwardSim();
		Vec p = mThermalData.params->getLastParams();
		if( p.size()>2 ){
			p[0] = -0.5;
			p[1] = -50.0/180.0*3.1415926;
			p[2] =  0.0;
			updateGeometryWithParameters(p);
			runForwardSim();
		}
	}



	if( mThermalVars.visualizeGradient >= 0){
		visGrad(mThermalVars.visualizeGradient);
		mThermalVars.visualizeGradient = -1;
	}

	CommandBuffer* cb = mGetCurrentCmdBuffer();
	if (!aViewDef->surfaces.size()) {
		const glm::vec4 cc = glm::vec4(var::bg.getInt3(), 255.f) / 255.0f;
		mSwapchain->CMD_BeginRenderClear(cb, { { cc.x, cc.y, cc.z, 1.0f }, {0,0} });
		mSwapchain->CMD_EndRender(cb);
		return;
	};

	// ubo
	GlobalUbo ubo;
	std::memcpy(&ubo.viewMat, &aViewDef->view_matrix[0], sizeof(glm::mat4));
	std::memcpy(&ubo.projMat, &aViewDef->projection_matrix[0], sizeof(glm::mat4));
	std::memcpy(&ubo.inverseViewMat, &aViewDef->inv_view_matrix[0], sizeof(glm::mat4));
	std::memcpy(&ubo.inverseProjMat, &aViewDef->inv_projection_matrix[0], sizeof(glm::mat4));
	std::memcpy(&ubo.viewPos, &aViewDef->view_pos[0], sizeof(glm::vec3));
	std::memcpy(&ubo.viewDir, &aViewDef->view_dir[0], sizeof(glm::vec3));
	mVkFrameData[mCurrentFrame()].globalUniformBuffer.STC_UploadData(&stc, &ubo, sizeof(GlobalUbo));

	// aux ubo
	AuxiliaryUbo aux_ubo;
	aux_ubo.vertexCount = mThermalScene.getProperties().vertexCount;
	aux_ubo.sourceVertexInd = mThermalGui.sourceVertexId;
	aux_ubo.visualization = mThermalGui.visualization;
	aux_ubo.displayRangeMin = mThermalGui.displayRangeMin;
	aux_ubo.displayRangeMax = mThermalGui.displayRangeMax;
	aux_ubo.clipDistance = mThermalGui.clipDistance;
	aux_ubo.backfaceCulling = mThermalGui.backfaceCulling;
	aux_ubo.cullMask = mThermalGui.showSky ? 0xff : 0xf0;
	aux_ubo.wireframeFactor = mThermalGui.wireframe ? 0.01 : 1.0;
	aux_ubo.aoSamples = mThermalGui.aoSamples;
	aux_ubo.aoDist = mThermalGui.aoDistance;
	aux_ubo.aoBlending = mThermalGui.aoBlending;
	mVkFrameData[mCurrentFrame()].globalAuxUniformBuffer.STC_UploadData(&stc, &aux_ubo, sizeof(AuxiliaryUbo));

	// lights
	LightsSSBO lights;
	int count = 0;
	for (const RefLight_s* l : aViewDef->lights) {
		PunctualLight& pl = lights.punctual_lights[count];
		// TODO: don't use enum type direct
		pl.type = (int)l->light->getType();
		std::memcpy(&pl.color, &l->light->getColor()[0], sizeof(glm::vec3));
		std::memcpy(&pl.position, &l->position[0], sizeof(glm::vec3));
		std::memcpy(&pl.direction, &l->direction[0], sizeof(glm::vec3));
		pl.intensity = l->light->getIntensity();
		switch (l->light->getType()) {
		case Light::Type::POINT:
			pl.range = ((PointLight*)l->light)->getRange();
			break;
		case Light::Type::SPOT:
			pl.range = ((SpotLight*)l->light)->getRange();
			pl.lightAngleScale = 1.0f / std::max(0.001f, (float)(cos(((SpotLight*)l->light)->getInnerConeAngle()) - cos(((SpotLight*)l->light)->getOuterConeAngle())));
			pl.lightAngleOffset = -cos(((SpotLight*)l->light)->getOuterConeAngle()) * pl.lightAngleScale;
			break;
		}
		count++;
	}
	lights.punctual_light_count = count;
	mVkData->pointLightBuffer.STC_UploadData(&stc, &lights, sizeof(LightsSSBO));

	InstanceSSBO instance[VK_INSTANCE_SIZE] = {};

	if (aViewDef->scene.refModels.size() != 0) {
		VkFrameData& frameData = mVkFrameData[mCurrentFrame()];
		frameData.top.clear();
		//frameData.top.destroy();
		//frameData.top.reserve(vd->refModels->size());

		int ind = 0;
		int geometry_offset = 0;
		ThermalObjects thermalObjects = mThermalScene.getObjects();
		for (RefModel_s *refModel : aViewDef->scene.refModels) {
			// glm stores matrices in a column-major order but vulkan acceleration structures requiere a row-major order
			glm::mat4 model_matrix = glm::transpose(refModel->model_matrix);
			// add instance infos for our instance lookup buffer
			
			//int ind = frameData.top.size();
			
			instance[ind].geometry_buffer_offset = geometry_offset;
			geometry_offset += refModel->refMeshes.size();
			std::memcpy(&instance[ind].model_matrix, &refModel->model_matrix[0], sizeof(glm::mat4));

			glm::mat4 trp_inv_model_matrix = glm::transpose(glm::inverse(refModel->model_matrix));
			std::memcpy(&instance[ind].trp_inv_model_matrix, &trp_inv_model_matrix[0], sizeof(glm::mat4));

			instance[ind].diffuseReflectance = thermalObjects.diffuseReflectance[ind];
			instance[ind].specularReflectance = thermalObjects.specularReflectance[ind];
			instance[ind].absorption = thermalObjects.absorption[ind];
			instance[ind].diffuseEmission = thermalObjects.diffuseEmission[ind];
			instance[ind].traceable = thermalObjects.traceable[ind];

			// create an instance for the blas
			rvk::ASInstance as_instance(blas_gpu.getBlas(refModel->model));
			as_instance.setTransform(&model_matrix[0][0]);
			as_instance.setMask(refModel->mask);
			// add the blas to the tlas
			frameData.top.addInstance(as_instance);
			ind++;
		}

		frameData.top.CMD_Build(cb, &frameData.top);
		cb->cmdMemoryBarrier(VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);

		// upload instance
		frameData.instanceDataBuffer.STC_UploadData(&stc, &instance, frameData.top.size() * sizeof(InstanceSSBO));
		frameData.instanceDataBuffer.CMD_BufferMemoryBarrier(cb, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);

		mVkData->rtpipeline.CMD_BindDescriptorSets(cb, { &mVkFrameData[mCurrentFrame()].globalDescriptor, td_gpu.getDescriptor()});
		mVkData->rtpipeline.CMD_BindPipeline(cb);
		mVkData->rtpipeline.CMD_TraceRays(cb, aViewDef->target_size.x, aViewDef->target_size.y);
	}

	rvk::swapchain::CMD_BlitImageToCurrentSwapchainImage(cb, mSwapchain, &mVkFrameData[mCurrentFrame()].rtImage, VK_FILTER_LINEAR);
}

void ThermalRenderer::drawUI(uiConf_s* uc)
{
	mThermalGui.draw(uc, mThermalScene.getProperties().vertexCount, solver, mThermalVars, mThermalData, mThermalScene.getObjects());
}


void ThermalRenderer::adjointTimestep(int tstep, Vec& adjointState, Vec& adjointSource)
{
	Vec temperature = mTemperatureHistory[tstep];
	
	adjointSource = (mThermalData.fixedVarsVector.array() > 0.0).select(0.0, adjointSource);

	Mat systemMatrix;
	ACAMatrix systemMatrixACA;
	solver.jacobian(temperature, systemMatrix, mThermalData.lumpedMassMatrix, mThermalTransport.getTransportMatrix(),
		mThermalData.conductiveLaplacian, mThermalData.fixedVarsVector, mThermalTransport.getACATransportMatrix(),
		mThermalTransport.isCompressed(), systemMatrixACA,
		true // transpose!
	);

	if (mThermalTransport.isCompressed()) {
		BiCGSTAB<ACAMatrix, IdentityPreconditioner> linSolver(systemMatrixACA);
		adjointState = linSolver.solve(adjointSource);
	}
	else {
		BiCGSTAB<Mat> linSolver(systemMatrix);
		adjointState = linSolver.solve(adjointSource);
	}

	adjointState = (mThermalData.fixedVarsVector.array() > 0.0).select(0.0, adjointState);
}

void ThermalRenderer::runForwardSim(){
	mThermalVars.outputFileCount = 0;
	int timeSteps = mThermalVars.timeSteps;
	if(solver.compute_steady_state) timeSteps=1;

	resetSimulation();

	Vec zeroVec;
	zeroVec.resizeLike( mThermalData.currentValueVector );
	zeroVec.setZero();
	mTemperatureHistory.assign(timeSteps, zeroVec );

	spdlog::stopwatch steptransport_sw;
	mThermalTransport.compute(mGetStcBuffer(), mDevice, mThermalScene, mThermalData, mThermalVars, solver.mode);
	mThermalGui.transportSum += steptransport_sw.elapsed();

	if( mThermalData.params->isNopTestActive() ){
		Mat copy =  mThermalTransport.getTransportMatrix();
		mThermalTransport.setTransportMatrix( copy + mThermalData.params->getLastParams()[0] * 1e-8 * Mat::Identity(copy.rows(),copy.cols()) );
		spdlog::info("nop test mod T-op {}",  mThermalData.params->getLastParams()[0]);
	}

	for(int tstep = 0; tstep < timeSteps; ++tstep){

		// if this is the first call to thermalTimestep and we're supposed to start with steady-state initial conditions temporarily switch to steady state mode ...
		bool tmpSteadyState = false;
		if( mThermalVars.steadyStateInitialCondition && solver.compute_steady_state==false && tstep==0 ){
			tmpSteadyState = true;
			solver.compute_steady_state = true;
		}

		thermalTimestep();

		if( tmpSteadyState ) solver.compute_steady_state = false;

		mTemperatureHistory[tstep] = mThermalData.currentValueVector;

		if( mThermalData.objFcn->needUpdateTargets())
			mThermalData.objFcn->updateTargetsWithData(mThermalData.currentValueVector,tstep);
	}

	if(mThermalVars.exportEnabled)
	{
		RenderScene* _scene = Common::getInstance().getRenderSystem()->getMainScene();
		std::filesystem::path sceneFile(_scene->getSceneFileName());
		std::string fnameNoExt = sceneFile.filename().string();
		if( fnameNoExt.find('.')!=fnameNoExt.npos ) fnameNoExt = fnameNoExt.substr(0,fnameNoExt.find_first_of('.'));
		std::string path = sceneFile.parent_path().string() + std::string(1,sceneFile.preferred_separator) + fnameNoExt + "_temperatures.m";
		std::ofstream outfile(path);
		outfile << "t = zeros(" << mTemperatureHistory.size() << "," << mThermalData.currentValueVector.size() << ");" << std::endl;
		for (int tstep = 0; tstep < mTemperatureHistory.size(); ++tstep){
			outfile << "t(" << (tstep+1) << ",:) = [ " << mTemperatureHistory[tstep].transpose() << " ];" << std::endl;
		}
		outfile.close();
		spdlog::info("exported temperature data to {}", path);
		// write transport matrix to matlab file
		path = sceneFile.parent_path().string() + std::string(1, sceneFile.preferred_separator) + fnameNoExt + "_transport.m";
		outfile.open(path);
		int transportRows = mThermalTransport.getTransportMatrix().rows();
		int transportCols = mThermalTransport.getTransportMatrix().cols();
		outfile << "T = zeros(" << transportRows << "," << transportCols << ");" << std::endl;
		for (int rstep = 0; rstep < transportRows; ++rstep) {	
			outfile << "T(" << (rstep + 1) << ",:) = [" << mThermalTransport.getTransportMatrix().row(rstep) << "];" << std::endl;
		}
		outfile.close();
		export_m(sceneFile.parent_path().string() + std::string(1,sceneFile.preferred_separator) + fnameNoExt + "_geometry.m");
		spdlog::info("exported transport matrix to {}", path);
	}
}

double ThermalRenderer::runAdjointSim(Vec& dp){
	int timeSteps = mThermalVars.timeSteps;
	double timeScale = 1.0;
	if( solver.compute_steady_state ) timeSteps=1;
	else timeScale = 1.0/((double)timeSteps); //  1/timeSteps is better in terms of comparable values between transient and steady state sims than simply the time step size

	Vec zeroVec;
	zeroVec.resizeLike( mThermalData.currentValueVector );
	zeroVec.setZero();

	mAdjointStateHistory.assign(timeSteps, zeroVec );

	SCALAR objValue = 0.0;
	Vec adjState = zeroVec;
	Vec adjSource;

	//dp.resize(mThermalData.params->getNumberOfParams());
	dp.setZero();

	for(int tstep = timeSteps-1; tstep >=0; --tstep){
		// evaluate objective for current time
		objValue += timeScale * mThermalData.objFcn->eval(mTemperatureHistory[tstep], adjSource, tstep);

		if( dp.size()>0 ){ // skip if gradient is empty
			if(!solver.compute_steady_state){
				adjSource *= solver.step_size; // scale by timestep
				adjSource += mThermalData.lumpedMassMatrix.asDiagonal() * adjState; // add mass term
			}


			adjointTimestep(tstep, adjState, adjSource);

			mAdjointStateHistory[tstep] = timeScale * adjState;

			// if this is the last call to adjointTimestep and we've started with steady-state initial conditions temporarily switch to steady state mode ...
			if( mThermalVars.steadyStateInitialCondition && solver.compute_steady_state==false && tstep==0 ){
				solver.compute_steady_state = true;

				adjSource = mThermalData.lumpedMassMatrix.asDiagonal() * adjState;
				adjointTimestep(tstep, adjState, adjSource);

				mAdjointStateHistory[tstep] += timeScale/solver.step_size * adjState;

				solver.compute_steady_state = false;
			}
		}
	}

	spdlog::info("temperature objective = {}", objValue);
	if( mThermalData.params->isNopTestActive() ){
		mThermalData.objectiveGradX.setZero();
		Mat copy = mThermalTransport.getTransportMatrix();
		copy.setIdentity(); copy*=1e-8;
		//fwd sim does: setTransportMatrix( copy + mThermalData.params->getLastParams()[0] * 1e-8 * Mat::Identity(copy.rows(),copy.cols()) );
		for(int tstep = 0; tstep < timeSteps; ++tstep){
			Vec tmp = mTemperatureHistory[tstep];
			tmp = tmp.array().pow(4.0).eval();
			dp -= mAdjointStateHistory[tstep].transpose() * (copy * tmp);
		}
		spdlog::info("nop test grad");
	}else
	if( dp.size()>0 ){ // skip if gradient is empty
		mThermalTransport.computeAdjoint(mGetStcBuffer(), mDevice, mThermalScene, mThermalData, mThermalVars, mTemperatureHistory, mAdjointStateHistory);
	}
	// evaluate soft constraints
	SCALAR constrValue = mThermalData.constraints->evaluateAndAddToGradient(mThermalData.objectiveGradX);
	objValue += constrValue;
	spdlog::info("constraint penalty = {}", constrValue);

	if( dp.size()>0 ){ // skip if gradient is empty
		mThermalData.params->projectGradient( dp, mThermalData.objectiveGradX );
	}

	spdlog::info("obj = {}; ||dp|| = {};", objValue, dp.norm());
	spdlog::info("dp = [ {} ];", eigenToStr(dp.transpose()));

	mThermalGui.objectiveValue = objValue;
	mThermalGui.objectiveValueMin = std::min(mThermalGui.objectiveValueMin, (float)objValue);
	mThermalGui.objectiveValueMax = std::max(mThermalGui.objectiveValueMax, (float)objValue);
	mThermalGui.objectiveValues.emplace_back((float)objValue);

	float dp_norm = dp.norm();
	mThermalGui.gradientNorm = dp_norm;
	mThermalGui.gradientNormMin = std::min(mThermalGui.gradientNormMin, dp_norm);
	mThermalGui.gradientNormMax = std::max(mThermalGui.gradientNormMax, dp_norm);
	mThermalGui.gradientNorms.emplace_back(dp_norm);

	if(mThermalVars.exportEnabled)
	{
		RenderScene* _scene = Common::getInstance().getRenderSystem()->getMainScene();
		std::filesystem::path sceneFile(_scene->getSceneFileName());
		std::string fnameNoExt = sceneFile.filename().string();
		if( fnameNoExt.find('.')!=fnameNoExt.npos ) fnameNoExt = fnameNoExt.substr(0,fnameNoExt.find_first_of('.'));
		std::string path = sceneFile.parent_path().string() + std::string(1,sceneFile.preferred_separator) + fnameNoExt + "_adjointStates.m";
		std::ofstream outfile(path);
		outfile << "l = zeros(" << mAdjointStateHistory.size() << "," << mThermalData.currentValueVector.size() << ");" << std::endl;
		for (int tstep = 0; tstep < mAdjointStateHistory.size(); ++tstep){
			outfile << "l(" << (tstep+1) << ",:) = [ " << mAdjointStateHistory[tstep].transpose() << " ];" << std::endl;
		}
		outfile << std::endl << "gradX = [ " << mThermalData.objectiveGradX << " ];" << std::endl;
		outfile.close();
		spdlog::info("exported adjoint state data to {}", path);
	}

	return objValue;
}

void ThermalRenderer::runGeomOptim(bool onlyFDcheck){

	spdlog::stopwatch sw;

	if( mThermalData.params->getNumberOfParams()==0 ) return;
	Vec p,dp;
	dp.resize( mThermalData.params->getNumberOfParams() );
	dp.setZero();
	p = mThermalData.params->getLastParams();
	double obj = 0.0;

	if( mThermalData.objFcn->nTimeStepsWithTargets() != (solver.compute_steady_state? 1 : mThermalVars.timeSteps) ) return;

	Eigen::VectorXd objectiveHistory, gradNormHistory;
	objectiveHistory.resize(std::max(1,mThermalVars.maxSteps));
	gradNormHistory.resizeLike(objectiveHistory);

	if( onlyFDcheck ){
		if( mThermalVars.fdMode==0 ){ // standard central FD with specified step size
			Vec grad,fd_grad;
			grad.resizeLike( p );
			fd_grad.resizeLike( p );
			finiteDifferenceApprox(fd_grad, p, mThermalVars.fdStep);

			updateGeometryWithParameters(p);
			runForwardSim();
			runAdjointSim(grad);

			spdlog::info("fd_grad=[{}]", eigenToStr(fd_grad.transpose()));
			spdlog::info("aj_grad=[{}]",    eigenToStr(grad.transpose()));
			spdlog::info("max_abs_err = {}", (grad-fd_grad).cwiseAbs().maxCoeff());
		}else
		if( mThermalVars.fdMode==1 ){ // central FD over range of steps around selected step size 
			const int nSteps = 11;
			const double stepFactor = sqrt(10.0);
			Vec stepSizes;
			stepSizes.resize(nSteps);
			double nextStep = mThermalVars.fdStep * pow(stepFactor,nSteps/2);
			for(int stp=0; stp<nSteps; ++stp){
				stepSizes(stp) = nextStep;
				nextStep /= stepFactor;
			}
			spdlog::debug("FD step range: {}", eigenToStr(stepSizes.transpose()));

			Vec grad,fd_grad;
			std::vector<Vec> fd_grads;
			grad.resizeLike( p );
			fd_grad.resizeLike(p);
			fd_grads.assign(nSteps, fd_grad);
			for(int stp=0; stp<nSteps; ++stp){
				finiteDifferenceApprox(fd_grad, p, stepSizes(stp));
				fd_grads[stp] = fd_grad;
			}

			updateGeometryWithParameters(p);
			runForwardSim();
			runAdjointSim(grad);

			{
				RenderScene* _scene = Common::getInstance().getRenderSystem()->getMainScene();
				std::filesystem::path sceneFile(_scene->getSceneFileName());
				std::string fnameNoExt = sceneFile.filename().string();
				if( fnameNoExt.find('.')!=fnameNoExt.npos ) fnameNoExt = fnameNoExt.substr(0,fnameNoExt.find_first_of('.'));
				std::string path = sceneFile.parent_path().string() + std::string(1,sceneFile.preferred_separator) + fnameNoExt;
				path.append("_FDrange.m");
				std::ofstream outfile(path);
				outfile << "ad_grad = [ " <<  grad.transpose() << " ];" << std::endl;
				for(int stp=0; stp<nSteps; ++stp){
					outfile << "fd_grad(" << (stp+1) << ",:) = [" << fd_grads[stp].transpose() << "];" << std::endl;
				}
				outfile << "fdStepSizes = [ "   << stepSizes.transpose() << " ];" << std::endl;
				outfile.close();
				spdlog::info("exported FD results to {}", path);
			}
		}else
		if( mThermalVars.fdMode==2 ){ // only apply FD approx to transport op, run regular adjoint sim for the rest ...
			int timeSteps = mThermalVars.timeSteps;
			if(solver.compute_steady_state) timeSteps=1;
			
			if( mTemperatureHistory.size() != timeSteps || mAdjointStateHistory.size() != timeSteps ){
				spdlog::info("missing simulation data for testing - run forward and adjoint sim first");
				return;
			}

			const float fdStepSize = mThermalVars.fdStep;
			Vec dp_fd; dp_fd.resizeLike(dp); dp_fd.setZero();

			for(int dim = 0; dim < p.size(); ++dim){
				SCALAR p0 = p[dim];

				p[dim] = p0 + 0.5*fdStepSize;
				updateGeometryWithParameters(p);
				mThermalTransport.compute(mGetStcBuffer(), mDevice, mThermalScene, mThermalData, mThermalVars, solver.mode);
				Mat Tplus = mThermalTransport.getTransportMatrix();

				p[dim] = p0 - 0.5*fdStepSize;
				updateGeometryWithParameters(p);
				mThermalTransport.compute(mGetStcBuffer(), mDevice, mThermalScene, mThermalData, mThermalVars, solver.mode);
				Mat Tminus = mThermalTransport.getTransportMatrix();

				//spdlog::info("T-op FD p[{}] = ");
				//cout << ((Tplus-Tminus)/fdStepSize) << endl;

				for(int tstep=0; tstep < timeSteps; ++tstep){
					Vec t4 = mTemperatureHistory[tstep];
					t4 = t4.array().pow(4.0).eval();
					dp_fd[dim] = -t4.transpose() * ((Tplus-Tminus)/fdStepSize) * mAdjointStateHistory[tstep];
				}

				p[dim] = p0;
			}
			updateGeometryWithParameters(p);

			mThermalTransport.computeAdjoint(mGetStcBuffer(), mDevice, mThermalScene, mThermalData, mThermalVars, mTemperatureHistory, mAdjointStateHistory);
			mThermalData.params->projectGradient( dp, mThermalData.objectiveGradX );

			spdlog::info("fd_grad=[{}]", eigenToStr(dp_fd.transpose()));
			spdlog::info("aj_grad=[{}]",    eigenToStr(dp.transpose()));
			spdlog::info("max_abs_err = {}", (dp-dp_fd).cwiseAbs().maxCoeff());
	
		}
	}else
	for(int step=0; step<std::max(1,mThermalVars.maxSteps); ++step){ // gradient descent loop ...

		spdlog::stopwatch step_sw;

		runForwardSim(); // run forward
		obj = runAdjointSim(dp); // run adjoint
		
		if( mThermalVars.useFDforOptim ){
			finiteDifferenceApprox(dp, p, mThermalVars.fdStep); // overwrite dp with FD approximation for comparison
			spdlog::info("dp_fd = [ {} ];", eigenToStr(dp.transpose()));
		}

		objectiveHistory[step] = obj;
		gradNormHistory[step] = dp.norm();

		mThermalData.optimizer->step(p, dp, obj);

		// update parameters
		updateGeometryWithParameters(p);
		spdlog::info(" p = [ {} ];",  eigenToStr(p.transpose()));

		mThermalGui.stepSum += step_sw.elapsed();
		mThermalGui.computedStepCount += 1;
		

		if (mThermalVars.exportIterationCount > 0) {
			string path = get_scene_path_wo_ext("opt-export") + "-iter-" + fmt::format("{:0>4}", mThermalGui.computedStepCount);
			spdlog::info("Exporting iteration {} to {} ... ", mThermalGui.computedStepCount, path);

			string scene_file_path = get_scene_path_wo_ext("opt-export") + "-iter-" + fmt::format("{:0>4}", mThermalGui.computedStepCount);
			export_to_csv_float_vec(scene_file_path + "-obj-vals.csv", mThermalGui.objectiveValues);
			export_to_csv_float_vec(scene_file_path + "-kelvin-vals.csv", mThermalGui.avgKelvins);

			mThermalScene.getObjects().setVertexColors(mThermalData.currentValueVector);

			Common::getInstance().exportScene(scene_file_path + ".gltf", (uint32_t)0x1);
			export_vtk(scene_file_path + ".vtk", mThermalData.currentValueVector);

			//Common::getInstance().takeScreenshot(scene_file_path + "-screenshot-ui.png", false);
			Common::getInstance().takeScreenshot(scene_file_path + "-screenshot.png", false);

			spdlog::info("... done");

			mThermalVars.exportIterationCount -= 1;
			if( mThermalVars.exportIterationCount <= 0 ){
				mThermalVars.exportIterationCount = 0;
				mThermalVars.runOptim = false;
			}
		}
	}

	mThermalGui.totalTime += sw.elapsed();

	if( mThermalVars.maxSteps > 0 ){
		mThermalVars.runOptim=false; // stop if steps limit is set
	}

	if( mThermalVars.maxSteps > 1 ){
		string path = get_scene_path_wo_ext();
		if (mThermalVars.useFDforOptim) path.append("_fd");
		if (mThermalGui.selectOptimizer == 1) path.append("_adam");
		path.append("_optimHistory.m");
		std::ofstream outfile(path);
		outfile << "objValues = [ " << objectiveHistory.transpose() << " ];" << std::endl;
		outfile << "objGradNorm = [ " << gradNormHistory.transpose() << " ];" << std::endl;
		outfile.close();
		spdlog::info("exported optimization history to {}", path);
	}
}

void ThermalRenderer::updateGeometryWithParameters(const Vec& p){
	mThermalData.params->updateGeometry(p);
	if( mThermalData.params->isNopTestActive() ){return; }
	// assume CPU mesh data has changed, update GPU data ...
	scene_s _scene = Common::getInstance().getRenderSystem()->getMainScene()->getSceneData();
	mThermalData.recomputeAreaVectors(_scene);
	blas_gpu.loadScene(&mGetStcBuffer(), _scene); 
	mThermalTransport.load(mGetStcBuffer(), mDevice, blas_gpu, mVkData->geometryDataBuffer, _scene, mThermalScene, mThermalData, mThermalVars);
	mThermalTransport.uploadValueVector(mGetStcBuffer(), mThermalData.currentValueVector );
	mDevice->waitIdle();
}

void ThermalRenderer::finiteDifferenceApprox(Vec& fd_grad, Vec& p, const SCALAR fdStepSize){
	fd_grad.resizeLike(p); fd_grad.setZero();
	Vec unused;
	for(int dim = 0; dim < p.size(); ++dim){
		SCALAR p0 = p[dim];

		p[dim] = p0 + 0.5*fdStepSize;
		updateGeometryWithParameters(p);
		runForwardSim();
		SCALAR o_p = runAdjointSim(unused);

		p[dim] = p0 - 0.5*fdStepSize;
		updateGeometryWithParameters(p);
		runForwardSim();
		SCALAR o_m = runAdjointSim(unused);

		fd_grad[dim] = (o_p - o_m)/fdStepSize;
		p[dim]=p0;
	}
	updateGeometryWithParameters(p);
}

void ThermalRenderer::visGrad(unsigned int paramIdx) {
	/**
	if(!solver.compute_steady_state){
		spdlog::error("grad vis not implemented yet for transient sim, select steady state sim!");
		return;
	}

	// simply show x-component of per-vertex gradient: d(objective)/dx
	//mThermalTransport.uploadValueVector(mGetStcBuffer(), mThermalData.objectiveGradX.col(0) );
	//if( mThermalGui.enableAutoAdjust ){
	//	mThermalGui.displayRangeMin = mThermalData.objectiveGradX.col(0).minCoeff();
	//	mThermalGui.displayRangeMax = mThermalData.objectiveGradX.col(0).maxCoeff();
	//}


	Vec p = mThermalData.params->getLastParams();
	const float fdStepSize = mThermalVars.fdStep;
	Mat dT; Vec ldT;
	if( mThermalVars.useFDforOptim ){ // FD for selected parameter
		SCALAR p0 = p[paramIdx];
		p[paramIdx] = p0 + 0.5*fdStepSize;
		updateGeometryWithParameters(p);
		mThermalTransport.compute(mGetStcBuffer(), mDevice, mThermalScene, mThermalData, mThermalVars, solver.mode);
		Mat Tplus = mThermalTransport.getTransportMatrix();
		p[paramIdx] = p0 - 0.5*fdStepSize;
		updateGeometryWithParameters(p);
		mThermalTransport.compute(mGetStcBuffer(), mDevice, mThermalScene, mThermalData, mThermalVars, solver.mode);
		Mat Tminus = mThermalTransport.getTransportMatrix();
		dT = (Tplus - Tminus)/fdStepSize;
		p[paramIdx] = p0;
		updateGeometryWithParameters(p);
		ldT = mAdjointStateHistory[0].transpose()*dT; // seems to "look" more reasonable than columns/rows out of dT (FD has lots of noise there)

	}else{
		Vec dp; dp.resizeLike(p); dp.setZero();
		ldT.resizeLike( mThermalData.currentValueVector );
		dT.resize( ldT.size(), ldT.size() ); dT.setZero();
		for(unsigned int i=0; i<ldT.size();++i){
			mTemperatureHistory[0].setZero();
			mTemperatureHistory[0][i] = 1.0;
			
			if( mThermalGui.visualization==1 ){ // col of dT
				mAdjointStateHistory[0].setZero();
				mAdjointStateHistory[0][mThermalGui.sourceVertexId]=1.0;
			}

			mThermalTransport.computeAdjoint(mGetStcBuffer(), mDevice, mThermalScene, mThermalData, mThermalVars, mTemperatureHistory, mAdjointStateHistory);
			mThermalData.params->projectGradient( dp, mThermalData.objectiveGradX );
			ldT[i] = dp[paramIdx];
			if( mThermalGui.visualization==1 ) dT(i,mThermalGui.sourceVertexId) = dp[paramIdx]; // col of dT
		}
	}

	mThermalTransport.uploadValueVector(mGetStcBuffer(), ldT );
	mThermalTransport.uploadTransportMatrix(mGetStcBuffer(), dT.transpose().eval() );
	if( mThermalGui.enableAutoAdjust && mThermalGui.visualization==0 ){
		mThermalGui.displayRangeMin = dT.row(mThermalGui.sourceVertexId).minCoeff();
		mThermalGui.displayRangeMax = dT.row(mThermalGui.sourceVertexId).maxCoeff();
	}
	if( mThermalGui.enableAutoAdjust && mThermalGui.visualization==1 ){
		mThermalGui.displayRangeMin = dT.col(mThermalGui.sourceVertexId).minCoeff();
		mThermalGui.displayRangeMax = dT.col(mThermalGui.sourceVertexId).maxCoeff();
		spdlog::info("grad vis mode 1 vtx {} data:", mThermalGui.sourceVertexId);
		std::cout << std::endl << dT.col(mThermalGui.sourceVertexId).transpose() << std::endl;
	}

	if( mThermalGui.enableAutoAdjust && mThermalGui.visualization==2 ){
		mThermalGui.displayRangeMin = ldT.minCoeff();
		mThermalGui.displayRangeMax = ldT.maxCoeff();
	}
	/*/

	
	// simple option: run one step of grad desc (either using adjoint or FD grads) then record the difference of vertex positions before/after per vertex and Glyph plot / colourmap in paraview ...

	Vec dp,p0 = mThermalData.params->getLastParams();
	dp.resizeLike(p0); dp.setZero();
	Mat x0,x_adj,x_fd; 

	updateGeometryWithParameters(p0);
	writeVertexPositionsToMatrix(x0);

	runForwardSim();
	runAdjointSim(dp);
	Vec gradX = mThermalData.objectiveGradX;

	updateGeometryWithParameters( p0 + mThermalData.optimizer->getStepSize() * dp );
	writeVertexPositionsToMatrix(x_adj);
	x_adj = (x_adj-x0).eval();
	//spdlog::info("vtx offsets adj \n{}", x_adj);

	finiteDifferenceApprox(dp, p0, mThermalVars.fdStep);
	updateGeometryWithParameters( p0 + mThermalData.optimizer->getStepSize() * dp );
	writeVertexPositionsToMatrix(x_fd);
	x_fd = (x_fd-x0).eval();
	//spdlog::info("vtx offsets fd\n{}", x_fd);

	Vec gradX_fd; gradX_fd.resizeLike(gradX); gradX_fd.setZero();
	if(1){	// full FD for vertex positions (conditioned below on only vertices that are non-zero in x_adj)
		Vec empty;
		updateGeometryWithParameters(p0);
		runForwardSim();
		double obj0 = runAdjointSim(empty);

		{
			scene_s scene = Common::getInstance().getRenderSystem()->getMainScene()->getSceneData();
			unsigned int vertex_offset = 0;
			for (RefModel_s* refModel : scene.refModels){
				for(RefMesh_s* refMesh : refModel->refMeshes){
					Mesh* m = refMesh->mesh;
					vertex_s* verts = m->getVerticesArray();
					for(unsigned int id=0; id < m->getVertexCount(); ++id){
						spdlog::info("\n********\n****  FD vtx {} of {} \n********", id+vertex_offset , gradX_fd.rows());
						glVec4 v0 = verts[id].position;
						/*if( x_adj.row(id+vertex_offset).squaredNorm()>1e-10 )*/ for(unsigned int dim=0; dim<3; ++dim){
							verts[id].position[dim] = v0[dim] + 0.5*mThermalVars.fdStep;
							
							mThermalData.recomputeAreaVectors(scene);
							blas_gpu.loadScene(&mGetStcBuffer(), scene); 
							mThermalTransport.load(mGetStcBuffer(), mDevice, blas_gpu, mVkData->geometryDataBuffer, scene, mThermalScene, mThermalData, mThermalVars);
							mThermalTransport.uploadValueVector(mGetStcBuffer(), mThermalData.currentValueVector );
							mDevice->waitIdle();

							runForwardSim();
							double obj_p = runAdjointSim(empty);

							verts[id].position[dim] = v0[dim] - 0.5*mThermalVars.fdStep;
							
							mThermalData.recomputeAreaVectors(scene);
							blas_gpu.loadScene(&mGetStcBuffer(), scene); 
							mThermalTransport.load(mGetStcBuffer(), mDevice, blas_gpu, mVkData->geometryDataBuffer, scene, mThermalScene, mThermalData, mThermalVars);
							mThermalTransport.uploadValueVector(mGetStcBuffer(), mThermalData.currentValueVector );
							mDevice->waitIdle();

							runForwardSim();
							double obj_m = runAdjointSim(empty);

							gradX_fd( id+vertex_offset , dim ) = (obj_p - obj_m) / mThermalVars.fdStep;
							verts[id].position[dim] = v0[dim];
						}
						verts[id].position = v0;
					}
					vertex_offset += m->getVertexCount(); // track offsets
				}
			}
		}
	}

	{	// write to vtk file
		scene_s scene = Common::getInstance().getRenderSystem()->getMainScene()->getSceneData();

		unsigned int nNodes = mThermalData.vertexAreaVector.size();
		unsigned int nElems = mThermalData.triangleAreaVector.size();
		unsigned int cellType = 5, nNodesPerElem = 3;

		string path = get_scene_path_wo_ext();
		path.append("_gd_step.vtk");
		std::ofstream out(path);

		if( !out.is_open() ){ spdlog::error("Can not open file for VTK export.");
		}else{
			out.precision(12);

			//write header
			out << "# vtk DataFile Version 2.0" << std::endl;
			out << "VTK exported mesh" << std::endl;
			out << "ASCII" << std::endl;
			out << "DATASET UNSTRUCTURED_GRID" << std::endl;

			//node coordinates
			out << "POINTS " << nNodes << " double" << std::endl;

			for(RefModel_s* refModel : scene.refModels){
				glm::mat4 model_matrix = refModel->model_matrix;
				for(RefMesh_s* refMesh : refModel->refMeshes){
					Mesh* m = refMesh->mesh;
					for(const auto& v : *m->getVerticesVector()){
						glm::vec4 _v = model_matrix * v.position;
						out << _v.x << " " << -_v.z << " " << _v.y << std::endl; // switch Y and Z
					}
				}
			}

			//cells
			int vertex_offset = 0;
			out << "CELLS " << nElems << " " << (1 + nNodesPerElem) * nElems << std::endl;
			for(RefModel_s* refModel : scene.refModels){
				for(RefMesh_s* refMesh : refModel->refMeshes){
					Mesh* m = refMesh->mesh;
					int ic = 0;
					auto indices = *m->getIndicesVector();
					for(int i = 0; i < m->getPrimitiveCount(); i++)
						out << "3 " << vertex_offset + indices[ic++] << " " << vertex_offset + indices[ic++] << " " << vertex_offset + indices[ic++] << std::endl;
					vertex_offset += m->getVertexCount();
				}
			}

			//cell types
			out << "CELL_TYPES " << nElems << std::endl;
			for (int i = 0; i < nElems; ++i) out << cellType << std::endl;

			out << "POINT_DATA " << nNodes << std::endl;

			out << "VECTORS adj_grad_step double" << std::endl;
			vertex_offset = 0;
			for(RefModel_s* refModel : scene.refModels){
				for(RefMesh_s* refMesh : refModel->refMeshes){
					Mesh* m = refMesh->mesh;
					for(int i = vertex_offset; i < (vertex_offset + m->getVertexCount()); i++)
						out << x_adj( i, 0 ) << " " << -x_adj( i, 2 ) << " " << x_adj( i, 1 ) << std::endl; // switch Y and Z
					vertex_offset += m->getVertexCount();
				}
			}
			out << "VECTORS fd_grad_step double" << std::endl;
			vertex_offset = 0;
			for(RefModel_s* refModel : scene.refModels){
				for(RefMesh_s* refMesh : refModel->refMeshes){
					Mesh* m = refMesh->mesh;
					for(int i = vertex_offset; i < (vertex_offset + m->getVertexCount()); i++)
						out << x_fd( i, 0 ) << " " << -x_fd( i, 2 ) << " " << x_fd( i, 1 ) << std::endl; // switch Y and Z
					vertex_offset += m->getVertexCount();
				}
			}
			out << "VECTORS vtx_grad double" << std::endl;
			vertex_offset = 0;
			for(RefModel_s* refModel : scene.refModels){
				for(RefMesh_s* refMesh : refModel->refMeshes){
					Mesh* m = refMesh->mesh;
					for(int i = vertex_offset; i < (vertex_offset + m->getVertexCount()); i++)
						out << gradX( i, 0 ) << " " << -gradX( i, 2 ) << " " << gradX( i, 1 ) << std::endl; // switch Y and Z
					vertex_offset += m->getVertexCount();
				}
			}
			out << "VECTORS vtx_grad_fd double" << std::endl;
			vertex_offset = 0;
			for(RefModel_s* refModel : scene.refModels){
				for(RefMesh_s* refMesh : refModel->refMeshes){
					Mesh* m = refMesh->mesh;
					for(int i = vertex_offset; i < (vertex_offset + m->getVertexCount()); i++)
						out << gradX_fd( i, 0 ) << " " << -gradX_fd( i, 2 ) << " " << gradX_fd( i, 1 ) << std::endl; // switch Y and Z
					vertex_offset += m->getVertexCount();
				}
			}
		}
	}
	/**/
}

void ThermalRenderer::writeVertexPositionsToMatrix(Mat& pos){
	pos.resize( mThermalData.vertexAreaVector.size(), 3 );
	pos.setZero();
	unsigned int idx = 0;
	scene_s scene = Common::getInstance().getRenderSystem()->getMainScene()->getSceneData();
	for (RefModel_s* refModel : scene.refModels) {
		glm::mat4 model_matrix = refModel->model_matrix;
		for (RefMesh_s* refMesh : refModel->refMeshes) {
			Mesh* m = refMesh->mesh;
			for(const auto& v : *m->getVerticesVector()){
				glm::vec4 _v = model_matrix * v.position;
				pos( idx, 0) = _v.x;
				pos( idx, 1) = _v.y;
				pos( idx, 2) = _v.z;
				++idx;
			}
		}
	}
	if( idx != pos.rows() ) spdlog::error("size mismatch in vertex position matrix");
}
