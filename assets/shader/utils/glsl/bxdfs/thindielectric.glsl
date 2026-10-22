#ifndef GLSL_BXDFS_THINDIELECTRIC
#define GLSL_BXDFS_THINDIELECTRIC

#include "fresnel.glsl"
#include "lambert.glsl"
#include "microfacet.glsl"

// vec3 /*brdf*/ thindielectric_eval(const vec3 wi, const vec3 wo, vec3 n, const vec3 albedo, const float roughness, 
// 								float eta_int, float eta_ext) {
// 	float wiDotN = dot(wi, n);
// 	float eta_i = eta_ext;
// 	float eta_t = eta_int;
// 	// check correct normal dir
// 	if(wiDotN < 0) {
// 		//return vec3(0);
// 		n = -n;
// 		wiDotN = dot(wi, n);
// 		eta_i = eta_int;
// 		eta_t = eta_ext;
// 	}

// 	const float woDotN = dot(wo, n);
// 	const bool reflection = (wiDotN * woDotN) >= 0.0f;
// 	const float sampleRoughness = roughness;//(1.2f - 0.2f*sqrt(abs(wiDotN)))*roughness;

// 	vec3 m;
// 	if (reflection) m = halfvector(wi, wo);
// 	else m = halfvector(wi, wo, eta_i, eta_t);
// 	// make sure m points in same hemisphere direction
// 	m *= sign(dot(m, n));

// 	const float mDotN = dot(m, n);
// 	const float wiDotM = dot(wi, m);
// 	const float woDotM = dot(wo, m);

// 	// make sure we are in the upper hemisphere
// 	//if (wiDotN <= 0.0f) return vec3(0); //!!danger!!
// 	// make sure wi/wo are not parallel
// 	if (mDotN <= 0.0f) return vec3(0);
// 	//if (all(equal(wi + wo, vec3(0.0f)))) return vec3(0);

// 	float cosThetaT;
// 	float R = F_Dielectric2(eta_i, eta_t, abs(wiDotM), cosThetaT);
//     const float T = 1.0f - R;

// 	// Account for internal reflections: R' = R + TRT + TR^3T + ..
//     if (R < 1.0f) R += T * T * R / (1.0f - R * R);

// 	const float ggx = ggx_eval(wi, wo, n, m, roughness);
// 	vec3 result = vec3(1.0f);//albedo;
// 	if (reflection) {
// 		result *= R * vec3(abs(woDotN) * ggx * BSDF_GGX(wiDotN, woDotN));
// 	} else {
// 		bool equ = all(lessThan(abs(wi + wo), vec3(0.05f)));

// 		if(!equ) return vec3(0.0f);
// 		result *= (1.0f - R) * vec3(abs(woDotN) * ggx * BSDF_GGX(wiDotM, woDotM, wiDotN, woDotN, eta_i, eta_t));

// 		const float factor = cosThetaT < 0.0f ? eta_i / eta_t : eta_t / eta_i;
// 		result *= factor * factor;
// 	}

// #ifdef DEBUG_PRINT_BXDF_CHECK_NAN
// 	if (any(isnan(result))) debugPrintfEXT("NAN: dielectric_eval result");
// #endif
// 	if (any(isnan(result))) return vec3(0.0f);
// 	return result;
// }

vec3 /*brdf*/ thindielectric_eval(const vec3 wi, const vec3 wo, vec3 n, const vec3 albedo, const float roughness, 
								float eta_int, float eta_ext) {
	return vec3(0.0f);
}

float /*pdf*/ thindielectric_pdf(const vec3 wi, const vec3 wo, vec3 n, const float roughness, 
							 float eta_int, float eta_ext) {
	return 0.0f;
}

bool thindielectric_sample(const vec3 wi, vec3 n, const vec3 albedo, const float roughness, float eta_ext, float eta_int, 
						const vec3 rand, out vec3 wo, out float pdf, out vec3 weight){
	
	float wiDotN = dot(wi, n);
	float eta_i = eta_int;
	float eta_t = eta_ext;
	// check correct normal dir
	if(wiDotN < 0) {
		n = -n;
		wiDotN = dot(wi, n);
	}

	const float sampleRoughness = roughness;//(1.2f - 0.2f*sqrt(abs(wiDotN)))*roughness;
	const float sampleAlpha = sampleRoughness * sampleRoughness;
	const bool rZero = (roughness == 0.0f);
	const vec3 m = rZero ? n : ggx_sample(wi, n, sampleRoughness, rand.xy);
	const float wiDotM = dot(wi, m);

	float cosThetaT;
	float R = F_Dielectric(eta_i, eta_t, abs(wiDotM), cosThetaT);
    const float T = 1.0f - R;

    // Account for internal reflections: R' = R + TRT + TR^3T + ..
    if (R < 1.0f) R += T * T * R / (1.0f - R * R);

    const bool reflection = rand.z < R;

	if (reflection) /* specular */ {
        wo = reflectionDir(wi, m);
	}
	else /* refract */ { // so thin that there is no refraction
		wo = -wi;
	}

	const float woDotN = dot(wo, n);
	const float woDotM = dot(wo, m);
	const float mDotN = dot(m, n);
	// make sure we are in the upper hemisphere
	//if (wiDotN <= 0.0f) return false; //!!danger!!
	// make sure wi/wo are not parallel
	if (mDotN <= 0.0f) return false;
	//if (woDotN <= 0.0f) return false;
	// make sure wi and wo are correct for reflect/refract and wi/wo are not parallel
	//if (trans && reflection != ((wiDotN * woDotN) > 0.0f)) return false;
	//if (all(equal(wi + wo, vec3(0.0f)))) return false;

	pdf = !rZero ? ggx_pdf(n, m, wi, sampleRoughness) : 1.0f;
	//if (pdf < 1e-10f) return false;
	weight = /*albedo */ vec3(!rZero ? ggx_weight(wiDotM, woDotM, wiDotN, woDotN, mDotN, sampleAlpha) : 1.0f);
	if (reflection) /* specular */ {
		if(!rZero) pdf *= D_PDF_M_TO_WO(woDotM);
		//equivalent (non shortened version): 
		//weight = vec3((max(woDotN, 0.0f) * ggx_eval(wi,wo,n,m,roughness) * BSDF_GGX(wiDotN, woDotN)) / (pdf));
		pdf *= R;
	}
	else /* refract */ {
		if(!rZero) pdf *= D_PDF_M_TO_WO(wiDotM, woDotM, eta_i, eta_t);
		// equivalent (non shortened version)
		//weight = vec3((abs(woDotN) * ggx_eval(wi,wo,n,m,roughness) * BSDF_GGX(wiDotM, woDotM, wiDotN, woDotN, eta_i, eta_t)) / (pdf));
		pdf *= 1.0f - R;
	}

#ifdef DEBUG_PRINT_BXDF_CHECK_NAN
	if (isnan(pdf)) debugPrintfEXT("NAN: thindielectric_sample pdf");
	if (any(isnan(weight))) debugPrintfEXT("NAN: thindielectric_sample weight");
#endif
	if (isnan(pdf) || any(isnan(weight))) return false;
	return true;
}

#endif /*GLSL_BXDFS_THINDIELECTRIC*/