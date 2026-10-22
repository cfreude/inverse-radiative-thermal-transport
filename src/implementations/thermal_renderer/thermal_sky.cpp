#include "thermal_sky.hpp"
#include <thermal_renderer_backend.hpp>
#include "spdlog/fmt/ostr.h"

using namespace std;
using namespace glm;

void ThermalSky::loadGeometry(RenderScene * const _scene)
{
	auto sph2cart_Yup = [](float _r, float _incl, float _azi) {
		float sin_incl = glm::sin(_incl);
		float cos_incl = glm::cos(_incl);
		float sin_azi = glm::sin(_azi);
		float cos_azi = glm::cos(_azi);
		return vec3(
			_r * sin_incl * cos_azi,
			_r * cos_incl,
			_r* sin_incl* sin_azi);
		};

	vec3 min_aabb, max_aabb;
	std::tie(min_aabb, max_aabb) = ThermalRenderer::computeSceneAABB(_scene->getSceneData());
	vec3 boundingSphereCenter;
	float boundingSphereRadius;
	std::tie(boundingSphereCenter, boundingSphereRadius) = ThermalRenderer::computeSceneBoundingSphere(min_aabb, max_aabb);

	Model* model = Model::alloc();
	Material* mat = Material::alloc();
	Mesh* mesh = Mesh::alloc();

	size_t vertex_count = SkyMatrix::NUM_SKY_VALUES * 4;
	vector<vertex_s> vertices;
	vertices.reserve(vertex_count);

	size_t triangle_indices_count = SkyMatrix::NUM_SKY_VALUES * 2 * 3;
	std::vector<uint32_t> indices;
	indices.reserve(triangle_indices_count);

	constexpr float deg2rad = glm::pi<float>() / 180.0;
	float quad_radius = boundingSphereRadius * 2.2;

	size_t vind = 0;

	float inclination = 0.0;
	for (int ci = 0; ci < SkyMatrix::NUM_ROWS; ci++)
	{
		int c = SkyMatrix::TREGENZA_PATCHES_PER_ROW[SkyMatrix::NUM_ROWS-1-ci];

		cout << "inclination: " << inclination << endl;
		float azi_off = 360.0f / float(c);
		cout << "azi_off: " << azi_off << endl;
		float azimuth = 0.0f;
		for (int i = 0; i < c; i++)
		{
			cout << "azimuth: " << azimuth << endl;

			vec3 qc = sph2cart_Yup(boundingSphereRadius * 2.2, inclination * deg2rad,  azimuth * deg2rad); // quad center
			
			vec3 normal = normalize(qc);
			vec3 tangent = normalize(cross(normal, vec3(0, 1, 0)));
			if (c == 1)	tangent = normalize(cross(normal, vec3(1, 0, 0)));
			vec3 bitangent = normalize(cross(normal, tangent));

			vertex_s v0, v1, v2, v3;

			v0.position = vec4(qc + quad_radius * normalize(tangent + bitangent), 1.0);
			v1.position = vec4(qc + quad_radius * normalize(tangent - bitangent), 1.0);
			v2.position = vec4(qc + quad_radius * normalize(-tangent - bitangent), 1.0);
			v3.position = vec4(qc + quad_radius * normalize(-tangent + bitangent), 1.0);

			v0.normal = v1.normal = v2.normal = v3.normal = vec4(normal, 0.0);
			v0.tangent = v1.tangent = v2.tangent = v3.tangent = vec4(tangent, 0.0);

			vertices.push_back(v0);
			vertices.push_back(v1);
			vertices.push_back(v2);
			vertices.push_back(v3);

			indices.push_back(vind);
			indices.push_back(vind+1);
			indices.push_back(vind+2);

			indices.push_back(vind);
			indices.push_back(vind+2);
			indices.push_back(vind+3);

			vind += 4;

			azimuth += azi_off;
		}
		inclination += SkyMatrix::DEGREES_PER_ROW;
	}

	mesh->setVertices(vertices);
	mesh->setIndices(indices);

	mesh->hasPositions(true);
	mesh->hasIndices(true);
	mesh->hasNormals(true);
	mesh->hasTangents(true);

	mesh->setTopology(Mesh::Topology::TRIANGLE_LIST);

	mesh->setMaterial(mat);
	model->addMesh(mesh);

	RefModel_s* refModel = new RefModel_s();
	refModel->mask = 0x0f;
	RefMesh_s* refMesh = new RefMesh_s();
	refMesh->mesh = mesh;

	model->setName("Sky");

	const float* values = mSkyMatrix.get_data(0, 24);

	std::vector<Value> valueArray;
	valueArray.reserve(SkyMatrix::NUM_SKY_VALUES);

	spdlog::debug("sky values:");
	for (int i = 0; i < SkyMatrix::NUM_SKY_VALUES; i++)
	{
		cout << values[i] << ", ";
		valueArray.push_back(Value(values[i]));
	}
	cout << endl;

	Value pvk(valueArray);

	model->addCustomProperty("sky-values", pvk);
	model->addCustomProperty("kelvin", Value(0.0f));
	model->addCustomProperty("temperature-fixed", Value(int(true)));
	model->addCustomProperty("thickness", Value(1.0f));
	model->addCustomProperty("density", Value(1.0f));
	model->addCustomProperty("heat-capacity", Value(1.0f));
	model->addCustomProperty("heat-conductivity", Value(0.0f));
	model->addCustomProperty("diffuse-reflectance", Value(0.0f));
	model->addCustomProperty("specular-reflectance", Value(0.0f));
	model->addCustomProperty("diffuse-emission", Value(int(false)));
	model->addCustomProperty("traceable", Value(int(false)));
	model->addCustomProperty("animation-index", Value(-1));
	model->addCustomProperty("trace-bounding-cone", Value(int(false)));

	_scene->addModelRef(model);

}