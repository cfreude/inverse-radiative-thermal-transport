/*
 * PCG Random Number Generation for C.
 *
 * Copyright 2014 Melissa O'Neill <oneill@pcg-random.org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For additional information about the PCG random number generation scheme,
 * including its license and other licensing options, visit
 *
 *       http://www.pcg-random.org
 */

/*
 * This code is derived from the full C implementation, which is in turn
 * derived from the canonical C++ PCG implementation. The C++ version
 * has many additional features and is preferable if you can use C++ in
 * your project.
 */

#ifndef PCG32_GLSL
#define PCG32_GLSL 1

#extension GL_EXT_shader_explicit_arithmetic_types : require

// Generate a uniformly distributed 32-bit random number
uint32_t nextUIntPCG32(inout u64vec2 seed) {
    uint64_t oldstate = seed.x;
    seed.x = oldstate * 6364136223846793005UL + seed.y;
    uint32_t xorshifted = uint32_t(((oldstate >> 18u) ^ oldstate) >> 27u);
    uint32_t rot = uint32_t(oldstate >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((~rot + 1u) & 31));
}

// Generate a single precision floating point value on the interval [0, 1)
float nextFloatPCG32(inout u64vec2 seed) {
    // Trick from MTGP: generate an uniformly distributed
    // double precision number in [1,2) and subtract 1.
    const uint32_t u = (nextUIntPCG32(seed) >> 9) | 0x3f800000u;
    return uintBitsToFloat(u) - 1.0f;
}

// Generate a double precision floating point value on the interval [0, 1)
double nextDoublePCG32(inout u64vec2 seed) {
    // Trick from MTGP: generate an uniformly distributed
    // double precision number in [1,2) and subtract 1.
    const uint64_t u = (uint64_t(nextUIntPCG32(seed)) << 20) | 0x3ff0000000000000UL;
    return uint64BitsToDouble(u) - 1.0;
}

/* vec2(state, inc) */
u64vec2 seedPCG32(uint64_t initstate, uint64_t initseq) {
    initseq = max(1, initseq);
    u64vec2 s;
    s.x = 0U;
    s.y = (initseq << 1u) | 1u;
    nextUIntPCG32(s);
    s.x += initstate;
    nextUIntPCG32(s);
    return s;
}

#endif // PCG32_GLSL