#include <tamashii/engine/scene/render_scene.hpp>

#include <tamashii/engine/scene/model.hpp>
#include <tamashii/engine/scene/camera.hpp>
#include <tamashii/engine/scene/material.hpp>
#include <tamashii/engine/scene/light.hpp>
#include <tamashii/engine/scene/image.hpp>
#include <tamashii/engine/scene/ref_entities.hpp>
#include <tamashii/engine/scene/render_cmd_system.hpp>
#include <tamashii/engine/importer/importer.hpp>
#include <tamashii/engine/platform/system.hpp>
#include <tamashii/engine/common/common.hpp>
#include <tamashii/engine/common/vars.hpp>
#include <tamashii/engine/common/math.hpp>


T_USE_NAMESPACE

RenderScene::RenderScene() : mReady(false), mScene(nullptr), mCurrentCamera(nullptr), mSelection(nullptr), mPlayAnimation(false), mAnimationCycleTime(0), mAnimationTime(0), mUpdateRequests({})
{
	// add default camera to scene
	mDefaultCamera.initPerspectiveCamera(glm::radians(45.0f), 1.0f, 0.1f, 10000.0f);
	mDefaultCamera.setName(DEFAULT_CAMERA_NAME);

	mDefaultCameraRef = new RefCameraPrivate_s();
	mDefaultCameraRef->mode = RefCamera_s::Mode::EDITOR;
	mDefaultCameraRef->camera = &mDefaultCamera;
	static_cast<RefCameraPrivate_s*>(mDefaultCameraRef)->default_camera = true;
	mDefaultCameraRef->ref_camera_index = static_cast<int>(mRefCameras.size());
	mRefCameras.push_back(mDefaultCameraRef);

	mCurrentCamera = mRefCameras.front();
	for (RefCamera_s *rc : mRefCameras) {
		if (rc->camera->getName() == var::default_camera.getString()) {
			mCurrentCamera = rc;
			break;
		}
	}
	mScene = SceneGraph::alloc("");
}

RenderScene::~RenderScene()
{
	destroy();
	delete static_cast<RefCameraPrivate_s*>(mDefaultCameraRef);
}

bool RenderScene::initFromFile(const std::string& aFile)
{
	mSceneFile = aFile;

	SceneInfo_s	*si = Importer::instance().load_scene(aFile);
	std::unique_ptr<SceneInfo_s> ptr(si);
	if (!si) return false;
	return initFromData(si);
}

bool RenderScene::initFromData(SceneInfo_s* aSceneInfo)
{
	destroy();
	filterSceneInfo(aSceneInfo);

	mAnimationCycleTime = aSceneInfo->mCycleTime;
	mScene = aSceneInfo->mSceneGraphs.front();
	mModels = aSceneInfo->mModels;
	mMaterials = aSceneInfo->mMaterials;
	mTextures = aSceneInfo->mTextures;
	mImages = aSceneInfo->mImages;
	mCameras = aSceneInfo->mCameras;
	mLights = aSceneInfo->mLights;

	for (Node *n : *mScene) {
		traverseSceneGraph(n);
	}

    mCurrentCamera = mRefCameras.front();
	for (RefCamera_s* rc : mRefCameras) {
		if (rc->camera->getName() == var::default_camera.getString()) mCurrentCamera = rc;
	}
	return true;
}

bool RenderScene::addSceneFromFile(const std::string& aFile)
{
	SceneInfo_s* si = Importer::instance().load_scene(aFile);
	std::unique_ptr<SceneInfo_s> ptr(si);
	if (!si) return false;
	return addSceneFromData(si);
}

bool RenderScene::addSceneFromData(SceneInfo_s* aSceneInfo)
{
	filterSceneInfo(aSceneInfo);

	mAnimationCycleTime = std::max(mAnimationCycleTime, aSceneInfo->mCycleTime);
	mModels.insert(mModels.end(), aSceneInfo->mModels.begin(), aSceneInfo->mModels.end());
	mMaterials.insert(mMaterials.end(), aSceneInfo->mMaterials.begin(), aSceneInfo->mMaterials.end());
	mTextures.insert(mTextures.end(), aSceneInfo->mTextures.begin(), aSceneInfo->mTextures.end());
	mImages.insert(mImages.end(), aSceneInfo->mImages.begin(), aSceneInfo->mImages.end());
	mCameras.insert(mCameras.end(), aSceneInfo->mCameras.begin(), aSceneInfo->mCameras.end());
	mLights.insert(mLights.end(), aSceneInfo->mLights.begin(), aSceneInfo->mLights.end());

	for(Node* n : aSceneInfo->mSceneGraphs.front()->getRootNodes())
	{
		mScene->addRootNode(n);
		traverseSceneGraph(n);
	}
	{	// remove nodes otherwise they get destroyed with the scene graph
		aSceneInfo->mSceneGraphs.front()->getRootNodes().clear();
		delete aSceneInfo->mSceneGraphs.front();
	}

	for (RefCamera_s* rc : mRefCameras) {
		if (rc->camera->getName() == var::default_camera.getString()) mCurrentCamera = rc;
	}
	return true;
}

void RenderScene::destroy()
{
	mReady.store(false);
	mSelection = nullptr;
	mNewlyAddedRef.clear();
	mNewlyRemovedRef.clear();
	mNewlyRemovedAsset.clear();

	// remove scene data from engine storage
	for (const Model* m : mModels) delete m;
	for (const Image* img : mImages) delete img;
	for (const Camera* c : mCameras) delete c;
	for (Light* l : mLights) {
		switch (l->getType()) {
		case Light::Type::POINT: delete static_cast<PointLight*>(l); break;
		case Light::Type::SPOT: delete static_cast<SpotLight*>(l); break;
		case Light::Type::DIRECTIONAL: delete static_cast<DirectionalLight*>(l); break;
		case Light::Type::IES: delete static_cast<IESLight*>(l); break;
		case Light::Type::SURFACE: delete static_cast<SurfaceLight*>(l); break;
		}
	}
	for (const Material* m : mMaterials) delete m;
	for (const Texture* t : mTextures) delete t;
	delete mScene;
	mModels.clear();
	mImages.clear();
	mCameras.clear();
	mLights.clear();
	mMaterials.clear();
	mScene = nullptr;

	mCurrentCamera = nullptr;
	// clear ref data
	for (const RefModel_s* rm : mRefModels) delete rm;
	for (const RefLight_s* rl : mRefLights) delete rl;
	for (const RefCamera_s* rc : mRefCameras) if(!((RefCameraPrivate_s*)rc)->default_camera) delete rc;
	mRefModels.clear();
	mRefLights.clear();
	mRefCameras.clear();

	// add default camera to scene
	mRefCameras.push_back(mDefaultCameraRef);
}

void RenderScene::readyToRender(const bool aReady)
{ mReady.store(aReady); }

bool RenderScene::readyToRender() const
{ return mReady.load(); }

void RenderScene::addLight(Light* aLight)
{
	if (std::find(mLights.begin(), mLights.end(), aLight) == mLights.end()) mLights.push_back(aLight);
}

void RenderScene::addLightRef(Light* aLight, const glm::vec3 aPosition, const glm::vec4 aRotation, const glm::vec3 aScale)
{
	addLight(aLight);
	const auto node = Node::alloc("");
	node->setTranslation(aPosition);
	node->setRotation(aRotation);
	node->setScale(aScale);
	node->setLight(aLight);
	mScene->addRootNode(node);

	const auto refLight = new RefLight_s();
	refLight->light = node->getLight();
	refLight->ref_light_index = static_cast<int>(mRefLights.size());
	refLight->transforms.push_back(node->getTRS());
	refLight->model_matrix *= node->getTRS()->getMatrix(std::fmod(mAnimationTime, mAnimationCycleTime));
	const glm::vec4 dir = refLight->model_matrix * refLight->light->getDefaultDirection();
	const glm::vec4 pos = refLight->model_matrix * glm::vec4(0, 0, 0, 1);
	refLight->direction = glm::normalize(glm::vec3(dir));
	refLight->position = glm::vec3(pos);
	mRefLights.push_back(refLight);

	mSelection = refLight;
	mNewlyAddedRef.push_back(refLight);
	requestLightUpdate();

	if (refLight->light->getType() == Light::Type::IES)
	{
		const auto ies = static_cast<IESLight*>(refLight->light);
		mTextures.push_back(ies->getCandelaTexture());
		mImages.push_back(ies->getCandelaTexture()->image);
		requestImageUpdate();
		requestTextureUpdate();
	}
}

void RenderScene::addMaterial(Material* aMaterial)
{
	if (std::find(mMaterials.begin(), mMaterials.end(), aMaterial) == mMaterials.end()) mMaterials.push_back(aMaterial);
}

void RenderScene::addModel(Model* aModel)
{
	if (std::find(mModels.begin(), mModels.end(), aModel) == mModels.end()) mModels.push_back(aModel);
}

void RenderScene::addModelRef(Model* aModel, const glm::vec3 aPosition, const glm::vec4 aRotation, const glm::vec3 aScale)
{
	addModel(aModel);
	const auto node = Node::alloc("");
	node->setTranslation(aPosition);
	node->setRotation(aRotation);
	node->setScale(aScale);
	node->setModel(aModel);
	mScene->addRootNode(node);

	const auto refModel = new RefModel_s();
	refModel->model = node->getModel();
	refModel->ref_model_index = static_cast<int>(mRefModels.size());
	refModel->transforms.push_back(node->getTRS());
	refModel->model_matrix *= node->getTRS()->getMatrix(std::fmod(mAnimationTime, mAnimationCycleTime));
	for (Mesh* me : *refModel->model) {
		addMaterial(me->getMaterial());
		auto refMesh = new RefMesh_s();
		refMesh->mesh = me;
		refModel->refMeshes.push_back(refMesh);
	}
	mRefModels.push_back(refModel);

	mSelection = refModel;
	mNewlyAddedRef.push_back(refModel);
	requestMaterialUpdate();
	requestModelGeometryUpdate();
	requestModelInstanceUpdate();
}

void RenderScene::removeModel(RefModel_s* const aRefModel)
{
	Model* model = aRefModel->model;
	const auto it = std::find(mRefModels.begin(), mRefModels.end(), aRefModel);
	if (it != mRefModels.end())
	{
		mRefModels.erase(it);
		mNewlyRemovedRef.push_back(aRefModel);
		for (uint32_t i = 0; i < mRefModels.size(); i++) mRefModels[i]->ref_model_index = static_cast<int>(i);

		// check if there is no other ref to this model
		uint32_t count = 0;
		for (const auto& refModel : mRefModels) if (model == refModel->model) count++;
		if(count == 0) {
			const auto it2 = std::find(mModels.begin(), mModels.end(), model);
			if (it2 != mModels.end()) {
				mNewlyRemovedAsset.push_back(*it2);
				mModels.erase(it2);
			}
		}

		mUpdateRequests.mModelInstances = true;
		mUpdateRequests.mModelGeometries = true;
	}
}

void RenderScene::removeLight(RefLight_s* const aRefLight)
{
	Light* light = aRefLight->light;
	const auto it = std::find(mRefLights.begin(), mRefLights.end(), aRefLight);
	if (it != mRefLights.end())
	{
		mRefLights.erase(it);
		mNewlyRemovedRef.push_back(aRefLight);
		for (uint32_t i = 0; i < mRefLights.size(); i++) mRefLights[i]->ref_light_index = static_cast<int>(i);

		// check if there is no other ref to this light
		uint32_t count = 0;
		for (const auto& refLight : mRefLights) if (light == refLight->light) count++;
		if (count == 0) {
			const auto it2 = std::find(mLights.begin(), mLights.end(), light);
			if (it2 != mLights.end()) {
				mNewlyRemovedAsset.push_back(*it2);
				mLights.erase(it2);
			}
		}

		mUpdateRequests.mLights = true;
	}
}

void RenderScene::requestImageUpdate()
{ mUpdateRequests.mImages = true; }

void RenderScene::requestTextureUpdate()
{ mUpdateRequests.mTextures = true; }

void RenderScene::requestMaterialUpdate()
{ mUpdateRequests.mMaterials = true; }

void RenderScene::requestModelInstanceUpdate()
{ mUpdateRequests.mModelInstances = true; }

void RenderScene::requestModelGeometryUpdate()
{ mUpdateRequests.mModelGeometries = true; }

void RenderScene::requestLightUpdate()
{ mUpdateRequests.mLights = true; }

void RenderScene::requestCameraUpdate()
{ mUpdateRequests.mCamera = true; }

void RenderScene::intersect(const glm::vec3 aOrigin, const glm::vec3 aDirection, const IntersectionSettings aSettings, HitInfo_s *aHitInfo) const
{
	float t = std::numeric_limits<float>::max();
	auto barycentric = glm::vec2(0.0f);
	aHitInfo->mTmin = std::numeric_limits<float>::max();
	if (aSettings.mHitMask == HitMask_e::All || aSettings.mHitMask == HitMask_e::Geometry) {
		for (RefModel_s* refModel : mRefModels) {
			const glm::mat4 inverseModel = glm::inverse(refModel->model_matrix);
			const glm::vec3 osOrigin = glm::vec3(inverseModel * glm::vec4(aOrigin, 1));
			const glm::vec3 osDirection = glm::vec3(inverseModel * glm::vec4(aDirection, 0));
			if (refModel->model->getAABB().intersect(osOrigin, osDirection, t)) {
				for (RefMesh_s* refMesh : refModel->refMeshes) {
					if (refMesh->mesh->getAABB().intersect(osOrigin, osDirection, t)) {
						for (uint32_t i = 0; i < refMesh->mesh->getPrimitiveCount(); i++) {
							if (refMesh->mesh->getTriangle(i).intersect(osOrigin, osDirection, t, barycentric, static_cast<uint32_t>(aSettings.mCullMode))) {
								if (t < aHitInfo->mTmin) {
									aHitInfo->mHit = refModel;
									aHitInfo->mTmin = t;
									aHitInfo->mBarycentric = barycentric;
									aHitInfo->mRefMeshHit = refMesh;
									aHitInfo->mPrimitiveIndex = i;
								}
							}
						}
					}
				}
			}
			if (aHitInfo->mHit && aHitInfo->mHit->type == Ref_s::Type::Model) aHitInfo->mHitPos = refModel->model_matrix * glm::vec4((osOrigin + (osDirection * aHitInfo->mTmin)), 1);
		}
	}

	if (aSettings.mHitMask == HitMask_e::All || aSettings.mHitMask == HitMask_e::Light) {
		for (RefLight_s* refLight : mRefLights) {
			const glm::mat4 inverseModel = glm::inverse(refLight->model_matrix);
			const glm::vec3 osOrigin = glm::vec3(inverseModel * glm::vec4(aOrigin, 1));
			const glm::vec3 osDirection = glm::vec3(inverseModel * glm::vec4(aDirection, 0));
			if (refLight->light->getType() == Light::Type::SURFACE) {
				const SurfaceLight* sl = static_cast<SurfaceLight*>(refLight->light);
				if (sl->getShape() == SurfaceLight::Shape::SQUARE || sl->getShape() == SurfaceLight::Shape::RECTANGLE) {
					aabb_s aabb = { {-0.5,-0.5,0}, {0.5,0.5,0} };
					if (aabb.intersect(osOrigin, osDirection, t)) {
						if (t < aHitInfo->mTmin) {
							aHitInfo->mHit = refLight;
							aHitInfo->mTmin = t;
						}
					}
				}
				else if (sl->getShape() == SurfaceLight::Shape::DISK || sl->getShape() == SurfaceLight::Shape::ELLIPSE) {
					disk_s d{};
					d.mCenter = sl->getCenter();
					d.mRadius = 0.5f;
					d.mNormal = sl->getDefaultDirection();
					if (d.intersect(osOrigin, osDirection, t)) {
						if (t < aHitInfo->mTmin) {
							aHitInfo->mHit = refLight;
							aHitInfo->mTmin = t;
						}
					}
				}
			}
			else {
				disk_s d{};
				d.mCenter = glm::vec3(0, 0, 0);
				d.mRadius = LIGHT_OVERLAY_RADIUS;
				d.mNormal = glm::normalize(-osDirection);
				if (d.intersect(osOrigin, osDirection, t)) {
					if (t < aHitInfo->mTmin) {
						aHitInfo->mHit = refLight;
						aHitInfo->mTmin = t;
					}
				}
			}
			if (aHitInfo->mHit && aHitInfo->mHit->type == Ref_s::Type::Light) aHitInfo->mHitPos = refLight->model_matrix * glm::vec4((osOrigin + (osDirection * aHitInfo->mTmin)), 1);
		}
	}
	if (aHitInfo->mHit) aHitInfo->mTmin = glm::length(aOrigin - aHitInfo->mHitPos);
}

bool RenderScene::animation() const
{ return mPlayAnimation; }

void RenderScene::setAnimation(const bool aPlay)
{ mPlayAnimation = aPlay; }

void RenderScene::resetAnimation()
{
	mAnimationTime = 0;
	update(0);
}

void RenderScene::setSelection(Ref_s* aRef)
{
	mSelection = aRef;
}

void RenderScene::update(const float aMilliseconds)
{
	if (!mReady.load()) return;
	mAnimationTime += (aMilliseconds / 1000.0f);

	// set current camera
	const RefCamera_s* refCam = mCurrentCamera;
	Camera* cam = refCam->camera;
	
	// loop animation time
	const float relativeTime = std::fmod(mAnimationTime, mAnimationCycleTime);
	// update 3d models
	for (RefModel_s *refModel : mRefModels) {
		// if this model is animated, update model matrix
		if (refModel->animated) {
			mUpdateRequests.mModelInstances |= true;
			refModel->model_matrix = glm::mat4(1.0f);
			for (TRS *trs : refModel->transforms) {
				refModel->model_matrix *= trs->getMatrix(relativeTime);
			}
		}
	}
	// update lights
	for (RefLight_s *refLight : mRefLights) {
		// if this light is animated, update model matrix
		if (refLight->animated) {
			mUpdateRequests.mLights |= true;
			refLight->model_matrix = glm::mat4(1);
			for (TRS *trs : refLight->transforms) {
				refLight->model_matrix *= trs->getMatrix(relativeTime);
			}
			refLight->direction = glm::normalize(glm::vec3(refLight->model_matrix * refLight->light->getDefaultDirection()));
			refLight->position = glm::vec3(refLight->model_matrix * glm::vec4(0, 0, 0, 1));
		}
	}
	// update camera
	for (RefCamera_s *refCamera : mRefCameras) {
		if (static_cast<RefCameraPrivate_s*>(refCamera)->default_camera) continue;
		if (refCamera->animated) {
			mUpdateRequests.mCamera |= true;
			refCamera->model_matrix = glm::mat4(1);
			for (TRS *trs : refCamera->transforms) {
				refCamera->model_matrix *= trs->getMatrix(relativeTime);
			}
			static_cast<RefCameraPrivate_s*>(refCamera)->setModelMatrix(refCamera->model_matrix, true);
		}
	}
}

void RenderScene::draw()
{
	if (!mReady.load()) return;

	RefCamera_s *refCam = mCurrentCamera;
	Camera* cam = refCam->camera;

	viewDef_s* vd = new viewDef_s{ getSceneData(), Frustum(refCam) };
	vd->updates = mUpdateRequests;
	mUpdateRequests = {};

	// -- camera --
	// set projection matrix
	vd->projection_matrix = cam->getProjectionMatrix();
	vd->inv_projection_matrix = glm::inverse(vd->projection_matrix);
	// set view matrix
	vd->view_matrix = refCam->view_matrix;
	vd->inv_view_matrix = glm::inverse(refCam->view_matrix);
	// pos and dir
	vd->view_pos = getCurrentCameraPosition();
	vd->view_dir = getCurrentCameraDirection();

	// -- scene --
	// add the surfaces for the current frame
	for (RefModel_s *refModel : mRefModels) {
		// TODO: frustum culling or other optimizations
		// TODO: parallelize frustum culling
		const aabb_s aabb_model = refModel->model->getAABB().transform(refModel->model_matrix);
		if (!vd->view_frustum.checkAABBInside(aabb_model.mMin, aabb_model.mMax)) continue;
		vd->ref_models.push_back(refModel);
		for (RefMesh_s *refMesh : refModel->refMeshes) {
			if (refModel->refMeshes.size() != 1) {
				const aabb_s aabb_mesh = refMesh->mesh->getAABB().transform(refModel->model_matrix);
				if (!vd->view_frustum.checkAABBInside(aabb_mesh.mMin, aabb_mesh.mMax)) continue;
			}

			drawSurf_s ds = {};
			ds.refMesh = refMesh;
			ds.model_matrix = &refModel->model_matrix;
			ds.ref_model_index = refModel->ref_model_index;
			ds.ref_model_ptr = refModel;
			vd->surfaces.push_back(ds);
		}
	}
	//spdlog::info("{}", vd->surfaces.size()); // check view frustum culling
	// add the lights for the current frame
	for (RefLight_s *refLight : mRefLights) {
		// TODO: light culling or other optimizations
		vd->lights.push_back(refLight);
	}
	// set callback of added/removed entity this frame
	for (Ref_s* r : mNewlyAddedRef) renderCmdSystem.addEntityAddedCmd(r);
	mNewlyAddedRef.clear();
	for (Ref_s* r : mNewlyRemovedRef) renderCmdSystem.addEntityRemovedCmd(r);
	mNewlyRemovedRef.clear();
	for (Asset* a : mNewlyRemovedAsset) renderCmdSystem.addAssetRemovedCmd(a);
	mNewlyRemovedAsset.clear();
	// append the prepared render cmd to the list
	renderCmdSystem.addDrawSurfCmd(vd);
}

std::string RenderScene::getSceneFileName()
{ return mSceneFile; }

std::deque<RefCamera_s*>* RenderScene::getAvailableCameras()
{ return &mRefCameras; }

RefCamera_s* RenderScene::getCurrentCamera() const
{
	if (mCurrentCamera) { return mCurrentCamera; }
	else return mDefaultCameraRef;
}

void RenderScene::setCurrentCamera(RefCamera_s* const aCamera)
{
	if (mCurrentCamera != aCamera) {
		mCurrentCamera = aCamera;
		requestCameraUpdate();
	}
}

glm::vec3 RenderScene::getCurrentCameraPosition() const
{
	if (!mReady.load()) return glm::vec3(0);
	return mCurrentCamera->getPosition();
}

glm::vec3 RenderScene::getCurrentCameraDirection() const
{
	if (!mReady.load()) return glm::vec3(0);
	return mCurrentCamera->getDirection();
}

Ref_s* RenderScene::getSelection() const
{
	return mSelection;
}

std::deque<RefLight_s*>* RenderScene::getLightList()
{ return &mRefLights; }

float RenderScene::getCurrentTime() const
{ return mAnimationTime; }

float RenderScene::getCycleTime() const
{ return mAnimationCycleTime; }

scene_s RenderScene::getSceneData()
{ return { mImages, mTextures, mModels, mMaterials, mRefModels, mRefLights, mRefCameras }; }

SceneInfo_s RenderScene::getSceneInfo()
{
	// check all model matrices so that the scene graph is up to date
	for (const RefLight_s* refLight : mRefLights) refLight->updateSceneGraphNodesFromModelMatrix();
	for (const RefCamera_s* refCamera : mRefCameras) refCamera->updateSceneGraphNodesFromModelMatrix(refCamera->y_flipped);
	for (const RefModel_s* refModel : mRefModels) refModel->updateSceneGraphNodesFromModelMatrix();

	SceneInfo_s si = {};
	si.mCycleTime = mAnimationCycleTime;
	si.mSceneGraphs = { mScene };
	si.mModels = mModels;
	si.mCameras = mCameras;
	si.mLights = mLights;
	si.mMaterials = mMaterials;
	si.mTextures = mTextures;
	si.mImages = mImages;
	return si;
}

void RenderScene::filterSceneInfo(SceneInfo_s* aSceneInfo)
{
	if (var::unique_model_refs.getBool()) {
		std::unordered_map<Model*, Model*> map(aSceneInfo->mModels.size());
		aSceneInfo->mModels.clear();
		const std::function f = [&map, &aSceneInfo](Node* aNode)
		{
			if (!aNode->hasModel()) return;
			Model* m = aNode->getModel();
			if (map.find(m) == map.end()) {
				map.insert({ m, m });
				aSceneInfo->mModels.push_back(m);
			}
			else {
				m = new Model(*aNode->getModel());
				aSceneInfo->mModels.push_back(m);
				aNode->setModel(m);
			}
		};
		aSceneInfo->mSceneGraphs.front()->applyFunction(f);
	}
	if (var::unique_material_refs.getBool()) {
		const std::deque<Material*> materialsOld = aSceneInfo->mMaterials;
		aSceneInfo->mMaterials.clear();
		for (const Model* model : aSceneInfo->mModels) {
			for (Mesh* mesh : model->getMeshList()) {
				aSceneInfo->mMaterials.push_back(new Material(*mesh->getMaterial()));
				mesh->setMaterial(aSceneInfo->mMaterials.back());
			}
		}
		for (const Material* m : materialsOld) delete m;
	}
	if (var::unique_light_refs.getBool()) {
		std::unordered_map<Light*, Light*> map(aSceneInfo->mLights.size());
		aSceneInfo->mLights.clear();
		const std::function f = [&map, &aSceneInfo](Node* aNode)
		{
			if (!aNode->hasLight()) return;
			Light* l = aNode->getLight();
			
			if (map.find(l) == map.end()) {
				map.insert({ l, l });
				aSceneInfo->mLights.push_back(l);
			}
			else {
				switch(l->getType())
				{
				case Light::Type::DIRECTIONAL: l = new DirectionalLight(*static_cast<DirectionalLight*>(l)); break;
				case Light::Type::POINT: l = new PointLight(*static_cast<PointLight*>(l)); break;
				case Light::Type::SPOT: l = new SpotLight(*static_cast<SpotLight*>(l));  break;
				case Light::Type::SURFACE: l = new SurfaceLight(*static_cast<SurfaceLight*>(l)); break;
				// todo: copy constructor ies -> copy image etc
				case Light::Type::IES: l = new IESLight(*static_cast<IESLight*>(l)); break;
				}
				aSceneInfo->mLights.push_back(l);
				aNode->setLight(l);
			}
		};
		aSceneInfo->mSceneGraphs.front()->applyFunction(f);
	}
}

void RenderScene::traverseSceneGraph(Node *aNode, glm::mat4 aMatrix, const bool aAnimatedPath) {
	const bool animated_node = aAnimatedPath || aNode->hasAnimation();
	// keep track of all nodes until the current node
	static std::deque<Node*> nodeHistory;
	nodeHistory.push_back(aNode);
	// keep track of all transforms until the current node
	static std::deque<TRS*> trsHistory;
	if (aNode->hasLocalTransform()) {
		trsHistory.push_back(aNode->getTRS());
		// accumulate matrix
		aMatrix *= trsHistory.back()->getMatrix(std::fmod(mAnimationTime, mAnimationCycleTime));
	}
	// if node has a model, add it to the scene with current model matrix
	if (aNode->hasModel()) {
		const auto refModel = new RefModel_s();
		refModel->model = aNode->getModel();
		refModel->ref_model_index = static_cast<int>(mRefModels.size());
		refModel->model_matrix = aMatrix;
		refModel->animated = animated_node;
		refModel->transforms = trsHistory;
		for (Mesh *me : *refModel->model) {
			auto refMesh = new RefMesh_s();
			refMesh->mesh = me;
			refModel->refMeshes.push_back(refMesh);
		}
		mRefModels.push_back(refModel);
	}
	// node has a camera, inverse of model matrix = view matrix
	if (aNode->hasCamera()) {
		const auto refCamera = new RefCameraPrivate_s();
		refCamera->setModelMatrix(aMatrix, true);
		refCamera->camera = aNode->getCamera();
		refCamera->ref_camera_index = static_cast<int>(mRefCameras.size());
		refCamera->animated = animated_node;
		refCamera->transforms = trsHistory;
		mRefCameras.push_back(refCamera);
	}
	// node has a light
	if (aNode->hasLight()) {
		const auto refLight = new RefLight_s();
		refLight->light = aNode->getLight();
		refLight->ref_light_index = static_cast<int>(mRefLights.size());
		const glm::vec4 dir = aMatrix * refLight->light->getDefaultDirection();
		const glm::vec4 pos = aMatrix * glm::vec4(0, 0, 0, 1);
		refLight->direction = glm::normalize(glm::vec3(dir));
		refLight->position = glm::vec3(pos);
		refLight->model_matrix = aMatrix;
		refLight->animated = animated_node;
		refLight->transforms = trsHistory;
		mRefLights.push_back(refLight);
	}
	// go down the graph
	for (Node *n : *aNode) {
		traverseSceneGraph(n, aMatrix, animated_node);
	}
	// before leaving the node, remove current transform data
	if (aNode->hasLocalTransform()) trsHistory.pop_back();
	// before leaving the node, remove current node data
	nodeHistory.pop_back();
}

