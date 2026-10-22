#include "thermal_data.hpp"
#include "thermal_objective.hpp"
#include "thermal_params.hpp"

#include <cassert>

void ThermalData::init(scene_s _scene, const ThermalObjects& _thermal_object, unsigned int _vertex_count, unsigned int _triangle_count)
{
	currentValueVector = Vec(_vertex_count);
	currentValueVector.setConstant(0.0);
	initialValueVector = Vec(_vertex_count);
	initialValueVector.setConstant(0.0);
	emissionVector = Vec(_vertex_count);
	emissionVector.setConstant(0.0);
	absorptionVector = Vec(_vertex_count);
	absorptionVector.setConstant(0.0);

	vertexAreaVector = Vec(_vertex_count);
	vertexAreaVector.setConstant(0.0);

	vertexTriangleCountVector = VectorXi(_vertex_count);
	vertexTriangleCountVector.setConstant(0);

	triangleAreaVector = Vec(_triangle_count);
	triangleAreaVector.setConstant(0.0);

	fixedVarsVector = Vec(_vertex_count);
	fixedVarsVector.setConstant(0.0);

	lumpedMassMatrix.resize(_vertex_count);
	lumpedMassMatrix.setZero();

	conductiveLaplacian.resize(_vertex_count,_vertex_count);
	conductiveLaplacian.setZero();

	objectiveGradX.resize( _vertex_count, 3 );
	objectiveGradX.setZero();
}

void ThermalData::recomputeAreaVectors(const scene_s& _scene) {
	unsigned int vertex_offset = 0;
	unsigned int triangle_index_offset = 0;
	vertexAreaVector.setZero();
	triangleAreaVector.setZero();
	for (RefModel_s* refModel : _scene.refModels) {
		glm::mat4 model_matrix = refModel->model_matrix;
		for (RefMesh_s* refMesh : refModel->refMeshes) {
			Mesh* m = refMesh->mesh;
			size_t vertex_count = m->getVertexCount();
			updateAreaVectorsForMesh(refModel, m, vertex_offset, triangle_index_offset); // counts triangles in triangle_index_offset
			vertex_offset += vertex_count;
		}
	}
}

void ThermalData::updateAreaVectorsForMesh(RefModel_s* refModel, Mesh* m, unsigned int vertex_offset, unsigned int& triangle_index_offset){
	for(unsigned int vi = 0; vi < m->getIndexCount(); vi += 3 ){
		unsigned int ind0 = m->getIndicesVector()->at(vi);
		unsigned int ind1 = m->getIndicesVector()->at(vi + 1);
		unsigned int ind2 = m->getIndicesVector()->at(vi + 2);
		glm::dvec4 v0_ = refModel->model_matrix * m->getVerticesVector()->at(ind0).position;
		glm::dvec4 v1_ = refModel->model_matrix * m->getVerticesVector()->at(ind1).position;
		glm::dvec4 v2_ = refModel->model_matrix * m->getVerticesVector()->at(ind2).position;
		glm::dvec3 v0 = glm::dvec3(v0_.x, v0_.y, v0_.z);
		glm::dvec3 v1 = glm::dvec3(v1_.x, v1_.y, v1_.z);
		glm::dvec3 v2 = glm::dvec3(v2_.x, v2_.y, v2_.z);
		SCALAR area = glm::length(glm::cross<double>(v1 - v0, v2 - v0)) * 0.5;
		if (area < (10.0 * FLT_EPSILON)) {
			//Problem: if a small element (representing a tiny bit of material) randomly gets hit by a ray, it absorbs some amount of energy that leads to a huge change in temperature (because the heat capacity is so small).
			//Result: this behaviour leads to ill-conditioned systems which can limit the time step size quite badly.
			//Also: these tiny elements may not generate many outgoing rays (or those rays that do get generated might hit nearby elements depending on the geometry around them) and may fail to radiate off enough energy to cool down properly.
			//The best way to deal with this would be to make sure we only get "clean" geometry as input.
			spdlog::warn("clamping small element {} area {} --> {}", triangle_index_offset, area, (10.0 * FLT_EPSILON));
			area = (10.0 * FLT_EPSILON);
		}
		vertexAreaVector[vertex_offset + ind0] += area / 3.0;
		vertexAreaVector[vertex_offset + ind1] += area / 3.0;
		vertexAreaVector[vertex_offset + ind2] += area / 3.0;
		triangleAreaVector[triangle_index_offset++] = area;
	}
}

void ThermalData::load(scene_s _scene, ThermalObjects& _thermal_objects, SCALAR _sky_minimum_kelvin, unsigned int _timeSteps)
{
	int i = 0;
	unsigned int vertex_offset = 0;
	unsigned int triangle_index_offset = 0;
	int geometry_offset = 0;

	for (RefModel_s* refModel : _scene.refModels) {
		glm::mat4 model_matrix = refModel->model_matrix;
		// add instance infos for our instance lookup buffer

		for (RefMesh_s* refMesh : refModel->refMeshes) {
			Mesh* m = refMesh->mesh;
			for (auto i : *m->getIndicesVector())
				vertexTriangleCountVector(vertex_offset + i) += 1;

			size_t vertex_count = m->getVertexCount();
			updateAreaVectorsForMesh(refModel, m, vertex_offset, triangle_index_offset); // counts triangles in triangle_index_offset
			fixedVarsVector.segment(vertex_offset, vertex_count).setConstant(_thermal_objects.temperatureFixed[i]);
			initialValueVector.segment(vertex_offset, vertex_count).setConstant(_thermal_objects.kelvin[i]);

			// overrride constant initialKelvin
			std::vector<Value> sky_vals = refModel->model->getCustomProperty("sky-values").getArray();
			if (sky_vals.size() > 0)
			{
				std::vector<float> sky_values;
				sky_values.reserve(sky_vals.size());
				for (auto v : sky_vals)
					sky_values.push_back(v.getFloat());

				std::vector<Value> sky_hour_vals = refModel->model->getCustomProperty("sky-hours").getArray();

				std::vector<unsigned int> sky_hours;
				sky_hours.reserve(sky_hour_vals.size());
				for (auto h : sky_hour_vals)
					sky_hours.push_back(h.getInt());

				if (sky_hours.size() == 0)
					sky_hours.push_back(0);

				int sky_hour = refModel->model->getCustomProperty("sky-hour").getInt();

				spdlog::debug("Sky | value count: {}, hours count: {}, hour: {}", sky_values.size(), sky_hours.size(), sky_hour);

				std::vector<uint32_t> indices = *m->getIndicesVector();
				sky.init(sky_values, sky_hours, vertex_offset, indices);

				int hour_index = 0;
				for (auto v : sky_hours)
				{
					if (v == sky_hour)
						break;
					hour_index++;
				}
				sky.current_hour = (float)hour_index;

				float avg_sky_value = sky.setHourByTime(sky.current_hour, initialValueVector, SkyData::MINIMAL_KELVIN);

				// WARNING: modification of 
				_thermal_objects.kelvin[i] = sky_value_to_kelvin(avg_sky_value, _sky_minimum_kelvin);
			}

			emissionVector.segment(vertex_offset, vertex_count).setConstant(_thermal_objects.absorption[i]);
			absorptionVector.segment(vertex_offset, vertex_count).setConstant(1.0 / (_thermal_objects.density[i] * _thermal_objects.heatCapacity[i] * _thermal_objects.thickness[i]));
			vertex_offset += vertex_count;
		}

		ObjectStatistics_s stats;
		stats.name = std::string(_thermal_objects.name[i]);
		stats.min = _thermal_objects.kelvin[i];
		stats.max = _thermal_objects.kelvin[i];
		stats.avg = _thermal_objects.kelvin[i];
		objectStatistics.push_back(stats);

		i++;
	}

	//emissionVector.array() *= vertexAreaVector.array();
	//absorptionVector.array() *= 1.0 / vertexAreaVector.array();

	logEigenBase("vertexAreaVector", vertexAreaVector.transpose());
	logEigenBase("vertexTriangleCount", vertexTriangleCountVector.transpose());

	computeLumpedThermalMass(_scene,  _thermal_objects);
	computeConductiveLaplacian(_scene,  _thermal_objects);


	objFcn.reset( new ObjectiveFunction(_scene, vertexAreaVector,initialValueVector, _timeSteps));
	params.reset( new Parametrization(_scene, vertexAreaVector.size() ) );
	constraints.reset( new Constraints(_scene, vertexAreaVector.size()) );
}

const std::tuple<SCALAR, SCALAR> ThermalData::getMinMaxOfNonFixedValues() const
{
	Vec nonFixed = (fixedVarsVector.array() < 1.0).select(currentValueVector, currentValueVector.mean());
	return std::make_tuple(nonFixed.minCoeff(), nonFixed.maxCoeff());
}

void ThermalData::setObjectStatistics(const ThermalObjects& objects, const Vec& _values)
{
	unsigned int i = 0;
	for (ObjectStatistics_s& v : objectStatistics)
	{
		if (_values.count() > 0) {
			Vec vals = _values.segment(objects.vertexOffset[i], objects.vertexCount[i]);
			v.min = vals.minCoeff();
			v.max = vals.maxCoeff();
			v.avg = vals.mean();
		}
		i++;
	}
}

void ThermalData::reset()
{
	currentValueVector = initialValueVector;
}

void ThermalData::unload()
{
	currentValueVector.resize(0);
	initialValueVector.resize(0);
	emissionVector.resize(0);
	absorptionVector.resize(0);
	vertexAreaVector.resize(0);
	vertexTriangleCountVector.resize(0);
	triangleAreaVector.resize(0);
	fixedVarsVector.resize(0);
	objectStatistics.clear();
	lumpedMassMatrix.resize(0);
	conductiveLaplacian.resize(0,0);
}

void ThermalData::computeLumpedThermalMass(scene_s _scene, ThermalObjects& _thermal_objects) {
	int i = 0;
	unsigned int vertex_offset = 0;
	unsigned int triangle_index_offset = 0;
	int geometry_offset = 0;

	lumpedMassMatrix.setZero();
	for (RefModel_s* refModel : _scene.refModels){
		glm::mat4 model_matrix = refModel->model_matrix;

		for (RefMesh_s* refMesh : refModel->refMeshes) {
			Mesh* m = refMesh->mesh;

			for (int vi = 0; vi < m->getIndexCount(); vi += 3){
				unsigned int ind0 = m->getIndicesVector()->at(vi);
				unsigned int ind1 = m->getIndicesVector()->at(vi + 1);
				unsigned int ind2 = m->getIndicesVector()->at(vi + 2);
				glm::dvec4 v0_ = refModel->model_matrix * m->getVerticesVector()->at(ind0).position;
				glm::dvec4 v1_ = refModel->model_matrix * m->getVerticesVector()->at(ind1).position;
				glm::dvec4 v2_ = refModel->model_matrix * m->getVerticesVector()->at(ind2).position;
				glm::dvec3 v0 = glm::dvec3(v0_.x, v0_.y, v0_.z);
				glm::dvec3 v1 = glm::dvec3(v1_.x, v1_.y, v1_.z);
				glm::dvec3 v2 = glm::dvec3(v2_.x, v2_.y, v2_.z);
				double area = glm::length(glm::cross<double>(v1 - v0, v2 - v0)) * 0.5;
				lumpedMassMatrix[vertex_offset + ind0] += area / 3.0 * _thermal_objects.density[i] * _thermal_objects.heatCapacity[i] * _thermal_objects.thickness[i];
				lumpedMassMatrix[vertex_offset + ind1] += area / 3.0 * _thermal_objects.density[i] * _thermal_objects.heatCapacity[i] * _thermal_objects.thickness[i];
				lumpedMassMatrix[vertex_offset + ind2] += area / 3.0 * _thermal_objects.density[i] * _thermal_objects.heatCapacity[i] * _thermal_objects.thickness[i];
			}
			vertex_offset += m->getVertexCount();
		}
		++i;
	}
	logEigenBase("lumpedMassMatrix", lumpedMassMatrix.transpose());
	//std::cout << "M = [ " << lumpedMassMatrix.transpose() << " ]; " << std::endl;
}

// helper function: cotan of angle between two vectors
double cotan(glm::dvec3 a, glm::dvec3 b){
	double aa = glm::dot(a, a);
	double ab = glm::dot(a, b);
	double bb = glm::dot(b, b);
	return (ab / std::sqrt(aa*bb-ab*ab));
}
void ThermalData::computeConductiveLaplacian(scene_s _scene, ThermalObjects& _thermal_objects) {
	int i = 0;
	unsigned int vertex_offset = 0;
	unsigned int triangle_index_offset = 0;
	int geometry_offset = 0;

    // preallocate storage for sparse matrix triplets (i,j,v)
	unsigned int num_ijv = 9*triangleAreaVector.size();
	std::vector<Eigen::Triplet<double> > ijvs;
	ijvs.reserve(num_ijv);

	for (RefModel_s* refModel : _scene.refModels){
		glm::mat4 model_matrix = refModel->model_matrix;
		double k = _thermal_objects.heatConductivity[i] * _thermal_objects.thickness[i]; // multiply thickness here because we use a "volumetric" mass matrix (i.e. for surface conduction M u_t + L u = 0, we would include rho*c_p in M and k in L, but not consider thickness, but due to radiation - i.e surface to assumed volume transfer - we include thickness in M, hence also in L)
		spdlog::debug("thermal conductivity[{}]={}", i, k);

		for (RefMesh_s* refMesh : refModel->refMeshes) {
			Mesh* m = refMesh->mesh;

			for (int vi = 0; vi < m->getIndexCount(); vi += 3){
				unsigned int ind0 = m->getIndicesVector()->at(vi);
				unsigned int ind1 = m->getIndicesVector()->at(vi + 1);
				unsigned int ind2 = m->getIndicesVector()->at(vi + 2);
				glm::dvec4 v0_ = refModel->model_matrix * m->getVerticesVector()->at(ind0).position;
				glm::dvec4 v1_ = refModel->model_matrix * m->getVerticesVector()->at(ind1).position;
				glm::dvec4 v2_ = refModel->model_matrix * m->getVerticesVector()->at(ind2).position;
				glm::dvec3 a = glm::dvec3(v0_.x, v0_.y, v0_.z);
				glm::dvec3 b = glm::dvec3(v1_.x, v1_.y, v1_.z);
				glm::dvec3 c = glm::dvec3(v2_.x, v2_.y, v2_.z);

				double cot1=cotan(a-c,b-c);
				double cot2=cotan(b-a,c-a);
				double cot3=cotan(c-b,a-b);

				// matrix entries for first corner
				ijvs.push_back(Eigen::Triplet<double>( vertex_offset + ind0, vertex_offset + ind0, k*0.5*( cot1+cot3) ));
				ijvs.push_back(Eigen::Triplet<double>( vertex_offset + ind0, vertex_offset + ind1, k*0.5*(-cot1     ) ));
				ijvs.push_back(Eigen::Triplet<double>( vertex_offset + ind0, vertex_offset + ind2, k*0.5*(     -cot3) ));

				// matrix entries for second corner
				ijvs.push_back(Eigen::Triplet<double>( vertex_offset + ind1, vertex_offset + ind0, k*0.5*(-cot1     ) ));
				ijvs.push_back(Eigen::Triplet<double>( vertex_offset + ind1, vertex_offset + ind1, k*0.5*( cot1+cot2) ));
				ijvs.push_back(Eigen::Triplet<double>( vertex_offset + ind1, vertex_offset + ind2, k*0.5*(     -cot2) ));
		
				// matrix entries for third corner
				ijvs.push_back(Eigen::Triplet<double>( vertex_offset + ind2, vertex_offset + ind0, k*0.5*(     -cot3) ));
				ijvs.push_back(Eigen::Triplet<double>( vertex_offset + ind2, vertex_offset + ind1, k*0.5*(-cot2     ) ));
				ijvs.push_back(Eigen::Triplet<double>( vertex_offset + ind2, vertex_offset + ind2, k*0.5*( cot2+cot3) ));
			}
			vertex_offset += m->getVertexCount();
		}
		++i;
	}
	conductiveLaplacian.setFromTriplets(ijvs.begin(), ijvs.end());

	//std::cout << "L = [ " << conductiveLaplacian.transpose() << " ]; " << std::endl;
}

float ThermalData::setSkyHour(float _hour)
{
	float hour;
	hour = sky.setHourByTime(_hour, initialValueVector, SkyData::MINIMAL_KELVIN);
	hour = sky.setHourByTime(_hour, currentValueVector, SkyData::MINIMAL_KELVIN);
	return hour;
}