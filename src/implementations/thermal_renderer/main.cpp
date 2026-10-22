#if !defined(DISABLE_GUI) && !defined(ENABLE_CLI)

#include "thermal_renderer.hpp"
#include "thermal_renderer_backend.hpp"

#include <tamashii/tamashii.hpp>
#include <tamashii/engine/common/vars.hpp>
#include <tamashii/engine/common/common.hpp>
#include <tamashii/engine/common/input.hpp>

#include "spdlog/spdlog.h"

int main(int argc, char* argv[]) {

	//spdlog::set_level(spdlog::level::level_enum::debug);

	// use our vulkan backend
	tamashii::addBackend(new VulkanThermalRenderBackend());
	var::default_implementation.setValue(THERMAL_RENDERER_NAME);
	tamashii::run(argc, argv);
}

#elif defined(ENABLE_LIB_TEST) // !DISABLE_GUI

#include "thermal_api.hpp"

int main(int argc, char* argv[]) {
	load(true);
	unload();
	return 0;
}

#elif defined(ENABLE_CLI) // ENABLE_LIB_TEST

//#include "thermal_api.hpp"

#include "spdlog/spdlog.h"

#include <tamashii/tamashii.hpp>
#include <tamashii/engine/common/vars.hpp>
#include <tamashii/engine/common/common.hpp>
#include "thermal_renderer.hpp"
#include "thermal_renderer_backend.hpp"
#include "thermal_export.hpp"
#include <tamashii/engine/common/input.hpp>
#include <tamashii/engine/platform/filewatcher.hpp>

#include <cassert>
#include <ctime>
#include <iostream>
#include <utility>

#include <chrono>

#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <glm/gtx/rotate_vector.hpp>

#include <CLI/CLI.hpp>
#include "gpl/solar_position.h"
#include <stack>

#ifdef WIN32
// source: https://stackoverflow.com/questions/321849/strptime-equivalent-on-windows
extern "C" char* strptime(const char* s, const char* f,	struct tm* tm) {
	// strptime is POSIX-only, so reimplement it here via std::get_time.
	std::istringstream input(s);
	input.imbue(std::locale(setlocale(LC_ALL, nullptr)));
	input >> std::get_time(tm, f);
	if (input.fail()) {
		return nullptr;
	}
	return (char*)(s + input.tellg());
}
#endif // WIN32

static std::pair<struct tm, int> parse_time(const std::string& time_string) {
	int tz;
	struct tm tv;
	char* end = strptime(time_string.c_str(), "%Y-%m-%dT%H:%M:%S", &tv);
	struct tm tv2;
	assert(end != nullptr);
	if (*end == '-' || *end == '+') {
		end = strptime(end + 1, "%H:%M", &tv2);
		assert(end != nullptr);
		tz = (*end == '-') ? -tv2.tm_hour : tv2.tm_hour;
	}
	return std::make_pair(tv, tz);
}

int load_sun(ThermalRenderer* lib_impl, RenderScene* scene, float _lat, float _long, tm _tv, int _tz)
{
	//RenderScene* scene = Common::getInstance().getRenderSystem()->getMainScene();

	Model* model = Model::alloc();
	Material* mat = Material::alloc();
	Mesh* mesh = Mesh::alloc();

	glm::vec4 bs = lib_impl->getBoundingSphere();
	glm::vec3 sphere_center = glm::vec3(bs.x, bs.y, bs.z);
	float sphere_radius = bs.w;

	float zenith_angle = sunAngle(_lat, _long, _tz, _tv);
	glm::vec3 sun_vector_base = glm::vec3(0, 0, -1);
	glm::vec3 sun_vector = glm::rotateX(sun_vector_base, zenith_angle);
	glm::vec3 sun_center = sphere_center + sun_vector * sphere_radius * 1.2f;
	float sun_flux = std::max(1362.0f / std::cos(zenith_angle), 0.0f);  // W/m^2

	glm::vec3 pv1 = glm::normalize(glm::cross(sun_vector, sun_vector_base));
	glm::vec3 pv2 = glm::normalize(glm::cross(sun_vector, pv1));

	float vertex_scaling = glm::sqrt(sphere_radius * sphere_radius + sphere_radius * sphere_radius) * 1.2;

	std::vector<vertex_s> vertices;
	vertices.reserve(4);
	{ vertex_s v; v.position = glm::vec4(sun_center + pv1 * vertex_scaling, 1.0); vertices.push_back(v); }
	{ vertex_s v; v.position = glm::vec4(sun_center + pv2 * vertex_scaling, 1.0); vertices.push_back(v); }
	{ vertex_s v; v.position = glm::vec4(sun_center - pv1 * vertex_scaling, 1.0); vertices.push_back(v); }
	{ vertex_s v; v.position = glm::vec4(sun_center - pv2 * vertex_scaling, 1.0); vertices.push_back(v); }

	std::vector<uint32_t> indices;
	indices.reserve(6);
	indices.push_back(2);
	indices.push_back(1);
	indices.push_back(0);
	indices.push_back(0);
	indices.push_back(3);
	indices.push_back(2);

	mesh->setIndices(indices);
	mesh->hasIndices(true);

	mesh->setVertices(vertices);
	mesh->hasPositions(true);
	mesh->setTopology(Mesh::Topology::TRIANGLE_LIST);

	mesh->setMaterial(mat);
	model->addMesh(mesh);

	/*
	RefModel_s* refModel = new RefModel_s();
	refModel->mask = 0x0f;
	RefMesh_s* refMesh = new RefMesh_s();
	refMesh->mesh = mesh;
	*/

	model->setName("Sun");

	std::vector<Value> valueArray;
	valueArray.reserve(1);
	valueArray.emplace_back(Value(sun_flux));
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
	model->addCustomProperty("traceable", Value(int(true)));
	model->addCustomProperty("animation-index", Value(-1));
	model->addCustomProperty("trace-bounding-cone", Value(int(false)));

	scene->addModelRef(model);

	lib_impl->sky_vertex_count = 4;
	//lib_impl->sceneLoad(scene->getSceneData());

	return 0;
}


void add_mesh(Model& model, const std::vector<vertex_s>& _vertices, const std::vector<uint32_t>& _indices)
{
	Material* mat = Material::alloc();
	Mesh* mesh = Mesh::alloc();
	mesh->setIndices(_indices);
	mesh->hasIndices(true);
	mesh->setVertices(_vertices);
	mesh->hasPositions(true);
	mesh->setTopology(Mesh::Topology::TRIANGLE_LIST);
	mesh->setMaterial(mat);
	model.addMesh(mesh);
}

template<typename T>
T parse_bytes(std::ifstream& f, const std::string& name = std::string())
{
	T value;
	f.read(reinterpret_cast<char*>(&value), sizeof(T));
	if(!name.empty())
		spdlog::debug("{}: {}", name, value);
	return value;
}

void parse_triangle_indices(std::ifstream& f, std::vector<uint32_t>& _indices, const std::string& name)
{
	std::stringstream fmt; fmt << name << " count";
	uint32_t entitiesCount = parse_bytes<uint32_t>(f, fmt.str());
	for (int e = 0; e < entitiesCount; e++)
	{
		Model* model = Model::alloc();
		fmt.clear(); fmt << name << e;
		model->setName(fmt.str());

		uint32_t surfaceCount = parse_bytes<uint32_t>(f, "Surface count");
		for (int s = 0; s < surfaceCount; s++)
		{
			uint8_t triangleCount = parse_bytes<uint8_t>(f, "Triangle count");
			_indices.resize(_indices.size() + triangleCount * 3);
			for (int t = 0; t < triangleCount; t++)
			{
				_indices.push_back(parse_bytes<uint32_t>(f));
				_indices.push_back(parse_bytes<uint32_t>(f));
				_indices.push_back(parse_bytes<uint32_t>(f));
			}
		}
	}
}

tamashii::SceneInfo_s* load_scene(const std::string& aFile)
{
	/*
	uint8 version = 1
	#INDEXED DATA FOR OBSTACLES
	uint32 entitiesCount
	foreach ENTITY
		uint32 surfacesCount
		foreach SURFACE
			uint8 trianglesCount
			foreach TRIANGLE
				uint32 index0
				uint32 index1
				uint32 index2
	#INDEXED DATA FOR SENSORS
	uint32 entitiesCount
	foreach ENTITY
		uint32 surfacesCount
		foreach SURFACE
			uint8 trianglesCount
			foreach TRIANGLE
				uint32 index0
				uint32 index1
				uint32 index2
	#POINTS DATA
	uint32 pointsCount
		#foreach POINT
		float32 x
		float32 y
		float32 z
	*/

	SceneInfo_s* scene = SceneInfo_s::alloc();
	SceneGraph* tscene = SceneGraph::alloc("AgroEco mesh scene");
	Node* rootNode = Node::alloc("root");
	Node* cameraNode = Node::alloc("camera");
	Node* geometryNode = Node::alloc("geometry");
	Node* groundNode = Node::alloc("ground");
	rootNode->addNode(cameraNode);
	rootNode->addNode(geometryNode);
	rootNode->addNode(groundNode);
	std::stack<glm::dmat4> transforms;
	transforms.push(glm::dmat4(1.0f));

	bool as_single_mesh = true;

	spdlog::info("Loading AgroEco mesh file: {}", aFile);

	std::ifstream f(aFile, std::ios::binary);
	uint8_t format = parse_bytes<uint8_t>(f, "File format");
	if (format != 1)
	{
		spdlog::error("Format {} not supported.", format);
	}
	else
	{
		if (as_single_mesh)
		{
			{
				std::vector<uint32_t> indices;
				parse_triangle_indices(f, indices, "Obstacle");
				parse_triangle_indices(f, indices, "Sensors");

				uint32_t vertexCount = parse_bytes<uint32_t>(f, "Vertex count");
				std::vector<vertex_s> vertices;
				vertices.reserve(vertexCount);
				for (int v = 0; v < vertexCount; v++)
				{
					vertex_s vertex;
					vertex.position = glm::vec4(
						parse_bytes<float>(f),
						parse_bytes<float>(f),
						parse_bytes<float>(f),
						1.0);
					vertices.push_back(vertex);
				}

				Model* model = Model::alloc();
				Material* mat = Material::alloc();
				Mesh* mesh = Mesh::alloc();

				mesh->setIndices(indices);
				mesh->hasIndices(true);

				mesh->setVertices(vertices);
				mesh->hasPositions(true);
				mesh->setTopology(Mesh::Topology::TRIANGLE_LIST);

				mesh->setMaterial(mat);
				model->addMesh(mesh);

				model->setName("SingleBigObject");

				model->addCustomProperty("kelvin", Value(0.0f));
				model->addCustomProperty("temperature-fixed", Value(int(false)));
				model->addCustomProperty("thickness", Value(1.0f));
				model->addCustomProperty("density", Value(1.0f));
				model->addCustomProperty("heat-capacity", Value(1.0f));
				model->addCustomProperty("heat-conductivity", Value(0.0f));
				model->addCustomProperty("diffuse-reflectance", Value(0.0f));
				model->addCustomProperty("specular-reflectance", Value(0.0f));
				model->addCustomProperty("diffuse-emission", Value(int(true)));
				model->addCustomProperty("traceable", Value(int(true)));

				geometryNode->setModel(model);
				scene->mModels.push_back(model);
			}
			{
				float size = 20.0;
				std::vector<vertex_s> vertices;
				vertex_s v0; v0.position = glm::vec4(size, 0, size, 1.0); vertices.push_back(v0);
				vertex_s v1; v1.position = glm::vec4(-size, 0, size, 1.0); vertices.push_back(v1);
				vertex_s v2; v2.position = glm::vec4(-size, 0, -size, 1.0); vertices.push_back(v2);
				vertex_s v3; v3.position = glm::vec4(size, 0, -size, 1.0); vertices.push_back(v3);

				std::vector<uint32_t> indices = { 2, 1, 0, 0, 3, 2 };

				Model* model = Model::alloc();
				Material* mat = Material::alloc();
				Mesh* mesh = Mesh::alloc();

				mesh->setIndices(indices);
				mesh->hasIndices(true);

				mesh->setVertices(vertices);
				mesh->hasPositions(true);
				mesh->setTopology(Mesh::Topology::TRIANGLE_LIST);

				mesh->setMaterial(mat);
				model->addMesh(mesh);

				model->setName("Ground");

				model->addCustomProperty("kelvin", Value(0.0f));
				model->addCustomProperty("temperature-fixed", Value(int(false)));
				model->addCustomProperty("thickness", Value(1.0f));
				model->addCustomProperty("density", Value(1.0f));
				model->addCustomProperty("heat-capacity", Value(1.0f));
				model->addCustomProperty("heat-conductivity", Value(0.0f));
				model->addCustomProperty("diffuse-reflectance", Value(0.0f));
				model->addCustomProperty("specular-reflectance", Value(0.0f));
				model->addCustomProperty("diffuse-emission", Value(int(true)));
				model->addCustomProperty("traceable", Value(int(true)));

				groundNode->setModel(model);
				scene->mModels.push_back(model);
			}
		}
		else
		{
			// OBSTACLES
			uint32_t entitiesCount = parse_bytes<uint32_t>(f, "Obstacle count");
			for (int e = 0; e < entitiesCount; e++)
			{
				Model* model = Model::alloc();
				std::stringstream fmt; fmt << "Obstacle" << e;
				model->setName(fmt.str());

				uint32_t surfaceCount = parse_bytes<uint32_t>(f, "Surface count");
				for (int s = 0; s < surfaceCount; s++)
				{
					uint8_t triangleCount = parse_bytes<uint8_t>(f, "Triangle count");
					std::vector<uint32_t> indices;
					indices.reserve(triangleCount * 3);
					for (int t = 0; t < triangleCount; t++)
					{
						indices.push_back(parse_bytes<uint32_t>(f));
						indices.push_back(parse_bytes<uint32_t>(f));
						indices.push_back(parse_bytes<uint32_t>(f));
					}
					for (int v = 0; v < indices.size(); v += 3)
						spdlog::debug("Indices: {}, {}, {}", indices[v], indices[v + 1], indices[v + 2]);
				}
			}
		}		
	}
	f.close();

	tscene->addRootNode(rootNode);
	scene->mSceneGraphs.push_back(tscene);
	return scene;
}

int main(int argc, char* argv[]) {

	// debug args: --gltf_path assets/scenes/ecosys/t700-thermal.gltf --lat 48 --long 16 --rays-per-triangle 128 --time 2022-07-12T14:58:48 --verbose --gui
	bool arg_debug_override = true;

	using std::chrono::high_resolution_clock;
	using std::chrono::duration_cast;
	using std::chrono::duration;
	using std::chrono::milliseconds;

	auto t1 = high_resolution_clock::now();
		
	CLI::App app{ "Thermal Renderer CLI" };

	std::string gltf_path;
	app.add_option("--gltf_path", gltf_path, "Path to GLTF")->required();

	double latitude = 0.0f;
	double longitude = 0.0f;
	app.add_option("--lat", latitude, "Latitude")->required();
	app.add_option("--long", longitude, "Longitude")->required();

	// should we change this to an abstract quality setting? 
	size_t rays_per_triangle = 0;
	app.add_option("--rays-per-triangle", rays_per_triangle,
		"Number of rays to cast from each triangle in the mesh")
		->default_val(1);
	assert(rays_per_triangle != 0);

	std::string time_string;
	CLI::Option* time_option = app.add_option(
		"--time", time_string,
		//"Time of day - should be in %Y-%m-%dT%H:%M%:S[-/+%H:%M] format");
		"Time of day - should be in %Y-%m-%dT%H:%M:%S format");

	double timestep = 0.0f;
	app.add_option("--timestep", timestep, "Timestep (h)");

	bool debug = false;
	app.add_flag("--verbose", debug, "Be verbose.");

	bool gui = false;
	app.add_flag("--gui", gui, "show GUI");

	if (arg_debug_override)
	{
		gltf_path = "assets/scenes/ecosys/t900.mesh";
		latitude = 48.0;
		longitude = 16.0;
		rays_per_triangle = 1280.0;
		time_string = "2022-07-12T14:58:48";
		debug = false;
		gui = true;
	}
	else
	{
		CLI11_PARSE(app, argc, argv);
	}

	struct tm tv = {};
	tv.tm_mon = 1;
	tv.tm_mday = 1;
	tv.tm_hour = 12;
	int tz = 0;
	if (time_option->count() > 0) {
		std::tie(tv, tz) = parse_time(time_string);
	}

	if (debug) {
		spdlog::set_level(spdlog::level::level_enum::debug);
		std::cout << "loading " << gltf_path << std::endl;
		//std::cout << tv.tm_hour << ":" << tv.tm_min << std::endl;
	}
	else
	{
		spdlog::set_level(spdlog::level::level_enum::off);
	}

	// use our vulkan backend
	if(!gui)
		tamashii::var::headless.setValue("1");

	tamashii::addBackend(new VulkanThermalRenderBackend());
	var::default_implementation.setValue(THERMAL_RENDERER_NAME);

	tamashii::Importer::instance().add_load_scene_format("AgroEco Mesh", { "*.mesh" }, &load_scene);

	tamashii::init(0, NULL);

	ThermalRenderer* lib_impl = static_cast<ThermalRenderer*>(tamashii::findBackendImplementation(THERMAL_RENDERER_NAME));
	lib_impl->setRayBatchCount(rays_per_triangle, 1);
	lib_impl->temporaryDisableTransportCompute();

	tamashii::EventSystem& es = tamashii::EventSystem::getInstance();
	es.setCallback(tamashii::EventType::ACTION, tamashii::Input::A_OPEN_SCENE, [=](const tamashii::Event& aEvent)
	{
		tamashii::RenderSystem* mRenderSystem = tamashii::Common::getInstance().getRenderSystem();
		// remove old scene from file watcher
		tamashii::FileWatcher::getInstance().removeFile(mRenderSystem->getMainScene()->getSceneFileName());
		// delete old scene
		mRenderSystem->getMainScene()->readyToRender(false);
		mRenderSystem->sceneUnload(mRenderSystem->getMainScene()->getSceneData());
		mRenderSystem->freeRenderScene(mRenderSystem->getMainScene());
		// unselect object before switching
		mRenderSystem->getMainScene()->setSelection(nullptr);

		// load new scene
		tamashii::RenderScene* scene = mRenderSystem->allocRenderScene();
		tamashii::SceneInfo_s* aSceneInfo = tamashii::Importer::instance().load_scene(aEvent.getMessage());
		mRenderSystem->setMainRenderScene(scene);
		// add custom stuff to aSceneInfo before scene creation
		if (scene->initFromData(aSceneInfo)) {
			// let backend load the data
			mRenderSystem->sceneLoad(scene->getSceneData());
		}
		// frame
		ThermalRenderer* lib_impl = static_cast<ThermalRenderer*>(tamashii::findBackendImplementation(THERMAL_RENDERER_NAME));
		load_sun(lib_impl, scene, latitude, longitude, tv, tz);
		
		mRenderSystem->sceneUnload(mRenderSystem->getMainScene()->getSceneData());
		mRenderSystem->sceneLoad(scene->getSceneData());
		mRenderSystem->setMainRenderScene(scene);
		scene->readyToRender(true);

		return true;
	});

	EventSystem::queueEvent(EventType::ACTION, Input::A_OPEN_SCENE, 0, 0, 0, gltf_path);

	if (gui)
	{
		tamashii::whileFrame();
	}
	else
	{
		EventSystem::getInstance().eventLoop();

		ThermalRenderer* lib_impl = static_cast<ThermalRenderer*>(tamashii::findBackendImplementation(THERMAL_RENDERER_NAME));
		lib_impl->setSolverMode(1);
		if(timestep > 0)
			lib_impl->setTimeStep(timestep);
		else
			lib_impl->setSteadState(true);
		lib_impl->thermalTimestep();
		export_to_csv_object_avg(lib_impl->getThermalStatsArray());

		Common::Common::getInstance().shutdown();
	}

	auto t2 = high_resolution_clock::now();

	/* Getting number of milliseconds as an integer. */
	auto ms_int = duration_cast<milliseconds>(t2 - t1);

	/* Getting number of milliseconds as a double. */
	duration<double, std::milli> ms_double = t2 - t1;

	std::cout << "Dur.: " << ms_int.count() / 1000.0 << " sec.\n";
}

#endif // !ENABLE_CLI