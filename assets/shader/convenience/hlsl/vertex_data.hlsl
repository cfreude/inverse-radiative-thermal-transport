// define before including
// #define CONV_INDEX_BUFFER_BINDING
// #define CONV_INDEX_BUFFER_SET
// #define CONV_VERTEX_BUFFER_BINDING
// #define CONV_VERTEX_BUFFER_SET
#ifndef VERTEX_DATA_HLSL
#define VERTEX_DATA_HLSL

// optional
#if defined(CONV_INDEX_BUFFER_BINDING) && defined(CONV_INDEX_BUFFER_SET)
[[vk::binding(CONV_INDEX_BUFFER_BINDING, CONV_INDEX_BUFFER_SET)]] StructuredBuffer<uint> index_buffer;
#endif

#if defined(CONV_VERTEX_BUFFER_BINDING) && defined(CONV_VERTEX_BUFFER_SET)
struct vertex_s {
	float4	position;
	float4	normal;	
	float4	tangent;

	float2	texture_coordinates_0;
	float2	texture_coordinates_1;
	float4	color_0;
};
[[vk::binding(CONV_VERTEX_BUFFER_BINDING, CONV_VERTEX_BUFFER_SET)]] StructuredBuffer<vertex_s> vertex_buffer;
#endif

#endif 	// VERTEX_DATA_HLSL