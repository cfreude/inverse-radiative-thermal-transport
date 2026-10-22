#ifndef  _TYPE_CONV_DEF_H
#define  _TYPE_CONV_DEF_H

// shared structures between GLSL/HLSL and C/C++
// define GLSL or HLSL if used in GLSL/HLSL
// define nothing when used in pure c++
// define GLM if used with glm
#define STRUCT(content, name)   struct name { content };
#if defined(GLSL)
#define BOOL(v)                 bool v;
#define INT(v)                  int v;
#define UINT(v)                 uint v;
#define FLOAT(v)                float v;
#define DOUBLE(v)               double v;
// vector
#define VEC2(v)                 vec2 v;
#define VEC3(v)                 vec3 v;
#define VEC4(v)                 vec4 v;
#define DVEC2(v)                dvec2 v;
#define DVEC3(v)                dvec3 v;
#define DVEC4(v)                dvec4 v;
#define UVEC2(v)                uvec2 v;
#define UVEC3(v)                uvec3 v;
#define UVEC4(v)                uvec4 v;
// matrix
#define MAT4(v)                 mat4 v;
#define MAT4X3(v)               mat4x3 v;
#elif defined(HLSL)
#define BOOL(v)                 bool v;
#define INT(v)                  int v;
#define UINT(v)                 uint v;
#define FLOAT(v)                float v;
#define DOUBLE(v)               double v;
// vector
#define VEC2(v)                 float2 v;
#define VEC3(v)                 float3 v;
#define VEC4(v)                 float4 v;
#define DVEC2(v)                double2 v;
#define DVEC3(v)                double3 v;
#define DVEC4(v)                double4 v;
#define UVEC2(v)                uint2 v;
#define UVEC3(v)                uint3 v;
#define UVEC4(v)                uint4 v;
// matrix
#define MAT4(v)                 float4x4 v;
#define MAT4X3(v)               float4x3 v;
#else
#define BOOL(v)                 unsigned int v;     // sometimes a bool is 1 byte in c++ and in glsl it is always 4 byte
#define INT(v)                  int v;
#define UINT(v)                 unsigned int v;
#define FLOAT(v)                float v;
#define DOUBLE(v)               double v;
#if defined(GLM)
#include "glm/glm.hpp"
// vector
#define VEC2(v)                 glm::vec2 v;
#define VEC3(v)                 glm::vec3 v;
#define VEC4(v)                 glm::vec4 v;
#define DVEC2(v)                glm::dvec2 v;
#define DVEC3(v)                glm::dvec3 v;
#define DVEC4(v)                glm::dvec4 v;
#define UVEC2(v)                glm::uvec2 v;
#define UVEC3(v)                glm::uvec3 v;
#define UVEC4(v)                glm::uvec4 v;
// matrix
#define MAT4(v)                 glm::mat4 v;
#define MAT4X3(v)               glm::mat4x3 v;
#else
// vector
#define VEC2(v)                 float v[2];
#define VEC3(v)                 float v[3];
#define VEC4(v)                 float v[4];
#define DVEC2(v)                double v[2];
#define DVEC3(v)                double v[3];
#define DVEC4(v)                double v[4];
#define UVEC2(v)                unsigned int v[2];
#define UVEC3(v)                unsigned int v[3];
#define UVEC4(v)                unsigned int v[4];
// matrix
#define MAT4(v)                 float v[16];
#define MAT4X3(v)               float v[12];
#endif
#endif

// --Example--
// GlobalUbo_s can now be used in c++ and HLSL/GLSL
// STRUCT (
//     MAT4    (viewMat)
//     MAT4    (inverseViewMat)
//     MAT4    (projMat)
//     MAT4    (inverseProjMat)
//     VEC4    (viewPos)
//     VEC4    (viewDir)
//     VEC2    (size)
//     FLOAT   (frameCount)
//     BOOL    (shade)
//     FLOAT   (dither_strength)
// ,GlobalUbo_s)

#endif /*_TYPE_CONV_DEF_H*/
