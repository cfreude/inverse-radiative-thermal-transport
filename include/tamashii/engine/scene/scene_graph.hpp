#pragma once
#include <tamashii/public.hpp>
#include <tamashii/engine/scene/asset.hpp>

#include <string>
#include <deque>
#include <vector>
#include <functional>

T_BEGIN_NAMESPACE
class Model;
class Light;
class Camera;
class Material;
class Texture;
class Image;

// translationMatrix * rotationMatrix * scaleMatrix
struct TRS {
	enum class Interpolation {
		NONE, LINEAR, STEP, CUBICSPLINE
	};
											TRS();

	glm::vec3								translation;
	glm::vec4								rotation;	// Quaternion (x, y, z, w)
	glm::vec3								scale;
											// interpolation
	Interpolation							trans_anim_inter;
	std::vector<float>						translation_time_steps;
	std::vector<glm::vec3>					translation_steps;

	Interpolation							rot_anim_inter;
	std::vector<float>						rotation_time_steps;
	std::vector<glm::vec4>					rotation_steps;

	Interpolation							scale_anim_inter;
	std::vector<float>						scale_time_steps;
	std::vector<glm::vec3>					scale_steps;

	float									time;

	bool									hasTranslation() const;
	bool									hasRotation() const;
	bool									hasScale() const;
	bool									hasTranslationAnimation() const;
	bool									hasRotationAnimation() const;
	bool									hasScaleAnimation() const;

	// time in seconds
	glm::mat4								getMatrix(float aTime);

private:
	void									computeTranslation(float const& aTime, glm::mat4& aLocalModelMatrix) const;
	void									computeRotation(float const& aTime, glm::mat4& aLocalModelMatrix);
	void									computeScale(float const& aTime, glm::mat4& aLocalModelMatrix) const;
};

class Node : public Asset {
public:
												Node(const std::string& aName);
												~Node();

	static Node*								alloc(const std::string& aName);

	// nodes
	uint32_t									numNodes() const;
	Node*										getNode(int aIndex) const;
	void										addNode(Node* aNode);

	// content
	bool										hasModel() const;
	bool										hasCamera() const;
	bool										hasLight() const;
	Model*										getModel() const;
	Camera*										getCamera() const;
	Light*										getLight() const;


	void										setModel(Model* aModel);
	void										setCamera(Camera* aCamera);
	void										setLight(Light* aLight);


	// local model matrix for this node is
	TRS*										getTRS();
	glm::mat4									getLocalModelMatrix() const;
	bool										hasAnimation() const;
	bool										hasLocalTransform() const;

	bool										hasTranslation() const;
	bool										hasRotation() const;
	bool										hasScale() const;
	bool										hasTranslationAnimation() const;
	bool										hasRotationAnimation() const;
	bool										hasScaleAnimation() const;

	void										setTranslation(glm::vec3 const& aTranslation);
    void										setRotation(glm::vec4 const& aRotation);
    void										setScale(glm::vec3 const& aScale);

	void										setTranslationAnimation(TRS::Interpolation aInterpolation, 
											std::vector<float> const& aTimeSteps, std::vector<glm::vec3> const& aTranslationSteps);
	void										setRotationAnimation(TRS::Interpolation aInterpolation, 
											std::vector<float> const& aTimeSteps, std::vector<glm::vec4> const& aRotationSteps);
	void										setScaleAnimation(TRS::Interpolation aInterpolation, 
											std::vector<float> const& aTimeSteps, std::vector<glm::vec3> const& aScaleSteps);
	void										applyFunction(const std::function<void(Node*)>& aFunction);

	// iterator
	std::vector<Node*>::iterator				begin();
	std::vector<Node*>::const_iterator			begin() const;
	std::vector<Node*>::iterator				end();
	std::vector<Node*>::const_iterator			end() const;
private:

	std::vector<Node*>							mChildren;

	Model*										mModel;
	Camera*										mCamera;
	Light*										mLight;
												
												// transform of this node
	TRS											mTrs;
};
class SceneGraph : public Asset {
public:
												SceneGraph(const std::string& aName);
												~SceneGraph();

	static SceneGraph*							alloc(const std::string& aName);

	Node*										getRootNode(int aIndex) const;
	std::vector<Node*>&							getRootNodes();

	void										addRootNode(Node* aNode);
	void										applyFunction(const std::function<void(Node*)>& aFunction);

	// iterator
	std::vector<Node*>::iterator				begin();
	std::vector<Node*>::const_iterator			begin() const;
	std::vector<Node*>::iterator				end();
	std::vector<Node*>::const_iterator			end() const;
private:
	std::vector<Node*>							mRoots;
};

struct SceneInfo_s {
	float							mCycleTime;
	std::deque<SceneGraph*>			mSceneGraphs;
	std::deque<Model*>				mModels;
	std::deque<Camera*>				mCameras;
	std::deque<Light*>				mLights;
	std::deque<Material*>			mMaterials;
	std::deque<Texture*>			mTextures;
	std::deque<Image*>				mImages;

	SceneInfo_s();
	[[nodiscard]] static SceneInfo_s* alloc();
};
T_END_NAMESPACE
