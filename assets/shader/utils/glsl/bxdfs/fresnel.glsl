#ifndef GLSL_BXDFS_FRESNEL
#define GLSL_BXDFS_FRESNEL

// https://en.wikipedia.org/wiki/Refractive_index#Typical_values
#define RI_VACUUM 		1.0f
#define RI_AIR 			1.000293f
#define RI_WATER 		1.333f
#define RI_ICE 			1.31f
#define RI_GLASS 		1.52f
#define RI_FLINT_GLASS 	1.69f
#define RI_SAPPHIRE 	1.77f
#define RI_DIAMOND 	    2.42f
float F0(const float refractionIndex1, const float refractionIndex2){
    float F0 = (refractionIndex1 - refractionIndex2) / (refractionIndex1 + refractionIndex2);
    return F0 * F0;
}
vec3 F0(const vec3 refractionIndex1, const vec3 refractionIndex2){
    vec3 F0 = (refractionIndex1 - refractionIndex2) / (refractionIndex1 + refractionIndex2);
    return F0 * F0;
}

// F Schlick's approximation
// eta = eta_ext/eta_int or eta_i/eta_t
// https://en.wikipedia.org/wiki/Schlick%27s_approximation
// https://google.github.io/filament/Filament.html#materialsystem/specularbrdf/fresnel(specularf)
// Schlick 1994, "An Inexpensive BRDF Model for Physically-Based Rendering"
// cosTheta: NdotV/NdotL or HdotV/HdotL
// F0: reflectance at 0 degree angle (parallel to normal) (achromatic for dielectrics, and chromatic for metals)
// F90: reflectance at 90 degree angle (orthogonal to normal) (both dielectrics and conductors exhibit achromatic specular reflectance)
// return: reflection amount
float F_Schlick(const float cosTheta)
{
    const float f = clamp(1.0f - cosTheta, 0.0f, 1.0f);
    const float f2 = f * f;
    return f2 * f2 * f;
}
float F_Schlick(const float cosTheta, const float F0, const float F90)
{   
    return F0 + (F90 - F0) * F_Schlick(cosTheta); 
}
vec3 F_Schlick(const float cosTheta, const vec3 F0, const float F90)
{   
    return F0 + (vec3(F90) - F0) * F_Schlick(cosTheta); 
}
float F_Schlick(const float cosTheta, const float F0)
{   
    return F_Schlick(cosTheta, F0, 1.0f);  
}
vec3 F_Schlick(const float cosTheta, const vec3 F0)
{   
    return F_Schlick(cosTheta, F0, 1.0f); 
}

/*
*   https://www.scratchapixel.com/lessons/3d-basic-rendering/introduction-to-shading/reflection-refraction-fresnel
*   https://pbr-book.org/3ed-2018/Reflection_Models/Specular_Reflection_and_Transmission#FresnelReflectance
*   https://en.wikipedia.org/wiki/Fresnel_equations
*   eta_i: index of refraction for medium outside object
*   eta_t: index of refraction for medium inside object
*   cosThetaI: angle between incoming ray and normal
*   cosThetaT: angle between refracted ray and negative normal
*   return: reflection amount 
*/
float F_Dielectric(inout float eta_i, inout float eta_t, float cosThetaI, out float cosThetaT) {
    // swap etas when inside of object -> IncomingDotN < 0
    // since we handle eta outside -> not used
    if(cosThetaI <= 0){
        const float buff = eta_i;
        eta_i = eta_t;
        eta_t = buff;
        cosThetaI = abs(cosThetaI);
    }
    const float eta = eta_i/eta_t;
    // no reflection -> straight transmission
    if (eta == 1.0f) {
        cosThetaT = -cosThetaI;
        return 0.0f;
    }
    /* Using Snell's law, calculate the squared sine of the 
       angle between the normal and the transmitted ray */
    const float sinThetaTSq = eta * eta * (1.0f - cosThetaI * cosThetaI);

    // Total internal reflection
    if (sinThetaTSq >= 1.0f) {
        cosThetaT = 0.0f;
        return 1.0f;
    }

    cosThetaT = sqrt(max(1.0f - sinThetaTSq, 0.0f));

    const float rs =  (eta_i * cosThetaI - eta_t * cosThetaT)
                    / (eta_i * cosThetaI + eta_t * cosThetaT); /*⊥*/
    const float rp =  (eta_t * cosThetaI - eta_i * cosThetaT)
                    / (eta_t * cosThetaI + eta_i * cosThetaT); /*∥*/

    /* No polarization -- return the unpolarized reflectance */
    return 0.5f * (rs * rs + rp * rp);
}

/*
*   https://pbr-book.org/3ed-2018/Reflection_Models/Specular_Reflection_and_Transmission#FresnelReflectance
*   eta:
*   k: extinction coefficient
*   cosThetaI: angle between incoming ray and normal
*   return: reflection amount 
*/
// From "PHYSICALLY BASED LIGHTING CALCULATIONS FOR COMPUTER GRAPHICS" by Peter Shirley
// http://www.cs.virginia.edu/~jdl/bib/globillum/shirley_thesis.pdf
vec3 F_Conductor(const vec3 eta, const vec3 k, const float cosThetaI)
{
    const float cosThetaI2 = cosThetaI * cosThetaI;
    const float sinThetaI2 = max(1.0f - cosThetaI2, 0.0f);
    const float sinThetaI4 = sinThetaI2 * sinThetaI2;

    const vec3 eta2 = eta * eta;
    const vec3 k2 = k * k;
    const vec3 innerTerm = eta2 - k2 - sinThetaI2;
    const vec3 a2PlusB2 = sqrt(max(innerTerm * innerTerm + 4.0f * eta2 * k2, 0.0f));
    const vec3 a = sqrt(max((a2PlusB2 + innerTerm) * 0.5f, 0.0f));

    const vec3 A = a2PlusB2 + cosThetaI2;
    const vec3 B = 2.0f * cosThetaI * a;
    const vec3 C = A + sinThetaI4;
    const vec3 D = B * sinThetaI2;

    const vec3 rs = (A - B)
                   / (A + B); /*⊥*/
    const vec3 rp = rs * (C - D)
                   / (C + D); /*∥*/

    return 0.5f * (rs + rp);
}

/*
** reflection
*/
// Walter 2007, "Microfacet Models for Refraction through Rough Surfaces" eq: 39
// wi: normalized incoming dir pointing away
// n: normalized normal
// result: normalized reflection dir pointing away
vec3 reflectionDir(const vec3 wi, const vec3 n){
    return 2.0f * dot(wi, n) * n - wi;
}
/* Bram de Greve 2006 - Reflections and Refractions in Ray Tracing */
// wi: normalized incoming dir pointing in
// n: normalized normal
// result: normalized reflection dir pointing away
// vec3 reflection2(const vec3 wi, const vec3 n){
//     //return 2.0 * dot(wi, n) * n - wi;
//     return wi - 2.0f * dot(wi, n) * n;
// }
// vec3 reflection2_norm(const vec3 wi, const vec3 n){
//     return normalize(reflection2(wi, n));
// }

/*
** refraction
*/
// Walter 2007, "Microfacet Models for Refraction through Rough Surfaces"
// eq: 40 is wrong
// fixed -> with or without cosThetaT
// wi: normalized incoming dir pointing away
// n: normalized normal
// eta_i: index of refraction of media on the incident side
// eta_t: index of refraction of media on the transmitted side
// cosThetaT: refraction angle (dot(wi,-n))
// result: normalized refraction dir pointing away
vec3 refractionDir(const vec3 wi, const vec3 n, const float eta_i, const float eta_t, const float cosThetaT)
{
    const float eta = eta_i / eta_t;
    const float wiDotN = dot(wi, n);
    return (eta * wiDotN - sign(wiDotN) * cosThetaT) * n - eta * wi;
}
vec3 refractionDir(const vec3 wi, const vec3 n, const float eta_i, const float eta_t)
{
    const float eta = eta_i / eta_t;
    const float wiDotN = dot(wi, n);
    const float wiDotN2 = wiDotN * wiDotN;
    const float sinThetaTSq = eta * eta * (1.0f - wiDotN2);
    const float cosThetaT = sqrt(max(1 - sinThetaTSq, 0));
    return (eta * wiDotN - sign(wiDotN) * cosThetaT) * n - eta * wi;
}

// https://github1s.com/mitsuba-renderer/mitsuba/blob/master/src/libcore/util.cpp#L814-L862
// eta_i: eta inside ray space
// eta_o: eta outside ray space
float fresnelDiffuseReflectance(const float eta_ext, const float eta_int) {
    const float eta = eta_int / eta_ext;
    const float invEta = eta_ext / eta_int;
    /* Fast mode: the following code approximates the
    ** diffuse Frensel reflectance for the eta<1 and
    ** eta>1 cases. An evalution of the accuracy led
    ** to the following scheme, which cherry-picks
    ** fits from two papers where they are best.
    */
    if (eta < 1) {
        /* Fit by Egan and Hilgeman (1973). Works
        ** reasonably well for "normal" IOR values (<2).

        ** Max rel. error in 1.0 - 1.5 : 0.1%
        ** Max rel. error in 1.5 - 2   : 0.6%
        ** Max rel. error in 2.0 - 5   : 9.5%
        */
        return -1.4399f * (eta * eta)
                + 0.7099f * eta
                + 0.6681f
                + 0.0636f / eta;
    } else {
        /* Fit by d'Eon and Irving (2011)
        **
        ** Maintains a good accuracy even for
        ** unrealistic IOR values.
        **
        ** Max rel. error in 1.0 - 2.0   : 0.1%
        ** Max rel. error in 2.0 - 10.0  : 0.2%
        */
        const float invEta2 = invEta*invEta;
        const float invEta3 = invEta2*invEta;
        const float invEta4 = invEta3*invEta;
        const float invEta5 = invEta4*invEta;

        return 0.919317f - 3.4793f * invEta
                + 6.75335f * invEta2
                - 7.80989f * invEta3
                + 4.98554f * invEta4
                - 1.36881f * invEta5;
    }

    return 0.0f;
}

/* 
** UNTESTED/UNFINISHED 
*/
// Cook and Torrance 1982, "A Reflectance Model for Computer Graphics"
// float F_Cook_Torrance(const float F0, const float cosTheta)
// {   
//     const float eta = (1.0f + sqrt(F0))/(1.0f - sqrt(F0));
//     const float c = cosTheta;
//     const float g = sqrt((eta * eta) + (c * c) - 1.0f);

//     const float A = (g - c) / (g + c);
//     const float B = ((g + c) * c - 1.0f) / ((g - c) * c + 1.0f);
//     return 0.5f * (A * A) * (1.0f + (B * B));
// }
// eta = refraction index
// kappa extinction coefficient
// float F_Full(const float eta, const float kappa){
//     return 0;
// }

#endif /* GLSL_BXDFS_FRESNEL */