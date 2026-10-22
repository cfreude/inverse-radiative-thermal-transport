#pragma once
#include <tamashii/engine/render/render_backend_implementation.hpp>
#include <tamashii/renderer_vk/render_backend.hpp>
#include <rvk/rvk.hpp>

#include "thermal_common.hpp"
#include "thermal_sky.hpp"
#include "thermal_scene.hpp"
#include "thermal_transport.hpp"
#include "thermal_solver.hpp"
#include "thermal_gui.hpp"

#include "../../../assets/shader/raytracing_thermal/defines.h"
#include <tamashii/renderer_vk/convenience/texture_to_gpu.hpp>
#include <tamashii/renderer_vk/convenience/geometry_to_gpu.hpp>

using namespace Eigen;
using namespace tamashii;

class ThermalRenderer final : public tamashii::RenderBackendImplementation {
public:
						ThermalRenderer(rvk::LogicalDevice* aDevice, rvk::Swapchain* aSwapchain, const uint32_t aFrameCount,
							std::function<uint32_t()> aCurrentFrame,
							std::function<rvk::CommandBuffer* ()> aGetCurrentCmdBuffer,
							std::function<rvk::SingleTimeCommand()> aGetStcBuffer) :
							mDevice(aDevice), mSwapchain(aSwapchain), mFrameCount(aFrameCount),
							mCurrentFrame(std::move(aCurrentFrame)),
							mGetCurrentCmdBuffer(std::move(aGetCurrentCmdBuffer)),
							mGetStcBuffer(std::move(aGetStcBuffer)), td_gpu(aDevice), blas_gpu(aDevice),
							min_aabb(),	max_aabb(),
							boundingSphereCenter(),	boundingSphereRadius(0),
							mThermalTransport(aDevice)
						{
						}

						~ThermalRenderer() override = default;

	const char*			getName() override { return THERMAL_RENDERER_NAME; }
	void				windowSizeChanged(int aWidth, int aHeight) override;

	// implementation preparation
	void				prepare(renderInfo_s* aRenderInfo) override;
	void				destroy() override;

	void				prepareData(renderInfo_s* aRenderInfo);
	void				loadData(scene_s scene);

	// scene
	void				sceneLoad(scene_s scene) override;
	void				_sceneLoad();
	void				sceneUnload(scene_s scene) override;
	void				_sceneUnload();

	// frame
	void				drawView(viewDef_s* aViewDef) override;
	void				drawUI(uiConf_s* uc) override;

	//thermal
	void				thermalInit(scene_s scene);
	void				thermalTimestep();
	void				adjointTimestep(int tstep, Vec& adjointState, Vec& adjointSource);
	void				computeTransportMatrix();
	void				updateSky(float _hour);

	void				resetSimulation();
	void				runForwardSim();
	double				runAdjointSim(Vec& dp);
	void				runGeomOptim(bool onlyFDcheck=false);
	void				visGrad(unsigned int paramIdx);

	static std::tuple<glm::vec3, glm::vec3> computeSceneAABB(scene_s _scene);
	static std::tuple<glm::vec3, float> computeSceneBoundingSphere(glm::vec3 _min_aabb, glm::vec3 _max_aabb);

	void				computeSceneAABB();
	glm::vec4			getBoundingSphere();

	void				getKelvin(float* _arr, unsigned int _count);
	void				getValue(float* _arr, unsigned int _count, unsigned int _type);
	void				setKelvin(float* _arr, unsigned int _triangle_count, unsigned int _vertex_offset, bool _sky_values);
	void				setTimeStep(float _hours);
	void				setTime(float _hours);
	float				getTimeHours();
	void				setSteadState(bool _enabled) {
		spdlog::info("set_steady_state({})", _enabled);
		solver.compute_steady_state = _enabled;
	};
	void				setSolverMode(int _v) { solver.mode = _v; };
	void				setRayBatchCount(unsigned int _ray_count, unsigned int _batch_count) { mThermalVars.threadsPerTriangle = _ray_count; mThermalVars.raysPerThread = _batch_count; };
	void				temporaryDisableTransportCompute() { mDisableCompute = true; };

	unsigned int		sky_vertex_offset = 0;
	unsigned int		sky_vertex_count = 0;
	SCALAR				sky_min_kelvin = 0;
			
	const std::vector<ObjectStatistics_s>& getThermalStatsArray() { return mThermalData.objectStatistics; };

private:
	void updateGeometryWithParameters(const Vec& p);

	void finiteDifferenceApprox(Vec& g, Vec& p, const SCALAR fdStepSize);

	void writeVertexPositionsToMatrix(Mat& pos);

	rvk::LogicalDevice*										mDevice;
	rvk::Swapchain*											mSwapchain;
	uint32_t												mFrameCount;
	std::function<uint32_t()>								mCurrentFrame;
	std::function<rvk::CommandBuffer* ()>					mGetCurrentCmdBuffer;
	std::function<rvk::SingleTimeCommand()>					mGetStcBuffer;

	TextureDataVulkan										td_gpu;
	GeometryDataBlasVulkan									blas_gpu;

	// data that is frame independent
	struct VkData {
		explicit VkData(rvk::LogicalDevice* aDevice) : rtshader(aDevice), rtpipeline(aDevice),
		pointLightBuffer(aDevice), geometryDataBuffer(aDevice) {}

		rvk::RTShader										rtshader;
		rvk::RTPipeline										rtpipeline;

		rvk::Buffer											pointLightBuffer;

		rvk::Buffer											geometryDataBuffer; // index to index/vertex buffer and material (tex ind) !always align!
	};
	// data that is frame dependent
	struct VkFrameData {
		explicit VkFrameData(rvk::LogicalDevice* aDevice) : instanceDataBuffer(aDevice), top(aDevice),
			globalDescriptor(aDevice), globalUniformBuffer(aDevice),
			globalAuxUniformBuffer(aDevice), rtImage(aDevice) {}


		rvk::Buffer											instanceDataBuffer;
		// tlas
		rvk::TopLevelAS										top;
		// descriptor
		rvk::Descriptor										globalDescriptor;
		// buffers
		rvk::Buffer											globalUniformBuffer;
		rvk::Buffer											globalAuxUniformBuffer;
		rvk::Image											rtImage;
	};
	
	std::optional<VkData>									mVkData;
	std::vector<VkFrameData>								mVkFrameData;

	ThermalSolver											solver;

	std::vector<Vector3f>									sunDirections;
	bool													createSunAndSky = false;
	glm::vec3												min_aabb, max_aabb;
	glm::vec3												boundingSphereCenter;
	float													boundingSphereRadius;

	bool mDisableCompute = false;

	// max exact interger for float  = 16,777,217 (224 + 1)

	ThermalScene mThermalScene;
	ThermalSky mThermalSky;
	ThermalData mThermalData;
	ThermalTransport mThermalTransport;
	ThermalGui mThermalGui;

	ThermalVars_s mThermalVars;

	std::vector<Vec> mTemperatureHistory;
	std::vector<Vec> mAdjointStateHistory;
};


