#pragma once
#include <glm/glm.hpp>

#include <rvk/rvk.hpp>
#include <tamashii/engine/common/vars.hpp>
#include <tamashii/engine/scene/render_scene.hpp>
#include <tamashii/renderer_vk/convenience/geometry_to_gpu.hpp>
#include <tamashii/engine/render/render_backend_implementation.hpp>

#include "../../../assets/shader/raytracing_thermal/defines.h"

#include "thermal_common.hpp"
#include "thermal_objects.hpp"
#include "thermal_data.hpp"
#include "thermal_scene.hpp"
#include "thermal_objects.hpp"
#include "thermal_aca_matrix.hpp"

#include <unordered_set>

T_USE_NAMESPACE

using namespace Eigen;

class ThermalTransport {

public:

	ThermalTransport(rvk::LogicalDevice* aDevice) :
		top(aDevice),
		instanceDataBuffer(aDevice),
		globalDescriptor(aDevice),
		globalUniformBuffer(aDevice),
		rtImage(aDevice),
		rt_transport_shader(aDevice),
		rt_transport_pipeline(aDevice),
		adj_transport_shader(aDevice),
		adj_transport_pipeline(aDevice),
		transportBuffer(aDevice),
		triangleAreaBuffer(aDevice),
		valueBuffer(aDevice),
		adjAdjStBuffer(aDevice),
		adjTemp4Buffer(aDevice),
		adjGradXBuffer(aDevice),
		transportCompressed(false)
		//vertexEmissionBuffer(aDevice),
		//vertexAbsorptionBuffer(aDevice)
	{}

	~ThermalTransport() = default;

	void prepare(rvk::Buffer& _geomBuffer, GeometryDataBlasVulkan& _gpuBlas);
	void load(rvk::SingleTimeCommand& _stc, rvk::LogicalDevice* _device, GeometryDataBlasVulkan& _gpuBlas, rvk::Buffer& geometryDataBuffer, scene_s& _scene, ThermalScene& _thermalScene, ThermalData& _thermalData, const ThermalVars_s& _thermalVars, bool _setup = false);
	void initAS(rvk::SingleTimeCommand& _stc, rvk::LogicalDevice* _device, GeometryDataBlasVulkan& _gpuBlas, rvk::Buffer& geometryDataBuffer, scene_s& _scene, ThermalScene& _thermalScene);
	void setupBuffers(unsigned int _vertex_count, unsigned int _triangle_count);
	void compute(rvk::SingleTimeCommand& stc, rvk::LogicalDevice* _device, ThermalScene& _thermalScene, ThermalData& _thermalData, const ThermalVars_s& _thermalVars, int mode);
	void computeAdjoint(rvk::SingleTimeCommand& stc, rvk::LogicalDevice* _device, ThermalScene& _thermalScene, ThermalData& _thermalData, const ThermalVars_s& _thermalVars, std::vector<Vec>& _temperatureHistory, std::vector<Vec>& _adjointStateHistory);
	//void recompute(viewDef_s* aViewDef, rvk::SingleTimeCommand& stc, rvk::LogicalDevice* _device, GeometryDataBlasVulkan& _gpuBlas, ThermalScene& _thermalScene, ThermalData& _thermalData, unsigned int batchCount, unsigned int rayCount, int mode);

	void initTransportBuffer(rvk::SingleTimeCommand& _stc, unsigned int _vertex_count);
	void initAuxilaryBuffer(rvk::SingleTimeCommand& _stc, unsigned int _vertex_count, const ThermalVars_s& _thermalVars);
	void initInstanceBuffer(rvk::SingleTimeCommand& _stc, scene_s& _scene, ThermalScene& _thermalScene);
	void initTriangleAreaBuffer(rvk::SingleTimeCommand& _stc, ThermalData& _thermalData, unsigned int traingle_count);
	//void initAbsorptionEmissionBuffers(rvk::SingleTimeCommand& _stc, ThermalData& _thermalData, unsigned int vertex_count);
	void initKelvinBuffer(rvk::SingleTimeCommand& _stc, const ThermalData& _thermalData, unsigned int vertex_count);

	void setAuxiliaryUbo(AuxiliaryUbo& _aux_ubo, unsigned int _vertex_count, const ThermalVars_s& _thermalVars, unsigned int _timeSteps=0);

	void uploadTransportMatrix(rvk::SingleTimeCommand& stc, const Mat& _transportMatrix);
	void uploadValueVector(rvk::SingleTimeCommand& stc, const Vec& _values);

	void unload();

	void printTransportMatrixSums();

	void compressTransportMatrix(int iters, int compressionAlgorithm);
	void collumnSumCorrection();
	void simpleAdaptiveCrossApproximation(double tol, int iters);
	void completeAdaptiveCrossApproximation(double tol, int iters);
	void adaptiveCrossApproximationPlus(double tol, int iters);
	Eigen::Index findArgmaxNotInList(std::unordered_set<Eigen::Index>& indices, Vec& v);
	void calculateResidualRow(Vec& v_k, Eigen::Index& i_k, int k);
	void calculateResidualCol(Vec& u_k, Eigen::Index& j_k, int k);
	void resetReferenceRow(Eigen::Index& r, std::unordered_set<Eigen::Index>& prevRows);
	void resetReferenceCol(Eigen::Index& c, std::unordered_set<Eigen::Index>& prevCols);
	SCALAR& getCompressionDuration() { return compressionDuration; };
	double& getTFrobError() { return T_frob_error; };
	double& getTInfError() { return T_inf_error; };
	double& getTFrobErrorRel() { return T_frob_error_rel; };
	double& getTInfErrorRel() { return T_inf_error_rel; };

	void setTransportMatrix(const Mat& transportMatrix_) { transportMatrix = transportMatrix_; };
	const Mat& getTransportMatrix() { return transportMatrix; }
	const ACAMatrix& getACATransportMatrix() { return acaTransportMatrix; }
	const bool isCompressed() { return transportCompressed; }

	rvk::Buffer& getTransportBuffer() { return transportBuffer; }
	rvk::Buffer& getKelvinBuffer() { return valueBuffer; }

private:

	#define VK_INDEX_DATA_SIZE 1024 * 1024 * 4
	#define VK_VERTEX_DATA_SIZE 512 * 1024 * 4
	#define VK_GLOBAL_IMAGE_SIZE 128
	#define VK_INSTANCE_SIZE 256 * 4
	#define VK_GEOMETRY_SIZE 256 * 4

	EIGEN_MAKE_ALIGNED_OPERATOR_NEW

	// tlas
	rvk::TopLevelAS										top;

	rvk::Buffer											instanceDataBuffer;
	rvk::Descriptor										globalDescriptor;
	// buffers
	rvk::Buffer											globalUniformBuffer;
	rvk::Image											rtImage;

	rvk::RTShader										rt_transport_shader;
	rvk::RTPipeline										rt_transport_pipeline;

	rvk::RTShader										adj_transport_shader;
	rvk::RTPipeline										adj_transport_pipeline;

	rvk::Buffer											transportBuffer;
	rvk::Buffer											triangleAreaBuffer;

	rvk::Buffer											valueBuffer;
	rvk::Buffer											adjTemp4Buffer; // history of temperatures^4
	rvk::Buffer											adjAdjStBuffer; // history of adjoint states
	rvk::Buffer											adjGradXBuffer; // result - gradient wrt. vertex pos.
	//rvk::Buffer											vertexEmissionBuffer;
	//rvk::Buffer											vertexAbsorptionBuffer;

	Mat transportMatrix;
	ACAMatrix acaTransportMatrix;
	bool transportCompressed;
	SCALAR compressionDuration;
	double T_frob_error;
	double T_inf_error;
	double T_frob_error_rel;
	double T_inf_error_rel;
};