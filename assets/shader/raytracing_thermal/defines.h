#ifndef  _DEFINES_H_
#define  _DEFINES_H_

#define RUNTIME_OPTIMIZED 1
#define CHECK_NAN_INF 1

#define MAX_NUM_POINT_LIGHTS 8
#define MAX_NUM_DIRECTIONAL_LIGHTS 8
#define MAX_NUM_SPOT_LIGHTS 8

#define GLSL_GLOBAL_DESC_SET                    0
#define GLSL_GLOBAL_IMAGE_DESC_SET              1

#define GLSL_GLOBAL_IMAGE_BINDING               0

#define GLSL_GLOBAL_UBO_BINDING                 0
#define GLSL_GLOBAL_INSTANCE_DATA_BINDING       1
#define GLSL_GLOBAL_GEOMETRY_DATA_BINDING       2
#define GLSL_GLOBAL_INDEX_BUFFER_BINDING        3
#define GLSL_GLOBAL_VERTEX_BUFFER_BINDING       4
#define GLSL_GLOBAL_LIGHT_DATA_BINDING          5
#define GLSL_GLOBAL_AS_BINDING                  6
#define GLSL_GLOBAL_NOISE_IMAGE_BINDING         7
#define GLSL_GLOBAL_OUT_IMAGE_BINDING           8
#define GLSL_GLOBAL_TRANSPORT_DATA_BINDING      9
#define GLSL_GLOBAL_AUX_UBO_BINDING             10
#define GLSL_GLOBAL_KELVIN_DATA_BINDING         11
#define GLSL_GLOBAL_AREA_DATA_BINDING           12
//#define GLSL_GLOBAL_EMISSION_DATA_BINDING       13
//#define GLSL_GLOBAL_ABSORPTION_DATA_BINDING     14
#define GLSL_GLOBAL_ADJTEMP4_DATA_BINDING       13
#define GLSL_GLOBAL_ADJADJST_DATA_BINDING       14
#define GLSL_GLOBAL_ADJGRADX_DATA_BINDING       15

#ifdef GLSL
#define M_PI 3.14159265358979323846264338327950288f


/**/ // float --- make sure to sync with thermal_common.hpp
#define SCALAR float
#define glVec3 vec3
/*/ // double
#define SCALAR double
#define glVec3 dvec3
/**/

#else

#endif

// shared structures between GLSL and C/C++
#ifdef GLSL
    #define STRUCT(content, name) struct name { content };
    #define BOOL(n) bool n;
    #define INT(n) int n;
    #define UINT(n) uint n;
    #define FLOAT(n) float n;
    // vector
    #define VEC2(n) vec2 n;
    #define VEC3(n) vec3 n;
    #define VEC4(n) vec4 n;
    #define UVEC2(n) uvec2 n;
    #define UVEC3(n) uvec3 n;
    #define UVEC4(n) uvec4 n;
    // mat
    #define MAT4(n) mat4 n;
    #define MAT4X3(n) mat4x3 n;
#else
    #define STRUCT(content, name) typedef struct { content } name;
    #define BOOL(n) unsigned int n;
    #define INT(n) int n;
    #define UINT(n) unsigned int n;
    #define FLOAT(n) float n;
    // vector
    #define VEC2(n) float n[2];
    #define VEC3(n) float n[3];
    #define VEC4(n) float n[4];
    #define UVEC2(n) unsigned int[2] n;
    #define UVEC3(n) unsigned int[3] n;
    #define UVEC4(n) unsigned int[4] n;
    // matrix
    #define MAT4(n) float n[16];
    #define MAT4X3(n) float n[12];
#endif

// global ubo
STRUCT (
    MAT4    (viewMat)
    MAT4    (inverseViewMat)
    MAT4    (projMat)
    MAT4    (inverseProjMat)
    VEC4    (viewPos)
    VEC4    (viewDir)
, GlobalUbo)

// auxiliary ubo
STRUCT(
    INT     (instanceCount)
    UINT    (vertexCount)
    UINT    (sourceVertexInd)
    FLOAT   (displayScale) //
    UINT    (visualization)
    FLOAT   (displayRangeMin)
    FLOAT   (displayRangeMax)
    UINT    (rayDepth) //
    UINT    (rayCount)
    UINT    (batchSeed)
    FLOAT   (clipDistance)
    BOOL    (backfaceCulling) //
    FLOAT   (stefanBoltzmannConst)
    FLOAT   (gradEdgeRatio)
    UINT    (historySize) // for adjoint thermal sim, we sum over all time steps in the adjoint tracing shader
    UINT    (cullMask) // traceRayEXT cullMask, currently only used for scene view, not for transport
    FLOAT   (wireframeFactor)
    FLOAT   (tinyEps)
    FLOAT   (tinyDist)
    INT     (aoSamples) //
    FLOAT   (aoDist)
    FLOAT   (aoBlending)
    INT     (align2)
    INT     (align3) //

, AuxiliaryUbo)

// instance
STRUCT(
    MAT4    (model_matrix)
    MAT4    (trp_inv_model_matrix)
    INT     (geometry_buffer_offset)
    UINT    (geometryCount)
    FLOAT   (diffuseReflectance)
    FLOAT   (specularReflectance)
    INT     (diffuseEmission)
    FLOAT   (absorption)
    INT     (traceable)
    BOOL    (traceBoundingCone)
, InstanceSSBO)
// geometry
STRUCT(
    INT     (index_buffer_offset)
    INT     (vertex_buffer_offset)
    BOOL    (has_indices)
    INT     (baseColorTexIdx)
    VEC4    (baseColorFactor)
    FLOAT   (alphaCutoff)
    UINT    (triangleCount)
    INT     (aligned1)
    INT     (aligned2)
, GeometrySSBO)

// geometry
STRUCT(
    VEC4(factor)
, TransportFactor)

// punctual lights
// type
#define LIGHT_TYPE_DIRECTIONAL 0
#define LIGHT_TYPE_POINT 1
#define LIGHT_TYPE_SPOT 2
STRUCT(
    VEC3    (color)
    FLOAT   (intensity)
    VEC3    (position)
    FLOAT   (range)
    VEC3    (direction)
    FLOAT   (lightAngleScale)
    VEC2    (alligne)
    INT     (type)
    FLOAT   (lightAngleOffset)
, PunctualLight)
// lights
STRUCT(
    PunctualLight       punctual_lights[8];
    INT                 (punctual_light_count)
, LightsSSBO)

// undef
#undef STRUCT
#undef BOOL
#undef INT
#undef UINT
#undef FLOAT
// vector
#undef VEC2
#undef VEC3
#undef VEC4
#undef UVEC2
#undef UVEC3
#undef UVEC4
// matrix
#undef MAT4
#undef MAT4X3

#endif /*_DEFINES_H_*/