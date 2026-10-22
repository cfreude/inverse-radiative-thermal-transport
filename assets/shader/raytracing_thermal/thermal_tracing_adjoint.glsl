#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_shader_atomic_float : require
#extension GL_EXT_debug_printf : enable

#define TRACING_MODE (2)

#include "thermal_tracing_unified.glsl"
