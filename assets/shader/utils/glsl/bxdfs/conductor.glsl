#ifndef GLSL_BXDFS_CONDUCTOR
#define GLSL_BXDFS_CONDUCTOR

#include "fresnel.glsl"
#include "microfacet.glsl"

vec3 conductor_eval(const vec3 wi, const vec3 wo, const vec3 n, const float roughness, const vec3 f0) {
	if(roughness == 0.0f) return vec3(0.0f);
	const vec3 m = halfvector(wi, wo);
	const float wiDotN = dot(wi, n);
	const float woDotN = dot(wo, n);
	const float wDotM = dot(wi, m); /* in case of reflection -> wiDotM == woDotM */
	// make sure we are in the upper hemisphere and wi/wo are not parallel
	if (wiDotN <= 0.0f || woDotN <= 0.0f || wDotM <= 0.0f) return vec3(0);
	if (all(equal(wi + wo, vec3(0.0f)))) return vec3(0.0f);

	const vec3 eta = vec3(1.657460, 0.880369, 0.521229);
	const vec3 k = vec3(9.223869, 6.269523, 4.837001);
	//const vec3 fi = F_Conductor(eta*1.2, k / 1.5, wDotM);
	const vec3 fi = F_Schlick(wDotM, f0);

	float ggx = ggx_eval(wi, wo, n, m, roughness) * BSDF_GGX(wiDotN, woDotN);
#ifndef BXDFS_EVAL_NO_COS
	ggx *= max(woDotN, 0.0f);
#endif
	const vec3 result = ggx * fi;
#ifdef DEBUG_PRINT_BXDF_CHECK_NAN
	if (any(isnan(fi))) debugPrintfEXT("NAN: conductor_eval fi");
	if (any(isnan(result))) debugPrintfEXT("NAN: conductor_eval result");
#endif
	if (any(isnan(result))) return vec3(0.0f);
	return result;
}

float conductor_pdf(const vec3 wi, const vec3 wo, const vec3 n, const float roughness) {
	if(roughness == 0.0f) return 0.0f;

	const vec3 m = halfvector(wi, wo);
	const float wiDotN = dot(wi, n);
	const float woDotN = dot(wo, n);
	const float wDotM = dot(wi, m); /* in case of reflection -> wiDotM == woDotM */
	
	if (wDotM <= 0.0f) return 0.0f;
	// make sure we are in the upper hemisphere
	if (wiDotN <= 0.0f) return 0.0f;
	if (woDotN <= 0.0f) return 0.0f;
	// make sure wi/wo are not parallel
	if (all(equal(wi + wo, vec3(0.0f)))) return 0.0f;

	const float pdf = ggx_pdf(n, m, wi, roughness) * D_PDF_M_TO_WO(wDotM);
#ifdef DEBUG_PRINT_BXDF_CHECK_NAN
	if (isnan(pdf)) debugPrintfEXT("NAN: conductor_pdf pdf");
#endif
	if (pdf < 1e-10f || isnan(pdf)) return 0.0f;
	return pdf;
}

bool conductor_sample(const vec3 wi, const vec3 n, const float roughness, const vec3 f0, const vec2 rand, out vec3 wo, out float pdf, out vec3 weight) {
	const float wiDotN = dot(wi, n);
	const float sampleRoughness = roughness;//(1.2f - 0.2f*sqrt(abs(wiDotN)))*roughness;
	const float sampleAlpha = sampleRoughness * sampleRoughness;
	const bool rZero = (roughness == 0.0f);
	const vec3 m = rZero ? n : ggx_sample(wi, n, sampleRoughness, rand.xy);
	wo = normalize(reflect(-wi, m));

	const float woDotN = dot(wo, n);
	const float wDotM = dot(wi, m); /* in case of reflection -> wiDotM == woDotM */

	if (wDotM <= 0.0f) return false;
	// make sure we are in the upper hemisphere
	if (wiDotN <= 0.0f) return false;
	if (woDotN <= 0.0f) return false;
	// make sure wi/wo are not parallel
	if (all(equal(wi + wo, vec3(0.0f)))) return false;

	const vec3 eta = vec3(1.657460, 0.880369, 0.521229);
	const vec3 k = vec3(9.223869, 6.269523, 4.837001);
	//const vec3 fi = F_Conductor(eta*1.2, k / 1.5, wDotM);
	const vec3 fi = F_Schlick(wDotM, f0);

	pdf = 1.0f;
	weight = fi;
	if(!rZero) {
		pdf = ggx_pdf(n, m, wi, sampleRoughness) * D_PDF_M_TO_WO(wDotM);
		if (pdf < 1e-10f) return false;
		weight *= ggx_weight(wDotM, wDotM, wiDotN, woDotN, dot(m, n), sampleAlpha);
		// equivalent (non shortened version)
		//weight = fi * vec3((abs(woDotN) * ggx_eval(wi,wo,n,m,roughness) * BSDF_GGX(wiDotN, woDotN)) / (pdf));
	}
#ifdef DEBUG_PRINT_BXDF_CHECK_NAN
	if (isnan(pdf)) debugPrintfEXT("NAN: conductor_sample pdf");
	if (any(isnan(weight))) debugPrintfEXT("NAN: conductor_sample weight");
#endif
	if (isnan(pdf) || any(isnan(weight))) return false;
	return true;
}

#endif /* GLSL_BXDFS_CONDUCTOR */
