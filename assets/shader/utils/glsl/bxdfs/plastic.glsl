#ifndef GLSL_BXDFS_PLASTIC
#define GLSL_BXDFS_PLASTIC

#include "fresnel.glsl"
#include "lambert.glsl"
#include "microfacet.glsl"

/* material that consists of layering of a reflective part and a diffuse part */

// Jan van Bergen - Physically Based Specular + Diffuse
#define PLASTIC_NONLINEAR false 

vec3 plastic_eval(const vec3 wi, const vec3 wo, const vec3 n, const vec3 albedo, 
	const float roughness, float eta_int, float eta_ext) {
	const float wiDotN = dot(wi, n);
	const float woDotN = dot(wo, n);
	const bool reflection = (wiDotN * woDotN) >= 0.0f;
	const float sampleRoughness = roughness;//(1.2f - 0.2f*sqrt(abs(wiDotN)))*roughness;

	vec3 m;
	if (reflection) m = halfvector(wi, wo);
	else m = halfvector(wi, wo, eta_ext, eta_int);
	// make sure m points in same hemisphere direction
	m *= sign(dot(m, n));

	const float mDotN = dot(m, n);
	const float wiDotM = dot(wi, m);
	const float woDotM = dot(wo, m);

	if (mDotN <= 0.0f) return vec3(0);
	// make sure we are in the upper hemisphere
	if (wiDotN <= 0.0f) return vec3(0);
	if (woDotN <= 0.0f) return vec3(0);
	//if (woDotM <= 0.0f) return vec3(0);
	//if (wiDotM <= 0.0f) return vec3(0);
	// make sure wi/wo are not parallel
	if (all(equal(wi + wo, vec3(0.0f)))) return vec3(0);

	float cosThetaT;
	const float fo = F_Dielectric(eta_ext, eta_int, abs(woDotN), cosThetaT);
	const float fi = F_Dielectric(eta_ext, eta_int, abs(wiDotN), cosThetaT);
	const bool rZero = (roughness == 0.0f);
	const float specular = !rZero ? ggx_eval(wi, wo, n, m, roughness) * BSDF_GGX(wiDotN, woDotN) : 0.0f;
	// internal scattering
	const float eta = eta_ext / eta_int;
	const float eta2 = eta * eta;
	const float re = fresnelDiffuseReflectance(eta_ext, eta_int);
	const float ri = 1.0f - eta2 * (1.0f - re);
	// const vec3 diffuse = abs(woDotN) * albedo * M_INV_PI;
	const vec3 diffuse = eta2 * albedo * (1.0f - fo) / (M_PI * (1.0f - (PLASTIC_NONLINEAR ? albedo : vec3(1.0f)) * ri));
	vec3 result = (1.0f - fi) * diffuse + (fi) * specular;
#ifndef BXDFS_EVAL_NO_COS
	result *= max(woDotN, 0.0f);
#endif

#ifdef DEBUG_PRINT_BXDF_CHECK_NAN
	if (any(isnan(result))) debugPrintfEXT("NAN: plastic_eval result");
#endif
	if (any(isnan(result))) return vec3(0.0f);
	return result;
}

float plastic_pdf(const vec3 wi, const vec3 wo, const vec3 n, 
	const float roughness, float eta_int, float eta_ext) {
	const float wiDotN = dot(wi, n);
	const float woDotN = dot(wo, n);
	const bool reflection = (wiDotN * woDotN) >= 0.0f;
	const bool rZero = (roughness == 0.0f);
	const float sampleRoughness = roughness;//(1.2f - 0.2f*sqrt(abs(wiDotN)))*roughness;

	vec3 m;
	if (reflection) m = halfvector(wi, wo);
	else m = halfvector(wi, wo, eta_ext, eta_int);
	// make sure m points in same hemisphere direction
	m *= sign(dot(m, n));

	const float mDotN = dot(m, n);
	const float wiDotM = dot(wi, m);
	const float woDotM = dot(wo, m);
	if (mDotN <= 0.0f) return 0;
	// make sure we are in the upper hemisphere
	if (wiDotN <= 0.0f) return 0;
	//if (woDotN <= 0.0f) return 0;
	// make sure wi/wo are not parallel
	if (all(equal(wi + wo, vec3(0.0f)))) return 0;

	float cosThetaT;
	const float fi = F_Dielectric(eta_ext, eta_int, abs(wiDotN), cosThetaT);
	const float pdf_ggx = ggx_pdf(n, m, wi, roughness);

	const float pdf_specular = !rZero ? pdf_ggx * D_PDF_M_TO_WO(woDotM) : 0.0f;
	const float pdf_diffuse = abs(woDotN) * M_INV_PI;
	const float pdf = mix(pdf_diffuse, pdf_specular, fi);

#ifdef DEBUG_PRINT_BXDF_CHECK_NAN
	if (isnan(pdf)) debugPrintfEXT("NAN: plastic_pdf pdf");
#endif
	if (isnan(pdf)) return 0.0f;
	return pdf;
}

bool plastic_sample(const vec3 wi, const vec3 n, const vec3 albedo, const float roughness,
	float eta_int, float eta_ext, const vec3 rand0, const vec2 rand1, 
	out vec3 wo, out float pdf, out vec3 weight) {

	const float wiDotN = dot(wi, n);
	const float sampleRoughness = roughness;//(1.2f - 0.2f*sqrt(abs(wiDotN)))*roughness;
	const float sampleAlpha = sampleRoughness * sampleRoughness;
	const bool rZero = (roughness == 0.0f);
	const vec3 m = rZero ? n : ggx_sample(wi, n, sampleRoughness, rand0.xy);
	const float wiDotM = dot(wi, m);

	float cosThetaT;
	const float fi = F_Dielectric(eta_ext, eta_int, abs(wiDotN), cosThetaT);
	const bool reflection = rand0.z < fi;

	if (reflection) wo = reflectionDir(wi, m);
	else wo = normalize(tangentSpaceToWorldSpace(sampleUnitHemisphereCosine(rand1.xy), n));

	const float woDotN = dot(wo, n);
	const float woDotM = dot(wo, m);
	const float mDotN = dot(m, n);
	if (mDotN <= 0.0f) return false;
	// make sure wi/wo are not parallel
	if (all(equal(wi + wo, vec3(0.0f)))) return false;
	// make sure we are in the upper hemisphere
	if (woDotN <= 0.0f) return false;
	if (wiDotN <= 0.0f) return false;
	// make sure wi and wo are correct for reflect/refract and wi/wo are not parallel
	//if (trans && reflection != ((wiDotN * woDotN) > 0.0f)) return false;

	pdf = 1.0f;
	weight = vec3(1.0f);
	if (reflection) /* specular */ {
		if(!rZero) {
			pdf = ggx_pdf(n, m, wi, sampleRoughness) * D_PDF_M_TO_WO(woDotM);
			weight = vec3(ggx_weight(wiDotM, woDotM, wiDotN, woDotN, mDotN, sampleAlpha));
		}
		//equivalent (non shortened version): 
		//weight = vec3((max(woDotN, 0.0f) * ggx_eval(wi,wo,n,m,roughness) * BSDF_GGX(wiDotN, woDotN)) / (pdf));
		pdf *= fi;
	} else /* diffuse */ {
		pdf = abs(woDotN) * M_INV_PI;
		weight = albedo;
		// equivalent (non shortened version)
		//weight = (max(woDotN, 0.0f) * albedo * M_INV_PI) / pdf;
		pdf *= (1.0f - fi);


		// internal scattering
		const float fo = F_Dielectric(eta_ext, eta_int, abs(woDotN), cosThetaT);
		const float eta = eta_ext / eta_int;
		const float eta2 = eta * eta;
		const float re = fresnelDiffuseReflectance(eta_ext, eta_int);
		const float ri = 1.0f - eta2 * (1.0f - re);
		weight *= eta2 * (1.0f - fo) / (1.0f - (PLASTIC_NONLINEAR ? albedo : vec3(1.0f)) * ri);
	}

#ifdef DEBUG_PRINT_BXDF_CHECK_NAN
	if (isnan(pdf)) debugPrintfEXT("NAN: plastic_sample pdf");
	if (any(isnan(weight))) debugPrintfEXT("NAN: plastic_sample weight");
#endif
	if (isnan(pdf) || any(isnan(weight))) return false;
	return true;
}

#endif /* GLSL_BXDFS_PLASTIC */
