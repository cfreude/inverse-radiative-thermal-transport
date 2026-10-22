#ifndef GLSL_BXDFS_MICROFACET
#define GLSL_BXDFS_MICROFACET

#include "../rendering_utils.glsl"

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

// https://simonstechblog.blogspot.com/2011/12/microfacet-brdf.html
// but there are different versions 
// -> https://computergraphics.stackexchange.com/questions/1515/what-is-the-accepted-method-of-converting-shininess-to-roughness-and-vice-versa
// e.g for blinn phong
float shininessToRoughness(const float shininess) {
    return sqrt(2.0f / (shininess + 2.0f));
}
float roughnessToShininess(const float roughness) {
    return (2.0f / (roughness * roughness)) - 2.0f;
}

//Float eta = CosTheta(wo) > 0 ? (etaB / etaA) : (etaA / etaB);
//Vector3f wh = Normalize(wo + wi * eta);

// https://www.pbr-book.org/3ed-2018/Light_Transport_I_Surface_Reflection/Sampling_Reflection_Functions#MicrofacetBxDFs
// has to be applied to the ndf pdf in order to sample incident direction (wi) instead of micro normals (m)
float D_PDF_M_TO_WO(const float woDotM /*VdotM*/) {
    if (woDotM == 0.0f) return 0.0f;
    return 0.25f / abs(woDotM);
}
// in this case wiDotM != woDotM 
float D_PDF_M_TO_WO(const float wiDotM, const float woDotM, const float eta_i, const float eta_o) {
    const float a = eta_i * wiDotM + eta_o * woDotM;
    if (a == 0.0f) return 0.0f;
    return (eta_o * eta_o * abs(woDotM)) / (a * a);
}

// BRDF Explorer
// https://github.com/wdas/brdf/tree/main/src/brdfs

/*
** Normal Distribution Function (NDF)
*/
/* Isotropic */
// D GGX
// Walter 2007, "Microfacet Models for Refraction through Rough Surfaces" 
float D_GGX(const float MdotN, const float alpha) {
    if (MdotN <= 0) return 0;
    const float alpha2 = alpha * alpha;
    const float cos_theta = MdotN;
    const float cos_theta2 = cos_theta * cos_theta;
    const float tan_theta2 = (1.0f - cos_theta2) / cos_theta2;

    const float A = M_PI * cos_theta2 * cos_theta2 * (alpha2 + tan_theta2) * (alpha2 + tan_theta2);
    return alpha2 / A;
}
// https://agraphicsguy.wordpress.com/2015/11/01/sampling-microfacet-brdf/
float D_GGX_PDF(const float MdotN, const float alpha) {
    return D_GGX(MdotN, alpha) * MdotN;
}

// Generalized Trowbridge-Reitz
// https://www.scitepress.org/Papers/2021/102527/102527.pdf
// Microfacet Distribution Function: To Change or Not to Change, That Is the Question
// https://github.com/wdas/brdf/tree/main/src/brdfs
float D_GTR(const float MdotN, const float alpha, const float gamma) {
    if (MdotN <= 0) return 0;
    const float alpha2 = alpha * alpha;
    const float t = 1.0f + (alpha2 - 1.0f) * MdotN * MdotN;
    return alpha2 / (M_PI * pow(t, gamma));
}
float D_GTR2(const float MdotN, const float alpha) {
    return D_GTR(MdotN, alpha, 2.0f);
}
#define D_GGX D_GTR2
float D_GTR2_PDF(const float MdotN, const float alpha) {
    return D_GTR2(MdotN, alpha) * MdotN;
}

vec2 D_GGX_Importance_Sample(const vec2 xi, const float alpha) {

    // theta can be rewritten without atan -> cos theta
    return vec2(atan(alpha * sqrt(xi.x / (1.0f - xi.x))),
        // phi
        M_2PI * xi.y);
    // theta
    //return vec2(acos(sqrt((1.0f - xi.x) / (((alpha*alpha) - 1.0f) * xi.x + 1.0f))),
    // phi
    //M_2PI * xi.y);
}
float D_GGX_Anisotropic(const vec3 N, const vec3 H, const float roughness_x, const float roughness_y)
{
    return 0;
}

/*
**	Geometry Masking and Shadowing Function
*/
float G1_Smith(const float cosTheta, const float alpha){
    const float alpha2 = alpha * alpha;
    const float term = cosTheta * cosTheta;
    return (cosTheta) / (cosTheta + sqrt(alpha2 + term - alpha2 * term));
}
float G_Smith(const float NdotV, const float NdotL, const float alpha){
    const float g1v = G1_Smith(abs(NdotV), alpha);
    const float g1l = G1_Smith(abs(NdotL), alpha);
    return g1v * g1l;
}
// Walter et al. 2007. Microfacet models for refraction through rough surfaces.
// Smith approximation: Smith B. G.: Geometrical shadowing of a random rough surface.
float G1_GGX(const float Mdot_, const float Ndot_, const float alpha) {
    if ((Mdot_ / Ndot_) <= 0) return 0;
    const float alpha2 = alpha * alpha;
    const float cos_theta2 = Ndot_ * Ndot_;
    const float tan_theta2 = (1.0f - cos_theta2) / cos_theta2;
    return 2.0f / (1.0f + sqrt(1.0f + alpha2 * tan_theta2));
}
float G_Smith_GGX(const float MdotV, const float MdotL, const float NdotV, const float NdotL, const float alpha) {
    const float g1v = G1_GGX(MdotV, NdotV, alpha);
    const float g1l = G1_GGX(MdotL, NdotL, alpha);
    return g1v * g1l;
}
// https://google.github.io/filament/Filament.html#materialsystem/specularbrdf/geometricshadowing(specularg)
// when using this version, do not multiply BSDF_GGX (1/4*LN*VN) to F*D*G 
//float G_Smith_Correlated_Reduced_GGX(const float MdotV, const float MdotL, const float NdotV, const float NdotL, const float alpha) {
//    if ((MdotV / NdotV) <= 0) return 0;
//    if ((MdotL / NdotL) <= 0) return 0;
//    float a2 = alpha * alpha;
//    float GGXV = NdotL * sqrt(NdotV * NdotV * (1.0f - a2) + a2);
//    float GGXL = NdotV * sqrt(NdotL * NdotL * (1.0f - a2) + a2);
//    return 0.5f / (GGXV + GGXL);
//}

/*
**	BRDF
**  https://graphicrants.blogspot.com/2013/08/specular-brdf-reference.html
*/
// Cook-Torrance BSDF
// https://en.wikipedia.org/wiki/Specular_highlight#Cook%E2%80%93Torrance_model
// Cook Torrance 1982 "A Reflectance Model for Computer Graphics" Page 5
// D Beckmann
// F Fresnel (Schlick or complete Fresnel)
// G Cook Torrance
float BSDF_Cook_Torrance(const float NdotL, const float NdotV) {
    //if(NdotL <= 0 || NdotV <= 0) return 0;
    return 1.0f / (M_PI * abs(NdotL) * abs(NdotV));
}
// Torrance–Sparrow BSDF (BRDF + BTDF)
float BSDF_Torrance_Sparrow(const float wiDotHr, const float woDotHr) {
    if (wiDotHr * woDotHr == 0) return 0;
    return 1.0f / (4.0f * abs(wiDotHr) * abs(woDotHr));
}
float BSDF_Torrance_Sparrow(const float wiDotHt, const float woDotHt, const float wiDotN, const float woDotN,
    const float eta_i, const float eta_o) {
    const float a = (abs(wiDotHt) * abs(woDotHt)) / (abs(wiDotN) * abs(woDotN));
    const float b = (eta_i * wiDotHt + eta_o * woDotHt);
    if (b == 0.0f) return 0.0f;
    return a * ((eta_o * eta_o) / (b * b));
}
// GGX BSDF
// D GGX
// F Fresnel (Schlick or complete Fresnel)
// G Smith Schlick GGX
#define BSDF_GGX BSDF_Torrance_Sparrow

/*
** Complete GGX
*/

float /*brdf*/ ggx_eval(const vec3 wi, const vec3 wo, const vec3 n, const vec3 h, const float roughness) {
    const float alpha = roughness * roughness;
    const float nDotH = dot(h, n);
    const float nDotWi = dot(n, wi);
    const float nDotWo = dot(n, wo);
    const float wiDotH = dot(h, wi);
    const float woDotH = dot(h, wo);

    const float d_ggx = D_GTR2(nDotH, alpha);
    const float g_ggx = G_Smith_GGX(wiDotH, woDotH, nDotWi, nDotWo, alpha);

#ifdef DEBUG_PRINT_BXDF_CHECK_NAN
    if (isnan(wiDotH)) debugPrintfEXT("NAN: ggx_eval wiDotH %f", wiDotH);
    if (isnan(woDotH)) debugPrintfEXT("NAN: ggx_eval woDotH %f", woDotH);
    if (isnan(nDotWo)) debugPrintfEXT("NAN: ggx_eval nDotWo %f", nDotWo);
    if (isnan(nDotWi)) debugPrintfEXT("NAN: ggx_eval nDotWi %f", nDotWi);
    if (isnan(d_ggx)) debugPrintfEXT("NAN: ggx_eval d_ggx");
    if (isnan(g_ggx)) debugPrintfEXT("NAN: ggx_eval g_ggx");
#endif
    return d_ggx * g_ggx;
}
vec3 /*m*/ ggx_sample(const vec3 wi, const vec3 n, const float roughness, const vec2 rand) {
    const float roughness2 = roughness * roughness;
    const vec2 spherical = D_GGX_Importance_Sample(rand, roughness2);
    vec3 m = normalize(sphericalToCartesian(vec3(1, spherical.xy)));
    return normalize(tangentSpaceToWorldSpace(m, n));
}
float /*pdf*/ ggx_pdf(vec3 n, vec3 m, const vec3 wi, float roughness) {
    if (dot(n, m) <= 0) return 0;
    const float roughness2 = roughness * roughness;
    const float d_ggx = D_GTR2(dot(n, m), roughness2);
    const float pdf = d_ggx * dot(m, n);
    return max(pdf, 0.0f);
    //const float g_ggx = G1_GGX(dot(wi, m), dot(wi, n), roughness2);
    //return d_ggx * g_ggx;
}
// Walter 2007, "Microfacet Models for Refraction through Rough Surfaces" eq: 41
float ggx_weight(const float wiDotM, const float woDotM, const float wiDotN, const float woDotN, const float mDotN, const float alpha) {
    const float G = G_Smith_GGX(wiDotM, woDotM, wiDotN, woDotN, alpha);
    const float denom = abs(wiDotN) * abs(mDotN);
    if(denom == 0.0f) return 0.0f;
    return (abs(wiDotM) * G) / denom;
}


// Input Ve: view direction
// Input alpha_x, alpha_y: roughness parameters
// Input U1, U2: uniform random numbers
// Output Ne: normal sampled with PDF D_Ve(Ne) = G1(Ve) * max(0, dot(Ve, Ne)) * D(Ne) / Ve.z
//vec3 sampleGGXVNDF(vec3 Ve, float alpha_x, float alpha_y, float U1, float U2)
//{
//    // Section 3.2: transforming the view direction to the hemisphere configuration
//    vec3 Vh = normalize(vec3(alpha_x * Ve.x, alpha_y * Ve.y, Ve.z));
//    // Section 4.1: orthonormal basis (with special case if cross product is zero)
//    float lensq = Vh.x * Vh.x + Vh.y * Vh.y;
//    vec3 T1 = lensq > 0 ? vec3(-Vh.y, Vh.x, 0) * inversesqrt(lensq) : vec3(1, 0, 0);
//    vec3 T2 = cross(Vh, T1);
//    // Section 4.2: parameterization of the projected area
//    float r = sqrt(U1);
//    float phi = 2.0 * M_PI * U2;
//    float t1 = r * cos(phi);
//    float t2 = r * sin(phi);
//    float s = 0.5 * (1.0 + Vh.z);
//    t2 = (1.0 - s) * sqrt(1.0 - t1 * t1) + s * t2;
//    // Section 4.3: reprojection onto hemisphere
//    vec3 Nh = t1 * T1 + t2 * T2 + sqrt(max(0.0, 1.0 - t1 * t1 - t2 * t2)) * Vh;
//    // Section 3.4: transforming the normal back to the ellipsoid configuration
//    vec3 Ne = normalize(vec3(alpha_x * Nh.x, alpha_y * Nh.y, max(0.0, Nh.z)));
//    return Ne;
//}

// float pdfGGXVNDF(vec3 wi, vec3 m, vec3 n, float roughness)
// {
//     const float roughness2 = roughness * roughness;
//     const float MdotN = dot(n, m);
//     const float MdotWI = dot(wi, m);
//     const float NdotWI = dot(wi, n);
//     const float d_ggx = D_GGX_Filament(n, m, MdotN, roughness2);
//     const float g1_ggx = G1_Smith_Beckmann(NdotWI, roughness2);

//     return (g1_ggx * abs(MdotWI) * d_ggx) / abs(NdotWI);
// }
// vndf wip
// vec3 /*wo*/ ggx_brdf_sample2(const vec3 wi, const vec3 n, const float roughness, const vec2 rand){
//     mat3 tbn = createONBMat(n);
//     vec3 t_wi = toLocalSpace(tbn, wi);
//     //vec3 t_wi = normalize(worldSpaceToTangentSpace(wi, n));
//     //if(t_wi.z < 0) debugPrintfEXT("t_wi %f %f", t_wi.z, dot(wi, n));

// 	const float roughness2 = roughness * roughness;
//     return normalize(toGlobalSpace(tbn, sampleGGXVNDF(t_wi,roughness2,roughness2,rand.x,rand.y)));
//     //vec3 m = normalize(tangentSpaceToWorldSpace(sampleGGXVNDF(t_wi,roughness2,roughness2,rand.x,rand.y), n));
// }
// float /*pdf*/ ggx_brdf_pdf2(vec3 wi, vec3 wo, vec3 n, vec3 m, float roughness){
// 	if(dot(n, wo) <= 0) return 0;
//     return pdfGGXVNDF(wi, m, n, roughness);
// }

#endif /* GLSL_BXDFS_MICROFACET */