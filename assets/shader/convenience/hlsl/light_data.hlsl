// define before including
// #define CONV_LIGHT_BUFFER_BINDING
// #define CONV_LIGHT_BUFFER_SET
#ifndef LIGHT_DATA_HLSL
#define LIGHT_DATA_HLSL
#if defined(CONV_LIGHT_BUFFER_BINDING) && defined(CONV_LIGHT_BUFFER_SET)

#define LIGHT_TYPE_POINT            0x00000001
#define LIGHT_TYPE_SPOT             0x00000002
#define LIGHT_TYPE_DIRECTIONAL      0x00000004
#define LIGHT_TYPE_IES      		0x00000008
// light type masks
#define LIGHT_TYPE_PUNCTUAL         (LIGHT_TYPE_POINT | LIGHT_TYPE_SPOT | LIGHT_TYPE_DIRECTIONAL)
struct Light_s {
	// ALL
	float3						    color;
	float							intensity;
	// LIGHT_TYPE_POINT
	// LIGHT_TYPE_SPOT
	// LIGHT_TYPE_IES
	float3						    pos_ws;
	float                           _;
	// LIGHT_TYPE_SPOT
	// LIGHT_TYPE_DIRECTIONAL
	// LIGHT_TYPE_IES
	float3						    n_ws_norm;
	float                           __;
	float3						    t_ws_norm;
	float                           ___;
	// LIGHT_TYPE_POINT
	// LIGHT_TYPE_SPOT
	float							range;
	// LIGHT_TYPE_PUNCTUAL
	float							light_offset;		// radius for point/spot and angle for directional
	// LIGHT_TYPE_SPOT
	float							inner_angle;
	float							outer_angle;
	float							light_angle_scale;
	float							light_angle_offset;
	// LIGHT_TYPE_IES
	int								texture_index;
	float							min_vertical_angle;
	float							max_vertical_angle;
	float							min_horizontal_angle;
	float							max_horizontal_angle;
	// LIGHT_TYPE_*
	uint							type;
};

[[vk::binding(CONV_LIGHT_BUFFER_BINDING, CONV_LIGHT_BUFFER_SET)]] StructuredBuffer<Light_s> light_buffer;

// helper functions
bool isPunctualLight(const uint idx){
	return (light_buffer[idx].type & LIGHT_TYPE_PUNCTUAL) > 0;
}
bool isPointLight(const uint idx){
	return light_buffer[idx].type == LIGHT_TYPE_POINT;
}
bool isSpotLight(const uint idx){
	return light_buffer[idx].type == LIGHT_TYPE_SPOT;
}
bool isDirectionalLight(const uint idx){
	return light_buffer[idx].type == LIGHT_TYPE_DIRECTIONAL;
}
bool isIesLight(const uint idx){
	return light_buffer[idx].type == LIGHT_TYPE_IES;
}

#endif
#endif 	// LIGHT_DATA_HLSL