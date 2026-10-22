#ifndef GLSL_BXDFS_OREN_NAYAR
#define GLSL_BXDFS_OREN_NAYAR

#ifndef M_PI
#define M_PI 3.14159265358979323846264338327950288f
#endif

// diffuse brdf Oren-Nayar
// https://en.wikipedia.org/wiki/Oren%E2%80%93Nayar_reflectance_model#Formulation
vec3 oren_nayar_brdf_eval(const vec3 rho, const vec3 V, const vec3 L, const vec3 N, const float roughness) {
    const float NdotV = dot(N, V);
    const float NdotL = dot(N, L);
    const float sigma2 = roughness * roughness;
    const float A = 1.0 - 0.5 * sigma2 / (sigma2 + 0.33);
    const float B = 0.45 * sigma2 / (sigma2 + 0.09);
    const float cosPhi = dot(normalize(V - NdotV * N), normalize(L - NdotL * N)); // cos(phi_v, phi_l)
    const float sinNV = sqrt(1.0 - NdotV * NdotV);
    const float sinNL = sqrt(1.0 - NdotL * NdotL);
    const float s = NdotV < NdotL ? sinNV : sinNL; // sin(max(theta_v, theta_l))
    const float t = NdotV > NdotL ? sinNV / NdotV : sinNL / NdotL; // tan(min(theta_v, theta_l))
    //return rho * M_INV_PI * (A + B * cosPhi * s * t);
    return (rho * M_INV_PI) * (A + B * max(0.0f, cosPhi) * sqrt(max((1.0-NdotV*NdotV)*(1.0-NdotL*NdotL), 0.)) / max(NdotL, NdotV));
}
// https://mimosa-pudica.net/improved-oren-nayar.html
vec3 oren_nayar_improved_brdf_eval(const vec3 rho, const float NdotL, const float NdotV, const float LdotV, const float roughness){
    const float s = LdotV - NdotL * NdotV;
    const float t = s <= 0.0f ? 1 : max(NdotL, NdotV);

    const float sigma2 = roughness * roughness;
    const vec3 A = 1.0 - ((0.5 * sigma2) / (sigma2 + 0.33)) + ((0.17 * rho * sigma2) / (sigma2 + 0.13) );
    const float B = 0.45 * sigma2 / (sigma2 + 0.09);

    return (rho * M_INV_PI) * (A + B * s / t);
}
// https://developer.blender.org/diffusion/C/browse/master/src/kernel/closure/bsdf_oren_nayar.h
vec3 oren_nayar_blender_brdf_eval(const vec3 rho, const float NdotL, const float NdotV, const float LdotV, const float roughness){
    const float s = LdotV - NdotL * NdotV;
    const float t = s <= 0.0f ? 1.0f : max(NdotL, NdotV);

    const float sigma2 = roughness * roughness;
    const float div = 1.0f / (M_PI + ((3.0f * M_PI - 4.0f) / 6.0f) * roughness);
	
	const float A = 1.0f * div;
	const float B = roughness * div;

    return rho * (A + B * s / t);
}

#endif /* GLSL_BXDFS_OREN_NAYAR */