#include "thermal_scene.hpp"

float ThermalScene::getCustomPropertyFloat(tamashii::RefModel_s* refModel, std::string key)
{
	tamashii::Value value = refModel->model->getCustomProperty(key);
	return (value.getType() == tamashii::Value::Type::INT) ? value.getInt() : value.getFloat();
}

void ThermalScene::load(scene_s _scene)
{
	CHECK_EMPTY_SCENE(_scene)

	mObjects.count = _scene.refModels.size();
	//mObjects.reserve(mObjects.count);
	mObjects.resize(mObjects.count);

	mProperties.minAABB = glm::vec3(std::numeric_limits<float>::max());
	mProperties.maxAABB = glm::vec3(std::numeric_limits<float>::min());

	mProperties.geometryCount = 0;
	mProperties.vertexCount = 0;
	mProperties.triangleCount = 0;

	unsigned int i = 0;
	for (RefModel_s* refModel : _scene.refModels) {
		
		mObjects.vertexOffset[i] = mProperties.vertexCount;
		//mObjects.vertexOffset.push_back(mProperties.vertexCount);

		initObject(refModel, i);

		glm::mat4 model_matrix = glm::transpose(refModel->model_matrix);
		for (RefMesh_s* refMesh : refModel->refMeshes) {
			Mesh* m = refMesh->mesh;
			for (int vi = 0; vi < m->getVertexCount(); vi++) {
				glm::vec4 v0_ = refModel->model_matrix * m->getVerticesVector()->at(vi).position;
				glm::vec3 v0 = glm::vec3(v0_.x, v0_.y, v0_.z) / v0_.w;
				mProperties.minAABB = glm::min(mProperties.minAABB, v0);
				mProperties.maxAABB = glm::max(mProperties.maxAABB, v0);
			}

			// index
			if (m->hasIndices())
				mProperties.triangleCount += m->getIndexCount() / 3;
			else
				mProperties.triangleCount += m->getVertexCount() / 3;

			// vertex
			mProperties.vertexCount += m->getVertexCount();

			// geometry
			mProperties.geometryCount++;
		}

		mObjects.vertexCount[i] = mProperties.vertexCount - mObjects.vertexOffset[i];
		//mObjects.vertexCount.push_back(mProperties.vertexCount - mObjects.vertexOffset[i]);

		i++;
	}

	const glm::vec3& min = mProperties.minAABB;
	const glm::vec3& max = mProperties.maxAABB;

	spdlog::debug("Scene AABB min: {}, {}, {}", min.x, min.y, min.z);
	spdlog::debug("Scene AABB max: {}, {}, {}", max.x, max.y, max.z);

	glm::vec3 half_diagonal = (max - max) * 0.5f;
	glm::vec3 center = min + half_diagonal;
	glm::vec4 bSphere = glm::vec4(center.x, center.y, center.z, glm::length(half_diagonal));
	mProperties.boundingSphere = bSphere;

	spdlog::debug("mBoundingSphere center: ({}, {}, {})", bSphere.x, bSphere.y, bSphere.z);
	spdlog::debug("mBoundingSphere radius: {}", bSphere.z);
}

void ThermalScene::initObject(RefModel_s* refModel, unsigned int i)
{
	mObjects.name[i] = std::string(refModel->model->getName());
	mObjects.model[i] = refModel;
	mObjects.kelvin[i] = getCustomPropertyFloat(refModel, "kelvin") * kelvinUnitFactor;
	mObjects.temperatureFixed[i] = refModel->model->getCustomProperty("temperature-fixed").getInt();
	mObjects.thickness[i] = getCustomPropertyFloat(refModel, "thickness");
	mObjects.density[i] = getCustomPropertyFloat(refModel, "density");
	mObjects.heatCapacity[i] = getCustomPropertyFloat(refModel, "heat-capacity") / (kelvinUnitFactor * pow(secondsUnitFactor, 2.0));
	mObjects.heatConductivity[i] = getCustomPropertyFloat(refModel, "heat-conductivity") / (kelvinUnitFactor * pow(secondsUnitFactor, 3.0));

	mObjects.diffuseReflectance[i] = getCustomPropertyFloat(refModel, "diffuse-reflectance");
	mObjects.specularReflectance[i] = getCustomPropertyFloat(refModel, "specular-reflectance");
	float total_reflectance = mObjects.diffuseReflectance[i] + mObjects.specularReflectance[i];
	if (total_reflectance > 1.0)
	{
		spdlog::warn("Invalid reflectance parameters: total reflectance > 0. Diffuse reflectance + specular reflectance need to be <= 1.0. Normalizing values.");
		mObjects.diffuseReflectance[i] /= total_reflectance;
		mObjects.specularReflectance[i] /= total_reflectance;
		total_reflectance = 1.0;
	}
	mObjects.absorption[i] = 1.0 - total_reflectance;
	mObjects.diffuseEmission[i] = refModel->model->getCustomProperty("diffuse-emission").getInt();
	mObjects.traceable[i] = (refModel->model->getCustomProperty("traceable").getInt() !=0 );
	//spdlog::warn("traceable {} prop-int {}" , mObjects.traceable[i], refModel->model->getCustomProperty("traceable").getInt());
	// WARNING: scene modification
	if( mObjects.traceable[i] ){
		refModel->mask = 0xff;
	}else{
		refModel->mask = 0x0f;
	}
	mObjects.print(i);
}

void ThermalScene::unload()
{
	mProperties = {};
	mObjects.clear();
}
