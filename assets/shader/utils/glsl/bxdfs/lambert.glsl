#ifndef GLSL_BXDFS_LAMBERT
#define GLSL_BXDFS_LAMBERT

#include "../ray_tracing_utils.glsl"

vec3 /*brdf*/ lambert_brdf_eval(const vec3 wo, const vec3 n, const vec3 albedo){
	vec3 result = albedo * M_INV_PI;
#ifndef BXDFS_EVAL_NO_COS
	result *= max(dot(n,wo), 0.0f);
#endif
    return result;
}

// disney 
// https://blog.selfshadow.com/publications/s2012-shading-course/#course_content
// 5.3: https://blog.selfshadow.com/publications/s2012-shading-course/burley/s2012_pbs_disney_brdf_notes_v3.pdf
vec3 lambert_disney_brdf_eval(const float wiDotN, const float woDotN, const float VdotH, const vec3 albedo, const float roughness){
    const float fd90 = 0.5 + 2 * roughness * VdotH * VdotH;
    const float fnl = 1 + (fd90 - 1) * pow(1 - wiDotN, 5);
    const float fnv = 1 + (fd90 - 1) * pow(1 - woDotN, 5);
    return albedo * M_INV_PI * fnl * fnv;
}

float /*pdf*/ lambert_brdf_pdf(const vec3 wo, const vec3 n){
	return max(0.0f, dot(wo, n)) * M_INV_PI;
}

bool lambert_brdf_sample(const vec3 n, const vec3 albedo, const vec2 rand, out vec3 wo, out float pdf, out vec3 weight){
	wo = normalize(tangentSpaceToWorldSpace(sampleUnitHemisphereCosine(rand), n));
    pdf = lambert_brdf_pdf(wo, n);
    //weight = (albedo * M_INV_PI * max(dot(n,wo),0)) / pdf;
    weight = albedo;
	if (dot(wo,n) <= 0.0f) return false;
    return true;
}

#endif /* GLSL_BXDFS_LAMBERT */