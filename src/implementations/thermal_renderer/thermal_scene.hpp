#pragma once

#include <glm/glm.hpp>

#include <tamashii/engine/scene/render_scene.hpp>
#include <tamashii/engine/render/render_backend_implementation.hpp>

#include "thermal_common.hpp"
#include "thermal_objects.hpp"

T_USE_NAMESPACE

class ThermalScene {

public:
		
	typedef struct SceneProperties_s {
		glm::vec3 minAABB = glm::vec3();
		glm::vec3 maxAABB = glm::vec3();
		glm::vec4 boundingSphere = glm::vec4(); // xyz, radius
		unsigned int geometryCount = 0;
		unsigned int vertexCount = 0;
		unsigned int triangleCount = 0;
	} SceneProperties_s;

	void load(scene_s _scene);
	void initObject(RefModel_s* refModel, unsigned int _i);
	void unload();

	SceneProperties_s& getProperties() { return mProperties; };
	ThermalObjects& getObjects() { return mObjects; };

private:

	static float getCustomPropertyFloat(tamashii::RefModel_s* refModel, std::string key);

	SceneProperties_s mProperties;
	ThermalObjects mObjects;
};