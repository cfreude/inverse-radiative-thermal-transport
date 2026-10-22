#include "thermal_objects.hpp"
#include <iomanip>

void ThermalObjects::reserve(unsigned int _size)
{
	name.reserve(_size);
	model.reserve(_size);

	// thermal values
	kelvin.resize(_size);
	thickness.resize(_size);
	density.resize(_size);
	heatCapacity.resize(_size);
	heatConductivity.resize(_size);

	// radiation values	
	diffuseReflectance.resize(_size);
	specularReflectance.resize(_size);
	absorption.resize(_size);

	// simulation flags
	diffuseEmission.reserve(_size);
	temperatureFixed.reserve(_size);
	traceable.reserve(_size);

	vertexCount.reserve(_size);
	vertexOffset.reserve(_size);
}

void ThermalObjects::resize(unsigned int _size)
{
	name.resize(_size);
	model.resize(_size);

	// thermal values
	kelvin.resize(_size);
	thickness.resize(_size);
	density.resize(_size);
	heatCapacity.resize(_size);
	heatConductivity.resize(_size);

	// radiation values	
	diffuseReflectance.resize(_size);
	specularReflectance.resize(_size);
	absorption.resize(_size);

	// simulation flags
	diffuseEmission.resize(_size);
	temperatureFixed.resize(_size);
	traceable.resize(_size);

	vertexCount.resize(_size);
	vertexOffset.resize(_size);
}

void ThermalObjects::clear()
{
	name.clear();
	model.clear();

	// thermal values
	kelvin.setZero();
	thickness.setZero();
	density.setZero();
	heatCapacity.setZero();
	heatConductivity.setZero();

	// radiation values	
	diffuseReflectance.setZero();
	specularReflectance.setZero();
	absorption.setZero();

	// simulation flags
	diffuseEmission.clear();
	temperatureFixed.clear();
	traceable.clear();

	vertexCount.clear();
	vertexOffset.clear();
}

void ThermalObjects::print(unsigned int _index) const
{
	spdlog::info("##### Object: {}", name[_index]);
	spdlog::info("- - -");
	spdlog::info("kelvin: {}", kelvin[_index]);
	spdlog::info("temperature_fixed: {}", temperatureFixed[_index]);
	spdlog::info("---");
	spdlog::info("diffuse_reflectance: {}", diffuseReflectance[_index]);
	spdlog::info("specular_reflectance: {}", specularReflectance[_index]);
	spdlog::info("absorption: {}", absorption[_index]);
	spdlog::info("- - -");
	spdlog::info("diffuse_emission: {}", diffuseEmission[_index]);
	spdlog::info("traceable: {}", traceable[_index]);
	spdlog::info("---");
	spdlog::info("thickness: {}", thickness[_index]);
	spdlog::info("density: {}", density[_index]);
	spdlog::info("heat_capacity: {}", heatCapacity[_index]);
	spdlog::info("heat_conductivity: {}", heatConductivity[_index]);
	spdlog::info("#####");
}

const char* ThermalObjects::getUiStr(unsigned int index) const
{
	std::ostringstream ss;
	ss << std::setw(4) << std::setfill('0') << std::setprecision(2) << std::fixed;
	ss << "diffRefl: " << diffuseReflectance[index] << " | ";
	ss << "specRefl: " << specularReflectance[index] << " | ";
	ss << name[index];
	return ss.str().c_str();
}


void ThermalObjects::setVertexColors(const Vec& _vals) const
{
	float max_val = _vals.maxCoeff();

	for(int i=0; i<model.size(); i++)
	{
		RefModel_s* refModel = model[i];
		const Vec& model_values = _vals.segment(vertexOffset[i], vertexCount[i]);

		int offset = 0;
		for (RefMesh_s* refMesh : refModel->refMeshes) {
			Mesh* mesh = refMesh->mesh;
			int vc = mesh->getVertexCount();
			std::vector<vertex_s>& vertices = mesh->getVerticesVectorRef();
			for (int vi = 0; vi < vc; vi++)
			{
				float value = model_values[offset++] / max_val;
				vertices[vi].color_0 = glm::vec4(value, value, value, 1.0);
				//spdlog::info("#v: {}, v: {}", offset, vertices[vi].color_0.x);
			}
			mesh->hasColors0(true);
		}
		refModel->model->addCustomProperty("max-vertex-kelvin", Value(max_val));

		if (offset != vertexCount[i])
			spdlog::warn("offset {} != {} vertexCount for mesh {}", offset, vertexCount[i], refModel->model->getName());
	}
}