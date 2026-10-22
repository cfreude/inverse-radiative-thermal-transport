#include <tamashii/engine/importer/importer.hpp>
#include <tamashii/engine/scene/camera.hpp>
#include <tamashii/engine/scene/image.hpp>
#include <tamashii/engine/scene/material.hpp>
#include <tamashii/engine/scene/model.hpp>
#include <tamashii/engine/scene/scene_graph.hpp>
#include <fstream>
#include <stack>
#include <filesystem>

// TODO: Currently just a test
T_USE_NAMESPACE
namespace {

	std::string getNextWord(std::string& s) {
		std::string::size_type p1 = 0;
		std::string::size_type p2 = 0;
		const std::string delimiters_front = " \n\t\"[]";
		std::string delimiters_back = " \"]";

		p1 = s.find_first_not_of(delimiters_front, p1);
		// check if this is the start of quotation marks, then we want to ignore whitespaces
		const std::string::size_type temp = s.find_first_of('\"');
		if (temp != std::string::npos && p1 == (temp + 1)) {
			delimiters_back.erase(std::remove(delimiters_back.begin(), delimiters_back.end(), ' '), delimiters_back.end());
		}
		if (p1 == std::string::npos) return "";
		p2 = s.find_first_of(delimiters_back, p1);
		if (p2 == std::string::npos) p2 = s.size();
		std::string re = s.substr(p1, (p2 - p1));
		if (s.size() == p2) {
			s = "";
			return re;
		}
		s = s.substr(p2+1, (s.size() - p2));
		return re;
	}
}

SceneInfo_s* Importer::load_pbrt(std::string const& aFile) {
	std::ifstream f(aFile);
	std::string path = std::filesystem::path(aFile).parent_path().string();

	SceneInfo_s* scene = SceneInfo_s::alloc();
	SceneGraph* tscene = SceneGraph::alloc("pbrt scene");
	Node* rootNode = Node::alloc("root");
	Node* cameraNode = Node::alloc("camera");
	Node* geometryNode = Node::alloc("geometry");
	rootNode->addNode(cameraNode);
	rootNode->addNode(geometryNode);
	std::stack<glm::dmat4> transforms; 
	transforms.push(glm::dmat4(1.0f));

	std::string line;
	float value;
	int pos;
	// read header
	while (f.good()) {
		std::getline(f, line);
		const std::string line_type = getNextWord(line);
		if (line_type == "Transform") {
			for (int i = 0; i < 4; i++) {
				for (int j = 0; j < 4; j++) {
					transforms.top()[i][j] = std::stof(getNextWord(line));
				}
			}
			transforms.top() = glm::transpose(transforms.top());
			transforms.top()[2] *= glm::vec4(-1);
			transforms.top() = glm::transpose(transforms.top());
		}
		if (line_type == "Camera") {
			glm::dvec3 scale;
			glm::dquat rotation;
			glm::dvec3 translation;
			glm::dvec3 skew;
			glm::dvec4 perspective;
			ASSERT(glm::decompose(transforms.top(), scale, rotation, translation, skew, perspective), "decompose error");

			bool cam_type_perspective = false;
			float fov;
			std::string arg;
			while (!(arg = getNextWord(line)).empty()) {
				if (arg == "perspective") cam_type_perspective = true;
				else if (arg == "float fov") fov = std::stof(getNextWord(line));
			}

			Camera* cam = Camera::alloc();
			cam->setName("Camera");
			if(cam_type_perspective) cam->initPerspectiveCamera(glm::radians(fov), 1.0f, 0.1f, 10000.0f);

			Node* tNode = Node::alloc("node");

			tNode->setCamera(cam);
			geometryNode->setRotation(glm::vec4(rotation.x, rotation.y, rotation.z, rotation.w));
			geometryNode->setScale(glm::vec3(scale.x, scale.y, scale.z));
			geometryNode->setTranslation(glm::vec3(translation.x, translation.y, translation.z));
			cameraNode->addNode(tNode);
		}
		if (line_type == "WorldBegin") {
			transforms.pop();
			transforms.push(glm::dmat4(1.0f));
			break;
		}
	}

	Model* tmodel = nullptr;
	Material* tmaterial = nullptr;
	Light* tlight = nullptr;
	std::map<std::string, Texture*> texture_map;
	std::map<std::string, Material*> material_map;
	// read world
	bool skip = false;
	bool attribute = false;
	auto color = glm::vec4(0);
	while (f.good()) {
		std::getline(f, line);

		const std::string line_type = getNextWord(line);

		if (line_type == "TransformBegin") {
			transforms.push(glm::dmat4(1.0f));
		}
		else if (line_type == "TransformEnd") {
			transforms.pop();
		}
		else if (line_type == "Transform") {
			for (int i = 0; i < 4; i++) {
				for (int j = 0; j < 4; j++) {
					transforms.top()[i][j] = std::stof(getNextWord(line));
				}
			}
			//transforms.back()[2] *= glm::vec4(-1);
		}

		else if (line_type == "AttributeBegin") {
			transforms.push(glm::dmat4(1.0f));
			skip = true;
			attribute = true;
		}
		else if (line_type == "AttributeEnd") {
			transforms.pop();
			color = glm::vec4(0);
			skip = false;
			attribute = false;
		}
		//if (skip) continue;

		else if (line_type == "AreaLightSource") {
			/*tlight = new SurfaceLight();
			scene.lights.push_back(tlight);
			Node* Node = Node::alloc("light node");
			Node->setLight(tlight);
			tRootNode->addNode(Node);*/
			std::string arg;
			while (!(arg = getNextWord(line)).empty()) {
				if (arg == "rgb L") {
					color.x = std::stof(getNextWord(line));
					color.y = std::stof(getNextWord(line));
					color.z = std::stof(getNextWord(line));
					color.w = 1;
					//tlight->setColor(color);
				}
			}
		}

		else if (line_type == "Texture") {
			const std::string texture_name = getNextWord(line);
			getNextWord(line);
			getNextWord(line);
			const std::string texture_type = getNextWord(line);
			if (texture_type == "string filename") {
				const std::string texture_filepath = std::filesystem::path(path + "/" + getNextWord(line)).make_preferred().string();
				spdlog::info("Load Image: {}", texture_filepath);
				Image* img = load_image_8_bit(texture_filepath, 4);
				img->needsMipMaps(true);
				scene->mImages.push_back(img);

				Texture* tex = Texture::alloc();
				tex->sampler = { Sampler::Filter::LINEAR, Sampler::Filter::LINEAR, Sampler::Filter::LINEAR,
					Sampler::Wrap::REPEAT, Sampler::Wrap::REPEAT, Sampler::Wrap::REPEAT, 0, std::numeric_limits<float>::max() };
				tex->texCoordIndex = 0;
				tex->image = img;
				texture_map.insert({ texture_name, tex });
				scene->mTextures.push_back(tex);
			}
		}
		else if (line_type == "MakeNamedMaterial") {
			const std::string material_name = getNextWord(line);
			spdlog::info("Load Material: {}", material_name);

			Material* mat = Material::alloc(material_name);
			scene->mMaterials.push_back(mat);
			material_map.insert({ material_name, mat });

			std::string arg;
			while (!(arg = getNextWord(line)).empty()) {
				if (arg == "string type") {
					std::string type = getNextWord(line);
					if (type == "mirror") {
						mat->setMetallicFactor(1);
						mat->setRoughnessFactor(0);
					}
					else if (type == "metal") {
						mat->setMetallicFactor(1);
						mat->setRoughnessFactor(0.01f);
					}
					else if (type == "glass") {
						mat->setMetallicFactor(0);
						mat->setRoughnessFactor(0);
						mat->setTransmissionFactor(1);
					}
					else if (type == "uber") {
						mat->setMetallicFactor(0);
						mat->setRoughnessFactor(0.1f);
						mat->setTransmissionFactor(0);
					}
					else if (type == "matt") {
						mat->setMetallicFactor(0);
						mat->setRoughnessFactor(1);
					}
					else if (type == "substrate") {
						mat->setMetallicFactor(0);
						mat->setRoughnessFactor(1);
					}
				}
				else if (arg == "texture Kd") {
					Texture* tex = texture_map[getNextWord(line)];
					tex->image->setSRGB(true);
					mat->setBaseColorTexture(tex);
				} else if (arg == "texture opacity") {
					Texture* tex = texture_map[getNextWord(line)];
					for(uint8_t &c : tex->image->getDataVector()) c = 255 - c;
					mat->setTransmissionTexture(tex);
					mat->setTransmissionFactor(1);
				} else if (arg == "rgb Kd") {
					glm::vec4 c;
					c.x = std::stof(getNextWord(line));
					c.y = std::stof(getNextWord(line));
					c.z = std::stof(getNextWord(line));
					c.w = 1;
					mat->setBaseColorFactor(c);
				}
				else if (arg == "rgb Ks") {
					glm::vec3 c;
					c.x = std::stof(getNextWord(line));
					c.y = std::stof(getNextWord(line));
					c.z = std::stof(getNextWord(line));
					mat->setSpecularColorFactor(c);
				}
				else if (arg == "rgb Ks") {
				}
				else if (arg == "rgb opacity") {
					glm::vec3 o;
					o.x = std::stof(getNextWord(line));
					o.y = std::stof(getNextWord(line));
					o.z = std::stof(getNextWord(line));
					mat->setTransmissionFactor(1.0f - glm::compMax(o));
				}
				else if (arg == "float uroughness") {
					mat->setRoughnessFactor(std::stof(getNextWord(line)));
				}
				else if (arg == "eta") {
					mat->setIOR(std::stof(getNextWord(line)));
				}
			}
		}
		else if (line_type == "NamedMaterial") {
			tmaterial = material_map[getNextWord(line)];
			/*if (tmodel) {
				scene.models.push_back(tmodel);
				Node* Node = Node::alloc("node");
				Node->setModel(tmodel);
				tRootNode->addNode(Node);
				tmodel = nullptr;
			}

			tmodel = modelManager.alloc("");
			tmodel->setIndex(modelManager.size());
			modelManager.add(tmodel);
			tmaterial = material_map[getNextWord(line)];*/
		}
		else if (line_type == "Shape") {
			const std::string shape_type = getNextWord(line);
			if (shape_type == "plymesh") {
				const std::string mesh_type = getNextWord(line);
				if (mesh_type == "string filename") {
					const std::filesystem::path mesh_filepath = std::filesystem::path(path + "/" + getNextWord(line)).make_preferred();
					Mesh* tmesh = Importer::load_mesh(mesh_filepath.string());
					if(tmaterial) tmesh->setMaterial(tmaterial);
					tmodel = Model::alloc(mesh_filepath.filename().string());
					scene->mModels.push_back(tmodel);
					Node* tNode = Node::alloc("node");
					tNode->setModel(tmodel);
					geometryNode->addNode(tNode);

					aabb_s aabb = tmodel->getAABB();
					aabb.set(tmesh->getAABB());
					tmodel->setAABB(aabb);
					tmodel->addMesh(tmesh);
					tmodel = nullptr;
				}
			}
			else if (shape_type == "trianglemesh") {
				std::vector<uint32_t> indices;
				std::vector<float> vertices;
				std::vector<float> normals;
				std::vector<float> uvs;

				Mesh* tmesh = Mesh::alloc();
				tmesh->setTopology(Mesh::Topology::TRIANGLE_LIST);

				std::string arg;
				while (!(arg = getNextWord(line)).empty()) {
					if (arg == "integer indices") {
						tmesh->hasIndices(true);
						while (!(arg = getNextWord(line)).empty()) {
							try {
								indices.push_back(static_cast<uint32_t>(std::stof(arg)));
							}
							catch (const std::exception& e) {
								break;
							}
						}
					}
					if (arg == "point P") {
						while (!(arg = getNextWord(line)).empty()) {
							try {
								vertices.push_back({ std::stof(arg) });
							}
							catch (const std::exception& e) {
								break;
							}
						}
					}
					if (arg == "normal N") {
						tmesh->hasNormals(true);
						while (!(arg = getNextWord(line)).empty()) {
							try {
								normals.push_back({ std::stof(arg) });
							}
							catch (const std::exception& e) {
								break;
							}
						}
					}
					if (arg == "float uv") {
						tmesh->hasTexCoords0(true);
						while (!(arg = getNextWord(line)).empty()) {
							try {
								uvs.push_back({ std::stof(arg) });
							}
							catch (const std::exception& e) {
								break;
							}
						}
					}
				}
				std::vector<vertex_s> v;
				v.reserve(vertices.size() / 3.0f);

				aabb_s aabb;
				for (int i = 0; i < vertices.size() / 3.0f; i++) {
					aabb.set(reinterpret_cast<glm::vec3*>(vertices.data())[i]);
					v.push_back({ glm::vec4(reinterpret_cast<glm::vec3*>(vertices.data())[i],1),
								glm::vec4(reinterpret_cast<glm::vec3*>(normals.data())[i],0),
								glm::vec4(0),
								glm::vec2(reinterpret_cast<glm::vec2*>(uvs.data())[i])});
				}
				tmesh->setAABB(aabb);

				
				tmesh->setIndices(indices);
				tmesh->setVertices(v);

				if (tmaterial) {
					Material* mat = Material::alloc("temp");
					scene->mMaterials.push_back(mat);
					*mat = *tmaterial;
                    glm::vec3 v = color;
                    mat->setEmissionFactor(v);
					tmesh->setMaterial(mat);
				}

				tmodel = Model::alloc("");
				scene->mModels.push_back(tmodel);

				glm::dvec3 scale;
				glm::dquat rotation;
				glm::dvec3 translation;
				glm::dvec3 skew;
				glm::dvec4 perspective;
				ASSERT(glm::decompose(transforms.top(), scale, rotation, translation, skew, perspective), "decompose error");

				Node* tNode = Node::alloc("node");

				tNode->setModel(tmodel);
				tNode->setRotation(glm::vec4(rotation.x, rotation.y, rotation.z, rotation.w));
				tNode->setScale(glm::vec3(scale.x, scale.y, scale.z));
				tNode->setTranslation(glm::vec3(translation.x, translation.y, translation.z));
				geometryNode->addNode(tNode);

				aabb.set(tmesh->getAABB());
				tmodel->setAABB(aabb);
				tmodel->addMesh(tmesh);
				tmodel = nullptr;

			}
			else if (shape_type == "sphere") {
				//SurfaceLight* l = nullptr;
				//l = new SurfaceLight();
				//lightManager.add(l);
				//l->setShape(SurfaceLight::Shape::SPHERE);
				//l->setColor(color);

				//std::string arg;
				//while ((arg = getNextWord(line)).size()) {
				//	if (!arg.compare("float radius")) l->setSize(glm::vec3(std::stof(getNextWord(line))));
				//}
				//scene.lights.push_back(l);

				//glm::dvec3 scale;
				//glm::dquat rotation;
				//glm::dvec3 translation;
				//glm::dvec3 skew;
				//glm::dvec4 perspective;
				//ASSERT(glm::decompose(transforms.top(), scale, rotation, translation, skew, perspective), "decompose error");

				//Node* Node = Node::alloc("node");
				//Node->setLight(l);
				//Node->setRotation(glm::vec4(rotation.x, rotation.y, rotation.z, rotation.w));
				//Node->setScale(l->getSize());// glm::vec3(scale.x, scale.y, scale.z));
				//Node->setTranslation(glm::vec3(translation.x, translation.y, translation.z));
				//tRootNode->addNode(Node);
			}
		}

		else if (line_type == "WorldEnd") {
			if (tmodel) {
				scene->mModels.push_back(tmodel);
				Node* tNode = Node::alloc("node");
				tNode->setModel(tmodel);
				geometryNode->addNode(tNode);
				tmodel = nullptr;
			}
			break;
		}
	}
	tscene->addRootNode(rootNode);
	scene->mSceneGraphs.push_back(tscene);
	return scene;
}
