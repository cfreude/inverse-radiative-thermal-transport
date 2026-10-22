#ifndef GLSL_RAY_TRACING_UTILS
#define GLSL_RAY_TRACING_UTILS

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

vec3 computeBarycentric(in vec2 hitAttribute) {
	return vec3(1.0f - hitAttribute.x - hitAttribute.y, hitAttribute.x, hitAttribute.y);
}

vec3 computeBarycentric2(mat3 v, vec3 ray_origin, vec3 ray_direction)
{
	const vec3 edge1 = v[1] - v[0];
	const vec3 edge2 = v[2] - v[0];

	const vec3 pvec = cross(ray_direction, edge2);

	const float det = dot(edge1, pvec);
	const float inv_det = 1.0f / det;

	const vec3 tvec = ray_origin - v[0];
	const vec3 qvec = cross(tvec, edge1);

	const float alpha = dot(tvec, pvec) * inv_det;
	const float beta = dot(ray_direction, qvec) * inv_det;

	return vec3(1.0f - alpha - beta, alpha, beta);
}

/*
** SAMPLING
*/
int sampleRangeUniform(const int min, const int max, const float rand) {
	return int((max - min) * rand + min);
}
// https://chrischoy.github.io/research/barycentric-coordinate-for-mesh-sampling/
// barycentric coordinates of a uniform distributed sample inside a triangle
vec3 sampleUnitTriangleUniform(vec2 uv) {
	const float sqrt_u = sqrt(uv.x);
	return vec3(1.0f - sqrt_u, sqrt_u * (1.0f - uv.y), sqrt_u * uv.y);
}

// instead of uniform, we could sample the Solid Angle of Area Light Sources https://schuttejoe.github.io/post/arealightsampling/
//Gram-Schmidt method
vec3 orthogonalize(in vec3 a, in vec3 b) {
	//we assume that a is normalized
	return normalize(b - dot(a, b) * a);
}
vec3 slerp(const vec3 start, const vec3 end, const float percent)
{
	// Dot product - the cosine of the angle between 2 vectors.
	float cosTheta = dot(start, end);
	// Clamp it to be in the range of Acos()
	// This may be unnecessary, but floating point
	// precision can be a fickle mistress.
	cosTheta = clamp(cosTheta, -1.0f, 1.0f);
	// Acos(dot) returns the angle between start and end,
	// And multiplying that by percent returns the angle between
	// start and the final result.
	const float theta = acos(cosTheta) * percent;
	const vec3 RelativeVec = normalize(end - start * cosTheta);
	// Orthonormal basis
								// The final result.
	return ((start * cos(theta)) + (RelativeVec * sin(theta)));
}
// https://www.shadertoy.com/view/4tGGzd
//Function which does triangle sampling proportional to their solid angle.
//You can find more information and pseudocode here:
// * Stratified Sampling of Spherical Triangles. J Arvo - 1995
// * Stratified sampling of 2d manifolds. J Arvo - 2001
void sampleSphericalTriangle(in vec3 A, in vec3 B, in vec3 C, in float Xi1, in float Xi2, out vec3 w, out float wPdf) {
	//calculate internal angles of spherical triangle: alpha, beta and gamma
	vec3 BA = orthogonalize(A, B - A);
	vec3 CA = orthogonalize(A, C - A);
	vec3 AB = orthogonalize(B, A - B);
	vec3 CB = orthogonalize(B, C - B);
	vec3 BC = orthogonalize(C, B - C);
	vec3 AC = orthogonalize(C, A - C);
	float alpha = acos(clamp(dot(BA, CA), -1.0, 1.0));
	float beta = acos(clamp(dot(AB, CB), -1.0, 1.0));
	float gamma = acos(clamp(dot(BC, AC), -1.0, 1.0));

	//calculate arc lengths for edges of spherical triangle
	float a = acos(clamp(dot(B, C), -1.0, 1.0));
	float b = acos(clamp(dot(C, A), -1.0, 1.0));
	float c = acos(clamp(dot(A, B), -1.0, 1.0));

	float area = alpha + beta + gamma - M_PI;

	//Use one random variable to select the new area.
	float area_S = Xi1 * area;

	//Save the sine and cosine of the angle delta
	float p = sin(area_S - alpha);
	float q = cos(area_S - alpha);

	// Compute the pair(u; v) that determines sin(beta_s) and cos(beta_s)
	float u = q - cos(alpha);
	float v = p + sin(alpha) * cos(c);

	//Compute the s coordinate as normalized arc length from A to C_s.
	float s = (1.0 / b) * acos(clamp(((v * q - u * p) * cos(alpha) - v) / ((v * p + u * q) * sin(alpha)), -1.0, 1.0));

	//Compute the third vertex of the sub - triangle.
	vec3 C_s = slerp(A, C, s);

	//Compute the t coordinate using C_s and Xi2
	float t = acos(1.0 - Xi2 * (1.0 - dot(C_s, B))) / acos(dot(C_s, B));

	//Construct the corresponding point on the sphere.
	vec3 P = slerp(B, C_s, t);

	w = P;
	wPdf = 1.0 / area;
}
// // Basu and Owen
// vec3 sampleUnitTriangleUniform2(const float u) {
// 	uint64_t uf = uint64_t(u) * (uint64_t(1) << 32);           	// Convert to fixed point
// 	vec2 A = vec2(1, 0);
// 	vec2 B = vec2(0, 1);
// 	vec2 C = vec2(0, 0);        	// Barycentrics
//     for (int i = 0; i < 16; ++i) {          // For each base-4 digit
//         int d = int((uf >> (2 * (15 - i))) & 0x3);
// 		vec2 An, Bn, Cn;
//         switch (d) {
//         case 0:
//             An = (B + C) / 2;
//             Bn = (A + C) / 2;
//             Cn = (A + B) / 2;
//             break;
//         case 1:
//             An = A;
//             Bn = (A + B) / 2;
//             Cn = (A + C) / 2;
//             break;
//         case 2:
//             An = (B + A) / 2;
//             Bn = B;
//             Cn = (B + C) / 2;
//             break;
//         case 3:
//             An = (C + A) / 2;
//             Bn = (C + B) / 2;
//             Cn = C;
//             break;
//         }
//         A = An;
//         B = Bn;
//         C = Cn;
//     }
// 	vec2 r = (A + B + C) / 3;
//     return vec3( r.x, r.y, 1 - r.x - r.y );
// }
vec2 sampleUnitSquareUniform(vec2 uv) {
	return uv;
}
// https://mathworld.wolfram.com/DiskPointPicking.html
// https://www.pbr-book.org/3ed-2018/Monte_Carlo_Integration/2D_Sampling_with_Multidimensional_Transformations#SamplingaUnitDisk
vec2 sampleUnitDiskUniform(const vec2 uv) {
	const float r = sqrt(uv.x);
	const float theta = M_2PI * uv.y; 		// theta in [0, 2PI]
	return r * vec2(cos(theta), sin(theta));
}
// http://l2program.co.uk/900/concentric-disk-sampling
// https://marc-b-reynolds.github.io/math/2017/01/08/SquareDisc.html
vec2 sampleUnitDiskConcentric(const vec2 uv) {
	const vec2 offset = 2.0f * uv - 1.0f;
	if (all(equal(offset, vec2(0.0f)))) return vec2(0.0f);

	float r;
	float theta;
	if (abs(offset.x) > abs(offset.y)) {
		r = offset.x;
		theta = M_PI_DIV_4 * (offset.y / offset.x);
	}
	else {
		r = offset.y;
		theta = M_PI_DIV_2 - M_PI_DIV_4 * (offset.x / offset.y);
	}
	return r * vec2(cos(theta), sin(theta));
}
// https://mathworld.wolfram.com/SpherePointPicking.html
vec3 sampleUnitSphereUniform(const vec2 uv) {
	const float z = 1.0f - 2.0f * uv.x; 			// z = cos(theta) 
	const float r = sqrt(max(0.0f, 1.0f - z * z));
	const float theta = M_2PI * uv.y;  		// theta in [0, 2PI]
	return vec3(r * cos(theta), r * sin(theta), z);
}
vec3 sampleUnitHemisphereUniform(const vec2 uv) {
	const float z = uv.x; 							// z = abs(cos(theta)) 
	const float r = sqrt(max(0.0f, 1.0f - z * z));
	const float theta = M_2PI * uv.y; 		// theta in [0, 2PI]
	return vec3(r * cos(theta), r * sin(theta), z);
}
// http://www.rorydriscoll.com/2009/01/07/better-sampling/
// https://www.pbr-book.org/3ed-2018/Monte_Carlo_Integration/2D_Sampling_with_Multidimensional_Transformations#Cosine-WeightedHemisphereSampling
// Malley’s method
vec3 sampleUnitHemisphereCosine(const vec2 uv) {
	const vec2 uDisk = sampleUnitDiskUniform(uv);
	return vec3(uDisk.x, uDisk.y, sqrt(max(0.0f, 1.0f - uDisk.x * uDisk.x - uDisk.y * uDisk.y)));
}

// https://www.pbr-book.org/3ed-2018/Monte_Carlo_Integration/2D_Sampling_with_Multidimensional_Transformations#SamplingaCone
vec3 sampleUnitConeUniform(const vec2 uv, const float cosThetaMax) {
	const float cosTheta = (1.0f - uv.x) + uv.x * cosThetaMax;
	const float sinTheta = sqrt(max(0.0f, 1.0f - cosTheta * cosTheta));
	const float phi = uv.y * M_2PI;
	return vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}

#define PDF_RANGE_UNIFORM(LENGTH) (1.0f/(LENGTH))
#define PDF_TRIANGLE_UNIFORM(area) (1.0f/(area))
#define PDF_SQUARE_UNIFORM(area) (1.0f/(area))
#define PDF_DISK_UNIFORM(area) (1.0f/(area))
#define PDF_ELLIPSE_UNIFORM(radius) (1.0f/(M_PI * radius.x * radius.y))
#define PDF_SPHERE_UNIFORM(radius) (1.0f/(M_4PI * radius * radius))
#define PDF_UNIT_HEMISPHERE_UNIFORM (M_INV_2PI)
#define PDF_UNIT_HEMISPHERE_COSINE(cosTheta) (cosTheta * M_INV_PI)
#define PDF_UNIT_CONE_UNIFORM(cosThetaMax) (1.0f / (M_2PI * (1.0f - cosThetaMax)))

// Ray Tracing Gems Chapter 6.2.2.4
// Normal points outward for rays exiting the surface, else is flipped.
vec3 offsetRayToAvoidSelfIntersection(const vec3 p, const vec3 n)
{
	const float _origin = 1.0f / 32.0f;
	const float _float_scale = 1.0f / 65536.0f;
	const float _int_scale = 256.0f;

	const ivec3 of_i = ivec3(_int_scale * n.x, _int_scale * n.y, _int_scale * n.z);

	const vec3 p_i = vec3(
		intBitsToFloat(floatBitsToInt(p.x) + ((p.x < 0) ? -of_i.x : of_i.x)),
		intBitsToFloat(floatBitsToInt(p.y) + ((p.y < 0) ? -of_i.y : of_i.y)),
		intBitsToFloat(floatBitsToInt(p.z) + ((p.z < 0) ? -of_i.z : of_i.z)));

	return vec3(
		abs(p.x) < _origin ? p.x + _float_scale * n.x : p_i.x,
		abs(p.y) < _origin ? p.y + _float_scale * n.y : p_i.y,
		abs(p.z) < _origin ? p.z + _float_scale * n.z : p_i.z);
}

// Multiple Importance Sampling
// Eric Veach : https://graphics.stanford.edu/courses/cs348b-01/chapter9.pdf
// Chapter 9.2. Figure 9.3
float misHeuristic(in float pdfA, in float pdfB, const float beta) {
	const float pdfABeta = pow(pdfA, beta);
	const float pdfBBeta = pow(pdfB, beta);
	const float pdfAB = pdfABeta + pdfBBeta;
	return pdfABeta / pdfAB;
}
float misBalanceHeuristic(in float pdfA, in float pdfB) {
	return misHeuristic(pdfA, pdfB, 1);
	//const float pdfAB = pdfA + pdfB;
	//return pdfA / pdfAB;
}
float misPowerHeuristic(in float pdfA, in float pdfB) {
	return misHeuristic(pdfA, pdfB, 2);
	// const float pdfA2 = pdfA * pdfA;
	// const float pdfB2 = pdfB * pdfB;
	// const float a2b2 = pdfA2 + pdfB2;
	// return a2 / a2b2;
}

#endif
