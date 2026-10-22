#include "thermal_transport.hpp"

#include <spdlog/stopwatch.h>
#include <GLM\common.hpp>

#include <iostream>
#include <chrono>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <string>


void ThermalTransport::prepare(rvk::Buffer& geometryDataBuffer, GeometryDataBlasVulkan& _gpuBlas)
{
	// descriptors
	// global descriptor
	globalDescriptor.reserve(8);
	globalDescriptor.addUniformBuffer(GLSL_GLOBAL_UBO_BINDING, rvk::Shader::Stage::RAYGEN);
	globalDescriptor.addStorageBuffer(GLSL_GLOBAL_INSTANCE_DATA_BINDING, rvk::Shader::Stage::RAYGEN | rvk::Shader::Stage::ANY_HIT);
	globalDescriptor.addStorageBuffer(GLSL_GLOBAL_GEOMETRY_DATA_BINDING, rvk::Shader::Stage::RAYGEN | rvk::Shader::Stage::ANY_HIT);
	globalDescriptor.addStorageBuffer(GLSL_GLOBAL_INDEX_BUFFER_BINDING, rvk::Shader::Stage::RAYGEN | rvk::Shader::Stage::ANY_HIT);
	globalDescriptor.addStorageBuffer(GLSL_GLOBAL_VERTEX_BUFFER_BINDING, rvk::Shader::Stage::RAYGEN | rvk::Shader::Stage::ANY_HIT);
	globalDescriptor.addAccelerationStructureKHR(GLSL_GLOBAL_AS_BINDING, rvk::Shader::Stage::RAYGEN);
	globalDescriptor.addStorageBuffer(GLSL_GLOBAL_AREA_DATA_BINDING, rvk::Shader::Stage::RAYGEN);
	//globalDescriptor.addStorageBuffer(GLSL_GLOBAL_EMISSION_DATA_BINDING, rvk::Shader::Stage::RAYGEN);
	//globalDescriptor.addStorageBuffer(GLSL_GLOBAL_ABSORPTION_DATA_BINDING, rvk::Shader::Stage::RAYGEN);
	globalDescriptor.addStorageBuffer(GLSL_GLOBAL_OUT_IMAGE_BINDING, rvk::Shader::Stage::RAYGEN);
	globalDescriptor.addStorageBuffer(GLSL_GLOBAL_ADJTEMP4_DATA_BINDING, rvk::Shader::Stage::RAYGEN);
	globalDescriptor.addStorageBuffer(GLSL_GLOBAL_ADJADJST_DATA_BINDING, rvk::Shader::Stage::RAYGEN);
	globalDescriptor.addStorageBuffer(GLSL_GLOBAL_ADJGRADX_DATA_BINDING, rvk::Shader::Stage::RAYGEN);	
	// set
	globalDescriptor.setBuffer(GLSL_GLOBAL_GEOMETRY_DATA_BINDING, &geometryDataBuffer);
	globalDescriptor.setBuffer(GLSL_GLOBAL_INDEX_BUFFER_BINDING, _gpuBlas.getIndexBuffer());
	globalDescriptor.setBuffer(GLSL_GLOBAL_VERTEX_BUFFER_BINDING, _gpuBlas.getVertexBuffer());
	globalDescriptor.finish(false);

	// forward tracing prep
	rt_transport_shader.addStage(rvk::Shader::Source::GLSL, rvk::Shader::Stage::RAYGEN, "./assets/shader/raytracing_thermal/thermal_tracing_forward.glsl", { "GLSL" });
	rt_transport_shader.addStage(rvk::Shader::Source::GLSL, rvk::Shader::Stage::MISS, "./assets/shader/raytracing_thermal/transport_ray.rmiss", { "GLSL" });
	rt_transport_shader.addStage(rvk::Shader::Source::GLSL, rvk::Shader::Stage::CLOSEST_HIT, "./assets/shader/raytracing_thermal/transport_ray.rchit", { "GLSL" });
	rt_transport_shader.addGeneralShaderGroup("./assets/shader/raytracing_thermal/transport_ray.rmiss");
	rt_transport_shader.addGeneralShaderGroup("./assets/shader/raytracing_thermal/thermal_tracing_forward.glsl");
	rt_transport_shader.addHitShaderGroup("./assets/shader/raytracing_thermal/transport_ray.rchit");// , "assets/shader/raytracing_thermal/transport_ray.rahit");
	rt_transport_shader.finish();

	rt_transport_pipeline.setShader(&rt_transport_shader);
	rt_transport_pipeline.addDescriptorSet({ &globalDescriptor });
	rt_transport_pipeline.finish();

	// forward tracing prep
	adj_transport_shader.addStage(rvk::Shader::Source::GLSL, rvk::Shader::Stage::RAYGEN, "./assets/shader/raytracing_thermal/thermal_tracing_adjoint.glsl", { "GLSL" });
	adj_transport_shader.addStage(rvk::Shader::Source::GLSL, rvk::Shader::Stage::MISS, "./assets/shader/raytracing_thermal/transport_ray.rmiss", { "GLSL" });
	adj_transport_shader.addStage(rvk::Shader::Source::GLSL, rvk::Shader::Stage::CLOSEST_HIT, "./assets/shader/raytracing_thermal/transport_ray.rchit", { "GLSL" });
	adj_transport_shader.addGeneralShaderGroup("./assets/shader/raytracing_thermal/transport_ray.rmiss");
	adj_transport_shader.addGeneralShaderGroup("./assets/shader/raytracing_thermal/thermal_tracing_adjoint.glsl");
	adj_transport_shader.addHitShaderGroup("./assets/shader/raytracing_thermal/transport_ray.rchit");// , "assets/shader/raytracing_thermal/transport_ray.rahit");
	adj_transport_shader.finish();

	adj_transport_pipeline.setShader(&adj_transport_shader);
	adj_transport_pipeline.addDescriptorSet({ &globalDescriptor });
	adj_transport_pipeline.finish();

}

void ThermalTransport::initAS(
	rvk::SingleTimeCommand& _stc,
	rvk::LogicalDevice* _device,
	GeometryDataBlasVulkan& _gpuBlas,
	rvk::Buffer& geometryDataBuffer,
	scene_s& _scene,
	ThermalScene& _thermalScene)
{
	top.clear();
	top.reserve(_scene.refModels.size());
	for (RefModel_s* refModel : _scene.refModels) {
		rvk::ASInstance as_instance(_gpuBlas.getBlas(refModel->model));
		glm::mat4 model_matrix = glm::transpose(refModel->model_matrix);
		as_instance.setTransform(&model_matrix[0][0]);
		if( refModel->model->getCustomProperty("traceable").getInt() ){
			refModel->mask = 0xff;
		}else{
			refModel->mask = 0x0f;
		}
		as_instance.setMask(refModel->mask);
		top.addInstance(as_instance);
	}

	top.prepare(VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR);
	_stc.begin();
	top.CMD_Build(_stc.buffer());
	_stc.end();
	// add the tlas to the descriptor and update it
	globalDescriptor.setAccelerationStructureKHR(GLSL_GLOBAL_AS_BINDING, &top);

	// geometry lookup buffer to find the correct vertex informations in shader during ray tracing
	GeometrySSBO geometry[VK_GEOMETRY_SIZE] = {};

	int geometry_count = 0;
	int vertex_count = 0;
	int triangle_count = 0;

	for (RefModel_s* refModel : _scene.refModels) {
		// each mesh in our model will be a geometry of this models blas
		for (RefMesh_s* refMesh : refModel->refMeshes) {
			Mesh* m = refMesh->mesh;
			GeometryDataVulkan::primitveBufferOffset_s offsets = _gpuBlas.getOffset(m);
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
	geometryDataBuffer.STC_UploadData(&_stc, &geometry, geometry_count * sizeof(GeometrySSBO));

	// add the tlas to the descriptor and update it
	globalDescriptor.setAccelerationStructureKHR(GLSL_GLOBAL_AS_BINDING, &top);

	_thermalScene.getProperties().vertexCount = vertex_count;
	_thermalScene.getProperties().triangleCount = triangle_count;
}

void ThermalTransport::setupBuffers(unsigned int _vertex_count, unsigned int _triangle_count)
{
	int vertex_matrix_size = glm::max((unsigned int)1, _vertex_count * _vertex_count);

	// create
	instanceDataBuffer.create(rvk::Buffer::Use::STORAGE, VK_INSTANCE_SIZE * sizeof(InstanceSSBO), rvk::Buffer::Location::HOST_COHERENT);
	transportBuffer.create(rvk::Buffer::Use::STORAGE, vertex_matrix_size * sizeof(SCALAR), rvk::Buffer::Location::DEVICE);
	globalUniformBuffer.create(rvk::Buffer::Use::UNIFORM, sizeof(AuxiliaryUbo), rvk::Buffer::Location::DEVICE);
	triangleAreaBuffer.create(rvk::Buffer::Use::STORAGE, _triangle_count * sizeof(SCALAR), rvk::Buffer::Location::DEVICE);
	valueBuffer.create(rvk::Buffer::Use::STORAGE, _vertex_count * sizeof(SCALAR), rvk::Buffer::Location::DEVICE);
	adjGradXBuffer.create(rvk::Buffer::Use::STORAGE, (3ll * _vertex_count) * sizeof(SCALAR), rvk::Buffer::Location::DEVICE);
	adjTemp4Buffer.create(rvk::Buffer::Use::STORAGE, 1 * sizeof(SCALAR), rvk::Buffer::Location::DEVICE); 
	adjAdjStBuffer.create(rvk::Buffer::Use::STORAGE, 1 * sizeof(SCALAR), rvk::Buffer::Location::DEVICE);

	// map
	instanceDataBuffer.mapBuffer();

	// set
	globalDescriptor.setBuffer(GLSL_GLOBAL_INSTANCE_DATA_BINDING, &instanceDataBuffer);
	globalDescriptor.setBuffer(GLSL_GLOBAL_OUT_IMAGE_BINDING, &transportBuffer);
	globalDescriptor.setBuffer(GLSL_GLOBAL_UBO_BINDING, &globalUniformBuffer);
	globalDescriptor.setBuffer(GLSL_GLOBAL_AREA_DATA_BINDING, &triangleAreaBuffer);
	globalDescriptor.setBuffer(GLSL_GLOBAL_ADJTEMP4_DATA_BINDING, &adjTemp4Buffer);
	globalDescriptor.setBuffer(GLSL_GLOBAL_ADJADJST_DATA_BINDING, &adjAdjStBuffer);
	globalDescriptor.setBuffer(GLSL_GLOBAL_ADJGRADX_DATA_BINDING, &adjGradXBuffer);
}

void ThermalTransport::initTransportBuffer(rvk::SingleTimeCommand& _stc, unsigned int _vertex_count)
{
	//MatrixXf tmp = transportMatrix.cast<FLOAT>();
	//transportBuffer.STC_UploadData(&_stc, tmp.data(), tmp.size() * sizeof(FLOAT), 0);
	transportBuffer.STC_UploadData(&_stc, transportMatrix.data(), transportMatrix.size() * sizeof(SCALAR), 0);
}

void ThermalTransport::setAuxiliaryUbo(AuxiliaryUbo& _aux_ubo, unsigned int _vertex_count, const ThermalVars_s& _thermalVars, unsigned int _timeSteps)
{
	// only set values used for transport
	_aux_ubo.instanceCount = top.size();
	_aux_ubo.vertexCount = _vertex_count;
	_aux_ubo.rayCount = _thermalVars.raysPerThread;
	_aux_ubo.rayDepth = _thermalVars.rayDepth;
	_aux_ubo.batchSeed = _thermalVars.constRandSeed ? _thermalVars.randSeed : rand();
	_aux_ubo.historySize = _timeSteps;
	_aux_ubo.stefanBoltzmannConst = STEFAN_BOLTZMANN_CONST;
	_aux_ubo.gradEdgeRatio = _thermalVars.gradEdgeRatio;
	_aux_ubo.tinyEps = _thermalVars.tinyEps;
	_aux_ubo.tinyDist = _thermalVars.tinyDist;
}

void ThermalTransport::initAuxilaryBuffer(rvk::SingleTimeCommand& _stc, unsigned int _vertex_count, const ThermalVars_s& _thermalVars)
{
	AuxiliaryUbo aux_ubo;
	setAuxiliaryUbo(aux_ubo, _vertex_count, _thermalVars);
	globalUniformBuffer.STC_UploadData(&_stc, &aux_ubo, sizeof(AuxiliaryUbo));
}

void ThermalTransport::initInstanceBuffer(rvk::SingleTimeCommand& _stc, scene_s& _scene, ThermalScene& _thermalScene)
{
	int i = 0;
	unsigned int vertex_offset = 0;
	unsigned int triangle_index_offset = 0;
	int geometry_offset = 0;

	InstanceSSBO instance[VK_INSTANCE_SIZE] = {};
	ThermalObjects& _objects = _thermalScene.getObjects();
	for (RefModel_s* refModel : _scene.refModels)
	{
		// glm stores matrices in a column-major order but vulkan acceleration structures requiere a row-major order
		glm::mat4 model_matrix = glm::transpose(refModel->model_matrix);
		// add instance infos for our instance lookup buffer
		instance[i].geometry_buffer_offset = geometry_offset;
		geometry_offset += refModel->refMeshes.size();
		instance[i].geometryCount = refModel->refMeshes.size();
		std::memcpy(&instance[i].model_matrix, &refModel->model_matrix[0], sizeof(glm::mat4));
		glm::mat4 trp_inv_model_matrix = glm::transpose(glm::inverse(refModel->model_matrix));
		std::memcpy(&instance[i].trp_inv_model_matrix, &trp_inv_model_matrix[0], sizeof(glm::mat4));

		instance[i].diffuseReflectance = _objects.diffuseReflectance[i];
		instance[i].specularReflectance = _objects.specularReflectance[i];
		instance[i].absorption = _objects.absorption[i];
		instance[i].diffuseEmission = _objects.diffuseEmission[i];
		instance[i].traceable = _objects.traceable[i];

		i++;
	}

	instanceDataBuffer.STC_UploadData(&_stc, &instance, i * sizeof(InstanceSSBO));
}


void ThermalTransport::initTriangleAreaBuffer(rvk::SingleTimeCommand& _stc, ThermalData& _thermalData, unsigned int triangle_count)
{
	logEigenBase("triangleAreaVector", _thermalData.triangleAreaVector.transpose());

	//VectorXf tmp = _thermalData.triangleAreaVector.cast<FLOAT>();
	//triangleAreaBuffer.STC_UploadData(&_stc, tmp.data(), tmp.size() * sizeof(FLOAT), 0);
	triangleAreaBuffer.STC_UploadData(&_stc, _thermalData.triangleAreaVector.data(), _thermalData.triangleAreaVector.size() * sizeof(SCALAR), 0);
}

void ThermalTransport::initKelvinBuffer(rvk::SingleTimeCommand& _stc, const ThermalData& _thermalData, unsigned int vertex_count)
{
	//VectorXf tmp = _thermalData.initialValueVector.cast<FLOAT>();
	//valueBuffer.STC_UploadData(&_stc, tmp.data(), tmp.size() * sizeof(FLOAT), 0);
	valueBuffer.STC_UploadData(&_stc, _thermalData.initialValueVector.data(), _thermalData.initialValueVector.size() * sizeof(SCALAR), 0);
}

void ThermalTransport::load(
	rvk::SingleTimeCommand& _stc,
	rvk::LogicalDevice* _device,
	GeometryDataBlasVulkan& _gpuBlas,
	rvk::Buffer& geometryDataBuffer,
	scene_s& _scene,
	ThermalScene& _thermalScene,
	ThermalData& _thermalData,
	const ThermalVars_s& _thermalVars,
	bool setup)
{
	CHECK_EMPTY_SCENE(_scene)

	unsigned int vertex_count = _thermalScene.getProperties().vertexCount;
	unsigned int triangle_count = _thermalScene.getProperties().triangleCount;

	int n = glm::max((unsigned int)1, vertex_count);
	transportMatrix = Mat::Constant(n, n, 0.0);
	spdlog::info("ThermalRenderer: created transport matrix of size {} x {}", n, n);

	if(setup)
		setupBuffers(vertex_count, triangle_count);

	initAS(_stc, _device, _gpuBlas, geometryDataBuffer, _scene, _thermalScene);
	initTransportBuffer(_stc, vertex_count);
	initAuxilaryBuffer(_stc, vertex_count, _thermalVars);
	initInstanceBuffer(_stc, _scene, _thermalScene);
	initTriangleAreaBuffer(_stc, _thermalData, triangle_count);
	//initAbsorptionEmissionBuffers(_stc, _thermalData, vertex_count);
	initKelvinBuffer(_stc, _thermalData, vertex_count);

	globalDescriptor.update();
}

void ThermalTransport::compressTransportMatrix(int iters, int compressionAlgorithm) {
	spdlog::stopwatch sw_comp;

	if (compressionAlgorithm == 0) {
		spdlog::info("Compressing Transport Matrix using simple ACA...");
		simpleAdaptiveCrossApproximation(1e-10, iters);
	}
	else if (compressionAlgorithm == 1) {
		spdlog::info("Compressing Transport Matrix using complete ACA...");
		completeAdaptiveCrossApproximation(1e-10, iters);
	}
	else if (compressionAlgorithm == 2) {
		spdlog::info("Compressing Transport Matrix using ACA+...");
		adaptiveCrossApproximationPlus(1e-10, iters);
	}

	auto comp_time = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(sw_comp.elapsed()).count() / 1000.0);
	spdlog::info("Finished Compression. (dur.: {:.6} s)", sw_comp);
	compressionDuration = std::stof(comp_time);
	
	// computing matrix approximation error (slow)
	Mat  errMat = (transportMatrix - acaTransportMatrix.getU() * acaTransportMatrix.getV());
	T_inf_error = errMat.cwiseAbs().maxCoeff();
	T_frob_error = errMat.norm();
	T_inf_error_rel = T_inf_error / transportMatrix.cwiseAbs().maxCoeff();
	T_frob_error_rel = T_frob_error / transportMatrix.norm();
}

void ThermalTransport::collumnSumCorrection() {
	spdlog::info("Applying column sum correction...");

	Mat& U = acaTransportMatrix.getU();
	Mat& V = acaTransportMatrix.getV();
	SpMat& correctionMat = acaTransportMatrix.getC();
	Vec correctionValues = correctionMat.diagonal();
	int size = U.rows();

	// TODO: find better method for collumn sum correction
	Eigen::Index p_row, p_col;
	Mat T_ = U * V;
	double maxVal = T_.cwiseAbs().maxCoeff(&p_row, &p_col);

	typedef Eigen::Triplet<SCALAR> T;
	std::vector<T> tripletList;
	tripletList.reserve(size);
	Vec uColSums = U.colwise().sum();

	// calculating collumn sums and correction values
	for (int i = 0; i < size; i++) {
		SCALAR recompressedColSum = uColSums.dot(V.col(i)) + correctionValues[i];
		if (recompressedColSum < 0.5 * maxVal) {
			SCALAR correctionValue = 0.5 * maxVal - recompressedColSum;
			tripletList.push_back(T(i, i, correctionValue));
		}
		
	}

	SpMat temp(size, size);
	temp.setFromTriplets(tripletList.begin(), tripletList.end());
	correctionMat += temp;

	// Col sum correction:
	/*
	Eigen::Index p_row, p_col;
	double maxVal = T_.cwiseAbs().maxCoeff(&p_row, &p_col);
	for (int i = 0; i < cols; i++) {
		SCALAR sum = T_.col(i).sum();
		SCALAR sum_ori = T.col(i).sum();
		if (sum < 0.5 * maxVal) { // kinda hacky, would be better to use col-sums in U somehow?
			spdlog::info("adaptiveCrossApproximation: sum {} = {}, was {}", i, sum, sum_ori);
			T_(i, i) += 0.5 * maxVal - sum;
			spdlog::info("adaptiveCrossApproximation: sum*{} = {}", i, T_.col(i).sum());
		}
	}
	*/

	spdlog::info("Finished Correction.");
}

void ThermalTransport::simpleAdaptiveCrossApproximation(double tol, int iters) {

	const Mat& T = transportMatrix;
	int rows = T.rows();
	int cols = T.cols();

	Mat R(rows, cols);
	Mat T_ = Mat::Zero(rows, cols);
	Mat& U = acaTransportMatrix.getU();
	Mat& V = acaTransportMatrix.getV();
	U.resize(rows, iters);
	V.resize(iters, cols);
	U.setZero(); V.setZero();

	for (int i = 0; i < iters; i++) {
		R = T - T_;

		Eigen::Index p_row, p_col;
		double maxVal = R.cwiseAbs().maxCoeff(&p_row, &p_col);
		U.col(i) = R.col(p_col) / R(p_row, p_col);
		V.row(i) = R.row(p_row);

		T_ = T_ + U.col(i) * V.row(i);

		// if (maxVal < tol) break;
	}
}

void ThermalTransport::completeAdaptiveCrossApproximation(double tol, int iters) {

	Mat& T = transportMatrix;
	int size = T.rows();

	Mat T_(size, size);
	Mat& U = acaTransportMatrix.getU();
	Mat& V = acaTransportMatrix.getV();
	U.resize(size, iters);
	V.resize(iters, size);
	U.setZero(); V.setZero();

	Vec u_k(size);
	Vec v_k(size);
	std::unordered_set<Eigen::Index> prevPivotRows; prevPivotRows.reserve(size);
	std::unordered_set<Eigen::Index> prevPivotCols; prevPivotCols.reserve(size);
	Eigen::Index i_k, j_k;

	// either random start or first row index == 0 (according to Zhao et al. 2005)
	//i_k = ((double)std::rand() / RAND_MAX) * (size - 1);
	i_k = 0;

	v_k = T.row(i_k);
	j_k = findArgmaxNotInList(prevPivotCols, v_k);
	u_k = T.col(j_k);

	prevPivotRows.insert(i_k);
	prevPivotCols.insert(j_k);

	if (v_k[j_k] != 0) {
		V.row(0) = v_k / v_k[j_k];
	}
	else {
		V.row(0) = v_k;
	}
	U.col(0) = u_k;

	i_k = findArgmaxNotInList(prevPivotRows, u_k);

	for (int k = 1; k < iters; k++) {
		
		calculateResidualRow(v_k, i_k, k);
		j_k = findArgmaxNotInList(prevPivotCols, v_k);
		prevPivotCols.insert(j_k);

		if (v_k[j_k] != 0) {
			V.row(k) = v_k / v_k[j_k];
		}
		else {
			V.row(k) = v_k;
		}

		calculateResidualCol(u_k, j_k, k);
		U.col(k) = u_k;
		i_k = findArgmaxNotInList(prevPivotRows, u_k);
		prevPivotRows.insert(i_k);

		// calculations for early termination
		// Does not quite work for this matrices, mu ~ 0 -> find good tol value
		/*
		nu = std::pow(u_k.squaredNorm(), 2) * std::pow(v_k.squaredNorm(), 2);
		double sum = 0;
		for (int j = 1; j < k; j++) {
			sum += 2 * std::abs(U.col(j).dot(u_k)) * std::abs(V.row(j).dot(v_k));
		}
		Z += nu + sum;
		std::cout << "left: " << u_k.squaredNorm() * v_k.squaredNorm() << std::endl;
		std::cout << "right: " << std::sqrt(Z) << std::endl;
		*/

		//if (std::sqrt(nu) < tol * std::sqrt(mu)) break;
		
	}
}

Eigen::Index ThermalTransport::findArgmaxNotInList(std::unordered_set<Eigen::Index>& indices, Vec& v) {
	SCALAR max = 0;
	Eigen::Index argmax = 0;
	for (int i = 0; i < v.rows(); i++) {
		SCALAR temp = std::abs(v[i]);
		if (temp > max && indices.find(i) == indices.end()) { // check if v[i] is larger and i not in indices
			argmax = i;
			max = temp;
		}
	}
	return argmax;
}

void ThermalTransport::calculateResidualRow(Vec& v_k, Eigen::Index& i_k, int k) {
	Mat& T = transportMatrix;
	Mat& U = acaTransportMatrix.getU();
	Mat& V = acaTransportMatrix.getV();
	int size = T.rows();
	Vec sum(size);
	sum.setConstant(0);
	for (int i = 0; i < k; i++) {
		sum += U.col(i).row(i_k) * V.row(i);
	}
	v_k = T.row(i_k) - sum.transpose();
};

void ThermalTransport::calculateResidualCol(Vec& u_k, Eigen::Index& j_k, int k) {
	Mat& T = transportMatrix;
	Mat& U = acaTransportMatrix.getU();
	Mat& V = acaTransportMatrix.getV();
	int size = T.rows();
	Vec sum(size);
	sum.setConstant(0);
	for (int i = 0; i < k; i++) {
		sum += V.coeff(i, j_k) * U.col(i);
	}
	u_k = T.col(j_k) - sum;
};

void ThermalTransport::resetReferenceRow(Eigen::Index& r, std::unordered_set<Eigen::Index>& prevRows) {
	int size = transportMatrix.rows();
	for (int i = 1; i < size; i++) {
		r = ((double)std::rand() / RAND_MAX) * (size - 1);
		if (prevRows.find(r) == prevRows.end()) {
			return;
		}
	}
};

void ThermalTransport::resetReferenceCol(Eigen::Index& c, std::unordered_set<Eigen::Index>& prevCols) {
	int size = transportMatrix.cols();
	for (int i = 1; i < size; i++) {
		c = ((double)std::rand() / RAND_MAX) * (size - 1);
		if (prevCols.find(c) == prevCols.end()) {
			return;
		}
	}
};

void ThermalTransport::adaptiveCrossApproximationPlus(double tol, int iters) {

	const Mat& T = transportMatrix;
	int size = T.rows();

	Mat& U = acaTransportMatrix.getU();
	Mat& V = acaTransportMatrix.getV();

	Vec u_k(size);
	Vec v_k(size);
	std::unordered_set<Eigen::Index> prevPivotRows; prevPivotRows.reserve(size);
	std::unordered_set<Eigen::Index> prevPivotCols; prevPivotCols.reserve(size);

	// generating random initial collumn index between 0 and size - 1
	Eigen::Index i_k, j_k, r, c;
	resetReferenceRow(r, prevPivotRows);
	resetReferenceCol(c, prevPivotRows);

	SCALAR nu = 0;
	SCALAR mu = 0;

	for (int k = 0; k < iters; k++) {

		calculateResidualRow(v_k, r, k);
		calculateResidualCol(u_k, c, k);
		
		i_k = findArgmaxNotInList(prevPivotRows, u_k);
		j_k = findArgmaxNotInList(prevPivotCols, v_k);
		SCALAR maxColVal = std::abs(u_k[i_k]);
		SCALAR maxRowVal = std::abs(v_k[j_k]);

		// pivot either first by col or by row
		if (maxColVal > maxRowVal) { // then find corresponding row to colVal
			calculateResidualRow(v_k, i_k, k);
			j_k = findArgmaxNotInList(prevPivotCols, v_k);
			calculateResidualCol(u_k, j_k, k);
		}
		else { // find corresponding col to rowVal
			calculateResidualCol(u_k, j_k, k);
			i_k = findArgmaxNotInList(prevPivotRows, u_k);
			calculateResidualRow(v_k, i_k, k);
		}

		prevPivotRows.insert(i_k);
		prevPivotCols.insert(j_k);

		if (i_k == r) {
			resetReferenceRow(r, prevPivotRows);
		}
		else if (j_k == c) {
			resetReferenceCol(c, prevPivotCols);
		}

		// maybe find a better way for this
		if (u_k[i_k] != 0) {
			U.col(k) = u_k / u_k[i_k];
		}
		else {
			U.col(k) = u_k;
		}
		V.row(k) = v_k;
		
		// calculations for early termination
		// Does not quite work for this matrices, mu ~ 0 -> find good tol value
		/*
		nu = std::pow(u_k.squaredNorm(), 2) * std::pow(v_k.squaredNorm(), 2);
		double sum = 0;
		for (int j = 0; j <= k; j++) {
			sum += 2 * V.row(j) * v_k * u_k.transpose() * U.col(j);
		}
		mu += nu + sum;
		std::cout << sum << std::endl;
		if (std::sqrt(nu) < tol * std::sqrt(mu)) break;
		*/
	}
}

void ThermalTransport::compute(rvk::SingleTimeCommand& stc, rvk::LogicalDevice* _device, ThermalScene& _thermalScene, ThermalData& _thermalData, const ThermalVars_s& _thermalVars, int mode)
{
	spdlog::info("computeTransportMatrix: Spawning threads for {} triangles...", _thermalScene.getProperties().triangleCount);

	spdlog::stopwatch sw_gpu;

	int n = _thermalScene.getProperties().vertexCount;

	AuxiliaryUbo aux_ubo;
	setAuxiliaryUbo(aux_ubo, n, _thermalVars);
	globalUniformBuffer.STC_UploadData(&stc, &aux_ubo, sizeof(AuxiliaryUbo));
	globalDescriptor.update();

	transportMatrix.setZero();
	//MatrixXf tmp = transportMatrix.cast<FLOAT>();
	//transportBuffer.STC_UploadData(&stc, tmp.data(), tmp.size() * sizeof(FLOAT), 0);
	transportBuffer.STC_UploadData(&stc, transportMatrix.data(), transportMatrix.size() * sizeof(SCALAR), 0);

	stc.begin();
	rt_transport_pipeline.CMD_BindDescriptorSets(stc.buffer(), { &globalDescriptor });
	rt_transport_pipeline.CMD_BindPipeline(stc.buffer());
	rt_transport_pipeline.CMD_TraceRays(stc.buffer(), _thermalVars.threadsPerTriangle, _thermalScene.getProperties().triangleCount); // lauch__.y must be number of triangles
	stc.end();

	_device->waitIdle();

	auto gpu_time = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(sw_gpu.elapsed()).count() / 1000.0);
	spdlog::info("\tfinished. (dur.: {:.6} s)", gpu_time);

	spdlog::stopwatch sw_cpu;

	//transportBuffer.STC_DownloadData(&stc, tmp.data(), tmp.size() * sizeof(FLOAT));
	//transportMatrix = tmp.cast<SCALAR>();
	transportBuffer.STC_DownloadData(&stc, transportMatrix.data(), transportMatrix.size() * sizeof(SCALAR));

	// Compressing Transport Matrix using ACA (before adding STEFAN_BOLTZMANN_CONST)
	
	if (_thermalVars.compressTransport) {
		int size = transportMatrix.rows();

		acaTransportMatrix.resize(size, _thermalVars.compressionMaxIters);
		acaTransportMatrix.setZero();

		// adding Stefan Boltzmann Diagonal
		typedef Eigen::Triplet<SCALAR> T;
		std::vector<T> tripletList;
		tripletList.reserve(size);
		Vec stefanBoltzmannDiag = STEFAN_BOLTZMANN_CONST * _thermalData.emissionVector.cwiseProduct(_thermalData.vertexAreaVector);
		for (int i = 0; i < size; i++) {
			tripletList.push_back(T(i, i, stefanBoltzmannDiag[i]));
		}
		SpMat& sparseAdditionMatrix = acaTransportMatrix.getC();
		sparseAdditionMatrix.setFromTriplets(tripletList.begin(), tripletList.end());

		compressTransportMatrix(_thermalVars.compressionMaxIters, _thermalVars.compressionAlgorithm);

		if (_thermalVars.applyCollumnSumCorrection) collumnSumCorrection();
	}
	transportCompressed = _thermalVars.compressTransport;

	// add diagonal (emissive) part - skipped in shader
	transportMatrix.diagonal() += STEFAN_BOLTZMANN_CONST * _thermalData.emissionVector.cwiseProduct(_thermalData.vertexAreaVector);
	

	//Note: transport matrix is generally asymmetric - for instance if we use non-traceable sources
	
	logEigenBase("solverData.transportMatrix", transportMatrix);
	
	// Changing transport matrix to decompressed matlab matrix

	/*int rows = transportMatrix.rows();
	int cols = transportMatrix.cols();
	Eigen::MatrixXf T_ = Eigen::MatrixXf(rows, cols);
	std::vector<std::string> row;
	std::string line, word, temp;

	const std::string path = "E:\\UNI lokal\\thermalSim\\matlab\\transport_reconstructed.csv";
	std::ifstream in(path);

	if (in.is_open()) {
		int i = 0;
		while (std::getline(in, line)) {

			std::stringstream s(line);

			int j = 0;
			while (std::getline(s, word, ',')) {
				T_(i,j) = (SCALAR)std::stod(word);
				j++;
			}
			i++;
		}
	}
	else {
		std::cerr << "Error: " << strerror(errno);
	}
	
	const Mat test(T_);
	//transportMatrix = test;
	*/

	if (transportMatrix.hasNaN())
		spdlog::error("Transport matrix has NaN!");
		
	//unsigned int emit_count = _ray_count * _batch_count;
	//for (int k = 0; k < n; k++)
	//	transportMatrix.col(k) /= 3.0 * emit_count * _thermalData.vertexTriangleCountVector(k);

	logEigenBase("solverData.transportMatrix", transportMatrix);
	

#ifndef RUNTIME_OPTIMIZED
	spdlog::debug("HINT: rows represent the 'sum' of emitted and absorbed factor for each node, if sum < 0 node is potentially 'loosing' energy, if sum > 0 potentially 'gaining' energy");
	spdlog::debug("HINT: columns represent the 'sum' of emitted and distributed factor per node, if sum < 0 some energy is not absorbed, if sum > 0 something is wrong");
	spdlog::debug("HINT: for closed systems the column sum must be 0");
	printTransportMatrixSums();

	spdlog::info("transportMatrix:min = {:.2}", transportMatrix.minCoeff());
	spdlog::info("transportMatrix:avg = {:.2}", transportMatrix.mean());
	spdlog::info("transportMatrix:max = {:.2}", transportMatrix.maxCoeff());

	//std::cout << std::endl << "T_ = [ " << transportMatrix << " ]; " << std::endl << std::endl;
#endif // RUNTIME_OPTIMIZED

	spdlog::info("Transport matrix generation dur.: {:.3} s; (GPU: {:.3} s, CPU: {:.3} s,))", sw_gpu, gpu_time, sw_cpu);

#ifndef RUNTIME_OPTIMIZED
	spdlog::info("transportMatrix:min = {:.2}", transportMatrix.minCoeff());
	spdlog::info("transportMatrix:avg = {:.2}", transportMatrix.mean());
	spdlog::info("transportMatrix:max = {:.2}", transportMatrix.maxCoeff());
#endif

#ifndef DISABLE_GUI
	uploadTransportMatrix(stc, transportMatrix);
#endif

#ifndef RUNTIME_OPTIMIZED
	printTransportMatrixSums();
#endif // RUNTIME_OPTIMIZED
}

void ThermalTransport::computeAdjoint(rvk::SingleTimeCommand& stc, rvk::LogicalDevice* _device, ThermalScene& _thermalScene, ThermalData& _thermalData, const ThermalVars_s& _thermalVars, std::vector<Vec>& _temperatureHistory, std::vector<Vec>& _adjointStateHistory) {
	// clear gradient data ... to be written on the GPU
	_thermalData.objectiveGradX.setZero();
	//{
	//	Matrix<FLOAT, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> tmp = _thermalData.objectiveGradX.cast<FLOAT>();
	//	adjGradXBuffer.STC_UploadData(&stc, tmp.data(), tmp.size() * sizeof(FLOAT), 0);
	//}
	adjGradXBuffer.STC_UploadData(&stc, _thermalData.objectiveGradX.data(), _thermalData.objectiveGradX.size() * sizeof(SCALAR), 0);

	// upload history data ...
	unsigned int vectorSize  = _thermalData.currentValueVector.size();
	unsigned int timeSteps   = _temperatureHistory.size();
	unsigned int historySize =  timeSteps * vectorSize;
	if( adjTemp4Buffer.getSize() != historySize ){
		adjTemp4Buffer.destroy();
		adjTemp4Buffer.create(rvk::Buffer::Use::STORAGE, historySize * sizeof(SCALAR), rvk::Buffer::Location::DEVICE);
		globalDescriptor.setBuffer(GLSL_GLOBAL_ADJTEMP4_DATA_BINDING, &adjTemp4Buffer);
		globalDescriptor.update();
	}
	if( adjAdjStBuffer.getSize() != historySize ){
		adjAdjStBuffer.destroy();
		adjAdjStBuffer.create(rvk::Buffer::Use::STORAGE, historySize * sizeof(SCALAR), rvk::Buffer::Location::DEVICE);
		globalDescriptor.setBuffer(GLSL_GLOBAL_ADJADJST_DATA_BINDING, &adjAdjStBuffer);
		globalDescriptor.update();
	}
	unsigned int offset=0;
	for(int tstep=0; tstep < timeSteps; ++tstep){
		Vec t4 = _temperatureHistory[tstep];
		t4 = t4.array().pow(4.0).eval();

		//VectorXf tmp = t4.cast<FLOAT>();
		//adjTemp4Buffer.STC_UploadData(&stc, tmp.data(), vectorSize * sizeof(FLOAT), offset);
		adjTemp4Buffer.STC_UploadData(&stc, t4.data(), vectorSize * sizeof(SCALAR), offset);

		//tmp = _adjointStateHistory[tstep].cast<FLOAT>();
		//adjAdjStBuffer.STC_UploadData(&stc, tmp.data(), vectorSize * sizeof(FLOAT), offset);
		adjAdjStBuffer.STC_UploadData(&stc, _adjointStateHistory[tstep].data(), vectorSize * sizeof(SCALAR), offset);
		offset += vectorSize * sizeof(FLOAT);
	}

	spdlog::stopwatch sw_gpu;

	int n = _thermalScene.getProperties().vertexCount;

	AuxiliaryUbo aux_ubo;
	setAuxiliaryUbo(aux_ubo, n, _thermalVars, timeSteps );
	globalUniformBuffer.STC_UploadData(&stc, &aux_ubo, sizeof(AuxiliaryUbo));
	globalDescriptor.update();

	int threads_per_triangle = _thermalVars.threadsPerTriangle;

	if (_thermalVars.adjointBatchCount > 1) {
		threads_per_triangle /= _thermalVars.adjointBatchCount;
	}

	spdlog::info("Adjoint trace: {} time steps, {} triangles, {} threads per triangle, {} rays per thread, {} adj. batches ...",
		timeSteps,
		_thermalScene.getProperties().triangleCount,
		threads_per_triangle,
		_thermalVars.raysPerThread,
		_thermalVars.adjointBatchCount
	);

	for (int i = 0; i < _thermalVars.adjointBatchCount; i++)
	{
		spdlog::info("Adjoint trace #{}/{} ...", i + 1, _thermalVars.adjointBatchCount);

		setAuxiliaryUbo(aux_ubo, n, _thermalVars, timeSteps);
		globalUniformBuffer.STC_UploadData(&stc, &aux_ubo, sizeof(AuxiliaryUbo));
		globalDescriptor.update();

		stc.begin();
		adj_transport_pipeline.CMD_BindDescriptorSets(stc.buffer(), { &globalDescriptor });
		adj_transport_pipeline.CMD_BindPipeline(stc.buffer());
		adj_transport_pipeline.CMD_TraceRays(stc.buffer(), threads_per_triangle /*threads per triangle*/, _thermalScene.getProperties().triangleCount); // lauch__.y must be number of triangles
		stc.end();
	}

	_device->waitIdle();

	//{
	//	Matrix<FLOAT, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> tmp; tmp.resizeLike(_thermalData.objectiveGradX);
	//	adjGradXBuffer.STC_DownloadData(&stc, tmp.data(), tmp.size() * sizeof(FLOAT), 0);
	//	_thermalData.objectiveGradX = tmp.cast<SCALAR>();
	//}
	adjGradXBuffer.STC_DownloadData(&stc, _thermalData.objectiveGradX.data(), _thermalData.objectiveGradX.size() * sizeof(SCALAR), 0);

	_thermalData.objectiveGradX /= _thermalVars.adjointBatchCount; // need to rescale in case adjoint batch count > 1

	auto gpu_time = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(sw_gpu.elapsed()).count() / 1000.0);
	spdlog::info("\tfinished. (dur.: {:.6} s)", gpu_time);

}

void ThermalTransport::uploadTransportMatrix(rvk::SingleTimeCommand& stc, const Mat& _transportMatrix)
{
	transportBuffer.STC_UploadData(&stc, _transportMatrix.data(), _transportMatrix.size() * sizeof(FLOAT), 0);
}

void ThermalTransport::uploadValueVector(rvk::SingleTimeCommand& stc, const Vec& _values)
{
	//VectorXf tmp = _values.cast<FLOAT>();
	//valueBuffer.STC_UploadData(&stc, tmp.data(), tmp.size() * sizeof(FLOAT), 0);
	valueBuffer.STC_UploadData(&stc, _values.data(), _values.size() * sizeof(SCALAR), 0);
}

void ThermalTransport::unload()
{
	top.destroy();
	transportBuffer.destroy();
	valueBuffer.destroy();
	triangleAreaBuffer.destroy();
	//vertexEmissionBuffer.destroy();
	//vertexAbsorptionBuffer.destroy();
}

void ThermalTransport::printTransportMatrixSums()
{
	//return;
	SCALAR threshold;
	threshold = FLT_EPSILON;
	if (spdlog::get_level() >= spdlog::level::debug)
	{
		for (int k = 0; k < transportMatrix.cols(); k++) {
			//if (std::abs(transportMatrix.col(k).sum()) > (10.0 * threshold))
			spdlog::debug("transport column {} sum {}", k, transportMatrix.col(k).sum());
		}
		for (int k = 0; k < transportMatrix.rows(); k++) {
			//if (std::abs(transportMatrix.row(k).sum()) > (10.0 * threshold))
			spdlog::debug("transport   row  {} sum {}", k, transportMatrix.row(k).sum());
		}
	}
}