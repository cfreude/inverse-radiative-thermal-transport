#ifndef GLSL_RENDERING_UTILS
#define GLSL_RENDERING_UTILS
#extension GL_EXT_control_flow_attributes : require

#ifndef M_PI
#define M_PI 3.14159265358979f
#endif
#ifndef M_2PI
#define M_2PI 6.28318530717959f
#endif
#ifndef M_INV_PI
#define M_INV_PI 0.318309886183791f
#endif
#ifndef M_INV_2PI
#define M_INV_2PI 0.159154943091895f
#endif
#ifndef M_INV_4PI
#define M_INV_4PI 0.0795774715459477f
#endif
#ifndef M_PI_DIV_2
#define M_PI_DIV_2 1.5707963267949f
#endif
#ifndef M_PI_DIV_4
#define M_PI_DIV_4 0.785398163397448f
#endif

#ifndef DOT01
#define DOT01(a,b) max(0.0f, dot(a, b))
#endif

#define BITS_SET(a,b) ((a&b)>0)

// half vector for reflection
// wi: normalized incoming dir pointing away
// wo: normalized outgoing dir pointing away
// result: normalized half dir pointing away
vec3 halfvector(const vec3 wi, const vec3 wo) {
#ifdef DEBUG_PRINT_BXDF_CHECK_NAN
	if (dot(wi, wo) == 0.0f) debugPrintfEXT("halfvector dot(wi,wo) == 0");
#endif
	return normalize(wi + wo);
}
// half vector for refraction
// wi: normalized incoming dir pointing away
// wo: normalized outgoing dir pointing away
// result: normalized half dir pointing away
vec3 halfvector(const vec3 wi, const vec3 wo, const float eta_i, const float eta_o) {
	return normalize(-eta_i * wi - eta_o * wo);
}

// Lighting
// Radiometric
// watt to watt/sr
//#define WATT_TO_RADIANT_INTENSITY(watt) (watt/(4*M_PI))
#define WATT_TO_RADIANT_INTENSITY(watt) (watt * M_INV_4PI)

// Inverse Square Law
// https://en.wikipedia.org/wiki/Inverse-square_law#Formula
#define ATTENUATION(distance) (1.0 / (distance * distance))
#define ATTENUATION_RANGE(distance,range) (range == 0.0f ? ((1.0f) / pow(distance,2)) : clamp(1.0f - pow( distance / range, 4.0f ), 0.0f, 1.0f ) / pow(distance, 2.0f))
// https://neil3d.github.io/assets/pdf/s2013_pbs_epic_notes_v2.pdf
#define ATTENUATION_RANGE_UE4(distance,range) (range == 0.0f ? ((1.0f) / (pow(distance, 2.0f)+1)) : pow(clamp(1.0f - pow( distance / range, 4.0f ), 0.0f, 1.0f ),2) / (pow(distance, 2.0f)+1))

// SRGB/Linear
// https://en.wikipedia.org/wiki/SRGB#Specification_of_the_transformation
float srgbToLinear(float srgb)
{
	if (srgb <= 0.04045f) return srgb / 12.92f;
	else return pow((srgb + 0.055f) / 1.055f, 2.4f);
}
vec3 srgbToLinear(vec3 srgb)
{
	return vec3(srgbToLinear(srgb.x), srgbToLinear(srgb.y), srgbToLinear(srgb.z));
}
vec4 srgbToLinear(vec4 srgb)
{
	return vec4(srgbToLinear(srgb.xyz), srgb.w);
}
float linearToSRGB(float linear)
{
	if (linear <= 0.0031308f) return 12.92f * linear;
	else return 1.055f * pow(linear, 1.0f / 2.4f) - 0.055f;
}
vec3 linearToSRGB(vec3 linear)
{
	return vec3(linearToSRGB(linear.x), linearToSRGB(linear.y), linearToSRGB(linear.z));
}
vec4 linearToSRGB(vec4 linear)
{
	return vec4(linearToSRGB(linear.xyz), linear.w);
}

// SRGB/Linear fast
// approximation of the more correct version from above
vec3 srgbToLinearFast(vec3 srgb)
{
	return pow(srgb, vec3(2.2f));
}
vec4 srgbToLinearFast(vec4 srgb)
{
	return vec4(srgbToLinearFast(srgb.xyz), srgb.a);
}
vec3 linearToSRGBFast(vec3 linear)
{
	return pow(linear, vec3(1.0 / 2.2f));
}
vec4 linearToSRGBFast(vec4 linear)
{
	return vec4(linearToSRGBFast(linear.xyz), linear.a);
}

// Luminance
// https://en.wikipedia.org/wiki/Grayscale#Converting_color_to_grayscale
// https://en.wikipedia.org/wiki/Relative_luminance
float
luminance(in vec3 color)
{
	return dot(vec3(0.2126, 0.7152, 0.0722), color);
	//return dot(vec3(0.299, 0.587, 0.114), color);
}

// Clamp
vec3
clampColor(vec3 color, float max_val)
{
	float m = max(color.r, max(color.g, color.b));
	if (m < max_val) return color;
	return color * (max_val / m);
}
float saturate(const float value) {
	return clamp(value, 0.0f, 1.0f);
}

float log10(const float x) {
    const float oneDivLog10 = 0.434294481903252f;
    return log(x) * oneDivLog10;
}

// transforms
vec2 worldSpaceToScreenSpace(const vec4 ws, const mat4 mvp, const vec2 screen_size) {
	vec4 clip_space = mvp * ws;
	vec4 ndc = clip_space / clip_space.w;
	vec2 screen_space = ((ndc.xy + vec2(1.0)) / vec2(2.0)) * screen_size;
	return screen_space;
}

mat4 rotationMatrix(vec3 axis, const float angle)
{
	axis = normalize(axis);
	float s = sin(angle);
	float c = cos(angle);
	float oc = 1.0 - c;

	return mat4(oc * axis.x * axis.x + c, oc * axis.x * axis.y - axis.z * s, oc * axis.z * axis.x + axis.y * s, 0.0,
		oc * axis.x * axis.y + axis.z * s, oc * axis.y * axis.y + c, oc * axis.y * axis.z - axis.x * s, 0.0,
		oc * axis.z * axis.x - axis.y * s, oc * axis.y * axis.z + axis.x * s, oc * axis.z * axis.z + c, 0.0,
		0.0, 0.0, 0.0, 1.0);
}

// [Duff et al. 17] Building An Orthonormal Basis, Revisited. JCGT. 2017.
// https://graphics.pixar.com/library/OrthonormalB/paper.pdf
void createONB(const in vec3 normal, out vec3 tangent, out vec3 bitangent)
{
	const float s = sign(normal.z);
	const float a = 1.0f / (1.0f + abs(normal.z));
	const float b = -s * normal.x * normal.y * a;
	tangent = vec3(1.0f - normal.x * normal.x * a, b, -s * normal.x);
	bitangent = vec3(b, s * 1.0f - normal.y * normal.y * a, -normal.y);

	// split version
	// if(normal.z<0.){
	// 	const float a = 1.0f / (1.0f - normal.z);
	// 	const float b = normal.x * normal.y * a;
	// 	tangent = vec3(1.0f - normal.x * normal.x * a, -b, normal.x);
	// 	bitangent = vec3(b, normal.y * normal.y*a - 1.0f, -normal.y);
	// }
	// else{
	// 	const float a = 1.0f / (1.0f + normal.z);
	// 	const float b = -normal.x * normal.y * a;
	// 	tangent = vec3(1.0f - normal.x * normal.x * a, b, -normal.x);
	// 	bitangent = vec3(b, 1.0f - normal.y * normal.y * a, -normal.y);
	// }
}
mat3 createONBMat(const in vec3 normal)
{
	vec3 tangent, bitangent;
	createONB(normal, tangent, bitangent);
	// T_x, B_x, N_x
	// T_y, B_y, N_y
	// T_z, B_z, N_z
	return mat3(tangent, bitangent, normal);
}
vec3 toLocalSpace(const in mat3 tbn /*mat3(t,b,n)*/, const in vec3 v) {
	return v * tbn;
}
vec3 toGlobalSpace(const in mat3 tbn /*mat3(t,b,n)*/, const in vec3 v) {
	return tbn * v;
}
vec3 toWorld(const vec3 T, const vec3 B, const vec3 N, const vec3 V)
{
	return V.x * T + V.y * B + V.z * N;
}

vec3 toLocal(const vec3 T, const vec3 B, const vec3 N, const vec3 V)
{
	return vec3(dot(V, T), dot(V, B), dot(V, N));
}

// Utility function to get a vector perpendicular to an input vector 
// (from "Efficient Construction of Perpendicular Vectors Without Branching")
// https://blog.selfshadow.com/2011/10/17/perp-vectors/
vec3 getPerpendicularVector(const vec3 u)
{
	const vec3 a = abs(u);
	const uint xm = ((a.x - a.y) < 0 && (a.x - a.z) < 0) ? 1 : 0;
	const uint ym = (a.y - a.z) < 0 ? (1 ^ xm) : 0;
	const uint zm = 1 ^ (xm | ym);
	return cross(u, vec3(xm, ym, zm));
}

// http://www.opengl-tutorial.org/intermediate-tutorials/tutorial-13-normal-mapping/
void getTBVector(const vec3 v0, const vec3 v1, const vec3 v2,
				const vec2 uv0, const vec2 uv1, const vec2 uv2,
				out vec3 tangent, out vec3 bitangent)
{
	const vec3 dv1 = v1-v0;
	const vec3 dv2 = v2-v0;

	const vec2 duv1 = uv1-uv0;
	const vec2 duv2 = uv2-uv0;

	float r = 1.0f / (duv1.x * duv2.y - duv1.y * duv2.x);
	tangent = (dv1 * duv2.y - dv2 * duv1.y) * r;
	bitangent = (dv2 * duv1.x - dv1 * duv2.x) * r;
}
void getTBVector(const vec3 v0, const vec3 v1, const vec3 v2,
				const vec2 uv0, const vec2 uv1, const vec2 uv2,
				out vec3 tangent)
{
	vec3 bitangent;
	getTBVector(v0, v1, v2, uv0, uv1, uv2, tangent, bitangent);
}

// http://jerome.jouvie.free.fr/opengl-tutorials/Lesson8.php
// transform tangent space coordinates v to world space coordinates with custom tangent, bitangent, normal vector 
vec3 tangentSpaceToWorldSpace(const vec3 v, const vec3 t, const vec3 b, const vec3 n) {
	// construct mat3 with the columns t b n
	// T_x, B_x, N_x
	// T_y, B_y, N_y
	// T_z, B_z, N_z
	return mat3(t, b, n) * v;
}
vec3 worldSpaceToTangentSpace(const vec3 v, const vec3 t, const vec3 b, const vec3 n) {
	// construct mat3 with the columns t b n
	// T_x, B_x, N_x
	// T_y, B_y, N_y
	// T_z, B_z, N_z
	return transpose(mat3(t, b, n)) * v;
}
// transform tangent space coordinates v to world space coordinates with tangent and bitangent calculated from a given normal vector
vec3 tangentSpaceToWorldSpace(const vec3 v, const vec3 n) {
	// tangent
	const vec3 t = normalize(getPerpendicularVector(n));
	// bitangent
	const vec3 b = normalize(cross(n, t));
	// construct mat3 with the columns t b n
	// T_x, B_x, N_x
	// T_y, B_y, N_y
	// T_z, B_z, N_z
	//return vec3(1,-1,1)*(mat3(t, b, n) * v).xzy;
	return mat3(t, b, n) * v;
}
vec3 worldSpaceToTangentSpace(const vec3 v, const vec3 n) {
	// tangent
	const vec3 t = normalize(getPerpendicularVector(n));
	// bitangent
	const vec3 b = normalize(cross(n, t));
	// construct mat3 with the columns t b n
	// T_x, B_x, N_x
	// T_y, B_y, N_y
	// T_z, B_z, N_z
	return transpose(mat3(t, b, n)) * v;
}
// for normal maps
vec3 nTangentSpaceToWorldSpace(const vec3 n_ts, const float scale, const vec3 t, const vec3 b, const vec3 n) {
	return normalize(tangentSpaceToWorldSpace(normalize((n_ts * 2.0 - 1.0) * vec3(scale, scale, 1)), t, b, n));
}
vec3 nTangentSpaceToWorldSpace(const vec3 n_ts, const float scale, const vec3 n) {
	// tangent
	const vec3 t = normalize(getPerpendicularVector(n));
	// bitangent
	const vec3 b = normalize(cross(n, t));
	return normalize(tangentSpaceToWorldSpace(normalize((n_ts * 2.0 - 1.0) * vec3(scale, scale, 1)), t, b, n));
}

/*
** Convertions
*/
// unit sphere
vec3 sphericalToCartesian(const float r, const float theta, const float phi) {
	return vec3(r * sin(theta) * cos(phi), r * sin(theta) * sin(phi), r * cos(theta));
	// or Y is up 
	//return vec3(r * sin(theta) * cos(phi), r * cos(theta), r * sin(theta) * sin(phi));
}
// vec3(r, theta, phi)
vec3 sphericalToCartesian(const vec3 v) {
	return sphericalToCartesian(v.x, v.y, v.z);
}
vec3 cartesianToSpherical(const vec3 v) {
	const vec3 v2 = v * v;
	const float r = sqrt(v2.x + v2.y + v2.z);
	const float theta = atan(sqrt(v2.x + v2.y), v.z);
	const float phi = atan(v.y, v.x);
	//const float theta = atan(v.y / v.x);
	//const float phi = acos(v.z / r);
	return vec3(r, theta, phi);
}

// PDF conversion from Solid angle to Area and vice versa
// https://www.pbr-book.org/3ed-2018/Light_Transport_I_Surface_Reflection/Sampling_Light_Sources#fragment-Convertlightsampleweighttosolidanglemeasure-0
// area : area of the light source
// dist : distance between fragment and light source sample
// cosTheta : angle between light normal and direction from light source sample to fragment
float solidAngleToArea(const float area, const float dist, const float cosTheta) {
	return (area * abs(cosTheta)) / (dist * dist);
}
float areaToSolidAngle(const float area, const float dist, float cosTheta) {
	return (dist * dist) / (area * abs(cosTheta));
}

/*
** Post Processing
*/
// from https://developer.blender.org/diffusion/B/browse/master/intern/opencolorio/gpu_shader_display_transform.glsl$142
// uv are uniform random vars 
vec3 dither(const vec3 col, const float strength, const float rnd)
{
	return col.rgb + (rnd * 0.0033f * strength);
}

/*
**	Random
*/
// Simple Random
// Based on http://byteblacksmith.com/improvements-to-the-canonical-one-liner-glsl-rand-for-opengl-es-2-0/
float random(const vec2 co)
{
	const float a = 12.9898;
	const float b = 78.233;
	const float c = 43758.5453;
	const float dt = dot(co.xy, vec2(a, b));
	const float sn = mod(dt, 3.14);
	return fract(sin(sn) * c);
}

// Better Random
// Generates a seed for a random number generator from 2 inputs plus a backoff
// https://github.com/nvpro-samples/optix_prime_baking/blob/332a886f1ac46c0b3eea9e89a59593470c755a0e/random.h
// https://github.com/nvpro-samples/vk_raytracing_tutorial_KHR/tree/master/ray_tracing_jitter_cam
// https://en.wikipedia.org/wiki/Tiny_Encryption_Algorithm
uint initRandomSeed(const uint val0, const uint val1)
{
	uint v0 = val0, v1 = val1, s0 = 0;

	[[unroll]]
	for (uint n = 0; n < 16; n++)
	{
		s0 += 0x9e3779b9;
		v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
		v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
	}

	return v0;
}

uint randomInt(inout uint seed)
{
	// LCG values from Numerical Recipes
	return (seed = 1664525u * seed + 1013904223u);
}

// Generate a random float in [0, 1) given the previous RNG state
float randomFloat(inout uint seed)
{
	// Float version using bitmask from Numerical Recipes
	const uint one = 0x3f800000;
	const uint msk = 0x007fffff;
	return uintBitsToFloat(one | (msk & (randomInt(seed) >> 9))) - 1;

	// Faster version from NVIDIA examples; quality good enough for our use case.
	//return (float(randomInt(seed) & 0x00FFFFFF) / float(0x01000000));
}
vec2 randomFloat2(inout uint seed) { return vec2(randomFloat(seed), randomFloat(seed)); }
vec3 randomFloat3(inout uint seed) { return vec3(randomFloat(seed), randomFloat(seed), randomFloat(seed)); }
vec4 randomFloat4(inout uint seed) { return vec4(randomFloat(seed), randomFloat(seed), randomFloat(seed), randomFloat(seed)); }

// from https://developer.blender.org/diffusion/B/browse/master/intern/opencolorio/gpu_shader_display_transform.glsl$120
// Original code from https://www.shadertoy.com/view/4t2SDh
// Uniform noise in [0..1[ range
float randomFloatNormDist(const vec2 uv)
{
	return fract(sin(dot(uv, vec2(12.9898f, 78.233f))) * 43758.5453f);
}

// Using a triangle distribution which gives a more final uniform noise.
// See Banding in Games:A Noisy Rant(revision 5) Mikkel Gjøl, Playdead (slide 27) https://loopit.dk/banding_in_games.pdf
// triangle noise 
float randomFloatTriDist(const vec2 uv)
{
	// Original code from https://www.shadertoy.com/view/4t2SDh
	// Uniform noise in [0..1[ range
	float nrnd0 = randomFloatNormDist(uv);
	// Convert uniform distribution into triangle-shaped distribution.
	// https://en.wikipedia.org/wiki/Triangular_distribution
	const float orig = nrnd0 * 2.0f - 1.0f;		// range [-1..1[
	nrnd0 = orig * inversesqrt(abs(orig));
	nrnd0 = max(-1.0f, nrnd0); 				// removes NaN's generated by 0*rsqrt(0). Thanks @FioraAeterna!
	// Vulkan is rounding a float before writting it to an image with UNORM so we center the distribution around 0.0, instead of 0.5
	// https://www.khronos.org/registry/vulkan/specs/1.2/html/chap3.html#fundamentals-fpfixedconv
	return nrnd0 - sign(orig); 				// result is range [-1..1[
	//return nrnd0 - sign(orig) + 0.5; 		// result is range [-0.5,1.5[
}

uvec2 linTo2D(uint _index, uint _row_size) {
	uint j = _index / _row_size;
	uint i = _index % _row_size;
	return uvec2(i, j);
}

uint linFrom2D(uint _i, uint _j, uint _row_size) {
	return (_j * _row_size) + _i;
}

// https://www.shadertoy.com/view/llGSzw
vec3 hash3( uint n ) 
{
    // integer hash copied from Hugo Elias
	n = (n << 13U) ^ n;
    n = n * (n * n * 15731U + 789221U) + 1376312589U;
    uvec3 k = n * uvec3(n,n*16807U,n*48271U);
    return vec3( k & uvec3(0x7fffffffU))/float(0x7fffffff);
}

#endif