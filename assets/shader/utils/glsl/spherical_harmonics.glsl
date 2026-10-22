#ifndef GLSL_SPHERICAL_HARMONICS
#define GLSL_SPHERICAL_HARMONICS
#extension GL_EXT_control_flow_attributes : enable

// based on:
// https://github.com/google/spherical-harmonics
// branch 'master' on Aug. 4, 2021

// --- original copyright notice ---
// Copyright 2015 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// --- --- ---

float factorial(const int x) {
    switch(x) {
        case 0: return 1.0f;
        case 1: return 1.0f;
        case 2: return 2.0f;
        case 3: return 6.0f;
        case 4: return 24.0f;
        case 5: return 120.0f;
        case 6: return 720.0f;
        case 7: return 5040.0f;
        case 8: return 40320.0f;
        case 9: return 362880.0f;
        case 10: return 3628800.0f;
        case 11: return 39916800.0f;
        case 12: return 479001600.0f;
        case 13: return 6227020800.0f;
        case 14: return 87178291200.0f;
        case 15: return 1307674368000.0f;
        case 16: return 20922789888000.0f;
        case 17: return 355687428096000.0f;
        default: break;
    }
    float s = 1.0f;
    [[unroll, dependency_infinite]]
    for (int n = 1; n <= x; n++) {
        s *= float(n);
    }
    return s;
}

float doubleFactorial(const int x) {
    switch(x) {
        case 0: return 1.0f;
        case 1: return 1.0f;
        case 2: return 2.0f;
        case 3: return 3.0f;
        case 4: return 8.0f;
        case 5: return 15.0f;
        case 6: return 48.0f;
        case 7: return 105.0f;
        case 8: return 384.0f;
        case 9: return 945.0f;
        case 10: return 3840.0f;
        case 11: return 10395.0f;
        case 12: return 46080.0f;
        case 13: return 135135.0f;
        case 14: return 645120.0f;
        case 15: return 2027025.0f;
        case 16: return 10321920.0f;
        case 17: return 34459425.0f;
        default: break;
    }
    float s = 1.0f;
    float n = float(x);
    [[unroll, dependency_infinite]]
    while (n >= 2.0) {
        s *= n;
        n -= 2.0;
    }
    return s;
}

// Evaluate the associated Legendre polynomial of degree @l and order @m at
// coordinate @x. The inputs must satisfy:
// 1. l >= 0
// 2. 0 <= m <= l
// 3. -1 <= x <= 1
// See http://en.wikipedia.org/wiki/Associated_Legendre_polynomials
//
// This implementation is based off the approach described in [1],
// instead of computing Pml(x) directly, Pmm(x) is computed. Pmm can be
// lifted to Pmm+1 recursively until Pml is found
float evalLegendrePolynomial(const int l, const int m, const float x) {
    // Compute Pmm(x) = (-1)^m(2m - 1)!!(1 - x^2)^(m/2), where !! is the float
    // factorial.
    float pmm = 1.0;
    // P00 is defined as 1.0, do don't evaluate Pmm unless we know m > 0
    if (m > 0) {
        float sign = (m % 2 == 0 ? 1.0f : -1.0f);
        pmm = sign * doubleFactorial(2 * m - 1) * pow(1.0f - x * x, m / 2.0f);
    }

    if (l == m) {
        // Pml is the same as Pmm so there's no lifting to higher bands needed
        return pmm;
    }

    // Compute Pmm+1(x) = x(2m + 1)Pmm(x)
    float pmm1 = x * (2.0f * float(m) + 1.0f) * pmm;
    if (l == m + 1) {
        // Pml is the same as Pmm+1 so we are done as well
        return pmm1;
    }

    // Use the last two computed bands to lift up to the next band until l is
    // reached, using the recurrence relationship:
    // Pml(x) = (x(2l - 1)Pml-1 - (l + m - 1)Pml-2) / (l - m)
    [[unroll, dependency_infinite]]
    for (int n = m + 2; n <= l; n++) {
        const float pmn = (x * (2 * n - 1) * pmm1 - (n + m - 1) * pmm) / (n - m);
        pmm = pmm1;
        pmm1 = pmn;
    }
    // Pmm1 at the end of the above loop is equal to Pml
    return pmm1;
}

// l is the band, range [0..N]
// m in the range [-l..l]
// theta in the range [0..Pi]
// phi in the range [0..2*Pi]
#define EVAL_SH(norm, costheta)                                                                                         \
    /*CHECK(l >= 0, "l must be at least 0.");*/                                                                         \
    /*CHECK(-l <= m && m <= l, "m must be between -l and l.");*/                                                        \
    const int absM = abs(m);                                                                                            \
    const float kml = sqrt((2.0f * l + 1.0f) * factorial(l - absM) / ((norm) * factorial(l + absM)));                   \
    const float kmlTimesLPolyEval = kml * evalLegendrePolynomial(l, absM, (costheta));                                  \
    if (m == 0) return kmlTimesLPolyEval;                                                                               \
    const float mTimesPhi = absM * phi;                                                                                 \
    const float sh = 1.414213562373095f /*sqrt(2)*/ * kmlTimesLPolyEval;                                                \
    if (m > 0) return sh * cos(mTimesPhi);                                                                              \
    else return sh * sin(mTimesPhi);


// smoothing
float smoothSH(const int l, const float factor) {
    return 1.0f / (1.0f + factor * float((l * (l + 1)) * (l * (l + 1))));
}

// --- spherical harmonics ---
uint getIndexSH(const int l, const int m) {
    return uint(l * (l + 1) + m);
}
void dirToSphericalCoordsSH(in vec3 dir, out float theta, out float phi) { // assume dir is unit vector
	theta = atan(sqrt(dir.x * dir.x + dir.y * dir.y), dir.z);
    phi = atan(dir.y, dir.x);
}

// orthonormalized
float evalOrthonormSH(const int l, const int m, const float theta, const float phi) {
    EVAL_SH(12.5663706144f /*4PI*/, cos(theta))
}

float evalOrthonormSH(const int l, const int m, const vec3 dir) {
    float theta, phi;
    dirToSphericalCoordsSH(dir, theta, phi);
    return evalOrthonormSH(l, m, theta, phi);
}

// 4pi normalized
float eval4PiNormSH(const int l, const int m, const float theta, const float phi) {
    EVAL_SH(1.0f, cos(theta))
}

float eval4PiNormSH(const int l, const int m, const vec3 dir) {
    float theta, phi;
    dirToSphericalCoordsSH(dir, theta, phi);
    return eval4PiNormSH(l, m, theta, phi);
}

// --- hemispherical harmonics ---
#define getIndexHSH getIndexSH

void dirToSphericalCoordsHSH(in vec3 dir, in vec3 normal, in vec3 tangent, out float theta, out float phi) { // assumes input vectors are unit length!
    const vec3 bitangent = cross(normal, tangent);
	//theta = atan(sqrt(dir.x * dir.x + dir.y * dir.y), dot(dir, normal));
    theta = acos(dot(dir, normal));
    phi = atan(dot(dir, bitangent), dot(dir, tangent));
}

// orthonormalized
float evalOrthonormHSH(const int l, const int m, const float theta, const float phi) {
    EVAL_SH(6.28318530718 /*2Pi*/, 2.0f * cos(theta) - 1.0f)
}

float evalOrthonormHSH(const int l, const int m, const vec3 dir, const vec3 normal, const vec3 tangent, const bool clampTheta) { // computes (phi,theta) such that +z --> theta==0, z==0 --> theta==pi/2, and +x --> phi==0
    float theta, phi;
    dirToSphericalCoordsHSH(dir, normal, tangent, theta, phi);
    // clamp
    if(clampTheta) theta = clamp(theta, 0.0, 1.570796326794897f /*pi/2*/);
    // or kill
    else if(dot(dir, normal) < 0.0) return 0.0;

    return evalOrthonormHSH(l, m, theta, phi);
}

// 2pi normalized
float eval2PiNormHSH(const int l, const int m, const float theta, const float phi) {
    EVAL_SH(1.0f, 2.0f * cos(theta) - 1.0f)
}

float eval2PiNormHSH(const int l, const int m, const vec3 dir, const vec3 normal, const vec3 tangent, const bool clampTheta) { // computes (phi,theta) such that +z --> theta==0, z==0 --> theta==pi/2, and +x --> phi==0
    float theta, phi;
    dirToSphericalCoordsHSH(dir, normal, tangent, theta, phi);
    // clamp
    if(clampTheta) theta = clamp(theta, 0.0, 1.570796326794897f /*pi/2*/);
    // or kill
    else if(dot(dir, normal) < 0.0) return 0.0;

    return eval2PiNormHSH(l, m, theta, phi);
}

#undef EVAL_SH
#endif /* GLSL_SPHERICAL_HARMONICS */