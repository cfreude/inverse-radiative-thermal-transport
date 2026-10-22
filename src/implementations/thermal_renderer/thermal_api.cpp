#include "thermal_api.hpp"

#include <iostream>

#include <tamashii/engine/render/render_backend_implementation.hpp>
#include <rvk/rvk.hpp>

#include <tamashii/engine/common/common.hpp>
#include <tamashii/engine/common/vars.hpp>

#include <spdlog/fmt/ostr.h>
#include <spdlog/stopwatch.h>
#include "spdlog/sinks/basic_file_sink.h"
#include <tamashii/tamashii.hpp>

#include "thermal_renderer_backend.hpp"

using namespace tamashii;

#include "thermal_renderer.hpp"

ThermalRenderer* lib_impl;
bool console_open = false;

struct ObjectProperties
{
	unsigned int	vertex_offset;
	unsigned int	vertex_count;
	unsigned int	indices_offset;
	unsigned int	indices_count;

	float			kelvin;
	bool			temperature_fixed;

	float			thickness;
	float			density;
	float			heat_capacity;
	float			heat_conductivity;

	float			diffuse_reflectance;
	float			specular_reflectance;

	bool			diffuse_emission;
	bool			traceable;
};

scene_s _test_load_scene()
{
	RenderScene* scene = Common::getInstance().getRenderSystem()->getMainScene();
	Model* model = Model::alloc();
	Material* mat = Material::alloc();
	Mesh* mesh = Mesh::alloc();

	std::vector<vertex_s> vertices;
	vertex_s v0, v1, v2, v3;
	v0.position = glm::vec4(-1.0, 1.0, 0.0, 1.0);
	v1.position = glm::vec4(1.0, 1.0, 0.0, 1.0);
	v2.position = glm::vec4(1.0, -1.0, 0.0, 1.0);
	v3.position = glm::vec4(-1.0, -1.0, 0.0, 1.0);

	v0.color_0 = glm::vec4(1.0, 1.0, 0.0, 1.0);
	v1.color_0 = glm::vec4(0.0, 1.0, 0.0, 1.0);
	v2.color_0 = glm::vec4(0.0, 0.0, 1.0, 1.0);
	v3.color_0 = glm::vec4(1.0, 0.0, 1.0, 1.0);

	vertices.push_back(v0);
	vertices.push_back(v1);
	vertices.push_back(v2);
	vertices.push_back(v3);

	mesh->setVertices(vertices);
	mesh->hasPositions(true);
	mesh->hasColors0(true);

	std::vector<uint32_t> indices{ 0,1,3, 1,2,3 };
	mesh->setIndices(indices);
	mesh->hasIndices(true);

	mesh->setMaterial(mat);
	model->addMesh(mesh);

	scene->addModelRef(model);

	return scene->getSceneData();
}

scene_s _load_geometry(
	float* _vertices,
	float* _vertex_colors,
	unsigned int _total_vertex_count,
	unsigned int* _indices,
	unsigned int _total_indices_count,
	ObjectProperties* _object_properties,
	unsigned int _object_count)
{
	RenderScene* scene = Common::getInstance().getRenderSystem()->getMainScene();

	for (int oi = 0; oi < _object_count; oi++)
	{
		Model* model = Model::alloc();
		Material* mat = Material::alloc();
		Mesh* mesh = Mesh::alloc();

		unsigned int vertex_count = _object_properties[oi].vertex_count * 3;

		std::vector<vertex_s> vertices;
		vertices.reserve(vertex_count);
		unsigned int start = _object_properties[oi].vertex_offset * 3;
		unsigned int stop = start + vertex_count;
		for (unsigned int i = start; i < stop; i += 3)
		{
			vertex_s v;
			v.position = glm::vec4(_vertices[i], _vertices[i + 1], _vertices[i + 2], 1.0);
			v.color_0 = glm::vec4(_vertex_colors[i], _vertex_colors[i + 1], _vertex_colors[i + 2], 1.0);
			vertices.push_back(v);
		}

		mesh->setVertices(vertices);
		mesh->hasPositions(true);
		mesh->hasColors0(true);
		mesh->setTopology(Mesh::Topology::TRIANGLE_LIST);

		unsigned int ind_start = _object_properties[oi].indices_offset * 3;
		unsigned int ind_stop = ind_start + _object_properties[oi].indices_count * 3;
		std::vector<uint32_t> indices(_indices + ind_start, _indices + ind_stop);
		mesh->setIndices(indices);
		mesh->hasIndices(true);

		mesh->setMaterial(mat);
		model->addMesh(mesh);

		RefModel_s* refModel = new RefModel_s();
		refModel->mask = 0xf0;
		RefMesh_s* refMesh = new RefMesh_s();
		refMesh->mesh = mesh;

		ObjectProperties obj_props = _object_properties[oi];

		spdlog::info("_object_properties: {}, {}, {}", obj_props.kelvin, obj_props.diffuse_emission, obj_props.temperature_fixed);
		spdlog::info("_object_material: {}, {}, {}", obj_props.density, obj_props.heat_capacity, obj_props.heat_conductivity);
		spdlog::info("_vertex_: {}, {}", obj_props.vertex_count, obj_props.vertex_offset);
		spdlog::info("_indices_: {}, {}", obj_props.indices_count, obj_props.indices_offset);

		std::ostringstream stringStream;
		stringStream << "Object" << oi;
		model->setName(stringStream.str());

		model->addCustomProperty("kelvin", Value(obj_props.kelvin));
		model->addCustomProperty("temperature-fixed", Value(int(obj_props.temperature_fixed)));
		model->addCustomProperty("thickness", Value(obj_props.thickness));
		model->addCustomProperty("density", Value(obj_props.density));
		model->addCustomProperty("heat-capacity", Value(obj_props.heat_capacity));
		model->addCustomProperty("heat-conductivity", Value(obj_props.heat_conductivity));
		model->addCustomProperty("diffuse-reflectance", Value(obj_props.diffuse_reflectance));
		model->addCustomProperty("specular-reflectance", Value(obj_props.specular_reflectance));
		model->addCustomProperty("diffuse-emission", Value(int(obj_props.diffuse_emission)));
		model->addCustomProperty("traceable", Value(int(obj_props.traceable)));
		model->addCustomProperty("animation-index", Value(-1));

		scene->addModelRef(model);
	}

	return scene->getSceneData();
}

scene_s _load_sky(
	float* _vertices,
	unsigned int _vertex_count,
	unsigned int* _indices,
	float* _values,
	unsigned int _quad_count)
{
	RenderScene* scene = Common::getInstance().getRenderSystem()->getMainScene();

	Model* model = Model::alloc();
	Material* mat = Material::alloc();
	Mesh* mesh = Mesh::alloc();

	std::vector<vertex_s> vertices;
	vertices.reserve(_vertex_count);
	for (unsigned int i = 0; i < _vertex_count * 3; i += 3)
	{
		vertex_s v;
		v.position = glm::vec4(_vertices[i], _vertices[i + 1], _vertices[i + 2], 1.0);
		vertices.push_back(v);
	}

	unsigned int triangle_indices_count = _quad_count * 2 * 3;

	std::vector<uint32_t> indices;
	indices.reserve(triangle_indices_count);

	glm::vec4 bs = lib_impl->getBoundingSphere();
	glm::vec3 sphere_center = glm::vec3(bs.x, bs.y, bs.z);
	float sphere_radius = bs.w;

	float vertex_scaling = glm::sqrt(sphere_radius * sphere_radius + sphere_radius * sphere_radius) * 1.2;

	for (unsigned int i = 0; i < _quad_count*4; i += 4)
	{
		unsigned int ia = _indices[i];
		unsigned int ib = _indices[i + 1];
		unsigned int ic = _indices[i + 2];
		unsigned int id = _indices[i + 3];

		// split quad into triangles

		indices.push_back(ic);
		indices.push_back(ib);
		indices.push_back(id);

		indices.push_back(ib);
		indices.push_back(ia);
		indices.push_back(id);

		// offset vertices to fit scene bounding sphere 
		
		glm::vec3 va = vertices[ia].position;
		glm::vec3 vb = vertices[ib].position;
		glm::vec3 vc = vertices[ic].position;
		glm::vec3 vd = vertices[id].position;

		glm::vec3 quad_center = (va + vb + vc + vd) * 0.25f;

		glm::vec3 qvab = vb - va;
		glm::vec3 qvac = vc - va;
		glm::vec3 qvad = vd - va;
		float min_length = glm::min(glm::min(glm::length(qvab), glm::length(qvac)), glm::length(qvad));

		glm::vec3 quad_normal = glm::normalize(glm::cross(qvab, qvac));

		glm::vec3 quad_to_sphere_dir = sphere_center - quad_center;
		float projection_distance = glm::dot(quad_normal, quad_to_sphere_dir);
		glm::vec3 projection_on_plane = sphere_center - quad_normal * projection_distance;
		
		for(int j = 0; j < 4; j++)
		{
			unsigned int ind = _indices[i + j];
			glm::vec3 v = vertices[ind].position;
			glm::vec3 center_to_v = v - quad_center;
			center_to_v = glm::normalize(center_to_v) * vertex_scaling;

			//v = quad_center + center_to_v;
			v = projection_on_plane + center_to_v;

			vertices[ind].position = glm::vec4(v.x, v.y, v.z, 1.0);
		}

	}

	mesh->setIndices(indices);
	mesh->hasIndices(true);

	mesh->setVertices(vertices);
	mesh->hasPositions(true);
	mesh->setTopology(Mesh::Topology::TRIANGLE_LIST);

	mesh->setMaterial(mat);
	model->addMesh(mesh);

	RefModel_s* refModel = new RefModel_s();
	refModel->mask = 0x0f;
	RefMesh_s* refMesh = new RefMesh_s();
	refMesh->mesh = mesh;

	model->setName("Sky");

	std::vector<Value> valueArray;
	float avg = 0;
	for (unsigned int i = 0; i < _quad_count; i++)
	{
		float val = _values[i];
		avg += val;
		valueArray.emplace_back(Value(val));
	}
	Value pvk(valueArray);
	avg /= _vertex_count;

	model->addCustomProperty("sky-values", pvk);
	model->addCustomProperty("kelvin", Value(0.0f));
	model->addCustomProperty("temperature-fixed", Value(int(true)));
	model->addCustomProperty("thickness", Value(1.0f));
	model->addCustomProperty("density", Value(1.0f));
	model->addCustomProperty("heat-capacity", Value(1.0f));
	model->addCustomProperty("heat-conductivity", Value(0.0f));
	model->addCustomProperty("diffuse-reflectance", Value(0.0f));
	model->addCustomProperty("specular-reflectance", Value(0.0f));
	model->addCustomProperty("diffuse-emission", Value(int(false)));
	model->addCustomProperty("traceable", Value(int(false)));
	model->addCustomProperty("animation-index", Value(-1));
	model->addCustomProperty("trace-bounding-cone", Value(int(false)));

	scene->addModelRef(model);

	return scene->getSceneData();
}

#ifdef DISABLE_GUI

extern "C" int load(bool _withConsole)
{
	if (_withConsole)
	{
		sys::createConsole("Thermal Renderer Console");
		console_open = true;
	}
	else
	{
		try
		{
			auto logger = spdlog::basic_logger_mt("file_logger", "./thermal-simulation-log.txt", true);
			spdlog::set_default_logger(logger);
			spdlog::flush_on(spdlog::level::info);
			spdlog::set_level(spdlog::level::debug);
			spdlog::info("File logger init done!");
		}
		catch (const spdlog::spdlog_ex& ex)
		{
			std::cout << "Log init failed: " << ex.what() << std::endl;
			//assert(false);//, "File logger init failed!");
		}
	}

	// use our vulkan backend
	tamashii::addBackend(new VulkanThermalRenderBackendApi());
	tamashii::var::headless.setValue("1");
	Common::getInstance().init(0, NULL, NULL);
	lib_impl = static_cast<ThermalRenderer*>(tamashii::findBackendImplementation(THERMAL_RENDERER_NAME));

	//_load_scene();
	return 0;
}

extern "C" int test_load_scene()
{
	_test_load_scene();
	return 0;
}

extern "C" int unload(){
	if (console_open)
		sys::destroyConsole();
	Common::getInstance().shutdown();
	return 0;
}

extern "C" int test_lib(int _value)
{
	return _value * 2;
}

extern "C" int test_fill_array(float* _arr, int _size)
{
	for (int i = 0; i < _size; i++)
		_arr[i] = _arr[i] * float(i * 3);

	return 0;
}

extern "C" int load_geometry(
	float* _vertices,
	float* _vertex_colors,
	unsigned int _total_vertex_count,
	unsigned int* _indices,
	unsigned int _total_indices_count,
	ObjectProperties * _object_properties,
	unsigned int _object_count
)
{
	spdlog::info("_vertex_count {}", _total_vertex_count);
	spdlog::info("_indices_count {}", _total_indices_count);
	spdlog::info("_object_count {}", _object_count);

	spdlog::stopwatch total_sw;
	spdlog::stopwatch sw;

	scene_s scene = _load_geometry(
		_vertices,
		_vertex_colors,
		_total_vertex_count,
		_indices,
		_total_indices_count,
		_object_properties,
		_object_count);

	lib_impl->sky_vertex_offset = _total_vertex_count / 3;

	lib_impl->computeSceneAABB();

	spdlog::info("---> _process_mesh dur.: {}", sw);
	sw.reset();

	/*
	lib_impl->sceneLoad(scene);
	spdlog::info("---> sceneLoad dur.: {}", sw);
	sw.reset();
	lib_impl->resetSimulation();
	spdlog::info("---> resetSimulation dur.: {}", sw);
	sw.reset();	

	spdlog::info("---> load_geometry total dur.: {}", total_sw);
	if(!console_open)
		spdlog::get("file_logger")->flush();
	*/
	return 0;
}

extern "C" int unload_scene()
{
	spdlog::stopwatch sw;
	lib_impl->_sceneUnload();
	spdlog::info("---> sceneUnload dur.: {}", sw);
	return 0;
}

extern "C" int load_sky(
	float* _vertices,
	unsigned int _vertex_count,
	unsigned int* _indices,
	float* _values,
	unsigned int _quad_count)
{
	spdlog::stopwatch total_sw;
	spdlog::stopwatch sw;

	scene_s scene = _load_sky(
		_vertices,
		_vertex_count,
		_indices,
		_values,
		_quad_count);

	lib_impl->sky_vertex_count = _vertex_count;
	
	lib_impl->sceneLoad(scene);
	spdlog::info("---> sceneLoad dur.: {}", sw);
	sw.reset();
	lib_impl->resetSimulation();
	spdlog::info("---> resetSimulation dur.: {}", sw);
	sw.reset();

	spdlog::info("---> load_geometry total dur.: {}", total_sw);
	if(!console_open)
		spdlog::get("file_logger")->flush();
	
	return 0;
}

extern "C" int update_sky_values(
	float* _values,
	unsigned int _quad_count)
{
	spdlog::stopwatch sw;
	lib_impl->setKelvin(_values, _quad_count, lib_impl->sky_vertex_offset, true);
	spdlog::info("---> update_sky_values dur.: {}", sw);

	return 0;
}

extern "C" int simulate(
	float _step_size_hours,
	unsigned int _time_step_count,
	float* _time_hours)
{
	spdlog::stopwatch sw;

	spdlog::info("_step_size_hours {}", _step_size_hours);
	spdlog::info("_time_step_count {}", _time_step_count);

	spdlog::info("Start Time: {}", (*_time_hours));

	lib_impl->setTimeStep(_step_size_hours);
	lib_impl->setTime(*_time_hours);
	for (int i = 0; i < _time_step_count; i++)
		lib_impl->thermalTimestep();
	(*_time_hours) = lib_impl->getTimeHours();

	spdlog::info("End Time: {}", (*_time_hours));

	spdlog::info("---> thermalTimesteps dur.: {}", sw);

	return 0;
}

extern "C" int reset_simulation()
{
	lib_impl->resetSimulation();
	return 0;
}

extern "C" int set_ray_batch_count(unsigned int _ray_count, unsigned int _batch_count)
{
	spdlog::info("---> ray count = {}, batch count = {}", _ray_count, _batch_count);
	lib_impl->setRayBatchCount(_ray_count, _batch_count);
	return 0;
}

extern "C" int set_minimum_sky_kelvin(float _min)
{
	spdlog::info("---> set_minimum_sky_kelvin = {}", _min);
	lib_impl->sky_min_kelvin = _min;
	return 0;
}

extern "C" int set_steady_state(bool _enabled)
{
	lib_impl->setSteadState(_enabled);
	return 0;
}

extern "C" int get_vertex_temperatures(
	float* _vertex_temperatures,
	unsigned int _total_vertex_count)
{
	spdlog::stopwatch sw;

	lib_impl->getKelvin(_vertex_temperatures, _total_vertex_count);
	spdlog::info("---> getKelvin dur.: {}", sw);

	return 0;
}

extern "C" int get_vertex_values(
	float* _vertex_temperatures,
	unsigned int _total_vertex_count,
	unsigned int _type)
{
	spdlog::stopwatch sw;

	lib_impl->getValue(_vertex_temperatures, _total_vertex_count, _type);
	spdlog::info("---> getValue (type: {}) dur.: {}", _type, sw);

	return 0;
}

#endif // !DISABLE_GUI