
#include "defines.h"
#include "../utils/glsl/ray_tracing_utils.glsl"
#include "../utils/glsl/rendering_utils.glsl"

// hard-cutoff for max depth (for mem-alloc -- alternatively re-write adjoint to double-trace instead of store path vertices)
#define MAX_MAX_DEPTH (512)

// set 0
#define CONV_TEXTURE_BINDING GLSL_GLOBAL_IMAGE_BINDING
#define CONV_TEXTURE_SET GLSL_GLOBAL_IMAGE_DESC_SET

layout(binding = GLSL_GLOBAL_UBO_BINDING, set = GLSL_GLOBAL_DESC_SET) uniform global_ubo { AuxiliaryUbo ubo; };
layout(binding = GLSL_GLOBAL_INSTANCE_DATA_BINDING, set = GLSL_GLOBAL_DESC_SET) buffer instance_ssbo { InstanceSSBO instance_buffer[]; };
layout(binding = GLSL_GLOBAL_GEOMETRY_DATA_BINDING, set = GLSL_GLOBAL_DESC_SET) buffer geometry_ssbo { GeometrySSBO geometry_buffer[]; };
struct vertex_s {
	vec4	position;
	vec4	normal;	
	vec4	tangent;

	vec2	texture_coordinates_0;
	vec2	texture_coordinates_1;
	vec4	color_0;
};

layout(binding = GLSL_GLOBAL_INDEX_BUFFER_BINDING, set = GLSL_GLOBAL_DESC_SET) buffer index_storage_buffer { uint index_buffer[]; };
layout(binding = GLSL_GLOBAL_VERTEX_BUFFER_BINDING, set = GLSL_GLOBAL_DESC_SET) buffer vertex_storage_buffer { vertex_s vertex_buffer[]; };

layout(binding = GLSL_GLOBAL_AS_BINDING, set = GLSL_GLOBAL_DESC_SET) uniform accelerationStructureEXT topLevelAS;

layout(binding = GLSL_GLOBAL_OUT_IMAGE_BINDING, set = GLSL_GLOBAL_DESC_SET) buffer transport_storage_buffer { SCALAR transport_buffer[]; };
layout(binding = GLSL_GLOBAL_AREA_DATA_BINDING, set = GLSL_GLOBAL_DESC_SET) buffer triangle_area_storage_buffer { SCALAR triangle_area_buffer[]; };

#if TRACING_MODE == 2 // adjoint
layout(binding = GLSL_GLOBAL_ADJTEMP4_DATA_BINDING, set = GLSL_GLOBAL_DESC_SET) buffer adjoint_temperatures_storage_buffer { SCALAR adjoint_temp4_buffer[]; };
layout(binding = GLSL_GLOBAL_ADJADJST_DATA_BINDING, set = GLSL_GLOBAL_DESC_SET) buffer adjoint_adjointstate_storage_buffer { SCALAR adjoint_adjSt_buffer[]; };
layout(binding = GLSL_GLOBAL_ADJGRADX_DATA_BINDING, set = GLSL_GLOBAL_DESC_SET) buffer adjoint_gradientvtxp_storage_buffer { SCALAR adjoint_gradX_buffer[]; };
#endif

#include "payload.glsl"
layout(location = 0) rayPayloadEXT RayPayload rp;

struct Ray {
	vec3 origin, direction;
	vec3 bar_coord;
	float t_min, t_max;
	SCALAR weight;
	bool absorbed;
};

uvec4 getThreadTriangleIndices() {
	uint thread_id = gl_LaunchIDEXT.y;
	uvec4 vertex_indices = uvec4(0, 0, 0, 0);

	bool stop = false;
	uint triangle_count = 0;
	for(uint i=0; i<ubo.instanceCount; i++) {	
		InstanceSSBO hit_instance = instance_buffer[i];		
		for(uint j=0; j<hit_instance.geometryCount; j++) {
			GeometrySSBO geometry_data = geometry_buffer[hit_instance.geometry_buffer_offset + j];
			if(thread_id < (triangle_count + geometry_data.triangleCount)) { // triangle in current geometry_buffer
				uint triangle_offset = thread_id - triangle_count;
				
				/*
				if(thread_id < triangle_count)
				{
					debugPrintfEXT("ERROR %d >= %d, ID: %d", thread_id, triangle_count, gl_LaunchIDEXT.y);
					return uvec4(0, 0, 0, 0);
				} else {
					if(gl_LaunchIDEXT.y == 0)
						debugPrintfEXT("OK");
				}
				*/
					
				if(geometry_data.has_indices) {
					// vertex index zero based
					uint indices_offset = geometry_data.index_buffer_offset + triangle_offset * 3;
					vertex_indices.x = geometry_data.vertex_buffer_offset + index_buffer[indices_offset + 0];
					vertex_indices.y = geometry_data.vertex_buffer_offset + index_buffer[indices_offset + 1];
					vertex_indices.z = geometry_data.vertex_buffer_offset + index_buffer[indices_offset + 2];
				}
				else {					
					uint offset = triangle_offset * 3;
					vertex_indices.x = geometry_data.vertex_buffer_offset + offset + 0;
					vertex_indices.y = geometry_data.vertex_buffer_offset + offset + 1;
					vertex_indices.z = geometry_data.vertex_buffer_offset + offset + 2;
				}
				vertex_indices.w = i;
				stop = true;
				break;
			}
			triangle_count += geometry_data.triangleCount;
		}
		if(stop)
			break;
	}

	return vertex_indices;
}

vec3 getNormal(uvec4 _vertex_indices)
{	
	vertex_s v0 = vertex_buffer[_vertex_indices.x];	
	vertex_s v1 = vertex_buffer[_vertex_indices.y];	
	vertex_s v2 = vertex_buffer[_vertex_indices.z];
		
	mat4 model_matrix = instance_buffer[_vertex_indices.w].model_matrix;
		
	vec4 p0 = model_matrix * v0.position;	
	vec4 p1 = model_matrix * v1.position;
	vec4 p2 = model_matrix * v2.position;

	//vec3 normal = normalize(cross(p1.xyz-p0.xyz, p2.xyz-p1.xyz));
	vec3 normal = normalize(cross(p1.xyz-p0.xyz, p2.xyz-p0.xyz));

	return normal;
}

vec3 rand(inout uint _seed)
{
	return vec3(randomFloat(_seed), randomFloat(_seed), randomFloat(_seed));
	//return hash3(_seed);
}

Ray getEmissionRay(uvec4 _vertex_indices, SCALAR ray_weight, inout uint _seed)
{
	vec3 barycentricCoords = sampleUnitTriangleUniform(rand(_seed).xy);
	
	float sum = barycentricCoords.x+barycentricCoords.y+barycentricCoords.z;
	if(sum > 1.000001)
		debugPrintfEXT("#%d | getEmissionRay | barycentricCoords sum(%f) > 1.000001: %f, %f, %f", gl_LaunchIDEXT.y, sum, barycentricCoords.x, barycentricCoords.y, barycentricCoords.z);	

	vertex_s v0 = vertex_buffer[_vertex_indices.x];	
	vertex_s v1 = vertex_buffer[_vertex_indices.y];	
	vertex_s v2 = vertex_buffer[_vertex_indices.z];
	
	InstanceSSBO instance = instance_buffer[_vertex_indices.w];
	
	vec4 origin = vec4(v0.position * barycentricCoords.x + v1.position * barycentricCoords.y + v2.position * barycentricCoords.z);
	origin = instance.model_matrix * origin;
			
	vec3 normal = getNormal(_vertex_indices);
	
	Ray ray;
	ray.origin = offsetRayToAvoidSelfIntersection(origin.xyz, normal);
	ray.direction = normal;
	ray.bar_coord = barycentricCoords;
	ray.t_min = 1e-12;
	ray.t_max = 1e12;
	ray.weight = ray_weight;
	ray.absorbed = false;	
	
	if(instance.diffuseEmission == 1) {		
		vec3 rnd_direction = sampleUnitHemisphereCosine(rand(_seed).xy);
		ray.direction = tangentSpaceToWorldSpace(rnd_direction, normal);		
	} else {
		ray.direction = normal;
	}
	
	return ray;
}

Ray getNextRay(uvec4 _vertex_indices, vec3 _barycentricCoords, vec3 _inDir, SCALAR ray_weight, inout uint _seed){
	vertex_s v0 = vertex_buffer[_vertex_indices.x];	
	vertex_s v1 = vertex_buffer[_vertex_indices.y];	
	vertex_s v2 = vertex_buffer[_vertex_indices.z];
	
	InstanceSSBO instance = instance_buffer[_vertex_indices.w];
	
	vec4 origin = vec4(v0.position * _barycentricCoords.x + v1.position * _barycentricCoords.y + v2.position * _barycentricCoords.z);
	origin = instance.model_matrix * origin;
		
	vec3 normal = getNormal(_vertex_indices);

	// calculate next direction ray
	Ray ray;
	ray.origin = offsetRayToAvoidSelfIntersection(origin.xyz, normal);
	ray.direction = normal;
	ray.bar_coord = _barycentricCoords;
	ray.t_min = 1e-12;
	ray.t_max = 1e12;
	ray.weight = ray_weight;
	ray.absorbed = false;

	float rnd = randomFloat(_seed);

	if(rnd >= instance.diffuseReflectance + instance.specularReflectance) {	// absorb
		ray.absorbed = true;
	} else if(rnd >= instance.diffuseReflectance && rnd < (instance.diffuseReflectance + instance.specularReflectance)) { // specular reflection		
		ray.direction = reflect(normalize(_inDir), normal);
	} else{ // diffuse reflection		
		vec3 rnd_direction = sampleUnitHemisphereCosine(rand(_seed).xy);
		ray.direction = tangentSpaceToWorldSpace(rnd_direction, normal);
	}

	return ray;
}

uint findMaxIdx(in vec3 b) {
	uint idx=0;
	float v = b[0];
	if( b[1] >= v ){
		idx = 1;
		v = b[1];
	}
	if( b[2] >= v ){
		idx = 2;
		//v = b[2];
	}
	return idx;
}
uint findMinIdx(in vec3 b) {
	uint idx=0;
	float v = b[0];
	if( b[1] < v ){
		idx = 1;
		v = b[1];
	}
	if( b[2] < v ){
		idx = 2;
		//v = b[2];
	}
	return idx;
}

#if TRACING_MODE == 2 // adjoint
void sumOverHistory(uint paramVertexIdx, uint absorbedVertexIdx, uint sourceVertexIdx, const glVec3 data){
	#if CHECK_NAN_INF == 1
	if( any(isnan(data)) || any(isinf(data)) ){
		debugPrintfEXT("Nan/Inf in grad data (paramVertex: %d, sourceVertex: %d, absorbedVertex: %d)",paramVertexIdx,sourceVertexIdx,absorbedVertexIdx);
		return;
	}
	#endif
	for(uint hidx=0; hidx < ubo.historySize; ++hidx){
		for(uint dim=0; dim<3; ++dim){ // iterate x,y,z
			atomicAdd(
				adjoint_gradX_buffer[3 * paramVertexIdx + dim],
				adjoint_adjSt_buffer[ubo.historySize * hidx + absorbedVertexIdx] *
				adjoint_temp4_buffer[ubo.historySize * hidx + sourceVertexIdx  ] *
				data[dim]
			);
		}
	}
}
#endif

void main() 
{	
	const float tinyEps  = ubo.tinyEps; //1e-2; // regularize (r^2+tinyEps) in denominator of geom. term (and when normalizing normal vectors)
	const float tinyDist = ubo.tinyDist; //1e-3; // min distance & cos(angle) before hit accepted

	uint maxDepth = MAX_MAX_DEPTH;
	if( ubo.rayDepth > 0 && ubo.rayDepth < maxDepth ) maxDepth = ubo.rayDepth;

	{ // checks
		if( gl_LaunchIDEXT.x == 0 && gl_LaunchIDEXT.y == 0){
			debugPrintfEXT("shader PRINTF test");
			uvec2 ind = linTo2D(11, 5);
			debugPrintfEXT("index test: %d == 11, ubo.vertexCount: %d, stefanBoltzmann: %g, hist size: %d", linFrom2D(ind.x, ind.y, 5), ubo.vertexCount, ubo.stefanBoltzmannConst, ubo.historySize);
			//debugPrintfEXT("emission_buffer[vertex==7] %g", vertex_emission_buffer[7]);
			//debugPrintfEXT("absorption_buffer[vertex==7] %g", vertex_absorption_buffer[7]);

			//debugPrintfEXT("traceable[0]=%d", instance_buffer[0].traceable);
			//debugPrintfEXT("traceable[1]=%d", instance_buffer[1].traceable);
			//debugPrintfEXT("traceable[2]=%d", instance_buffer[2].traceable);
			//debugPrintfEXT("traceable[3]=%d", instance_buffer[3].traceable);


#if TRACING_MODE == 1 // forward
			debugPrintfEXT("TRACING forward");
#elif TRACING_MODE == 2 // adjoint
			debugPrintfEXT("TRACING adjoint");
			debugPrintfEXT("t4 %.3g %.3g %.3g ...", adjoint_temp4_buffer[0], adjoint_temp4_buffer[1], adjoint_temp4_buffer[2]);
			debugPrintfEXT("la %.3g %.3g %.3g ...", adjoint_adjSt_buffer[0], adjoint_adjSt_buffer[1], adjoint_adjSt_buffer[2]);
			//for(uint ii=0; ii<ubo.vertexCount; ++ii) debugPrintfEXT("%.3g",adjoint_adjSt_buffer[ii]);
#endif
		}	
	}	
	// determine threads triangle vertex ids	
	uvec4 source_vertex_indices = getThreadTriangleIndices();

	SCALAR triangle_area = triangle_area_buffer[gl_LaunchIDEXT.y];
	//debugPrintfEXT("tra[%d] = %.3f", gl_LaunchIDEXT.y, triangle_area_buffer[gl_LaunchIDEXT.y]);

	uint ray_count = ubo.rayCount;

	const SCALAR ray_weight = triangle_area / SCALAR(ray_count * gl_LaunchSizeEXT.x); 

	// generate rays
	for(uint n=0; n<ray_count; n++){
		uint seed = initRandomSeed(gl_LaunchSizeEXT.y*gl_LaunchIDEXT.x+gl_LaunchIDEXT.y, ubo.batchSeed + n);
#if TRACING_MODE == 2 // adjoint
		uint resetSeed = seed;
		uint adjAbsorbedVertexIdx = 0;
		for(uint adjPass = 0; adjPass <= 1; ++adjPass){
			seed = resetSeed;
#endif
		uint depth = 0;		
		
		//seed += n;
		Ray ray = getEmissionRay(source_vertex_indices, ray_weight, seed);

		// recall: origin = vec4(v0.position * barycentricCoords.x + v1.position * barycentricCoords.y + v2.position * barycentricCoords.z);
		// and ... v0 = vertex_buffer[_vertex_indices.x] etc.
		uint sourceVertexIdx = source_vertex_indices[ findMaxIdx(ray.bar_coord) ];

		// get emissivity for the source vertex
		InstanceSSBO source_instance = instance_buffer[source_vertex_indices.w];
		float sourceEmissivity = 1.0f - source_instance.diffuseReflectance - source_instance.specularReflectance;
		ray.weight *= ubo.stefanBoltzmannConst * sourceEmissivity; // ray.weight == photon power

		uint ii_idx = linFrom2D(sourceVertexIdx, sourceVertexIdx, ubo.vertexCount);

#if TRACING_MODE == 1 // forward
		// deduct energy loss from diagonal (actually positive weight here, because we build -T so the result is positive definite)
		//atomicAdd(transport_buffer[ii_idx], ray.weight); // save some work here and add the diagonal later on the CPU side
#elif TRACING_MODE == 2 // adjoint
		uvec4 previous_vertex_indices = source_vertex_indices;
		vec3  previous_bar_coord = ray.bar_coord;
		InstanceSSBO previous_instance = source_instance;
		
		glVec3 dEdgeIrradiance, dPhotonPower_dv0, dPhotonPower_dv1, dPhotonPower_dv2;
		bool isEdge = false, isNeighbour = false;
		uint edgeVertex = 0;

		// derivatives wrt. source element area ... (i.e. d(ray.weight)/d(triangle_area)*d(triangle_area)/d(vertex_positions)) --> both at source and absorber --> cancels out
		if( adjPass == 1 ){ // note: test0_1bs (scaling the blocker) confirms that gradients improve when including this part (and the corresponding absorption term further below)
			glVec3 dadai,dadbi,dadci; // area derivatives
			{
				vec4 a = source_instance.model_matrix * vertex_buffer[source_vertex_indices.x].position;
				vec4 b = source_instance.model_matrix * vertex_buffer[source_vertex_indices.y].position;
				vec4 c = source_instance.model_matrix * vertex_buffer[source_vertex_indices.z].position;
				const SCALAR ai_1=a.x, ai_2=a.y, ai_3=a.z;
				const SCALAR bi_1=b.x, bi_2=b.y, bi_3=b.z;
				const SCALAR ci_1=c.x, ci_2=c.y, ci_3=c.z;
				// writes dadai,dadbi,dadci
				#include "symGradArea3D.h"
				
				#if CHECK_NAN_INF == 1
				if (any(isnan(dadai)) || any(isinf(dadai))) dadai = vec3(0.0f);
				if (any(isnan(dadbi)) || any(isinf(dadbi))) dadbi = vec3(0.0f);								
				if (any(isnan(dadci)) || any(isinf(dadci))) dadci = vec3(0.0f);	
				#endif
			}
			dPhotonPower_dv0 = dadai * ray.weight/triangle_area;
			dPhotonPower_dv1 = dadbi * ray.weight/triangle_area;
			dPhotonPower_dv2 = dadci * ray.weight/triangle_area;
			sumOverHistory( source_vertex_indices.x, sourceVertexIdx, sourceVertexIdx, -dPhotonPower_dv0 ); // source-to-source here (ii_idx), source-to-absorbed later (see below)
			sumOverHistory( source_vertex_indices.y, sourceVertexIdx, sourceVertexIdx, -dPhotonPower_dv1 );
			sumOverHistory( source_vertex_indices.z, sourceVertexIdx, sourceVertexIdx, -dPhotonPower_dv2 );
		}
#endif

		uvec4 ray_vertex_indices;
		while(true){ // trace and get hit info (break on miss)
			++depth;
			if( depth > maxDepth ) break;

			InstanceSSBO hit_instance;
			{ // just for code folding ...
				rp.instanceID = -1;
				traceRayEXT(topLevelAS, gl_RayFlagsNoneEXT, 0xf0, 0, 0, 0, ray.origin, ray.t_min, ray.direction, ray.t_max, 0);
						
				if(rp.instanceID < 0 || rp.hit_distance < tinyDist) break; // missed or too close

				hit_instance = instance_buffer[rp.instanceID];
				//if( hit_instance.traceable == 0 ) break; // taken care of by tracing flags above

				GeometrySSBO geometry_data = geometry_buffer[hit_instance.geometry_buffer_offset + rp.geometryIndex];
				uint vertex_offset = rp.primitiveID * 3;				
				ray_vertex_indices.xyz = uvec3(0, 1, 2) + vertex_offset;
				if(geometry_data.has_indices){
					for(uint i=0; i<3; i++){
						ray_vertex_indices[i] = index_buffer[geometry_data.index_buffer_offset + vertex_offset + i];
					}
				}
				ray_vertex_indices.xyz += geometry_data.vertex_buffer_offset;
				ray_vertex_indices.w = rp.instanceID;

				vec3 hit_normal = getNormal(ray_vertex_indices);
				//if(dot(hit_normal, ray.direction) > -tinyDist) break; // (near) backface hit
				if(dot(hit_normal, ray.direction) > -1e-3) break; // (near) backface hit

				ray.bar_coord = computeBarycentric(rp.hitAttribute); // update to hit coords
			}
#if TRACING_MODE == 2 // adjoint
				if( adjPass == 1 ){ // adjAbsorbedVertexIdx found in first pass, now process derivatives ...
					//if( gl_LaunchIDEXT.x == 0 && gl_LaunchIDEXT.y == 0) debugPrintfEXT("adj absorbed vertex %d", adjAbsorbedVertexIdx);
					
					//continuous derivatives: compute geometric term derivs for current path segment ... multiply source temp^4 and absorbed adjoint state ... sum up gradient
					{
						// for the segment from previous_vertex_indices to ray_vertex_indices compute grads ...
						
						// previous triangle verts
						vec4 ai = previous_instance.model_matrix * vertex_buffer[previous_vertex_indices.x].position;
						vec4 bi = previous_instance.model_matrix * vertex_buffer[previous_vertex_indices.y].position;
						vec4 ci = previous_instance.model_matrix * vertex_buffer[previous_vertex_indices.z].position;

						// current triangle verts
						vec4 aj = hit_instance.model_matrix * vertex_buffer[ray_vertex_indices.x].position;
						vec4 bj = hit_instance.model_matrix * vertex_buffer[ray_vertex_indices.y].position;
						vec4 cj = hit_instance.model_matrix * vertex_buffer[ray_vertex_indices.z].position;

						//// previous and current hit points
						//vec3 p = p_a.xyz * previous_bar_coord.x + p_b.xyz * previous_bar_coord.y + p_c.xyz * previous_bar_coord.z;
						//vec3 r = r_a.xyz * ray.bar_coord.x      + r_b.xyz * ray.bar_coord.y      + r_c.xyz * ray.bar_coord.z;
						//vec3 d = normalize(r-p); // should be == ray.direction

						SCALAR geomTerm = 0.0; // written in symGradGeomTerm3D.h
						glVec3 dgdai,dgdbi,dgdci,dgdaj,dgdbj,dgdcj; // written in symGradGeomTerm3D.h
						{
							const float ai_1=ai.x, ai_2=ai.y, ai_3=ai.z;
							const float bi_1=bi.x, bi_2=bi.y, bi_3=bi.z;
							const float ci_1=ci.x, ci_2=ci.y, ci_3=ci.z;
							const float aj_1=aj.x, aj_2=aj.y, aj_3=aj.z;
							const float bj_1=bj.x, bj_2=bj.y, bj_3=bj.z;
							const float cj_1=cj.x, cj_2=cj.y, cj_3=cj.z;
							const float beta_i_1 = previous_bar_coord.y, beta_i_2 = previous_bar_coord.z; // .x = 1-y-z, skip
							const float beta_j_1 =      ray.bar_coord.y, beta_j_2 =      ray.bar_coord.z; // .x = 1-y-z, skip
							#include "symGradGeomTerm3D.h"
							
							
							#if CHECK_NAN_INF == 1
							if (any(isnan(dgdai)) || any(isinf(dgdai))) dgdai = vec3(0.0f);
							if (any(isnan(dgdbi)) || any(isinf(dgdbi))) dgdbi = vec3(0.0f);	
							
							if (any(isnan(dgdci)) || any(isinf(dgdci))) dgdci = vec3(0.0f);	
							if (any(isnan(dgdaj)) || any(isinf(dgdaj))) dgdaj = vec3(0.0f);

							if (any(isnan(dgdbj)) || any(isinf(dgdbj))) dgdbj = vec3(0.0f);
							if (any(isnan(dgdcj)) || any(isinf(dgdcj))) dgdcj = vec3(0.0f);
							#endif
						}
						
						if( abs(geomTerm) > tinyEps*tinyEps ){ // prevent div-by-zero
							//if( any(isnan(ray.weight/geomTerm*dgdai+ray.weight/geomTerm*dgdbi+ray.weight/geomTerm*dgdci+ray.weight/geomTerm*dgdaj+ray.weight/geomTerm*dgdbj+ray.weight/geomTerm*dgdcj)) || 
							//	any(isinf(ray.weight/geomTerm*dgdai+ray.weight/geomTerm*dgdbi+ray.weight/geomTerm*dgdci+ray.weight/geomTerm*dgdaj+ray.weight/geomTerm*dgdbj+ray.weight/geomTerm*dgdcj)) )
							//	debugPrintfEXT("cont. derivs. Nan/Inf");

							// integrate timesteps ...
							sumOverHistory( previous_vertex_indices.x, adjAbsorbedVertexIdx, sourceVertexIdx, ray.weight/geomTerm*dgdai );
							sumOverHistory( previous_vertex_indices.y, adjAbsorbedVertexIdx, sourceVertexIdx, ray.weight/geomTerm*dgdbi );
							sumOverHistory( previous_vertex_indices.z, adjAbsorbedVertexIdx, sourceVertexIdx, ray.weight/geomTerm*dgdci );

							sumOverHistory( ray_vertex_indices.x, adjAbsorbedVertexIdx, sourceVertexIdx, ray.weight/geomTerm*dgdaj );
							sumOverHistory( ray_vertex_indices.y, adjAbsorbedVertexIdx, sourceVertexIdx, ray.weight/geomTerm*dgdbj );
							sumOverHistory( ray_vertex_indices.z, adjAbsorbedVertexIdx, sourceVertexIdx, ray.weight/geomTerm*dgdcj );
						}
						//else debugPrintfEXT("cont. derivs. reject geomTerm-->0");
					}
					//ToDo: there is still some issue with the continuous part for paths with reflections ...


					//discontinuous edges: ToDo: this is still kind of hand-wavy, try to formulate it clearly ...
					dEdgeIrradiance = vec3(0.0);
					isEdge = false;
					uint oppositeCorner = findMinIdx(ray.bar_coord); // min. bary. coord. indicates the corner of the triangle barycentrically furthest from the hit location
					if( /*false&&*/ /*depth==1&&*/ ray.bar_coord[oppositeCorner] <= ubo.gradEdgeRatio ){
						// barycentric coords within x% of edge, compute approx. edge irradiance due to current ray
						isEdge = true;
						uint nearestCorner = findMaxIdx(ray.bar_coord);

						uint otherEdgeCorner = 0;
						if( oppositeCorner==0 || nearestCorner==0 ){
							otherEdgeCorner=1; // otherEdgeCorner can't be 0 if one of the already known ones is
							if( oppositeCorner==1 || nearestCorner==1 ){
								otherEdgeCorner=2; // otherEdgeCorner can't be 1 if one of the already known ones is
							}
						}
						
						edgeVertex = ray_vertex_indices[nearestCorner];
						vec3 edgeDir      = vec3( hit_instance.model_matrix * (vertex_buffer[edgeVertex].position - vertex_buffer[ray_vertex_indices[otherEdgeCorner]].position ));
						vec3 oppositeEdge = vec3( hit_instance.model_matrix * (vertex_buffer[edgeVertex].position - vertex_buffer[ray_vertex_indices[oppositeCorner ]].position ));
						float edgeLength = length(edgeDir);
						edgeDir /= edgeLength;
						vec3 n_tri = getNormal(ray_vertex_indices); //= normalize(cross( edgeDir, oppositeEdge )); // normal to hit triangle
						vec3 n_edg = normalize(cross(edgeDir, n_tri)); // in-plane edge normal

						float triHeight = dot( oppositeEdge, n_edg );
						if( triHeight < 0.0 ){ // flip sign such that n_edg points out of the triangle
							triHeight = -triHeight;
							n_edg = -n_edg;
						}

						SCALAR localArea = ubo.gradEdgeRatio*triHeight * (1.0f-0.5f*ubo.gradEdgeRatio)*edgeLength; // area of the trapezoid formed by offsetting the edge inward by edgeRatio (the mean length of the trapezoid is 1/2*(edgeLength + (1-opt.edgeRatio)*edgeLength)
						vec3 da = normalize( cross(edgeDir, ray.direction) );
						if( dot(da,n_edg)<0 ) da=-da; // orient outwards relative to edge normal
						dEdgeIrradiance = da*abs(dot(da,n_edg))*edgeLength * ray.weight / localArea;

						#if CHECK_NAN_INF == 1
						if (any(isnan(dEdgeIrradiance)) || any(isinf(dEdgeIrradiance))) dEdgeIrradiance = vec3(0.0f);
						#endif

						// check if edge is a silhouette edge wrt. to current ray, and if it blocks an object further back along the ray
						vec4 edg_v = hit_instance.model_matrix * vertex_buffer[edgeVertex].position;
						vec4 test_origin = hit_instance.model_matrix * (
								vertex_buffer[ray_vertex_indices.x].position * ray.bar_coord.x + 
								vertex_buffer[ray_vertex_indices.y].position * ray.bar_coord.y +
								vertex_buffer[ray_vertex_indices.z].position * ray.bar_coord.z );
						test_origin.xyz += n_edg * (tinyEps+dot(edg_v.xyz-test_origin.xyz,n_edg));
						test_origin.xyz -= tinyEps*ray.direction; // move a tiny bit back too

						//if( gl_LaunchIDEXT.x <= 20 && gl_LaunchIDEXT.y == 1){
						//	vec4 src = previous_instance.model_matrix * (
						//		vertex_buffer[previous_vertex_indices.x].position * previous_bar_coord.x + 
						//		vertex_buffer[previous_vertex_indices.y].position * previous_bar_coord.y +
						//		vertex_buffer[previous_vertex_indices.z].position * previous_bar_coord.z );
						//	vec4 hit = hit_instance.model_matrix * (
						//		vertex_buffer[ray_vertex_indices.x].position * ray.bar_coord.x + 
						//		vertex_buffer[ray_vertex_indices.y].position * ray.bar_coord.y +
						//		vertex_buffer[ray_vertex_indices.z].position * ray.bar_coord.z );
						//	//debugPrintfEXT("src(%.3g %.3g %.3g)->hit(%.3g %.3g %.3g)->edg(%.3g %.3g %.3g)", src.x,src.y,src.z, hit.x,hit.y,hit.z, test_origin.x,test_origin.y,test_origin.z);
						//	//debugPrintfEXT("src=[%.3g %.3g %.3g]; hit=[%.3g %.3g %.3g]; n_e=[%.3g %.3g %.3g];%%dI(%.3g %.3g %.3g)",src.x,src.y,src.z, hit.x,hit.y,hit.z, n_edg.x,n_edg.y,n_edg.z, dEdgeIrradiance.x,dEdgeIrradiance.y,dEdgeIrradiance.z);
						//	debugPrintfEXT("src=[%.3g %.3g %.3g]; hit=[%.3g %.3g %.3g]; n_e=[%.3g %.3g %.3g]; edg_v=[%.3g %.3g %.3g]; tst=[%.3g %.3g %.3g];",
						//		src.x,src.y,src.z, hit.x,hit.y,hit.z, n_edg.x,n_edg.y,n_edg.z, edg_v.x,edg_v.y,edg_v.z,test_origin.x,test_origin.y,test_origin.z);
						//}

						isNeighbour=false;
						RayPayload rp_copy = rp; // restore later
						uint originVertex = previous_vertex_indices[ findMaxIdx( previous_bar_coord ) ];
						//ToDo: !!! handle also reflection vs occlusion -- i.e. if ray is not absorbed (check adjAbsorbedVertexIdx) at current (occluding) element, consider the jump in reflection also --> adjAbsorbedVertexIdx vs occluded vertex etc (... eventually with mirror constraints??)
						{	// occlusion-absorption case
							rp.instanceID = -1;
							traceRayEXT(topLevelAS, gl_RayFlagsNoneEXT, 0xf0, 0, 0, 0, test_origin.xyz, ray.t_min, ray.direction, ray.t_max, 0);
							//if( gl_LaunchIDEXT.x <= 20 && gl_LaunchIDEXT.y == 1) debugPrintfEXT("shdw[%d]",rp.instanceID);
							if( rp.instanceID >= 0 && rp.hit_distance >= tinyDist){ // hit something (and not too close) ...
								GeometrySSBO test_geometry_data = geometry_buffer[instance_buffer[rp.instanceID].geometry_buffer_offset + rp.geometryIndex];
								uint test_vertex_offset = rp.primitiveID * 3;
								uvec4 test_vertex_indices = uvec4(0, 1, 2, 0) + test_vertex_offset;
								if( test_geometry_data.has_indices ){
									for(uint ii=0; ii<3; ii++){
										test_vertex_indices[ii] = index_buffer[test_geometry_data.index_buffer_offset + test_vertex_offset + ii];
									}
								}
								test_vertex_indices.xyz += test_geometry_data.vertex_buffer_offset;
								test_vertex_indices.w = rp.instanceID;
								for(uint ii=0; ii<3 && !isNeighbour; ii++) for(uint jj=0; jj<3 && !isNeighbour; jj++){
									if( test_vertex_indices[ii] == ray_vertex_indices[jj] ) isNeighbour=true; // have at least one shared node with originally hit triangle
								}
								vec3 test_normal = getNormal(test_vertex_indices);
								//if( gl_LaunchIDEXT.x <= 20 && gl_LaunchIDEXT.y == 1) debugPrintfEXT("src %d -> edgv %d, nb %d, n_e(%.3g %.3g %.3g)",sourceVertexIdx, edgeVertex, isNeighbour, n_edg[0], n_edg[1], n_edg[2]);
								//if( !isNeighbour && dot(test_normal, ray.direction) < -tinyDist){ //ignore (near) backface hits (valid hit must have dot(r,n) sufficiently negative
								if( !isNeighbour && dot(test_normal, ray.direction) < -1e-3){ //ignore (near) backface hits (valid hit must have dot(r,n) sufficiently negative
									// silhouette edge --> account for loss of irradiance on occluded object if edge moves along in-plane outward edge normal
									// and conversely gain of irradiance if occluded object AND ray source move along the same direction instead
									uint shadowVertex = test_vertex_indices[ findMaxIdx( computeBarycentric(rp.hitAttribute) ) ];
									float shadowAbsorbtivity = 1.0f - instance_buffer[test_vertex_indices.w].diffuseReflectance - instance_buffer[test_vertex_indices.w].specularReflectance;

									const SCALAR distSrcToEdge   = rp_copy.hit_distance;
									const SCALAR distEdgToShadow =      rp.hit_distance;
									const SCALAR totalDistance   = distSrcToEdge + distEdgToShadow;
									//const float distSrcToEdge=1.0, distEdgToShadow=1.0, totalDistance=1.0; // disable for testing (clearly worse in GD opt. on test0_1_asym compared to standard version above)
									//const float distSrcToEdge=1.0, distEdgToShadow=1.0, totalDistance=2.0; // half/half for testing ... also worse in FD-range check on test0_1_asym compared to standard version above
									////if( gl_LaunchIDEXT.x <= 50 && gl_LaunchIDEXT.y == 1) debugPrintfEXT("src %d -> edg %d -> shd %d: di=[%.3g; %.3g; %.3g]", sourceVertexIdx, edgeVertex, shadowVertex, dEdgeIrradiance[0], dEdgeIrradiance[1], dEdgeIrradiance[2]);

									sumOverHistory( shadowVertex, shadowVertex, sourceVertexIdx, shadowAbsorbtivity *(distSrcToEdge  /totalDistance)  * dEdgeIrradiance );
									sumOverHistory( originVertex, shadowVertex, sourceVertexIdx, shadowAbsorbtivity *(distEdgToShadow/totalDistance)  * dEdgeIrradiance );
									sumOverHistory( edgeVertex,   shadowVertex, sourceVertexIdx, shadowAbsorbtivity *                                  -dEdgeIrradiance );
								}
							}
						}
						//if(!isNeighbour){ //Note: post-EG25-submission experiments .... change for source to original absorption point (reduced reflection if edge moves away -- sort of should be handled in the continuous term already)
						//	sumOverHistory( edgeVertex,   adjAbsorbedVertexIdx, sourceVertexIdx, -dEdgeIrradiance ); //ToDo: sign could be wrong - more testing
						//	sumOverHistory( originVertex, adjAbsorbedVertexIdx, sourceVertexIdx,  dEdgeIrradiance );
						//}
						rp = rp_copy; // restore to hit data from original ray, just in case we need it later
					}

					// move to next segment when done
					previous_vertex_indices = ray_vertex_indices;
					previous_bar_coord = ray.bar_coord;
					previous_instance = hit_instance;
				}
#endif
			//seed += n + depth;
			ray = getNextRay(ray_vertex_indices, ray.bar_coord, ray.direction, ray.weight, seed);

			if( ray.absorbed || depth == maxDepth) { // absorb and terminate trace (force absorb on max depth)
				uint absorbedVertexIdx = ray_vertex_indices[ findMaxIdx(ray.bar_coord) ];
				uint ij_idx = linFrom2D(sourceVertexIdx, absorbedVertexIdx,  ubo.vertexCount); //Note: order must be source vertex, then absorbed vertex (storage order) - test with non-traceable sources (which cause asymmetric transport)

#if TRACING_MODE == 1 // forward
				atomicAdd(transport_buffer[ij_idx], -ray.weight); // negative weight here, because we build -T so the result is positive definite
#elif TRACING_MODE == 2 // adjoint
				// now that the path is complete (from source to absorption), store / check absorbed vertex for second pass
				if( adjPass==0 )  adjAbsorbedVertexIdx  = absorbedVertexIdx;
				if( adjPass==1 && adjAbsorbedVertexIdx != absorbedVertexIdx ) debugPrintfEXT("adj absorbed vertex mismatch %d <> %d", adjAbsorbedVertexIdx, absorbedVertexIdx);
				if( adjPass==1 ){
					sumOverHistory( source_vertex_indices.x, adjAbsorbedVertexIdx, sourceVertexIdx, dPhotonPower_dv0 ); // works in test0_1bs
					sumOverHistory( source_vertex_indices.y, adjAbsorbedVertexIdx, sourceVertexIdx, dPhotonPower_dv1 );
					sumOverHistory( source_vertex_indices.z, adjAbsorbedVertexIdx, sourceVertexIdx, dPhotonPower_dv2 );
				}
#endif
				break;
			}
		}
#if TRACING_MODE == 2 // adjoint
		}
#endif
	}
}

