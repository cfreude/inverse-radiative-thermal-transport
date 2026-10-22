#pragma once
#include <tamashii/public.hpp>

T_BEGIN_NAMESPACE
class Mesh;
namespace topology {

	void calcNormals(Mesh* aMesh);
	void calcFlatNormals(Mesh* aMesh);
	void calcSmoothNormals(Mesh* aMesh);

	// http://www.mikktspace.com/
	void calcMikkTSpaceTangents(Mesh* aMesh);

	// Utility function to get a vector perpendicular to an input vector 
	// (from "Efficient Construction of Perpendicular Vectors Without Branching")
	// https://blog.selfshadow.com/2011/10/17/perp-vectors/
	// v should be normalized
	glm::vec4 calcStarkTangent(glm::vec3 aNormal);
	void calcStarkTangent(Mesh* aMesh);
}
T_END_NAMESPACE
