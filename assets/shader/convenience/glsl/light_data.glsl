// define before including
// #define CONV_LIGHT_BUFFER_BINDING
// #define CONV_LIGHT_BUFFER_SET
// when using ies lights include texture_data.glsl before this
#ifndef LIGHT_DATA_GLSL
#define LIGHT_DATA_GLSL
#if defined(CONV_LIGHT_BUFFER_BINDING) && defined(CONV_LIGHT_BUFFER_SET)

#define LIGHT_TYPE_POINT            0x00000001u
#define LIGHT_TYPE_SPOT             0x00000002u
#define LIGHT_TYPE_DIRECTIONAL      0x00000004u
#define LIGHT_TYPE_SQUARE			0x00000008u
#define LIGHT_TYPE_RECTANGLE		0x00000010u
#define LIGHT_TYPE_CUBE				0x00000020u
#define LIGHT_TYPE_CUBOID			0x00000040u
#define LIGHT_TYPE_DISK				0x00000080u
#define LIGHT_TYPE_ELLIPSE			0x00000100u
#define LIGHT_TYPE_SPHERE			0x00000200u
#define LIGHT_TYPE_ELLIPSOID		0x00000400u
#define LIGHT_TYPE_IES				0x00000800u
#define LIGHT_TYPE_TRIANGLE_MESH	0x00001000u
// light type masks
#define LIGHT_TYPE_PUNCTUAL         (LIGHT_TYPE_POINT | LIGHT_TYPE_SPOT | LIGHT_TYPE_DIRECTIONAL)
#define LIGHT_TYPE_HYPERRECTANGULAR	(LIGHT_TYPE_SQUARE | LIGHT_TYPE_RECTANGLE | LIGHT_TYPE_CUBE | LIGHT_TYPE_CUBOID)
#define LIGHT_TYPE_ELLIPTICAL		(LIGHT_TYPE_DISK | LIGHT_TYPE_ELLIPSE | LIGHT_TYPE_SPHERE | LIGHT_TYPE_ELLIPSOID)
#define LIGHT_TYPE_SURFACE			(LIGHT_TYPE_HYPERRECTANGULAR | LIGHT_TYPE_ELLIPTICAL)
struct Light_s {
	// ALL
	vec3						    color;
	float							intensity;
	// LIGHT_TYPE_POINT
	// LIGHT_TYPE_SPOT
	// LIGHT_TYPE_SURFACE
	// LIGHT_TYPE_IES
	vec4						    pos_ws;
	// LIGHT_TYPE_SPOT
	// LIGHT_TYPE_DIRECTIONAL
	// LIGHT_TYPE_SURFACE
	// LIGHT_TYPE_IES
	vec4						    n_ws_norm;
	vec4						    t_ws_norm;
	// LIGHT_TYPE_SURFACE
	vec3							dimensions;			// width, height, depth for HYPERRECTANGULAR, diameter for ELLIPTICAL
	float							____;
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
	float							min_vertical_angle;
	float							max_vertical_angle;
	float							min_horizontal_angle;
	float							max_horizontal_angle;
	// LIGHT_TYPE_IES
	// LIGHT_TYPE_TRIANGLE_MESH
	int								texture_index; // -1 if no texture
	// LIGHT_TYPE_TRIANGLE_MESH
	uint							triangle_count;
	int								id;
	uint							index_buffer_offset; 	// -1 if no indices used
	uint							vertex_buffer_offset;
	// LIGHT_TYPE_*
	uint							type;
};

layout(binding = CONV_LIGHT_BUFFER_BINDING, set = CONV_LIGHT_BUFFER_SET) buffer light_storage_buffer { Light_s light_buffer[]; };

// helper functions
// high level types
bool isPunctualLight(const uint idx){
	return (light_buffer[idx].type & LIGHT_TYPE_PUNCTUAL) > 0;
}
bool isSurfaceLight(const uint idx){
	return (light_buffer[idx].type & LIGHT_TYPE_SURFACE) > 0;
}
bool isIesLight(const uint idx){
	return light_buffer[idx].type == LIGHT_TYPE_IES;
}
// low level types
// LIGHT_TYPE_PUNCTUAL
bool isPointLight(const uint idx){
	return light_buffer[idx].type == LIGHT_TYPE_POINT;
}
bool isSpotLight(const uint idx){
	return light_buffer[idx].type == LIGHT_TYPE_SPOT;
}
bool isDirectionalLight(const uint idx){
	return light_buffer[idx].type == LIGHT_TYPE_DIRECTIONAL;
}
// LIGHT_TYPE_SURFACE
bool isHyperrectangularLight(const uint idx){
	return (light_buffer[idx].type & LIGHT_TYPE_HYPERRECTANGULAR) > 0;
}
bool isEllipticalLight(const uint idx){
	return (light_buffer[idx].type & LIGHT_TYPE_ELLIPTICAL) > 0;
}
// LIGHT_TYPE_HYPERRECTANGULAR
bool isSquareLight(const uint idx){
	return light_buffer[idx].type == LIGHT_TYPE_SQUARE;
}
bool isRectangleLight(const uint idx){
	return light_buffer[idx].type == LIGHT_TYPE_RECTANGLE;
}
bool isCubeLight(const uint idx){
	return light_buffer[idx].type == LIGHT_TYPE_CUBE;
}
bool isCuboidLight(const uint idx){
	return light_buffer[idx].type == LIGHT_TYPE_CUBOID;
}
// LIGHT_TYPE_ELLIPTICAL
bool isDiskLight(const uint idx){
	return light_buffer[idx].type == LIGHT_TYPE_DISK;
}
bool isEllipseLight(const uint idx){
	return light_buffer[idx].type == LIGHT_TYPE_ELLIPSE;
}
bool isSphereLight(const uint idx){
	return light_buffer[idx].type == LIGHT_TYPE_SPHERE;
}
bool isEllipsoidLight(const uint idx){
	return light_buffer[idx].type == LIGHT_TYPE_ELLIPSOID;
}

vec3 iesWorldSpaceToTangentSpace(const vec3 v, const vec3 n, const vec3 t){
	// bitangent
	const vec3 b = normalize(cross(n, t));
	// construct mat3 with the columns t b n
	// T_x, B_x, N_x
	// T_y, B_y, N_y
	// T_z, B_z, N_z
	return transpose(mat3(t, b, n)) * v;
}
// https://stackoverflow.com/a/42179924
vec4 iesCubic(float v){
    vec4 n = vec4(1.0, 2.0, 3.0, 4.0) - v;
    vec4 s = n * n * n;
    float x = s.x;
    float y = s.y - 4.0 * s.x;
    float z = s.z - 4.0 * s.y + 6.0 * s.x;
    float w = 6.0 - x - y - z;
    return vec4(x, y, z, w) * (1.0/6.0);
}

float iesTextureBicubic(sampler2D sam, vec2 texCoords){
	vec2 texSize = textureSize(sam, 0);
	vec2 invTexSize = 1.0 / texSize;
	texCoords = texCoords * texSize - 0.5;

    vec2 fxy = fract(texCoords);
    texCoords -= fxy;

    vec4 xcubic = iesCubic(fxy.x);
    vec4 ycubic = iesCubic(fxy.y);

    vec4 c = texCoords.xxyy + vec2 (-0.5, +1.5).xyxy;
    vec4 s = vec4(xcubic.xz + xcubic.yw, ycubic.xz + ycubic.yw);
    vec4 offset = c + vec4 (xcubic.yw, ycubic.yw) / s;
    offset *= invTexSize.xxyy;

    float sample0 = texture(sam, offset.xz).x;
    float sample1 = texture(sam, offset.yz).x;
    float sample2 = texture(sam, offset.xw).x;
    float sample3 = texture(sam, offset.yw).x;

    float sx = s.x / (s.x + s.y);
    float sy = s.z / (s.z + s.w);

    return mix(mix(sample3, sample2, sx), mix(sample1, sample0, sx), sy);
}


#if defined(CONV_TEXTURE_BINDING) && defined(CONV_TEXTURE_SET)
// transform v from [a,b] to [c,d]
float transformInterval(float v, float a, float b, float c, float d) {
	const float A = (d-c)/(b-a);
	return c + A * (v-a);
}
vec2 transformInterval(vec2 v, vec2 a, vec2 b, vec2 c, vec2 d) {
	const float v1 = transformInterval(v.x, a.x, b.x, c.x, d.x);
	const float v2 = transformInterval(v.y, a.y, b.y, c.y, d.y);
	return vec2(v1, v2);
}

// offset = 0.50f for bilinear
// offset = 1.00f for cubic
vec2 getIesUVs(const uint idx, vec3 dir, const uint texIdx, const float offset){
	dir = normalize(dir);
	const vec3 b_ws_norm = normalize(cross(light_buffer[idx].n_ws_norm.xyz, light_buffer[idx].t_ws_norm.xyz));
	const float vAngle = degrees(acos(dot(light_buffer[idx].n_ws_norm.xyz, dir)));
	const float hAngle = degrees(atan(dot(b_ws_norm, dir), dot(light_buffer[idx].t_ws_norm.xyz, dir)));

	// calc uvs; either within texture [0,1] or outside
    vec2 uv = vec2(0.0f);
	// u should be within 0 - 1 for dirs were data is available, otherwise -> clamp to border
	const float vertical_denom = light_buffer[idx].max_vertical_angle - light_buffer[idx].min_vertical_angle;
    if(vertical_denom != 0.0f) {
		uv.x = (vAngle - light_buffer[idx].min_vertical_angle) / vertical_denom;
	}
	// v can be any value -> mirror repeat
    const float horizontal_denom = light_buffer[idx].max_horizontal_angle - light_buffer[idx].min_horizontal_angle;
	if(horizontal_denom != 0.0f) {
		uv.y = (hAngle - light_buffer[idx].min_horizontal_angle) / horizontal_denom;
	}

	// important adjustments to make sure the pixel center is read
	const vec2 texelSize = 1.0f / textureSize(texture_sampler[texIdx], 0);
	const vec2 texelSizeHalf = offset * texelSize;
	const vec2 range = vec2(1.0f) - texelSize;		// what is the effective range in uvs
	const vec2 correction = floor(uv) * texelSize;	// how often is the texture repeated and in which direction +/-
	uv = (texelSizeHalf + range * uv) + correction;

	return uv;
}
vec2 getIesUVsBilinear(const uint idx, vec3 dir){
	const uint texIdx = light_buffer[idx].texture_index;
	return getIesUVs(idx, dir, texIdx, 0.50f);
}
float evalIesLightBilinear(const uint idx, vec3 dir){
	const uint texIdx = light_buffer[idx].texture_index;
	return texture(texture_sampler[texIdx], getIesUVs(idx, dir, texIdx, 0.50f)).r;
}
float evalIesLightCubic(const uint idx, vec3 dir){
	const uint texIdx = light_buffer[idx].texture_index;
	return iesTextureBicubic(texture_sampler[texIdx], getIesUVs(idx, dir, texIdx, 1.00f)).r;
}
#endif // CONV_TEXTURE_BINDING && CONV_TEXTURE_SET

#if defined(VERTEX_DATA_GLSL) && defined(MATERIAL_DATA_GLSL)
bool isTriangleMeshLight(const uint idx){
	return light_buffer[idx].type == LIGHT_TYPE_TRIANGLE_MESH;
}
#endif // VERTEX_DATA_GLSL && MATERIAL_DATA_GLSL

#endif // CONV_LIGHT_BUFFER_BINDING && CONV_LIGHT_BUFFER_SET
#endif // LIGHT_DATA_GLSL