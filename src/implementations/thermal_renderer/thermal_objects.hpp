#pragma once

#include "thermal_common.hpp"

#include <tamashii/engine/scene/render_scene.hpp>
#include <tamashii/engine/render/render_backend_implementation.hpp>

#include <string>

using namespace tamashii;

class ThermalObjects {

public:

	std::vector<std::string> name;
	std::vector<RefModel_s*> model;

	// thermal values
	Vec	kelvin;
	Vec	thickness;
	Vec	density;
	Vec	heatCapacity;
	Vec	heatConductivity;

	// radiation values	
	Vec	diffuseReflectance;
	Vec	specularReflectance;
	Vec	absorption;

	// simulation flags
	std::vector<bool> diffuseEmission;
	std::vector<bool> temperatureFixed;
	std::vector<bool> traceable;

	std::vector<unsigned int> vertexCount;
	std::vector<unsigned int> vertexOffset;

	void reserve(unsigned int _size);
	void resize(unsigned int _size);
	void clear();
	void print(unsigned int _index) const;
	const char * getUiStr(unsigned int index) const;

	void setVertexColors(const Vec& _vals) const;

	unsigned int count = 0;

private:

	// ...

};