#pragma once
#include <tamashii/public.hpp>
#include <tamashii/engine/scene/scene_graph.hpp>

#include <list>

T_BEGIN_NAMESPACE
class Model;
class Mesh;
class Light;
class Camera;
struct Ref_s {
	enum class Type {
		Mesh, Model, Camera, Light
	};
											
								Ref_s(const Type type) : type(type), model_matrix(glm::mat4(1.0f)), extra(nullptr) {}
								~Ref_s() = default;
	const Type					type;
	glm::mat4					model_matrix;

	std::deque<TRS*>			transforms;			// all the transforms for this ref
	void*						extra;				// custom data, not maintained by the engine
    const Ref_s&				operator=(const Ref_s& other) { return other; }

	void						updateSceneGraphNodesFromModelMatrix(bool aFlipY = false) const;
};
struct RefMesh_s : Ref_s {
								RefMesh_s() : Ref_s(Type::Mesh), mesh(nullptr) {}

	Mesh*						mesh;
};
struct RefModel_s : Ref_s {
								RefModel_s() : Ref_s(Type::Model), model(nullptr), ref_model_index(-1), mask(0xffffffff), animated(false) {}
								~RefModel_s()
								{
									for (const RefMesh_s* rm : refMeshes) delete rm;
									refMeshes.clear();
								}
	Model*						model;
	std::list<RefMesh_s*>		refMeshes;
	int							ref_model_index;	// unique identifier for this reference
	uint32_t					mask;
	bool						animated;
};

struct RefCamera_s : Ref_s {
	enum class Mode {
		STATIC, FPS, EDITOR
	};
	Camera*						camera;

	Mode						mode;
	glm::mat4					view_matrix;
	bool						y_flipped;			// y flipped in view and model matrix
	glm::vec3					getPosition();
	glm::vec3					getDirection();

	int							ref_camera_index;	// unique identifier for this reference
	bool						animated;
protected:
	RefCamera_s() : Ref_s(Type::Camera), camera(nullptr), mode(Mode::STATIC), view_matrix(1), y_flipped(false), ref_camera_index(-1), animated(false) {}
	RefCamera_s(Camera* camera, int ref_camera_index) : Ref_s(Type::Camera), camera(camera), mode(Mode::STATIC), view_matrix(1), y_flipped(false), ref_camera_index(ref_camera_index), animated(false) {}
};
struct RefCameraPrivate_s : RefCamera_s {
								RefCameraPrivate_s();
	struct Angles {
		float					pitch;
		float					yaw;
		float					roll;
	};
	struct FpsData
	{
								// pointer movement
		const float				mouse_sensitivity = 5.0f;
		const float				yaw_sensitivity = 0.022f;
		const float				pitch_sensitivity = 0.022f;
								// mouse wheel 
		const float				mouse_wheel_sensitivity = 10.0f;
								// wasdqe movement
		const float				movement_speed = 650.0f;
		const float				acceleration = 10.0f;

								// smooth mouse over multiple frames
		const size_t			smooth_history_usage = 1.0f;
		size_t					smooth_history_count = 0;
		float					smooth_history[16][2] = {};

								// state
		float					current_movement_speed = 100;
		glm::vec3				current_velocity = {0,0,0};
	};
	struct EditorData
	{							// settings
		float					zoom_speed = 0.2f;

								// state
								// the point around this camera rotates
		glm::vec3				pivot = { 0, 0, 0 };
								// distance from pivot
		float					zoom = 40.0f;
	};
	bool						default_camera;
	FpsData						fps_data;
	EditorData					editor_data;

	glm::vec3					axis[3];	// { right, up, forward }
	Angles						angles;		// in degree (look in -z direction)
	glm::vec3					position;

								// fps
								// update angles with relative mouse coordinates
	bool						updateAngles(float aNewMx, float aNewMy);
								// update position (e.g. wasdqe)
	bool						updatePosition(bool aFront, bool aBack, bool aLeft, bool aRight, bool aUp, bool aDown);
								// update movement speed of the camera with the mouse wheel
	bool						updateSpeed(float aNewMz);

								// editor
	bool						drag(glm::vec2 aDelta);
	bool						updateCenter(glm::vec2 aDelta);
	bool						updateZoom(float aNewMz);

								// setup this camera with a model matrx (this correctly sets axis etc)
	void						setModelMatrix(glm::mat4& aMatrix, bool aFlipY = false);
	void						updateMatrix(bool aFlipY = false);
};

struct RefLight_s : Ref_s {
								RefLight_s() : Ref_s(Type::Light), light(nullptr), ref_light_index(-1), animated(false), direction(glm::vec3(1.0f)), position(glm::vec3(1.0f)) {}
	Light*						light;
	int							ref_light_index;	// unique identifier for this reference
	bool						animated;

	glm::vec3					direction;			// not needed for point light
	glm::vec3					position;			// not needed for directional light
};

// http://www.lighthouse3d.com/tutorials/view-frustum-culling/geometric-approach-implementation/
struct Frustum
{
	enum class Plane : uint8_t { N = 0, F = 1, T = 2, B = 3, L = 4, R = 5, COUNT = 6 };
	struct Plane_s { glm::vec3 n; glm::vec3 p; };
	Plane_s plane[static_cast<uint8_t>(Plane::COUNT)]{};
	glm::vec3 origin{};
	glm::vec3 near_corners[4]{};
	glm::vec3 far_corners[4]{};
	Frustum(const RefCamera_s *c);
	bool checkPointInside(const glm::vec3 p) const;
	bool checkAABBInside(const glm::vec3 min, const glm::vec3 max) const;
};
T_END_NAMESPACE
