#pragma once
#include <tamashii/public.hpp>
#include <tamashii/engine/importer/importer.hpp>
#include <tamashii/engine/scene/camera.hpp>

#include <string>
#include <deque>

T_BEGIN_NAMESPACE
struct Ref_s;
struct RefLight_s;
struct RefCamera_s;
struct RefModel_s;
struct RefMesh_s;
struct SceneInfo_s;
struct RefCameraPrivate_s;
class Image;
class Texture;
class Node;
class Material;
class SceneGraph;
// data for the rendering backend
struct scene_s
{
	// unique data of this scene
	std::deque<Image*>&						images;
	std::deque<Texture*>&					textures;
	std::deque<Model*>&						models;
	std::deque<Material*>&					materials;
	// instantiated data of this scene 
	// e.g. one model from scene_s.models can be instantiated multible times at different position but with the same geometry
	std::deque<RefModel_s*>&				refModels;
	std::deque<RefLight_s*>&				refLights;
	std::deque<RefCamera_s*>&				refCameras;
};
enum class HitMask_e {
	Geometry = 1,
	Light = 2,
	All = Geometry | Light
};
enum class CullMode_e {
	None = 0,
	Front = 1,
	Back = 2,
	Both = 3
};
struct IntersectionSettings
{
	IntersectionSettings() : mCullMode(CullMode_e::None), mHitMask(HitMask_e::All) {}
	IntersectionSettings(const CullMode_e	aCullMode, const HitMask_e aHitMask) : mCullMode(aCullMode), mHitMask(aHitMask) {}
	CullMode_e								mCullMode;
	HitMask_e								mHitMask;
};
struct HitInfo_s {
	Ref_s*									mHit;				// object that was hit
	RefMesh_s*								mRefMeshHit;		// if object was a model then this also includes the mesh that was hit
	uint32_t								mPrimitiveIndex;	// if object was a model/mesh, this is the triangle index that was hit
	glm::vec3								mOriginPos;			// ray start pos in worldspace
	glm::vec3								mHitPos;			// hit pos in worldspace
	glm::vec2								mBarycentric;		// bary coords if hit was triangle mesh
	float									mTmin;				// distance
};
struct DrawInfo_s {
	enum class Target_e : uint32_t {
		VERTEX_COLOR,
		CUSTOM
	};
	Target_e								mTarget;
	glm::vec3								mCursorColor;
	bool									mDrawMode;			// draw mode off/on
	bool									mHoverOver;			// is mouse hovering over mesh
	HitInfo_s								mHitInfo;
	glm::vec3								mPositionWs;		// position of draw pos
	glm::vec3								mNormalWsNorm;		// normal of draw pos
    glm::vec3								mTangentWsNorm;		// tangent of draw pos
	float									mRadius;			// draw influence radius
	glm::vec4								mColor0;			// color for left mouse button
	glm::vec4								mColor1;			// color for right mouse button
	bool									mDrawRgb;
	bool									mDrawAlpha;
	bool									mSoftBrush;
	bool									mDrawAll;
};

struct SceneUpdates_s
{
	bool									mImages;
	bool									mTextures;
	bool									mMaterials;
	bool									mModelInstances;	// the instantiations or the model matrix of the scene geometry has changed
	bool									mModelGeometries;	// the overall scene is the same but the geometry has changed
	bool									mLights;
	bool									mCamera;

	bool any() const { return mImages || mTextures || mMaterials || mModelInstances || mModelGeometries || mLights || mCamera; }
	bool all() const { return mImages && mTextures && mMaterials && mModelInstances && mModelGeometries && mLights && mCamera; }
	SceneUpdates_s operator| (const SceneUpdates_s& aSceneUpdates) const
	{
		SceneUpdates_s su = {};
		su.mImages = mImages || aSceneUpdates.mImages;
		su.mTextures = mTextures || aSceneUpdates.mTextures;
		su.mMaterials = mMaterials || aSceneUpdates.mMaterials;
		su.mModelInstances = mModelInstances || aSceneUpdates.mModelInstances;
		su.mModelGeometries = mModelGeometries || aSceneUpdates.mModelGeometries;
		su.mLights = mLights || aSceneUpdates.mLights;
		su.mCamera = mCamera || aSceneUpdates.mCamera;
		return su;
	};
	SceneUpdates_s operator& (const SceneUpdates_s& aSceneUpdates) const
	{
		SceneUpdates_s su = {};
		su.mImages = mImages && aSceneUpdates.mImages;
		su.mTextures = mTextures && aSceneUpdates.mTextures;
		su.mMaterials = mMaterials && aSceneUpdates.mMaterials;
		su.mModelInstances = mModelInstances && aSceneUpdates.mModelInstances;
		su.mModelGeometries = mModelGeometries && aSceneUpdates.mModelGeometries;
		su.mLights = mLights && aSceneUpdates.mLights;
		su.mCamera = mCamera && aSceneUpdates.mCamera;
		return su;
	};
};

class RenderScene {
public:
											RenderScene();
											~RenderScene();

	bool									initFromFile(const std::string& aFile);
	bool									initFromData(SceneInfo_s* aSceneInfo);
	bool									addSceneFromFile(const std::string& aFile);
	bool									addSceneFromData(SceneInfo_s* aSceneInfo);

	void									destroy();

											// since the scene can be loaded/reloaded from different thread
											// it is necessary to synchronize the access
	void									readyToRender(bool aReady);
	bool									readyToRender() const;

	void									addLight(Light* aLight);
	void									addLightRef(Light* aLight, glm::vec3 aPosition = glm::vec3(0), glm::vec4 aRotation = glm::vec4(0), glm::vec3 aScale = glm::vec3(1));
	void									addMaterial(Material* aMaterial);
	void									addModel(Model* aModel);
	void									addModelRef(Model* aModel, glm::vec3 aPosition = glm::vec3(0), glm::vec4 aRotation = glm::vec4(0), glm::vec3 aScale = glm::vec3(1));

	void									removeModel(RefModel_s* aRefModel);
	void									removeLight(RefLight_s* aRefLight);

											// call to indicate data changes
	void									requestImageUpdate();
	void									requestTextureUpdate();
	void									requestMaterialUpdate();
	void									requestModelInstanceUpdate();
	void									requestModelGeometryUpdate();
	void									requestLightUpdate();
	void									requestCameraUpdate();

	void									intersect(glm::vec3 aOrigin, glm::vec3 aDirection, IntersectionSettings aSettings, HitInfo_s *aHitInfo) const;

	bool									animation() const;
	void									setAnimation(bool aPlay);
	void									resetAnimation();

	void									setSelection(Ref_s* aRef);

	void									update(float aMilliseconds);
	void									draw();

	std::string								getSceneFileName();

	std::deque<RefCamera_s*>*				getAvailableCameras();
	RefCamera_s*							getCurrentCamera() const;
	void									setCurrentCamera(RefCamera_s* aCamera);
	glm::vec3								getCurrentCameraPosition() const;
	glm::vec3								getCurrentCameraDirection() const;
	Ref_s*									getSelection() const;

	std::deque<RefLight_s*>*				getLightList();

											// animation
	float									getCurrentTime() const;
	float									getCycleTime() const;

	scene_s									getSceneData();
	SceneInfo_s								getSceneInfo();
private:
	static void								filterSceneInfo(SceneInfo_s* aSceneInfo);
	void									traverseSceneGraph(Node* aNode, glm::mat4 aMatrix = glm::mat4(1.0f), bool aAnimatedPath = false);

	std::string								mSceneFile;
	std::atomic<bool>						mReady;	// scene is ready to render

											// assets used in this scene referenced from global asset manager
	SceneGraph*								mScene;
	std::deque<Model*>						mModels;
	std::deque<Camera*>						mCameras;
	std::deque<Light*>						mLights;
	std::deque<Material*>					mMaterials;
	std::deque<Texture*>					mTextures;
	std::deque<Image*>						mImages;

											// instantiations of models
	std::deque<RefModel_s*>					mRefModels;
	std::deque<RefCamera_s*>				mRefCameras;
	std::deque<RefLight_s*>					mRefLights;

	Camera									mDefaultCamera;
	RefCamera_s*							mDefaultCameraRef;

	RefCamera_s*							mCurrentCamera;
	Ref_s*									mSelection;

	bool									mPlayAnimation;
											// time of one animation cycle in seconds
	float									mAnimationCycleTime;
											// current time of this scene in seconds
	float									mAnimationTime;
											// was there an update since the last frame
	SceneUpdates_s							mUpdateRequests;
											// added since last cycle
	std::deque<Ref_s*>						mNewlyAddedRef;
	std::deque<Ref_s*>						mNewlyRemovedRef;
	std::deque<Asset*>						mNewlyRemovedAsset;
};

T_END_NAMESPACE
