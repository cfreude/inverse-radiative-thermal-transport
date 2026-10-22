// define before including
// #define CONV_TLAS_BINDING
// #define CONV_TLAS_SET
// #define CONV_GEOMETRY_BUFFER_BINDING
// #define CONV_GEOMETRY_BUFFER_SET
#ifndef AS_DATA_GLSL
#define AS_DATA_GLSL

// optional
#if defined(CONV_TLAS_BINDING) && defined(CONV_TLAS_SET)
layout(binding = CONV_TLAS_BINDING, set = CONV_TLAS_SET) uniform accelerationStructureEXT tlas;
#endif

// optional
#if defined(CONV_GEOMETRY_BUFFER_BINDING) && defined(CONV_GEOMETRY_BUFFER_SET)
struct GeometryData_s {
	mat4 model_matrix;
	uint index_buffer_offset;
	uint vertex_buffer_offset;
	uint has_indices;
	uint material_index;
};
layout(binding = CONV_GEOMETRY_BUFFER_BINDING, set = CONV_GEOMETRY_BUFFER_SET) buffer geometry_storage_buffer { GeometryData_s geometry_buffer[]; };
#endif

#endif 	// AS_DATA_GLSL