#pragma once

#ifdef DISABLE_GUI

#include "thermal_renderer_lib_Export.h"
#include "thermal_renderer.hpp"

extern "C" thermal_renderer_lib_EXPORT struct ObjectProperties;

extern "C" thermal_renderer_lib_EXPORT int load(bool _withConsole);
extern "C" thermal_renderer_lib_EXPORT int unload();

// testing
extern "C" thermal_renderer_lib_EXPORT int test_load_scene();
extern "C" thermal_renderer_lib_EXPORT int test_lib(int _value);
extern "C" thermal_renderer_lib_EXPORT int test_fill_array(float* _arr, int _size);

// rhino interface
extern "C" thermal_renderer_lib_EXPORT int load_geometry(
	float* _vertices,
	float* _vertex_colors,
	unsigned int _total_vertex_count,
	unsigned int* _indices,
	unsigned int _total_indices_count,
	ObjectProperties * _object_properties,
	unsigned int _object_count);

extern "C" thermal_renderer_lib_EXPORT int load_sky(
	float* _vertices,
	unsigned int _vertex_count,
	unsigned int* _indices,
	float* _values,
	unsigned int _quad_count);

extern "C" thermal_renderer_lib_EXPORT int unload_scene();

extern "C" thermal_renderer_lib_EXPORT int update_sky_values(
	float* _values,
	unsigned int _quad_count);

extern "C" thermal_renderer_lib_EXPORT int reset_simulation();

extern "C" thermal_renderer_lib_EXPORT int set_ray_batch_count(unsigned int _ray_count, unsigned int _batch_count);

extern "C" thermal_renderer_lib_EXPORT int set_minimum_sky_kelvin(float _min);

extern "C" thermal_renderer_lib_EXPORT int set_steady_state(bool _enabled);

extern "C" thermal_renderer_lib_EXPORT int simulate(
	float _step_size_hours,
	unsigned int _time_step_count,
	float* _time_hours);

extern "C" thermal_renderer_lib_EXPORT int get_vertex_temperatures(
	float* _vertex_temperatures,
	unsigned int _total_vertex_count);

extern "C" thermal_renderer_lib_EXPORT int get_vertex_values(
	float* _vertex_temperatures,
	unsigned int _total_vertex_count,
	unsigned int _type);

#endif //!DISABLE_GUI