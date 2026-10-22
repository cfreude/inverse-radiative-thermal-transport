#ifndef GLSL_BXDFS_METALLIC_ROUGHNESS
#define GLSL_BXDFS_METALLIC_ROUGHNESS

// metallic roughness workflow
// base color: dielectric color or metallic F0
// metallic: used to interpolate diffuse and specular brdf
// roughness: used as roughness*roughness = alpha

#include "plastic.glsl"
#include "conductor.glsl"
#include "dielectric.glsl"
#include "thindielectric.glsl"
#include "mix.glsl"

vec3 /*brdf*/ metalroughness_eval(const vec3 wi, const vec3 wo, const vec3 n, const vec3 albedo, 
							const float metallic, const float roughness, const float transmission, 
							const float eta_int, const float eta_ext, const bool thin){
	const vec3 pl_brdf = plastic_eval(wi, wo, n, albedo, roughness, eta_int, eta_ext);
	const vec3 di_brdf = thin ? thindielectric_eval(wi, wo, n, albedo, roughness, eta_int, eta_ext) : dielectric_eval(wi, wo, n, albedo, roughness, eta_int, eta_ext);							
	const vec3 co_brdf = conductor_eval(wi, wo, n, roughness, albedo);
	return mix(mix(pl_brdf, di_brdf, transmission), co_brdf, metallic);
}

float /*pdf*/ metalroughness_pdf(const vec3 wi, const vec3 wo, const vec3 n, const float metallic, 
						const float roughness, const float transmission, const float eta_int, const float eta_ext, const bool thin){
	const float pl_pdf = plastic_pdf(wi, wo, n, roughness, eta_int, eta_ext);
	const float di_pdf = thin ? thindielectric_pdf(wi, wo, n, roughness, eta_int, eta_ext) : dielectric_pdf(wi, wo, n, roughness, eta_int, eta_ext);
	const float co_pdf = conductor_pdf(wi, wo, n, roughness);
	return mix(mix(pl_pdf, di_pdf, transmission), co_pdf, metallic);
}

bool metalroughness_sample(const vec3 wi, const vec3 n, const vec3 albedo, const float metallic, 
						const float roughness, const float transmission, const float eta_int, const float eta_ext, 
						const bool thin, const vec4 rand0, const vec3 rand1, 
						out vec3 wo, out float pdf, out vec3 weight){
	if(rand0.x < metallic) {
		return conductor_sample(wi, n, roughness, albedo, rand1.xy, wo, pdf, weight);
	} else {
		if (rand0.y < transmission) {
			if(thin) return thindielectric_sample(wi, n, albedo, roughness, eta_int, eta_ext, rand1, wo, pdf, weight);
			else return dielectric_sample(wi, n, albedo, roughness, eta_int, eta_ext, rand1, wo, pdf, weight);
		} else {
			return plastic_sample(wi, n, albedo, roughness, eta_int, eta_ext, rand1, rand0.zw, wo, pdf, weight);
		}
	}
}

#endif /* GLSL_BXDFS_METALLIC_ROUGHNESS */